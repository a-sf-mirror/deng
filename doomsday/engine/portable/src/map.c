/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
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

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_dam.h"
#include "de_defs.h"
#include "de_bsp.h"
#include "de_edit.h"
#include "de_play.h"
#include "de_audio.h"

#include "bsp_edge.h"
#include "s_environ.h"

// MACROS ------------------------------------------------------------------

/**
 * @defGroup pruneUnusedObjectsFlags Prune Unused Objects Flags
 */
/*{*/
#define PRUNE_LINEDEFS      0x1
#define PRUNE_VERTEXES      0x2
#define PRUNE_SIDEDEFS      0x4
#define PRUNE_SECTORS       0x8
#define PRUNE_ALL           (PRUNE_LINEDEFS|PRUNE_VERTEXES|PRUNE_SIDEDEFS|PRUNE_SECTORS)
/*}*/

// $smoothmatoffset: Maximum speed for a smoothed material offset.
#define MAX_SMOOTH_MATERIAL_MOVE (8)

// TYPES -------------------------------------------------------------------

typedef struct {
    map_t*          map; // @todo should not be necessary.
    mobj_t*         mo;
    vec2_t          box[2];
} linelinker_data_t;

typedef struct {
    uint            numLineOwners;
    lineowner_t*    lineOwners; // Head of the lineowner list.
} vertexinfo_t;

typedef struct {
    map_t*          map;
    void*           obj;
    objcontacttype_t       type;
} linkobjtosubsectorparams_t;

// Used for vertex sector owners, side line owners and reverb subsectors.
typedef struct ownernode_s {
    void*           data;
    struct ownernode_s* next;
} ownernode_t;

typedef struct {
    ownernode_t*    head;
    uint            count;
} ownerlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int bspFactor = 7;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static vertex_t* rootVtx; // Used when sorting vertex line owners.

// List of unused and used obj-face contacts.
static objcontact_t* contFirst = NULL, *contCursor = NULL;

static ownernode_t* unusedNodeList = NULL;

// CODE --------------------------------------------------------------------

/**
 * Register the ccmds and cvars of the BSP builder. Called during engine
 * startup
 */
void P_MapRegister(void)
{
    C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0);
}

static thid_t newMobjID(map_t* map)
{
    thinkers_t* thinkers = Map_Thinkers(map);

    // Increment the ID dealer until a free ID is found.
    // \fixme What if all IDs are in use? 65535 thinkers!?
    while(Thinkers_IsUsedMobjID(thinkers, ++thinkers->iddealer));
    // Mark this ID as used.
    Thinkers_SetMobjID(thinkers, thinkers->iddealer, true);
    return thinkers->iddealer;
}

static linedef_t* createLineDef(map_t* map)
{
    linedef_t* line = Z_Calloc(sizeof(*line), PU_STATIC, 0);

    map->lineDefs =
        Z_Realloc(map->lineDefs, sizeof(line) * (++map->numLineDefs + 1), PU_STATIC);
    map->lineDefs[map->numLineDefs-1] = line;
    map->lineDefs[map->numLineDefs] = NULL;

    line->buildData.index = map->numLineDefs; // 1-based index.
    return line;
}

static sidedef_t* createSideDef(map_t* map)
{
    sidedef_t* side = Z_Calloc(sizeof(*side), PU_STATIC, 0);

    map->sideDefs = Z_Realloc(map->sideDefs, sizeof(side) * (++map->numSideDefs + 1), PU_STATIC);
    map->sideDefs[map->numSideDefs-1] = side;
    map->sideDefs[map->numSideDefs] = NULL;

    side->buildData.index = map->numSideDefs; // 1-based index.
    side->SW_bottomsurface.owner = (void*) side;
    side->SW_middlesurface.owner = (void*) side;
    side->SW_topsurface.owner = (void*) side;
    return side;
}

static sector_t* createSector(map_t* map)
{
    sector_t* sec = Z_Calloc(sizeof(*sec), PU_STATIC, 0);

    map->sectors = Z_Realloc(map->sectors, sizeof(sec) * (++map->numSectors + 1), PU_STATIC);
    map->sectors[map->numSectors-1] = sec;
    map->sectors[map->numSectors] = NULL;

    sec->buildData.index = map->numSectors; // 1-based index.
    return sec;
}

static plane_t* P_CreatePlane(map_t* map)
{
    return Z_Calloc(sizeof(plane_t), PU_STATIC, 0);
}

static polyobj_t* createPolyobj(map_t* map)
{
    polyobj_t* po = Z_Calloc(POLYOBJ_SIZE, PU_STATIC, 0);

    map->polyObjs = Z_Realloc(map->polyObjs, sizeof(po) * (++map->numPolyObjs + 1), PU_STATIC);
    map->polyObjs[map->numPolyObjs-1] = po;
    map->polyObjs[map->numPolyObjs] = NULL;

    po->buildData.index = map->numPolyObjs; // 1-based index, 0 = NIL.
    return po;
}

/**
 * Initialize subsector -> obj contact lists.
 */
static void initSubsectorContacts(map_t* map)
{
    map->_subsectorContacts =
        Z_Calloc(sizeof(*map->_subsectorContacts) * map->numSubsectors, PU_STATIC, 0);
}

/**
 * Initialize the obj > subsector contact lists ready for adding new
 * luminous objects. Called by R_BeginWorldFrame() at the beginning of a new
 * frame (if the render lists are not frozen).
 */
static void clearSubsectorContacts(map_t* map)
{
    // Start reusing nodes from the first one in the list.
    contCursor = contFirst;
    if(map->_subsectorContacts)
        memset(map->_subsectorContacts, 0, map->numSubsectors * sizeof(*map->_subsectorContacts));
}

/**
 * Link the given objcontact node to list.
 */
static __inline void linkContact(objcontact_t** list, objcontact_t* con,
                                 uint index)
{
    con->next = list[index];
    list[index] = con;
}

static void linkContactToSubsector(map_t* map, uint subsectorIdx,
                                   objcontacttype_t type, objcontact_t* node)
{
    linkContact(&map->_subsectorContacts[subsectorIdx].head[type], node, 0);
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
    objcontact_t* con;

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

void P_DestroyVertexInfo(mvertex_t* vinfo)
{
    assert(vinfo);
    Z_Free(vinfo);
}

static int destroyLinkedSubsector(face_t* face, void* context)
{
    subsector_t* subsector = (subsector_t*) face->data;
    if(subsector)
        P_DestroySubsector(subsector);
    face->data = NULL;
    return true; // Continue iteration.
}

static int destroyLinkedSeg(hedge_t* hEdge, void* context)
{
    seg_t* seg = hEdge->data;
    if(seg)
       P_DestroySeg(seg);
    hEdge->data = NULL;
    return true; // Continue iteration.
}

static int destroyLinkedVertexInfo(vertex_t* vertex, void* context)
{
    mvertex_t* vinfo = vertex->data;
    if(vinfo)
        P_DestroyVertexInfo(vinfo);
    vertex->data = NULL;
    return true; // Continue iteration.
}

static void destroyHalfEdgeDS(halfedgeds_t* halfEdgeDS)
{
    HalfEdgeDS_IterateFaces(halfEdgeDS, destroyLinkedSubsector, NULL);
    HalfEdgeDS_IterateHEdges(halfEdgeDS, destroyLinkedSeg, NULL);
    HalfEdgeDS_IterateVertices(halfEdgeDS, destroyLinkedVertexInfo, NULL);

    P_DestroyHalfEdgeDS(halfEdgeDS);
}

static boolean C_DECL freeNode(binarytree_t* tree, void* data)
{
    node_t* node;

    if(!BinaryTree_IsLeaf(tree) && (node = BinaryTree_GetData(tree)))
    {
        P_DestroyNode(node);
    }

    BinaryTree_SetData(tree, NULL);

    return true; // Continue iteration.
}

static void clearSectorFlags(map_t* map)
{
    uint i;

    if(!map)
        return;

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sec = map->sectors[i];

        // Clear all flags that can be cleared before each frame.
        sec->frameFlags &= ~SIF_FRAME_CLEAR;
    }
}

static int recycleMobjs(void* ptr, void* context)
{
    thinker_t* th = (thinker_t*) ptr;
    if(th->id)
    {   // Its a mobj.
        Thinkers_Remove(Map_Thinkers(Thinker_Map(th)), th);
        Thinker_SetMap(th, NULL);
        P_MobjRecycle((mobj_t*) th);
    }
    return true; // Continue iteration.
}

static int destroyGenerator(void* ptr, void* content)
{
    generator_t* gen = (generator_t*) ptr;
    thinker_t* th = (thinker_t*) gen;
    Map_RemoveThinker(Thinker_Map(th), th);
    P_DestroyGenerator(gen);
    return true; // Continue iteration.
}

/**
 * @note Part of the Doomsday public API.
 */
map_t* P_CreateMap(const char* mapID)
{
    map_t* map;
    
    if(!mapID || !mapID[0])
        Con_Error("P_CreateMap: Cannot create Map, a mapID must be specified.");

    map = Z_Calloc(MAP_SIZE, PU_STATIC, NULL);

    dd_snprintf(map->mapID, 9, "%s", mapID);
    strncpy(map->uniqueID, DAM_GenerateUniqueMapName(mapID), sizeof(map->uniqueID));

    map->editActive = true;
    map->_bias.editGrabbedID = -1;

    map->_thinkers = P_CreateThinkers();
    map->_halfEdgeDS = P_CreateHalfEdgeDS();
    map->_gameObjectRecords = P_CreateGameObjectRecords();
    map->_lightGrid = P_CreateLightGrid();

    map->_dlights.linkList = Z_Calloc(sizeof(dynlist_t), PU_STATIC, 0);

    return map;
}

/**
 * @note Part of the Doomsday public API.
 */
void P_DestroyMap(map_t* map)
{
    if(!map)
    {
        Con_Error("P_DestroyMap: Cannot destroy NULL map.");
        return; // Unreachable.
    }

    DL_DestroyDynlights(map->_dlights.linkList);
    Z_Free(map->_dlights.linkList);
    map->_dlights.linkList = NULL;

    /**
     * @todo handle vertexillum allocation/destruction along with the surfaces,
     * utilizing global storage.
     */
    {
    biassurface_t* bsuf;
    for(bsuf = map->_bias.surfaces; bsuf; bsuf = bsuf->next)
    {
        if(bsuf->illum)
            Z_Free(bsuf->illum);
    }
    }

    SB_DestroySurfaces(map);
    if(map->_bias.sources)
        SB_DestroySourceList(map->_bias.sources);
    map->_bias.sources = NULL;

    if(map->_lightGrid)
        P_DestroyLightGrid(map->_lightGrid);
    map->_lightGrid = NULL;

    if(map->_watchedPlaneList)
        P_DestroyPlaneList(map->_watchedPlaneList);
    map->_watchedPlaneList = NULL;

    if(map->_movingSurfaceList)
        P_DestroySurfaceList(map->_movingSurfaceList);
    map->_movingSurfaceList = NULL;

    if(map->_decoratedSurfaceList)
        P_DestroySurfaceList(map->_decoratedSurfaceList);
    map->_decoratedSurfaceList = NULL;

    if(map->_mobjBlockmap)
        P_DestroyMobjBlockmap(map->_mobjBlockmap);
    map->_mobjBlockmap = NULL;

    if(map->_lineDefBlockmap)
        P_DestroyLineDefBlockmap(map->_lineDefBlockmap);
    map->_lineDefBlockmap = NULL;

    if(map->_subsectorBlockmap)
        P_DestroySubsectorBlockmap(map->_subsectorBlockmap);
    map->_subsectorBlockmap = NULL;

    if(map->_particleBlockmap)
        P_DestroyParticleBlockmap(map->_particleBlockmap);
    map->_particleBlockmap = NULL;

    if(map->_lumobjBlockmap)
        P_DestroyLumobjBlockmap(map->_lumobjBlockmap);
    map->_lumobjBlockmap = NULL;

    if(map->_subsectorContacts)
        Z_Free(map->_subsectorContacts);
    map->_subsectorContacts = NULL;

    P_DestroyNodePile(map->mobjNodes);
    map->mobjNodes = NULL;
    P_DestroyNodePile(map->lineNodes);
    map->lineNodes = NULL;
    if(map->lineLinks)
        Z_Free(map->lineLinks);
    map->lineLinks = NULL;

    if(map->sideDefs)
    {
        uint i;

        for(i = 0; i < map->numSideDefs; ++i)
        {
            sidedef_t* sideDef = map->sideDefs[i];

            if(sideDef->sections[SEG_MIDDLE].decorations)
                Z_Free(sideDef->sections[SEG_MIDDLE].decorations);
            if(sideDef->sections[SEG_BOTTOM].decorations)
                Z_Free(sideDef->sections[SEG_BOTTOM].decorations);
            if(sideDef->sections[SEG_TOP].decorations)
                Z_Free(sideDef->sections[SEG_TOP].decorations);
            Z_Free(sideDef);
        }
        Z_Free(map->sideDefs);
    }
    map->sideDefs = NULL;
    map->numSideDefs = 0;

    if(map->lineDefs)
    {
        uint i;
        for(i = 0; i < map->numLineDefs; ++i)
        {
            linedef_t* line = map->lineDefs[i];
            Z_Free(line);
        }
        Z_Free(map->lineDefs);
    }
    map->lineDefs = NULL;
    map->numLineDefs = 0;
    if(map->lineOwners)
        Z_Free(map->lineOwners);
    map->lineOwners = NULL;

    if(map->polyObjs)
    {
        uint i;
        for(i = 0; i < map->numPolyObjs; ++i)
        {
            polyobj_t* po = map->polyObjs[i];
            uint j;

            for(j = 0; j < po->numLineDefs; ++j)
            {
                linedef_t* lineDef = ((objectrecord_t*) po->lineDefs[j])->obj;
                Z_Free(lineDef->hEdges[0]->twin);
                Z_Free(lineDef->hEdges[0]);
            }
            Z_Free(po->lineDefs);
            Z_Free(po->segs);
            Z_Free(po->originalPts);
            Z_Free(po->prevPts);

            Z_Free(po);
        }

        Z_Free(map->polyObjs);
    }
    map->polyObjs = NULL;
    map->numPolyObjs = 0;

    if(map->sectors)
    {
        uint i;
        for(i = 0; i < map->numSectors; ++i)
        {
            sector_t* sector = map->sectors[i];

            if(sector->planes)
                Z_Free(sector->planes);
            sector->planeCount = 0;

            if(sector->subsectors)
                Z_Free(sector->subsectors);
            if(sector->reverbSubsectors)
                Z_Free(sector->reverbSubsectors);
            if(sector->blocks)
                Z_Free(sector->blocks);
            if(sector->lineDefs)
                Z_Free(sector->lineDefs);

            Z_Free(sector);
        }

        Z_Free(map->sectors);
    }
    map->sectors = NULL;
    map->numSectors = 0;

    if(map->planes)
    {
        uint i;
        for(i = 0; i < map->numPlanes; ++i)
        {
            plane_t* plane = map->planes[i];
            if(plane->surface.decorations)
                Z_Free(plane->surface.decorations);
            Z_Free(plane);
        }
        Z_Free(map->planes);
    }
    map->planes = NULL;
    map->numPlanes = 0;

    if(map->segs)
        Z_Free(map->segs);
    map->segs = NULL;
    map->numSegs = 0;

    if(map->subsectors)
        Z_Free(map->subsectors);
    map->subsectors = NULL;
    map->numSubsectors = 0;

    if(map->nodes)
        Z_Free(map->nodes);
    map->nodes = NULL;
    map->numNodes = 0;

    destroyHalfEdgeDS(map->_halfEdgeDS);
    map->_halfEdgeDS = NULL;

    // Free the BSP tree.
    if(map->_rootNode)
    {
        BinaryTree_PostOrder(map->_rootNode, freeNode, NULL);
        BinaryTree_Destroy(map->_rootNode);
    }
    map->_rootNode = NULL;

    /**
     * Destroy Generators. This is only necessary because the particles of
     * each generator use storage linked only to the generator.
     */
    Thinkers_Iterate(map->_thinkers, (think_t) P_GeneratorThinker, ITF_PRIVATE,
                     destroyGenerator, NULL);

    Thinkers_Iterate(map->_thinkers, NULL, ITF_PUBLIC, recycleMobjs, NULL);
    P_DestroyThinkers(map->_thinkers);
    map->_thinkers = NULL;

    if(P_CurrentMap() == map)
        P_SetCurrentMap(NULL);

    Z_Free(map);
}

