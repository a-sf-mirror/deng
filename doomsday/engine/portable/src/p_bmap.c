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

typedef struct subsectormapblock_s {
    subsector_t**   subsectors;
} subsectormapblock_t;

typedef struct bmapblock_s {
    linedef_t**     lineDefs;
    linkmobj_t*     mobjLinks;
    linkpolyobj_t*  polyLinks;
} bmapblock_t;

typedef struct bmap_s {
    vec2_t          bBox[2];
    vec2_t          blockSize;
    uint            dimensions[2]; // In blocks.
    gridmap_t*      gridmap;
} bmap_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Blockmap_BoxToBlocks(blockmap_t* blockmap, uint blockBox[4],
                          const arvec2_t box)
{
    if(blockmap)
    {
        bmap_t*bmap = (bmap_t*) blockmap;
        vec2_t m[2];

        m[0][VX] = MAX_OF(bmap->bBox[0][VX], box[0][VX]);
        m[1][VX] = MIN_OF(bmap->bBox[1][VX], box[1][VX]);
        m[0][VY] = MAX_OF(bmap->bBox[0][VY], box[0][VY]);
        m[1][VY] = MIN_OF(bmap->bBox[1][VY], box[1][VY]);

        blockBox[BOXLEFT] =
            MINMAX_OF(0, (m[0][VX] - bmap->bBox[0][VX]) /
                            bmap->blockSize[VX], bmap->dimensions[0]);
        blockBox[BOXRIGHT] =
            MINMAX_OF(0, (m[1][VX] - bmap->bBox[0][VX]) /
                            bmap->blockSize[VX], bmap->dimensions[0]);
        blockBox[BOXBOTTOM] =
            MINMAX_OF(0, (m[0][VY] - bmap->bBox[0][VY]) /
                            bmap->blockSize[VY], bmap->dimensions[1]);
        blockBox[BOXTOP] =
            MINMAX_OF(0, (m[1][VY] - bmap->bBox[0][VY]) /
                            bmap->blockSize[VY], bmap->dimensions[1]);
    }
}

/**
 * Given a world coordinate, output the blockmap block[x, y] it resides in.
 */

boolean Blockmap_Block2fv(blockmap_t* blockmap, uint dest[2], const pvec2_t pos)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;

        if(!(pos[VX] < bmap->bBox[0][VX] || pos[VX] >= bmap->bBox[1][VX] ||
             pos[VY] < bmap->bBox[0][VY] || pos[VY] >= bmap->bBox[1][VY]))
        {
            dest[VX] = (pos[VX] - bmap->bBox[0][VX]) / bmap->blockSize[VX];
            dest[VY] = (pos[VY] - bmap->bBox[0][VY]) / bmap->blockSize[VY];

            return true;
        }

        return false; // Outside blockmap.
    }

    return false; // hmm...
}

static __inline int xToSubSectorBlockX(bmap_t* bmap, float x)
{
    if(x >= bmap->bBox[0][VX] && x < bmap->bBox[1][VX])
        return (x - bmap->bBox[0][VX]) / bmap->blockSize[VX];

    return -1;
}

static __inline int yToSubSectorBlockY(bmap_t* bmap, float y)
{
    if(y >= bmap->bBox[0][VY] && y < bmap->bBox[1][VY])
        return (y - bmap->bBox[0][VY]) / bmap->blockSize[VY];

    return -1;
}

static bmap_t* allocBmap(void)
{
    return Z_Calloc(sizeof(bmap_t), PU_MAP, 0);
}

blockmap_t* P_CreateBlockmap(const pvec2_t min, const pvec2_t max,
                             uint width, uint height)
{
    bmap_t* bmap = allocBmap();

    V2_Copy(bmap->bBox[0], min);
    V2_Copy(bmap->bBox[1], max);
    bmap->dimensions[VX] = width;
    bmap->dimensions[VY] = height;

    V2_Set(bmap->blockSize,
           (bmap->bBox[1][VX] - bmap->bBox[0][VX]) / bmap->dimensions[VX],
           (bmap->bBox[1][VY] - bmap->bBox[0][VY]) / bmap->dimensions[VY]);

    bmap->gridmap = M_GridmapCreate(bmap->dimensions[VX], bmap->dimensions[VY],
                                    sizeof(bmapblock_t), PU_MAP);

    VERBOSE(Con_Message
            ("P_BlockMapCreate: w=%i h=%i\n", bmap->dimensions[VX],
             bmap->dimensions[VY]));

    return (blockmap_t*) bmap;
}

