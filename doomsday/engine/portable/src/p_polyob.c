/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * p_polyob.c: Polygon Objects
 *
 * Polyobj translation and rotation.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void updateSegBBox(seg_t* seg);
static void rotatePoint(int an, float* x, float* y, float startSpotX,
                        float startSpotY);
static boolean CheckMobjBlocking(seg_t* seg, polyobj_t* po);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Called when the polyobj hits a mobj.
void (*po_callback) (mobj_t* mobj, void* seg, void* po);

polyobj_t** polyObjs; // List of all poly-objects in the map.
uint numPolyObjs;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * The po_callback is called when a polyobj hits a mobj.
 */
void P_SetPolyobjCallback(void (*func) (struct mobj_s*, void*, void*))
{
    po_callback = func;
}

/**
 * Retrieve a ptr to polyobj_t by index or by tag.
 *
 * @param num               If MSB is set, treat num as an index, ELSE
 *                          num is a tag that *should* match one polyobj.
 */
polyobj_t* P_GetPolyobj(uint num)
{
    if(num & 0x80000000)
    {
        uint                idx = num & 0x7fffffff;

        if(idx < numPolyObjs)
            return polyObjs[idx];
    }
    else
    {
        uint                i;

        for(i = 0; i < numPolyObjs; ++i)
        {
            polyobj_t*          po = polyObjs[i];

            if((uint) po->tag == num)
            {
                return po;
            }
        }
    }

    return NULL;
}

/**
 * @return              @c true, iff this is indeed a polyobj origin.
 */
boolean P_IsPolyobjOrigin(void* ddMobjBase)
{
    uint                i;
    polyobj_t*          po;

    for(i = 0; i < numPolyObjs; ++i)
    {
        po = polyObjs[i];

        if(po == ddMobjBase)
        {
            return true;
        }
    }

    return false;
}

static void updateSegBBox(seg_t* seg)
{
    linedef_t* line = seg->lineDef;

    line->bBox[BOXLEFT]  = MIN_OF(seg->SG_v2pos[VX], seg->SG_v1pos[VX]);
    line->bBox[BOXRIGHT] = MAX_OF(seg->SG_v2pos[VX], seg->SG_v1pos[VX]);

    line->bBox[BOXBOTTOM] = MIN_OF(seg->SG_v2pos[VY], seg->SG_v1pos[VY]);
    line->bBox[BOXTOP]    = MAX_OF(seg->SG_v2pos[VY], seg->SG_v1pos[VY]);

    // Update the line's slopetype.
    line->dX = line->L_v2pos[VX] - line->L_v1pos[VX];
    line->dY = line->L_v2pos[VY] - line->L_v1pos[VY];
    if(!line->dX)
    {
        line->slopeType = ST_VERTICAL;
    }
    else if(!line->dY)
    {
        line->slopeType = ST_HORIZONTAL;
    }
    else
    {
        if(line->dY / line->dX > 0)
        {
            line->slopeType = ST_POSITIVE;
        }
        else
        {
            line->slopeType = ST_NEGATIVE;
        }
    }
}

/**
 * Update the polyobj bounding box.
 */
void P_PolyobjUpdateBBox(polyobj_t* po)
{
    uint                i;
    vec2_t              point;
    vertex_t*           vtx;
    seg_t**             segPtr;

    segPtr = po->segs;
    V2_Set(point, (*segPtr)->SG_v1pos[VX], (*segPtr)->SG_v1pos[VY]);
    V2_InitBox(po->box, point);

    for(i = 0; i < po->numSegs; ++i, segPtr++)
    {
        vtx = (*segPtr)->SG_v1;

        V2_Set(point, vtx->V_pos[VX], vtx->V_pos[VY]);
        V2_AddToBox(po->box, point);
    }
}

/**
 * Called at the start of the map after all the structures needed for
 * refresh have been setup.
 */
