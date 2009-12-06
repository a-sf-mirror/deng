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
 * p_objlink.c: Objlink management.
 *
 * Object>surface contacts and object>subsector spreading.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_defs.h"

// MACROS ------------------------------------------------------------------

#define X_TO_OBBX(bm, cx)           (((cx) - (bm)->origin[0]) >> (FRACBITS+7))
#define Y_TO_OBBY(bm, cy)           (((cy) - (bm)->origin[1]) >> (FRACBITS+7))
#define OBB_XY(bm, bx, by)          (&(bm)->blocks[(bx) + (by) * (bm)->width])

BEGIN_PROF_TIMERS()
  PROF_OBJLINK_SPREAD,
  PROF_OBJLINK_LINK
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

typedef struct objlink_s {
    struct objlink_s* next; // Next in the same obj block, or NULL.
    struct objlink_s* nextUsed;
    void*           obj;
} objlink_t;

typedef struct objblock_s {
    objlink_t*      head[NUM_OBJ_TYPES];

    // Used to prevent multiple processing of a block during one refresh frame.
    boolean         doneSpread;
} objblock_t;

typedef struct contactfinder_data_s {
    void*           obj;
    objtype_t       objType;
    vec3_t          objPos;
    float           objRadius;
    float           box[4];
} contactfinderparams_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void processSeg(hedge_t* seg, void* data);
static boolean PIT_LinkObjToSubsector(subsector_t* subsector, void* params);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static objlink_t* objLinkFirst = NULL, *objLinkCursor = NULL;

// CODE --------------------------------------------------------------------

static objlink_t* allocObjLink(void)
{
    objlink_t* oLink;

    if(objLinkCursor == NULL)
    {
        oLink = Z_Malloc(sizeof(*oLink), PU_STATIC, NULL);

        // Link to the list of objlink nodes.
        oLink->nextUsed = objLinkFirst;
        objLinkFirst = oLink;
    }
    else
    {
        oLink = objLinkCursor;
        objLinkCursor = objLinkCursor->nextUsed;
    }

    oLink->next = NULL;
    oLink->obj = NULL;

    return oLink;
}

static void clearRoots(objblockmap_t* obm)
{
    // Clear the list head ptrs and doneSpread flags.
    memset(obm->blocks, 0, sizeof(*obm->blocks) * obm->width * obm->height);
}

objblockmap_t* P_CreateObjBlockmap(float originX, float originY, int width,
                                   int height)
{
    objblockmap_t* obm;

    obm = Z_Malloc(sizeof(objblockmap_t), PU_STATIC, 0);

    // Origin has fixed-point coordinates.
    obm->origin[0] = FLT2FIX(originX);
    obm->origin[1] = FLT2FIX(originY);
    obm->width = width;
    obm->height = height;
    obm->blocks = Z_Malloc(sizeof(*obm->blocks) * obm->width * obm->height, PU_STATIC, 0);

    return obm;
}

void P_DestroyObjBlockmap(objblockmap_t* obm)
{
    assert(obm);

    ObjBlockmap_Clear(obm);

    Z_Free(obm->blocks);
    Z_Free(obm);
}

void ObjBlockmap_Clear(objblockmap_t* obm)
{
    assert(obm);
    clearRoots(obm);
    objLinkCursor = objLinkFirst;
}

/**
 * Attempt to spread the obj from the given contact from the source subSector and
 * into the (relative) back subSector.
 *
 * @param subsector     Ptr to subsector to attempt to spread over to.
 * @param data          Ptr to contactfinder_data structure.
 *
 * @return              @c true, because this function is also used as an
 *                      iterator.
 */
static void spreadInSubsector(subsector_t* subsector, void* data)
{
    hedge_t* hEdge;

    if((hEdge = subsector->face->hEdge))
    {
        do
        {
            processSeg(hEdge, data);
        } while((hEdge = hEdge->next) != subsector->face->hEdge);
    }
}

