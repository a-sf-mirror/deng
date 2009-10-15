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

static void updateLineDefAABB(linedef_t* line);
static void rotatePoint(int an, float* x, float* y, float startSpotX,
                        float startSpotY);
static boolean CheckMobjBlocking(linedef_t* line, polyobj_t* po);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Called when the polyobj hits a mobj.
void (*po_callback) (mobj_t* mobj, void* lineDef, void* po);

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
    gamemap_t* map = DMU_CurrentMap();

    if(map)
    {
        if(num & 0x80000000)
        {
            uint idx = num & 0x7fffffff;

            if(idx < map->numPolyObjs)
                return map->polyObjs[idx];
        }
        else
        {
            uint i;

            for(i = 0; i < map->numPolyObjs; ++i)
            {
                polyobj_t* po = map->polyObjs[i];

                if((uint) po->tag == num)
                {
                    return po;
                }
            }
        }
    }

    return NULL;
}

/**
 * @return              @c true, iff this is indeed a polyobj origin.
 */
boolean P_IsPolyobjOrigin(gamemap_t* map, void* ddMobjBase)
{
    if(map)
    {
        uint i;
        polyobj_t* po;

        for(i = 0; i < map->numPolyObjs; ++i)
        {
            po = map->polyObjs[i];

            if(po == ddMobjBase)
            {
                return true;
            }
        }
    }

    return false;
}

static void updateLineDefAABB(linedef_t* line)
{
    byte                edge;

    edge = (line->L_v1->pos[VX] < line->L_v2->pos[VX]);
    line->bBox[BOXLEFT]  = LINE_VERTEX(line, edge^1)->pos[VX];
    line->bBox[BOXRIGHT] = LINE_VERTEX(line, edge)->pos[VX];

    edge = (line->L_v1->pos[VY] < line->L_v2->pos[VY]);
    line->bBox[BOXBOTTOM] = LINE_VERTEX(line, edge^1)->pos[VY];
    line->bBox[BOXTOP]    = LINE_VERTEX(line, edge)->pos[VY];

    // Update the line's slopetype.
    line->dX = line->L_v2->pos[VX] - line->L_v1->pos[VX];
    line->dY = line->L_v2->pos[VY] - line->L_v1->pos[VY];
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
    uint i;
    vec2_t point;
    linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[0])->obj;

    V2_Set(point, line->L_v1->pos[VX], line->L_v1->pos[VY]);
    V2_InitBox(po->box, point);

    for(i = 0; i < po->numLineDefs; ++i)
    {
        linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[i])->obj;

        V2_Set(point, line->L_v1->pos[VX], line->L_v1->pos[VY]);
        V2_AddToBox(po->box, point);
    }
}

/**
 * Called at the start of the map after all the structures needed for
 * refresh have been setup.
 */
void P_MapInitPolyobjs(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numPolyObjs; ++i)
    {
        polyobj_t* po = map->polyObjs[i];
        subsector_t* subsector;
        uint j;
        fvertex_t avg; // Used to find a polyobj's center, and hence subsector.

        for(j = 0; j < po->numSegs; ++j)
        {
            poseg_t* seg = &po->segs[j];
            uint k;
            biassurface_t* bsuf = SB_CreateSurface(map);

            bsuf->size = 4;
            bsuf->illum = Z_Calloc(sizeof(vertexillum_t) * bsuf->size,
                PU_STATIC, 0);
            for(k = 0; k < bsuf->size; ++k)
                SB_InitVertexIllum(&bsuf->illum[k]);

            seg->bsuf = bsuf;
        }

        avg.pos[VX] = 0;
        avg.pos[VY] = 0;

        for(j = 0; j < po->numLineDefs; ++j)
        {
            linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[j])->obj;
            sidedef_t* side = LINE_FRONTSIDE(line);
            surface_t* surface = &side->SW_topsurface;

            side->SW_topinflags |= SUIF_NO_RADIO;
            side->SW_middleinflags |= SUIF_NO_RADIO;
            side->SW_bottominflags |= SUIF_NO_RADIO;

            avg.pos[VX] += line->L_v1->pos[VX];
            avg.pos[VY] += line->L_v1->pos[VY];

            // Set the surface normal.
            surface->normal[VY] = (line->L_v1->pos[VX] - line->L_v2->pos[VX]) / line->length;
            surface->normal[VX] = (line->L_v2->pos[VY] - line->L_v1->pos[VY]) / line->length;
            surface->normal[VZ] = 0;

            // All surfaces of a sidedef have the same normal.
            memcpy(side->SW_middlenormal, surface->normal, sizeof(surface->normal));
            memcpy(side->SW_bottomnormal, surface->normal, sizeof(surface->normal));
        }

        avg.pos[VX] /= po->numLineDefs;
        avg.pos[VY] /= po->numLineDefs;

        if((subsector = R_PointInSubSector(avg.pos[VX], avg.pos[VY])))
        {
            if(subsector->polyObj)
            {
                Con_Message("P_MapInitPolyobjs: Warning: Multiple polyobjs in a single subsector\n"
                            "  (face %i, sector %i). Previous polyobj overridden.\n",
                            (int) DMU_GetObjRecord(DMU_SUBSECTOR, subsector)->id,
                            (int) DMU_GetObjRecord(DMU_SECTOR, subsector->sector)->id);
            }
            subsector->polyObj = po;
            po->subsector = subsector;
        }

        P_PolyobjUnLink(po);
        P_PolyobjLink(po);

        P_PolyobjChanged(po);
    }
}