void Blockmap_SetBlockSubsectors(blockmap_t* blockmap, uint x, uint y, subsector_t** subsectors)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        subsectormapblock_t* block =
            M_GridmapGetBlock(bmap->gridmap, x, y, true);

        if(block)
        {
            block->subsectors = subsectors;
        }
    }
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

    int xl, xh, yl, yh, x, y;
    int subMapWidth, subMapHeight;
    uint i;
    subsectornode_t* iter, *next;
    subsectormap_t* bmap, *block;
    vec2_t bounds[2], blockSize, dims;
    blockmap_t* subsectorBlockMap;
    subsector_t** subsectorLinks;
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

    subsectorBlockMap = (blockmap_t*)
        P_CreateBlockmap(bounds[0], bounds[1], subMapWidth, subMapHeight);

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
        xl = xToSubSectorBlockX((bmap_t*)subsectorBlockMap, subsector->bBox[0].pos[VX]);
        xh = xToSubSectorBlockX((bmap_t*)subsectorBlockMap, subsector->bBox[1].pos[VX]);
        yl = yToSubSectorBlockY((bmap_t*)subsectorBlockMap, subsector->bBox[0].pos[VY]);
        yh = yToSubSectorBlockY((bmap_t*)subsectorBlockMap, subsector->bBox[1].pos[VY]);

        for(x = xl; x <= xh; ++x)
            for(y = yl; y <= yh; ++y)
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

    subsectorLinks = Z_Malloc(totalLinks * sizeof(subsector_t*), PU_MAP, NULL);

    // Create the actual links by 'hardening' the lists into arrays.
    for(y = 0; y < subMapHeight; ++y)
        for(x = 0; x < subMapWidth; ++x)
        {
            block = &bmap[y * subMapWidth + x];

            if(block->count > 0)
            {
                subsector_t** subsectors, **ptr;

                // A NULL-terminated array of pointers to subsectors.
                subsectors = subsectorLinks;

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
                Blockmap_SetBlockSubsectors(subsectorBlockMap, x, y, subsectors);

                subsectorLinks += block->count + 1;
            }
        }

    map->subsectorBlockMap = subsectorBlockMap;

    // Free the temporary link map.
    M_Free(bmap);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("Map_BuildSubsectorBlockmap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

#undef BLOCK_WIDTH
#undef BLOCK_HEIGHT
}

void Blockmap_SetBlock(blockmap_t* blockmap, uint x, uint y,
                        linedef_t** lines, linkmobj_t* moLink,
                        linkpolyobj_t* poLink)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        bmapblock_t* block = M_GridmapGetBlock(bmap->gridmap, x, y, true);

        if(block)
        {
            block->lineDefs = lines;
            block->mobjLinks = moLink;
            block->polyLinks = poLink;
        }
    }
}

boolean unlinkPolyobjInBlock(void* ptr, void* context)
{
    bmapblock_t*        block = (bmapblock_t*) ptr;
    polyobj_t*          po = (polyobj_t*) context;

    P_PolyobjUnlinkFromRing(po, &block->polyLinks);
    return true;
}

boolean linkPolyobjInBlock(void* ptr, void* context)
{
    bmapblock_t*        block = (bmapblock_t*) ptr;
    polyobj_t*          po = (polyobj_t *) context;

    P_PolyobjLinkToRing(po, &block->polyLinks);
    return true;
}

void Blockmap_LinkPolyobj(blockmap_t* blockmap, polyobj_t* po)
{
    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;
        uint                blockBox[4];

        P_PolyobjUpdateBBox(po);
        Blockmap_BoxToBlocks(blockmap, blockBox, po->box);

        M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                              linkPolyobjInBlock, (void*) po);
    }
}

void Blockmap_UnlinkPolyobj(blockmap_t* blockmap, polyobj_t* po)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        uint blockBox[4];

        P_PolyobjUpdateBBox(po);
        Blockmap_BoxToBlocks(blockmap, blockBox, po->box);

        M_GridmapBoxIteratorv(bmap->gridmap, blockBox, unlinkPolyobjInBlock, (void*) po);
    }
}

void Blockmap_LinkMobj(blockmap_t* blockmap, mobj_t* mo)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        uint blockXY[2];
        bmapblock_t* block;

        Blockmap_Block2fv(blockmap, blockXY, mo->pos);
        block = (bmapblock_t*) M_GridmapGetBlock(bmap->gridmap, blockXY[0], blockXY[1], true);

        if(block)
            P_MobjLinkToRing(mo, &block->mobjLinks);
    }
}

