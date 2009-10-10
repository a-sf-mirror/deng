/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * r_shadow.c: Runtime Map Shadowing (FakeRadio)
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static zblockset_t *shadowLinksBlockSet;

// CODE --------------------------------------------------------------------

/**
 * Line1 and line2 are the (dx,dy)s for two lines, connected at the
 * origin (0,0).  Dist1 and dist2 are the distances from these lines.
 * The returned point (in 'point') is dist1 away from line1 and dist2
 * from line2, while also being the nearest point to the origin (in
 * case the lines are parallel).
 */
void R_CornerNormalPoint(const pvec2_t line1, float dist1,
                         const pvec2_t line2, float dist2, pvec2_t point,
                         pvec2_t lp)
{
    float               len1, len2;
    vec2_t              norm1, norm2;

    // Length of both lines.
    len1 = V2_Length(line1);
    len2 = V2_Length(line2);

    // Calculate normals for both lines.
    V2_Set(norm1, -line1[VY] / len1 * dist1, line1[VX] / len1 * dist1);
    V2_Set(norm2, line2[VY] / len2 * dist2, -line2[VX] / len2 * dist2);

    // Do we need to calculate the extended points, too?  Check that
    // the extension does not bleed too badly outside the legal shadow
    // area.
    if(lp)
    {
        V2_Set(lp, line2[VX] / len2 * dist2, line2[VY] / len2 * dist2);
    }

    // Are the lines parallel?  If so, they won't connect at any
    // point, and it will be impossible to determine a corner point.
    if(V2_IsParallel(line1, line2))
    {
        // Just use a normal as the point.
        if(point)
            V2_Copy(point, norm1);
        return;
    }

    // Find the intersection of normal-shifted lines.  That'll be our
    // corner point.
    if(point)
        V2_Intersection(norm1, line1, norm2, line2, point);
}

/**
 * @return          The width (world units) of the shadow edge.
 *                  It is scaled depending on the length of the edge.
 */
float R_ShadowEdgeWidth(const pvec2_t edge)
{
    float       length = V2_Length(edge);
    float       normalWidth = 20;   //16;
    float       maxWidth = 60;
    float       w;

    // A long edge?
    if(length > 600)
    {
        w = length - 600;
        if(w > 1000)
            w = 1000;
        return normalWidth + w / 1000 * maxWidth;
    }

    return normalWidth;
}

/**
 * Updates all the shadow offsets for the given vertex.
 *
 * \pre Lineowner rings MUST be set up.
 *
 * @param vtx           Ptr to the vertex being updated.
 */
void R_UpdateVertexShadowOffsets(vertex_t *vtx)
{
    vec2_t              left, right;

    if(vtx->numLineOwners > 0)
    {
        lineowner_t        *own, *base;

        own = base = vtx->lineOwners;
        do
        {
            linedef_t          *lineB = own->lineDef;
            linedef_t          *lineA = own->LO_next->lineDef;

            if(lineB->L_v1 == vtx)
            {
                right[VX] = lineB->dX;
                right[VY] = lineB->dY;
            }
            else
            {
                right[VX] = -lineB->dX;
                right[VY] = -lineB->dY;
            }

            if(lineA->L_v1 == vtx)
            {
                left[VX] = -lineA->dX;
                left[VY] = -lineA->dY;
            }
            else
            {
                left[VX] = lineA->dX;
                left[VY] = lineA->dY;
            }

            // The left side is always flipped.
            V2_Scale(left, -1);

            R_CornerNormalPoint(left, R_ShadowEdgeWidth(left), right,
                                R_ShadowEdgeWidth(right),
                                own->shadowOffsets.inner,
                                own->shadowOffsets.extended);

            own = own->LO_next;
        } while(own != base);
    }
}

/**
 * Link a seg to an arbitary subsector for the purposes of shadowing.
 */
