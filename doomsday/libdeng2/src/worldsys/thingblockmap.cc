/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include <math.h>

#include "de/ThingBlockmap"

using namespace de;

/// \todo This stuff is obsolete and needs to be removed!
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

typedef struct listnode_s {
    struct listnode_s* next;
    void*           data;
} listnode_t;

typedef struct {
    uint            size;
    listnode_t*     head;
} linklist_t;

typedef struct {
    boolean       (*func) (mobj_t*, void *);
    int             localValidCount;
    void*           context;
} iteratemobjs_args_t;

static ThingBlockmap* allocBlockmap(void)
{
    return Z_Calloc(sizeof(ThingBlockmap), PU_STATIC, 0);
}

static void freeBlockmap(ThingBlockmap* bmap)
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

static void listPushFront(linklist_t* list, mobj_t* mobj)
{
    listnode_t* node = allocListNode();

    node->data = mobj;

    node->next = list->head;
    list->head = node;

    list->size += 1;
}

static boolean listRemove(linklist_t* list, mobj_t* mobj)
{
    if(list->head)
    {
        listnode_t** node = &list->head;

        do
        {
            listnode_t** next = &(*node)->next;

            if((*node)->data == mobj)
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

static void linkMobjToBlock(ThingBlockmap* blockmap, uint x, uint y, mobj_t* mobj)
{
    linklist_t* list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);

    if(!list)
        list = Gridmap_SetBlock(blockmap->gridmap, x, y, allocList());

    listPushFront(list, mobj);
}

static boolean unlinkMobjFromBlock(ThingBlockmap* blockmap, uint x, uint y, mobj_t* mobj)
{
    linklist_t* list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);

    if(list)
    {
        boolean result = listRemove(list, mobj);
        if(result && !list->head)
        {
            freeList(list);
            Gridmap_SetBlock(blockmap->gridmap, x, y, NULL);
        }

        return result;
    }

    return false;
}

static boolean iterateMobjs(void* ptr, void* context)
{
    linklist_t* list = (linklist_t*) ptr;
    iteratemobjs_args_t* args = (iteratemobjs_args_t*) context;

    if(list)
    {
        listnode_t* node = list->head;

        while(node)
        {
            listnode_t* next = node->next;

            if(node->data)
            {
                mobj_t* mobj = (mobj_t*) node->data;

                if(mobj->validCount != args->localValidCount)
                {
                    mobj->validCount = args->localValidCount;

                    if(!args->func(mobj, args->context))
                        return false;
                }
            }

            node = next;
        }
    }

    return true;
}

static boolean freeMobjBlockData(void* data, void* context)
{
    linklist_t* list = (linklist_t*) data;

    if(list)
        freeList(list);

    return true; // Continue iteration.
}

static void boxToBlocks(ThingBlockmap* bmap, uint blockBox[4], const arvec2_t box)
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

ThingBlockmap* P_CreateMobjBlockmap(const pvec2_t min, const pvec2_t max,
                                     uint width, uint height)
{
    ThingBlockmap* blockmap;

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

void P_DestroyMobjBlockmap(ThingBlockmap* blockmap)
{
    assert(blockmap);

    Gridmap_Iterate(blockmap->gridmap, freeMobjBlockData, NULL);

    M_DestroyGridmap(blockmap->gridmap);
    Z_Free(blockmap);
}

void MobjBlockmap_BoxToBlocks(ThingBlockmap* blockmap, uint blockBox[4],
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
boolean MobjBlockmap_Block2f(ThingBlockmap* blockmap, uint dest[2], float x, float y)
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
boolean MobjBlockmap_Block2fv(ThingBlockmap* blockmap, uint dest[2], const float pos[2])
{
    return MobjBlockmap_Block2f(blockmap, dest, pos[0], pos[1]);
}

void MobjBlockmap_Link(ThingBlockmap* blockmap, mobj_t* mobj)
{
    uint block[2];

    assert(blockmap);
    assert(mobj);

    MobjBlockmap_Block2fv(blockmap, block, mobj->pos);

    linkMobjToBlock(blockmap, block[0], block[1], mobj);
}

boolean MobjBlockmap_Unlink(ThingBlockmap* blockmap, mobj_t* mobj)
{
    assert(blockmap);
    assert(mobj);
    {
    boolean result = false;
    uint dimensions[2], x, y;

    Gridmap_Dimensions(blockmap->gridmap, dimensions);
    for(y = 0; y < dimensions[1]; ++y)
        for(x = 0; x < dimensions[0]; ++x)
        {
            if(unlinkMobjFromBlock(blockmap, x, y, mobj))
                if(!result)
                    result = true;
        }
    return result;
    }
}

void MobjBlockmap_Bounds(ThingBlockmap* blockmap, pvec2_t min, pvec2_t max)
{
    assert(blockmap);

    if(min)
        V2_Copy(min, blockmap->aabb[0]);
    if(max)
        V2_Copy(max, blockmap->aabb[1]);
}

void MobjBlockmap_BlockSize(ThingBlockmap* blockmap, pvec2_t blockSize)
{
    assert(blockmap);
    assert(blockSize);

    V2_Copy(blockSize, blockmap->blockSize);
}

void MobjBlockmap_Dimensions(ThingBlockmap* blockmap, uint v[2])
{
    assert(blockmap);

    Gridmap_Dimensions(blockmap->gridmap, v);
}

uint MobjBlockmap_NumInBlock(ThingBlockmap* blockmap, uint x, uint y)
{
    linklist_t* list;

    assert(blockmap);

    list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);
    if(list)
        return listSize(list);

    return 0;
}

boolean MobjBlockmap_Iterate(ThingBlockmap* blockmap, const uint block[2],
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

    return iterateMobjs(Gridmap_Block(blockmap->gridmap, block[0], block[1]), (void*) &args);
}

boolean MobjBlockmap_BoxIterate(ThingBlockmap* blockmap, const uint blockBox[4],
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

static boolean addMobjIntercepts(mobj_t* mo, void* data)
{
    float x1, y1, x2, y2;
    int s[2];
    divline_t dl;
    float frac;

    if(mo->user() && (mo->user().flags & DDPF_CAMERA))
        return true; // $democam: ssshh, keep going, we're not here...

    // Check a corner to corner crosubSectortion for hit.
    if((traceLOS.dX ^ traceLOS.dY) > 0)
    {
        x1 = mo->pos[VX] - mo->radius;
        y1 = mo->pos[VY] + mo->radius;

        x2 = mo->pos[VX] + mo->radius;
        y2 = mo->pos[VY] - mo->radius;
    }
    else
    {
        x1 = mo->pos[VX] - mo->radius;
        y1 = mo->pos[VY] - mo->radius;

        x2 = mo->pos[VX] + mo->radius;
        y2 = mo->pos[VY] + mo->radius;
    }

    s[0] = P_PointOnDivlineSide(x1, y1, &traceLOS);
    s[1] = P_PointOnDivlineSide(x2, y2, &traceLOS);
    if(s[0] == s[1])
        return true; // Line isn't crossed.

    dl.pos[VX] = FLT2FIX(x1);
    dl.pos[VY] = FLT2FIX(y1);
    dl.dX = FLT2FIX(x2 - x1);
    dl.dY = FLT2FIX(y2 - y1);

    frac = P_InterceptVector(&traceLOS, &dl);
    if(frac < 0)
        return true; // Behind source.

    P_AddIntercept(frac, false, mo);

    return true; // Keep going.
}

boolean MobjBlockmap_PathTraverse(ThingBlockmap* blockmap, const uint originBlock[2],
                                  const uint destBlock[2], const float origin[2],
                                  const float dest[2],
                                  boolean (*func) (intercept_t*))
{
    uint count, block[2];
    float delta[2], partial;
    dfixed intercept[2], step[2];
    int stepDir[2];

    if(destBlock[0] > originBlock[0])
    {
        stepDir[0] = 1;
        partial = FIX2FLT(FRACUNIT - ((FLT2FIX(origin[0]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
        delta[1] = (dest[1] - origin[1]) / fabs(dest[0] - origin[0]);
    }
    else if(destBlock[0] < originBlock[0])
    {
        stepDir[0] = -1;
        partial = FIX2FLT((FLT2FIX(origin[0]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[1] = (dest[1] - origin[1]) / fabs(dest[0] - origin[0]);
    }
    else
    {
        stepDir[0] = 0;
        partial = 1;
        delta[1] = 256;
    }
    intercept[1] = (FLT2FIX(origin[1]) >> MAPBTOFRAC) +
        FLT2FIX(partial * delta[1]);

    if(destBlock[1] > originBlock[1])
    {
        stepDir[1] = 1;
        partial = FIX2FLT(FRACUNIT - ((FLT2FIX(origin[1]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
        delta[0] = (dest[0] - origin[0]) / fabs(dest[1] - origin[1]);
    }
    else if(destBlock[1] < originBlock[1])
    {
        stepDir[1] = -1;
        partial = FIX2FLT((FLT2FIX(origin[1]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[0] = (dest[0] - origin[0]) / fabs(dest[1] - origin[1]);
    }
    else
    {
        stepDir[1] = 0;
        partial = 1;
        delta[0] = 256;
    }
    intercept[0] = (FLT2FIX(origin[0]) >> MAPBTOFRAC) +
        FLT2FIX(partial * delta[0]);

    /**
     * Step through map blocks.
     *
     * Count is present because DOOM fails to check the case where the ray
     * passes diagonally through a vertex into another block.
     *
     * @todo Fix the problem. This will require a game-side copy of the blockmap
     * where the original behaviour is emulated.
     */
    block[0] = originBlock[0];
    block[1] = originBlock[1];
    step[0] = FLT2FIX(delta[0]);
    step[1] = FLT2FIX(delta[1]);
    for(count = 0; count < 64; ++count)
    {
        if(!MobjBlockmap_Iterate(blockmap, block, addMobjIntercepts, 0))
            return false; // Early out.

        if(block[0] == destBlock[0] && block[1] == destBlock[1])
            break;

        if((unsigned) (intercept[1] >> FRACBITS) == block[1])
        {
            intercept[1] += step[1];
            block[0] += stepDir[0];
        }
        else if((unsigned) (intercept[0] >> FRACBITS) == block[0])
        {
            intercept[0] += step[0];
            block[1] += stepDir[1];
        }
    }

    return true;
}