/**
 * The given line might cross the mobj. If necessary, link the mobj into
 * the line's mobj link ring.
 */
static boolean PIT_LinkToLines(linedef_t* ld, void* parm)
{
    linelinker_data_t* data = parm;
    nodeindex_t nix;

    if(data->box[1][VX] <= ld->bBox[BOXLEFT] ||
       data->box[0][VX] >= ld->bBox[BOXRIGHT] ||
       data->box[1][VY] <= ld->bBox[BOXBOTTOM] ||
       data->box[0][VY] >= ld->bBox[BOXTOP])
        // Bounding boxes do not overlap.
        return true;

    if(LineDef_BoxOnSide2(ld, data->box[0][VX], data->box[1][VX],
                              data->box[0][VY], data->box[1][VY]) != -1)
        // Line does not cross the mobj's bounding box.
        return true;

    // One sided lines will not be linked to because a mobj
    // can't legally cross one.
    if(!LINE_FRONTSIDE(ld) || !LINE_BACKSIDE(ld))
        return true;

    // No redundant nodes will be creates since this routine is
    // called only once for each line.

    // Add a node to the mobj's ring.
    NP_Link(data->map->mobjNodes, nix = NP_New(data->map->mobjNodes, ld), data->mo->lineRoot);

    // Add a node to the line's ring. Also store the linenode's index
    // into the mobjring's node, so unlinking is easy.
    data->map->mobjNodes->nodes[nix].data = NP_New(data->map->lineNodes, data->mo);
    NP_Link(data->map->lineNodes, data->map->mobjNodes->nodes[nix].data,
            data->map->lineLinks[P_ObjectRecord(DMU_LINEDEF, ld)->id - 1]);

    return true;
}

/**
 * @pre The mobj must be currently unlinked.
 */
static void linkMobjToLineDefs(map_t* map, mobj_t* mo)
{
    linelinker_data_t data;
    vec2_t point;

    assert(map);
    assert(mo);

    // Get a new root node.
    mo->lineRoot = NP_New(map->mobjNodes, NP_ROOT_NODE);

    // Set up a line iterator for doing the linking.
    data.mo = mo;
    data.map = map;
    V2_Set(point, mo->pos[VX] - mo->radius, mo->pos[VY] - mo->radius);
    V2_InitBox(data.box, point);
    V2_Set(point, mo->pos[VX] + mo->radius, mo->pos[VY] + mo->radius);
    V2_AddToBox(data.box, point);

    validCount++;
    Map_LineDefsBoxIteratorv(map, data.box, PIT_LinkToLines, &data, false);
}

/**
 * Unlinks the mobj from all the lines it's been linked to. Can be called
 * without checking that the list does indeed contain lines.
 */
static boolean unlinkMobjFromLineDefs(map_t* map, mobj_t* mo)
{
    linknode_t* tn;
    nodeindex_t nix;

    assert(map);
    assert(mo);

    tn = map->mobjNodes->nodes;

    // Try unlinking from lines.
    if(!mo->lineRoot)
        return false; // A zero index means it's not linked.

    // Unlink from each line.
    for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
        nix = tn[nix].next)
    {
        // Data is the linenode index that corresponds this mobj.
        NP_Unlink(map->lineNodes, tn[nix].data);
        // We don't need these nodes any more, mark them as unused.
        // Dismissing is a macro.
        NP_Dismiss(map->lineNodes, tn[nix].data);
        NP_Dismiss(map->mobjNodes, nix);
    }

    // The mobj no longer has a line ring.
    NP_Dismiss(map->mobjNodes, mo->lineRoot);
    mo->lineRoot = 0;

    return true;
}

/**
 * Two links to update:
 * 1) The link to us from the previous node (sprev, always set) will
 *    be modified to point to the node following us.
 * 2) If there is a node following us, set its sprev pointer to point
 *    to the pointer that points back to it (our sprev, just modified).
 */
static boolean unlinkMobjFromSector(map_t* map, mobj_t* mo)
{
    assert(map);
    assert(mo);

    if(mo->sPrev == NULL)
        return false;

    if((*mo->sPrev = mo->sNext))
        mo->sNext->sPrev = mo->sPrev;

    // Not linked any more.
    mo->sNext = NULL;
    mo->sPrev = NULL;

    return true;
}

void Map_LinkMobj(map_t* map, mobj_t* mo, byte flags)
{
    assert(map);
    assert(mo);
    {
    subsector_t* subsector = Map_PointInSubsector(map, mo->pos[VX], mo->pos[VY]);

    // Link into the sector.
    mo->subsector = (subsector_t*) P_ObjectRecord(DMU_SUBSECTOR, subsector);

    if(flags & DDLINK_SECTOR)
    {
        // Unlink from the current sector, if any.
        if(mo->sPrev)
            unlinkMobjFromSector(map, mo);

        // Link the new mobj to the head of the list.
        // Prev pointers point to the pointer that points back to us.
        // (Which practically disallows traversing the list backwards.)

        if((mo->sNext = subsector->sector->mobjList))
            mo->sNext->sPrev = &mo->sNext;

        *(mo->sPrev = &subsector->sector->mobjList) = mo;
    }

    // Link into blockmap?
    if(flags & DDLINK_BLOCKMAP)
    {
        // Unlink from the old block, if any.
        MobjBlockmap_Unlink(map->_mobjBlockmap, mo);
        MobjBlockmap_Link(map->_mobjBlockmap, mo);
    }

    // Link into lines.
    if(!(flags & DDLINK_NOLINE))
    {
        unlinkMobjFromLineDefs(map, mo);
        linkMobjToLineDefs(map, mo);
    }

    // If this is a player - perform addtional tests to see if they have
    // entered or exited the void.
    if(mo->dPlayer)
    {
        ddplayer_t* player = mo->dPlayer;

        player->inVoid = true;
        if(Sector_PointInside2(subsector->sector, player->mo->pos[VX], player->mo->pos[VY]) &&
           (player->mo->pos[VZ] < subsector->sector->SP_ceilvisheight  - 4 &&
            player->mo->pos[VZ] >= subsector->sector->SP_floorvisheight))
            player->inVoid = false;
    }
    }
}

int Map_UnlinkMobj(map_t* map, mobj_t* mo)
{
    assert(map);
    assert(mo);
    {
    int links = 0;

    if(unlinkMobjFromSector(map, mo))
        links |= DDLINK_SECTOR;
    if(MobjBlockmap_Unlink(map->_mobjBlockmap, mo))
        links |= DDLINK_BLOCKMAP;
    if(!unlinkMobjFromLineDefs(map, mo))
        links |= DDLINK_NOLINE;

    return links;
    }
}

/**
 * @param th            Thinker to be added.
 * @param makePublic    If @c true, this thinker will be visible publically
 *                      via the Doomsday public API thinker interface(s).
 */
void Map_AddThinker(map_t* map, thinker_t* th, boolean makePublic)
{
    assert(map);
    assert(th);

    if(!th->function)
    {
        Con_Error("Map_AddThinker: Invalid thinker function.");
    }

    // Will it need an ID?
    if(P_IsMobjThinker(th, NULL))
    {
        // It is a mobj, give it an ID.
        th->id = newMobjID(map);
    }
    else
    {
        // Zero is not a valid ID.
        th->id = 0;
    }

    // Link the thinker to the thinker list.
    Thinkers_Add(map->_thinkers, th, makePublic);
    Thinker_SetMap(th, map);
}

/**
 * Deallocation is lazy -- it will not actually be freed until its
 * thinking turn comes up.
 */
void Map_RemoveThinker(map_t* map, thinker_t* th)
{
    assert(map);
    assert(th);

    // Has got an ID?
    if(th->id)
    {   // Then it must be a mobj.
        mobj_t* mo = (mobj_t *) th;

        // Flag the ID as free.
        Thinkers_SetMobjID(map->_thinkers, th->id, false);

        // If the state of the mobj is the NULL state, this is a
        // predictable mobj removal (result of animation reaching its
        // end) and shouldn't be included in netGame deltas.
        if(!isClient)
        {
            if(!mo->state || mo->state == states)
            {
                Sv_MobjRemoved(map, th->id);
            }
        }
    }

    th->function = (think_t) - 1;
}

/**
 * Iterate the list of thinkers making a callback for each.
 *
 * @param func          If not @c NULL, only make a callback for objects whose
 *                      thinker matches.
 * @param flags         Thinker filter flags.
 * @param callback      The callback to make. Iteration will continue
 *                      until a callback returns a zero value.
 * @param context       Is passed to the callback function.
 */
boolean Map_IterateThinkers(map_t* map, think_t func, byte flags,
                            int (*callback) (void* p, void*), void* context)
{
    assert(map);
    return Thinkers_Iterate(map->_thinkers, func, flags, callback, context);
}

void Map_ThinkerSetStasis(map_t* map, thinker_t* th, boolean on)
{
    assert(map);
    assert(th);
    th->inStasis = on;
}

subsector_t* Map_PointInSubsector(map_t* map, float x, float y)
{
    assert(map);
    {
    binarytree_t* tree;
    face_t* face;

    if(!map->numNodes) // Single subsector is a special case.
        return (subsector_t*) map->subsectors;

    tree = map->_rootNode;
    while(!BinaryTree_IsLeaf(tree))
    {
        node_t* node = (node_t*) BinaryTree_GetData(tree);
        tree = BinaryTree_GetChild(tree, R_PointOnSide(x, y, &node->partition));
    }

    face = (face_t*) BinaryTree_GetData(tree);
    return (subsector_t*) face->data;
    }
}

/**
 * Returns a ptr to the sector which owns the given ddmobj_base_t.
 *
 * @param ddMobjBase    ddmobj_base_t to search for.
 *
 * @return              Ptr to the Sector where the ddmobj_base_t resides,
 *                      else @c NULL.
 */
sector_t* Map_SectorForOrigin(map_t* map, const void* ddMobjBase)
{
    assert(map);
    assert(ddMobjBase);
    {
    uint i;

    for(i = 0; i < map->numSectors; ++i)
    {
        if(ddMobjBase == &map->sectors[i]->soundOrg)
            return map->sectors[i];
    }

    return NULL;
    }
}

/**
 * @return              @c true, iff this is indeed a polyobj origin.
 */
polyobj_t* Map_PolyobjForOrigin(map_t* map, const void* ddMobjBase)
{
    assert(map);
    assert(ddMobjBase);
    {
    uint i;

    for(i = 0; i < map->numPolyObjs; ++i)
    {
        if(ddMobjBase == (ddmobj_base_t*) map->polyObjs[i])
            return map->polyObjs[i];
    }

    return NULL;
    }
}

static void spawnMapParticleGens(map_t* map)
{
    ded_generator_t* def;
    int i;

    if(isDedicated || !useParticles)
        return;

    for(i = 0, def = defs.generators; i < defs.count.generators.num; ++i, def++)
    {
        if(!def->map[0] || stricmp(def->map, map->mapID))
            continue;

        if(def->spawnAge > 0 && ddMapTime > def->spawnAge)
            continue; // No longer spawning this generator.

        P_SpawnMapParticleGen(map, def);
    }
}

/**
 * Spawns all type-triggered particle generators, regardless of whether
 * the type of mobj exists in the map or not (mobjs might be dynamically
 * created).
 */
static void spawnTypeParticleGens(map_t* map)
{
    ded_generator_t* def;
    int i;

    if(isDedicated || !useParticles)
        return;

    for(i = 0, def = defs.generators; i < defs.count.generators.num; ++i, def++)
    {
        if(def->typeNum < 0)
            continue;

        P_SpawnTypeParticleGen(map, def);
    }
}

typedef struct {
    sector_t*       sector;
    uint            planeID;
} findplanegeneratorparams_t;

static int findPlaneGenerator(void* ptr, void* context)
{
    generator_t* gen = (generator_t*) ptr;
    findplanegeneratorparams_t* params = (findplanegeneratorparams_t*) context;

    if(gen->thinker.function && gen->sector == params->sector &&
       gen->planeID == params->planeID)
        return false; // Stop iteration, we've found one!
    return true; // Continue iteration.
}

/**
 * Returns true iff there is an active ptcgen for the given plane.
 */
static boolean P_HasActiveGenerator(sector_t* sector, uint planeID)
{
    findplanegeneratorparams_t params;

    params.sector = sector;
    params.planeID = planeID;

    // @todo Sector should return the map its linked to.
    return !Map_IterateThinkers(P_CurrentMap(), (think_t) P_GeneratorThinker, ITF_PRIVATE,
                                findPlaneGenerator, &params);
}

/**
 * Spawns new ptcgens for planes, if necessary.
 */
