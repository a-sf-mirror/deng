/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1998-2006 James Haley <haleyjd@hotmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_sight.c: Line of Sight Testing.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

typedef struct losdata_s {
    int             flags; // LS_* flags @see lineSightFlags
    divline_t       trace;
    float           startZ; // Eye z of looker.
    float           topSlope; // Slope to top of target.
    float           bottomSlope; // Slope to bottom of target.
    float           bBox[4];
    float           to[3];
} losdata_t;

// CODE --------------------------------------------------------------------

static boolean interceptLineDef(const linedef_t* li, losdata_t* los,
                                divline_t* dl)
{
    divline_t localDL, *dlPtr;

    // Try a quick, bounding-box rejection.
    if(li->bBox[BOXLEFT]   > los->bBox[BOXRIGHT] ||
       li->bBox[BOXRIGHT]  < los->bBox[BOXLEFT] ||
       li->bBox[BOXBOTTOM] > los->bBox[BOXTOP] ||
       li->bBox[BOXTOP]    < los->bBox[BOXBOTTOM])
        return false;

    if(P_PointOnDivlineSide(li->L_v1->pos[VX], li->L_v1->pos[VY], &los->trace) ==
       P_PointOnDivlineSide(li->L_v2->pos[VX], li->L_v2->pos[VY], &los->trace))
        return false; // Not crossed.

    if(dl)
        dlPtr = dl;
    else
        dlPtr = &localDL;

    LineDef_ConstructDivline(li, dlPtr);

    if(P_PointOnDivlineSide(FIX2FLT(los->trace.pos[VX]),
                            FIX2FLT(los->trace.pos[VY]), dlPtr) ==
       P_PointOnDivlineSide(los->to[VX], los->to[VY], dlPtr))
        return false; // Not crossed.

    return true; // Crossed.
}

