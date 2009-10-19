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

#include "s_environ.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

nodepile_t* mobjNodes = NULL, *lineNodes = NULL; // All kinds of wacky links.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static vertex_t* rootVtx; // Used when sorting vertex line owners.

// CODE --------------------------------------------------------------------

/**
 * @note Only releases memory for the data structure itself, any objects linked
 * to the component parts of the data structure will remain (therefore this is
 * the caller's responsibility).
 */
void P_DestroyHalfEdgeDS(halfedgeds_t* halfEdgeDS)
{
    if(halfEdgeDS->faces)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->numFaces; ++i)
        {
            face_t* face = halfEdgeDS->faces[i];
            Z_Free(face);
        }

        Z_Free(halfEdgeDS->faces);
    }
    halfEdgeDS->faces = NULL;
    halfEdgeDS->numFaces = 0;

    if(halfEdgeDS->hEdges)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->numHEdges; ++i)
        {
            hedge_t* hEdge = halfEdgeDS->hEdges[i];
            Z_Free(hEdge);
        }

        Z_Free(halfEdgeDS->hEdges);
    }
    halfEdgeDS->hEdges = NULL;
    halfEdgeDS->numHEdges = 0;

    if(halfEdgeDS->vertices)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->numVertices; ++i)
        {
            vertex_t* vertex = halfEdgeDS->vertices[i];
            Z_Free(vertex);
        }

        Z_Free(halfEdgeDS->vertices);
    }
    halfEdgeDS->vertices = NULL;
    halfEdgeDS->numVertices = 0;
}

gamemap_t* P_CreateMap(const char* mapID)
{
    gamemap_t* map = Z_Calloc(sizeof(gamemap_t), PU_STATIC, 0);

    dd_snprintf(map->mapID, 9, "%s", mapID);
    map->editActive = true;

    return map;
}

static void destroySubsectors(halfedgeds_t* halfEdgeDS)
{
    if(halfEdgeDS->faces)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->numFaces; ++i)
        {
            face_t* face = halfEdgeDS->faces[i];
            subsector_t* subsector = (subsector_t*) face->data;

            if(subsector)
            {
                /*shadowlink_t* slink;
                while((slink = subsector->shadows))
                {
                    subsector->shadows = slink->next;
                    Z_Free(slink);
                }*/
                
                Z_Free(subsector);
            }
        }
    }
}

static void destroySegs(halfedgeds_t* halfEdgeDS)
{
    if(halfEdgeDS->hEdges)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->numHEdges; ++i)
        {
            hedge_t* hEdge = halfEdgeDS->hEdges[i];
            seg_t* seg = hEdge->data;

            if(seg)
            {
               Z_Free(seg);
            }
        }
    }
}

static void destroyVertexInfo(halfedgeds_t* halfEdgeDS)
{
    if(halfEdgeDS->vertices)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->numVertices; ++i)
        {
            vertex_t* vertex = halfEdgeDS->vertices[i];
            mvertex_t* mvertex = vertex->data;

            if(mvertex)
            {
               Z_Free(mvertex);
            }
        }
    }
}

static void destroyHalfEdgeDS(halfedgeds_t* halfEdgeDS)
{
    destroySubsectors(halfEdgeDS);
    destroySegs(halfEdgeDS);
    destroyVertexInfo(halfEdgeDS);

    P_DestroyHalfEdgeDS(halfEdgeDS);
}

