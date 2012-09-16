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

#include "quadtree.h"

#define BLOCK_WIDTH                 (128)
#define BLOCK_HEIGHT                (128)

BEGIN_PROF_TIMERS()
  PROF_OBJLINK_SPREAD,
  PROF_OBJLINK_LINK
END_PROF_TIMERS()

struct ObjLinkTypeData
{
    objtype_t type;
    union
    {
        mobj_t*   mobj;
        lumobj_t* lumobj;
    } data;

    ObjLinkTypeData(mobj_t* mobj) : type(OT_MOBJ) { data.mobj = mobj; }
    ObjLinkTypeData(lumobj_t* lumobj) : type(OT_LUMOBJ) { data.lumobj = lumobj; }

    inline mobj_t* mobj() const { return data.mobj; }
    inline lumobj_t* lumobj() const { return data.lumobj; }

    ObjLinkTypeData& operator= (const ObjLinkTypeData& other)
    {
        if(this != &other)
        {
            type = other.type;
            memcpy((void*)&data, (void*)&other.data, sizeof(data));
        }
        return *this;
    }

    ObjLinkTypeData& clear()
    {
        type = objtype_t(0);
        memset((void*)&data, 0, sizeof(data));
        return *this;
    }
};

struct ObjLink
{
    ObjLink* nextInCell; /// Next in the same blockmap cell (if any).
    ObjLink* nextUsed;
    ObjLink* next; /// Next in list of ALL objlinks.

    ObjLinkTypeData tdata;

    inline objtype_t type() const { return tdata.type; }

    ObjLink& clear()
    {
        tdata.clear();
        nextInCell = 0;
        return *this;
    }
};

struct ObjlinkCellData
{
    ObjLink* head;
    /// Used to prevent repeated per-frame processing of a block.
    bool doneSpread;
};

static int clearCellDataWorker(ObjlinkCellData* cell, void* /*parameters*/)
{
    Z_Free(cell);
    return false; // Continue iteration.
}

struct ObjlinkBlockmap
{
    coord_t origin[2]; /// Origin of the blockmap in world coordinates [x,y].

    /**
     * Implemented in terms of a region Quadtree for its inherent sparsity,
     * spacial cohession and compression potential.
     */
    typedef Quadtree<ObjlinkCellData*> DataGrid;
    DataGrid grid;

    ObjlinkBlockmap(coord_t const min[2], coord_t const max[2], uint cellWidth, uint cellHeight)
        : grid(uint( ceil((max[0] - min[0]) / coord_t(cellWidth)) ),
               uint( ceil((max[1] - min[1]) / coord_t(cellHeight))) )
    {
        origin[0] = min[0];
        origin[1] = min[1];
    }

    ~ObjlinkBlockmap()
    {
        iterate(clearCellDataWorker);
    }

    bool leafAtCell(const_QuadtreeCell mcell)
    {
        if(!grid.leafAtCell(mcell)) return false;
        return !!grid.cell(mcell);
    }
    inline bool leafAtCell(QuadtreeCoord x, QuadtreeCoord y)
    {
        QuadtreeCell mcell = { x, y };
        return leafAtCell(mcell);
    }

    /**
     * Retrieve the user data associated with the identified cell.
     *
     * @param mcells         XY coordinates of the cell whose data to retrieve.
     *
     * @return  User data for the identified cell.
     */
    ObjlinkCellData* cell(const_QuadtreeCell mcell)
    {
        if(!grid.leafAtCell(mcell)) return 0;
        return grid.cell(mcell);
    }
    inline ObjlinkCellData* cell(QuadtreeCoord x, QuadtreeCoord y)
    {
        QuadtreeCell mcell = { x, y };
        return cell(mcell);
    }

    void setCell(const_QuadtreeCell mcell, ObjlinkCellData* userData)
    {
        grid.setCell(mcell, userData);
    }
    inline void setCell(QuadtreeCoord x, QuadtreeCoord y, ObjlinkCellData* userData)
    {
        QuadtreeCell mcell = { x, y };
        setCell(mcell, userData);
    }

    typedef int (*IterateCallback) (ObjlinkCellData* cell, void* parameters);
    struct actioncallback_paramaters_t
    {
        IterateCallback callback;
        void* callbackParameters;
    };