static void checkPtcPlanes(map_t* map)
{
    uint i, p;

    if(isDedicated || !useParticles)
        return;

    // There is no need to do this on every tic.
    if(SECONDS_TO_TICKS(gameTime) % 4)
        return;

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sector = map->sectors[i];

        for(p = 0; p < 2; ++p)
        {
            uint plane = p;
            material_t* mat = sector->SP_planematerial(plane);
            const ded_generator_t* def = Material_GetGenerator(mat);

            if(!def)
                continue;

            if(def->flags & PGF_CEILING_SPAWN)
                plane = PLN_CEILING;
            if(def->flags & PGF_FLOOR_SPAWN)
                plane = PLN_FLOOR;

            if(!P_HasActiveGenerator(sector, plane))
            {
                // Spawn it!
                P_SpawnPlaneParticleGen(sector, plane, def);
            }
        }
    }
}

static void initGenerators(map_t* map)
{
    // Spawn all type-triggered particle generators.
    // Let's hope there aren't too many...
    spawnTypeParticleGens(map);
    spawnMapParticleGens(map);
}

static int updateGenerator(void* ptr, void* context)
{
    generator_t* gen = (generator_t*) ptr;

    if(gen->thinker.function)
    {
        int i;
        ded_generator_t* def;
        boolean found;

        // Map-static generators cannot be updated (we have no means to reliably
        // identify them), so destroy them.
        // Flat generators will be spawned automatically within a few tics so we'll
        // just destroy those too.
        if((gen->flags & PGF_UNTRIGGERED) || gen->sector)
        {
            destroyGenerator(gen, NULL);
            return true; // Continue iteration.
        }

        // Search for a suitable definition.
        i = 0;
        def = defs.generators;
        found = false;
        while(i < defs.count.generators.num && !found)
        {
            // A type generator?
            if(def->typeNum >= 0 &&
               (gen->type == def->typeNum || gen->type2 == def->type2Num))
            {
                found = true;
            }
            // A damage generator?
            else if(gen->source && gen->source->type == def->damageNum)
            {
                found = true;
            }
            // A state generator?
            else if(gen->source && def->state[0] &&
                    gen->source->state - states == Def_GetStateNum(def->state))
            {
                found = true;
            }
            else
            {
                i++;
                def++;
            }
        }

        if(found)
        {   // Update the generator using the new definition.
            gen->def = def;
        }
        else
        {   // Nothing else we can do, destroy it.
            destroyGenerator(gen, NULL);
        }
    }

    return true; // Continue iteration.
}

/**
 * Called after a reset once the definitions have been re-read.
 */
void Map_Update(map_t* map)
{
    uint i;

    assert(map);

    // Defs might've changed, so update the generators.
    Map_IterateThinkers(map, (think_t) P_GeneratorThinker, ITF_PRIVATE,
                        updateGenerator, NULL);
    // Re-spawn map generators.
    spawnMapParticleGens(map);

    // Update all world surfaces.
    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sec = map->sectors[i];
        uint j;

        for(j = 0; j < sec->planeCount; ++j)
            Surface_Update(&sec->SP_planesurface(j));
    }

    for(i = 0; i < map->numSideDefs; ++i)
    {
        sidedef_t* side = map->sideDefs[i];

        Surface_Update(&side->SW_topsurface);
        Surface_Update(&side->SW_middlesurface);
        Surface_Update(&side->SW_bottomsurface);
    }

    for(i = 0; i < map->numPolyObjs; ++i)
    {
        polyobj_t* po = map->polyObjs[i];
        uint j;

        for(j = 0; j < po->numLineDefs; ++j)
        {
            linedef_t* line = ((objectrecord_t*) po->lineDefs[j])->obj;

            Surface_Update(&LINE_FRONTSIDE(line)->SW_middlesurface);
        }
    }
}

static boolean updatePlaneHeightTracking(plane_t* plane, void* context)
{
    Plane_UpdateHeightTracking(plane);
    return true; // Continue iteration.
}

static boolean resetPlaneHeightTracking(plane_t* plane, void* context)
{
    map_t* map = (map_t*) context;
    uint i;

    Plane_ResetHeightTracking(plane);
    if(map->_watchedPlaneList)
        PlaneList_Remove(map->_watchedPlaneList, plane);

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sec = map->sectors[i];
        uint j;

        for(j = 0; j < sec->planeCount; ++j)
        {
            if(sec->planes[i] == plane)
                R_MarkDependantSurfacesForDecorationUpdate(sec);
        }
    }

    return true; // Continue iteration.
}

static boolean interpolatePlaneHeight(plane_t* plane, void* context)
{
    map_t* map = (map_t*) context;
    uint i;

    Plane_InterpolateHeight(plane);

    // Has this plane reached its destination?
    if(plane->visHeight == plane->height && map->_watchedPlaneList)
        PlaneList_Remove(map->_watchedPlaneList, plane);

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sec = map->sectors[i];
        uint j;

        for(j = 0; j < sec->planeCount; ++j)
        {
            if(sec->planes[i] == plane)
                R_MarkDependantSurfacesForDecorationUpdate(sec);
        }
    }

    return true; // Continue iteration.
}

/**
 * $smoothplane: Roll the height tracker buffers.
 */
void Map_UpdateWatchedPlanes(map_t* map)
{
    assert(map);
    if(map->_watchedPlaneList)
        PlaneList_Iterate(map->_watchedPlaneList, updatePlaneHeightTracking, NULL);
}

/**
 * $smoothplane: interpolate the visual offset.
 */
static void interpolateWatchedPlanes(map_t* map, boolean resetNextViewer)
{
    assert(map);

    if(resetNextViewer)
    {
        // $smoothplane: Reset the plane height trackers.
        PlaneList_Iterate(map->_watchedPlaneList, resetPlaneHeightTracking, map);
    }
    // While the game is paused there is no need to calculate any
    // visual plane offsets $smoothplane.
    else //if(!clientPaused)
    {
        // $smoothplane: Set the visible offsets.
        PlaneList_Iterate(map->_watchedPlaneList, interpolatePlaneHeight, map);
    }
}

static boolean resetMovingSurface(surface_t* suf, void* context)
{
    // X Offset.
    suf->visOffsetDelta[0] = 0;
    suf->oldOffset[0][0] = suf->oldOffset[0][1] = suf->offset[0];

    // Y Offset.
    suf->visOffsetDelta[1] = 0;
    suf->oldOffset[1][0] = suf->oldOffset[1][1] = suf->offset[1];

    Surface_Update(suf);
    SurfaceList_Remove((surfacelist_t*) context, suf);

    return true;
}

static boolean interpMovingSurface(surface_t* suf, void* context)
{
    // X Offset.
    suf->visOffsetDelta[0] =
        suf->oldOffset[0][0] * (1 - frameTimePos) +
                suf->offset[0] * frameTimePos - suf->offset[0];

    // Y Offset.
    suf->visOffsetDelta[1] =
        suf->oldOffset[1][0] * (1 - frameTimePos) +
                suf->offset[1] * frameTimePos - suf->offset[1];

    // Visible material offset.
    suf->visOffset[0] = suf->offset[0] + suf->visOffsetDelta[0];
    suf->visOffset[1] = suf->offset[1] + suf->visOffsetDelta[1];

    Surface_Update(suf);

    // Has this material reached its destination?
    if(suf->visOffset[0] == suf->offset[0] &&
       suf->visOffset[1] == suf->offset[1])
        SurfaceList_Remove((surfacelist_t*) context, suf);

    return true;
}

/**
 * $smoothmatoffset: interpolate the visual offset.
 */
static void interpolateMovingSurfaces(map_t* map, boolean resetNextViewer)
{
    if(!map)
        return;

    if(resetNextViewer)
    {
        // Reset the material offset trackers.
        SurfaceList_Iterate(map->_movingSurfaceList, resetMovingSurface, map->_movingSurfaceList);
    }
    // While the game is paused there is no need to calculate any
    // visual material offsets.
    else //if(!clientPaused)
    {
        // Set the visible material offsets.
        SurfaceList_Iterate(map->_movingSurfaceList, interpMovingSurface, map->_movingSurfaceList);
    }
}

static boolean updateMovingSurface(surface_t* suf, void* context)
{
    // X Offset
    suf->oldOffset[0][0] = suf->oldOffset[0][1];
    suf->oldOffset[0][1] = suf->offset[0];
    if(suf->oldOffset[0][0] != suf->oldOffset[0][1])
        if(fabs(suf->oldOffset[0][0] - suf->oldOffset[0][1]) >=
           MAX_SMOOTH_MATERIAL_MOVE)
        {
            // Too fast: make an instantaneous jump.
            suf->oldOffset[0][0] = suf->oldOffset[0][1];
        }

    // Y Offset
    suf->oldOffset[1][0] = suf->oldOffset[1][1];
    suf->oldOffset[1][1] = suf->offset[1];
    if(suf->oldOffset[1][0] != suf->oldOffset[1][1])
        if(fabs(suf->oldOffset[1][0] - suf->oldOffset[1][1]) >=
           MAX_SMOOTH_MATERIAL_MOVE)
        {
            // Too fast: make an instantaneous jump.
            suf->oldOffset[1][0] = suf->oldOffset[1][1];
        }

    return true;
}

/**
 * $smoothmatoffset: Roll the surface material offset tracker buffers.
 */
void Map_UpdateMovingSurfaces(map_t* map)
{
    assert(map);
    if(map->_movingSurfaceList)
        SurfaceList_Iterate(map->_movingSurfaceList, updateMovingSurface, NULL);
}

typedef struct {
    arvec2_t        bounds;
    boolean         inited;
} findaabbforvertices_params_t;

static int addToAABB(vertex_t* vertex, void* context)
{
    findaabbforvertices_params_t* params = (findaabbforvertices_params_t*) context;
    vec2_t point;

    V2_Set(point, vertex->pos[VX], vertex->pos[VY]);
    if(!params->inited)
    {
        V2_InitBox(params->bounds, point);
        params->inited = true;
    }
    else
        V2_AddToBox(params->bounds, point);
    return true; // Continue iteration.
}

static void findAABBForVertices(map_t* map, arvec2_t bounds)
{
    findaabbforvertices_params_t params;

    params.bounds = bounds;
    params.inited = false;
    HalfEdgeDS_IterateVertices(Map_HalfEdgeDS(map), addToAABB, &params);
}

static void buildLineDefBlockmap(map_t* map)
{
#define BLKMARGIN               (8) // size guardband around map
#define MAPBLOCKUNITS           128

    uint i;
    uint bMapWidth, bMapHeight; // Blockmap dimensions.
    vec2_t blockSize; // Size of the blocks.
    uint numBlocks; // Number of cells = nrows*ncols.
    vec2_t bounds[2], dims;
    linedefblockmap_t* blockmap;

    // Scan for map limits, which the blockmap must enclose.
    findAABBForVertices(map, bounds);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], bounds[0][VX] - BLKMARGIN, bounds[0][VY] - BLKMARGIN);
    V2_Set(bounds[1], bounds[1][VX] + BLKMARGIN, bounds[1][VY] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, MAPBLOCKUNITS, MAPBLOCKUNITS);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[VX] <= blockSize[VX])
        bMapWidth = 1;
    else
        bMapWidth = ceil(dims[VX] / blockSize[VX]);

    if(dims[VY] <= blockSize[VY])
        bMapHeight = 1;
    else
        bMapHeight = ceil(dims[VY] / blockSize[VY]);
    numBlocks = bMapWidth * bMapHeight;

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][VX] + bMapWidth  * blockSize[VX],
                      bounds[0][VY] + bMapHeight * blockSize[VY]);

    blockmap = P_CreateLineDefBlockmap(bounds[0], bounds[1], bMapWidth, bMapHeight);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* lineDef = map->lineDefs[i];
        if(lineDef->inFlags & LF_POLYOBJ)
            continue;
        LineDefBlockmap_Link(blockmap, lineDef);
    }

    map->_lineDefBlockmap = blockmap;

#undef BLKMARGIN
#undef MAPBLOCKUNITS
}

static void buildSubsectorBlockmap(map_t* map)
{
#define BLKMARGIN       8
#define BLOCK_WIDTH     128
#define BLOCK_HEIGHT    128

    uint i, subMapWidth, subMapHeight;
    vec2_t bounds[2], blockSize, dims;
    subsectorblockmap_t* blockmap;

    // @fixme why not use map->bBox?
    bounds[0][0] = bounds[0][1] = DDMAXFLOAT;
    bounds[1][0] = bounds[1][1] = DDMINFLOAT;
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* ssec = map->subsectors[i];

        if(ssec->bBox[0][0] < bounds[0][0])
            bounds[0][0] = ssec->bBox[0][0];
        if(ssec->bBox[0][1] < bounds[0][1])
            bounds[0][1] = ssec->bBox[0][1];
        if(ssec->bBox[1][0] > bounds[1][0])
            bounds[1][0] = ssec->bBox[1][0];
        if(ssec->bBox[1][1] > bounds[1][1])
            bounds[1][1] = ssec->bBox[1][1];
    }
    bounds[0][0] -= BLKMARGIN;
    bounds[0][1] -= BLKMARGIN;
    bounds[1][0] += BLKMARGIN;
    bounds[1][1] += BLKMARGIN;

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    /*V2_Set(bounds[0], map->bBox[BOXLEFT] - BLKMARGIN,
                      map->bBox[BOXBOTTOM] - BLKMARGIN);
    V2_Set(bounds[1], map->bBox[BOXRIGHT] + BLKMARGIN,
                      map->bBox[BOXTOP] + BLKMARGIN);*/

    // Select a good size for the blocks.
    V2_Set(blockSize, BLOCK_WIDTH, BLOCK_HEIGHT);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[VX] <= blockSize[VX])
        subMapWidth = 1;
    else
        subMapWidth = ceil(dims[VX] / blockSize[VX]);

    if(dims[VY] <= blockSize[VY])
        subMapHeight = 1;
    else
        subMapHeight = ceil(dims[VY] / blockSize[VY]);

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][VX] + subMapWidth  * blockSize[VX],
                      bounds[0][VY] + subMapHeight * blockSize[VY]);

    blockmap = P_CreateSubsectorBlockmap(bounds[0], bounds[1], subMapWidth, subMapHeight);

    // Process all the subsectors in the map.
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* subsector = map->subsectors[i];
        SubsectorBlockmap_Link(blockmap, subsector);
    }

    map->_subsectorBlockmap = blockmap;

#undef BLKMARGIN
#undef BLOCK_WIDTH
#undef BLOCK_HEIGHT
}