void P_DestroyMap(gamemap_t* map)
{
    biassurface_t* bsuf;

    if(!map)
        return;

    DL_DestroyDynlights(&map->dlights.linkList);

    /**
     * @todo handle vertexillum allocation/destruction along with the surfaces,
     * utilizing global storage.
     */
    for(bsuf = map->bias.surfaces; bsuf; bsuf = bsuf->next)
    {
        if(bsuf->illum)
            Z_Free(bsuf->illum);
    }

    SB_DestroySurfaces(map);

    PlaneList_Empty(&map->watchedPlaneList);
    SurfaceList_Empty(&map->movingSurfaceList);
    SurfaceList_Empty(&map->decoratedSurfaceList);

    if(map->_mobjBlockmap)
        P_DestroyMobjBlockmap(map->_mobjBlockmap);
    if(map->_lineDefBlockmap)
        P_DestroyLineDefBlockmap(map->_lineDefBlockmap);
    if(map->_subsectorBlockmap)
        P_DestroySubsectorBlockmap(map->_subsectorBlockmap);

    if(map->sideDefs)
    {
        uint i;

        for(i = 0; i < map->numSideDefs; ++i)
        {
            sidedef_t* sideDef = map->sideDefs[i];
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

    if(map->polyObjs)
    {
        uint i;
        for(i = 0; i < map->numPolyObjs; ++i)
        {
            polyobj_t* po = map->polyObjs[i];
            uint j;

            for(j = 0; j < po->numLineDefs; ++j)
            {
                linedef_t* lineDef = po->lineDefs[j];
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
            uint j;

            if(sector->planes)
            {
                for(j = 0; j < sector->planeCount; ++j)
                {
                    Z_Free(sector->planes[j]);
                }
                Z_Free(sector->planes);
            }

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

    if(map->subsectors)
        Z_Free(map->subsectors);
    map->subsectors = NULL;
    map->numSubsectors = 0;

    if(map->segs)
        Z_Free(map->segs);
    map->segs = NULL;
    map->numSegs = 0;

    destroyHalfEdgeDS(Map_HalfEdgeDS(map));

    if(map->nodes)
    {
        uint i;

        for(i = 0; i < map->numNodes; ++i)
        {
            node_t* node = map->nodes[i];
            Z_Free(node);
        }
        Z_Free(map->nodes);
    }
    map->nodes = NULL;
    map->numNodes = 0;

    Z_Free(map);
}

void Map_LinkMobj(gamemap_t* map, mobj_t* mo, byte flags)
{
    subsector_t* subsector;

    if(!map)
        return;
    if(!mo)
        return;

    subsector = R_PointInSubSector(mo->pos[VX], mo->pos[VY]);

    // Link into the sector.
    mo->subsector = (subsector_t*) P_ObjectRecord(DMU_SUBSECTOR, subsector);

    if(flags & DDLINK_SECTOR)
    {
        // Unlink from the current sector, if any.
        if(mo->sPrev)
            P_UnlinkFromSector(mo);

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
        // Unlink from any existing lines.
        P_UnlinkFromLines(mo);

        // Link to all contacted lines.
        P_LinkToLines(mo);
    }

    // If this is a player - perform addtional tests to see if they have
    // entered or exited the void.
    if(mo->dPlayer)
    {
        ddplayer_t* player = mo->dPlayer;

        player->inVoid = true;
        if(R_IsPointInSector2(player->mo->pos[VX], player->mo->pos[VY],
                              subsector->sector) &&
           (player->mo->pos[VZ] < subsector->sector->SP_ceilvisheight  - 4 &&
            player->mo->pos[VZ] > subsector->sector->SP_floorvisheight + 4))
            player->inVoid = false;
    }
}

int Map_UnlinkMobj(gamemap_t* map, mobj_t* mo)
{
    int links = 0;

    if(P_UnlinkFromSector(mo))
        links |= DDLINK_SECTOR;
    if(MobjBlockmap_Unlink(map->_mobjBlockmap, mo))
        links |= DDLINK_BLOCKMAP;
    if(!P_UnlinkFromLines(mo))
        links |= DDLINK_NOLINE;

    return links;
}

void Map_BuildLineDefBlockmap(gamemap_t* map)
{
#define BLKMARGIN               (8) // size guardband around map
#define MAPBLOCKUNITS           128

    uint i;
    uint bMapWidth, bMapHeight; // Blockmap dimensions.
    vec2_t blockSize; // Size of the blocks.
    uint numBlocks; // Number of cells = nrows*ncols.
    vec2_t bounds[2], point, dims;
    vertex_t* vtx;
    linedefblockmap_t* blockmap;

    // Scan for map limits, which the blockmap must enclose.
    for(i = 0; i < Map_HalfEdgeDS(map)->numVertices; ++i)
    {
        vtx = Map_HalfEdgeDS(map)->vertices[i];

        V2_Set(point, vtx->pos[VX], vtx->pos[VY]);
        if(!i)
            V2_InitBox(bounds, point);
        else
            V2_AddToBox(bounds, point);
    }

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

        LineDefBlockmap_Link(blockmap, lineDef);
    }

    map->_lineDefBlockmap = blockmap;

#undef BLKMARGIN
#undef MAPBLOCKUNITS
}

void Map_BuildSubsectorBlockmap(gamemap_t* map)
{
#define BLKMARGIN       8
#define BLOCK_WIDTH     128
#define BLOCK_HEIGHT    128

    uint i, subMapWidth, subMapHeight;
    vec2_t bounds[2], blockSize, dims;
    subsectorblockmap_t* blockmap;

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

void Map_BuildMobjBlockmap(gamemap_t* map)
{
#define BLKMARGIN               (8) // size guardband around map
#define MAPBLOCKUNITS           128

    uint i, bMapWidth, bMapHeight; // Blockmap dimensions.
    vec2_t blockSize; // Size of the blocks.
    vec2_t bounds[2], dims;

    // Scan for map limits, which the blockmap must enclose.
    for(i = 0; i < Map_HalfEdgeDS(map)->numVertices; ++i)
    {
        vertex_t* vtx = Map_HalfEdgeDS(map)->vertices[i];
        vec2_t point;

        V2_Set(point, vtx->pos[VX], vtx->pos[VY]);
        if(!i)
            V2_InitBox(bounds, point);
        else
            V2_AddToBox(bounds, point);
    }

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
    size_t i;
    vertex_t** hits;

    hits = M_Malloc(halfEdgeDS->numVertices * sizeof(vertex_t*));

    // Sort array of ptrs.
    for(i = 0; i < halfEdgeDS->numVertices; ++i)
        hits[i] = halfEdgeDS->vertices[i];
    qsort(hits, halfEdgeDS->numVertices, sizeof(vertex_t*), vertexCompare);

    // Now mark them off.
    for(i = 0; i < halfEdgeDS->numVertices - 1; ++i)
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

static void findEquivalentVertexes(gamemap_t* map)
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

static void pruneLineDefs(gamemap_t* map)
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
    uint i, newNum, unused = 0;

    // Scan all vertices.
    for(i = 0, newNum = 0; i < halfEdgeDS->numVertices; ++i)
    {
        vertex_t* v = halfEdgeDS->vertices[i];

        if(((mvertex_t*) v->data)->refCount < 0)
            Con_Error("Vertex %d ref_count is %d", i, ((mvertex_t*) v->data)->refCount);

        if(((mvertex_t*) v->data)->refCount == 0)
        {
            if(((mvertex_t*) v->data)->equiv == NULL)
                unused++;

            Z_Free(v);
            continue;
        }

        ((mvertex_t*) v->data)->index = newNum + 1;
        halfEdgeDS->vertices[newNum++] = v;
    }

    if(newNum < halfEdgeDS->numVertices)
    {
        int dupNum = halfEdgeDS->numVertices - newNum - unused;

        if(unused > 0)
            Con_Message("  Pruned %d unused vertices.\n", unused);

        if(dupNum > 0)
            Con_Message("  Pruned %d duplicate vertices\n", dupNum);

        halfEdgeDS->numVertices = newNum;
    }
}

static void pruneUnusedSideDefs(gamemap_t* map)
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

static void pruneUnusedSectors(gamemap_t* map)
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
 * \note Order here is critical!
 */
static void pruneUnusedObjects(gamemap_t* map, int flags)
{
    /**
     * \fixme Pruning cannot be done as game map data object properties
     * are currently indexed by their original indices as determined by the
     * position in the map data. The same problem occurs within ACS scripts
     * and XG line/sector references.
     */
    findEquivalentVertexes(map);

/*    if(flags & PRUNE_LINEDEFS)
        pruneLineDefs(map);*/

    if(flags & PRUNE_VERTEXES)
        pruneVertices(&map->_halfEdgeDS);

/*    if(flags & PRUNE_SIDEDEFS)
        pruneUnusedSideDefs(map);

    if(flags & PRUNE_SECTORS)
        pruneUnusedSectors(map);*/
}

static void hardenSectorSubsectorList(gamemap_t* map, uint secIDX)
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
static void buildSectorSubsectorLists(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numSectors; ++i)
    {
        hardenSectorSubsectorList(map, i);
    }
}

static void buildSectorLineLists(gamemap_t* map)
{
    typedef struct linelink_s {
        linedef_t*      line;
        struct linelink_s *next;
    } linelink_t;

    uint i, j;
    zblockset_t* lineLinksBlockSet;
    linelink_t** sectorLineLinks;
    uint* sectorLinkLinkCounts;

    // build line tables for each sector.
    lineLinksBlockSet = Z_BlockCreate(sizeof(linelink_t), 512, PU_STATIC);
    sectorLineLinks = M_Calloc(sizeof(linelink_t*) * map->numSectors);
    sectorLinkLinkCounts = M_Calloc(sizeof(uint) * map->numSectors);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* li = map->lineDefs[i];
        uint secIDX;
        linelink_t* link;

        if(LINE_FRONTSIDE(li))
        {
            link = Z_BlockNewElement(lineLinksBlockSet);

            secIDX = P_ObjectRecord(DMU_SECTOR, LINE_FRONTSECTOR(li))->id - 1;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            sectorLinkLinkCounts[secIDX]++;
            LINE_FRONTSECTOR(li)->lineDefCount++;
        }

        if(LINE_BACKSIDE(li) && LINE_BACKSECTOR(li) != LINE_FRONTSECTOR(li))
        {
            link = Z_BlockNewElement(lineLinksBlockSet);

            secIDX = P_ObjectRecord(DMU_SECTOR, LINE_BACKSECTOR(li))->id - 1;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            sectorLinkLinkCounts[secIDX]++;
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

            sector->lineDefs =
                Z_Malloc((sectorLinkLinkCounts[i] + 1) * sizeof(linedef_t*), PU_STATIC, 0);

            j = 0;
            while(link)
            {
                sector->lineDefs[j++] = link->line;
                link = link->next;
            }

            sector->lineDefs[j] = NULL; // terminate.
            sector->lineDefCount = j;
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
    M_Free(sectorLinkLinkCounts);
}

static void finishSectors2(gamemap_t* map)
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

        // Set the z height of the sector sound origin.
        sec->soundOrg.pos[VZ] =
            (sec->SP_ceilheight - sec->SP_floorheight) / 2;

        // Set the position of the sound origin for all plane sound origins.
        // Set target heights for all planes.
        for(k = 0; k < sec->planeCount; ++k)
        {
            sec->planes[k]->soundOrg.pos[VX] = sec->soundOrg.pos[VX];
            sec->planes[k]->soundOrg.pos[VY] = sec->soundOrg.pos[VY];
            sec->planes[k]->soundOrg.pos[VZ] = sec->planes[k]->height;

            sec->planes[k]->target = sec->planes[k]->height;
        }
    }
}

static void updateMapBounds(gamemap_t* map)
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
static void finishLineDefs2(gamemap_t* map)
{
    uint i;

    VERBOSE2(Con_Message("Finalizing LineDefs...\n"));

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

static void prepareSubsectors(gamemap_t* map)
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
        lineowner_t* base, *owner;

        owner = base = vInfo->lineOwners;
        do
        {
            if(owner->LO_prev->LO_next != owner || owner->LO_next->LO_prev != owner)
               Con_Error("Invalid line owner link ring!");

            owner = owner->LO_next;
        } while(owner != base);
    }
}
#endif

/**
 * Generates the line owner rings for each vertex. Each ring includes all
 * the lines which the vertex belongs to sorted by angle, (the rings are
 * arranged in clockwise order, east = 0).
 */
static void buildVertexOwnerRings(gamemap_t* map, vertexinfo_t* vertexInfo)
{
    lineowner_t* lineOwners, *allocator;
    halfedgeds_t* halfEdgeDS = Map_HalfEdgeDS(map);
    uint i;

    // We know how many vertex line owners we need (numLineDefs * 2).
    lineOwners = Z_Malloc(sizeof(lineowner_t) * map->numLineDefs * 2, PU_MAP, 0);
    allocator = lineOwners;

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
    for(i = 0; i < halfEdgeDS->numVertices; ++i)
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

static void addLineDefsToDMU(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* lineDef = map->lineDefs[i];

        P_CreateObjectRecord(DMU_LINEDEF, lineDef);
    }
}

static void finishSideDefs(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numSideDefs; ++i)
    {
        sidedef_t* sideDef = map->sideDefs[i];

        sideDef->sector = map->sectors[sideDef->sector->buildData.index - 1];
        sideDef->SW_bottomsurface.owner = sideDef;
        sideDef->SW_middlesurface.owner = sideDef;
        sideDef->SW_topsurface.owner = sideDef;
        sideDef->SW_bottomsurface.visOffset[0] = sideDef->SW_bottomsurface.offset[0];
        sideDef->SW_bottomsurface.visOffset[1] = sideDef->SW_bottomsurface.offset[1];
        sideDef->SW_middlesurface.visOffset[0] = sideDef->SW_middlesurface.offset[0];
        sideDef->SW_middlesurface.visOffset[1] = sideDef->SW_middlesurface.offset[1];
        sideDef->SW_topsurface.visOffset[0] = sideDef->SW_topsurface.offset[0];
        sideDef->SW_topsurface.visOffset[1] = sideDef->SW_topsurface.offset[1];

        P_CreateObjectRecord(DMU_SIDEDEF, sideDef);
    }
}

static void addSectorsToDMU(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sector = map->sectors[i];

        P_CreateObjectRecord(DMU_SECTOR, sector);
    }
}

