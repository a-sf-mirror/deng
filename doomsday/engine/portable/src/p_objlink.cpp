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

/// Clear all links and the spread flag.
static void ObjlinkCellData_ClearLinks(ObjlinkCellData* cell)
{
    DENG2_ASSERT(cell);
    cell->head = NULL;
    cell->doneSpread = false;
}

static int clearCellDataWorker(ObjlinkCellData* cell, void* /*parameters*/)
{
    Z_Free(cell);
    return false; // Continue iteration.
}

class ObjlinkBlockmap
{
public:
    ObjlinkBlockmap(coord_t const min[2], coord_t const max[2], uint cellWidth, uint cellHeight);

    ~ObjlinkBlockmap();

    QuadtreeCoord cellX(coord_t x) const;

    QuadtreeCoord cellY(coord_t y) const;

    /**
     * Given world coordinates @a x, @a y, determine the objlink blockmap block
     * [x, y] it resides in. If the coordinates are outside the blockmap they
     * are clipped within valid range.
     *
     * @return  @c true if the coordinates specified had to be adjusted.
     */
    bool cell(QuadtreeCell& mcell, coord_t x, coord_t y) const;

    void clearLinks();

    /// @pre  Coordinates held by @a blockXY are within valid range.
    void linkObjlinkInBlockmap(ObjLink* link, QuadtreeCell& mcell);

    /**
     * Spread contacts to all other BspLeafs within distance.
     *
     * @param bspLeaf       BspLeaf to spread contacts for.
     * @param maxDistance   Maximum distance to spread.
     */
    void spreadContacts(const BspLeaf* bspLeaf, float maxDistance);

private:
    struct Instance;
    Instance* d;
};

struct ObjlinkBlockmap::Instance
{
    /// Minimal and Maximal points in map space coordinates.
    AABoxd bounds;

    /// Cell dimensions in map space coordinates.
    vec2d_t cellSize;

    /**
     * Implemented in terms of a region Quadtree for its inherent sparsity,
     * spacial cohession and compression potential.
     */
    typedef Quadtree<ObjlinkCellData*> DataGrid;
    DataGrid grid;

    Instance(coord_t const min[2], coord_t const max[2], QuadtreeCoord cellWidth, QuadtreeCoord cellHeight)
        : bounds(min, max),
          grid(QuadtreeCoord( ceil((max[0] - min[0]) / coord_t(cellWidth)) ),
               QuadtreeCoord( ceil((max[1] - min[1]) / coord_t(cellHeight))) )
    {
        cellSize[VX] = cellWidth;
        cellSize[VY] = cellHeight;
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

    /**
     * Iterate over a block of populated cells in the blockmap making a callback
     * for each. Iteration ends when all selected cells have been visited or
     * @a callback returns non-zero.
     *
     * @param min            Minimal coordinates for the top left cell.
     * @param max            Maximal coordinates for the bottom right cell.
     * @param callback       Callback function ptr.
     * @param parameters     Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int blockIterate(QuadtreeCellBlock const& block_, IterateCallback callback,
                     void* parameters = 0)
    {
        // Clip coordinates to our boundary dimensions (the underlying Quadtree
        // is normally larger than this so we cannot use the dimensions of the
        // root cell here).
        QuadtreeCellBlock block = block_;
        grid.clipBlock(block);

        // Process all leafs in the block.
        /// @optimize: We could avoid repeatedly descending the tree...
        QuadtreeCell mcell;
        DataGrid::TreeLeaf* leaf;
        int result;
        for(mcell[1] = block.minY; mcell[1] <= block.maxY; ++mcell[1])
        for(mcell[0] = block.minX; mcell[0] <= block.maxX; ++mcell[0])
        {
            leaf = grid.findLeaf(mcell, false);
            if(!leaf || !leaf->value()) continue;

            result = callback(leaf->value(), parameters);
            if(result) return result;
        }
        return false;
    }

    /**
     * Same as blockIterate except cell block coordinates are expressed with
     * independent X and Y coordinate arguments. For convenience.
     */
    inline int blockIterate(QuadtreeCoord minX, QuadtreeCoord minY,
                            QuadtreeCoord maxX, QuadtreeCoord maxY,
                            IterateCallback callback, void* parameters = 0)
    {
        QuadtreeCellBlock block = QuadtreeCellBlock(minX, minY, maxX, maxY);
        return blockIterate(block, callback, parameters);
    }
};

ObjlinkBlockmap::ObjlinkBlockmap(coord_t const min[2], coord_t const max[2], uint cellWidth, uint cellHeight)
{
    d = new Instance(min, max, cellWidth, cellHeight);
}

ObjlinkBlockmap::~ObjlinkBlockmap()
{
    d->iterate(clearCellDataWorker);
    delete d;
}

QuadtreeCoord ObjlinkBlockmap::cellX(coord_t x) const
{
    DENG2_ASSERT(x >= d->bounds.min[0]);
    return QuadtreeCoord((x - d->bounds.min[0]) / d->cellSize[0]);
}

QuadtreeCoord ObjlinkBlockmap::cellY(coord_t y) const
{
    DENG2_ASSERT(y >= d->bounds.min[1]);
    return QuadtreeCoord((y - d->bounds.min[1]) / d->cellSize[1]);
}

bool ObjlinkBlockmap::cell(QuadtreeCell& mcell, coord_t x, coord_t y) const
{
    const QuadtreeCell& size = d->grid.widthHeight();
    coord_t max[2] = { d->bounds.min[0] + size[0] * BLOCK_WIDTH,
                       d->bounds.min[1] + size[1] * BLOCK_HEIGHT};

    bool adjusted = false;
    if(x < d->bounds.min[0])
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
        mcell[0] = cellX(x);
    }

