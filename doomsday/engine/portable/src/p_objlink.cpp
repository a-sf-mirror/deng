/**
 * @file p_objlink.cpp
 * Objlink management. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_defs.h"

#include "gridmap.h"

#define BLOCK_WIDTH                 (128)
#define BLOCK_HEIGHT                (128)

BEGIN_PROF_TIMERS()
  PROF_OBJLINK_SPREAD,
  PROF_OBJLINK_LINK
END_PROF_TIMERS()

struct objlink_t
{
    objlink_t* nextInBlock; /// Next in the same obj block, or NULL.
    objlink_t* nextUsed;
    objlink_t* next; /// Next in list of ALL objlinks.
    objtype_t type;
    void* obj;
};

struct objlinkblock_t
{
    objlink_t* head;
    /// Used to prevent repeated per-frame processing of a block.
    bool doneSpread;
};

struct objlinkblockmap_t
{
    coord_t origin[2]; /// Origin of the blockmap in world coordinates [x,y].
    Gridmap* gridmap;
};

struct contactfinderparams_t
{
    void* obj;
    objtype_t objType;
    coord_t objOrigin[3];
    coord_t objRadius;
    coord_t box[4];
};

struct objcontact_t
{
    objcontact_t* next; /// Next in the BSP leaf.
    objcontact_t* nextUsed; /// Next used contact.
    void* obj;
};

struct objcontactlist_t
{
    objcontact_t* head[NUM_OBJ_TYPES];
};

static objlink_t* objlinks;
static objlink_t* objlinkFirst, *objlinkCursor;

// Each objlink type gets its own blockmap.
static objlinkblockmap_t blockmaps[NUM_OBJ_TYPES];

// List of unused and used contacts.
static objcontact_t* contFirst, *contCursor;

// List of contacts for each BSP leaf.
static objcontactlist_t* bspLeafContacts;

static void processSeg(HEdge* hedge, void* data);

static inline objlinkblockmap_t* chooseObjlinkBlockmap(objtype_t type)
{
    DENG2_ASSERT(VALID_OBJTYPE(type));
    return blockmaps + (int)type;
}

static inline GridmapCoord toObjlinkBlockmapX(objlinkblockmap_t* obm, coord_t x)
{
    DENG2_ASSERT(obm && x >= obm->origin[0]);
    return uint((x - obm->origin[0]) / coord_t(BLOCK_WIDTH));
}

static inline GridmapCoord toObjlinkBlockmapY(objlinkblockmap_t* obm, coord_t y)
{
    DENG2_ASSERT(obm && y >= obm->origin[1]);
    return uint((y - obm->origin[1]) / coord_t(BLOCK_HEIGHT));
}

/**
 * Given world coordinates @a x, @a y, determine the objlink blockmap block
 * [x, y] it resides in. If the coordinates are outside the blockmap they
 * are clipped within valid range.
 *
 * @return  @c true if the coordinates specified had to be adjusted.
 */
static bool toObjlinkBlockmapCell(objlinkblockmap_t* obm, GridmapCell& mcell, coord_t x, coord_t y)
{
    DENG2_ASSERT(obm);

    const GridmapCell& size = obm->gridmap->widthHeight();
    coord_t max[2] = { obm->origin[0] + size[0] * BLOCK_WIDTH,
                       obm->origin[1] + size[1] * BLOCK_HEIGHT};

    bool adjusted = false;
    if(x < obm->origin[0])
    {
        mcell[0] = 0;
        adjusted = true;
    }
    else if(x >= max[0])
    {
        mcell[0] = size[0]-1;
        adjusted = true;
    }
    else
    {
        mcell[0] = toObjlinkBlockmapX(obm, x);
    }

    if(y < obm->origin[1])
    {
        mcell[1] = 0;
        adjusted = true;
    }
    else if(y >= max[1])
    {
        mcell[1] = size[1]-1;
        adjusted = true;
    }
    else
    {
        mcell[1] = toObjlinkBlockmapY(obm, y);
    }
    return adjusted;
}

