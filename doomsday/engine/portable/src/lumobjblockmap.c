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

#include "r_lumobjs.h"
#include "lumobjblockmap.h"

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
    boolean       (*func) (lumobj_t*, void*);
    void*           context;
} iterateparticles_args_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static lumobjblockmap_t* allocBlockmap(void)
{
    return Z_Calloc(sizeof(lumobjblockmap_t), PU_STATIC, 0);
}

static void freeBlockmap(lumobjblockmap_t* bmap)
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

static void listPushFront(linklist_t* list, lumobj_t* lumobj)
{
    listnode_t* node = allocListNode();

    node->data = lumobj;

    node->next = list->head;
    list->head = node;

    list->size += 1;
}

static boolean listRemove(linklist_t* list, lumobj_t* lumobj)
{
    if(list->head)
    {
        listnode_t** node = &list->head;

        do
        {
            listnode_t** next = &(*node)->next;

            if((*node)->data == lumobj)
            {
                freeListNode(*node);
                *node = *next;
                list->size -= 1;
                return true; // Was unlinked.
            }

            node = next;
        } while(*node);
    }

    return false; // Was not linked.
}

static uint listSize(linklist_t* list)
{
    return list->size;
}

static void linkLumobjToBlock(lumobjblockmap_t* blockmap, uint x, uint y,
                                lumobj_t* lumobj)
{
    linklist_t* list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);

    if(!list)
        list = Gridmap_SetBlock(blockmap->gridmap, x, y, allocList());

    listPushFront(list, lumobj);
}

static boolean unlinkLumobjFromBlock(lumobjblockmap_t* blockmap, uint x, uint y,
                                       lumobj_t* lumobj)
{
    linklist_t* list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);

    if(list)
    {
        boolean result = listRemove(list, lumobj);
        if(result && !list->head)
        {
            freeList(list);
            Gridmap_SetBlock(blockmap->gridmap, x, y, NULL);
        }

        return result;
    }

    return false;
}

static boolean iterateLumobjs(void* ptr, void* context)
{
    linklist_t* list = (linklist_t*) ptr;
    iterateparticles_args_t* args = (iterateparticles_args_t*) context;

    if(list)
    {
        listnode_t* node = list->head;

        while(node)
        {
            listnode_t* next = node->next;

            if(node->data)
                if(!args->func((lumobj_t*) node->data, args->context))
                    return false;

            node = next;
        }
    }

    return true;
}

static boolean freeLumobjBlockData(void* data, void* context)
{
    linklist_t* list = (linklist_t*) data;

    if(list)
        freeList(list);

    return true; // Continue iteration.
}

static void boxToBlocks(lumobjblockmap_t* bmap, uint blockBox[4], const arvec2_t box)
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

lumobjblockmap_t* P_CreateLumobjBlockmap(const pvec2_t min, const pvec2_t max,
                                             uint width, uint height)
{
    lumobjblockmap_t* blockmap;

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

void P_DestroyLumobjBlockmap(lumobjblockmap_t* blockmap)
{
    assert(blockmap);

    Gridmap_Iterate(blockmap->gridmap, freeLumobjBlockData, NULL);

    M_DestroyGridmap(blockmap->gridmap);
    Z_Free(blockmap);
}

void LumobjBlockmap_Empty(lumobjblockmap_t* blockmap)
{
    assert(blockmap);
    Gridmap_Iterate(blockmap->gridmap, freeLumobjBlockData, NULL);
    Gridmap_Empty(blockmap->gridmap);
}

void LumobjBlockmap_BoxToBlocks(lumobjblockmap_t* blockmap, uint blockBox[4],
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
boolean LumobjBlockmap_Block2f(lumobjblockmap_t* blockmap, uint dest[2], float x, float y)
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
boolean LumobjBlockmap_Block2fv(lumobjblockmap_t* blockmap, uint dest[2], const float pos[2])
{
    return LumobjBlockmap_Block2f(blockmap, dest, pos[0], pos[1]);
}

void LumobjBlockmap_Link(lumobjblockmap_t* blockmap, lumobj_t* lumobj)
{
    uint block[2], dimensions[2];

    assert(blockmap);
    assert(lumobj);

    Gridmap_Dimensions(blockmap->gridmap, dimensions);

    // Blockcoords to link to.
    if(LumobjBlockmap_Block2fv(blockmap, block, lumobj->pos))
        linkLumobjToBlock(blockmap, block[0], block[1], lumobj);
}

boolean LumobjBlockmap_Unlink(lumobjblockmap_t* blockmap, lumobj_t* lumobj)
{
    uint x, y, dimensions[2];

    assert(blockmap);
    assert(lumobj);

    Gridmap_Dimensions(blockmap->gridmap, dimensions);

    // @optimize Keep a record of lumobj to block links.
    for(y = 0; y < dimensions[1]; ++y)
        for(x = 0; x < dimensions[0]; ++x)
            unlinkLumobjFromBlock(blockmap, x, y, lumobj);

    return true;
}

void LumobjBlockmap_Bounds(lumobjblockmap_t* blockmap, pvec2_t min, pvec2_t max)
{
    assert(blockmap);

    if(min)
        V2_Copy(min, blockmap->aabb[0]);
    if(max)
        V2_Copy(max, blockmap->aabb[1]);
}

void LumobjBlockmap_BlockSize(lumobjblockmap_t* blockmap, pvec2_t blockSize)
{
    assert(blockmap);
    assert(blockSize);

    V2_Copy(blockSize, blockmap->blockSize);
}

void LumobjBlockmap_Dimensions(lumobjblockmap_t* blockmap, uint v[2])
{
    assert(blockmap);

    Gridmap_Dimensions(blockmap->gridmap, v);
}

uint LumobjBlockmap_NumInBlock(lumobjblockmap_t* blockmap, uint x, uint y)
{
    linklist_t* list;

    assert(blockmap);

    list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);
    if(list)
        return listSize(list);

    return 0;
}

boolean LumobjBlockmap_Iterate(lumobjblockmap_t* blockmap, const uint block[2],
                                 boolean (*func) (lumobj_t*, void*),
                                 void* context)
{
    iterateparticles_args_t args;

    assert(blockmap);
    assert(block);
    assert(func);

    args.func = func;
    args.context = context;

    return iterateLumobjs(Gridmap_Block(blockmap->gridmap, block[0], block[1]), &args);
}

boolean LumobjBlockmap_BoxIterate(lumobjblockmap_t* blockmap,
                                    const uint blockBox[4],
                                    boolean (*func) (lumobj_t*, void*),
                                    void* context)
{
    iterateparticles_args_t args;

    assert(blockmap);
    assert(blockBox);
    assert(func);

    args.func = func;
    args.context = context;

    return Gridmap_IterateBoxv(blockmap->gridmap, blockBox, iterateLumobjs,
                               (void*) &args);
}