static void buildMobjBlockmap(map_t* map)
{
#define BLKMARGIN               (8) // size guardband around map
#define MAPBLOCKUNITS           128

    uint bMapWidth, bMapHeight; // Blockmap dimensions.
    vec2_t blockSize; // Size of the blocks.
    vec2_t bounds[2], dims;

    // Scan for map limits, which the blockmap must enclose.
    findAABBForVertices(map, bounds);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], bounds[0][VX] - BLKMARGIN, bounds[0][VY] - BLKMARGIN);
    V2_Set(bounds[1], bounds[1][VX] + BLKMARGIN, bounds[1][VY] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, MAPBLOCKUNITS, MAPBLOCKUNITS);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[VX] <= blockSize[VX])
        bMapWidth = 1;
    else
        bMapWidth = ceil(dims[VX] / blockSize[VX]);

    if(dims[VY] <= blockSize[VY])
        bMapHeight = 1;
    else
        bMapHeight = ceil(dims[VY] / blockSize[VY]);

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][VX] + bMapWidth  * blockSize[VX],
                      bounds[0][VY] + bMapHeight * blockSize[VY]);

    map->_mobjBlockmap = P_CreateMobjBlockmap(bounds[0], bounds[1], bMapWidth, bMapHeight);

#undef BLKMARGIN
#undef MAPBLOCKUNITS
}

static void buildParticleBlockmap(map_t* map)
{
#define BLKMARGIN       8
#define BLOCK_WIDTH     128
#define BLOCK_HEIGHT    128

    uint mapWidth, mapHeight;
    vec2_t bounds[2], blockSize, dims;

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], map->bBox[BOXLEFT] - BLKMARGIN,
                      map->bBox[BOXBOTTOM] - BLKMARGIN);
    V2_Set(bounds[1], map->bBox[BOXRIGHT] + BLKMARGIN,
                      map->bBox[BOXTOP] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, BLOCK_WIDTH, BLOCK_HEIGHT);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[VX] <= blockSize[VX])
        mapWidth = 1;
    else
        mapWidth = ceil(dims[VX] / blockSize[VX]);

    if(dims[VY] <= blockSize[VY])
        mapHeight = 1;
    else
        mapHeight = ceil(dims[VY] / blockSize[VY]);

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][VX] + mapWidth  * blockSize[VX],
                      bounds[0][VY] + mapHeight * blockSize[VY]);

    map->_particleBlockmap = P_CreateParticleBlockmap(bounds[0], bounds[1], mapWidth, mapHeight);

#undef BLKMARGIN
#undef BLOCK_WIDTH
#undef BLOCK_HEIGHT
}

static void buildLumobjBlockmap(map_t* map)
{
#define BLKMARGIN       8
#define BLOCK_WIDTH     128
#define BLOCK_HEIGHT    128

    uint mapWidth, mapHeight;
    vec2_t bounds[2], blockSize, dims;

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], map->bBox[BOXLEFT] - BLKMARGIN,
                      map->bBox[BOXBOTTOM] - BLKMARGIN);
    V2_Set(bounds[1], map->bBox[BOXRIGHT] + BLKMARGIN,
                      map->bBox[BOXTOP] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, BLOCK_WIDTH, BLOCK_HEIGHT);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[VX] <= blockSize[VX])
        mapWidth = 1;
    else
        mapWidth = ceil(dims[VX] / blockSize[VX]);

    if(dims[VY] <= blockSize[VY])
        mapHeight = 1;
    else
        mapHeight = ceil(dims[VY] / blockSize[VY]);

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][VX] + mapWidth  * blockSize[VX],
                      bounds[0][VY] + mapHeight * blockSize[VY]);

    map->_lumobjBlockmap = P_CreateLumobjBlockmap(bounds[0], bounds[1], mapWidth, mapHeight);

#undef BLKMARGIN
#undef BLOCK_WIDTH
#undef BLOCK_HEIGHT
}

static int C_DECL vertexCompare(const void* p1, const void* p2)
{
    const vertex_t* a = *((const void**) p1);
    const vertex_t* b = *((const void**) p2);

    if(a == b)
        return 0;

    if((int) a->pos[VX] != (int) b->pos[VX])
        return (int) a->pos[VX] - (int) b->pos[VX];

    return (int) a->pos[VY] - (int) b->pos[VY];
}

static void detectDuplicateVertices(halfedgeds_t* halfEdgeDS)
{
    vertex_t** hits;
    uint numVertices = HalfEdgeDS_NumVertices(halfEdgeDS);
    size_t i;

    hits = M_Malloc(numVertices * sizeof(vertex_t*));

    // Sort array of ptrs.
    for(i = 0; i < numVertices; ++i)
        hits[i] = halfEdgeDS->vertices[i];
    qsort(hits, numVertices, sizeof(vertex_t*), vertexCompare);

    // Now mark them off.
    for(i = 0; i < numVertices - 1; ++i)
    {
        // A duplicate?
        if(vertexCompare(hits + i, hits + i + 1) == 0)
        {   // Yes.
            vertex_t* a = hits[i];
            vertex_t* b = hits[i + 1];

            ((mvertex_t*) b->data)->equiv =
                (((mvertex_t*) a->data)->equiv ? ((mvertex_t*) a->data)->equiv : a);
        }
    }

    M_Free(hits);
}

static void findEquivalentVertexes(map_t* map)
{
    uint i, newNum;

    // Scan all linedefs.
    for(i = 0, newNum = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* l = map->lineDefs[i];

        // Handle duplicated vertices.
        while(((mvertex_t*) l->buildData.v[0]->data)->equiv)
        {
            ((mvertex_t*) l->buildData.v[0]->data)->refCount--;
            l->buildData.v[0] = ((mvertex_t*) l->buildData.v[0]->data)->equiv;
            ((mvertex_t*) l->buildData.v[0]->data)->refCount++;
        }

        while(((mvertex_t*) l->buildData.v[1]->data)->equiv)
        {
            ((mvertex_t*) l->buildData.v[1]->data)->refCount--;
            l->buildData.v[1] = ((mvertex_t*) l->buildData.v[1]->data)->equiv;
            ((mvertex_t*) l->buildData.v[1]->data)->refCount++;
        }

        l->buildData.index = newNum + 1;
        map->lineDefs[newNum++] = map->lineDefs[i];
    }
}

static void pruneLineDefs(map_t* map)
{
    uint i, newNum, unused = 0;

    for(i = 0, newNum = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* l = map->lineDefs[i];

        if(!l->buildData.sideDefs[FRONT] && !l->buildData.sideDefs[BACK])
        {
            unused++;

            Z_Free(l);
            continue;
        }

        l->buildData.index = newNum + 1;
        map->lineDefs[newNum++] = l;
    }

    if(newNum < map->numLineDefs)
    {
        if(unused > 0)
            Con_Message("  Pruned %d unused linedefs\n", unused);

        map->numLineDefs = newNum;
    }
}

static void pruneVertices(halfedgeds_t* halfEdgeDS)
{
    uint i, newNum, unused = 0, numVertices = HalfEdgeDS_NumVertices(halfEdgeDS);

    // Scan all vertices.
    for(i = 0, newNum = 0; i < numVertices; ++i)
    {
        vertex_t* v = halfEdgeDS->vertices[i];

        if(((mvertex_t*) v->data)->refCount == 0)
        {
            if(((mvertex_t*) v->data)->equiv == NULL)
                unused++;

            free(v);
            continue;
        }

        ((mvertex_t*) v->data)->index = newNum + 1;
        halfEdgeDS->vertices[newNum++] = v;
    }

    if(newNum < numVertices)
    {
        int dupNum = numVertices - newNum - unused;

        if(unused > 0)
            Con_Message("  Pruned %d unused vertices.\n", unused);

        if(dupNum > 0)
            Con_Message("  Pruned %d duplicate vertices\n", dupNum);

        halfEdgeDS->_numVertices = newNum;
    }
}

static void pruneUnusedSideDefs(map_t* map)
{
    uint i, newNum, unused = 0;

    for(i = 0, newNum = 0; i < map->numSideDefs; ++i)
    {
        sidedef_t* s = map->sideDefs[i];

        if(s->buildData.refCount == 0)
        {
            unused++;

            Z_Free(s);
            continue;
        }

        s->buildData.index = newNum + 1;
        map->sideDefs[newNum++] = s;
    }

    if(newNum < map->numSideDefs)
    {
        int dupNum = map->numSideDefs - newNum - unused;

        if(unused > 0)
            Con_Message("  Pruned %d unused sidedefs\n", unused);

        if(dupNum > 0)
            Con_Message("  Pruned %d duplicate sidedefs\n", dupNum);

        map->numSideDefs = newNum;
    }
}

static void pruneUnusedSectors(map_t* map)
{
    uint i, newNum;

    for(i = 0; i < map->numSideDefs; ++i)
    {
        sidedef_t* s = map->sideDefs[i];

        if(s->sector)
            s->sector->buildData.refCount++;
    }

    // Scan all sectors.
    for(i = 0, newNum = 0; i < map->numSectors; ++i)
    {
        sector_t* s = map->sectors[i];

        if(s->buildData.refCount == 0)
        {
            Z_Free(s);
            continue;
        }

        s->buildData.index = newNum + 1;
        map->sectors[newNum++] = s;
    }

    if(newNum < map->numSectors)
    {
        Con_Message("  Pruned %d unused sectors\n", map->numSectors - newNum);
        map->numSectors = newNum;
    }
}

/**
 * @note Order here is critical!
 * @param flags             @see pruneUnusedObjectsFlags
 */
static void pruneUnusedObjects(map_t* map, int flags)
{
    /**
     * @fixme Pruning cannot be done as game map data object properties
     * are currently indexed by their original indices as determined by the
     * position in the map data. The same problem occurs within ACS scripts
     * and XG line/sector references.
     */
    findEquivalentVertexes(map);

/*    if(flags & PRUNE_LINEDEFS)
        pruneLineDefs(map);*/

    if(flags & PRUNE_VERTEXES)
        pruneVertices(map->_halfEdgeDS);

/*    if(flags & PRUNE_SIDEDEFS)
        pruneUnusedSideDefs(map);

    if(flags & PRUNE_SECTORS)
        pruneUnusedSectors(map);*/
}

static void hardenSectorSubsectorList(map_t* map, uint secIDX)
{
    uint i, n, count;
    sector_t* sec = map->sectors[secIDX];

    count = 0;
    for(i = 0; i < map->numSubsectors; ++i)
    {
        const subsector_t* subsector = map->subsectors[i];

        if(subsector->sector == sec)
            count++;
    }

    sec->subsectors = Z_Malloc((count + 1) * sizeof(subsector_t*), PU_STATIC, NULL);

    n = 0;
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* subsector = map->subsectors[i];

        if(subsector->sector == sec)
        {
            sec->subsectors[n++] = subsector;
        }
    }

    sec->subsectors[n] = NULL; // Terminate.
    sec->subsectorCount = count;
}

/**
 * Build subsector tables for all sectors.
 */
static void buildSectorSubsectorLists(map_t* map)
{
    uint i;

    for(i = 0; i < map->numSectors; ++i)
    {
        hardenSectorSubsectorList(map, i);
    }
}

static void buildSectorLineLists(map_t* map)
{
    typedef struct linelink_s {
        linedef_t*      line;
        struct linelink_s *next;
    } linelink_t;

    uint i, j;
    zblockset_t* lineLinksBlockSet;
    linelink_t** sectorLineLinks;

    // build line tables for each sector.
    lineLinksBlockSet = Z_BlockCreate(sizeof(linelink_t), 512, PU_STATIC);
    sectorLineLinks = M_Calloc(sizeof(linelink_t*) * map->numSectors);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* li = map->lineDefs[i];
        uint secIDX;
        linelink_t* link;

        if(li->flags & LF_POLYOBJ)
            continue;

        if(LINE_FRONTSIDE(li))
        {
            link = Z_BlockNewElement(lineLinksBlockSet);

            secIDX = P_ObjectRecord(DMU_SECTOR, LINE_FRONTSECTOR(li))->id - 1;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            LINE_FRONTSECTOR(li)->lineDefCount++;
        }

        if(LINE_BACKSIDE(li) && LINE_BACKSECTOR(li) != LINE_FRONTSECTOR(li))
        {
            link = Z_BlockNewElement(lineLinksBlockSet);

            secIDX = P_ObjectRecord(DMU_SECTOR, LINE_BACKSECTOR(li))->id - 1;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            LINE_BACKSECTOR(li)->lineDefCount++;
        }
    }

    // Harden the sector line links into arrays.
    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sector = map->sectors[i];

        if(sectorLineLinks[i])
        {
            linelink_t* link = sectorLineLinks[i];
            uint numLineDefs;

            /**
             * The behaviour of some algorithms used in original DOOM are
             * dependant upon the order of these lists (e.g., EV_DoFloor
             * and EV_BuildStairs). Lets be helpful and use the same order.
             *
             * Sort: LineDef index ascending (zero based).
             */
            numLineDefs = 0;
            while(link)
            {
                numLineDefs++;
                link = link->next;
            }

            sector->lineDefs =
                Z_Malloc((numLineDefs + 1) * sizeof(linedef_t*), PU_STATIC, 0);

            j = numLineDefs - 1;
            link = sectorLineLinks[i];
            while(link)
            {
                sector->lineDefs[j--] = link->line;
                link = link->next;
            }

            sector->lineDefs[numLineDefs] = NULL; // terminate.
            sector->lineDefCount = numLineDefs;
        }
        else
        {
            sector->lineDefs = NULL;
            sector->lineDefCount = 0;
        }
    }

    // Free temporary storage.
    Z_BlockDestroy(lineLinksBlockSet);
    M_Free(sectorLineLinks);
}

static void finishSectors2(map_t* map)
{
    uint i;

    for(i = 0; i < map->numSectors; ++i)
    {
        uint k;
        float min[2], max[2];
        sector_t* sec = map->sectors[i];

        Sector_UpdateBounds(sec);
        Sector_Bounds(sec, min, max);

        // Set the degenmobj_t to the middle of the bounding box.
        sec->soundOrg.pos[VX] = (min[VX] + max[VX]) / 2;
        sec->soundOrg.pos[VY] = (min[VY] + max[VY]) / 2;
        sec->soundOrg.pos[VZ] = 0;

        // Set the position of the sound origin for all plane sound origins.
        // Set target heights for all planes.
        for(k = 0; k < sec->planeCount; ++k)
        {
            sec->planes[k]->target = sec->planes[k]->height;
        }
    }
}

static void updateMapBounds(map_t* map)
{
    uint i;

    memset(map->bBox, 0, sizeof(map->bBox));
    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sec = map->sectors[i];

        if(i == 0)
        {
            // The first sector is used as is.
            memcpy(map->bBox, sec->bBox, sizeof(map->bBox));
        }
        else
        {
            // Expand the bounding box.
            M_JoinBoxes(map->bBox, sec->bBox);
        }
    }
}

/**
 * Completes the linedef loading by resolving the front/back
 * sector ptrs which we couldn't do earlier as the sidedefs
 * hadn't been loaded at the time.
 */