static inline void linkContact(objcontact_t* con, objcontact_t** list, uint index)
{
    con->next = list[index];
    list[index] = con;
}

static void linkContactToBspLeaf(objcontact_t* node, objtype_t type, uint index)
{
    linkContact(node, &bspLeafContacts[index].head[type], 0);
}

/**
 * Create a new objcontact. If there are none available in the list of
 * used objects a new one will be allocated and linked to the global list.
 */
static objcontact_t* allocObjContact(void)
{
    objcontact_t* con;
    if(!contCursor)
    {
        con = (objcontact_t*) Z_Malloc(sizeof *con, PU_APPSTATIC, NULL);

        // Link to the list of objcontact nodes.
        con->nextUsed = contFirst;
        contFirst = con;
    }
    else
    {
        con = contCursor;
        contCursor = contCursor->nextUsed;
    }
    con->obj = 0;
    return con;
}

static objlink_t* allocObjlink(void)
{
    objlink_t* link;
    if(!objlinkCursor)
    {
        link = (objlink_t*) Z_Malloc(sizeof *link, PU_APPSTATIC, NULL);

        // Link the link to the global list.
        link->nextUsed = objlinkFirst;
        objlinkFirst = link;
    }
    else
    {
        link = objlinkCursor;
        objlinkCursor = objlinkCursor->nextUsed;
    }
    link->nextInBlock = 0;
    link->obj = 0;

    // Link it to the list of in-use objlinks.
    link->next = objlinks;
    objlinks = link;

    return link;
}

void R_InitObjlinkBlockmapForMap(void)
{
    GameMap* map = theMap;

    // Determine the dimensions of the objlink blockmaps in blocks.
    coord_t min[2], max[2];
    GameMap_Bounds(map, min, max);
    uint width  = uint( ceil((max[VX] - min[VX]) / coord_t(BLOCK_WIDTH)) );
    uint height = uint( ceil((max[VY] - min[VY]) / coord_t(BLOCK_HEIGHT)) );

    // Create the blockmaps.
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap(objtype_t(i));
        obm->origin[0] = min[VX];
        obm->origin[1] = min[VY];
        obm->gridmap = new Gridmap(width, height);
    }

    // Initialize obj => BspLeaf contact lists.
    bspLeafContacts = (objcontactlist_t*)Z_Calloc(sizeof *bspLeafContacts * NUM_BSPLEAFS, PU_MAPSTATIC, 0);
}

void R_DestroyObjlinkBlockmap(void)
{
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap(objtype_t(i));
        if(!obm->gridmap) continue;
        delete obm->gridmap;
        obm->gridmap = 0;
    }
    if(bspLeafContacts)
    {
        Z_Free(bspLeafContacts);
        bspLeafContacts = 0;
    }
}

static int clearObjlinkBlock(void* obj, void* /*parameters*/)
{
    objlinkblock_t* block = (objlinkblock_t*)obj;
    block->head = NULL;
    block->doneSpread = false;
    return false; // Continue iteration.
}

void R_ClearObjlinkBlockmap(objtype_t type)
{
    if(!VALID_OBJTYPE(type))
    {
#if _DEBUG
        Con_Error("R_ClearObjlinkBlockmap: Attempted with invalid type %i.", (int)type);
#endif
        return;
    }
    // Clear all the contact list heads and spread flags.
    objlinkblockmap_t* bm = chooseObjlinkBlockmap(type);
    bm->gridmap->iterate(clearObjlinkBlock);
}

void R_ClearObjlinksForFrame(void)
{
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap(objtype_t(i));
        if(!obm->gridmap) continue;
        R_ClearObjlinkBlockmap(objtype_t(i));
    }

    // Start reusing objlinks.
    objlinkCursor = objlinkFirst;
    objlinks = NULL;
}

