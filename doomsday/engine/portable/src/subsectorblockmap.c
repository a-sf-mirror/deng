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

typedef struct linksubsector_s {
    struct linksubsector_s* next;
    struct subsector_s* subsector;
} linksubsector_t;

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
    linksubsector_t* link = (linksubsector_t*) data;

    while(link)
    {
        linksubsector_t* next = link->next;
        Z_Free(link);
        link = next;
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

static void linkSubsectorToBlock(subsectorblockmap_t* blockmap, uint x, uint y,
                                 subsector_t* subsector)
{
    linksubsector_t* link = (linksubsector_t*)
        Gridmap_Block(blockmap->gridmap, x, y);

    if(!link)
    {   // Create a new link at the current block cell.
        link = Z_Malloc(sizeof(*link), PU_STATIC, 0);
        link->next = NULL;
        link->subsector = subsector;

        Gridmap_SetBlock(blockmap->gridmap, x, y, link);
        return;
    }

    while(link->next != NULL && link->subsector != NULL)
    {
        link = link->next;
    }

    if(link->subsector == NULL)
    {
        link->subsector = subsector;
        return;
    }

    link->next = Z_Malloc(sizeof(linksubsector_t), PU_STATIC, 0);
    link->next->next = NULL;
    link->next->subsector = subsector;
}

static void unlinkSubsectorFromBlock(subsectorblockmap_t* blockmap, uint x, uint y,
                                     subsector_t* subsector)
{
    linksubsector_t* link = (linksubsector_t*)
        Gridmap_Block(blockmap->gridmap, x, y);

    for(; link; link = link->next)
    {
        if(link->subsector != subsector)
            continue;

        link->subsector = NULL;
        return; // Subsector was unlinked.
    }

    // Subsector was not linked.
}

void SubsectorBlockmap_Insert(subsectorblockmap_t* blockmap, subsector_t* subsector)
{
    uint x, y, minBlock[2], maxBlock[2], dimensions[2];

    assert(blockmap);
    assert(subsector);

    if(!subsector->sector)
        return; // Never added.

    Gridmap_Dimensions(blockmap->gridmap, dimensions);

    // Blockcoords to link to.
    SubsectorBlockmap_Block2fv(blockmap, minBlock, subsector->bBox[0].pos);
    SubsectorBlockmap_Block2fv(blockmap, maxBlock, subsector->bBox[1].pos);

    for(x = minBlock[0]; x <= maxBlock[0]; ++x)
        for(y = minBlock[1]; y <= maxBlock[1]; ++y)
        {
            linkSubsectorToBlock(blockmap, x, y, subsector);
        }
}

boolean SubsectorBlockmap_Remove(subsectorblockmap_t* blockmap, subsector_t* subsector)
{
    uint i, block[2];

    assert(blockmap);
    assert(subsector);

    /*if(!subsector->numBlockLinks)
        return false; // Subsector was not linked.

    for(i = 0; i < subsector->numBlockLinks; ++i)
        unlinkSubsectorFromBlock(blockmap, block[0], block[1], subsector);*/

    return true;    
}

uint SubsectorBlockmap_NumInBlock(subsectorblockmap_t* blockmap, uint x, uint y)
{
    linksubsector_t* data;
    uint num = 0;

    assert(blockmap);

    data = (linksubsector_t*) Gridmap_Block(blockmap->gridmap, x, y);
    // Count the number of subsectors linked to this block.
    if(data)
    {
        linksubsector_t* link = data;
        while(link)
        {
            if(link->subsector)
                num++;
            link = link->next;
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
    linksubsector_t* link = (linksubsector_t*) ptr;
    iteratesubsector_args_t* args = (iteratesubsector_args_t*) context;

    while(link)
    {
        linksubsector_t* next = link->next;

        if(link->subsector)
        {
            subsector_t* subsector = link->subsector;

            if(subsector->validCount != args->localValidCount)
            {
                boolean ok = true;
                void* ptr;

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
                    if(args->retObjRecord)
                        ptr = (void*) P_ObjectRecord(DMU_SUBSECTOR, subsector);
                    else
                        ptr = (void*) subsector;

                    if(!args->func(ptr, args->context))
                        return false;
                }
            }
        }

        link = next;
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