boolean Blockmap_UnlinkMobj(blockmap_t* blockmap, mobj_t* mo)
{
    boolean unlinked = false;

    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        uint blockXY[2];
        bmapblock_t* block;

        Blockmap_Block2fv(blockmap, blockXY, mo->pos);
        block = (bmapblock_t*) M_GridmapGetBlock(bmap->gridmap, blockXY[0], blockXY[1], false);
        if(block)
            unlinked = P_MobjUnlinkFromRing(mo, &block->mobjLinks);
    }

    return unlinked;
}

void Blockmap_Bounds(blockmap_t* blockmap, pvec2_t min, pvec2_t max)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;

        if(min)
            V2_Copy(min, bmap->bBox[0]);
        if(max)
            V2_Copy(max, bmap->bBox[1]);
    }
}

void Blockmap_BlockSize(blockmap_t* blockmap, pvec2_t blockSize)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;

        V2_Copy(blockSize, bmap->blockSize);
    }
}

void Blockmap_Dimensions(blockmap_t* blockmap, uint v[2])
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;

        v[VX] = bmap->dimensions[VX];
        v[VY] = bmap->dimensions[VY];
    }
}

int Blockmap_NumLineDefs(blockmap_t* blockmap, uint x, uint y)
{
    int num = -1;

    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        bmapblock_t* block = M_GridmapGetBlock(bmap->gridmap, x, y, false);

        if(block)
        {
            // Count the number of lines linked to this block.
            num = 0;
            if(block->lineDefs)
            {
                linedef_t** iter = block->lineDefs;
                while(*iter)
                {
                    num++;
                    *iter++;
                }
            }
        }
    }

    return num;
}

int Blockmap_NumMobjs(blockmap_t* blockmap, uint x, uint y)
{
    int num = -1;

    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        bmapblock_t* block = M_GridmapGetBlock(bmap->gridmap, x, y, false);

        if(block)
        {
            // Count the number of mobjs linked to this block.
            num = 0;
            if(block->mobjLinks)
            {
                linkmobj_t* link = block->mobjLinks;
                while(link)
                {
                    if(link->mobj)
                        num++;
                    link = link->next;
                }
            }
        }
    }

    return num;
}

int Blockmap_NumPolyobjs(blockmap_t* blockmap, uint x, uint y)
{
    int num = -1;

    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        bmapblock_t* block = M_GridmapGetBlock(bmap->gridmap, x, y, false);

        if(block)
        {
            // Count the number of polyobjs linked to this block.
            num = 0;
            if(block->polyLinks)
            {
                linkpolyobj_t* link = block->polyLinks;
                while(link)
                {
                    if(link->polyobj)
                        num++;
                    link = link->next;
                }
            }
        }
    }

    return num;
}

int Blockmap_NumSubsectors(blockmap_t* blockmap, uint x, uint y)
{
    int num = -1;

    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        subsectormapblock_t* block = M_GridmapGetBlock(bmap->gridmap, x, y, false);

        if(block)
        {
            subsector_t** iter = block->subsectors;

            // Count the number of subsectors linked to this block.
            num = 0;
            while(*iter)
            {
                num++;
                *iter++;
            }
        }
    }

    return num;
}

typedef struct bmapiterparams_s {
    int             localValidCount;
    boolean       (*func) (linedef_t*, void *);
    void*           param;
    boolean         retObjRecord;
} bmapiterparams_t;

static boolean bmapBlockLinesIterator(void* ptr, void* context)
{
    bmapblock_t* block = (bmapblock_t*) ptr;

    if(block->lineDefs)
    {
        linedef_t** iter;
        bmapiterparams_t* args = (bmapiterparams_t*) context;

        iter = block->lineDefs;
        while(*iter)
        {
            linedef_t* line = *iter;

            if(line->validCount != args->localValidCount)
            {
                void* ptr;

                line->validCount = args->localValidCount;

                if(args->retObjRecord)
                    ptr = (void*) DMU_GetObjRecord(DMU_LINEDEF, line);
                else
                    ptr = (void*) line;

                if(!args->func(ptr, args->param))
                    return false;
            }

            *iter++;
        }
    }

    return true;
}