static boolean crossLineDef(const linedef_t* li, byte side, losdata_t* los)
{
#define RTOP            0x1
#define RBOTTOM         0x2
#define RMIDDLE         0x4

    float frac;
    byte ranges = 0;
    divline_t dl;
    const sector_t* fsec, *bsec;
    boolean noBack;

    if(!interceptLineDef(li, los, &dl))
        return true; // Ray does not intercept seg on the X/Y plane.

    if(!LINE_SIDE(li, side))
        return true; // Seg is on the back side of a one-sided window.

    fsec = LINE_SECTOR(li, side);
    bsec  = (LINE_BACKSIDE(li)? LINE_SECTOR(li, side^1) : NULL);
    noBack = LINE_BACKSIDE(li)? false : true;

    if(!noBack && !(los->flags & LS_PASSLEFT) &&
       (!(bsec->SP_floorheight < fsec->SP_ceilheight) ||
        !(fsec->SP_floorheight < bsec->SP_ceilheight)))
        noBack = true;

    if(noBack)
    {
        if((los->flags & LS_PASSLEFT) &&
           side != LineDef_PointOnSide(li, FIX2FLT(los->trace.pos[VX]),
                                           FIX2FLT(los->trace.pos[VY])))
            return true; // Ray does not intercept seg from left to right.

        if(!(los->flags & (LS_PASSOVER | LS_PASSUNDER)))
            return false; // Stop iteration.
    }

    // Handle the case of a zero height backside in the top range.
    if(noBack)
    {
        ranges |= RTOP;
    }
    else
    {
        if(bsec->SP_floorheight != fsec->SP_floorheight)
            ranges |= RBOTTOM;
        if(bsec->SP_ceilheight != fsec->SP_ceilheight)
            ranges |= RTOP;
        if(!(los->flags & LS_PASSMIDDLE))
            ranges |= RMIDDLE;
    }

    if(!ranges)
        return true;

    frac = P_InterceptVector(&los->trace, &dl);

    if(ranges & RMIDDLE)
    {
        surface_t* surface = &LINE_FRONTSIDE(li)->SW_middlesurface;

        if(surface->material && !(surface->blendMode > 0) &&
           !(surface->rgba[CA] < 1.0f) &&
           (!(los->flags & LS_PASSLEFT) ||
            side == LineDef_PointOnSide(li, FIX2FLT(los->trace.pos[VX]),
                                        FIX2FLT(los->trace.pos[VY]))))
        {
            material_snapshot_t ms;

            Materials_Prepare(surface->material, MPF_SMOOTH, NULL, &ms);
            if(ms.isOpaque)
            {
                hedge_t* hEdge = li->hEdges[0];
                plane_t* ffloor, *fceil, *bfloor, *bceil;
                float bottom, top;

                R_PickPlanesForSegExtrusion(hEdge, R_UseSectorsFromFrontSideDef(hEdge, SEG_MIDDLE),
                                            &ffloor, &fceil, &bfloor, &bceil);
                if(R_FindBottomTopOfHEdgeSection(hEdge, SEG_MIDDLE, ffloor, fceil, bfloor, bceil,
                                                 &bottom, &top, NULL, NULL))
                {
                    if(!(los->bottomSlope > (top - los->startZ) / frac) ||
                         los->topSlope < (bottom - los->startZ) / frac)
                        return false; // Stop iteration.
                }
            }
        }
    }

    if((ranges & ~RMIDDLE) == 0)
        return true;

    if(!noBack &&
       (((los->flags & LS_PASSOVER_SKY) &&
         IS_SKYSURFACE(&fsec->SP_ceilsurface) &&
         IS_SKYSURFACE(&bsec->SP_ceilsurface) &&
         los->bottomSlope > (bsec->SP_ceilheight - los->startZ) / frac) ||
        ((los->flags & LS_PASSUNDER_SKY) &&
         IS_SKYSURFACE(&fsec->SP_floorsurface) &&
         IS_SKYSURFACE(&bsec->SP_floorsurface) &&
         los->topSlope < (bsec->SP_floorheight - los->startZ) / frac)))
        return true;

    if((los->flags & LS_PASSOVER) &&
       los->bottomSlope > (fsec->SP_ceilheight - los->startZ) / frac)
        return true;

    if((los->flags & LS_PASSUNDER) &&
       los->topSlope < (fsec->SP_floorheight - los->startZ) / frac)
        return true;

    if(ranges & RTOP)
    {
        float top = (noBack? fsec->SP_ceilheight : fsec->SP_ceilheight < bsec->SP_ceilheight? fsec->SP_ceilheight : bsec->SP_ceilheight);
        float slope = (top - los->startZ) / frac;

        if((slope < los->topSlope) ^ (noBack && !(los->flags & LS_PASSOVER)) ||
           (noBack && los->topSlope > (fsec->SP_floorheight - los->startZ) / frac))
            los->topSlope = slope;
        if((slope < los->bottomSlope) ^ (noBack && !(los->flags & LS_PASSUNDER)) ||
           (noBack && los->bottomSlope > (fsec->SP_floorheight - los->startZ) / frac))
            los->bottomSlope = slope;
    }

    if(ranges & RBOTTOM)
    {
        float bottom = (noBack? fsec->SP_floorheight : fsec->SP_floorheight > bsec->SP_floorheight? fsec->SP_floorheight : bsec->SP_floorheight);
        float slope = (bottom - los->startZ) / frac;

        if(slope > los->bottomSlope)
            los->bottomSlope = slope;
        if(slope > los->topSlope)
            los->topSlope = slope;
    }

    if(los->topSlope <= los->bottomSlope)
        return false; // Stop iteration.

    return true;

#undef RTOP
#undef RBOTTOM
}

/**
 * @return              @c true iff trace crosses the given subsector.
 */
