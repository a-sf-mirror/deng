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
 * p_bmap.c: Blockmaps
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct linkmobj_s {
    struct linkmobj_s* next;
    struct mobj_s* mobj;
} linkmobj_t;

typedef struct linklinedef_s {
    struct linklinedef_s* next;
    struct linedef_s* lineDef;
} linklinedef_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static blockmap_t* allocBlockmap(void)
{
    return Z_Calloc(sizeof(blockmap_t), PU_STATIC, 0);
}

static void freeBlockmap(blockmap_t* bmap)
{
    Z_Free(bmap);
}

static void boxToBlocks(blockmap_t* bmap, uint blockBox[4], const arvec2_t box)
{
    uint dimensions[2];
    vec2_t min, max;

    Gridmap_Dimensions(bmap->gridmap, dimensions);

    V2_Set(min, MAX_OF(bmap->aabb[0][VX], box[0][VX]),
                MAX_OF(bmap->aabb[0][VY], box[0][VY]));

    V2_Set(max, MIN_OF(bmap->aabb[1][VX], box[1][VX]),
                MIN_OF(bmap->aabb[1][VY], box[1][VY]));

    blockBox[BOXLEFT]   = MINMAX_OF(0, (min[VX] - bmap->aabb[0][VX]) / bmap->blockSize[VX], dimensions[0]);
    blockBox[BOXBOTTOM] = MINMAX_OF(0, (min[VY] - bmap->aabb[0][VY]) / bmap->blockSize[VY], dimensions[1]);

    blockBox[BOXRIGHT]  = MINMAX_OF(0, (max[VX] - bmap->aabb[0][VX]) / bmap->blockSize[VX], dimensions[0]);
    blockBox[BOXTOP]    = MINMAX_OF(0, (max[VY] - bmap->aabb[0][VY]) / bmap->blockSize[VY], dimensions[1]);
}

void MobjBlockmap_BoxToBlocks(mobjblockmap_t* blockmap, uint blockBox[4],
                              const arvec2_t box)
{
    assert(blockmap);
    assert(blockBox);
    assert(box);

    boxToBlocks(blockmap, blockBox, box);
}

void LineDefBlockmap_BoxToBlocks(linedefblockmap_t* blockmap, uint blockBox[4],
                                 const arvec2_t box)
{
    assert(blockmap);
    assert(blockBox);
    assert(box);

    boxToBlocks(blockmap, blockBox, box);
}

void SubsectorBlockmap_BoxToBlocks(subsectorblockmap_t* blockmap, uint blockBox[4],
                                   const arvec2_t box)
{
    assert(blockmap);
    assert(blockBox);
    assert(box);

    boxToBlocks(blockmap, blockBox, box);
}

void PolyobjBlockmap_BoxToBlocks(polyobjblockmap_t* blockmap, uint blockBox[4],
                                 const arvec2_t box)
{
    assert(blockmap);
    assert(blockBox);
    assert(box);

    boxToBlocks(blockmap, blockBox, box);
}

/**
 * Given a world coordinate, output the blockmap block[x, y] it resides in.
 */
boolean MobjBlockmap_Block2f(mobjblockmap_t* blockmap, uint dest[2], float x, float y)
{
    assert(blockmap);

    if(!(x < blockmap->aabb[0][VX] || x >= blockmap->aabb[1][VX] ||
         y < blockmap->aabb[0][VY] || y >= blockmap->aabb[1][VY]))
    {
        dest[VX] = (x - blockmap->aabb[0][VX]) / blockmap->blockSize[VX];
        dest[VY] = (y - blockmap->aabb[0][VY]) / blockmap->blockSize[VY];

        return true;
    }

    return false;
}

/**
 * Given a world coordinate, output the blockmap block[x, y] it resides in.
 */
boolean MobjBlockmap_Block2fv(mobjblockmap_t* blockmap, uint dest[2], const float pos[2])
{
    return MobjBlockmap_Block2f(blockmap, dest, pos[0], pos[1]);
}

/**
 * Given a world coordinate, output the blockmap block[x, y] it resides in.
 */
boolean LineDefBlockmap_Block2f(linedefblockmap_t* blockmap, uint dest[2], float x, float y)
{
    assert(blockmap);

    if(!(x < blockmap->aabb[0][VX] || x >= blockmap->aabb[1][VX] ||
         y < blockmap->aabb[0][VY] || y >= blockmap->aabb[1][VY]))
    {
        dest[VX] = (x - blockmap->aabb[0][VX]) / blockmap->blockSize[VX];
        dest[VY] = (y - blockmap->aabb[0][VY]) / blockmap->blockSize[VY];

        return true;
    }

    return false;
}

/**
 * Given a world coordinate, output the blockmap block[x, y] it resides in.
 */
boolean LineDefBlockmap_Block2fv(linedefblockmap_t* blockmap, uint dest[2], const float pos[2])
{
    return LineDefBlockmap_Block2f(blockmap, dest, pos[0], pos[1]);
}

/**
 * Given a world coordinate, output the blockmap block[x, y] it resides in.
 */
boolean PolyobjBlockmap_Block2f(polyobjblockmap_t* blockmap, uint dest[2], float x, float y)
{
    assert(blockmap);

    if(!(x < blockmap->aabb[0][VX] || x >= blockmap->aabb[1][VX] ||
         y < blockmap->aabb[0][VY] || y >= blockmap->aabb[1][VY]))
    {
        dest[VX] = (x - blockmap->aabb[0][VX]) / blockmap->blockSize[VX];
        dest[VY] = (y - blockmap->aabb[0][VY]) / blockmap->blockSize[VY];

        return true;
    }

    return false;
}

/**
 * Given a world coordinate, output the blockmap block[x, y] it resides in.
 */
boolean PolyobjBlockmap_Block2fv(polyobjblockmap_t* blockmap, uint dest[2], const float pos[2])
{
    return PolyobjBlockmap_Block2f(blockmap, dest, pos[0], pos[1]);
}

