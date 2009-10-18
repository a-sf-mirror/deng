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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static subsectorblockmap_t* allocBlockmap(void)
{
    return Z_Calloc(sizeof(subsectorblockmap_t), PU_STATIC, 0);
}

static void freeBlockmap(subsectorblockmap_t* bmap)
{
    Z_Free(bmap);
}

static void boxToBlocks(subsectorblockmap_t* bmap, uint blockBox[4], const arvec2_t box)
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

void SubsectorBlockmap_BoxToBlocks(subsectorblockmap_t* blockmap, uint blockBox[4],
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
