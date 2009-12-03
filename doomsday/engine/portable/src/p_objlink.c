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
    struct objlink_s* nextInBlock; // Next in the same obj block, or NULL.
    struct objlink_s* nextUsed;
    struct objlink_s* next; // Next in list of ALL objlinks.
    objtype_t       type;
    void*           obj;
} objlink_t;

typedef struct objblock_s {
    objlink_t*      head;

    // Used to prevent multiple processing of a block during one refresh frame.
    boolean         doneSpread;
} objblock_t;

typedef struct objblockmap_s {
    objblock_t*     blocks;
    fixed_t         origin[2];
    int             width, height; // In blocks.
} objblockmap_t;

typedef struct contactfinder_data_s {
    void*           obj;
    objtype_t       objType;
    vec3_t          objPos;
    float           objRadius;
    float           box[4];
} contactfinderparams_t;

typedef struct objcontact_s {
    struct objcontact_s* next; // Next in the subsector.
    struct objcontact_s* nextUsed; // Next used contact.
    void*           obj;
} objcontact_t;

typedef struct objcontactlist_s {
    objcontact_t*   head[NUM_OBJ_TYPES];
} objcontactlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void processSeg(hedge_t* seg, void* data);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static objlink_t* objLinks = NULL;
static objlink_t* objLinkFirst = NULL, *objLinkCursor = NULL;

static objblockmap_t* objBlockmap = NULL;

// List of unused and used obj-face contacts.
static objcontact_t* contFirst = NULL, *contCursor = NULL;

// List of obj contacts for each face.
static objcontactlist_t* subsectorContacts = NULL;

// CODE --------------------------------------------------------------------

/**
 * Link the given objcontact node to list.
 */
static __inline void linkContact(objcontact_t* con, objcontact_t** list,
                                 uint index)
{
    con->next = list[index];
    list[index] = con;
}

static void linkContactToFace(objcontact_t* node, objtype_t type,
                              uint index)
{
    linkContact(node, &subsectorContacts[index].head[type], 0);
}

/**
 * Create a new objcontact for the given obj. If there are free nodes in
 * the list of unused nodes, the new contact is taken from there.
 *
 * @param obj           Ptr to the obj the contact is required for.
 *
 * @return              Ptr to the new objcontact.
 */
static objcontact_t* allocObjContact(void)
{
    objcontact_t*       con;

    if(contCursor == NULL)
    {
        con = Z_Malloc(sizeof(*con), PU_STATIC, NULL);

        // Link to the list of objcontact nodes.
        con->nextUsed = contFirst;
        contFirst = con;
    }
    else
    {
        con = contCursor;
        contCursor = contCursor->nextUsed;
    }

    con->obj = NULL;

    return con;
}

static objlink_t* allocObjLink(void)
{
    objlink_t*          oLink;

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

    oLink->nextInBlock = NULL;
    oLink->obj = NULL;

    // Link it to the list of in-use objlinks.
    oLink->next = objLinks;
    objLinks = oLink;

    return oLink;
}

objblockmap_t* R_ObjBlockmapCreate(float originX, float originY, int width,
                                   int height)
{
    objblockmap_t* obm;

    obm = Z_Malloc(sizeof(objblockmap_t), PU_MAP, 0);

    // Origin has fixed-point coordinates.
    obm->origin[0] = FLT2FIX(originX);
    obm->origin[1] = FLT2FIX(originY);
    obm->width = width;
    obm->height = height;
    obm->blocks = Z_Malloc(sizeof(*obm->blocks) * obm->width * obm->height, PU_MAP, 0);

    return obm;
}

void R_InitObjLinksForMap(map_t* map)
{
    float min[2], max[2];
    int width, height;

    if(!map)
        return;

    // Determine the dimensions of the objblockmap.
    Map_Bounds(map, &min[0], &max[0]);
    max[0] -= min[0];
    max[1] -= min[1];

    // In blocks, 128x128 world units.
    width  = (FLT2FIX(max[0]) >> (FRACBITS + 7)) + 1;
    height = (FLT2FIX(max[1]) >> (FRACBITS + 7)) + 1;

    // Create the blockmap.
    objBlockmap = R_ObjBlockmapCreate(min[0], min[1], width, height);

    // Initialize face -> obj contact lists.
    subsectorContacts = Z_Calloc(sizeof(*subsectorContacts) * map->numSubsectors, PU_STATIC, 0);
}