/**
 * Given a world coordinate, output the blockmap block[x, y] it resides in.
 */
boolean SubsectorBlockmap_Block2f(subsectorblockmap_t* blockmap, uint dest[2], float x, float y)
{
    assert(blockmap);

    if(!(x < blockmap->aabb[0][VX] || x >= blockmap->aabb[1][VX] ||
         y < blockmap->aabb[0][VY] || y >= blockmap->aabb[1][VY]))
    {
        dest[VX] = (x - blockmap->aabb[0][VX]) / blockmap->blockSize[VX];
        dest[VY] = (y - blockmap->aabb[0][VY]) / blockmap->blockSize[VY];

        return true;
    }

    return false;
}

/**
 * Given a world coordinate, output the blockmap block[x, y] it resides in.
 */
boolean SubsectorBlockmap_Block2fv(subsectorblockmap_t* blockmap, uint dest[2], const float pos[2])
{
    return SubsectorBlockmap_Block2f(blockmap, dest, pos[0], pos[1]);
}

static boolean freeMobjBlockData(void* data, void* context)
{
    linkmobj_t* link = (linkmobj_t*) data;

    while(link)
    {
        linkmobj_t* next = link->next;
        Z_Free(link);
        link = next;
    }

    return true; // Continue iteration.
}

mobjblockmap_t* P_CreateMobjBlockmap(const pvec2_t min, const pvec2_t max,
                                     uint width, uint height)
{
    mobjblockmap_t* blockmap;

    assert(min);
    assert(max);

    blockmap = allocBlockmap();
    V2_Copy(blockmap->aabb[0], min);
    V2_Copy(blockmap->aabb[1], max);
    V2_Set(blockmap->blockSize,
           (blockmap->aabb[1][VX] - blockmap->aabb[0][VX]) / width,
           (blockmap->aabb[1][VY] - blockmap->aabb[0][VY]) / height);

    blockmap->gridmap = M_CreateGridmap(width, height, PU_STATIC);

    return blockmap;
}

void P_DestroyMobjBlockmap(mobjblockmap_t* blockmap)
{
    assert(blockmap);

    Gridmap_Iterate(blockmap->gridmap, freeMobjBlockData, NULL);

    M_DestroyGridmap(blockmap->gridmap);
    Z_Free(blockmap);
}

linedefblockmap_t* P_CreateLineDefBlockmap(const pvec2_t min, const pvec2_t max,
                                           uint width, uint height)
{
    linedefblockmap_t* blockmap;

    assert(min);
    assert(max);

    blockmap = allocBlockmap();
    V2_Copy(blockmap->aabb[0], min);
    V2_Copy(blockmap->aabb[1], max);
    V2_Set(blockmap->blockSize,
           (blockmap->aabb[1][VX] - blockmap->aabb[0][VX]) / width,
           (blockmap->aabb[1][VY] - blockmap->aabb[0][VY]) / height);

    blockmap->gridmap = M_CreateGridmap(width, height, PU_STATIC);

    return blockmap;
}

static boolean freeLineDefBlockData(void* data, void* context)
{
    linklinedef_t* link = (linklinedef_t*) data;

    while(link)
    {
        linklinedef_t* next = link->next;
        Z_Free(link);
        link = next;
    }

    return true; // Continue iteration.
}

void P_DestroyLineDefBlockmap(linedefblockmap_t* blockmap)
{
    assert(blockmap);

    Gridmap_Iterate(blockmap->gridmap, freeLineDefBlockData, NULL);

    M_DestroyGridmap(blockmap->gridmap);
    Z_Free(blockmap);
}

polyobjblockmap_t* P_CreatePolyobjBlockmap(const pvec2_t min, const pvec2_t max,
                                           uint width, uint height)
{
    polyobjblockmap_t* blockmap;

    assert(min);
    assert(max);

    blockmap = allocBlockmap();
    V2_Copy(blockmap->aabb[0], min);
    V2_Copy(blockmap->aabb[1], max);
    V2_Set(blockmap->blockSize,
           (blockmap->aabb[1][VX] - blockmap->aabb[0][VX]) / width,
           (blockmap->aabb[1][VY] - blockmap->aabb[0][VY]) / height);

    blockmap->gridmap = M_CreateGridmap(width, height, PU_STATIC);

    return blockmap;
}

static boolean freePolyobjBlockData(void* data, void* context)
{
    linkpolyobj_t* link = (linkpolyobj_t*) data;

    while(link)
    {
        linkpolyobj_t* next = link->next;
        Z_Free(link);
        link = next;
    }

    return true; // Continue iteration.
}

void P_DestroyPolyobjBlockmap(polyobjblockmap_t* blockmap)
{
    assert(blockmap);

    Gridmap_Iterate(blockmap->gridmap, freePolyobjBlockData, NULL);

    M_DestroyGridmap(blockmap->gridmap);
    Z_Free(blockmap);
}

subsectorblockmap_t* P_CreateSubsectorBlockmap(const pvec2_t min, const pvec2_t max,
                                               uint width, uint height)
{
    subsectorblockmap_t* blockmap;

    assert(min);
    assert(max);

    blockmap = allocBlockmap();
    V2_Copy(blockmap->aabb[0], min);
    V2_Copy(blockmap->aabb[1], max);
    V2_Set(blockmap->blockSize,
           (blockmap->aabb[1][VX] - blockmap->aabb[0][VX]) / width,
           (blockmap->aabb[1][VY] - blockmap->aabb[0][VY]) / height);

    blockmap->gridmap = M_CreateGridmap(width, height, PU_STATIC);

    return blockmap;
}

static boolean freeSubsectorBlockData(void* data, void* context)
{
    /*linkmobj_t* link = (linkmobj_t*) data;

    while(link)
    {
        linkmobj_t* next = link->next;
        Z_Free(link);
        link = next;
    }*/
    {
    subsector_t** subsectors = (subsector_t**) data;
    Z_Free(subsectors);
    }

    return true; // Continue iteration.
}