static void hardenPlanes(gamemap_t* map)
{
    uint i, j;

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sector = map->sectors[i];

        for(j = 0; j < sector->planeCount; ++j)
        {
            plane_t* plane = sector->planes[j]; //R_NewPlaneForSector(destS);

            plane->height = plane->oldHeight[0] = plane->oldHeight[1] =
                plane->visHeight = plane->height;
            plane->visHeightDelta = 0;
            plane->sector = sector;

            P_CreateObjectRecord(DMU_PLANE, plane);
        }
    }
}

static void finishPolyobjs(gamemap_t* map)
{
    uint i;

    for(i = 0; i < map->numPolyObjs; ++i)
    {
        polyobj_t* po = map->polyObjs[i];
        uint j;

        for(j = 0; j < po->numLineDefs; ++j)
        {
            linedef_t* lineDef = po->lineDefs[po->lineDefs[j]->buildData.index - 1];
            hedge_t* hEdge;

            hEdge = Z_Calloc(sizeof(hedge_t), PU_MAP, 0);
            hEdge->face = NULL;
            lineDef->hEdges[0] = lineDef->hEdges[1] = hEdge;

            po->lineDefs[j] = (linedef_t*) P_ObjectRecord(DMU_LINEDEF, lineDef);
        }
        po->lineDefs[j] = NULL;

        po->originalPts = Z_Malloc(po->numLineDefs * sizeof(fvertex_t), PU_STATIC, 0);
        po->prevPts = Z_Malloc(po->numLineDefs * sizeof(fvertex_t), PU_STATIC, 0);

        // Temporary: Create a seg for each line of this polyobj.
        po->numSegs = po->numLineDefs;
        po->segs = Z_Calloc(sizeof(poseg_t) * (po->numSegs+1), PU_STATIC, 0);

        for(j = 0; j < po->numSegs; ++j)
        {
            linedef_t* lineDef = ((objectrecord_t*) po->lineDefs[j])->obj;
            poseg_t* seg = &po->segs[j];

            lineDef->hEdges[0]->data = seg;

            seg->lineDef = lineDef;
            seg->sideDef = map->sideDefs[lineDef->buildData.sideDefs[FRONT]->buildData.index - 1];
        }
    }
}

/**
 * @algorithm Cast a line horizontally or vertically and see what we hit.
 * @todo Construct the lineDef blockmap first so it may be used for this
 */
static void testForWindowEffect(gamemap_t* map, linedef_t* l)
{
// Smallest distance between two points before being considered equal.
#define DIST_EPSILON        (1.0 / 128.0)

    uint i;
    double mX, mY, dX, dY;
    boolean castHoriz;
    double backDist = DDMAXFLOAT;
    sector_t* backOpen = NULL;
    double frontDist = DDMAXFLOAT;
    sector_t* frontOpen = NULL;
    linedef_t* frontLine = NULL, *backLine = NULL;

    mX = (l->buildData.v[0]->pos[VX] + l->buildData.v[1]->pos[VX]) / 2.0;
    mY = (l->buildData.v[0]->pos[VY] + l->buildData.v[1]->pos[VY]) / 2.0;

    dX = l->buildData.v[1]->pos[VX] - l->buildData.v[0]->pos[VX];
    dY = l->buildData.v[1]->pos[VY] - l->buildData.v[0]->pos[VY];

    castHoriz = (fabs(dX) < fabs(dY)? true : false);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* n = map->lineDefs[i];
        double dist;
        boolean isFront;
        sidedef_t* hitSide;
        double dX2, dY2;

        if(n == l || (n->buildData.sideDefs[FRONT] && n->buildData.sideDefs[BACK] &&
            n->buildData.sideDefs[FRONT]->sector == n->buildData.sideDefs[BACK]->sector) /*|| n->buildData.overlap ||
           (n->buildData.mlFlags & MLF_ZEROLENGTH)*/)
            continue;

        dX2 = n->buildData.v[1]->pos[VX] - n->buildData.v[0]->pos[VX];
        dY2 = n->buildData.v[1]->pos[VY] - n->buildData.v[0]->pos[VY];

        if(castHoriz)
        {   // Horizontal.
            if(fabs(dY2) < DIST_EPSILON)
                continue;

            if((MAX_OF(n->buildData.v[0]->pos[VY], n->buildData.v[1]->pos[VY]) < mY - DIST_EPSILON) ||
               (MIN_OF(n->buildData.v[0]->pos[VY], n->buildData.v[1]->pos[VY]) > mY + DIST_EPSILON))
                continue;

            dist = (n->buildData.v[0]->pos[VX] + (mY - n->buildData.v[0]->pos[VY]) * dX2 / dY2) - mX;

            isFront = (((dY > 0) != (dist > 0)) ? true : false);

            dist = fabs(dist);
            if(dist < DIST_EPSILON)
                continue; // Too close (overlapping lines ?)

            hitSide = n->buildData.sideDefs[(dY > 0) ^ (dY2 > 0) ^ !isFront];
        }
        else
        {   // Vertical.
            if(fabs(dX2) < DIST_EPSILON)
                continue;

            if((MAX_OF(n->buildData.v[0]->pos[VX], n->buildData.v[1]->pos[VX]) < mX - DIST_EPSILON) ||
               (MIN_OF(n->buildData.v[0]->pos[VX], n->buildData.v[1]->pos[VX]) > mX + DIST_EPSILON))
                continue;

            dist = (n->buildData.v[0]->pos[VY] + (mX - n->buildData.v[0]->pos[VX]) * dY2 / dX2) - mY;

            isFront = (((dX > 0) == (dist > 0)) ? true : false);

            dist = fabs(dist);

            hitSide = n->buildData.sideDefs[(dX > 0) ^ (dX2 > 0) ^ !isFront];
        }

        if(dist < DIST_EPSILON) // Too close (overlapping lines ?)
            continue;

        if(isFront)
        {
            if(dist < frontDist)
            {
                frontDist = dist;
                if(hitSide)
                    frontOpen = hitSide->sector;
                else
                    frontOpen = NULL;

                frontLine = n;
            }
        }
        else
        {
            if(dist < backDist)
            {
                backDist = dist;
                if(hitSide)
                    backOpen = hitSide->sector;
                else
                    backOpen = NULL;

                backLine = n;
            }
        }
    }