boolean P_PolyobjMove(struct polyobj_s* po, float x, float y)
{
    uint count;
    fvertex_t* prevPts;
    boolean blocked;

    if(!po)
        return false;

    P_PolyobjUnLink(po);

    prevPts = po->prevPts;
    blocked = false;

    validCount++;
    for(count = 0; count < po->numLineDefs; ++count, prevPts++)
    {
        linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[count])->obj;

        if(line->validCount != validCount)
        {
            line->bBox[BOXTOP]    += y;
            line->bBox[BOXBOTTOM] += y;
            line->bBox[BOXLEFT]   += x;
            line->bBox[BOXRIGHT]  += x;
            line->validCount = validCount;
        }

        line->L_v1->pos[VX] += x;
        line->L_v1->pos[VY] += y;

        (*prevPts).pos[VX] += x; // Previous points are unique for each linedef.
        (*prevPts).pos[VY] += y;
    }

    for(count = 0; count < po->numLineDefs; ++count)
    {
        linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[count])->obj;

        if(CheckMobjBlocking(line, po))
        {
            blocked = true;
        }
    }

    if(blocked)
    {
        prevPts = po->prevPts;
        validCount++;

        for(count = 0; count < po->numLineDefs; ++count, prevPts++)
        {
            linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[count])->obj;

            if(line->validCount != validCount)
            {
                line->bBox[BOXTOP]    -= y;
                line->bBox[BOXBOTTOM] -= y;
                line->bBox[BOXLEFT]   -= x;
                line->bBox[BOXRIGHT]  -= x;
                line->validCount = validCount;
            }

            line->L_v1->pos[VX] -= x;
            line->L_v1->pos[VY] -= y;

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
    float trx, try, gxt, gyt;

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
    boolean blocked;

    if(!po)
        return false;

    an = (po->angle + angle) >> ANGLETOFINESHIFT;

    P_PolyobjUnLink(po);

    originalPts = po->originalPts;
    prevPts = po->prevPts;

    for(count = 0; count < po->numLineDefs;
        ++count, originalPts++, prevPts++)
    {
        linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[count])->obj;
        sidedef_t* side = LINE_FRONTSIDE(line);
        surface_t* surface = &side->SW_topsurface;
        vec2_t pos;

        prevPts->pos[VX] = line->L_v1->pos[VX];
        prevPts->pos[VY] = line->L_v1->pos[VY];
        line->L_v1->pos[VX] = originalPts->pos[VX];
        line->L_v1->pos[VY] = originalPts->pos[VY];

        V2_Set(pos, line->L_v1->pos[VX], line->L_v1->pos[VY]);
        rotatePoint(an, &pos[VX], &pos[VY], po->pos[VX], po->pos[VY]);
        line->L_v1->pos[VX] = pos[VX];
        line->L_v1->pos[VY] = pos[VY];

        // Now update the surface normal.
        surface->normal[VY] = (line->L_v1->pos[VX] - line->L_v2->pos[VX]) / line->length;
        surface->normal[VX] = (line->L_v2->pos[VY] - line->L_v1->pos[VY]) / line->length;
        surface->normal[VZ] = 0;

        // All surfaces of a sidedef have the same normal.
        memcpy(side->SW_middlenormal, surface->normal, sizeof(surface->normal));
        memcpy(side->SW_bottomnormal, surface->normal, sizeof(surface->normal));
    }

    blocked = false;
    validCount++;

    for(count = 0; count < po->numLineDefs; ++count)
    {
        linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[count])->obj;

        if(CheckMobjBlocking(line, po))
        {
            blocked = true;
        }

        if(line->validCount != validCount)
        {
            updateLineDefAABB(line);
            line->validCount = validCount;
        }

        line->angle += angle;
    }

    if(blocked)
    {
        prevPts = po->prevPts;
        for(count = 0; count < po->numLineDefs; ++count, prevPts++)
        {
            linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[count])->obj;

            line->L_v1->pos[VX] = prevPts->pos[VX];
            line->L_v1->pos[VY] = prevPts->pos[VY];
        }

        validCount++;

        for(count = 0; count < po->numLineDefs; ++count, prevPts++)
        {
            linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[count])->obj;

            if(line->validCount != validCount)
            {
                updateLineDefAABB(line);
                line->validCount = validCount;
            }
            line->angle -= angle;
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
    linkpolyobj_t* tempLink;

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
        tempLink->next = Z_Malloc(sizeof(linkpolyobj_t), PU_MAP, 0);
        tempLink->next->next = NULL;
        tempLink->next->prev = tempLink;
        tempLink->next->polyobj = po;
    }
}