void P_DestroySubsectorBlockmap(subsectorblockmap_t* blockmap)
{
    assert(blockmap);

    Gridmap_Iterate(blockmap->gridmap, freeSubsectorBlockData, NULL);

    M_DestroyGridmap(blockmap->gridmap);
    Z_Free(blockmap);
}

void SubsectorBlockmap_SetBlock(subsectorblockmap_t* blockmap, uint x, uint y,
                                subsector_t** subsectors)
{
    subsector_t** oldData;

    assert(blockmap);

    oldData = (subsector_t**) Gridmap_Block(blockmap->gridmap, x, y);
    if(oldData)
        Z_Free(oldData);

    Gridmap_SetBlock(blockmap->gridmap, x, y, subsectors);
}

void Map_BuildSubsectorBlockmap(gamemap_t* map)
{
#define BLKMARGIN       8
#define BLOCK_WIDTH     128
#define BLOCK_HEIGHT    128

typedef struct subsectornode_s {
    void*           data;
    struct subsectornode_s* next;
} subsectornode_t;

typedef struct subsecmap_s {
    uint            count;
    subsectornode_t* nodes;
} subsectormap_t;

    uint startTime = Sys_GetRealTime();

    uint i, x, y, subMapWidth, subMapHeight, minBlock[2], maxBlock[2];
    subsectornode_t* iter, *next;
    subsectormap_t* bmap, *block;
    vec2_t bounds[2], blockSize, dims;
    subsectorblockmap_t* blockmap;
    size_t totalLinks;

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], map->bBox[BOXLEFT] - BLKMARGIN,
                      map->bBox[BOXBOTTOM] - BLKMARGIN);
    V2_Set(bounds[1], map->bBox[BOXRIGHT] + BLKMARGIN,
                      map->bBox[BOXTOP] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, MAPBLOCKUNITS, MAPBLOCKUNITS);
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

    // We'll construct the temporary links using nodes.
    bmap = M_Calloc(sizeof(subsectormap_t) * subMapWidth * subMapHeight);

    // Process all the subsectors in the map.
    totalLinks = 0;
    for(i = 0; i < map->numSubsectors; ++i)
    {
        subsector_t* subsector = map->subsectors[i];

        if(!subsector->sector)
            continue;

        // Blockcoords to link to.
        SubsectorBlockmap_Block2fv(blockmap, minBlock, subsector->bBox[0].pos);
        SubsectorBlockmap_Block2fv(blockmap, maxBlock, subsector->bBox[1].pos);

        for(x = minBlock[0]; x <= maxBlock[0]; ++x)
            for(y = minBlock[1]; y <= maxBlock[1]; ++y)
            {
                if(x < 0 || y < 0 || x >= subMapWidth || y >= subMapHeight)
                {
                    Con_Printf("sub%i: outside block x=%i, y=%i\n", i, x, y);
                    continue;
                }

                // Create a new node.
                iter = M_Malloc(sizeof(subsectornode_t));
                iter->data = subsector;

                // Link to the temporary map.
                block = &bmap[x + y * subMapWidth];
                iter->next = block->nodes;
                block->nodes = iter;
                totalLinks++;
                if(!block->count)
                    totalLinks++; // +1 for terminating NULL.
                block->count++;
            }
    }

    // Create the actual links by 'hardening' the lists into arrays.
    for(y = 0; y < subMapHeight; ++y)
        for(x = 0; x < subMapWidth; ++x)
        {
            block = &bmap[y * subMapWidth + x];

            if(block->count > 0)
            {
                subsector_t** subsectors, **ptr;

                // A NULL-terminated array of pointers to subsectors.
                subsectors = Z_Malloc(sizeof(subsector_t*) * (block->count + 1), PU_STATIC, NULL);;

                // Copy pointers to the array, delete the nodes.
                ptr = subsectors;
                for(iter = block->nodes; iter; iter = next)
                {
                    *ptr++ = (subsector_t*) iter->data;
                    // Kill the node.
                    next = iter->next;
                    M_Free(iter);
                }
                // Terminate.
                *ptr = NULL;

                // Link it into the subSectorblockmap.
                SubsectorBlockmap_SetBlock(blockmap, x, y, subsectors);
            }
        }

    // Free the temporary link map.
    M_Free(bmap);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("Map_BuildSubsectorBlockmap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

    map->_subsectorBlockmap = blockmap;

#undef BLOCK_WIDTH
#undef BLOCK_HEIGHT
}

void PolyobjBlockmap_SetBlock(polyobjblockmap_t* blockmap, uint x, uint y,
                              linkpolyobj_t* poLink)
{
    void* data;

    assert(blockmap);

    data = Gridmap_Block(blockmap->gridmap, x, y);
    /*if(data)
        // Free block.*/
    Gridmap_SetBlock(blockmap->gridmap, x, y, poLink);
}

boolean unlinkPolyobjInBlock(void* ptr, void* context)
{
    linkpolyobj_t** data = ptr;
    polyobj_t* po = (polyobj_t*) context;

    P_PolyobjUnlinkFromRing(po, &(*data));
    return true;
}

boolean linkPolyobjInBlock(void* ptr, void* context)
{
    linkpolyobj_t** data = ptr;
    polyobj_t* po = (polyobj_t*) context;

    P_PolyobjLinkToRing(po, &(*data));
    return true;
}

void PolyobjBlockmap_Insert(polyobjblockmap_t* blockmap, polyobj_t* po)
{
    uint blockBox[4];

    assert(blockmap);
    assert(po);

    P_PolyobjUpdateBBox(po);
    PolyobjBlockmap_BoxToBlocks(blockmap, blockBox, po->box);

    Gridmap_IterateBoxv(blockmap->gridmap, blockBox, linkPolyobjInBlock, (void*) po);
}

void PolyobjBlockmap_Remove(polyobjblockmap_t* blockmap, polyobj_t* po)
{
    uint blockBox[4];

    assert(blockmap);
    assert(po);

    P_PolyobjUpdateBBox(po);
    PolyobjBlockmap_BoxToBlocks(blockmap, blockBox, po->box);

    Gridmap_IterateBoxv(blockmap->gridmap, blockBox, unlinkPolyobjInBlock, (void*) po);
}