void R_ObjBlockmapClear(objblockmap_t* obm)
{
    if(obm)
    {   // Clear the list head ptrs and doneSpread flags.
        memset(obm->blocks, 0, sizeof(*obm->blocks) * obm->width * obm->height);
    }
}

/**
 * Called at the begining of each frame (iff the render lists are not frozen)
 * by R_BeginWorldFrame().
 */
void R_ClearObjLinksForFrame(map_t* map)
{
    if(!map)
        return;

    // Clear all the roots.
    R_ObjBlockmapClear(objBlockmap);

    objLinkCursor = objLinkFirst;
    objLinks = NULL;
}

void R_ObjLinkCreate(void* obj, objtype_t type)
{
    objlink_t* ol = allocObjLink();

    ol->obj = obj;
    ol->type = type;
}

boolean RIT_LinkObjToSubsector(subsector_t* subsector, void* data)
{
    linkobjtosubSectorparams_t* params = (linkobjtosubSectorparams_t*) data;
    objcontact_t* con = allocObjContact();

    con->obj = params->obj;

    // Link the contact list for this face.
    linkContactToFace(con, params->type, P_ObjectRecord(DMU_SUBSECTOR, subsector)->id - 1);

    return true; // Continue iteration.
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
    {
    linkobjtosubSectorparams_t lparams;

    lparams.obj = params->obj;
    lparams.type = params->objType;

    RIT_LinkObjToSubsector(dst, &lparams);
    }

    spreadInSubsector(dst, data);
}

/**
 * Create a contact for the objlink in all the subsectors the linked obj is
 * contacting (tests done on bounding boxes and the subsector spread test).
 *
 * @param oLink         Ptr to objlink to find subsector contacts for.
 */
static void findContacts(objlink_t* oLink)
{
    contactfinderparams_t params;
    float radius;
    pvec3_t pos;
    subsector_t* subsector;

    switch(oLink->type)
    {
    case OT_LUMOBJ:
        {
        lumobj_t* lum = (lumobj_t*) oLink->obj;

        if(lum->type != LT_OMNI)
            return; // Only omni lights spread.

        pos = lum->pos;
        radius = LUM_OMNI(lum)->radius;
        subsector = lum->subsector;
        break;
        }
    case OT_MOBJ:
        {
        mobj_t* mo = (mobj_t*) oLink->obj;

        pos = mo->pos;
        radius = R_VisualRadius(mo);
        subsector = (subsector_t*) ((objectrecord_t*) mo->subsector)->obj;
        break;
        }
    default:
        Con_Error("Internal Error: Invalid value (%i) for objlink_t->type "
                  "in findContacts.", (int) oLink->type);
        return; // Unreachable.
    }

    // Do the subsector spread. Begin from the obj's own subSector.
    subsector->validCount = ++validCount;

    params.obj = oLink->obj;
    params.objType = oLink->type;
    V3_Copy(params.objPos, pos);
    // Use a slightly smaller radius than what the obj really is.
    params.objRadius = radius * .9f;

    params.box[BOXLEFT]   = params.objPos[VX] - radius;
    params.box[BOXRIGHT]  = params.objPos[VX] + radius;
    params.box[BOXBOTTOM] = params.objPos[VY] - radius;
    params.box[BOXTOP]    = params.objPos[VY] + radius;

    // Always contact the obj's own subsector.
    {
    linkobjtosubSectorparams_t params;

    params.obj = oLink->obj;
    params.type = oLink->type;

    RIT_LinkObjToSubsector(subsector, &params);
    }

    spreadInSubsector(subsector, &params);
}