boolean Blockmap_IterateLineDefs(blockmap_t* blockmap, const uint block[2],
                                 boolean (*func) (linedef_t*, void*),
                                 void* data, boolean retObjRecord)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        bmapblock_t* bmapBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(bmapBlock)
        {
            bmapiterparams_t args;

            args.localValidCount = validCount;
            args.func = func;
            args.param = data;
            args.retObjRecord = retObjRecord;

            return bmapBlockLinesIterator(bmapBlock, &args);
        }
    }

    return true;
}

boolean Blockmap_BoxIterateLineDefs(blockmap_t* blockmap, const uint blockBox[4],
                                boolean (*func) (linedef_t*, void*),
                                void* data, boolean retObjRecord)
{
    bmap_t* bmap = (bmap_t*) blockmap;
    bmapiterparams_t args;

    args.localValidCount = validCount;
    args.func = func;
    args.param = data;
    args.retObjRecord = retObjRecord;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 bmapBlockLinesIterator, (void*) &args);
}

typedef struct bmappoiterparams_s {
    int             localValidCount;
    boolean       (*func) (polyobj_t*, void *);
    void*           param;
} bmappoiterparams_t;

static boolean bmapBlockPolyobjsIterator(void* ptr, void* context)
{
    bmapblock_t* block = (bmapblock_t*) ptr;
    bmappoiterparams_t* args = (bmappoiterparams_t*) context;
    linkpolyobj_t* link;

    link = block->polyLinks;
    while(link)
    {
        linkpolyobj_t*      next = link->next;

        if(link->polyobj)
            if(link->polyobj->validCount != args->localValidCount)
            {
                link->polyobj->validCount = args->localValidCount;

                if(!args->func(link->polyobj, args->param))
                    return false;
            }

        link = next;
    }

    return true;
}

boolean Blockmap_IteratePolyobjs(blockmap_t* blockmap, const uint block[2],
                                 boolean (*func) (polyobj_t*, void*),
                                 void* data)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        bmapblock_t* bmapBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(bmapBlock)
        {
            bmappoiterparams_t args;

            args.localValidCount = validCount;
            args.func = func;
            args.param = data;

            return bmapBlockPolyobjsIterator(bmapBlock, (void*) &args);
        }
    }

    return true;
}

boolean Blockmap_BoxIteratePolyobjs(blockmap_t* blockmap, const uint blockBox[4],
                                    boolean (*func) (polyobj_t*, void*),
                                    void* data)
{
    bmap_t* bmap = (bmap_t*) blockmap;
    bmappoiterparams_t args;

    args.localValidCount = validCount;
    args.func = func;
    args.param = data;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 bmapBlockPolyobjsIterator, (void*) &args);
}

typedef struct bmapmoiterparams_s {
    int             localValidCount;
    boolean       (*func) (mobj_t*, void *);
    void*           param;
} bmapmoiterparams_t;

static boolean bmapBlockMobjsIterator(void* ptr, void* context)
{
    bmapblock_t* block = (bmapblock_t*) ptr;
    bmapmoiterparams_t* args = (bmapmoiterparams_t*) context;
    linkmobj_t* link;

    link = block->mobjLinks;
    while(link)
    {
        linkmobj_t* next = link->next;

        if(link->mobj)
            if(link->mobj->validCount != args->localValidCount)
            {
                link->mobj->validCount = args->localValidCount;

                if(!args->func(link->mobj, args->param))
                    return false;
            }

        link = next;
    }

    return true;
}

boolean Blockmap_IterateMobjs(blockmap_t* blockmap, const uint block[2],
                              boolean (*func) (mobj_t*, void*),
                              void* data)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        bmapblock_t* bmapBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(bmapBlock)
        {
            bmapmoiterparams_t  args;

            args.localValidCount = validCount;
            args.func = func;
            args.param = data;

            return bmapBlockMobjsIterator(bmapBlock, (void*) &args);
        }
    }

    return true;
}

boolean Blockmap_BoxIterateMobjs(blockmap_t *blockmap, const uint blockBox[4],
                                boolean (*func) (mobj_t*, void*),
                                void* data)
{
    bmap_t*             bmap = (bmap_t*) blockmap;
    bmapmoiterparams_t  args;

    args.localValidCount = validCount;
    args.func = func;
    args.param = data;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 bmapBlockMobjsIterator, (void*) &args);
}

typedef struct subsectoriterparams_s {
    arvec2_t        box;
    sector_t*       sector;
    int             localValidCount;
    boolean       (*func) (subsector_t*, void*);
    void*           param;
    boolean         retObjRecord;
} subsectoriterparams_t;