void P_MapInitPolyobjs(void)
{
    uint                i;

    for(i = 0; i < numPolyObjs; ++i)
    {
        polyobj_t*          po = polyObjs[i];
        seg_t**             segPtr;
        subsector_t*        ssec;
        fvertex_t           avg; // Used to find a polyobj's center, and hence subsector.

        avg.pos[VX] = 0;
        avg.pos[VY] = 0;

        segPtr = po->segs;
        while(*segPtr)
        {
            seg_t*              seg = *segPtr;
            sidedef_t*          side = SEG_SIDEDEF(seg);
            surface_t*          surface = &side->SW_topsurface;

            side->SW_topinflags |= SUIF_NO_RADIO;
            side->SW_middleinflags |= SUIF_NO_RADIO;
            side->SW_bottominflags |= SUIF_NO_RADIO;

            avg.pos[VX] += seg->SG_v1pos[VX];
            avg.pos[VY] += seg->SG_v1pos[VY];

            // Set the surface normal.
            surface->normal[VY] = (seg->SG_v1pos[VX] - seg->SG_v2pos[VX]) / seg->length;
            surface->normal[VX] = (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]) / seg->length;
            surface->normal[VZ] = 0;

            // All surfaces of a sidedef have the same normal.
            memcpy(side->SW_middlenormal, surface->normal, sizeof(surface->normal));
            memcpy(side->SW_bottomnormal, surface->normal, sizeof(surface->normal));
            segPtr++;
        }

        avg.pos[VX] /= po->numSegs;
        avg.pos[VY] /= po->numSegs;

        ssec = R_PointInSubsector(avg.pos[VX], avg.pos[VY]);
        if(ssec)
        {
            if(ssec->polyObj)
            {
                Con_Message("P_MapInitPolyobjs: Warning: Multiple polyobjs in a single subsector\n"
                            "  (ssec %ld, sector %ld). Previous polyobj overridden.\n",
                            (long)GET_SUBSECTOR_IDX(ssec), (long)GET_SECTOR_IDX(ssec->sector));
            }
            ssec->polyObj = po;
            po->subsector = ssec;
        }

        P_PolyobjUnLink(po);
        P_PolyobjLink(po);
    }
}

boolean P_PolyobjMove(struct polyobj_s* po, float x, float y)
{
    uint count;
    fvertex_t* prevPts;
    seg_t** segList;
    seg_t** veryTempSeg;
    boolean blocked;

    if(!po)
        return false;

    P_PolyobjUnLink(po);

    segList = po->segs;
    prevPts = po->prevPts;
    for(count = 0; count < po->numSegs; ++count, segList++, prevPts++)
    {
        seg_t* seg = *segList;

        seg->lineDef->bBox[BOXTOP]    += y;
        seg->lineDef->bBox[BOXBOTTOM] += y;
        seg->lineDef->bBox[BOXLEFT]   += x;
        seg->lineDef->bBox[BOXRIGHT]  += x;

        for(veryTempSeg = po->segs; veryTempSeg != segList; veryTempSeg++)
        {
            if((*veryTempSeg)->SG_v1 == seg->SG_v1)
            {
                break;
            }
        }

        if(veryTempSeg == segList)
        {
            seg->SG_v1pos[VX] += x;
            seg->SG_v1pos[VY] += y;
        }

        (*prevPts).pos[VX] += x; // Previous points are unique for each seg.
        (*prevPts).pos[VY] += y;
    }

    segList = po->segs;
    blocked = false;
    for(count = 0; count < po->numSegs; ++count, segList++)
    {
        if(CheckMobjBlocking(*segList, po))
        {
            blocked = true;
        }
    }

    if(blocked)
    {
        count = 0;
        segList = po->segs;
        prevPts = po->prevPts;
        for(count = 0; count < po->numSegs; ++count, segList++, prevPts++)
        {
            seg_t* seg = *segList;

            seg->lineDef->bBox[BOXTOP]    -= y;
            seg->lineDef->bBox[BOXBOTTOM] -= y;
            seg->lineDef->bBox[BOXLEFT]   -= x;
            seg->lineDef->bBox[BOXRIGHT]  -= x;

            for(veryTempSeg = po->segs; veryTempSeg != segList; veryTempSeg++)
            {
                if((*veryTempSeg)->SG_v1 == seg->SG_v1)
                {
                    break;
                }
            }

            if(veryTempSeg == segList)
            {
                seg->SG_v1pos[VX] -= x;
                seg->SG_v1pos[VY] -= y;
            }

            (*prevPts).pos[VX] -= x;
            (*prevPts).pos[VY] -= y;
        }

        P_PolyobjLink(po);
        return false;
    }

    po->pos[VX] += x;
    po->pos[VY] += y;
    P_PolyobjLink(po);

    // A change has occured.
    P_PolyobjChanged(po);

    return true;
}