static void linkShadowLineDefToSubSector(linedef_t* line, byte side,
                                    face_t* face)
{
    shadowlink_t*       link;
    subsector_t*        subSector = (subsector_t*) face->data;

#ifdef _DEBUG
// Check the links for dupes!
{
shadowlink_t*       i;

for(i = subSector->shadows; i; i = i->next)
    if(i->lineDef == line && i->side == side)
        Con_Error("R_LinkShadow: Already here!!\n");
}
#endif

    // We'll need to allocate a new link.
    link = Z_BlockNewElement(shadowLinksBlockSet);

    // The links are stored into a linked list.
    link->next = subSector->shadows;
    subSector->shadows = link;
    link->lineDef = line;
    link->side = side;
}

typedef struct shadowlinkerparms_s {
    linedef_t*          lineDef;
    byte                side;
} shadowlinkerparms_t;

/**
 * If the shadow polygon (parm) contacts the subsector, link the poly
 * to the subsector's shadow list.
 */
boolean RIT_ShadowSubSectorLinker(face_t* face, void* parm)
{
    shadowlinkerparms_t* data = (shadowlinkerparms_t*) parm;

    linkShadowLineDefToSubSector(data->lineDef, data->side, face);
    return true;
}

/**
 * Does the given linedef qualify as an edge shadow caster?
 */
boolean R_IsShadowingLineDef(linedef_t *line)
{
    if(line)
    {
        if(!(line->inFlags & LF_POLYOBJ) && !LINE_SELFREF(line) &&
           !(line->vo[0]->LO_next->lineDef == line ||
             line->vo[1]->LO_next->lineDef == line))
        {
            return true;
        }
    }

    return false;
}

/**
 * Calculate sector edge shadow points, create the shadow polygons and link
 * them to the subsectors.
 */
void R_InitSectorShadows(void)
{
    uint            startTime = Sys_GetRealTime();

    uint            i;
    vec2_t          bounds[2], point;
    vertex_t*       vtx0, *vtx1;
    lineowner_t*    vo0, *vo1;
    shadowlinkerparms_t data;

    for(i = 0; i < numVertexes; ++i)
    {
        R_UpdateVertexShadowOffsets(VERTEX_PTR(i));
    }

    /**
     * The algorithm:
     *
     * 1. Use the subsector blockmap to look for all the blocks that are
     *    within the linedef's shadow bounding box.
     *
     * 2. Check the subsectors whose sector is the same as the linedef.
     *
     * 3. If any of the shadow points are in the subsector, or any of the
     *    shadow edges cross one of the subsector's edges (not parallel),
     *    link the linedef to the subsector.
     */
    shadowLinksBlockSet = Z_BlockCreate(sizeof(shadowlink_t), 1024, PU_MAP);

    for(i = 0; i < numSideDefs; ++i)
    {
        sidedef_t*          side = SIDE_PTR(i);
        byte                sid;

        if(!R_IsShadowingLineDef(side->lineDef))
            continue;

        sid = LINE_BACKSIDE(side->lineDef) == side? BACK : FRONT;

        vtx0 = LINE_VERTEX(side->lineDef, sid);
        vtx1 = LINE_VERTEX(side->lineDef, sid^1);
        vo0 = side->lineDef->L_vo(sid)->LO_next;
        vo1 = side->lineDef->L_vo(sid^1)->LO_prev;

        // Use the extended points, they are wider than inoffsets.
        V2_Set(point, vtx0->V_pos[VX], vtx0->V_pos[VY]);
        V2_InitBox(bounds, point);

        V2_Sum(point, point, vo0->shadowOffsets.extended);
        V2_AddToBox(bounds, point);

        V2_Set(point, vtx1->V_pos[VX], vtx1->V_pos[VY]);
        V2_AddToBox(bounds, point);

        V2_Sum(point, point, vo1->shadowOffsets.extended);
        V2_AddToBox(bounds, point);

        data.lineDef = side->lineDef;
        data.side = sid;

        P_SubSectorsBoxIteratorv(bounds, side->sector,
                                 RIT_ShadowSubSectorLinker, &data, false);
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_InitSectorShadows: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}