static boolean subsectorBlockIterator(void* ptr, void* context)
{
    subsectormapblock_t* block = (subsectormapblock_t*) ptr;

    if(block->subsectors)
    {
        subsector_t** iter;
        subsectoriterparams_t* args = (subsectoriterparams_t*) context;

        iter = block->subsectors;

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
                        ptr = (void*) DMU_GetObjRecord(DMU_SUBSECTOR, subsector);
                    else
                        ptr = (void*) subsector;

                    if(!args->func(ptr, args->param))
                        return false;
                }
            }

            *iter++;
        }
    }

    return true;
}

boolean Blockmap_IterateSubsectors(blockmap_t* blockmap, const uint block[2],
                                   sector_t* sector, const arvec2_t box,
                                   int localValidCount,
                                   boolean (*func) (subsector_t*, void*),
                                   void* data)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        subsectormapblock_t* subsectorBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(subsectorBlock && subsectorBlock->subsectors)
        {
            subsectoriterparams_t args;

            args.box = box;
            args.localValidCount = localValidCount;
            args.sector = sector;
            args.func = func;
            args.param = data;
            args.retObjRecord = false;

            return subsectorBlockIterator(subsectorBlock, &args);
        }
    }

    return true;
}

boolean Blockmap_BoxIterateSubsectors(blockmap_t* blockmap,
                                      const uint blockBox[4],
                                      sector_t* sector,  const arvec2_t box,
                                      int localValidCount,
                                      boolean (*func) (subsector_t*, void*),
                                      void* data, boolean retObjRecord)
{
    bmap_t* bmap = (bmap_t*) blockmap;
    subsectoriterparams_t args;

    args.box = box;
    args.localValidCount = localValidCount;
    args.sector = sector;
    args.func = func;
    args.param = data;
    args.retObjRecord = retObjRecord;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 subsectorBlockIterator, (void*) &args);
}

typedef struct poiterparams_s {
    boolean       (*func) (linedef_t*, void*);
    void*           param;
    boolean         retObjRecord;
} poiterparams_t;

boolean PTR_PolyobjLines(polyobj_t* po, void* data)
{
    poiterparams_t* args = (poiterparams_t*) data;

    return P_PolyobjLinesIterator(po, args->func, args->param, args->retObjRecord);
}

boolean Blockmap_IteratePolyobjLineDefs(blockmap_t* blockmap, const uint block[2],
                                       boolean (*func) (linedef_t*, void*),
                                       void* data, boolean retObjRecord)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        bmapblock_t* bmapBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(bmapBlock && bmapBlock->polyLinks)
        {
            bmappoiterparams_t args;
            poiterparams_t poargs;

            poargs.func = func;
            poargs.param = data;
            poargs.retObjRecord = retObjRecord;

            args.localValidCount = validCount;
            args.func = PTR_PolyobjLines;
            args.param = &poargs;

            return bmapBlockPolyobjsIterator(bmapBlock, &args);
        }
    }

    return true;
}

boolean Blockmap_BoxIteratePolyobjLineDefs(blockmap_t* blockmap, const uint blockBox[4],
                                           boolean (*func) (linedef_t*, void*),
                                           void* data, boolean retObjRecord)
{
    bmap_t* bmap = (bmap_t*) blockmap;
    bmappoiterparams_t args;
    poiterparams_t poargs;

    poargs.func = func;
    poargs.param = data;
    poargs.retObjRecord = retObjRecord;

    args.localValidCount = validCount;
    args.func = PTR_PolyobjLines;
    args.param = &poargs;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 bmapBlockPolyobjsIterator, (void*) &args);
}

boolean Blockmap_PathTraverse(blockmap_t* bmap, const uint originBlock[2],
                              const uint destBlock[2],
                              const float origin[2], const float dest[2],
                              int flags, boolean (*func) (intercept_t*))
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
        if(flags & PT_ADDLINES)
        {
            if(!Blockmap_IteratePolyobjLineDefs(bmap, block, PIT_AddLineIntercepts, 0, false))
                return false; // Early out.

            if(!Blockmap_IterateLineDefs(bmap, block, PIT_AddLineIntercepts, 0, false))
                return false; // Early out
        }

        if(flags & PT_ADDMOBJS)
        {
            if(!Blockmap_IterateMobjs(bmap, block, PIT_AddMobjIntercepts, 0))
                return false; // Early out.
        }

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