static void finishLineDefs2(map_t* map)
{
    uint i;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* ld = map->lineDefs[i];
        vertex_t* v[2];

        if(!ld->hEdges[0])
            continue;

        v[0] = ld->hEdges[0]->HE_v1;
        v[1] = ld->hEdges[1]->HE_v2;

        ld->dX = v[1]->pos[VX] - v[0]->pos[VX];
        ld->dY = v[1]->pos[VY] - v[0]->pos[VY];

        // Calculate the accurate length of each line.
        ld->length = P_AccurateDistance(ld->dX, ld->dY);
        ld->angle = bamsAtan2((int) (v[1]->pos[VY] - v[0]->pos[VY]),
                              (int) (v[1]->pos[VX] - v[0]->pos[VX])) << FRACBITS;

        if(!ld->dX)
            ld->slopeType = ST_VERTICAL;
        else if(!ld->dY)
            ld->slopeType = ST_HORIZONTAL;
        else
        {
            if(ld->dY / ld->dX > 0)
                ld->slopeType = ST_POSITIVE;
            else
                ld->slopeType = ST_NEGATIVE;
        }

        if(v[0]->pos[VX] < v[1]->pos[VX])
        {
            ld->bBox[BOXLEFT]   = v[0]->pos[VX];
            ld->bBox[BOXRIGHT]  = v[1]->pos[VX];
        }
        else
        {
            ld->bBox[BOXLEFT]   = v[1]->pos[VX];
            ld->bBox[BOXRIGHT]  = v[0]->pos[VX];
        }

        if(v[0]->pos[VY] < v[1]->pos[VY])
        {
            ld->bBox[BOXBOTTOM] = v[0]->pos[VY];
            ld->bBox[BOXTOP]    = v[1]->pos[VY];
        }
        else
        {
            ld->bBox[BOXBOTTOM] = v[1]->pos[VY];
            ld->bBox[BOXTOP]    = v[0]->pos[VY];
        }
    }
}