void P_PolyobjUnlinkFromRing(polyobj_t* po, linkpolyobj_t** list)
{
    linkpolyobj_t* iter = *list;

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
    gamemap_t* map;

    if(!po)
        return;

    map = DMU_CurrentMap();
    if(!map)
        return;

    Blockmap_UnlinkPolyobj(map->blockMap, po);
}

void P_PolyobjLink(struct polyobj_s* po)
{
    gamemap_t* map;

    if(!po)
        return;

    map = DMU_CurrentMap();
    if(!map)
        return;

    Blockmap_LinkPolyobj(map->blockMap, po);
}

typedef struct ptrmobjblockingparams_s {
    boolean         blocked;
    linedef_t*      line;
    polyobj_t*      po;
} ptrmobjblockingparams_t;

boolean PTR_CheckMobjBlocking(mobj_t* mo, void* data)
{
    if((mo->ddFlags & DDMF_SOLID) ||
       (mo->dPlayer && !(mo->dPlayer->flags & DDPF_CAMERA)))
    {
        float tmbbox[4];
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
                    po_callback(mo, DMU_GetObjRecord(DMU_LINEDEF, params->line), params->po);

                params->blocked = true;
            }
        }
    }

    return true; // Continue iteration.
}

static boolean CheckMobjBlocking(linedef_t* line, polyobj_t* po)
{
    uint blockBox[4];
    vec2_t bbox[2];
    ptrmobjblockingparams_t params;
    gamemap_t* map;

    params.blocked = false;
    params.line = line;
    params.po = po;

    bbox[0][VX] = line->bBox[BOXLEFT]   - DDMOBJ_RADIUS_MAX;
    bbox[0][VY] = line->bBox[BOXBOTTOM] - DDMOBJ_RADIUS_MAX;
    bbox[1][VX] = line->bBox[BOXRIGHT]  + DDMOBJ_RADIUS_MAX;
    bbox[1][VY] = line->bBox[BOXTOP]    + DDMOBJ_RADIUS_MAX;

    map = DMU_CurrentMap();
    Blockmap_BoxToBlocks(map->blockMap, blockBox, bbox);
    Blockmap_BoxIterateMobjs(map->blockMap, blockBox, PTR_CheckMobjBlocking, &params);

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
                               void* data, boolean retObjRecord)
{
    uint i;

    for(i = 0; i < po->numLineDefs; ++i)
    {
        linedef_t* line = ((dmuobjrecord_t*) po->lineDefs[i])->obj;
        void* ptr;

        if(line->validCount == validCount)
            continue;

        line->validCount = validCount;

        if(retObjRecord)
            ptr = po->lineDefs[i];
        else
            ptr = line;

        if(!func(ptr, data))
            return false;
    }

    return true;
}