    /**
     * Callback actioner. Executes the callback and then returns the result
     * to the current iteration to determine if it should continue.
     */
    static int actionCallback(DataGrid::TreeBase* node, void* parameters)
    {
        DataGrid::TreeLeaf* leaf = static_cast<DataGrid::TreeLeaf*>(node);
        actioncallback_paramaters_t* p = (actioncallback_paramaters_t*) parameters;
        if(leaf->value())
        {
            return p->callback(leaf->value(), p->callbackParameters);
        }
        return 0; // Continue traversal.
    }

    /**
     * Iterate over populated cells in the Gridmap making a callback for each. Iteration ends
     * when all cells have been visited or @a callback returns non-zero.
     *
     * @param parameters     Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int iterate(IterateCallback callback, void* parameters = 0)
    {
        actioncallback_paramaters_t p;
        p.callback = callback;
        p.callbackParameters = parameters;
        return grid.iterateLeafs(actionCallback, (void*)&p);
    }
};

struct ContactLink
{
    ContactLink* next; /// Next in the BSP leaf.
    ContactLink* nextUsed; /// Next used contact.
    void* obj;
};

struct Contacts
{
    ContactLink* lists[NUM_OBJ_TYPES];
};

struct contactfinderparams_t
{
    ObjLinkTypeData* tdata;
    coord_t objOrigin[3];
    coord_t objRadius;
    coord_t box[4];
};

static ObjLink* objlinks;
static ObjLink* objlinkFirst, *objlinkCursor;

// Each objlink type gets its own blockmap.
static ObjlinkBlockmap* blockmaps[NUM_OBJ_TYPES];

// List of unused and used contacts.
static ContactLink* contFirst, *contCursor;

// Lists of contacts for each BSP leaf.
static Contacts* bspLeafContacts;

static void spreadOverHEdge(HEdge* hedge, contactfinderparams_t* params);

static inline ObjlinkBlockmap* blockmapForObjType(objtype_t type)
{
    DENG2_ASSERT(VALID_OBJTYPE(type));
    return blockmaps[int(type)];
}

static inline QuadtreeCoord toObjlinkBlockmapX(ObjlinkBlockmap* obm, coord_t x)
{
    DENG2_ASSERT(obm && x >= obm->origin[0]);
    return uint((x - obm->origin[0]) / coord_t(BLOCK_WIDTH));
}

static inline QuadtreeCoord toObjlinkBlockmapY(ObjlinkBlockmap* obm, coord_t y)
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
static bool toObjlinkBlockmapCell(ObjlinkBlockmap* bm, QuadtreeCell& mcell, coord_t x, coord_t y)
{
    DENG2_ASSERT(bm);

    const QuadtreeCell& size = bm->grid.widthHeight();
    coord_t max[2] = { bm->origin[0] + size[0] * BLOCK_WIDTH,
                       bm->origin[1] + size[1] * BLOCK_HEIGHT};

    bool adjusted = false;
    if(x < bm->origin[0])
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
        mcell[0] = toObjlinkBlockmapX(bm, x);
    }

    if(y < bm->origin[1])
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
        mcell[1] = toObjlinkBlockmapY(bm, y);
    }
    return adjusted;
}

static inline void linkContact(ContactLink* con, ContactLink** list)
{
    con->next = *list;
    *list = con;
}

static void linkContactToBspLeaf(ContactLink* node, objtype_t type, uint index)
{
    linkContact(node, &bspLeafContacts[index].lists[type]);
}

/**
 * Create a new objcontact. If there are none available in the list of
 * used objects a new one will be allocated and linked to the global list.
 */
static ContactLink* allocContactLink(void)
{
    ContactLink* con;
    if(!contCursor)
    {
        con = (ContactLink*) Z_Malloc(sizeof *con, PU_APPSTATIC, NULL);

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

static ObjLink* allocObjlink(void)
{
    ObjLink* link;
    if(!objlinkCursor)
    {
        link = (ObjLink*) Z_Malloc(sizeof *link, PU_APPSTATIC, NULL);

        // Link the link to the global list.
        link->nextUsed = objlinkFirst;
        objlinkFirst = link;
    }
    else
    {
        link = objlinkCursor;
        objlinkCursor = objlinkCursor->nextUsed;
    }

    link->clear();

    // Link it to the list of in-use objlinks.
    link->next = objlinks;
    objlinks = link;

    return link;
}

void R_InitObjlinkBlockmapForMap(void)
{
    GameMap* map = theMap;

    if(!map) return;

    coord_t min[2], max[2];
    GameMap_Bounds(map, min, max);

    // Create the blockmaps.
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        blockmaps[i] = new ObjlinkBlockmap(min, max, BLOCK_WIDTH, BLOCK_WIDTH);
    }

    // Initialize obj => BspLeaf contact lists.
    bspLeafContacts = (Contacts*)Z_Calloc(sizeof *bspLeafContacts * NUM_BSPLEAFS, PU_MAPSTATIC, 0);
}

void R_DestroyObjlinkBlockmaps(void)
{
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        if(!blockmaps[i]) continue;
        delete blockmaps[i]; blockmaps[i] = 0;
    }
    if(bspLeafContacts)
    {
        Z_Free(bspLeafContacts);
        bspLeafContacts = 0;
    }
}

/// Clear all the list and spread flag.
static void ObjlinkCellData_ClearLinks(ObjlinkCellData* cell)
{
    DENG2_ASSERT(cell);
    cell->head = NULL;
    cell->doneSpread = false;
}

static int clearObjlinkCellDataWorker(ObjlinkCellData* cell, void* /*parameters*/)
{
    ObjlinkCellData_ClearLinks(cell);
    return false; // Continue iteration.
}

void R_ClearObjlinkBlockmap(objtype_t type)
{
    ObjlinkBlockmap* bm = blockmapForObjType(type);
    if(!bm) return;

    // Clear all the list heads and spread flags.
    bm->iterate(clearObjlinkCellDataWorker);
}

void R_ClearObjlinksForFrame(void)
{
    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        ObjlinkBlockmap* bm = blockmapForObjType(objtype_t(i));
        if(!bm) continue;

        R_ClearObjlinkBlockmap(objtype_t(i));
    }