/**
 * Spread obj contacts in the subsector > obj blockmap to all other
 * subsectors within the block.
 *
 * @param subsector     Ptr to the subsector to spread the obj contacts of.
 */
void R_ObjBlockmapSpreadObjsInSubsector(const objblockmap_t* obm,
                                        const subsector_t* subsector,
                                        float maxRadius)
{
    int xl, xh, yl, yh, x, y;
    objlink_t* iter;

    if(!obm || !subsector)
        return; // Wha?

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
                // Spread the objs in this block.
                iter = block->head;
                while(iter)
                {
                    findContacts(iter);
                    iter = iter->nextInBlock;
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
void R_InitForSubsector(subsector_t* subsector)
{
    float maxRadius = MAX_OF(DDMOBJ_RADIUS_MAX, loMaxRadius);

BEGIN_PROF( PROF_OBJLINK_SPREAD );

    // Make sure we know which objs are contacting us.
    R_ObjBlockmapSpreadObjsInSubsector(objBlockmap, subsector, maxRadius);

END_PROF( PROF_OBJLINK_SPREAD );
}

/**
 * Link all objlinks into the objlink blockmap.
 */
void R_ObjBlockmapLinkObjLink(objblockmap_t* obm, objlink_t* oLink, float x, float y)
{
    int bx, by;
    objlink_t** root;

    if(!obm || !oLink)
        return; // Wha?

    oLink->nextInBlock = NULL;
    bx = X_TO_OBBX(obm, FLT2FIX(x));
    by = Y_TO_OBBY(obm, FLT2FIX(y));

    if(bx >= 0 && by >= 0 && bx < obm->width && by < obm->height)
    {
        root = &OBB_XY(obm, bx, by)->head;
        oLink->nextInBlock = *root;
        *root = oLink;
    }
}

/**
 * Called by R_BeginWorldFrame() at the beginning of render tic (iff the
 * render lists are not frozen) to link all objlinks into the objlink
 * blockmap.
 */
void R_LinkObjs(map_t* map)
{
    objlink_t* oLink;

    if(!map)
        return;

BEGIN_PROF( PROF_OBJLINK_LINK );

    // Link objlinks into the objlink blockmap.
    oLink = objLinks;
    while(oLink)
    {
        pvec3_t pos;

        switch(oLink->type)
        {
        case OT_LUMOBJ:
            pos = ((lumobj_t*) oLink->obj)->pos;
            break;

        case OT_MOBJ:
            pos = ((mobj_t*) oLink->obj)->pos;
            break;
        default:
            Con_Error("Internal Error: Invalid value (%i) for objlink_t->type "
                      "in R_LinkObjs.", (int) oLink->type);
            return; // Unreachable.
        }

        R_ObjBlockmapLinkObjLink(objBlockmap, oLink, pos[VX], pos[VY]);

        oLink = oLink->next;
    }

END_PROF( PROF_OBJLINK_LINK );
}

/**
 * Initialize the obj > subsector contact lists ready for adding new
 * luminous objects. Called by R_BeginWorldFrame() at the beginning of a new
 * frame (if the render lists are not frozen).
 */
void R_InitForNewFrame(map_t* map)
{
#ifdef DD_PROFILE
    static int i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF(PROF_OBJLINK_SPREAD);
        PRINT_PROF(PROF_OBJLINK_LINK);
    }
#endif

    if(!map)
        return;

    // Start reusing nodes from the first one in the list.
    contCursor = contFirst;
    if(subsectorContacts)
        memset(subsectorContacts, 0, map->numSubsectors * sizeof(*subsectorContacts));
}

boolean R_IterateSubsectorContacts(subsector_t* subsector, objtype_t type,
                                   boolean (*func) (void*, void*),
                                   void* data)
{
    objcontact_t* con;

    con = subsectorContacts[P_ObjectRecord(DMU_SUBSECTOR, subsector)->id - 1].head[type];
    while(con)
    {
        if(!func(con->obj, data))
            return false;

        con = con->next;
    }

    return true;
}