/*#if _DEBUG
Con_Message("back line: %d  back dist: %1.1f  back_open: %s\n",
            (backLine? backLine->buildData.index : -1), backDist,
            (backOpen? "OPEN" : "CLOSED"));
Con_Message("front line: %d  front dist: %1.1f  front_open: %s\n",
            (frontLine? frontLine->buildData.index : -1), frontDist,
            (frontOpen? "OPEN" : "CLOSED"));
#endif*/

    if(backOpen && frontOpen && l->buildData.sideDefs[FRONT]->sector == backOpen)
    {
        Con_Message("LineDef #%d seems to be a One-Sided Window "
                    "(back faces sector #%d).\n", l->buildData.index - 1,
                    backOpen->buildData.index - 1);

        l->buildData.windowEffect = frontOpen;
    }

#undef DIST_EPSILON
}

static void countVertexLineOwners(vertexinfo_t* vInfo, uint* oneSided, uint* twoSided)
{
    lineowner_t* base = vInfo->lineOwners;

    if(base)
    {
        lineowner_t* owner = base;

        do
        {
            if(!owner->lineDef->buildData.sideDefs[FRONT] ||
               !owner->lineDef->buildData.sideDefs[BACK])
                (*oneSided)++;
            else
                (*twoSided)++;

            owner = owner->LO_next;
        } while(owner != base);
    }
}

/**
 * \note Algorithm:
 * Scan the linedef list looking for possible candidates, checking for an
 * odd number of one-sided linedefs connected to a single vertex.
 * This idea courtesy of Graham Jackson.
 */
static void detectOnesidedWindows(gamemap_t* map, vertexinfo_t* vertexInfo)
{
    uint i, oneSiders, twoSiders;

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* l = map->lineDefs[i];

        if((l->buildData.sideDefs[FRONT] && l->buildData.sideDefs[BACK]) ||
           !l->buildData.sideDefs[FRONT] /*||
           (l->buildData.mlFlags & MLF_ZEROLENGTH) ||
           l->buildData.overlap*/)
            continue;

        oneSiders = twoSiders = 0;
        countVertexLineOwners(&vertexInfo[((mvertex_t*)l->buildData.v[0]->data)->index - 1],
                              &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*#if _DEBUG
Con_Message("FUNNY LINE %d : start vertex %d has odd number of one-siders\n",
            i, l->buildData.v[0]->index);
#endif*/
            testForWindowEffect(map, l);
            continue;
        }

        oneSiders = twoSiders = 0;
        countVertexLineOwners(&vertexInfo[((mvertex_t*)l->buildData.v[1]->data)->index - 1],
                              &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*#if _DEBUG
Con_Message("FUNNY LINE %d : end vertex %d has odd number of one-siders\n",
            i, l->buildData.v[1]->index));
#endif*/
            testForWindowEffect(map, l);
        }
    }
}

static void findMapLimits(gamemap_t* src, int* bbox)
{
    uint i;

    M_ClearBox(bbox);

    for(i = 0; i < src->numLineDefs; ++i)
    {
        linedef_t* l = src->lineDefs[i];
        double x1 = l->buildData.v[0]->pos[VX];
        double y1 = l->buildData.v[0]->pos[VY];
        double x2 = l->buildData.v[1]->pos[VX];
        double y2 = l->buildData.v[1]->pos[VY];
        int lX = (int) floor(MIN_OF(x1, x2));
        int lY = (int) floor(MIN_OF(y1, y2));
        int hX = (int) ceil(MAX_OF(x1, x2));
        int hY = (int) ceil(MAX_OF(y1, y2));

        M_AddToBox(bbox, lX, lY);
        M_AddToBox(bbox, hX, hY);
    }
}

/**
 * Initially create all half-edges, one for each side of a linedef.
 *
 * @return              The list of created half-edges.
 */
