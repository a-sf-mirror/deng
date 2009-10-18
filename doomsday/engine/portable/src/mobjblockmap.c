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

typedef struct linkmobj_s {
    struct linkmobj_s* next;
    struct mobj_s* mobj;
} linkmobj_t;

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

static void boxToBlocks(mobjblockmap_t* bmap, uint blockBox[4], const arvec2_t box)
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
