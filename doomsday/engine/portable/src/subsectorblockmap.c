/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
#include "de_play.h"

#include "subsectorblockmap.h"

// MACROS ------------------------------------------------------------------

//// \todo This stuff is obsolete and needs to be removed!
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

// TYPES -------------------------------------------------------------------

typedef struct listnode_s {
    struct listnode_s* next;
    void*           data;
} listnode_t;

typedef struct {
    uint            size;
    listnode_t*     head;
} linklist_t;

typedef struct {
    boolean       (*func) (subsector_t*, void*);
    void*           context;
    arvec2_t        box;
    sector_t*       sector;
    int             localValidCount;
    boolean         retObjRecord;
} iteratesubsectors_args_t;

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

static listnode_t* allocListNode(void)
{
    return Z_Calloc(sizeof(listnode_t), PU_STATIC, 0);
}

static void freeListNode(listnode_t* node)
{
    Z_Free(node);
}

static linklist_t* allocList(void)
{
    return Z_Calloc(sizeof(linklist_t), PU_STATIC, 0);
}

static void freeList(linklist_t* list)
{
    listnode_t* node = list->head;

    while(node)
    {
        listnode_t* next = node->next;
        freeListNode(node);
        node = next;
    }

    Z_Free(list);
}

static void listPushFront(linklist_t* list, subsector_t* subsector)
{
    listnode_t* node = allocListNode();
    
    node->data = subsector;

    node->next = list->head;
    list->head = node;

    list->size += 1;
}

static boolean listRemove(linklist_t* list, subsector_t* subsector)
{
    if(list->head)
    {
        listnode_t** node = &list->head;

        do
        {
            listnode_t** next = &(*node)->next;

            if((*node)->data == subsector)
            {
                freeListNode(*node);
                *node = *next;
                list->size -= 1;
                return true; // Mobj was unlinked.
            }

            node = next;
        } while(*node);
    }

    return false; // Mobj was not linked.
}

static uint listSize(linklist_t* list)
{
    return list->size;
}

static void linkSubsectorToBlock(subsectorblockmap_t* blockmap, uint x, uint y,
                                 subsector_t* subsector)
{
    linklist_t* list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);

    if(!list)
        list = Gridmap_SetBlock(blockmap->gridmap, x, y, allocList());

    listPushFront(list, subsector);
}

static boolean unlinkSubsectorFromBlock(subsectorblockmap_t* blockmap, uint x, uint y,
                                        subsector_t* subsector)
{
    linklist_t* list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);

    if(list)
    {
        boolean result = listRemove(list, subsector);
        if(result && !list->head)
        {
            freeList(list);
            Gridmap_SetBlock(blockmap->gridmap, x, y, NULL);
        }

        return result;
    }

    return false;
}

static boolean iterateSubsectors(void* ptr, void* context)
{
    linklist_t* list = (linklist_t*) ptr;
    iteratesubsectors_args_t* args = (iteratesubsectors_args_t*) context;

    if(list)
    {
        listnode_t* node = list->head;

        while(node)
        {
            listnode_t* next = node->next;

            if(node->data)
            {
                subsector_t* subsector = (subsector_t*) node->data;

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
                       (subsector->bBox[1].pos[0] < args->box[0][0] ||
                        subsector->bBox[0].pos[0] > args->box[1][0] ||
                        subsector->bBox[0].pos[1] > args->box[1][1] ||
                        subsector->bBox[1].pos[1] < args->box[0][1]))
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

            node = next;
        }
    }

    return true;
}

static boolean freeSubsectorBlockData(void* data, void* context)
{
    linklist_t* list = (linklist_t*) data;

    if(list)
        freeList(list);

    return true; // Continue iteration.
}