static void rotatePoint(int an, float* x, float* y, float startSpotX,
                        float startSpotY)
{
    float               trx, try, gxt, gyt;

    trx = *x;
    try = *y;

    gxt = trx * FIX2FLT(fineCosine[an]);
    gyt = try * FIX2FLT(finesine[an]);
    *x = gxt - gyt + startSpotX;

    gxt = trx * FIX2FLT(finesine[an]);
    gyt = try * FIX2FLT(fineCosine[an]);
    *y = gyt + gxt + startSpotY;
}

boolean P_PolyobjRotate(struct polyobj_s* po, angle_t angle)
{
    int an;
    uint count;
    fvertex_t* originalPts;
    fvertex_t* prevPts;
    vertex_t* vtx;
    seg_t** segList;
    boolean blocked;

    if(!po)
        return false;

    an = (po->angle + angle) >> ANGLETOFINESHIFT;

    P_PolyobjUnLink(po);

    segList = po->segs;
    originalPts = po->originalPts;
    prevPts = po->prevPts;

    for(count = 0; count < po->numSegs;
        ++count, segList++, originalPts++, prevPts++)
    {
        seg_t* seg = *segList;

        vtx = seg->SG_v1;

        prevPts->pos[VX] = vtx->V_pos[VX];
        prevPts->pos[VY] = vtx->V_pos[VY];
        vtx->V_pos[VX] = originalPts->pos[VX];
        vtx->V_pos[VY] = originalPts->pos[VY];

        rotatePoint(an, &vtx->V_pos[VX], &vtx->V_pos[VY],
                    po->pos[VX], po->pos[VY]);
    }

    segList = po->segs;
    for(count = 0; count < po->numSegs; ++count, segList++)
    {
        seg_t* seg = *segList;
        sidedef_t* side = SEG_SIDEDEF(seg);
        surface_t* surface = &side->SW_topsurface;

        updateSegBBox(seg);
        seg->angle += angle;
        seg->lineDef->angle += angle >> FRACBITS;

        // Now update the surface normal.
        surface->normal[VY] = (seg->SG_v1pos[VX] - seg->SG_v2pos[VX]) / seg->length;
        surface->normal[VX] = (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]) / seg->length;
        surface->normal[VZ] = 0;

        // All surfaces of a sidedef have the same normal.
        memcpy(side->SW_middlenormal, surface->normal, sizeof(surface->normal));
        memcpy(side->SW_bottomnormal, surface->normal, sizeof(surface->normal));
    }

    segList = po->segs;
    blocked = false;
    for(count = 0; count < po->numSegs; ++count, segList++)
    {
        seg_t* seg = *segList;

        if(CheckMobjBlocking(seg, po))
        {
            blocked = true;
        }
    }

    if(blocked)
    {
        segList = po->segs;
        prevPts = po->prevPts;
        for(count = 0; count < po->numSegs; ++count, segList++, prevPts++)
        {
            seg_t* seg = *segList;

            vtx = seg->SG_v1;
            vtx->V_pos[VX] = prevPts->pos[VX];
            vtx->V_pos[VY] = prevPts->pos[VY];
        }

        segList = po->segs;
        for(count = 0; count < po->numSegs; ++count, segList++)
        {
            seg_t* seg = *segList;
            sidedef_t* side = SEG_SIDEDEF(seg);
            surface_t* surface = &side->SW_topsurface;

            updateSegBBox(seg);
            seg->angle -= angle;
            seg->lineDef->angle -= angle >> FRACBITS;

            // Now update the surface normal.
            surface->normal[VY] = (seg->SG_v1pos[VX] - seg->SG_v2pos[VX]) / seg->length;
            surface->normal[VX] = (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]) / seg->length;
            surface->normal[VZ] = 0;

            // All surfaces of a sidedef have the same normal.
            memcpy(side->SW_middlenormal, surface->normal, sizeof(surface->normal));
            memcpy(side->SW_bottomnormal, surface->normal, sizeof(surface->normal));
        }

        P_PolyobjLink(po);
        return false;
    }

    po->angle += angle;

    P_PolyobjLink(po);
    P_PolyobjChanged(po);
    return true;
}

void P_PolyobjLinkToRing(polyobj_t* po, linkpolyobj_t** link)
{
    linkpolyobj_t*      tempLink;

    if(!(*link))
    {   // Create a new link at the current block cell.
        *link = Z_Malloc(sizeof(linkpolyobj_t), PU_MAP, 0);
        (*link)->next = NULL;
        (*link)->prev = NULL;
        (*link)->polyobj = po;
        return;
    }
    else
    {
        tempLink = *link;
        while(tempLink->next != NULL && tempLink->polyobj != NULL)
        {
            tempLink = tempLink->next;
        }
    }

    if(tempLink->polyobj == NULL)
    {
        tempLink->polyobj = po;
        return;
    }
    else
    {
        tempLink->next =
            Z_Malloc(sizeof(linkpolyobj_t), PU_MAP, 0);
        tempLink->next->next = NULL;
        tempLink->next->prev = tempLink;
        tempLink->next->polyobj = po;
    }
}