    // Start reusing objlinks.
    objlinkCursor = objlinkFirst;
    objlinks = NULL;
}

void R_CreateMobjLink(mobj_t* mobj)
{
    if(!mobj) return;

    ObjLink* link = allocObjlink();
    link->tdata.type = OT_MOBJ;
    link->tdata.data.mobj = mobj;
}

void R_CreateLumobjLink(lumobj_t* lumobj)
{
    if(!lumobj) return;

    ObjLink* link = allocObjlink();
    link->tdata.type = OT_LUMOBJ;
    link->tdata.data.lumobj = lumobj;
}

/**
 * Create a new object => BspLeaf contact in the objlink blockmap.
 * Can be used as an iterator.
 *
 * @params parameters  @see ObjLinkTypeData
 * @return  @c false (always).
 */
static int RIT_LinkObjToBspLeaf(BspLeaf* bspLeaf, void* parameters)
{
    DENG2_ASSERT(bspLeaf && parameters);

    const ObjLinkTypeData* tdata = (ObjLinkTypeData*) parameters;
    ContactLink* con = allocContactLink();

    switch(tdata->type)
    {
    case OT_MOBJ:   con->obj = tdata->mobj(); break;
    case OT_LUMOBJ: con->obj = tdata->lumobj(); break;
    default: exit(1); // Shutup compiler.
    }

    // Link the contact list for this bspLeaf.
    linkContactToBspLeaf(con, tdata->type, GET_BSPLEAF_IDX(bspLeaf));

    return false; // Continue iteration.
}

void R_MobjContactBspLeaf(mobj_t* mobj, BspLeaf* bspLeaf)
{
    if(!mobj || !bspLeaf) return;
    ObjLinkTypeData tdata = ObjLinkTypeData(mobj);
    RIT_LinkObjToBspLeaf(bspLeaf, &tdata);
}

void R_LumobjContactBspLeaf(lumobj_t* lumobj, BspLeaf* bspLeaf)
{
    if(!lumobj || !bspLeaf) return;
    ObjLinkTypeData tdata = ObjLinkTypeData(lumobj);
    RIT_LinkObjToBspLeaf(bspLeaf, &tdata);
}