void MobjBlockmap_Insert(mobjblockmap_t* blockmap, mobj_t* mo)
{
    uint block[2];
    linkmobj_t* link;

    assert(blockmap);
    assert(mo);

    MobjBlockmap_Block2fv(blockmap, block, mo->pos);

    link = (linkmobj_t*) Gridmap_Block(blockmap->gridmap, block[0], block[1]);
    if(!link)
    {   // Create a new link at the current block cell.
        link = Z_Malloc(sizeof(*link), PU_STATIC, 0);
        link->next = NULL;
        link->mobj = mo;

        Gridmap_SetBlock(blockmap->gridmap, block[0], block[1], link);
        return;
    }

    while(link->next != NULL && link->mobj != NULL)
    {
        link = link->next;
    }

    if(link->mobj == NULL)
    {
        link->mobj = mo;
        return;
    }

    link->next = Z_Malloc(sizeof(linkmobj_t), PU_STATIC, 0);
    link->next->next = NULL;
    link->next->mobj = mo;
}

boolean MobjBlockmap_Remove(mobjblockmap_t* blockmap, mobj_t* mo)
{
    uint blockXY[2];
    linkmobj_t* link;

    assert(blockmap);
    assert(mo);

    MobjBlockmap_Block2fv(blockmap, blockXY, mo->pos);

    link = (linkmobj_t*) Gridmap_Block(blockmap->gridmap, blockXY[0], blockXY[1]);
    for(; link; link = link->next)
    {
        if(link->mobj != mo)
            continue;

        link->mobj = NULL;
        return true; // Mobj was unlinked.
    }

    return false; // Mobj was not linked.
}

static void linkLineDefToBlock(linedefblockmap_t* blockmap, uint x, uint y,
                               linedef_t* lineDef)
{
    linklinedef_t* link = (linklinedef_t*)
        Gridmap_Block(blockmap->gridmap, x, y);

    if(!link)
    {   // Create a new link at the current block cell.
        link = Z_Malloc(sizeof(*link), PU_STATIC, 0);
        link->next = NULL;
        link->lineDef = lineDef;

        Gridmap_SetBlock(blockmap->gridmap, x, y, link);
        return;
    }

    while(link->next != NULL && link->lineDef != NULL)
    {
        link = link->next;
    }

    if(link->lineDef == NULL)
    {
        link->lineDef = lineDef;
        return;
    }

    link->next = Z_Malloc(sizeof(linklinedef_t), PU_STATIC, 0);
    link->next->next = NULL;
    link->next->lineDef = lineDef;
}

static void unlinkLineDefFromBlock(linedefblockmap_t* blockmap, uint x, uint y,
                                   linedef_t* lineDef)
{
    linklinedef_t* link = (linklinedef_t*)
        Gridmap_Block(blockmap->gridmap, x, y);

    for(; link; link = link->next)
    {
        if(link->lineDef != lineDef)
            continue;

        link->lineDef = NULL;
        return; // LineDef was unlinked.
    }

    // LineDef was not linked.
}