void R_ObjlinkCreate(void* obj, objtype_t type)
{
    objlink_t* link = allocObjlink();
    link->obj = obj;
    link->type = type;
}

int RIT_LinkObjToBspLeaf(BspLeaf* bspLeaf, void* parameters)
{
    const linkobjtobspleafparams_t* p = (linkobjtobspleafparams_t*) parameters;
    objcontact_t* con = allocObjContact();

    con->obj = p->obj;
    // Link the contact list for this bspLeaf.
    linkContactToBspLeaf(con, p->type, GET_BSPLEAF_IDX(bspLeaf));

    return false; // Continue iteration.
}

/**
 * Attempt to spread the obj from the given contact from the source
 * BspLeaf and into the (relative) back BspLeaf.
 *
 * @param bspLeaf  BspLeaf to attempt to spread over to.
 * @param parameters  @see contactfinderparams_t
 *
 * @return  Always @c true. (This function is also used as an iterator.)
 */
static void spreadInBspLeaf(BspLeaf* bspLeaf, void* parameters)
{
    if(!bspLeaf || !bspLeaf->hedge) return;

    HEdge* hedge = bspLeaf->hedge;
    do
    {
        processSeg(hedge, parameters);
    } while((hedge = hedge->next) != bspLeaf->hedge);
}

static void processSeg(HEdge* hedge, void* parameters)
{
    contactfinderparams_t* p = (contactfinderparams_t*) parameters;

    // HEdge must be between two different BspLeafs.
    if(/*hedge->lineDef $degenleaf &&*/
       (!hedge->twin || hedge->bspLeaf == hedge->twin->bspLeaf))
        return;

    // Which way does the spread go?
    if(!(hedge->bspLeaf->validCount == validCount &&
         hedge->twin->bspLeaf->validCount != validCount))
    {
        // Not eligible for spreading.
        return;
    }

    BspLeaf* source = hedge->bspLeaf;
    BspLeaf* dest   = hedge->twin->bspLeaf;

    // Is the dest BspLeaf outside the objlink's AABB?
    if(dest->aaBox.maxX <= p->box[BOXLEFT]   ||
       dest->aaBox.minX >= p->box[BOXRIGHT]  ||
       dest->aaBox.maxY <= p->box[BOXBOTTOM] ||
       dest->aaBox.minY >= p->box[BOXTOP])
    {
        return;
    }

    // Can the spread happen?
    if(hedge->lineDef)
    {
        if(dest->sector)
        {
            if(dest->sector->planes[PLN_CEILING]->height <= dest->sector->planes[PLN_FLOOR]->height ||
               dest->sector->planes[PLN_CEILING]->height <= source->sector->planes[PLN_FLOOR]->height ||
               dest->sector->planes[PLN_FLOOR]->height   >= source->sector->planes[PLN_CEILING]->height)
            {
                // No; destination sector is closed with no height.
                return;
            }
        }

        // Don't spread if the middle material completely fills the gap between
        // floor and ceiling (direction is from dest to source).
        if(R_MiddleMaterialCoversOpening(hedge->lineDef,
            dest == hedge->twin->bspLeaf? false : true, false))
            return;
    }

    // Calculate 2D distance to hedge.
    const coord_t dx = hedge->HE_v2origin[VX] - hedge->HE_v1origin[VX];
    const coord_t dy = hedge->HE_v2origin[VY] - hedge->HE_v1origin[VY];
    Vertex* vtx = hedge->HE_v1;

    coord_t distance = ((vtx->origin[VY] - p->objOrigin[VY]) * dx -
                        (vtx->origin[VX] - p->objOrigin[VX]) * dy) / hedge->length;

    if(hedge->lineDef)
    {
        if((source == hedge->bspLeaf && distance < 0) ||
           (source == hedge->twin->bspLeaf && distance > 0))
        {
            // Can't spread in this direction.
            return;
        }
    }

    if(distance < 0)
        distance = -distance;

    // Check distance against the obj radius.
    if(distance >= p->objRadius)
    {
        // The obj doesn't reach that far.
        return;
    }

    // During next step, obj will continue spreading from there.
    dest->validCount = validCount;

    // Add this obj to the destination BspLeaf.
    linkobjtobspleafparams_t loParams;
    loParams.obj   = p->obj;
    loParams.type = p->objType;
    RIT_LinkObjToBspLeaf(dest, &loParams);

    spreadInBspLeaf(dest, parameters);
}