/**
 * Attempt to spread the obj from the given contact from the source
 * BspLeaf and into the (relative) back BspLeaf.
 *
 * @param bspLeaf   BspLeaf to attempt to spread over to.
 * @param parms     @see contactfinderparams_t
 */
static void spreadInBspLeaf(BspLeaf* bspLeaf, contactfinderparams_t* parms)
{
    DENG2_ASSERT(bspLeaf && parms);
    if(!bspLeaf->hedge) return;

    HEdge* hedge = bspLeaf->hedge;
    do
    {
        spreadOverHEdge(hedge, parms);
    } while((hedge = hedge->next) != bspLeaf->hedge);
}

static void spreadOverHEdge(HEdge* hedge, contactfinderparams_t* parms)
{
    DENG2_ASSERT(hedge && parms);

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
    if(dest->aaBox.maxX <= parms->box[BOXLEFT]   ||
       dest->aaBox.minX >= parms->box[BOXRIGHT]  ||
       dest->aaBox.maxY <= parms->box[BOXBOTTOM] ||
       dest->aaBox.minY >= parms->box[BOXTOP])
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

    coord_t distance = ((vtx->origin[VY] - parms->objOrigin[VY]) * dx -
                        (vtx->origin[VX] - parms->objOrigin[VX]) * dy) / hedge->length;

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
    if(distance >= parms->objRadius)
    {
        // The obj doesn't reach that far.
        return;
    }

    // During next step, obj will continue spreading from there.
    dest->validCount = validCount;

    // Add this obj to the destination BspLeaf.
    RIT_LinkObjToBspLeaf(dest, parms->tdata);

    spreadInBspLeaf(dest, parms);
}

/**
 * Create a contact for the objlink in all the BspLeafs the linked obj is
 * contacting (tests done on bounding boxes and the BSP leaf spread test).
 *
 * @param oLink Ptr to objlink to find BspLeaf contacts for.
 */
static void findContacts(ObjLink* ol)
{
    DENG2_ASSERT(ol);

    coord_t radius;
    pvec3d_t origin;
    BspLeaf* bspLeaf;

    switch(ol->type())
    {
    case OT_LUMOBJ: {
        lumobj_t* lum = ol->tdata.lumobj();
        // Only omni lights spread.
        if(lum->type != LT_OMNI) return;

        origin  = lum->origin;
        radius  = LUM_OMNI(lum)->radius;
        bspLeaf = lum->bspLeaf;
        break; }

    case OT_MOBJ: {
        mobj_t* mo = ol->tdata.mobj();

        origin  = mo->origin;
        radius  = R_VisualRadius(mo);
        bspLeaf = mo->bspLeaf;
        break; }

    default: exit(1); // Shutup compiler.
    }
    DENG2_ASSERT(bspLeaf);

    // Do the BSP leaf spread. Begin from the obj's own BspLeaf.
    bspLeaf->validCount = ++validCount;

    contactfinderparams_t cfParams;
    cfParams.tdata   = &ol->tdata;
    V3d_Copy(cfParams.objOrigin, origin);
    // Use a slightly smaller radius than what the obj really is.
    cfParams.objRadius = radius * .98f;

    cfParams.box[BOXLEFT]   = cfParams.objOrigin[VX] - radius;
    cfParams.box[BOXRIGHT]  = cfParams.objOrigin[VX] + radius;
    cfParams.box[BOXBOTTOM] = cfParams.objOrigin[VY] - radius;
    cfParams.box[BOXTOP]    = cfParams.objOrigin[VY] + radius;

    // Always contact the obj's own BspLeaf.
    RIT_LinkObjToBspLeaf(bspLeaf, &ol->tdata);

    spreadInBspLeaf(bspLeaf, &cfParams);
}

static void ObjlinkCellData_FindContacts(ObjlinkCellData* cell)
{
    DENG2_ASSERT(cell);
    // Already been here?
    if(cell->doneSpread) return;

    for(ObjLink* ol = cell->head; ol; ol = ol->nextInCell)
    {
        findContacts(ol);
    }
    cell->doneSpread = true;
}