void LineDefBlockmap_Insert(linedefblockmap_t* blockmap, linedef_t* lineDef)
{
#define BLKSHIFT                7 // places to shift rel position for cell num
#define BLKMASK                 ((1<<BLKSHIFT)-1) // mask for rel position within cell

    uint i, block[2], dimensions[2];
    int vert, horiz;
    int origin[2], vtx1[2], vtx2[2], aabb[2][2], delta[2];
    boolean slopePos, slopeNeg;

    assert(blockmap);
    assert(lineDef);

    origin[0] = (int) blockmap->aabb[0][0];
    origin[1] = (int) blockmap->aabb[0][1];
    Gridmap_Dimensions(blockmap->gridmap, dimensions);

    // Determine all blocks it touches and add the lineDef number to those blocks.
    vtx1[0] = (int) lineDef->buildData.v[0]->pos[0];
    vtx1[1] = (int) lineDef->buildData.v[0]->pos[1];

    vtx2[0] = (int) lineDef->buildData.v[1]->pos[0];
    vtx2[1] = (int) lineDef->buildData.v[1]->pos[1];

    aabb[0][0] = (vtx1[0] > vtx2[0]? vtx2[0] : vtx1[0]);
    aabb[0][1] = (vtx1[1] > vtx2[1]? vtx2[1] : vtx1[1]);

    aabb[1][0] = (vtx1[0] > vtx2[0]? vtx1[0] : vtx2[0]);
    aabb[1][1] = (vtx1[1] > vtx2[1]? vtx1[1] : vtx2[1]);

    delta[0] = vtx2[0] - vtx1[0];
    delta[1] = vtx2[1] - vtx1[1];

    vert = !delta[0];
    horiz = !delta[1];

    slopePos = (delta[0] ^ delta[1]) > 0;
    slopeNeg = (delta[0] ^ delta[1]) < 0;

    // The lineDef always belongs to the blocks containing its endpoints
    block[0] = (vtx1[0] - origin[0]) >> BLKSHIFT;
    block[1] = (vtx1[1] - origin[1]) >> BLKSHIFT;
    linkLineDefToBlock(blockmap, block[0], block[1], lineDef);

    block[0] = (vtx2[0] - origin[0]) >> BLKSHIFT;
    block[1] = (vtx2[1] - origin[1]) >> BLKSHIFT;
    linkLineDefToBlock(blockmap, block[0], block[1], lineDef);

    // For each column, see where the lineDef along its left edge, which
    // it contains, intersects the LineDef. We don't want to interesect
    // vertical lines with columns.
    if(!vert)
    {
        for(i = 0; i < dimensions[0]; ++i)
        {
            // intersection of LineDef with x=origin[0]+(i<<BLKSHIFT)
            // (y-vtx1[1])*delta[0] = delta[1]*(x-vtx1[0])
            // y = delta[1]*(x-vtx1[0])+vtx1[1]*delta[0];
            int x = origin[0] + (i << BLKSHIFT); // (x,y) is intersection
            int y = (delta[1] * (x - vtx1[0])) / delta[0] + vtx1[1];
            int yb = (y - origin[1]) >> BLKSHIFT; // block row number
            int yp = (y - origin[1]) & BLKMASK; // y position within block

            // Already outside the blockmap?
            if(yb < 0 || yb > (signed) (dimensions[1]) + 1)
                continue;

            // Does the lineDef touch this column at all?
            if(x < aabb[0][0] || x > aabb[1][0])
                continue;

            // The cell that contains the intersection point is always added.
            linkLineDefToBlock(blockmap, i, yb, lineDef);

            // If the intersection is at a corner it depends on the slope
            // (and whether the lineDef extends past the intersection) which
            // blocks are hit.

            // Where does the intersection occur?
            if(yp == 0)
            {
                // Intersection occured at a corner
                if(slopeNeg) //   \ - blocks x,y-, x-,y
                {
                    if(yb > 0 && aabb[0][1] < y)
                        linkLineDefToBlock(blockmap, i, yb - 1, lineDef);

                    if(i > 0 && aabb[0][0] < x)
                        linkLineDefToBlock(blockmap, i - 1, yb, lineDef);
                }
                else if(slopePos) //   / - block x-,y-
                {
                    if(yb > 0 && i > 0 && aabb[0][0] < x)
                        linkLineDefToBlock(blockmap, i - 1, yb - 1, lineDef);
                }
                else if(horiz) //   - - block x-,y
                {
                    if(i > 0 && aabb[0][0] < x)
                        linkLineDefToBlock(blockmap, i - 1, yb, lineDef);
                }
            }
            else if(i > 0 && aabb[0][0] < x)
            {
                // Else not at corner: x-,y
                linkLineDefToBlock(blockmap, i - 1, yb, lineDef);
            }
        }
    }

    // For each row, see where the lineDef along its bottom edge, which
    // it contains, intersects the LineDef.
    if(!horiz)
    {
        for(i = 0; i < dimensions[1]; ++i)
        {
            // intersection of LineDef with y=origin[1]+(i<<BLKSHIFT)
            // (x,y) on LineDef satisfies: (y-vtx1[1])*delta[0] = delta[1]*(x-vtx1[0])
            // x = delta[0]*(y-vtx1[1])/delta[1]+vtx1[0];
            int y = origin[1] + (i << BLKSHIFT); // (x,y) is intersection
            int x = (delta[0] * (y - vtx1[1])) / delta[1] + vtx1[0];
            int xb = (x - origin[0]) >> BLKSHIFT; // block column number
            int xp = (x - origin[0]) & BLKMASK; // x position within block

            // Outside the blockmap?
            if(xb < 0 || xb > (signed) (dimensions[0]) + 1)
                continue;

            // Touches this row?
            if(y < aabb[0][1] || y > aabb[1][1])
                continue;

            linkLineDefToBlock(blockmap, xb, i, lineDef);

            // If the intersection is at a corner it depends on the slope
            // (and whether the lineDef extends past the intersection) which
            // blocks are hit

            // Where does the intersection occur?
            if(xp == 0)
            {
                // Intersection occured at a corner
                if(slopeNeg) //   \ - blocks x,y-, x-,y
                {
                    if(i > 0 && aabb[0][1] < y)
                        linkLineDefToBlock(blockmap, xb, i - 1, lineDef);

                    if(xb > 0 && aabb[0][0] < x)
                        linkLineDefToBlock(blockmap, xb - 1, i, lineDef);
                }
                else if(vert) //   | - block x,y-
                {
                    if(i > 0 && aabb[0][1] < y)
                        linkLineDefToBlock(blockmap, xb, i - 1, lineDef);
                }
                else if(slopePos) //   / - block x-,y-
                {
                    if(xb > 0 && i > 0 && aabb[0][1] < y)
                        linkLineDefToBlock(blockmap, xb - 1, i - 1, lineDef);
                }
            }
            else if(i > 0 && aabb[0][1] < y)
            {
                // Else not on a corner: x, y-
                linkLineDefToBlock(blockmap, xb, i - 1, lineDef);
            }
        }
    }

#undef BLKSHIFT
#undef BLKMASK
}

boolean LineDefBlockmap_Remove(linedefblockmap_t* blockmap, linedef_t* lineDef)
{
    uint i, block[2];

    assert(blockmap);
    assert(lineDef);

    /*if(!lineDef->numBlockLinks)
        return false; // LineDef was not linked.

    for(i = 0; i < lineDef->numBlockLinks; ++i)
        unlinkLineDefFromBlock(blockmap, block[0], block[1], lineDef);*/

    return true;    
}

void MobjBlockmap_Bounds(mobjblockmap_t* blockmap, pvec2_t min, pvec2_t max)
{
    assert(blockmap);

    if(min)
        V2_Copy(min, blockmap->aabb[0]);
    if(max)
        V2_Copy(max, blockmap->aabb[1]);
}

void LineDefBlockmap_Bounds(linedefblockmap_t* blockmap, pvec2_t min, pvec2_t max)
{
    assert(blockmap);

    if(min)
        V2_Copy(min, blockmap->aabb[0]);
    if(max)
        V2_Copy(max, blockmap->aabb[1]);
}

void MobjBlockmap_BlockSize(mobjblockmap_t* blockmap, pvec2_t blockSize)
{
    assert(blockmap);
    assert(blockSize);

    V2_Copy(blockSize, blockmap->blockSize);
}

void MobjBlockmap_Dimensions(mobjblockmap_t* blockmap, uint v[2])
{
    assert(blockmap);

    Gridmap_Dimensions(blockmap->gridmap, v);
}

uint LineDefBlockmap_NumInBlock(linedefblockmap_t* blockmap, uint x, uint y)
{
    linedef_t** data;
    uint num = 0;

    assert(blockmap);

    data = (linedef_t**) Gridmap_Block(blockmap->gridmap, x, y);
    // Count the number of lines linked to this block.
    if(data)
    {
        linedef_t** iter = data;
        while(*iter)
        {
            num++;
            *iter++;
        }
    }

    return num;
}