    if(y < d->bounds.min[1])
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
        mcell[1] = cellY(y);
    }
    return adjusted;
}

static int clearObjlinkCellDataWorker(ObjlinkCellData* cell, void* /*parameters*/)
{
    ObjlinkCellData_ClearLinks(cell);
    return false; // Continue iteration.
}

void ObjlinkBlockmap::clearLinks()
{
    // Clear all the list heads and spread flags.
    d->iterate(clearObjlinkCellDataWorker);
}

/// @pre  Coordinates held by @a blockXY are within valid range.
void ObjlinkBlockmap::linkObjlinkInBlockmap(ObjLink* link, QuadtreeCell& mcell)
{
    DENG2_ASSERT(link);
    ObjlinkCellData* cell;
    if(d->leafAtCell(mcell))
    {
        cell = d->cell(mcell);
        DENG2_ASSERT(cell);
    }
    else
    {
        cell = (ObjlinkCellData*)Z_Calloc(sizeof(*cell), PU_MAPSTATIC, 0);
        d->setCell(mcell, cell);
    }
    link->nextInCell = cell->head;
    cell->head = link;
}

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

void R_ClearObjlinkBlockmap(objtype_t type)
{
    ObjlinkBlockmap* bm = blockmapForObjType(type);
    if(!bm) return;
    bm->clearLinks();
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

    BspLeaf* leaf = hedge->bspLeaf;
    BspLeaf* backLeaf = hedge->twin? hedge->twin->bspLeaf : 0;

    // There must be a back leaf to spread too.
    if(!backLeaf) return;

    // Which way does the spread go?
    if(!(leaf->validCount == validCount && backLeaf->validCount != validCount))
    {
        return; // Not eligible for spreading.
    }

    // Is the leaf on the back side outside the origin's AABB?
    if(backLeaf->aaBox.maxX <= parms->box[BOXLEFT]   ||
       backLeaf->aaBox.minX >= parms->box[BOXRIGHT]  ||
       backLeaf->aaBox.maxY <= parms->box[BOXBOTTOM] ||
       backLeaf->aaBox.minY >= parms->box[BOXTOP]) return;

    // Do not spread if the sector on the back side is closed with no height.
    if(backLeaf->sector && backLeaf->sector->SP_ceilheight <= backLeaf->sector->SP_floorheight) return;
    if(backLeaf->sector && leaf->sector &&
       (backLeaf->sector->SP_ceilheight  <= leaf->sector->SP_floorheight ||
        backLeaf->sector->SP_floorheight >= leaf->sector->SP_ceilheight)) return;

    // Too far from the object?
    coord_t distance = HEdge_PointOnSide(hedge, parms->objOrigin) / hedge->length;
    if(fabs(distance) >= parms->objRadius) return;

    // Don't spread if the middle material covers the opening.
    if(hedge->lineDef)
    {
        // On which side of the line are we? (distance is from hedge to origin).
        byte lineSide = hedge->side ^ (distance < 0);
        LineDef* line = hedge->lineDef;
        Sector* frontSec  = lineSide == FRONT? leaf->sector : backLeaf->sector;
        Sector* backSec   = lineSide == FRONT? backLeaf->sector : leaf->sector;
        SideDef* frontDef = line->L_sidedef(lineSide);
        SideDef* backDef  = line->L_sidedef(lineSide^1);

        if(backSec && !backDef) return; // One-sided window.

        if(R_MiddleMaterialCoversOpening(line->flags, frontSec, backSec, frontDef, backDef,
                                         false /*do not ignore material opacity*/)) return;
    }

    // During next step, obj will continue spreading from there.
    backLeaf->validCount = validCount;

    // Add this obj to the destination BspLeaf.
    RIT_LinkObjToBspLeaf(backLeaf, parms->tdata);

    spreadInBspLeaf(backLeaf, parms);
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

static int spreadContactsWorker(ObjlinkCellData* cell, void* /*parameters*/)
{
    // Already been here?
    if(!cell->doneSpread)
    {
        for(ObjLink* ol = cell->head; ol; ol = ol->nextInCell)
        {
            findContacts(ol);
        }
        cell->doneSpread = true;
    }
    return 0; // Continue iteration.
}

void ObjlinkBlockmap::spreadContacts(const BspLeaf* bspLeaf, float maxRadius)
{
    if(!bspLeaf) return; // Wha?

    QuadtreeCellBlock block;
    cell(block.min, bspLeaf->aaBox.minX - maxRadius,
                    bspLeaf->aaBox.minY - maxRadius);

    cell(block.max, bspLeaf->aaBox.maxX + maxRadius,
                    bspLeaf->aaBox.maxY + maxRadius);

    d->blockIterate(block, spreadContactsWorker);
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
        bm->spreadContacts(bspLeaf, maxRadius(objtype_t(i)));
    }

END_PROF( PROF_OBJLINK_SPREAD );
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
        if(!bm->cell(mcell, origin[VX], origin[VY]))
        {
            bm->linkObjlinkInBlockmap(ol, mcell);
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