/**
 * Spread contacts in the object => BspLeaf objlink blockmap to all
 * other BspLeafs within the block.
 *
 * @param bm         Objlink blockmap.
 * @param bspLeaf    BspLeaf to spread the contacts of.
 * @param maxRadius  Maximum radius for the spread.
 */
static void R_ObjlinkBlockmapSpreadInBspLeaf(ObjlinkBlockmap* bm, const BspLeaf* bspLeaf, float maxRadius)
{
    DENG2_ASSERT(bm);
    if(!bspLeaf) return; // Wha?

    QuadtreeCell minBlock;
    toObjlinkBlockmapCell(bm, minBlock, bspLeaf->aaBox.minX - maxRadius,
                                        bspLeaf->aaBox.minY - maxRadius);

    QuadtreeCell maxBlock;
    toObjlinkBlockmapCell(bm, maxBlock, bspLeaf->aaBox.maxX + maxRadius,
                                        bspLeaf->aaBox.maxY + maxRadius);

    QuadtreeCell mcell;
    for(mcell[1] = minBlock[1]; mcell[1] <= maxBlock[1]; ++mcell[1])
    for(mcell[0] = minBlock[0]; mcell[0] <= maxBlock[0]; ++mcell[0])
    {
        if(!bm->leafAtCell(mcell)) continue;

        ObjlinkCellData* cell = bm->cell(mcell);
        ObjlinkCellData_FindContacts(cell);
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

    if(!bspLeaf) return;

    for(int i = 0; i < NUM_OBJ_TYPES; ++i)
    {
        ObjlinkBlockmap* bm = blockmapForObjType(objtype_t(i));
        R_ObjlinkBlockmapSpreadInBspLeaf(bm, bspLeaf, maxRadius(objtype_t(i)));
    }

END_PROF( PROF_OBJLINK_SPREAD );
}

/// @pre  Coordinates held by @a blockXY are within valid range.
static void linkObjlinkInBlockmap(ObjlinkBlockmap* bm, ObjLink* link, QuadtreeCell& mcell)
{
    DENG2_ASSERT(bm && link);
    ObjlinkCellData* cell;
    if(bm->leafAtCell(mcell))
    {
        cell = bm->cell(mcell);
        DENG2_ASSERT(cell);
    }
    else
    {
        cell = (ObjlinkCellData*)Z_Calloc(sizeof(*cell), PU_MAPSTATIC, 0);
        bm->setCell(mcell, cell);
    }
    link->nextInCell = cell->head;
    cell->head = link;
}

void R_LinkObjs(void)
{
    ObjlinkBlockmap* bm;
    QuadtreeCell mcell;
    pvec3d_t origin;

BEGIN_PROF( PROF_OBJLINK_LINK );

    // Link objlinks into the objlink blockmap.
    for(ObjLink* ol = objlinks; ol; ol = ol->next)
    {
        switch(ol->type())
        {
        case OT_LUMOBJ:     origin = ol->tdata.lumobj()->origin; break;
        case OT_MOBJ:       origin = ol->tdata.mobj()->origin; break;
        default: exit(1); // Shutup compiler.
        }

        bm = blockmapForObjType(ol->type());
        if(!toObjlinkBlockmapCell(bm, mcell, origin[VX], origin[VY]))
        {
            linkObjlinkInBlockmap(bm, ol, mcell);
        }
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
    {
        memset(bspLeafContacts, 0, NUM_BSPLEAFS * sizeof *bspLeafContacts);
    }
}

int R_IterateBspLeafContacts2(BspLeaf* bspLeaf, objtype_t type,
    int (*callback) (void* object, void* parameters), void* parameters)
{
    DENG2_ASSERT(VALID_OBJTYPE(type));

    if(!bspLeaf) return false; // Continue iteration.

    ContactLink* con = bspLeafContacts[GET_BSPLEAF_IDX(bspLeaf)].lists[type];
    int result;
    while(con)
    {
        result = callback(con->obj, parameters);
        if(result) result;
        con = con->next;
    }
    return false; // Continue iteration.
}

int R_IterateBspLeafContacts(BspLeaf* bspLeaf, objtype_t type,
    int (*callback) (void* object, void* parameters))
{
    return R_IterateBspLeafContacts2(bspLeaf, type, callback, NULL/*no parameters*/);
}
