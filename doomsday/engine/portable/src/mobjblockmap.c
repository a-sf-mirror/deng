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
#include "de_play.h"

#include "mobjblockmap.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct linkmobj_s {
    struct linkmobj_s* next;
    struct mobj_s* mobj;
} linkmobj_t;

typedef struct {
    boolean       (*func) (mobj_t*, void *);
    int             localValidCount;
    void*           context;
} iteratemobjs_args_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static mobjblockmap_t* allocBlockmap(void)
{
    return Z_Calloc(sizeof(mobjblockmap_t), PU_STATIC, 0);
}

static void freeBlockmap(mobjblockmap_t* bmap)
{
    Z_Free(bmap);
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

static void boxToBlocks(mobjblockmap_t* bmap, uint blockBox[4], const arvec2_t box)
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
           (blockmap->aabb[1][0] - blockmap->aabb[0][0]) / width,
           (blockmap->aabb[1][1] - blockmap->aabb[0][1]) / height);

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

void MobjBlockmap_BoxToBlocks(mobjblockmap_t* blockmap, uint blockBox[4],
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
boolean MobjBlockmap_Block2fv(mobjblockmap_t* blockmap, uint dest[2], const float pos[2])
{
    return MobjBlockmap_Block2f(blockmap, dest, pos[0], pos[1]);
}

void MobjBlockmap_Link(mobjblockmap_t* blockmap, mobj_t* mo)
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

boolean MobjBlockmap_Unlink(mobjblockmap_t* blockmap, mobj_t* mo)
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

void MobjBlockmap_Bounds(mobjblockmap_t* blockmap, pvec2_t min, pvec2_t max)
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

    return iterateMobjs(Gridmap_Block(blockmap->gridmap, block[0], block[1]), (void*) &args);
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

boolean MobjBlockmap_PathTraverse(mobjblockmap_t* blockmap, const uint originBlock[2],
                                  const uint destBlock[2], const float origin[2],
                                  const float dest[2],
                                  boolean (*func) (intercept_t*))
{
    uint count, block[2];
    float delta[2], partial;
    fixed_t intercept[2], step[2];
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

    //
    // Step through map blocks.
    //

    // Count is present to prevent a round off error from skipping the
    // break and ending up in an infinite loop..
    block[0] = originBlock[0];
    block[1] = originBlock[1];
    step[0] = FLT2FIX(delta[0]);
    step[1] = FLT2FIX(delta[1]);
    for(count = 0; count < 64; ++count)
    {
        if(!MobjBlockmap_Iterate(blockmap, block, PIT_AddMobjIntercepts, 0))
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