static superblock_t* createInitialHEdges(gamemap_t* map)
{
    uint startTime = Sys_GetRealTime();

    uint i;
    int bw, bh;
    hedge_t* back, *front;
    superblock_t* block;
    int mapBounds[4];

    // Find maximal vertexes.
    findMapLimits(map, mapBounds);

    VERBOSE(Con_Message("Map goes from (%d,%d) to (%d,%d)\n",
                        mapBounds[BOXLEFT], mapBounds[BOXBOTTOM],
                        mapBounds[BOXRIGHT], mapBounds[BOXTOP]));

    block = BSP_SuperBlockCreate();

    block->bbox[BOXLEFT]   = mapBounds[BOXLEFT]   - (mapBounds[BOXLEFT]   & 0x7);
    block->bbox[BOXBOTTOM] = mapBounds[BOXBOTTOM] - (mapBounds[BOXBOTTOM] & 0x7);
    bw = ((mapBounds[BOXRIGHT] - block->bbox[BOXLEFT])   / 128) + 1;
    bh = ((mapBounds[BOXTOP]   - block->bbox[BOXBOTTOM]) / 128) + 1;

    block->bbox[BOXRIGHT] = block->bbox[BOXLEFT]   + 128 * M_CeilPow2(bw);
    block->bbox[BOXTOP]   = block->bbox[BOXBOTTOM] + 128 * M_CeilPow2(bh);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* line = map->lineDefs[i];

        front = back = NULL;

        // Ignore polyobj lines.
        if(/*line->buildData.overlap || */
           (line->inFlags & LF_POLYOBJ))
            continue;

        // Check for Humungously long lines.
        if(ABS(line->buildData.v[0]->pos[VX] - line->buildData.v[1]->pos[VX]) >= 10000 ||
           ABS(line->buildData.v[0]->pos[VY] - line->buildData.v[1]->pos[VY]) >= 10000)
        {
            if(3000 >=
               M_Length(line->buildData.v[0]->pos[VX] - line->buildData.v[1]->pos[VX],
                        line->buildData.v[0]->pos[VY] - line->buildData.v[1]->pos[VY]))
            {
                Con_Message("LineDef #%d is VERY long, it may cause problems\n",
                            line->buildData.index);
            }
        }

        front = HEdge_Create(line, line, line->buildData.v[0],
                             line->buildData.sideDefs[FRONT]->sector, false);

        // Handle the 'One-Sided Window' trick.
        if(!line->buildData.sideDefs[BACK] && line->buildData.windowEffect)
        {
            back = HEdge_Create(((bsp_hedgeinfo_t*) front->data)->lineDef,
                                 line, line->buildData.v[1],
                                 line->buildData.windowEffect, true);
        }
        else
        {
            back = HEdge_Create(line, line, line->buildData.v[1],
                                line->buildData.sideDefs[BACK]? line->buildData.sideDefs[BACK]->sector : NULL, true);
        }

        // Half-edges always maintain a one-to-one relationship
        // with their twins, so if one gets split, the other
        // must be split also.
        back->twin = front;
        front->twin = back;

        BSP_UpdateHEdgeInfo(front);
        BSP_UpdateHEdgeInfo(back);

        BSP_AddHEdgeToSuperBlock(block, front);
        if((line->buildData.sideDefs[BACK] && line->buildData.sideDefs[BACK]->sector) ||
           (!line->buildData.sideDefs[BACK] && line->buildData.windowEffect))
            BSP_AddHEdgeToSuperBlock(block, back);

        // \todo edge tips should be created when half-edges are created.
        {
        double x1 = line->buildData.v[0]->pos[VX];
        double y1 = line->buildData.v[0]->pos[VY];
        double x2 = line->buildData.v[1]->pos[VX];
        double y2 = line->buildData.v[1]->pos[VY];

        BSP_CreateVertexEdgeTip(line->buildData.v[0], x2 - x1, y2 - y1, back, front);
        BSP_CreateVertexEdgeTip(line->buildData.v[1], x1 - x2, y1 - y2, front, back);
        }
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("createInitialHEdges: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

    return block;
}

static boolean C_DECL freeBSPData(binarytree_t *tree, void *data)
{
    void* bspData = BinaryTree_GetData(tree);

    if(bspData)
    {
        if(BinaryTree_IsLeaf(tree))
            BSPLeaf_Destroy(bspData);
        else
            M_Free(bspData);
    }

    BinaryTree_SetData(tree, NULL);

    return true; // Continue iteration.
}

/**
 * Build the BSP for the given map.
 *
 * @param map           The map to build the BSP for.
 * @return              @c true, if completed successfully.
 */
static boolean buildBSP(gamemap_t* map)
{
    boolean builtOK;
    uint startTime;
    superblock_t* hEdgeList;
    binarytree_t* rootNode;

    if(verbose >= 1)
    {
        Con_Message("Map_BuildBSP: Processing map using tunable "
                    "factor of %d...\n", bspFactor);
    }

    // It begins...
    startTime = Sys_GetRealTime();

    BSP_InitSuperBlockAllocator();
    BSP_InitIntersectionAllocator();

    // Create initial half-edges.
    hEdgeList = createInitialHEdges(map);

    // Build the BSP.
    {
    uint buildStartTime = Sys_GetRealTime();
    cutlist_t* cutList;

    cutList = BSP_CutListCreate();

    // Recursively create nodes.
    rootNode = NULL;
    builtOK = BuildNodes(hEdgeList, &rootNode, 0, cutList);

    // The cutlist data is no longer needed.
    BSP_CutListDestroy(cutList);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("BuildNodes: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - buildStartTime) / 1000.0f));
    }

    BSP_SuperBlockDestroy(hEdgeList);

    if(builtOK)
    {   // Success!
        // Wind the BSP tree and save to the map.
        ClockwiseBspTree(rootNode);
        SaveMap(map, rootNode);

        Con_Message("Map_BuildBSP: Built %d Nodes, %d Faces, %d HEdges, %d Vertexes\n",
                    map->numNodes, map->_halfEdgeDS.numFaces, map->_halfEdgeDS.numHEdges,
                    map->_halfEdgeDS.numVertices);

        if(rootNode && !BinaryTree_IsLeaf(rootNode))
        {
            long rHeight, lHeight;

            rHeight = (long)
                BinaryTree_GetHeight(BinaryTree_GetChild(rootNode, RIGHT));
            lHeight = (long)
                BinaryTree_GetHeight(BinaryTree_GetChild(rootNode, LEFT));

            Con_Message("  Balance %+ld (l%ld - r%ld).\n", lHeight - rHeight,
                        lHeight, rHeight);
        }
    }

    // We are finished with the BSP build data.
    if(rootNode)
    {
        BinaryTree_PostOrder(rootNode, freeBSPData, NULL);
        BinaryTree_Destroy(rootNode);
    }
    rootNode = NULL;

    // Free temporary storage.
    BSP_ShutdownIntersectionAllocator();
    BSP_ShutdownSuperBlockAllocator();

    // How much time did we spend?
    VERBOSE(Con_Message("  Done in %.2f seconds.\n",
                        (Sys_GetRealTime() - startTime) / 1000.0f));

    return builtOK;
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
void MPE_DetectOverlappingLines(gamemap_t* map)
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

void Map_EditEnd(gamemap_t* map)
{
    uint i, numVertices;
    vertexinfo_t* vertexInfo;

    if(!map)
        return;
    if(!map->editActive)
        return;

    /**
     * Perform cleanup on the loaded map data, removing duplicate vertexes,
     * pruning unused sectors etc, etc...
     */
    detectDuplicateVertices(&map->_halfEdgeDS);
    pruneUnusedObjects(map, PRUNE_ALL);

    numVertices = Map_HalfEdgeDS(map)->numVertices;
    vertexInfo = M_Calloc(sizeof(vertexinfo_t) * numVertices);

    buildVertexOwnerRings(map, vertexInfo);

#if _DEBUG
checkVertexOwnerRings(vertexInfo, numVertices);
#endif

    detectOnesidedWindows(map, vertexInfo);

    /**
     * Harden most of the map data so that we can construct some of the more
     * intricate data structures early on (and thus make use of them during
     * the BSP generation).
     *
     * \todo I'm sure this can be reworked further so that we destroy as we
     * go and reduce the current working memory surcharge.
     */
    addSectorsToDMU(map);
    finishSideDefs(map);
    addLineDefsToDMU(map);
    finishPolyobjs(map);

    /**
     * Build a LineDef blockmap for this map.
     */
    {
    uint startTime = Sys_GetRealTime();

    Map_BuildLineDefBlockmap(map);

    // How much time did we spend?
    VERBOSE(Con_Message("Map_BuildLineDefBlockmap: Done in %.2f seconds.\n",
            (Sys_GetRealTime() - startTime) / 1000.0f))
    }

    /*builtOK =*/ buildBSP(map);

    for(i = 0; i < map->numPolyObjs; ++i)
    {
        polyobj_t* po = map->polyObjs[i];
        uint j;

        for(j = 0; j < po->numLineDefs; ++j)
        {
            linedef_t* line = ((objectrecord_t*) po->lineDefs[j])->obj;

            line->L_v1 = map->_halfEdgeDS.vertices[
                ((mvertex_t*) line->buildData.v[0]->data)->index - 1];
            line->L_v2 = map->_halfEdgeDS.vertices[
                ((mvertex_t*) line->buildData.v[1]->data)->index - 1];

            // The original Pts are based off the anchor Pt, and are unique
            // to each linedef.
            po->originalPts[j].pos[VX] = line->L_v1->pos[VX] - po->pos[VX];
            po->originalPts[j].pos[VY] = line->L_v1->pos[VY] - po->pos[VY];
        }
    }

    buildSectorSubsectorLists(map);
    hardenPlanes(map);
    buildSectorLineLists(map);
    finishLineDefs2(map);
    finishSectors2(map);
    updateMapBounds(map);

    S_DetermineSubSecsAffectingSectorReverb(map);
    prepareSubsectors(map);

    // Pass on the vertex lineowner info.
    // @todo will soon be unnecessary once Map_BuildBSP is linking the half-edges
    // into rings around the vertices.
    for(i = 0; i < numVertices; ++i)
    {
        vertex_t* vertex = map->_halfEdgeDS.vertices[i];
        vertexinfo_t* vInfo = &vertexInfo[i];

        ((mvertex_t*) vertex->data)->lineOwners = vInfo->lineOwners;
        ((mvertex_t*) vertex->data)->numLineOwners = vInfo->numLineOwners;
    }

    M_Free(vertexInfo);

    map->editActive = false;
}

/**
 * This ID is the name of the lump tag that marks the beginning of map
 * data, e.g. "MAP03" or "E2M8".
 */
const char* Map_ID(gamemap_t* map)
{
    if(!map)
        return NULL;

    return map->mapID;
}

/**
 * @return              The 'unique' identifier of the map.
 */
const char* Map_UniqueName(gamemap_t* map)
{
    if(!map)
        return NULL;

    return map->uniqueID;
}

void Map_Bounds(gamemap_t* map, float* min, float* max)
{
    min[VX] = map->bBox[BOXLEFT];
    min[VY] = map->bBox[BOXBOTTOM];

    max[VX] = map->bBox[BOXRIGHT];
    max[VY] = map->bBox[BOXTOP];
}

/**
 * Get the ambient light level of the specified map.
 */
int Map_AmbientLightLevel(gamemap_t* map)
{
    if(!map)
        return 0;

    return map->ambientLightLevel;
}

halfedgeds_t* Map_HalfEdgeDS(gamemap_t* map)
{
    return &map->_halfEdgeDS;
}

mobjblockmap_t* Map_MobjBlockmap(gamemap_t* map)
{
    return map->_mobjBlockmap;
}

linedefblockmap_t* Map_LineDefBlockmap(gamemap_t* map)
{
    return map->_lineDefBlockmap;
}

subsectorblockmap_t* Map_SubsectorBlockmap(gamemap_t* map)
{
    return map->_subsectorBlockmap;
}

vertex_t* Map_CreateVertex(gamemap_t* map, float x, float y)
{
    vertex_t* vertex;

    if(!map)
        return NULL;
    if(!map->editActive)
        return NULL;

    vertex = HalfEdgeDS_CreateVertex(Map_HalfEdgeDS(map));
    vertex->pos[VX] = x;
    vertex->pos[VY] = y;

    return vertex;
}

static linedef_t* createLineDef(gamemap_t* map)
{
    linedef_t* line = Z_Calloc(sizeof(*line), PU_STATIC, 0);

    map->lineDefs =
        Z_Realloc(map->lineDefs, sizeof(line) * (++map->numLineDefs + 1), PU_STATIC);
    map->lineDefs[map->numLineDefs-1] = line;
    map->lineDefs[map->numLineDefs] = NULL;

    line->buildData.index = map->numLineDefs; // 1-based index, 0 = NIL.
    return line;
}

linedef_t* Map_CreateLineDef(gamemap_t* map, vertex_t* vtx1, vertex_t* vtx2,
                             sidedef_t* front, sidedef_t* back)
{
    linedef_t* l;
    
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

static sidedef_t* createSideDef(gamemap_t* map)
{
    sidedef_t* side = Z_Calloc(sizeof(*side), PU_STATIC, 0);

    map->sideDefs = Z_Realloc(map->sideDefs, sizeof(side) * (++map->numSideDefs + 1), PU_STATIC);
    map->sideDefs[map->numSideDefs-1] = side;
    map->sideDefs[map->numSideDefs] = NULL;

    side->buildData.index = map->numSideDefs; // 1-based index, 0 = NIL.
    side->SW_bottomsurface.owner = (void*) side;
    return side;
}

sidedef_t* Map_CreateSideDef(gamemap_t* map, sector_t* sector, short flags, material_t* topMaterial,
                             float topOffsetX, float topOffsetY, float topRed, float topGreen,
                             float topBlue, material_t* middleMaterial, float middleOffsetX,
                             float middleOffsetY, float middleRed, float middleGreen, float middleBlue,
                             float middleAlpha, material_t* bottomMaterial, float bottomOffsetX,
                             float bottomOffsetY, float bottomRed, float bottomGreen, float bottomBlue)
{
    sidedef_t* s = NULL;

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

static sector_t* createSector(gamemap_t* map)
{
    sector_t* sec = Z_Calloc(sizeof(*sec), PU_STATIC, 0);

    map->sectors = Z_Realloc(map->sectors, sizeof(sec) * (++map->numSectors + 1), PU_STATIC);
    map->sectors[map->numSectors-1] = sec;
    map->sectors[map->numSectors] = NULL;

    sec->buildData.index = map->numSectors; // 1-based index, 0 = NIL.
    return sec;
}

sector_t* Map_CreateSector(gamemap_t* map, float lightLevel, float red, float green, float blue)
{
    sector_t* s;

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

static plane_t* createPlane(gamemap_t* map)
{
    return Z_Calloc(sizeof(plane_t), PU_STATIC, 0);
}

void Map_CreatePlane(gamemap_t* map, sector_t* sector, float height, material_t* material,
                     float matOffsetX, float matOffsetY, float r, float g, float b, float a,
                     float normalX, float normalY, float normalZ)
{
    uint i;
    plane_t** newList, *pln;

    if(!map->editActive)
        return;

    pln = createPlane(map);

    pln->height = height;
    Surface_SetMaterial(&pln->surface, material? ((objectrecord_t*) material)->obj : NULL, false);
    Surface_SetColorRGBA(&pln->surface, r, g, b, a);
    Surface_SetMaterialOffsetXY(&pln->surface, matOffsetX, matOffsetY);
    pln->PS_normal[VX] = normalX;
    pln->PS_normal[VY] = normalY;
    pln->PS_normal[VZ] = normalZ;
    if(pln->PS_normal[VZ] < 0)
        pln->type = PLN_CEILING;
    else
        pln->type = PLN_FLOOR;
    M_Normalize(pln->PS_normal);
    pln->sector = sector;

    newList = Z_Malloc(sizeof(plane_t*) * (++sector->planeCount + 1), PU_STATIC, 0);
    for(i = 0; i < sector->planeCount - 1; ++i)
    {
        newList[i] = sector->planes[i];
    }
    newList[i++] = pln;
    newList[i] = NULL; // Terminate.

    if(sector->planes)
        Z_Free(sector->planes);
    sector->planes = newList;
}

static polyobj_t* createPolyobj(gamemap_t* map)
{
    polyobj_t* po = Z_Calloc(POLYOBJ_SIZE, PU_STATIC, 0);

    map->polyObjs = Z_Realloc(map->polyObjs, sizeof(po) * (++map->numPolyObjs + 1), PU_STATIC);
    map->polyObjs[map->numPolyObjs-1] = po;
    map->polyObjs[map->numPolyObjs] = NULL;

    po->buildData.index = map->numPolyObjs; // 1-based index, 0 = NIL.
    return po;
}

polyobj_t* Map_CreatePolyobj(gamemap_t* map, objectrecordid_t* lines, uint lineCount, int tag,
                             int sequenceType, float anchorX, float anchorY)
{
    polyobj_t* po;
    uint i;

    if(!map->editActive)
        return NULL;

    po = createPolyobj(map);

    po->idx = map->numPolyObjs; // 0-based index.
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

/**
 * Begin the process of loading a new map.
 * Can be accessed by the games via the public API.
 *
 * @param levelId       Identifier of the map to be loaded (eg "E1M1").
 *
 * @return              @c true, if the map was loaded successfully.
 */
boolean P_LoadMap(const char* mapID)
{
    uint i;
    gamemap_t* map = NULL;

    if(!mapID || !mapID[0])
        return false; // Yeah, ok... :P

    Con_Message("P_LoadMap: \"%s\"\n", mapID);

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

    if(DAM_TryMapConversion(mapID))
    {
        ddstring_t* s = DAM_ComposeArchiveMapFilepath(mapID);

        map = MPE_GetLastBuiltMap();

        //DAM_MapWrite(map, Str_Text(s));

        Str_Delete(s);
    }
    else
        return false;

    if(1)//(map = DAM_LoadMap(mapID)))
    {
        ded_sky_t* skyDef = NULL;
        ded_mapinfo_t* mapInfo;
        uint i;

        // Call the game's setup routines.
        if(gx.SetupForMapData)
        {
            gx.SetupForMapData(DMU_LINEDEF, map->numLineDefs);
            gx.SetupForMapData(DMU_SIDEDEF, map->numSideDefs);
            gx.SetupForMapData(DMU_SECTOR, map->numSectors);
        }

        SBE_InitForMap(map);

        // Do any initialization/error checking work we need to do.
        // Must be called before we go any further.
        P_InitUnusedMobjList();

        // Must be called before any mobjs are spawned.
        R_InitLinks(map);

        R_BuildSectorLinks(map);

        // Init other blockmaps.
        Map_BuildMobjBlockmap(map);
        Map_BuildSubsectorBlockmap(map);

        strncpy(map->mapID, mapID, 8);
        strncpy(map->uniqueID, P_GenerateUniqueMapName(mapID),
                sizeof(map->uniqueID));

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

        // \todo should be called from P_LoadMap() but R_InitMap requires the
        // currentMap to be set first.
        P_SetCurrentMap(map);

        map = P_CurrentMap();

        R_InitSectorShadows(map);

        {
        uint startTime = Sys_GetRealTime();

        R_InitSkyFix(map);

        // How much time did we spend?
        VERBOSE(Con_Message("  InitialSkyFix: Done in %.2f seconds.\n",
                            (Sys_GetRealTime() - startTime) / 1000.0f));
        }

        // Init the thinker lists (public and private).
        P_InitThinkerLists(map, 0x1 | 0x2);

        // Tell shadow bias to initialize the bias light sources.
        SB_InitForMap(map);

        Cl_Reset();
        RL_DeleteLists();
        R_CalcLightModRange(NULL);

        // Invalidate old cmds and init player values.
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t* plr = &ddPlayers[i];

            if(isServer && plr->shared.inGame)
                clients[i].runTime = SECONDS_TO_TICKS(gameTime);

            plr->extraLight = plr->targetExtraLight = 0;
            plr->extraLightCounter = 0;
        }

        // Make sure that the next frame doesn't use a filtered viewer.
        R_ResetViewer();

        // Texture animations should begin from their first step.
        Materials_RewindAnimationGroups();

        R_InitObjLinksForMap(map);
        LO_InitForMap(map); // Lumobj management.
        VL_InitForMap(); // Converted vlights (from lumobjs) management.

        // Init Particle Generator links.
        P_PtcInitForMap(map);

        // Initialize the lighting grid.
        LG_Init(map);

        return true;
    }

    return false;
}

boolean Map_MobjsBoxIterator(gamemap_t* map, const float box[4],
                             boolean (*func) (mobj_t*, void*), void* data)
{
    vec2_t bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return Map_MobjsBoxIteratorv(map, bounds, func, data);
}

boolean Map_MobjsBoxIteratorv(gamemap_t* map, const arvec2_t box,
                              boolean (*func) (mobj_t*, void*), void* data)
{
    uint blockBox[4];

    if(!map)
        return true;

    MobjBlockmap_BoxToBlocks(map->_mobjBlockmap, blockBox, box);
    return MobjBlockmap_BoxIterate(map->_mobjBlockmap, blockBox, func, data);
}

static boolean linesBoxIteratorv(gamemap_t* map, const arvec2_t box,
                                 boolean (*func) (linedef_t*, void*),
                                 void* data, boolean retObjRecord)
{
    uint blockBox[4];

    if(!map)
        return true;

    LineDefBlockmap_BoxToBlocks(map->_lineDefBlockmap, blockBox, box);
    return LineDefBlockmap_BoxIterate(map->_lineDefBlockmap, blockBox, func, data, retObjRecord);
}

boolean Map_LineDefsBoxIteratorv(gamemap_t* map, const arvec2_t box,
                                 boolean (*func) (linedef_t*, void*), void* data,
                                 boolean retObjRecord)
{
    return linesBoxIteratorv(map, box, func, data, retObjRecord);
}

/**
 * @return              @c false, if the iterator func returns @c false.
 */
boolean Map_SubsectorsBoxIterator(gamemap_t* map, const float box[4], sector_t* sector,
                                  boolean (*func) (subsector_t*, void*),
                                  void* parm, boolean retObjRecord)
{
    vec2_t bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return Map_SubsectorsBoxIteratorv(map, bounds, sector, func, parm, retObjRecord);
}

/**
 * Same as the fixed-point version of this routine, but the bounding box
 * is specified using an vec2_t array (see m_vector.c).
 */
boolean Map_SubsectorsBoxIteratorv(gamemap_t* map, const arvec2_t box, sector_t* sector,
                                   boolean (*func) (subsector_t*, void*),
                                   void* data, boolean retObjRecord)
{
    static int localValidCount = 0;
    uint blockBox[4];

    if(!map)
        return true;

    // This is only used here.
    localValidCount++;

    // Blockcoords to check.
    SubsectorBlockmap_BoxToBlocks(map->_subsectorBlockmap, blockBox, box);
    return SubsectorBlockmap_BoxIterate(map->_subsectorBlockmap, blockBox, sector,
                                       box, localValidCount, func, data,
                                       retObjRecord);
}

static valuetable_t* getDBTable(valuedb_t* db, valuetype_t type, boolean canCreate)
{
    uint i;
    valuetable_t* tbl;

    if(!db)
        return NULL;

    for(i = 0; i < db->num; ++i)
    {
        tbl = db->tables[i];
        if(tbl->type == type)
        {   // Found it!
            return tbl;
        }
    }

    if(!canCreate)
        return NULL;

    // We need to add a new value table to the db.
    db->tables = Z_Realloc(db->tables, ++db->num * sizeof(*db->tables), PU_STATIC);
    tbl = db->tables[db->num - 1] = Z_Malloc(sizeof(valuetable_t), PU_STATIC, 0);

    tbl->data = NULL;
    tbl->type = type;
    tbl->numElements = 0;

    return tbl;
}

static uint insertIntoDB(valuedb_t* db, valuetype_t type, void* data)
{
    valuetable_t* tbl = getDBTable(db, type, true);

    // Insert the new value.
    switch(type)
    {
    case DDVT_BYTE:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements, PU_STATIC);
        ((byte*) tbl->data)[tbl->numElements - 1] = *((byte*) data);
        break;

    case DDVT_SHORT:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements * sizeof(short), PU_STATIC);
        ((short*) tbl->data)[tbl->numElements - 1] = *((short*) data);
        break;

    case DDVT_INT:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements * sizeof(int), PU_STATIC);
        ((int*) tbl->data)[tbl->numElements - 1] = *((int*) data);
        break;

    case DDVT_FIXED:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements * sizeof(fixed_t), PU_STATIC);
        ((fixed_t*) tbl->data)[tbl->numElements - 1] = *((fixed_t*) data);
        break;

    case DDVT_ANGLE:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements * sizeof(angle_t), PU_STATIC);
        ((angle_t*) tbl->data)[tbl->numElements - 1] = *((angle_t*) data);
        break;

    case DDVT_FLOAT:
        tbl->data = Z_Realloc(tbl->data, ++tbl->numElements * sizeof(float), PU_STATIC);
        ((float*) tbl->data)[tbl->numElements - 1] = *((float*) data);
        break;

    default:
        Con_Error("insetIntoDB: Unknown value type %d.", type);
    }

    return tbl->numElements - 1;
}

static void* getPtrToDBElm(valuedb_t* db, valuetype_t type, uint elmIdx)
{
    valuetable_t* tbl = getDBTable(db, type, false);

    if(!tbl)
        Con_Error("getPtrToDBElm: Table for type %i not found.", (int) type);

    // Sanity check: ensure the elmIdx is in bounds.
    if(elmIdx < 0 || elmIdx >= tbl->numElements)
        Con_Error("P_GetObjectRecordByte: valueIdx out of bounds.");

    switch(tbl->type)
    {
    case DDVT_BYTE:
        return &(((byte*) tbl->data)[elmIdx]);

    case DDVT_SHORT:
        return &(((short*)tbl->data)[elmIdx]);

    case DDVT_INT:
        return &(((int*) tbl->data)[elmIdx]);

    case DDVT_FIXED:
        return &(((fixed_t*) tbl->data)[elmIdx]);

    case DDVT_ANGLE:
        return &(((angle_t*) tbl->data)[elmIdx]);

    case DDVT_FLOAT:
        return &(((float*) tbl->data)[elmIdx]);

    default:
        Con_Error("P_GetObjectRecordByte: Invalid table type %i.", tbl->type);
    }

    // Should never reach here.
    return NULL;
}

/**
 * Destroy the given game map obj database.
 */
void Map_DestroyGameObjectRecords(gamemap_t* map)
{
    gameobjectrecordset_t* records = &map->_gameObjectRecordSet;

    if(records->namespaces)
    {
        uint i;
        for(i = 0; i < records->numNamespaces; ++i)
        {
            gameobjectrecordnamespace_t* rnamespace = &records->namespaces[i];
            uint j;

            for(j = 0; j < rnamespace->numRecords; ++j)
            {
                gameobjectrecord_t* record = rnamespace->records[j];

                if(record->properties)
                    Z_Free(record->properties);

                Z_Free(record);
            }
        }

        Z_Free(records->namespaces);
    }
    records->namespaces = NULL;

    if(records->values.tables)
    {
        uint i;
        for(i = 0; i < records->values.num; ++i)
        {
            valuetable_t* tbl = records->values.tables[i];

            if(tbl->data)
                Z_Free(tbl->data);

            Z_Free(tbl);
        }

        Z_Free(records->values.tables);
    }
    records->values.tables = NULL;
    records->values.num = 0;
}

static uint numGameObjectRecords(gameobjectrecordset_t* records, int identifier)
{
    if(records)
    {
        uint i;
        for(i = 0; i < records->numNamespaces; ++i)
        {
            gameobjectrecordnamespace_t* rnamespace = &records->namespaces[i];

            if(rnamespace->def->identifier == identifier)
                return rnamespace->numRecords;
        }
    }

    return 0;
}

uint Map_NumGameObjectRecords(gamemap_t* map, int identifier)
{
    return numGameObjectRecords(&map->_gameObjectRecordSet, identifier);
}

static gameobjectrecordnamespace_t*
getGameObjectRecordNamespace(gameobjectrecordset_t* records, def_gameobject_t* def)
{
    uint i;

    for(i = 0; i < records->numNamespaces; ++i)
        if(records->namespaces[i].def == def)
            return &records->namespaces[i];

    return NULL;
}

gameobjectrecord_t* Map_GameObjectRecord(gameobjectrecordset_t* records, def_gameobject_t* def,
                                         uint elmIdx, boolean canCreate)
{
    uint i;
    gameobjectrecord_t* record;
    gameobjectrecordnamespace_t* rnamespace;

    if(!records->numNamespaces)
    {   // We haven't yet created the lists.
        records->numNamespaces = P_NumGameObjectDefs();
        records->namespaces = Z_Malloc(sizeof(*rnamespace) * records->numNamespaces, PU_STATIC, 0);
        for(i = 0; i < records->numNamespaces; ++i)
        {
            rnamespace = &records->namespaces[i];

            rnamespace->def = P_GetGameObjectDef(i);
            rnamespace->records = NULL;
            rnamespace->numRecords = 0;
        }
    }

    rnamespace = getGameObjectRecordNamespace(records, def);
    assert(rnamespace);

    // Have we already created this record?
    for(i = 0; i < rnamespace->numRecords; ++i)
    {
        record = rnamespace->records[i];
        if(record->elmIdx == elmIdx)
            return record; // Yep, return it.
    }

    if(!canCreate)
        return NULL;

    // It is a new record.
    rnamespace->records = Z_Realloc(rnamespace->records,
        ++rnamespace->numRecords * sizeof(gameobjectrecord_t*), PU_STATIC);

    record = (gameobjectrecord_t*) Z_Malloc(sizeof(*record), PU_STATIC, 0);
    record->elmIdx = elmIdx;
    record->numProperties = 0;
    record->properties = NULL;

    rnamespace->records[rnamespace->numRecords - 1] = record;

    return record;
}

void Map_UpdateGameObjectRecord(gamemap_t* map, def_gameobject_t* def,
                                uint propIdx, uint elmIdx, valuetype_t type,
                                void* data)
{
    uint i;
    gameobjectrecord_property_t* prop;
    gameobjectrecordset_t* records = &map->_gameObjectRecordSet;
    gameobjectrecord_t* record = Map_GameObjectRecord(records, def, elmIdx, true);

    if(!record)
        Con_Error("Map_UpdateGameObjectRecord: Failed creation.");

    // Check whether this is a new value or whether we are updating an
    // existing one.
    for(i = 0; i < record->numProperties; ++i)
    {
        if(record->properties[i].idx == propIdx)
        {   // We are updating.
            Con_Error("addGameMapObj: Value type change not currently supported.");
            return;
        }
    }

    // Its a new property value.
    record->properties = Z_Realloc(record->properties,
        ++record->numProperties * sizeof(*record->properties), PU_STATIC);

    prop = &record->properties[record->numProperties - 1];
    prop->idx = propIdx;
    prop->type = type;
    prop->valueIdx = insertIntoDB(&records->values, type, data);
}

static void* getValueForGameObjectRecordProperty(gamemap_t* map, int identifier, uint elmIdx,
                                                 int propIdentifier, valuetype_t* type)
{
    uint i;
    def_gameobject_t* def;
    gameobjectrecord_t* record;
    gameobjectrecordset_t* records = &map->_gameObjectRecordSet;

    if((def = P_GameObjectDef(identifier, NULL, false)) == NULL)
        Con_Error("P_GetObjectRecordByte: Invalid identifier %i.", identifier);

    if((record = Map_GameObjectRecord(records, def, elmIdx, false)) == NULL)
        Con_Error("P_GetObjectRecordByte: There is no element %i of type %s.", elmIdx,
                  def->name);

    // Find the requested property.
    for(i = 0; i < record->numProperties; ++i)
    {
        gameobjectrecord_property_t* rproperty = &record->properties[i];

        if(def->properties[rproperty->idx].identifier == propIdentifier)
        {
            void* ptr;

            if(NULL ==
               (ptr = getPtrToDBElm(&records->values, rproperty->type, rproperty->valueIdx)))
                Con_Error("P_GetObjectRecordByte: Failed db look up.");

            if(type)
                *type = rproperty->type;

            return ptr;
        }
    }

    return NULL;
}

/**
 * Handle some basic type conversions.
 */
static void setValue(void* dst, valuetype_t dstType, void* src, valuetype_t srcType)
{
    if(dstType == DDVT_FIXED)
    {
        fixed_t* d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = (*((byte*) src) << FRACBITS);
            break;
        case DDVT_INT:
            *d = (*((int*) src) << FRACBITS);
            break;
        case DDVT_FIXED:
            *d = *((fixed_t*) src);
            break;
        case DDVT_FLOAT:
            *d = FLT2FIX(*((float*) src));
            break;
        default:
            Con_Error("SetValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_FLOAT)
    {
        float* d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FIXED:
            *d = FIX2FLT(*((fixed_t*) src));
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        default:
            Con_Error("SetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_BYTE)
    {
        byte* d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_FLOAT:
            *d = (byte) *((float*) src);
            break;
        default:
            Con_Error("SetValue: DDVT_BYTE incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_INT)
    {
        int* d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        case DDVT_FIXED:
            *d = (*((fixed_t*) src) >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_SHORT)
    {
        short* d = dst;

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        case DDVT_FIXED:
            *d = (*((fixed_t*) src) >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_ANGLE)
    {
        angle_t* d = dst;

        switch(srcType)
        {
        case DDVT_ANGLE:
            *d = *((angle_t*) src);
            break;
        default:
            Con_Error("SetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else
    {
        Con_Error("SetValue: unknown value type %d.\n", dstType);
    }
}

byte Map_GameObjectRecordByte(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    byte returnVal = 0;

    assert(map);

    if((ptr = getValueForGameObjectRecordProperty(map, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_BYTE, ptr, type);

    return returnVal;
}

short Map_GameObjectRecordShort(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    short returnVal = 0;

    assert(map);

    if((ptr = getValueForGameObjectRecordProperty(map, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_SHORT, ptr, type);

    return returnVal;
}

int Map_GameObjectRecordInt(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    int returnVal = 0;

    assert(map);

    if((ptr = getValueForGameObjectRecordProperty(map, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_INT, ptr, type);

    return returnVal;
}

fixed_t Map_GameObjectRecordFixed(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    fixed_t returnVal = 0;

    assert(map);

    if((ptr = getValueForGameObjectRecordProperty(map, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_FIXED, ptr, type);

    return returnVal;
}

angle_t Map_GameObjectRecordAngle(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    angle_t returnVal = 0;

    assert(map);

    if((ptr = getValueForGameObjectRecordProperty(map, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_ANGLE, ptr, type);

    return returnVal;
}

float Map_GameObjectRecordFloat(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier)
{
    valuetype_t type;
    void* ptr;
    float returnVal = 0;

    assert(map);

    if((ptr = getValueForGameObjectRecordProperty(map, typeIdentifier, elmIdx, propIdentifier, &type)))
        setValue(&returnVal, DDVT_FLOAT, ptr, type);

    return returnVal;
}

/**
 * @note Part of the Doomsday public API.
 */
uint P_NumObjectRecords(int typeIdentifier)
{
    return Map_NumGameObjectRecords(P_CurrentMap(), typeIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
byte P_GetObjectRecordByte(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return Map_GameObjectRecordByte(P_CurrentMap(), typeIdentifier, elmIdx, propIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
short P_GetObjectRecordShort(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return Map_GameObjectRecordShort(P_CurrentMap(), typeIdentifier, elmIdx, propIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
int P_GetObjectRecordInt(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return Map_GameObjectRecordInt(P_CurrentMap(), typeIdentifier, elmIdx, propIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
fixed_t P_GetObjectRecordFixed(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return Map_GameObjectRecordFixed(P_CurrentMap(), typeIdentifier, elmIdx, propIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
angle_t P_GetObjectRecordAngle(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return Map_GameObjectRecordAngle(P_CurrentMap(), typeIdentifier, elmIdx, propIdentifier);
}

/**
 * @note Part of the Doomsday public API.
 */
float P_GetObjectRecordFloat(int typeIdentifier, uint elmIdx, int propIdentifier)
{
    return Map_GameObjectRecordFloat(P_CurrentMap(), typeIdentifier, elmIdx, propIdentifier);
}