static void processSeg(hedge_t* hEdge, void* data)
{
    contactfinderparams_t* params = (contactfinderparams_t*) data;
    subsector_t* src, *dst;
    float distance;
    vertex_t* vtx;
    seg_t* seg = (seg_t*) hEdge->data;

    // Can not spread over one-sided lines.
    if(!HE_BACKSECTOR(hEdge))
        return;

    // Which way does the spread go?
    if(HE_FRONTSUBSECTOR(hEdge)->validCount == validCount &&
       HE_BACKSUBSECTOR(hEdge)->validCount != validCount)
    {
        src = (subsector_t*) hEdge->face->data;
        dst = (subsector_t*) hEdge->twin->face->data;
    }
    else
    {
        // Not eligible for spreading.
        return;
    }

    // Is the dst subSector inside the objlink's AABB?
    if(dst->bBox[1][VX] <= params->box[BOXLEFT] ||
       dst->bBox[0][VX] >= params->box[BOXRIGHT] ||
       dst->bBox[1][VY] <= params->box[BOXBOTTOM] ||
       dst->bBox[0][VY] >= params->box[BOXTOP])
    {
        // The subSector is not inside the params's bounds.
        return;
    }

    // Can the spread happen?
    if(seg->sideDef)
    {
        if(dst->sector)
        {
            if(dst->sector->planes[PLN_CEILING]->height <= dst->sector->planes[PLN_FLOOR]->height ||
               dst->sector->planes[PLN_CEILING]->height <= src->sector->planes[PLN_FLOOR]->height ||
               dst->sector->planes[PLN_FLOOR]->height   >= src->sector->planes[PLN_CEILING]->height)
            {
                // No; destination sector is closed with no height.
                return;
            }
        }

        // Don't spread if the middle material completely fills the gap between
        // floor and ceiling (direction is from dst to src).
        if(R_DoesMiddleMaterialFillGap(seg->sideDef->lineDef,
            dst == ((subsector_t*) hEdge->twin->face->data)? false : true))
            return;
    }

    // Calculate 2D distance to hEdge.
    {
    float dx, dy;

    dx = hEdge->HE_v2->pos[VX] - hEdge->HE_v1->pos[VX];
    dy = hEdge->HE_v2->pos[VY] - hEdge->HE_v1->pos[VY];
    vtx = hEdge->HE_v1;
    distance = ((vtx->pos[VY] - params->objPos[VY]) * dx -
                (vtx->pos[VX] - params->objPos[VX]) * dy) / seg->length;
    }

    if(seg->sideDef)
    {
        if((src == ((subsector_t*) hEdge->face->data) && distance < 0) ||
           (src == ((subsector_t*) hEdge->twin->face->data) && distance > 0))
        {
            // Can't spread in this direction.
            return;
        }
    }

    // Check distance against the obj radius.
    if(distance < 0)
        distance = -distance;
    if(distance >= params->objRadius)
    {   // The obj doesn't reach that far.
        return;
    }

    // During next step, obj will continue spreading from there.
    dst->validCount = validCount;

    // Add this obj to the destination subsector.
    // @todo Subsector should return the map its linked to.
    Map_AddSubsectorContact(P_CurrentMap(), P_ObjectRecord(DMU_SUBSECTOR, dst)->id - 1, params->objType, params->obj);
    spreadInSubsector(dst, data);
}

/**
 * Create a contact for the objlink in all the subsectors the linked obj is
 * contacting (tests done on bounding boxes and the subsector spread test).
 *
 * @param oLink         Ptr to objlink to find subsector contacts for.
 */
static void findContacts(objtype_t type, objlink_t* oLink)
{
#define SUBSECTORSPREAD_MINRADIUS 16 // In world units.

    contactfinderparams_t params;
    subsector_t* subsector;

    switch(type)
    {
    case OT_LUMOBJ:
        {
        lumobj_t* lum = (lumobj_t*) oLink->obj;

        if(lum->type != LT_OMNI)
            return; // Only omni lights spread.

        V3_Copy(params.objPos, lum->pos);
        // Use a slightly smaller radius than what the obj really is.
        params.objRadius = LUM_OMNI(lum)->radius * .9f;
        subsector = lum->subsector;
        break;
        }
    case OT_MOBJ:
        {
        mobj_t* mo = (mobj_t*) oLink->obj;

        V3_Copy(params.objPos, mo->pos);
        params.objRadius = R_VisualRadius(mo);
        subsector = (subsector_t*) ((objectrecord_t*) mo->subsector)->obj;
        break;
        }
    case OT_PARTICLE:
        {
        particle_t* pt = (particle_t*) oLink->obj;

        V3_SetFixed(params.objPos, pt->pos[VX], pt->pos[VY], pt->pos[VZ]);
        // Use a slightly smaller radius than what the obj really is.
        params.objRadius = FIX2FLT(pt->gen->stages[pt->stage].radius) * .9f;
        subsector = pt->subsector;
        break;
        }
    default:
        Con_Error("Internal Error: Invalid value (%i) for objtype "
                  "in findContacts.", (int) type);
        return; // Unreachable.
    }

    params.obj = oLink->obj;
    params.objType = type;

    // Always contact the obj's own subsector.
    // @todo Subsector should return the map its linked to.
    Map_AddSubsectorContact(P_CurrentMap(), P_ObjectRecord(DMU_SUBSECTOR, subsector)->id - 1, type, oLink->obj);

    if(params.objRadius < SUBSECTORSPREAD_MINRADIUS)
        return;

    // Do the subsector spread. Begin from the obj's own subsector.
    params.box[BOXLEFT]   = params.objPos[VX] - params.objRadius;
    params.box[BOXRIGHT]  = params.objPos[VX] + params.objRadius;
    params.box[BOXBOTTOM] = params.objPos[VY] - params.objRadius;
    params.box[BOXTOP]    = params.objPos[VY] + params.objRadius;

    subsector->validCount = ++validCount;
    spreadInSubsector(subsector, &params);

#undef SUBSECTORSPREAD_MINRADIUS
}