uint MobjBlockmap_NumInBlock(mobjblockmap_t* blockmap, uint x, uint y)
{
    linkmobj_t* data;
    uint num = 0;

    assert(blockmap);

    data = (linkmobj_t*) Gridmap_Block(blockmap->gridmap, x, y);
    // Count the number of mobjs linked to this block.
    if(data)
    {
        linkmobj_t* link = data;
        while(link)
        {
            if(link->mobj)
                num++;
            link = link->next;
        }
    }

    return num;
}

uint PolyobjBlockmap_NumInBlock(polyobjblockmap_t* blockmap, uint x, uint y)
{
    linkpolyobj_t* data;
    uint num = 0;

    assert(blockmap);

    data = Gridmap_Block(blockmap->gridmap, x, y);
    // Count the number of polyobjs linked to this block.
    if(data)
    {
        linkpolyobj_t* link = data;
        while(link)
        {
            if(link->polyobj)
                num++;
            link = link->next;
        }
    }

    return num;
}

uint SubsectorBlockmap_NumInBlock(subsectorblockmap_t* blockmap, uint x, uint y)
{
    subsector_t** data;
    uint num = 0;

    assert(blockmap);

    data = (subsector_t**) Gridmap_Block(blockmap->gridmap, x, y);
    // Count the number of subsectors linked to this block.
    if(data)
    {
        subsector_t** iter = data;
        while(*iter)
        {
            num++;
            *iter++;
        }
    }

    return num;
}

typedef struct {
    boolean        (*func) (linedef_t*, void*);
    boolean         retObjRecord;
    int             localValidCount;
    void*           context;
} iteratelinedefs_args_t;

static boolean iterateLineDefs(void* ptr, void* context)
{
    linklinedef_t* link = (linklinedef_t*) ptr;
    iteratelinedefs_args_t* args = (iteratelinedefs_args_t*) context;

    while(link)
    {
        linklinedef_t* next = link->next;

        if(link->lineDef)
            if(link->lineDef->validCount != args->localValidCount)
            {
                void* ptr;

                link->lineDef->validCount = args->localValidCount;

                if(args->retObjRecord)
                    ptr = (void*) P_ObjectRecord(DMU_LINEDEF, link->lineDef);
                else
                    ptr = (void*) link->lineDef;

                if(!args->func(ptr, args->context))
                    return false;
            }

        link = next;
    }

    return true;
}

boolean LineDefBlockmap_Iterate(linedefblockmap_t* blockmap, const uint block[2],
                                boolean (*func) (linedef_t*, void*),
                                void* context, boolean retObjRecord)
{
    iteratelinedefs_args_t args;

    assert(blockmap);
    assert(block);
    assert(func);

    args.func = func;
    args.context = context;
    args.localValidCount = validCount;
    args.retObjRecord = retObjRecord;

    return iterateLineDefs(Gridmap_Block(blockmap->gridmap, block[VX], block[VY]),
                           (void*) &args);
}

boolean LineDefBlockmap_BoxIterate(linedefblockmap_t* blockmap, const uint blockBox[4],
                                   boolean (*func) (linedef_t*, void*),
                                   void* context, boolean retObjRecord)
{
    iteratelinedefs_args_t args;

    assert(blockmap);
    assert(blockBox);
    assert(func);

    args.func = func;
    args.context = context;
    args.localValidCount = validCount;
    args.retObjRecord = retObjRecord;

    return Gridmap_IterateBoxv(blockmap->gridmap, blockBox, iterateLineDefs, (void*) &args);
}

typedef struct {
    boolean       (*func) (polyobj_t*, void *);
    int             localValidCount;
    void*           context;
} iteratepolyobjs_args_t;

static boolean iteratePolyobjs(void* ptr, void* context)
{
    linkpolyobj_t* link = (linkpolyobj_t*) ptr;
    iteratepolyobjs_args_t* args = (iteratepolyobjs_args_t*) context;

    while(link)
    {
        linkpolyobj_t* next = link->next;

        if(link->polyobj)
            if(link->polyobj->validCount != args->localValidCount)
            {
                link->polyobj->validCount = args->localValidCount;

                if(!args->func(link->polyobj, args->context))
                    return false;
            }

        link = next;
    }

    return true;
}

boolean PolyobjBlockmap_Iterate(polyobjblockmap_t* blockmap, const uint block[2],
                                boolean (*func) (polyobj_t*, void*),
                                void* context)
{
    iteratepolyobjs_args_t args;

    assert(blockmap);
    assert(block);
    assert(func);

    args.func = func;
    args.context = context;
    args.localValidCount = validCount;

    return iteratePolyobjs(Gridmap_Block(blockmap->gridmap, block[VX], block[VY]),
                           (void*) &args);
}

boolean PolyobjBlockmap_BoxIterate(polyobjblockmap_t* blockmap, const uint blockBox[4],
                                   boolean (*func) (polyobj_t*, void*),
                                   void* context)
{
    iteratepolyobjs_args_t args;

    assert(blockmap);
    assert(blockBox);
    assert(func);

    args.func = func;
    args.context = context;
    args.localValidCount = validCount;

    return Gridmap_IterateBoxv(blockmap->gridmap, blockBox, iteratePolyobjs,
                               (void*) &args);
}

typedef struct {
    boolean       (*func) (mobj_t*, void *);
    int             localValidCount;
    void*           context;
} iteratemobjs_args_t;

static boolean iterateMobjs(void* ptr, void* context)
{
    linkmobj_t* link = (linkmobj_t*) ptr;
    iteratemobjs_args_t* args = (iteratemobjs_args_t*) context;

    while(link)
    {
        linkmobj_t* next = link->next;

        if(link->mobj)
            if(link->mobj->validCount != args->localValidCount)
            {
                link->mobj->validCount = args->localValidCount;

                if(!args->func(link->mobj, args->context))
                    return false;
            }

        link = next;
    }

    return true;
}