void P_PolyobjUnlinkFromRing(polyobj_t* po, linkpolyobj_t** list)
{
    linkpolyobj_t*      iter = *list;

    while(iter != NULL && iter->polyobj != po)
    {
        iter = iter->next;
    }

    if(iter != NULL)
    {
        iter->polyobj = NULL;
    }
}

void P_PolyobjUnLink(struct polyobj_s* po)
{
    P_BlockmapUnlinkPolyobj(BlockMap, po);
}

void P_PolyobjLink(struct polyobj_s* po)
{
    P_BlockmapLinkPolyobj(BlockMap, po);
}

typedef struct ptrmobjblockingparams_s {
    boolean         blocked;
    linedef_t*      line;
    seg_t*          seg;
    polyobj_t*      po;
} ptrmobjblockingparams_t;

boolean PTR_CheckMobjBlocking(mobj_t* mo, void* data)
{
    if((mo->ddFlags & DDMF_SOLID) ||
       (mo->dPlayer && !(mo->dPlayer->flags & DDPF_CAMERA)))
    {
        float               tmbbox[4];
        ptrmobjblockingparams_t* params = data;

        tmbbox[BOXTOP]    = mo->pos[VY] + mo->radius;
        tmbbox[BOXBOTTOM] = mo->pos[VY] - mo->radius;
        tmbbox[BOXLEFT]   = mo->pos[VX] - mo->radius;
        tmbbox[BOXRIGHT]  = mo->pos[VX] + mo->radius;

        if(!(tmbbox[BOXRIGHT]  <= params->line->bBox[BOXLEFT] ||
             tmbbox[BOXLEFT]   >= params->line->bBox[BOXRIGHT] ||
             tmbbox[BOXTOP]    <= params->line->bBox[BOXBOTTOM] ||
             tmbbox[BOXBOTTOM] >= params->line->bBox[BOXTOP]))
        {
            if(P_BoxOnLineSide(tmbbox, params->line) == -1)
            {
                if(po_callback)
                    po_callback(mo, params->seg, params->po);

                params->blocked = true;
            }
        }
    }

    return true; // Continue iteration.
}

static boolean CheckMobjBlocking(seg_t* seg, polyobj_t* po)
{
    uint                blockBox[4];
    vec2_t              bbox[2];
    linedef_t*          ld;
    ptrmobjblockingparams_t params;

    params.blocked = false;
    params.line = ld = seg->lineDef;
    params.seg = seg;
    params.po = po;

    bbox[0][VX] = ld->bBox[BOXLEFT]   - DDMOBJ_RADIUS_MAX;
    bbox[0][VY] = ld->bBox[BOXBOTTOM] - DDMOBJ_RADIUS_MAX;
    bbox[1][VX] = ld->bBox[BOXRIGHT]  + DDMOBJ_RADIUS_MAX;
    bbox[1][VY] = ld->bBox[BOXTOP]    + DDMOBJ_RADIUS_MAX;

    validCount++;
    P_BoxToBlockmapBlocks(BlockMap, blockBox, bbox);
    P_BlockBoxMobjsIterator(BlockMap, blockBox,
                            PTR_CheckMobjBlocking, &params);

    return params.blocked;
}

/**
 * Iterate the linedefs of the polyobj calling func for each.
 * Iteration will stop if func returns false.
 *
 * @param po            The polyobj whose lines are to be iterated.
 * @param func          Call back function to call for each line of this po.
 * @return              @c true, if all callbacks are successfull.
 */
boolean P_PolyobjLinesIterator(polyobj_t* po,
                               boolean (*func) (struct linedef_s*, void*),
                               void* data)
{
    uint                i;
    seg_t**             segList;

    segList = po->segs;
    for(i = 0; i < po->numSegs; ++i, segList++)
    {
        seg_t*              seg = *segList;
        linedef_t*          line = seg->lineDef;

        if(line->validCount == validCount)
            continue;

        line->validCount = validCount;

        if(!func(line, data))
            return false;
    }

    return true;
}