/**
 * Create a contact for the objlink in all the BspLeafs the linked obj is
 * contacting (tests done on bounding boxes and the BSP leaf spread test).
 *
 * @param oLink Ptr to objlink to find BspLeaf contacts for.
 */
static void findContacts(objlink_t* link)
{
    coord_t radius;
    pvec3d_t origin;
    BspLeaf** ssecAdr;

    switch(link->type)
    {
    case OT_LUMOBJ: {
        lumobj_t* lum = reinterpret_cast<lumobj_t*>(link->obj);
        // Only omni lights spread.
        if(lum->type != LT_OMNI) return;

        origin = lum->origin;
        radius = LUM_OMNI(lum)->radius;
        ssecAdr = &lum->bspLeaf;
        break; }

    case OT_MOBJ: {
        mobj_t* mo = reinterpret_cast<mobj_t*>(link->obj);

        origin = mo->origin;
        radius = R_VisualRadius(mo);
        ssecAdr = &mo->bspLeaf;
        break; }

    default:
        Con_Error("findContacts: Invalid objtype %i.", (int) link->type);
        exit(1); // Unreachable.
    }

    // Do the BSP leaf spread. Begin from the obj's own BspLeaf.
    (*ssecAdr)->validCount = ++validCount;

    contactfinderparams_t cfParams;
    cfParams.obj = link->obj;
    cfParams.objType = link->type;
    V3d_Copy(cfParams.objOrigin, origin);
    // Use a slightly smaller radius than what the obj really is.
    cfParams.objRadius = radius * .98f;

    cfParams.box[BOXLEFT]   = cfParams.objOrigin[VX] - radius;
    cfParams.box[BOXRIGHT]  = cfParams.objOrigin[VX] + radius;
    cfParams.box[BOXBOTTOM] = cfParams.objOrigin[VY] - radius;
    cfParams.box[BOXTOP]    = cfParams.objOrigin[VY] + radius;

    // Always contact the obj's own BspLeaf.
    linkobjtobspleafparams_t loParams;
    loParams.obj = link->obj;
    loParams.type = link->type;
    RIT_LinkObjToBspLeaf(*ssecAdr, &loParams);

    spreadInBspLeaf(*ssecAdr, &cfParams);
}

/**
 * Spread contacts in the object => BspLeaf objlink blockmap to all
 * other BspLeafs within the block.
 *
 * @param obm        Objlink blockmap.
 * @param bspLeaf    BspLeaf to spread the contacts of.
 * @param maxRadius  Maximum radius for the spread.
 */
void R_ObjlinkBlockmapSpreadInBspLeaf(objlinkblockmap_t* obm, const BspLeaf* bspLeaf, float maxRadius)
{
    DENG2_ASSERT(obm);
    if(!bspLeaf) return; // Wha?

    GridmapCell minBlock;
    toObjlinkBlockmapCell(obm, minBlock, bspLeaf->aaBox.minX - maxRadius,
                                         bspLeaf->aaBox.minY - maxRadius);

    GridmapCell maxBlock;
    toObjlinkBlockmapCell(obm, maxBlock, bspLeaf->aaBox.maxX + maxRadius,
                                         bspLeaf->aaBox.maxY + maxRadius);

    GridmapCell mcell;
    objlink_t* iter;
    for(mcell[1] = minBlock[1]; mcell[1] <= maxBlock[1]; ++mcell[1])
    for(mcell[0] = minBlock[0]; mcell[0] <= maxBlock[0]; ++mcell[0])
    {
        if(!obm->gridmap->leafAtCell(mcell)) continue;

        objlinkblock_t* block = reinterpret_cast<objlinkblock_t*>(obm->gridmap->cell(mcell));
        DENG2_ASSERT(block);
        if(block->doneSpread) continue;

        iter = block->head;
        while(iter)
        {
            findContacts(iter);
            iter = iter->nextInBlock;
        }
        block->doneSpread = true;
    }
}