boolean MobjBlockmap_Iterate(mobjblockmap_t* blockmap, const uint block[2],
                             boolean (*func) (mobj_t*, void*),
                             void* context)
{
    iteratemobjs_args_t  args;

    assert(blockmap);
    assert(block);
    assert(func);

    args.func = func;
    args.context = context;
    args.localValidCount = validCount;

    return iterateMobjs(Gridmap_Block(blockmap->gridmap, block[VX], block[VY]), (void*) &args);
}

boolean MobjBlockmap_BoxIterate(mobjblockmap_t* blockmap, const uint blockBox[4],
                                boolean (*func) (mobj_t*, void*),
                                void* context)
{
    iteratemobjs_args_t args;

    assert(blockmap);
    assert(blockBox);
    assert(func);

    args.func = func;
    args.context = context;
    args.localValidCount = validCount;

    return Gridmap_IterateBoxv(blockmap->gridmap, blockBox, iterateMobjs, (void*) &args);
}

typedef struct {
    boolean       (*func) (subsector_t*, void*);
    void*           context;
    arvec2_t        box;
    sector_t*       sector;
    int             localValidCount;
    boolean         retObjRecord;
} iteratesubsector_args_t;

static boolean iterateSubsectors(void* ptr, void* context)
{
    subsector_t** iter = (subsector_t**) ptr;
    iteratesubsector_args_t* args = (iteratesubsector_args_t*) context;

    if(iter)
    {
        while(*iter)
        {
            subsector_t* subsector = *iter;

            if(subsector->validCount != args->localValidCount)
            {
                boolean ok = true;

                subsector->validCount = args->localValidCount;

                // Check the sector restriction.
                if(args->sector && subsector->sector != args->sector)
                    ok = false;

                // Check the bounds.
                if(args->box &&
                   (subsector->bBox[1].pos[VX] < args->box[0][VX] ||
                    subsector->bBox[0].pos[VX] > args->box[1][VX] ||
                    subsector->bBox[0].pos[VY] > args->box[1][VY] ||
                    subsector->bBox[1].pos[VY] < args->box[0][VY]))
                   ok = false;

                if(ok)
                {
                    void* ptr;

                    if(args->retObjRecord)
                        ptr = (void*) P_ObjectRecord(DMU_SUBSECTOR, subsector);
                    else
                        ptr = (void*) subsector;

                    if(!args->func(ptr, args->context))
                        return false;
                }
            }

            *iter++;
        }
    }

    return true;
}

boolean SubsectorBlockmap_Iterate(subsectorblockmap_t* blockmap, const uint block[2],
                                  sector_t* sector, const arvec2_t box,
                                  int localValidCount,
                                  boolean (*func) (subsector_t*, void*),
                                  void* context)
{
    iteratesubsector_args_t args;

    assert(blockmap);
    assert(block);
    assert(func);

    args.func = func;
    args.context = context;
    args.box = box;
    args.localValidCount = localValidCount;
    args.sector = sector;
    args.retObjRecord = false;

    return iterateSubsectors(Gridmap_Block(blockmap->gridmap, block[VX], block[VY]), &args);
}

boolean SubsectorBlockmap_BoxIterate(subsectorblockmap_t* blockmap,
                                     const uint blockBox[4],
                                     sector_t* sector,  const arvec2_t box,
                                     int localValidCount,
                                     boolean (*func) (subsector_t*, void*),
                                     void* context, boolean retObjRecord)
{
    iteratesubsector_args_t args;

    assert(blockmap);
    assert(blockBox);
    assert(func);

    args.func = func;
    args.context = context;
    args.box = box;
    args.localValidCount = localValidCount;
    args.sector = sector;
    args.retObjRecord = retObjRecord;

    return Gridmap_IterateBoxv(blockmap->gridmap, blockBox, iterateSubsectors,
                               (void*) &args);
}

typedef struct {
    boolean       (*func) (linedef_t*, void*);
    void*           context;
    boolean         retObjRecord;
} iteratepolyobjlinedefs_args_t;

boolean PTR_PolyobjLines(polyobj_t* po, void* context)
{
    iteratepolyobjlinedefs_args_t* args = (iteratepolyobjlinedefs_args_t*) context;

    return P_PolyobjLinesIterator(po, args->func, args->context, args->retObjRecord);
}

boolean P_IterateLineDefsOfPolyobjs(polyobjblockmap_t* blockmap, const uint block[2],
                                    boolean (*func) (linedef_t*, void*),
                                    void* context, boolean retObjRecord)
{
    iteratepolyobjs_args_t args;
    iteratepolyobjlinedefs_args_t poargs;

    assert(blockmap);
    assert(block);
    assert(func);

    poargs.func = func;
    poargs.context = context;
    poargs.retObjRecord = retObjRecord;

    args.func = PTR_PolyobjLines;
    args.context = &poargs;
    args.localValidCount = validCount;

    return iteratePolyobjs(Gridmap_Block(blockmap->gridmap, block[VX], block[VY]),
                           (void*) &args);
}

boolean P_BoxIterateLineDefsOfPolyobjs(polyobjblockmap_t* blockmap, const uint blockBox[4],
                                       boolean (*func) (linedef_t*, void*),
                                       void* context, boolean retObjRecord)
{
    iteratepolyobjs_args_t args;
    iteratepolyobjlinedefs_args_t poargs;

    assert(blockmap);
    assert(blockBox);
    assert(func);

    poargs.func = func;
    poargs.context = context;
    poargs.retObjRecord = retObjRecord;

    args.func = PTR_PolyobjLines;
    args.context = &poargs;
    args.localValidCount = validCount;

    return Gridmap_IterateBoxv(blockmap->gridmap, blockBox, iteratePolyobjs, (void*) &args);
}