/**
 * Spread obj contacts in the subsector > obj blockmap to all other
 * subsectors within the block.
 *
 * @param subsector     Ptr to the subsector to spread the obj contacts of.
 */
void ObjBlockmap_Spread(objblockmap_t* obm, const subsector_t* subsector,
                        float maxRadius)
{
    int xl, xh, yl, yh, x, y;


    assert(obm);
    assert(subsector);

    xl = X_TO_OBBX(obm, FLT2FIX(subsector->bBox[0][VX] - maxRadius));
    xh = X_TO_OBBX(obm, FLT2FIX(subsector->bBox[1][VX] + maxRadius));
    yl = Y_TO_OBBY(obm, FLT2FIX(subsector->bBox[0][VY] - maxRadius));
    yh = Y_TO_OBBY(obm, FLT2FIX(subsector->bBox[1][VY] + maxRadius));

    // Are we completely outside the blockmap?
    if(xh < 0 || xl >= obm->width || yh < 0 || yl >= obm->height)
        return;

    // Clip to blockmap bounds.
    if(xl < 0)
        xl = 0;
    if(xh >= obm->width)
        xh = obm->width - 1;
    if(yl < 0)
        yl = 0;
    if(yh >= obm->height)
        yh = obm->height - 1;

    for(x = xl; x <= xh; ++x)
        for(y = yl; y <= yh; ++y)
        {
            objblock_t* block = OBB_XY(obm, x, y);

            if(!block->doneSpread)
            {
                objtype_t i;

                // Spread the objs in this block.
                for(i = 0; i < NUM_OBJ_TYPES; ++i)
                {
                    objlink_t* iter = block->head[i];
                    while(iter)
                    {
                        findContacts(i, iter);
                        iter = iter->next;
                    }
                }

                block->doneSpread = true;
            }
        }
}

/**
 * Perform any processing needed before we can draw surfaces within the
 * specified subsector with dynamic lights.
 *
 * @param subSector          Ptr to the subsector to process.
 */
void Subsector_SpreadObjLinks(subsector_t* subsector)
{
    float maxRadius = MAX_OF(DDMOBJ_RADIUS_MAX, loMaxRadius);

    assert(subsector);

BEGIN_PROF( PROF_OBJLINK_SPREAD );

    // Make sure we know which objs are contacting us.
    // @todo Subsector should return the map its linked to.
    ObjBlockmap_Spread(Map_ObjBlockmap(P_CurrentMap()), subsector, maxRadius);

END_PROF( PROF_OBJLINK_SPREAD );
}

static void addObjLink(objblockmap_t* obm, objtype_t type, objlink_t* oLink, float x, float y)
{
    int bx, by;
    objlink_t** root;

    assert(obm);
    assert(oLink);

    oLink->next = NULL;
    bx = X_TO_OBBX(obm, FLT2FIX(x));
    by = Y_TO_OBBY(obm, FLT2FIX(y));

    if(bx >= 0 && by >= 0 && bx < obm->width && by < obm->height)
    {
        root = &OBB_XY(obm, bx, by)->head[type];
        oLink->next = *root;
        *root = oLink;
    }
}

/**
 * Link all objlinks into the objlink blockmap.
 */
void ObjBlockmap_Add(objblockmap_t* obm, objtype_t type, void* obj)
{
    objlink_t* ol;
#ifdef DD_PROFILE
    static int i;
    if(++i > 40)
    {
        i = 0;
        PRINT_PROF(PROF_OBJLINK_SPREAD);
        PRINT_PROF(PROF_OBJLINK_LINK);
    }
#endif

    assert(obm);
    assert(obj);
    assert(type >= OT_FIRST && type < NUM_OBJ_TYPES);

BEGIN_PROF( PROF_OBJLINK_LINK );

    // Link objlink into the objlink blockmap.

    if((ol = allocObjLink()))
    {
        vec2_t pos;

        switch(type)
        {
        case OT_LUMOBJ:
            V2_Copy(pos, ((lumobj_t*) obj)->pos);
            break;

        case OT_MOBJ:
            V2_Copy(pos, ((mobj_t*) obj)->pos);
            break;

        case OT_PARTICLE:
            V2_SetFixed(pos, ((particle_t*) obj)->pos[VX], ((particle_t*) obj)->pos[VY]);
            break;

        default:
            Con_Error("Internal Error: Invalid objtype (%i) in Map_LinkObjs.", (int) type);
            return; // Unreachable.
        }

        ol->obj = obj;

        addObjLink(obm, type, ol, pos[VX], pos[VY]);
    }
    else
        Con_Error("Map_CreateObjLink: No more links.");

END_PROF( PROF_OBJLINK_LINK );
}