static inline const float maxRadius(objtype_t type)
{
    DENG2_ASSERT(VALID_OBJTYPE(type));
    if(type == OT_MOBJ) return DDMOBJ_RADIUS_MAX;
    // Must be OT_LUMOBJ
    return loMaxRadius;
}

void R_InitForBspLeaf(BspLeaf* bspLeaf)
{
BEGIN_PROF( PROF_OBJLINK_SPREAD );

    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        objlinkblockmap_t* obm = chooseObjlinkBlockmap(objtype_t(i));
        R_ObjlinkBlockmapSpreadInBspLeaf(obm, bspLeaf, maxRadius(objtype_t(i)));
    }

END_PROF( PROF_OBJLINK_SPREAD );
}

/// @pre  Coordinates held by @a blockXY are within valid range.
static void linkObjlinkInBlockmap(objlinkblockmap_t* obm, objlink_t* link, GridmapCell& mcell)
{
    DENG2_ASSERT(obm && link);
    objlinkblock_t* block;
    if(obm->gridmap->leafAtCell(mcell))
    {
        block = reinterpret_cast<objlinkblock_t*>(obm->gridmap->cell(mcell));
        DENG2_ASSERT(block);
    }
    else
    {
        block = (objlinkblock_t*)Z_Calloc(sizeof(*block), PU_MAPSTATIC, 0);
        obm->gridmap->setCell(mcell, block);
    }
    link->nextInBlock = block->head;
    block->head = link;
}

void R_LinkObjs(void)
{
    objlinkblockmap_t* obm;
    objlink_t* link;
    GridmapCell mcell;
    pvec3d_t origin;

BEGIN_PROF( PROF_OBJLINK_LINK );

    // Link objlinks into the objlink blockmap.
    link = objlinks;
    while(link)
    {
        switch(link->type)
        {
        case OT_LUMOBJ:     origin = reinterpret_cast<lumobj_t*>(link->obj)->origin; break;
        case OT_MOBJ:       origin = reinterpret_cast<mobj_t*>(link->obj)->origin; break;
        default:
            Con_Error("R_LinkObjs: Invalid objtype %i.", (int) link->type);
            exit(1); // Unreachable.
        }

        obm = chooseObjlinkBlockmap(link->type);
        if(!toObjlinkBlockmapCell(obm, mcell, origin[VX], origin[VY]))
        {
            linkObjlinkInBlockmap(obm, link, mcell);
        }
        link = link->next;
    }

END_PROF( PROF_OBJLINK_LINK );
}

void R_InitForNewFrame(void)
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

    // Start reusing nodes from the first one in the list.
    contCursor = contFirst;
    if(bspLeafContacts)
        memset(bspLeafContacts, 0, NUM_BSPLEAFS * sizeof *bspLeafContacts);
}

int R_IterateBspLeafContacts2(BspLeaf* bspLeaf, objtype_t type,
    int (*callback) (void* object, void* parameters), void* parameters)
{
    objcontact_t* con = bspLeafContacts[GET_BSPLEAF_IDX(bspLeaf)].head[type];
    int result = false; // Continue iteration.
    while(con)
    {
        result = callback(con->obj, parameters);
        if(result) break;
        con = con->next;
    }
    return result;
}

int R_IterateBspLeafContacts(BspLeaf* bspLeaf, objtype_t type,
    int (*callback) (void* object, void* parameters))
{
    return R_IterateBspLeafContacts2(bspLeaf, type, callback, NULL/*no parameters*/);
}