static void findSubsectorMidPoints(map_t* map)
{
    uint i;

    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* subsector = map->subsectors[i];

        Subsector_UpdateMidPoint(subsector);
    }
}

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static int C_DECL lineAngleSorter(const void* a, const void* b)
{
    uint i;
    fixed_t dx, dy;
    binangle_t angles[2];
    lineowner_t* own[2];
    linedef_t* line;

    own[0] = (lineowner_t*) a;
    own[1] = (lineowner_t*) b;
    for(i = 0; i < 2; ++i)
    {
        if(own[i]->LO_prev) // We have a cached result.
        {
            angles[i] = own[i]->angle;
        }
        else
        {
            vertex_t* otherVtx;

            line = own[i]->lineDef;
            otherVtx = line->buildData.v[line->buildData.v[0] == rootVtx? 1:0];

            dx = otherVtx->pos[VX] - rootVtx->pos[VX];
            dy = otherVtx->pos[VY] - rootVtx->pos[VY];

            own[i]->angle = angles[i] = bamsAtan2(-100 *dx, 100 * dy);

            // Mark as having a cached angle.
            own[i]->LO_prev = (lineowner_t*) 1;
        }
    }

    return (angles[1] - angles[0]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return              Ptr to the newly merged list.
 */
static lineowner_t* mergeLineOwners(lineowner_t* left, lineowner_t* right,
                                    int (C_DECL *compare) (const void* a, const void* b))
{
    lineowner_t tmp, *np;

    np = &tmp;
    tmp.LO_next = np;
    while(left != NULL && right != NULL)
    {
        if(compare(left, right) <= 0)
        {
            np->LO_next = left;
            np = left;

            left = left->LO_next;
        }
        else
        {
            np->LO_next = right;
            np = right;

            right = right->LO_next;
        }
    }

    // At least one of these lists is now empty.
    if(left)
        np->LO_next = left;
    if(right)
        np->LO_next = right;

    // Is the list empty?
    if(tmp.LO_next == &tmp)
        return NULL;

    return tmp.LO_next;
}

static lineowner_t* splitLineOwners(lineowner_t* list)
{
    lineowner_t* lista, *listb, *listc;

    if(!list)
        return NULL;

    lista = listb = listc = list;
    do
    {
        listc = listb;
        listb = listb->LO_next;
        lista = lista->LO_next;
        if(lista != NULL)
            lista = lista->LO_next;
    } while(lista);

    listc->LO_next = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static lineowner_t* sortLineOwners(lineowner_t* list,
                                   int (C_DECL *compare) (const void* a, const void* b))
{
    lineowner_t* p;

    if(list && list->LO_next)
    {
        p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(vertexinfo_t* vInfo, linedef_t* lineDef, byte vertex,
                               lineowner_t** storage)
{
    lineowner_t* newOwner;

    // Has this line already been registered with this vertex?
    if(vInfo->numLineOwners != 0)
    {
        lineowner_t* owner = vInfo->lineOwners;

        do
        {
            if(owner->lineDef == lineDef)
                return; // Yes, we can exit.

            owner = owner->LO_next;
        } while(owner);
    }

    //Add a new owner.
    vInfo->numLineOwners++;

    newOwner = (*storage)++;
    newOwner->lineDef = lineDef;
    newOwner->LO_prev = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->LO_next = vInfo->lineOwners;
    vInfo->lineOwners = newOwner;

    // Link the line to its respective owner node.
    lineDef->L_vo(vertex) = newOwner;
}

#if _DEBUG
static void checkVertexOwnerRings(vertexinfo_t* vertexInfo, uint num)
{
    uint i;

    for(i = 0; i < num; ++i)
    {
        vertexinfo_t* vInfo = &vertexInfo[i];

        validCount++;

        if(vInfo->numLineOwners)
        {
            lineowner_t* base, *owner;

            owner = base = vInfo->lineOwners;
            do
            {
                if(owner->lineDef->validCount == validCount)
                    Con_Error("LineDef linked multiple times in owner link ring!");
                owner->lineDef->validCount = validCount;

                if(owner->LO_prev->LO_next != owner || owner->LO_next->LO_prev != owner)
                    Con_Error("Invalid line owner link ring!");

                owner = owner->LO_next;
            } while(owner != base);
        }
    }
}
#endif

/**
 * Generates the line owner rings for each vertex. Each ring includes all
 * the lines which the vertex belongs to sorted by angle, (the rings are
 * arranged in clockwise order, east = 0).
 */
static void buildVertexOwnerRings(map_t* map, vertexinfo_t* vertexInfo)
{
    lineowner_t* allocator;
    halfedgeds_t* halfEdgeDS = Map_HalfEdgeDS(map);
    uint i, numVertices = HalfEdgeDS_NumVertices(halfEdgeDS);

    // We know how many vertex line owners we need (numLineDefs * 2).
    map->lineOwners = Z_Calloc(sizeof(lineowner_t) * map->numLineDefs * 2, PU_STATIC, 0);
    allocator = map->lineOwners;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* lineDef = map->lineDefs[i];
        uint j;

        for(j = 0; j < 2; ++j)
        {
            vertexinfo_t* vInfo =
                &vertexInfo[((mvertex_t*) lineDef->buildData.v[j]->data)->index - 1];

            setVertexLineOwner(vInfo, lineDef, j, &allocator);
        }
    }

    // Sort line owners and then finish the rings.
    for(i = 0; i < numVertices; ++i)
    {
        vertexinfo_t* vInfo = &vertexInfo[i];

        // Line owners:
        if(vInfo->numLineOwners != 0)
        {
            lineowner_t* owner, *last;
            binangle_t firstAngle;

            // Redirect the linedef links to the hardened map.
            owner = vInfo->lineOwners;
            while(owner)
            {
                owner->lineDef = map->lineDefs[owner->lineDef->buildData.index - 1];
                owner = owner->LO_next;
            }

            // Sort them; ordered clockwise by angle.
            rootVtx = halfEdgeDS->vertices[i];
            vInfo->lineOwners = sortLineOwners(vInfo->lineOwners, lineAngleSorter);

            // Finish the linking job and convert to relative angles.
            // They are only singly linked atm, we need them to be doubly
            // and circularly linked.
            firstAngle = vInfo->lineOwners->angle;
            last = vInfo->lineOwners;
            owner = last->LO_next;
            while(owner)
            {
                owner->LO_prev = last;

                // Convert to a relative angle between last and this.
                last->angle = last->angle - owner->angle;

                last = owner;
                owner = owner->LO_next;
            }
            last->LO_next = vInfo->lineOwners;
            vInfo->lineOwners->LO_prev = last;

            // Set the angle of the last owner.
            last->angle = last->angle - firstAngle;
        }
    }
}

static void addLineDefsToDMU(map_t* map)
{
    uint i;
    for(i = 0; i < map->numLineDefs; ++i)
        P_CreateObjectRecord(DMU_LINEDEF, map->lineDefs[i]);
}

static void addSideDefsToDMU(map_t* map)
{
    uint i;
    for(i = 0; i < map->numSideDefs; ++i)
        P_CreateObjectRecord(DMU_SIDEDEF, map->sideDefs[i]);
}

static void addSectorsToDMU(map_t* map)
{
    uint i;
    for(i = 0; i < map->numSectors; ++i)
        P_CreateObjectRecord(DMU_SECTOR, map->sectors[i]);
}

static int buildSeg(hedge_t* hEdge, void* context)
{
    hedge_info_t* info = (hedge_info_t*) hEdge->data;

    hEdge->data = NULL;

    if(!(info->lineDef &&
         (!info->sector || (info->side == BACK && info->lineDef->buildData.windowEffect))))
    {
        Map_CreateSeg((map_t*) context, info->lineDef, info->side, hEdge);
    }

    return true; // Continue iteration.
}

static sector_t* pickSectorFromHEdges(const hedge_t* firstHEdge, boolean allowSelfRef)
{
    const hedge_t* hEdge;
    sector_t* sector = NULL;

    hEdge = firstHEdge;
    do
    {
        if(!allowSelfRef && hEdge->twin &&
           ((hedge_info_t*) hEdge->data)->sector ==
           ((hedge_info_t*) hEdge->twin->data)->sector)
            continue;

        if(((hedge_info_t*) hEdge->data)->lineDef &&
           ((hedge_info_t*) hEdge->data)->sector)
        {
            linedef_t* lineDef = ((hedge_info_t*) hEdge->data)->lineDef;

            if(lineDef->buildData.windowEffect && ((hedge_info_t*) hEdge->data)->side == 1)
                sector = lineDef->buildData.windowEffect;
            else
                sector = lineDef->buildData.sideDefs[
                    ((hedge_info_t*) hEdge->data)->side]->sector;
        }
    } while(!sector && (hEdge = hEdge->next) != firstHEdge);

    return sector;
}

static boolean C_DECL buildSubsector(binarytree_t* tree, void* context)
{
    if(!BinaryTree_IsLeaf(tree))
    {
        node_t* node = BinaryTree_GetData(tree);
        binarytree_t* child;

        child = BinaryTree_GetChild(tree, RIGHT);
        if(child && BinaryTree_IsLeaf(child))
        {
            face_t* face = (face_t*) BinaryTree_GetData(child);
            sector_t* sector;

            /**
             * Determine which sector this subsector belongs to.
             * On the first pass, we are picky; do not consider half-edges from
             * self-referencing linedefs. If that fails, take whatever we can find.
             */
            sector = pickSectorFromHEdges(face->hEdge, false);
            if(!sector)
                sector = pickSectorFromHEdges(face->hEdge, true);

            Map_CreateSubsector((map_t*) context, face, sector);
        }

        child = BinaryTree_GetChild(tree, LEFT);
        if(child && BinaryTree_IsLeaf(child))
        {
            face_t* face = (face_t*) BinaryTree_GetData(child);
            sector_t* sector;

            /**
             * Determine which sector this subsector belongs to.
             * On the first pass, we are picky; do not consider half-edges from
             * self-referencing linedefs. If that fails, take whatever we can find.
             */
            sector = pickSectorFromHEdges(face->hEdge, false);
            if(!sector)
                sector = pickSectorFromHEdges(face->hEdge, true);

            Map_CreateSubsector((map_t*) context, face, sector);
        }
    }

    return true; // Continue iteration.
}

/**
 * Build the BSP for the given map.
 *
 * @param map           The map to build the BSP for.
 * @return              @c true, if completed successfully.
 */
static boolean buildBSP(map_t* map)
{
    uint startTime = Sys_GetRealTime();
    nodebuilder_t* nb;

    VERBOSE(
    Con_Message("buildBSP: Processing map using tunable factor of %d...\n",
                bspFactor));

    nb = P_CreateNodeBuilder(map, bspFactor);
    NodeBuilder_Build(nb);
    map->_rootNode = nb->rootNode;

    if(map->_rootNode)
    {   // Build subsectors and segs.
        BinaryTree_PostOrder(map->_rootNode, buildSubsector, map);
        HalfEdgeDS_IterateHEdges(map->_halfEdgeDS, buildSeg, map);

        if(verbose >= 1)
        {
            Con_Message("  Built %d Nodes, %d Subsectors, %d Segs.\n",
                        map->numNodes, map->numSubsectors, map->numSegs);

            if(!BinaryTree_IsLeaf(map->_rootNode))
            {
                long rHeight = (long) BinaryTree_GetHeight(BinaryTree_GetChild(map->_rootNode, RIGHT));
                long lHeight = (long) BinaryTree_GetHeight(BinaryTree_GetChild(map->_rootNode, LEFT));

                Con_Message("  Balance %+ld (l%ld - r%ld).\n",
                            lHeight - rHeight, lHeight, rHeight);
            }
        }
    }

    P_DestroyNodeBuilder(nb);

    // How much time did we spend?
    VERBOSE(Con_Message("  Done in %.2f seconds.\n",
                        (Sys_GetRealTime() - startTime) / 1000.0f));

    return map->_rootNode != NULL;
}

#if 0 /* Currently unused. */
/**
 * @return              The "lowest" vertex (normally the left-most, but if
 *                      the line is vertical, then the bottom-most).
 *                      @c => 0 for start, 1 for end.
 */
static __inline int lineVertexLowest(const linedef_t* l)
{
    return (((int) l->v[0]->buildData.pos[VX] < (int) l->v[1]->buildData.pos[VX] ||
             ((int) l->v[0]->buildData.pos[VX] == (int) l->v[1]->buildData.pos[VX] &&
              (int) l->v[0]->buildData.pos[VY] < (int) l->v[1]->buildData.pos[VY]))? 0 : 1);
}

static int C_DECL lineStartCompare(const void* p1, const void* p2)
{
    const linedef_t*    a = (const linedef_t*) p1;
    const linedef_t*    b = (const linedef_t*) p2;
    vertex_t*           c, *d;

    // Determine left-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[1] : a->v[0]);
    d = (lineVertexLowest(b)? b->v[1] : b->v[0]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

static int C_DECL lineEndCompare(const void* p1, const void* p2)
{
    const linedef_t*    a = (const linedef_t*) p1;
    const linedef_t*    b = (const linedef_t*) p2;
    vertex_t*           c, *d;

    // Determine right-most vertex of each line.
    c = (lineVertexLowest(a)? a->v[0] : a->v[1]);
    d = (lineVertexLowest(b)? b->v[0] : b->v[1]);

    if((int) c->buildData.pos[VX] != (int) d->buildData.pos[VX])
        return (int) c->buildData.pos[VX] - (int) d->buildData.pos[VX];

    return (int) c->buildData.pos[VY] - (int) d->buildData.pos[VY];
}

size_t numOverlaps;

boolean testOverlaps(linedef_t* b, void* data)
{
    linedef_t*          a = (linedef_t*) data;

    if(a != b)
    {
        if(lineStartCompare(a, b) == 0)
            if(lineEndCompare(a, b) == 0)
            {   // Found an overlap!
                b->buildData.overlap =
                    (a->buildData.overlap ? a->buildData.overlap : a);
                numOverlaps++;
            }
    }

    return true; // Continue iteration.
}

typedef struct {
    blockmap_t*         blockMap;
    uint                block[2];
} findoverlaps_params_t;

boolean findOverlapsForLineDef(linedef_t* l, void* data)
{
    findoverlaps_params_t* params = (findoverlaps_params_t*) data;

    LineDefBlockmap_Iterate(params->blockMap, params->block, testOverlaps, l, false);
    return true; // Continue iteration.
}

/**
 * \note Does not detect partially overlapping lines!
 */
void MPE_DetectOverlappingLines(map_t* map)
{
    uint                x, y, bmapDimensions[2];
    findoverlaps_params_t params;

    params.blockMap = map->blockMap;
    numOverlaps = 0;

    Blockmap_Dimensions(map->blockMap, bmapDimensions);

    for(y = 0; y < bmapDimensions[VY]; ++y)
        for(x = 0; x < bmapDimensions[VX]; ++x)
        {
            params.block[VX] = x;
            params.block[VY] = y;

            LineDefBlockmap_Iterate(map->blockMap, params.block,
                                    findOverlapsForLineDef, &params, false);
        }

    if(numOverlaps > 0)
        VERBOSE(Con_Message("Detected %lu overlapped linedefs\n",
                            (unsigned long) numOverlaps));
}
#endif

static ownernode_t* newOwnerNode(void)
{
    ownernode_t*        node;

    if(unusedNodeList)
    {   // An existing node is available for re-use.
        node = unusedNodeList;
        unusedNodeList = unusedNodeList->next;

        node->next = NULL;
        node->data = NULL;
    }
    else
    {   // Need to allocate another.
        node = M_Malloc(sizeof(ownernode_t));
    }

    return node;
}

static void setSectorOwner(ownerlist_t* ownerList, subsector_t* subsector)
{
    ownernode_t* node;

    if(!subsector)
        return;

    // Add a new owner.
    // NOTE: No need to check for duplicates.
    ownerList->count++;

    node = newOwnerNode();
    node->data = subsector;
    node->next = ownerList->head;
    ownerList->head = node;
}

static void findSubSectorsAffectingSector(map_t* map, uint secIDX)
{
    uint i;
    ownernode_t* node, *p;
    float bbox[4];
    ownerlist_t subsectorOwnerList;
    sector_t* sec = map->sectors[secIDX];

    memset(&subsectorOwnerList, 0, sizeof(subsectorOwnerList));

    memcpy(bbox, sec->bBox, sizeof(bbox));
    bbox[BOXLEFT]   -= 128;
    bbox[BOXRIGHT]  += 128;
    bbox[BOXTOP]    += 128;
    bbox[BOXBOTTOM] -= 128;
/*
#if _DEBUG
Con_Message("sector %i: (%f,%f) - (%f,%f)\n", c,
            bbox[BOXLEFT], bbox[BOXTOP], bbox[BOXRIGHT], bbox[BOXBOTTOM]);
#endif
*/
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* subsector =  map->subsectors[i];

        // Is this subsector close enough?
        if(subsector->sector == sec || // subsector is IN this sector
           (subsector->midPoint[VX] > bbox[BOXLEFT] &&
            subsector->midPoint[VX] < bbox[BOXRIGHT] &&
            subsector->midPoint[VY] < bbox[BOXTOP] &&
            subsector->midPoint[VY] > bbox[BOXBOTTOM]))
        {
            // It will contribute to the reverb settings of this sector.
            setSectorOwner(&subsectorOwnerList, subsector);
        }
    }

    // Now harden the list.
    sec->numReverbSubsectorAttributors = subsectorOwnerList.count;
    if(sec->numReverbSubsectorAttributors)
    {
        subsector_t** ptr;

        sec->reverbSubsectors =
            Z_Malloc((sec->numReverbSubsectorAttributors + 1) * sizeof(subsector_t*),
                     PU_STATIC, 0);

        for(i = 0, ptr = sec->reverbSubsectors, node = subsectorOwnerList.head;
            i < sec->numReverbSubsectorAttributors; ++i, ptr++)
        {
            p = node->next;
            *ptr = (subsector_t*) node->data;

            if(i < map->numSectors - 1)
            {   // Move this node to the unused list for re-use.
                node->next = unusedNodeList;
                unusedNodeList = node;
            }
            else
            {   // No further use for the nodes.
                M_Free(node);
            }
            node = p;
        }
        *ptr = NULL; // terminate.
    }
}

/**
 * Called during map init to determine which subsectors affect the reverb
 * properties of all sectors. Given that subsectors do not change shape (in
 * two dimensions at least), they do not move and are not created/destroyed
 * once the map has been loaded; this step can be pre-processed.
 */
static void initSoundEnvironment(map_t* map)
{
    ownernode_t* node, *p;
    uint i;

    for(i = 0; i < map->numSectors; ++i)
    {
        findSubSectorsAffectingSector(map, i);
    }

    // Free any nodes left in the unused list.
    node = unusedNodeList;
    while(node)
    {
        p = node->next;
        M_Free(node);
        node = p;
    }
    unusedNodeList = NULL;
}

static int addVertexToDMU(vertex_t* vertex, void* context)
{
    P_CreateObjectRecord(DMU_VERTEX, vertex);
    return true; // Continue iteration.
}

void Map_EditEnd(map_t* map)
{
    uint i;

    if(!map)
        return;
    if(!map->editActive)
        return;

    /**
     * Perform cleanup on the loaded map data, removing duplicate vertexes,
     * pruning unused sectors etc, etc...
     */
    detectDuplicateVertices(map->_halfEdgeDS);
    pruneUnusedObjects(map, PRUNE_ALL);

    addSectorsToDMU(map);
    addSideDefsToDMU(map);
    addLineDefsToDMU(map);

    /**
     * Build a LineDef blockmap for this map.
     */
    {
    uint startTime = Sys_GetRealTime();

    buildLineDefBlockmap(map);

    // How much time did we spend?
    VERBOSE(Con_Message("buildLineDefBlockmap: Done in %.2f seconds.\n",
            (Sys_GetRealTime() - startTime) / 1000.0f))
    }

    /*builtOK =*/ buildBSP(map);

    HalfEdgeDS_IterateVertices(map->_halfEdgeDS, addVertexToDMU, NULL);

    for(i = 0; i < map->numPolyObjs; ++i)
    {
        polyobj_t* po = map->polyObjs[i];
        uint j;

        po->originalPts = Z_Malloc(po->numLineDefs * sizeof(fvertex_t), PU_STATIC, 0);
        po->prevPts = Z_Malloc(po->numLineDefs * sizeof(fvertex_t), PU_STATIC, 0);

        for(j = 0; j < po->numLineDefs; ++j)
        {
            linedef_t* lineDef = po->lineDefs[j];
            hedge_t* fHEdge, *bHEdge;

            fHEdge = Z_Calloc(sizeof(*fHEdge), PU_STATIC, 0);
            fHEdge->face = NULL;
            fHEdge->vertex = map->_halfEdgeDS->vertices[
                ((mvertex_t*) lineDef->buildData.v[0]->data)->index - 1];
            fHEdge->vertex->hEdge = fHEdge;

            bHEdge = Z_Calloc(sizeof(*bHEdge), PU_STATIC, 0);
            bHEdge->face = NULL;
            bHEdge->vertex = map->_halfEdgeDS->vertices[
                ((mvertex_t*) lineDef->buildData.v[1]->data)->index - 1];
            bHEdge->vertex->hEdge = bHEdge;

            fHEdge->twin = bHEdge;
            bHEdge->twin = fHEdge;

            lineDef->hEdges[0] = lineDef->hEdges[1] = fHEdge;

            // The original Pts are based off the anchor Pt, and are unique
            // to each linedef.
            po->originalPts[j].pos[VX] = lineDef->L_v1->pos[VX] - po->pos[VX];
            po->originalPts[j].pos[VY] = lineDef->L_v1->pos[VY] - po->pos[VY];

            po->lineDefs[j] = (linedef_t*) P_ObjectRecord(DMU_LINEDEF, lineDef);
        }

        // Temporary: Create a seg for each line of this polyobj.
        po->numSegs = po->numLineDefs;
        po->segs = Z_Calloc(sizeof(seg_t) * po->numSegs, PU_STATIC, 0);

        for(j = 0; j < po->numSegs; ++j)
        {
            linedef_t* lineDef = ((objectrecord_t*) po->lineDefs[j])->obj;
            seg_t* seg = &po->segs[j];

            lineDef->hEdges[0]->data = seg;
            seg->sideDef = map->sideDefs[lineDef->buildData.sideDefs[FRONT]->buildData.index - 1];
        }
    }

    buildSectorSubsectorLists(map);
    buildSectorLineLists(map);
    finishLineDefs2(map);
    finishSectors2(map);
    updateMapBounds(map);

    initSoundEnvironment(map);
    findSubsectorMidPoints(map);

    {
    uint numVertices = HalfEdgeDS_NumVertices(map->_halfEdgeDS);
    vertexinfo_t* vertexInfo = M_Calloc(sizeof(vertexinfo_t) * numVertices);

    buildVertexOwnerRings(map, vertexInfo);

#if _DEBUG
checkVertexOwnerRings(vertexInfo, numVertices);
#endif

    // Build the vertex lineowner info.
    // @todo now redundant, rewire fakeradio to use HalfEdgeDS.
    for(i = 0; i < numVertices; ++i)
    {
        vertex_t* vertex = map->_halfEdgeDS->vertices[i];
        vertexinfo_t* vInfo = &vertexInfo[i];

        ((mvertex_t*) vertex->data)->lineOwners = vInfo->lineOwners;
        ((mvertex_t*) vertex->data)->numLineOwners = vInfo->numLineOwners;
    }
    M_Free(vertexInfo);
    }

    map->editActive = false;
}

static boolean PIT_ClientMobjTicker(clmobj_t *cmo, void *parm)
{
    P_MobjTicker((thinker_t*) &cmo->mo, NULL);
    return true; // Continue iteration.
}

void Map_Ticker(map_t* map, timespan_t time)
{
    assert(map);

    // New ptcgens for planes?
    checkPtcPlanes(map);

    Thinkers_Iterate(map->_thinkers, gx.MobjThinker, ITF_PUBLIC | ITF_PRIVATE, P_MobjTicker, NULL);

    // Check all client mobjs.
    Cl_MobjIterator(map, PIT_ClientMobjTicker, NULL);
}

void Map_BeginFrame(map_t* map, boolean resetNextViewer)
{
    assert(map);

    clearSectorFlags(map);
    if(map->_watchedPlaneList)
        interpolateWatchedPlanes(map, resetNextViewer);
    if(map->_movingSurfaceList)
        interpolateMovingSurfaces(map, resetNextViewer);

    if(!freezeRLs)
    {
        clearSubsectorContacts(map);

        LightGrid_Update(map->_lightGrid);
        SB_BeginFrame(map);
        LO_ClearForFrame(map);
        Rend_InitDecorationsForFrame(map);

        ParticleBlockmap_Empty(map->_particleBlockmap);
        R_CreateParticleLinks(map);

        LumobjBlockmap_Empty(map->_lumobjBlockmap);
        Rend_AddLuminousDecorations(map);
        LO_AddLuminousMobjs(map);
    }
}

void Map_EndFrame(map_t* map)
{
    assert(map);

    if(!freezeRLs)
    {
        // Wrap up with Source, Bias lights.
        SB_EndFrame(map);

        // Update the bias light editor.
        SBE_EndFrame(map);
    }
}

/**
 * This ID is the name of the lump tag that marks the beginning of map
 * data, e.g. "MAP03" or "E2M8".
 */
const char* Map_ID(map_t* map)
{
    if(!map)
        return NULL;

    return map->mapID;
}

/**
 * @return              The 'unique' identifier of the map.
 */
const char* Map_UniqueName(map_t* map)
{
    if(!map)
        return NULL;

    return map->uniqueID;
}

void Map_Bounds(map_t* map, float* min, float* max)
{
    min[VX] = map->bBox[BOXLEFT];
    min[VY] = map->bBox[BOXBOTTOM];

    max[VX] = map->bBox[BOXRIGHT];
    max[VY] = map->bBox[BOXTOP];
}

/**
 * Get the ambient light level of the specified map.
 */
int Map_AmbientLightLevel(map_t* map)
{
    if(!map)
        return 0;

    return map->ambientLightLevel;
}

thinkers_t* Map_Thinkers(map_t* map)
{
    assert(map);
    return map->_thinkers;
}

halfedgeds_t* Map_HalfEdgeDS(map_t* map)
{
    assert(map);
    return map->_halfEdgeDS;
}

mobjblockmap_t* Map_MobjBlockmap(map_t* map)
{
    assert(map);
    return map->_mobjBlockmap;
}

linedefblockmap_t* Map_LineDefBlockmap(map_t* map)
{
    assert(map);
    return map->_lineDefBlockmap;
}

subsectorblockmap_t* Map_SubsectorBlockmap(map_t* map)
{
    assert(map);
    return map->_subsectorBlockmap;
}

particleblockmap_t* Map_ParticleBlockmap(map_t* map)
{
    assert(map);
    return map->_particleBlockmap;
}

lumobjblockmap_t* Map_LumobjBlockmap(map_t* map)
{
    assert(map);
    return map->_lumobjBlockmap;
}

lightgrid_t* Map_LightGrid(map_t* map)
{
    assert(map);
    return map->_lightGrid;
}

void Map_MarkAllSectorsForLightGridUpdate(map_t* map)
{
    assert(map);
    {
    lightgrid_t* lg = map->_lightGrid;

    if(lg->inited)
    {
        uint i;

        // Mark all blocks and contributors.
        for(i = 0; i < map->numSectors; ++i)
        {
            sector_t* sec = map->sectors[i];
            LightGrid_MarkForUpdate(map->_lightGrid, sec->blocks, sec->changedBlockCount, sec->blockCount);
        }
    }
    }
}

seg_t* Map_CreateSeg(map_t* map, linedef_t* lineDef, byte side, hedge_t* hEdge)
{
    assert(map);
    assert(hEdge);
    {
    seg_t* seg = P_CreateSeg();

    seg->hEdge = hEdge;
    seg->side = side;
    seg->sideDef = NULL;
    if(lineDef && lineDef->buildData.sideDefs[seg->side])
        seg->sideDef = map->sideDefs[lineDef->buildData.sideDefs[seg->side]->buildData.index - 1];

    if(seg->sideDef)
    {
        linedef_t* ldef = seg->sideDef->lineDef;
        vertex_t* vtx = ldef->buildData.v[seg->side];

        seg->offset = P_AccurateDistance(hEdge->HE_v1->pos[VX] - vtx->pos[VX],
                                         hEdge->HE_v1->pos[VY] - vtx->pos[VY]);
    }

    seg->angle =
        bamsAtan2((int) (hEdge->twin->vertex->pos[VY] - hEdge->vertex->pos[VY]),
                  (int) (hEdge->twin->vertex->pos[VX] - hEdge->vertex->pos[VX])) << FRACBITS;

    // Calculate the length of the segment. We need this for
    // the texture coordinates. -jk
    seg->length = P_AccurateDistance(hEdge->HE_v2->pos[VX] - hEdge->HE_v1->pos[VX],
                                     hEdge->HE_v2->pos[VY] - hEdge->HE_v1->pos[VY]);

    if(seg->length == 0)
        seg->length = 0.01f; // Hmm...

    // Calculate the surface normals
    // Front first
    if(seg->sideDef)
    {
        surface_t* surface = &seg->sideDef->SW_topsurface;

        surface->normal[VY] = (hEdge->HE_v1->pos[VX] - hEdge->HE_v2->pos[VX]) / seg->length;
        surface->normal[VX] = (hEdge->HE_v2->pos[VY] - hEdge->HE_v1->pos[VY]) / seg->length;
        surface->normal[VZ] = 0;

        // All surfaces of a sidedef have the same normal.
        memcpy(seg->sideDef->SW_middlenormal, surface->normal, sizeof(surface->normal));
        memcpy(seg->sideDef->SW_bottomnormal, surface->normal, sizeof(surface->normal));
    }

    hEdge->data = seg;

    P_CreateObjectRecord(DMU_SEG, seg);
    map->segs = Z_Realloc(map->segs, ++map->numSegs * sizeof(seg_t*), PU_STATIC);
    map->segs[map->numSegs-1] = seg;
    return seg;
    }
}

subsector_t* Map_CreateSubsector(map_t* map, face_t* face, sector_t* sector)
{
    assert(map);
    assert(face);
    {
    subsector_t* subsector = P_CreateSubsector();
    size_t hEdgeCount;
    hedge_t* hEdge;

    hEdgeCount = 0;
    hEdge = face->hEdge;
    do
    {
        hEdgeCount++;
    } while((hEdge = hEdge->next) != face->hEdge);

    subsector->face = face;
    subsector->hEdgeCount = (uint) hEdgeCount;

    subsector->sector = sector;
    if(!subsector->sector)
        Con_Message("Map_CreateSubsector: Warning orphan subsector %p (%i half-edges).\n",
                    subsector, subsector->hEdgeCount);

    face->data = (void*) subsector;

    P_CreateObjectRecord(DMU_SUBSECTOR, subsector);
    map->subsectors = Z_Realloc(map->subsectors, ++map->numSubsectors * sizeof(subsector_t*), PU_STATIC);
    map->subsectors[map->numSubsectors-1] = subsector;

    return subsector;
    }
}

node_t* Map_CreateNode(map_t* map, float x, float y, float dX, float dY,
                       float rightAABB[4], float leftAABB[4])
{
    assert(map);
    {
    node_t* node = P_CreateNode(x, y, dX, dY, rightAABB, leftAABB);

    P_CreateObjectRecord(DMU_NODE, node);
    map->nodes = Z_Realloc(map->nodes, ++map->numNodes * sizeof(node_t*), PU_STATIC);
    map->nodes[map->numNodes-1] = node;
    return node;
    }
}

static void initLinks(map_t* map)
{
    uint i;

    // Initialize node piles and line rings.
    map->mobjNodes = P_CreateNodePile(256); // Allocate a small pile.
    map->lineNodes = P_CreateNodePile(map->numLineDefs + 1000);

    // Allocate the rings.
    map->lineLinks = Z_Malloc(sizeof(*map->lineLinks) * map->numLineDefs, PU_STATIC, 0);
    for(i = 0; i < map->numLineDefs; ++i)
        map->lineLinks[i] = NP_New(map->lineNodes, NP_ROOT_NODE);
}

/**
 * Fixing the sky means that for adjacent sky sectors the lower sky
 * ceiling is lifted to match the upper sky. The raising only affects
 * rendering, it has no bearing on gameplay.
 */
void Map_InitSkyFix(map_t* map)
{
    uint i;

    assert(map);

    map->skyFixFloor = DDMAXFLOAT;
    map->skyFixCeiling = DDMINFLOAT;

    // Update for sector plane heights and mobjs which intersect the ceiling.
    for(i = 0; i < map->numSectors; ++i)
    {
        Map_UpdateSkyFixForSector(map, i);
    }
}

void Map_UpdateSkyFixForSector(map_t* map, uint secIDX)
{
    boolean skyFloor, skyCeil;
    sector_t* sec;

    assert(map);
    assert(secIDX < map->numSectors);

    sec = map->sectors[secIDX];
    skyFloor = IS_SKYSURFACE(&sec->SP_floorsurface);
    skyCeil = IS_SKYSURFACE(&sec->SP_ceilsurface);

    if(!skyFloor && !skyCeil)
        return;

    if(skyCeil)
    {
        mobj_t* mo;

        // Adjust for the plane height.
        if(sec->SP_ceilvisheight > map->skyFixCeiling)
        {   // Must raise the skyfix ceiling.
            map->skyFixCeiling = sec->SP_ceilvisheight;
        }

        // Check that all the mobjs in the sector fit in.
        for(mo = sec->mobjList; mo; mo = mo->sNext)
        {
            float extent = mo->pos[VZ] + mo->height;

            if(extent > map->skyFixCeiling)
            {   // Must raise the skyfix ceiling.
                map->skyFixCeiling = extent;
            }
        }
    }

    if(skyFloor)
    {
        // Adjust for the plane height.
        if(sec->SP_floorvisheight < map->skyFixFloor)
        {   // Must lower the skyfix floor.
            map->skyFixFloor = sec->SP_floorvisheight;
        }
    }

    // Update for middle textures on two sided linedefs which intersect the
    // floor and/or ceiling of their front and/or back sectors.
    if(sec->lineDefs)
    {
        linedef_t** linePtr = sec->lineDefs;

        while(*linePtr)
        {
            linedef_t* li = *linePtr;

            // Must be twosided.
            if(LINE_FRONTSIDE(li) && LINE_BACKSIDE(li))
            {
                sidedef_t* si = LINE_FRONTSECTOR(li) == sec?
                    LINE_FRONTSIDE(li) : LINE_BACKSIDE(li);

                if(si->SW_middlematerial)
                {
                    if(skyCeil)
                    {
                        float               top =
                            sec->SP_ceilvisheight +
                                si->SW_middlevisoffset[VY];

                        if(top > map->skyFixCeiling)
                        {   // Must raise the skyfix ceiling.
                            map->skyFixCeiling = top;
                        }
                    }

                    if(skyFloor)
                    {
                        float               bottom =
                            sec->SP_floorvisheight +
                                si->SW_middlevisoffset[VY] -
                                    si->SW_middlematerial->height;

                        if(bottom < map->skyFixFloor)
                        {   // Must lower the skyfix floor.
                            map->skyFixFloor = bottom;
                        }
                    }
                }
            }
            *linePtr++;
        }
    }
}

vertex_t* Map_CreateVertex(map_t* map, float x, float y)
{
    assert(map);
    {
    vertex_t* vtx;
    if(!map->editActive)
        return NULL;
    vtx = HalfEdgeDS_CreateVertex(map->_halfEdgeDS);
    vtx->pos[VX] = x;
    vtx->pos[VY] = y;
    return vtx;
    }
}

linedef_t* Map_CreateLineDef(map_t* map, vertex_t* vtx1, vertex_t* vtx2,
                             sidedef_t* front, sidedef_t* back)
{
    linedef_t* l;

    assert(map);

    if(!map->editActive)
        return NULL;

    l = createLineDef(map);

    l->buildData.v[0] = vtx1;
    l->buildData.v[1] = vtx2;

    ((mvertex_t*) l->buildData.v[0]->data)->refCount++;
    ((mvertex_t*) l->buildData.v[1]->data)->refCount++;

    l->dX = vtx2->pos[VX] - vtx1->pos[VX];
    l->dY = vtx2->pos[VY] - vtx1->pos[VY];
    l->length = P_AccurateDistance(l->dX, l->dY);

    l->angle =
        bamsAtan2((int) (l->buildData.v[1]->pos[VY] - l->buildData.v[0]->pos[VY]),
                  (int) (l->buildData.v[1]->pos[VX] - l->buildData.v[0]->pos[VX])) << FRACBITS;

    if(l->dX == 0)
        l->slopeType = ST_VERTICAL;
    else if(l->dY == 0)
        l->slopeType = ST_HORIZONTAL;
    else
    {
        if(l->dY / l->dX > 0)
            l->slopeType = ST_POSITIVE;
        else
            l->slopeType = ST_NEGATIVE;
    }

    if(l->buildData.v[0]->pos[VX] < l->buildData.v[1]->pos[VX])
    {
        l->bBox[BOXLEFT]   = l->buildData.v[0]->pos[VX];
        l->bBox[BOXRIGHT]  = l->buildData.v[1]->pos[VX];
    }
    else
    {
        l->bBox[BOXLEFT]   = l->buildData.v[1]->pos[VX];
        l->bBox[BOXRIGHT]  = l->buildData.v[0]->pos[VX];
    }

    if(l->buildData.v[0]->pos[VY] < l->buildData.v[1]->pos[VY])
    {
        l->bBox[BOXBOTTOM] = l->buildData.v[0]->pos[VY];
        l->bBox[BOXTOP]    = l->buildData.v[1]->pos[VY];
    }
    else
    {
        l->bBox[BOXBOTTOM] = l->buildData.v[0]->pos[VY];
        l->bBox[BOXTOP]    = l->buildData.v[1]->pos[VY];
    }

    // Remember the number of unique references.
    if(front)
    {
        front->lineDef = l;
        front->buildData.refCount++;
    }

    if(back)
    {
        back->lineDef = l;
        back->buildData.refCount++;
    }

    l->buildData.sideDefs[FRONT] = front;
    l->buildData.sideDefs[BACK] = back;

    l->inFlags = 0;

    // Determine the default linedef flags.
    l->flags = 0;
    if(!front || !back)
        l->flags |= DDLF_BLOCKING;

    return l;
}

sidedef_t* Map_CreateSideDef(map_t* map, sector_t* sector, short flags, material_t* topMaterial,
                             float topOffsetX, float topOffsetY, float topRed, float topGreen,
                             float topBlue, material_t* middleMaterial, float middleOffsetX,
                             float middleOffsetY, float middleRed, float middleGreen, float middleBlue,
                             float middleAlpha, material_t* bottomMaterial, float bottomOffsetX,
                             float bottomOffsetY, float bottomRed, float bottomGreen, float bottomBlue)
{
    sidedef_t* s = NULL;

    assert(map);

    if(map->editActive)
    {
        s = createSideDef(map);
        s->flags = flags;
        s->sector = sector;

        Surface_SetMaterial(&s->SW_topsurface, topMaterial, false);
        Surface_SetMaterialOffsetXY(&s->SW_topsurface, topOffsetX, topOffsetY);
        Surface_SetColorRGBA(&s->SW_topsurface, topRed, topGreen, topBlue, 1);

        Surface_SetMaterial(&s->SW_middlesurface, middleMaterial, false);
        Surface_SetMaterialOffsetXY(&s->SW_middlesurface, middleOffsetX, middleOffsetY);
        Surface_SetColorRGBA(&s->SW_middlesurface, middleRed, middleGreen, middleBlue, middleAlpha);

        Surface_SetMaterial(&s->SW_bottomsurface, bottomMaterial, false);
        Surface_SetMaterialOffsetXY(&s->SW_bottomsurface, bottomOffsetX, bottomOffsetY);
        Surface_SetColorRGBA(&s->SW_bottomsurface, bottomRed, bottomGreen, bottomBlue, 1);
    }

    return s;
}

sector_t* Map_CreateSector(map_t* map, float lightLevel, float red, float green, float blue)
{
    sector_t* s;

    assert(map);

    if(!map->editActive)
        return NULL;

    s = createSector(map);

    s->planeCount = 0;
    s->planes = NULL;
    s->rgb[CR] = MINMAX_OF(0, red, 1);
    s->rgb[CG] = MINMAX_OF(0, green, 1);
    s->rgb[CB] = MINMAX_OF(0, blue, 1);
    s->lightLevel = MINMAX_OF(0, lightLevel, 1);

    return s;
}

plane_t* Map_CreatePlane(map_t* map, float height, material_t* material,
                         float matOffsetX, float matOffsetY, float r, float g,
                         float b, float a, float normalX, float normalY,
                         float normalZ)
{
    plane_t* pln;

    assert(map);

    if(!map->editActive)
        return NULL;

    pln = P_CreatePlane(map);

    pln->height = pln->visHeight = pln->oldHeight[0] =
        pln->oldHeight[1] = height;
    pln->visHeightDelta = 0;
    pln->speed = 0;
    pln->target = 0;

    pln->glowRGB[CR] = pln->glowRGB[CG] = pln->glowRGB[CB] = 1;
    pln->glow = 0;

    Surface_SetMaterial(&pln->surface, material? ((objectrecord_t*) material)->obj : NULL, false);
    Surface_SetColorRGBA(&pln->surface, r, g, b, a);
    Surface_SetBlendMode(&pln->surface, BM_NORMAL);
    Surface_SetMaterialOffsetXY(&pln->surface, matOffsetX, matOffsetY);

    pln->PS_normal[VX] = normalX;
    pln->PS_normal[VY] = normalY;
    pln->PS_normal[VZ] = normalZ;
    /*if(pln->PS_normal[VZ] < 0)
        pln->type = PLN_CEILING;
    else
        pln->type = PLN_FLOOR;*/
    M_Normalize(pln->PS_normal);

    pln->buildData.index = P_CreateObjectRecord(DMU_PLANE, pln) - 1;
    map->planes = Z_Realloc(map->planes, ++map->numPlanes * sizeof(plane_t*), PU_STATIC);
    map->planes[map->numPlanes-1] = pln;
    return pln;
}

polyobj_t* Map_CreatePolyobj(map_t* map, objectrecordid_t* lines, uint lineCount, int tag,
                             int sequenceType, float anchorX, float anchorY)
{
    polyobj_t* po;
    uint i;

    assert(map);

    if(!map->editActive)
        return NULL;

    po = createPolyobj(map);

    po->idx = map->numPolyObjs - 1; // 0-based index.
    po->lineDefs = Z_Calloc(sizeof(linedef_t*) * (lineCount+1), PU_STATIC, 0);
    for(i = 0; i < lineCount; ++i)
    {
        linedef_t* line = map->lineDefs[lines[i] - 1];

        // This line is part of a polyobj.
        line->inFlags |= LF_POLYOBJ;

        po->lineDefs[i] = line;
    }
    po->lineDefs[i] = NULL;
    po->numLineDefs = lineCount;

    po->tag = tag;
    po->seqType = sequenceType;
    po->pos[VX] = anchorX;
    po->pos[VY] = anchorY;

    return po;
}

static void findDominantLightSources(map_t* map)
{
#define DOMINANT_SIZE   1000

    uint i;

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sec = map->sectors[i];

        if(!sec->lineDefCount)
            continue;

        // Is this sector large enough to be a dominant light source?
        if(sec->lightSource == NULL && sec->planeCount > 0 &&
           sec->bBox[BOXRIGHT] - sec->bBox[BOXLEFT]   > DOMINANT_SIZE &&
           sec->bBox[BOXTOP]   - sec->bBox[BOXBOTTOM] > DOMINANT_SIZE)
        {
            if(R_SectorContainsSkySurfaces(sec))
            {
                uint k;

                // All sectors touching this one will be affected.
                for(k = 0; k < sec->lineDefCount; ++k)
                {
                    linedef_t* lin = sec->lineDefs[k];
                    sector_t* other;

                    other = LINE_FRONTSECTOR(lin);
                    if(other == sec)
                    {
                        if(LINE_BACKSIDE(lin))
                            other = LINE_BACKSECTOR(lin);
                    }

                    if(other && other != sec)
                        other->lightSource = sec;
                }
            }
        }
    }

#undef DOMINANT_SIZE
}

static boolean PIT_LinkObjToSubsector(uint subsectorIdx, void* data)
{
    linkobjtosubsectorparams_t* params = (linkobjtosubsectorparams_t*) data;
    objcontact_t* con = allocObjContact();

    con->obj = params->obj;

    // Link the contact list for this face.
    linkContactToSubsector(params->map, subsectorIdx, params->type, con);

    return true; // Continue iteration.
}

void Map_AddSubsectorContact(map_t* map, uint subsectorIdx, objcontacttype_t type,
                             void* obj)
{
    linkobjtosubsectorparams_t params;

    assert(map);
    assert(subsectorIdx < map->numSubsectors);
    assert(type >= OCT_FIRST && type < NUM_OBJCONTACT_TYPES);
    assert(obj);

    params.map = map;
    params.obj = obj;
    params.type = type;

    PIT_LinkObjToSubsector(subsectorIdx, &params);
}

boolean Map_IterateSubsectorContacts(map_t* map, uint subsectorIdx, objcontacttype_t type,
                                     boolean (*callback) (void*, void*),
                                     void* data)
{
    objcontact_t* con;

    assert(map);
    assert(subsectorIdx < map->numSubsectors);
    assert(callback);

    con = map->_subsectorContacts[subsectorIdx].head[type];
    while(con)
    {
        if(!callback(con->obj, data))
            return false;

        con = con->next;
    }

    return true;
}

/**
 * Begin the process of loading a new map.
 *
 * @param levelId       Identifier of the map to be loaded (eg "E1M1").
 *
 * @return              @c true, if the map was loaded successfully.
 */
boolean Map_Load(map_t* map)
{
    assert(map);

    Con_Message("Map_Load: \"%s\"\n", map->mapID);

    // It would be very cool if map loading happened in another
    // thread. That way we could be keeping ourselves busy while
    // the intermission is played...

    // We could even try to divide a HUB up into zones, so that
    // when a player enters a zone we could begin loading the map(s)
    // reachable through exits in that zone (providing they have
    // enough free memory of course) so that transitions are
    // (potentially) seamless :-)

    if(isServer)
    {
        int i;
        // Whenever the map changes, remote players must tell us when
        // they're ready to begin receiving frames.
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t* plr = &ddPlayers[i];

            if(!(plr->shared.flags & DDPF_LOCAL) && clients[i].connected)
            {
#ifdef _DEBUG
                Con_Printf("Cl%i NOT READY ANY MORE!\n", i);
#endif
                clients[i].ready = false;
            }
        }
    }

    MPE_SetEditMap(map);
    if(!DAM_TryMapConversion(map->mapID))
        return false;

    if(1)//(map = DAM_LoadMap(mapID)))
    {
        ded_sky_t* skyDef = NULL;
        ded_mapinfo_t* mapInfo;

        // Initialize The Logical Sound Manager.
        S_MapChange();

        // Call the game's setup routines.
        if(gx.SetupForMapData)
        {
            gx.SetupForMapData(DMU_LINEDEF, map->numLineDefs);
            gx.SetupForMapData(DMU_SIDEDEF, map->numSideDefs);
            gx.SetupForMapData(DMU_SECTOR, map->numSectors);
        }

        {
        uint starttime = Sys_GetRealTime();

        // Must be called before any mobjs are spawned.
        initLinks(map);

        // How much time did we spend?
        VERBOSE(Con_Message
                ("initLinks: Allocating line link rings. Done in %.2f seconds.\n",
                 (Sys_GetRealTime() - starttime) / 1000.0f));
        }

        findDominantLightSources(map);

        buildMobjBlockmap(map);
        buildSubsectorBlockmap(map);
        buildParticleBlockmap(map);
        buildLumobjBlockmap(map);

        initSubsectorContacts(map);

        // See what mapinfo says about this map.
        mapInfo = Def_GetMapInfo(map->mapID);
        if(!mapInfo)
            mapInfo = Def_GetMapInfo("*");

        if(mapInfo)
        {
            skyDef = Def_GetSky(mapInfo->skyID);
            if(!skyDef)
                skyDef = &mapInfo->sky;
        }

        R_SetupSky(theSky, skyDef);

        // Setup accordingly.
        if(mapInfo)
        {
            map->globalGravity = mapInfo->gravity;
            map->ambientLightLevel = mapInfo->ambient * 255;
        }
        else
        {
            // No map info found, so set some basic stuff.
            map->globalGravity = 1.0f;
            map->ambientLightLevel = 0;
        }

        R_InitSectorShadows(map);

        {
        uint startTime = Sys_GetRealTime();

        Map_InitSkyFix(map);

        // How much time did we spend?
        VERBOSE(Con_Message("  InitialSkyFix: Done in %.2f seconds.\n",
                            (Sys_GetRealTime() - startTime) / 1000.0f));
        }

        // Init the thinker lists (public and private).
        Thinkers_Init(map->_thinkers, 0x1 | 0x2);

        // Tell shadow bias to initialize the bias light sources.
        SB_InitForMap(map);

        Cl_Reset();
        RL_DeleteLists();
        R_CalcLightModRange(NULL);

        // Invalidate old cmds and init player values.
        {
        int i;
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t* plr = &ddPlayers[i];

            if(isServer && plr->shared.inGame)
                clients[i].runTime = SECONDS_TO_TICKS(gameTime);

            plr->extraLight = plr->targetExtraLight = 0;
            plr->extraLightCounter = 0;
        }
        }

        // Make sure that the next frame doesn't use a filtered viewer.
        R_ResetViewer();

        // Texture animations should begin from their first step.
        Materials_RewindAnimationGroups();

        LO_InitForMap(map); // Lumobj management.
        VL_InitForMap(); // Converted vlights (from lumobjs) management.

        initGenerators(map);

        // Initialize the lighting grid.
        LightGrid_Init(map);

        return true;
    }

    return false;
}

boolean Map_MobjsBoxIterator(map_t* map, const float box[4],
                             boolean (*func) (mobj_t*, void*), void* data)
{
    assert(map);
    {
    vec2_t bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return Map_MobjsBoxIteratorv(map, bounds, func, data);
    }
}

boolean Map_MobjsBoxIteratorv(map_t* map, const arvec2_t box,
                              boolean (*func) (mobj_t*, void*), void* data)
{
    assert(map);
    {
    uint blockBox[4];
    MobjBlockmap_BoxToBlocks(map->_mobjBlockmap, blockBox, box);
    return MobjBlockmap_BoxIterate(map->_mobjBlockmap, blockBox, func, data);
    }
}

static boolean linesBoxIteratorv(map_t* map, const arvec2_t box,
                                 boolean (*func) (linedef_t*, void*),
                                 void* data, boolean retObjRecord)
{
    uint blockBox[4];
    LineDefBlockmap_BoxToBlocks(map->_lineDefBlockmap, blockBox, box);
    return LineDefBlockmap_BoxIterate(map->_lineDefBlockmap, blockBox, func, data, retObjRecord);
}

boolean Map_LineDefsBoxIteratorv(map_t* map, const arvec2_t box,
                                 boolean (*func) (linedef_t*, void*), void* data,
                                 boolean retObjRecord)
{
    assert(map);
    return linesBoxIteratorv(map, box, func, data, retObjRecord);
}

/**
 * @return              @c false, if the iterator func returns @c false.
 */
boolean Map_SubsectorsBoxIterator(map_t* map, const float box[4], sector_t* sector,
                                  boolean (*func) (subsector_t*, void*),
                                  void* parm, boolean retObjRecord)
{
    assert(map);
    {
    vec2_t bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return Map_SubsectorsBoxIteratorv(map, bounds, sector, func, parm, retObjRecord);
    }
}

/**
 * Same as the fixed-point version of this routine, but the bounding box
 * is specified using an vec2_t array (see m_vector.c).
 */
boolean Map_SubsectorsBoxIteratorv(map_t* map, const arvec2_t box, sector_t* sector,
                                   boolean (*func) (subsector_t*, void*),
                                   void* data, boolean retObjRecord)
{
    assert(map);
    {
    static int localValidCount = 0;
    uint blockBox[4];

    // This is only used here.
    localValidCount++;

    // Blockcoords to check.
    SubsectorBlockmap_BoxToBlocks(map->_subsectorBlockmap, blockBox, box);
    return SubsectorBlockmap_BoxIterate(map->_subsectorBlockmap, blockBox, sector,
                                       box, localValidCount, func, data,
                                       retObjRecord);
    }
}

gameobjrecords_t* Map_GameObjectRecords(map_t* map)
{
    assert(map);
    return map->_gameObjectRecords;
}

/**
 * Destroy the given game map obj database.
 */
void Map_DestroyGameObjectRecords(map_t* map)
{
    assert(map);
    P_DestroyGameObjectRecords(map->_gameObjectRecords);
    map->_gameObjectRecords = NULL;
}

/**
 * @note Part of the Doomsday public API.
 */
uint P_NumObjectRecords(int typeIdentifier)
{
    return GameObjRecords_Num(Map_GameObjectRecords(P_CurrentMap()), typeIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
byte P_GetObjectRecordByte(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return GameObjRecords_GetByte(Map_GameObjectRecords(P_CurrentMap()), typeIdentifier, elmIdx, propIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
short P_GetObjectRecordShort(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return GameObjRecords_GetShort(Map_GameObjectRecords(P_CurrentMap()), typeIdentifier, elmIdx, propIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
int P_GetObjectRecordInt(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return GameObjRecords_GetInt(Map_GameObjectRecords(P_CurrentMap()), typeIdentifier, elmIdx, propIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
fixed_t P_GetObjectRecordFixed(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return GameObjRecords_GetFixed(Map_GameObjectRecords(P_CurrentMap()), typeIdentifier, elmIdx, propIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
angle_t P_GetObjectRecordAngle(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return GameObjRecords_GetAngle(Map_GameObjectRecords(P_CurrentMap()), typeIdentifier, elmIdx, propIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
float P_GetObjectRecordFloat(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return GameObjRecords_GetFloat(Map_GameObjectRecords(P_CurrentMap()), typeIdentifier, elmIdx, propIdentifier);
}

/**
 * Part of the Doomsday public API.
 */
void DD_InitThinkers(void)
{
    // @todo Map should be specified by the caller.
    Thinkers_Init(Map_Thinkers(P_CurrentMap()), ITF_PUBLIC); // Init the public thinker lists.
}

static int runThinker(void* p, void* context)
{
    thinker_t* th = (thinker_t*) p;

    // Thinker cannot think when in stasis.
    if(!th->inStasis)
    {
        if(th->function == (think_t) -1)
        {
            // Time to remove it.
            Thinkers_Remove(Map_Thinkers(Thinker_Map(th)), th);
            Thinker_SetMap(th, NULL);

            if(th->id)
            {   // Its a mobj.
                P_MobjRecycle((mobj_t*) th);
            }
            else
            {
                Z_Free(th);
            }
        }
        else if(th->function)
        {
            th->function(th);
        }
    }

    return true; // Continue iteration.
}

/**
 * Part of the Doomsday public API.
 */
void DD_RunThinkers(void)
{
    // @todo map should be specified by the caller
    Map_IterateThinkers(P_CurrentMap(), NULL, ITF_PUBLIC | ITF_PRIVATE,
                        runThinker, NULL);
}

/**
 * Part of the Doomsday public API.
 */
int DD_IterateThinkers(think_t func, int (*callback) (void* p, void* ctx),
                       void* context)
{
    // @todo map should be specified by the caller
    return Map_IterateThinkers(P_CurrentMap(), func, ITF_PUBLIC, callback, context);
}

/**
 * Adds a new thinker to the thinker lists.
 * Part of the Doomsday public API.
 */
void DD_ThinkerAdd(thinker_t* th)
{
    // @todo map should be specified by the caller
    Map_AddThinker(P_CurrentMap(), th, true); // This is a public thinker.
}

/**
 * Removes a thinker from the thinker lists.
 * Part of the Doomsday public API.
 */
void DD_ThinkerRemove(thinker_t* th)
{
    Map_RemoveThinker(Thinker_Map(th), th);
}

/**
 * Change the 'in stasis' state of a thinker (stop it from thinking).
 *
 * @param th            The thinker to change.
 * @param on            @c true, put into stasis.
 */
void DD_ThinkerSetStasis(thinker_t* th, boolean on)
{
    Map_ThinkerSetStasis(Thinker_Map(th), th, on);
}
