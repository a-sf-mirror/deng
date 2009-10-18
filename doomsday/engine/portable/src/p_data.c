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
 * p_data.c: Playsim Data Structures, Macros and Constants
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_dam.h"
#include "de_edit.h"
#include "de_defs.h"

#include "rend_bias.h"
#include "m_bams.h"

#include <math.h>
#include <stddef.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

nodepile_t* mobjNodes = NULL, *lineNodes = NULL; // All kinds of wacky links.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

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
    if(map->_polyobjBlockmap)
        P_DestroyPolyobjBlockmap(map->_polyobjBlockmap);
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
        MobjBlockmap_Remove(map->_mobjBlockmap, mo);
        MobjBlockmap_Insert(map->_mobjBlockmap, mo);
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
    if(MobjBlockmap_Remove(map->_mobjBlockmap, mo))
        links |= DDLINK_BLOCKMAP;
    if(!P_UnlinkFromLines(mo))
        links |= DDLINK_NOLINE;

    return links;
}

void Map_LinkPolyobj(gamemap_t* map, polyobj_t* po)
{
    if(!map)
        return;
    if(!po)
        return;

    PolyobjBlockmap_Insert(map->_polyobjBlockmap, po);
}

void Map_UnlinkPolyobj(gamemap_t* map, polyobj_t* po)
{
    if(!map)
        return;
    if(!po)
        return;

    PolyobjBlockmap_Remove(map->_polyobjBlockmap, po);
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

        LineDefBlockmap_Insert(blockmap, lineDef);
    }

    map->_lineDefBlockmap = blockmap;

#undef BLKMARGIN
#undef MAPBLOCKUNITS
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

polyobjblockmap_t* Map_PolyobjBlockmap(gamemap_t* map)
{
    return map->_polyobjBlockmap;
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
        //Map_BuildPolyobjBlockmap(map);
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

boolean Map_PolyobjsBoxIterator(gamemap_t* map, const float box[4],
                                boolean (*func) (struct polyobj_s*, void*),
                                void* data)
{
    vec2_t bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return Map_PolyobjsBoxIteratorv(map, bounds, func, data);
}

/**
 * The validCount flags are used to avoid checking polys that are marked in
 * multiple mapblocks, so increment validCount before the first call, then
 * make one or more calls to it.
 */
boolean Map_PolyobjsBoxIteratorv(gamemap_t* map, const arvec2_t box,
                                 boolean (*func) (struct polyobj_s*, void*),
                                 void* data)
{
    uint blockBox[4];

    if(!map)
        return true;
    if(!map->_polyobjBlockmap)
        return true;

    // Blockcoords to check.
    PolyobjBlockmap_BoxToBlocks(map->_polyobjBlockmap, blockBox, box);
    return PolyobjBlockmap_BoxIterate(map->_polyobjBlockmap, blockBox, func, data);
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

boolean Map_PolyobjLineDefsBoxIterator(gamemap_t* map, const float box[4],
                                    boolean (*func) (linedef_t*, void*),
                                    void* data, boolean retObjRecord)
{
    vec2_t bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return Map_PolyobjLineDefsBoxIteratorv(map, bounds, func, data, retObjRecord);
}

boolean Map_PolyobjLineDefsBoxIteratorv(gamemap_t* map, const arvec2_t box,
                                     boolean (*func) (linedef_t*, void*),
                                     void* data, boolean retObjRecord)
{
    uint blockBox[4];

    if(!map)
        return true;
    if(!map->_polyobjBlockmap)
        return true;

    PolyobjBlockmap_BoxToBlocks(map->_polyobjBlockmap, blockBox, box);
    return P_BoxIterateLineDefsOfPolyobjs(map->_polyobjBlockmap, blockBox, func, data, retObjRecord);
}

static boolean allLinesBoxIterator(gamemap_t* map, const arvec2_t box,
                                   boolean (*func) (linedef_t*, void*),
                                   void* data, boolean retObjRecord)
{
    if(!Map_PolyobjLineDefsBoxIteratorv(map, box, func, data, retObjRecord))
        return false;

    return linesBoxIteratorv(map, box, func, data, retObjRecord);
}

/**
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to LineDefBlockmap_Iterate, then make one or more calls to it.
 */
boolean Map_AllLineDefsBoxIterator(gamemap_t* map, const float box[4],
                                boolean (*func) (linedef_t*, void*),
                                void* data, boolean retObjRecord)
{
    vec2_t bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return allLinesBoxIterator(map, bounds, func, data, retObjRecord);
}

/**
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to LineDefBlockmap_Iterate, then make one or more calls to it.
 */
boolean Map_AllLineDefsBoxIteratorv(gamemap_t* map, const arvec2_t box,
                                 boolean (*func) (linedef_t*, void*),
                                 void* data, boolean retObjRecord)
{
    return allLinesBoxIterator(map, box, func, data, retObjRecord);
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