static boolean crossSubsector(subsector_t* subsector, losdata_t* los)
{
    if(subsector->polyObj)
    {   // Check polyobj lines.
        polyobj_t* po = subsector->polyObj;
        uint i;

        for(i = 0; i < po->numLineDefs; ++i)
        {
            linedef_t* line = ((objectrecord_t*) po->lineDefs[i])->obj;

            if(line->validCount != validCount)
            {
                line->validCount = validCount;

                if(!crossLineDef(line, FRONT, los))
                    return false; // Stop iteration.
            }
        }
    }

    {
    // Check lines.
    hedge_t* hEdge;

    if((hEdge = subsector->face->hEdge))
    {
        do
        {
            const seg_t* seg = (seg_t*) (hEdge)->data;

            if(seg && seg->sideDef && seg->sideDef->lineDef->validCount != validCount)
            {
                linedef_t* li = seg->sideDef->lineDef;

                li->validCount = validCount;

                if(!crossLineDef(li, seg->side, los))
                    return false;
            }
        } while((hEdge = hEdge->next) != subsector->face->hEdge);
    }
    }

    return true; // Continue iteration.
}

/**
 * @return              @c true iff trace crosses the node.
 */
static boolean crossBSPNode(map_t* map, binarytree_t* tree, losdata_t* los)
{
    face_t* face;

    while(!BinaryTree_IsLeaf(tree))
    {
        const node_t* node = BinaryTree_GetData(tree);
        byte side = R_PointOnSide(FIX2FLT(los->trace.pos[VX]),
                                  FIX2FLT(los->trace.pos[VY]),
                                  &node->partition);

        // Would the trace completely cross this partition?
        if(side == R_PointOnSide(los->to[VX], los->to[VY],
                                 &node->partition))
        {   // Yes, decend!
            tree = BinaryTree_GetChild(tree, side);
        }
        else
        {   // No.
            if(!crossBSPNode(map, BinaryTree_GetChild(tree, side), los))
                return 0; // Cross the starting side.
            else
                tree = BinaryTree_GetChild(tree, side^1); // Cross the ending side.
        }
    }

    face = (face_t*) BinaryTree_GetData(tree);
    return crossSubsector((subsector_t*)face->data, los);
}

/**
 * Traces a line of sight.
 *
 * @param from          World position, trace origin coordinates.
 * @param to            World position, trace target coordinates.
 * @param flags         Line Sight Flags (LS_*) @see lineSightFlags
 *
 * @return              @c true if the traverser function returns @c true
 *                      for all visited lines.
 */
boolean Map_CheckLineSight(map_t* map, const float from[3], const float to[3],
                           float bottomSlope, float topSlope, int flags)
{
    assert(map);
    {
    losdata_t los;

    los.flags = flags;
    los.startZ = from[VZ];
    los.topSlope = to[VZ] + topSlope - los.startZ;
    los.bottomSlope = to[VZ] + bottomSlope - los.startZ;
    los.trace.pos[VX] = FLT2FIX(from[VX]);
    los.trace.pos[VY] = FLT2FIX(from[VY]);
    los.trace.dX = FLT2FIX(to[VX] - from[VX]);
    los.trace.dY = FLT2FIX(to[VY] - from[VY]);
    los.to[VX] = to[VX];
    los.to[VY] = to[VY];
    los.to[VZ] = to[VZ];

    if(from[VX] > to[VX])
    {
        los.bBox[BOXRIGHT]  = from[VX];
        los.bBox[BOXLEFT]   = to[VX];
    }
    else
    {
        los.bBox[BOXRIGHT]  = to[VX];
        los.bBox[BOXLEFT]   = from[VX];
    }

    if(from[VY] > to[VY])
    {
        los.bBox[BOXTOP]    = from[VY];
        los.bBox[BOXBOTTOM] = to[VY];
    }
    else
    {
        los.bBox[BOXTOP]    = to[VY];
        los.bBox[BOXBOTTOM] = from[VY];
    }

    validCount++;
    return crossBSPNode(map, map->_rootNode, &los);
    }
}