boolean MobjBlockmap_PathTraverse(mobjblockmap_t* blockmap, const uint originBlock[2],
                                  const uint destBlock[2], const float origin[2],
                                  const float dest[2],
                                  boolean (*func) (intercept_t*))
{
    uint count, block[2];
    float delta[2], partial;
    fixed_t intercept[2], step[2];
    int stepDir[2];

    if(destBlock[VX] > originBlock[VX])
    {
        stepDir[VX] = 1;
        partial = FIX2FLT(FRACUNIT - ((FLT2FIX(origin[VX]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
        delta[VY] = (dest[VY] - origin[VY]) / fabs(dest[VX] - origin[VX]);
    }
    else if(destBlock[VX] < originBlock[VX])
    {
        stepDir[VX] = -1;
        partial = FIX2FLT((FLT2FIX(origin[VX]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VY] = (dest[VY] - origin[VY]) / fabs(dest[VX] - origin[VX]);
    }
    else
    {
        stepDir[VX] = 0;
        partial = 1;
        delta[VY] = 256;
    }
    intercept[VY] = (FLT2FIX(origin[VY]) >> MAPBTOFRAC) +
        FLT2FIX(partial * delta[VY]);

    if(destBlock[VY] > originBlock[VY])
    {
        stepDir[VY] = 1;
        partial = FIX2FLT(FRACUNIT - ((FLT2FIX(origin[VY]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
        delta[VX] = (dest[VX] - origin[VX]) / fabs(dest[VY] - origin[VY]);
    }
    else if(destBlock[VY] < originBlock[VY])
    {
        stepDir[VY] = -1;
        partial = FIX2FLT((FLT2FIX(origin[VY]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VX] = (dest[VX] - origin[VX]) / fabs(dest[VY] - origin[VY]);
    }
    else
    {
        stepDir[VY] = 0;
        partial = 1;
        delta[VX] = 256;
    }
    intercept[VX] = (FLT2FIX(origin[VX]) >> MAPBTOFRAC) +
        FLT2FIX(partial * delta[VX]);

    //
    // Step through map blocks.
    //

    // Count is present to prevent a round off error from skipping the
    // break and ending up in an infinite loop..
    block[VX] = originBlock[VX];
    block[VY] = originBlock[VY];
    step[VX] = FLT2FIX(delta[VX]);
    step[VY] = FLT2FIX(delta[VY]);
    for(count = 0; count < 64; ++count)
    {
        if(!MobjBlockmap_Iterate(blockmap, block, PIT_AddMobjIntercepts, 0))
            return false; // Early out.

        if(block[VX] == destBlock[VX] && block[VY] == destBlock[VY])
            break;

        if((unsigned) (intercept[VY] >> FRACBITS) == block[VY])
        {
            intercept[VY] += step[VY];
            block[VX] += stepDir[VX];
        }
        else if((unsigned) (intercept[VX] >> FRACBITS) == block[VX])
        {
            intercept[VX] += step[VX];
            block[VY] += stepDir[VY];
        }
    }

    return true;
}

boolean LineDefBlockmap_PathTraverse(linedefblockmap_t* blockmap, const uint originBlock[2],
                                     const uint destBlock[2], const float origin[2],
                                     const float dest[2],
                                     boolean (*func) (intercept_t*))
{
    uint count, block[2];
    float delta[2], partial;
    fixed_t intercept[2], step[2];
    int stepDir[2];

    if(destBlock[VX] > originBlock[VX])
    {
        stepDir[VX] = 1;
        partial = FIX2FLT(FRACUNIT - ((FLT2FIX(origin[VX]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
        delta[VY] = (dest[VY] - origin[VY]) / fabs(dest[VX] - origin[VX]);
    }
    else if(destBlock[VX] < originBlock[VX])
    {
        stepDir[VX] = -1;
        partial = FIX2FLT((FLT2FIX(origin[VX]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VY] = (dest[VY] - origin[VY]) / fabs(dest[VX] - origin[VX]);
    }
    else
    {
        stepDir[VX] = 0;
        partial = 1;
        delta[VY] = 256;
    }
    intercept[VY] = (FLT2FIX(origin[VY]) >> MAPBTOFRAC) +
        FLT2FIX(partial * delta[VY]);

    if(destBlock[VY] > originBlock[VY])
    {
        stepDir[VY] = 1;
        partial = FIX2FLT(FRACUNIT - ((FLT2FIX(origin[VY]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
        delta[VX] = (dest[VX] - origin[VX]) / fabs(dest[VY] - origin[VY]);
    }
    else if(destBlock[VY] < originBlock[VY])
    {
        stepDir[VY] = -1;
        partial = FIX2FLT((FLT2FIX(origin[VY]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VX] = (dest[VX] - origin[VX]) / fabs(dest[VY] - origin[VY]);
    }
    else
    {
        stepDir[VY] = 0;
        partial = 1;
        delta[VX] = 256;
    }
    intercept[VX] = (FLT2FIX(origin[VX]) >> MAPBTOFRAC) +
        FLT2FIX(partial * delta[VX]);

    //
    // Step through map blocks.
    //

    // Count is present to prevent a round off error from skipping the
    // break and ending up in an infinite loop..
    block[VX] = originBlock[VX];
    block[VY] = originBlock[VY];
    step[VX] = FLT2FIX(delta[VX]);
    step[VY] = FLT2FIX(delta[VY]);
    for(count = 0; count < 64; ++count)
    {
        //if(!P_IterateLineDefsOfPolyobjs(blockmap, block, PIT_AddLineIntercepts, 0, false))
        //    return false; // Early out.

        if(!LineDefBlockmap_Iterate(blockmap, block, PIT_AddLineIntercepts, 0, false))
            return false; // Early out

        if(block[VX] == destBlock[VX] && block[VY] == destBlock[VY])
            break;

        if((unsigned) (intercept[VY] >> FRACBITS) == block[VY])
        {
            intercept[VY] += step[VY];
            block[VX] += stepDir[VX];
        }
        else if((unsigned) (intercept[VX] >> FRACBITS) == block[VX])
        {
            intercept[VX] += step[VX];
            block[VY] += stepDir[VY];
        }
    }

    return true;
}