static void boxToBlocks(subsectorblockmap_t* bmap, uint blockBox[4], const arvec2_t box)
{
    uint dimensions[2];
    vec2_t min, max;

    Gridmap_Dimensions(bmap->gridmap, dimensions);

    V2_Set(min, MAX_OF(bmap->aabb[0][0], box[0][0]),
                MAX_OF(bmap->aabb[0][1], box[0][1]));

    V2_Set(max, MIN_OF(bmap->aabb[1][0], box[1][0]),
                MIN_OF(bmap->aabb[1][1], box[1][1]));

    blockBox[BOXLEFT]   = MINMAX_OF(0, (min[0] - bmap->aabb[0][0]) / bmap->blockSize[0], dimensions[0]);
    blockBox[BOXBOTTOM] = MINMAX_OF(0, (min[1] - bmap->aabb[0][1]) / bmap->blockSize[1], dimensions[1]);

    blockBox[BOXRIGHT]  = MINMAX_OF(0, (max[0] - bmap->aabb[0][0]) / bmap->blockSize[0], dimensions[0]);
    blockBox[BOXTOP]    = MINMAX_OF(0, (max[1] - bmap->aabb[0][1]) / bmap->blockSize[1], dimensions[1]);
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
           (blockmap->aabb[1][0] - blockmap->aabb[0][0]) / width,
           (blockmap->aabb[1][1] - blockmap->aabb[0][1]) / height);

    blockmap->gridmap = M_CreateGridmap(width, height, PU_STATIC);

    return blockmap;
}

void P_DestroySubsectorBlockmap(subsectorblockmap_t* blockmap)
{
    assert(blockmap);

    Gridmap_Iterate(blockmap->gridmap, freeSubsectorBlockData, NULL);

    M_DestroyGridmap(blockmap->gridmap);
    Z_Free(blockmap);
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

    if(!(x < blockmap->aabb[0][0] || x >= blockmap->aabb[1][0] ||
         y < blockmap->aabb[0][1] || y >= blockmap->aabb[1][1]))
    {
        dest[0] = (x - blockmap->aabb[0][0]) / blockmap->blockSize[0];
        dest[1] = (y - blockmap->aabb[0][1]) / blockmap->blockSize[1];

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

void SubsectorBlockmap_Link(subsectorblockmap_t* blockmap, subsector_t* subsector)
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

boolean SubsectorBlockmap_Unlink(subsectorblockmap_t* blockmap, subsector_t* subsector)
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

void SubsectorBlockmap_Bounds(subsectorblockmap_t* blockmap, pvec2_t min, pvec2_t max)
{
    assert(blockmap);

    if(min)
        V2_Copy(min, blockmap->aabb[0]);
    if(max)
        V2_Copy(max, blockmap->aabb[1]);
}

void SubsectorBlockmap_BlockSize(subsectorblockmap_t* blockmap, pvec2_t blockSize)
{
    assert(blockmap);
    assert(blockSize);

    V2_Copy(blockSize, blockmap->blockSize);
}

void SubsectorBlockmap_Dimensions(subsectorblockmap_t* blockmap, uint v[2])
{
    assert(blockmap);

    Gridmap_Dimensions(blockmap->gridmap, v);
}

uint SubsectorBlockmap_NumInBlock(subsectorblockmap_t* blockmap, uint x, uint y)
{
    linklist_t* list;

    assert(blockmap);

    list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);
    if(list)
        return listSize(list);

    return 0;
}

boolean SubsectorBlockmap_Iterate(subsectorblockmap_t* blockmap, const uint block[2],
                                  sector_t* sector, const arvec2_t box,
                                  int localValidCount,
                                  boolean (*func) (subsector_t*, void*),
                                  void* context)
{
    iteratesubsectors_args_t args;

    assert(blockmap);
    assert(block);
    assert(func);

    args.func = func;
    args.context = context;
    args.box = box;
    args.localValidCount = localValidCount;
    args.sector = sector;
    args.retObjRecord = false;

    return iterateSubsectors(Gridmap_Block(blockmap->gridmap, block[0], block[1]), &args);
}

boolean SubsectorBlockmap_BoxIterate(subsectorblockmap_t* blockmap,
                                     const uint blockBox[4],
                                     sector_t* sector,  const arvec2_t box,
                                     int localValidCount,
                                     boolean (*func) (subsector_t*, void*),
                                     void* context, boolean retObjRecord)
{
    iteratesubsectors_args_t args;

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
