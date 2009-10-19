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

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_play.h"

#include "linedefblockmap.h"

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
    boolean        (*func) (linedef_t*, void*);
    boolean         retObjRecord;
    int             localValidCount;
    void*           context;
} iteratelinedefs_args_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static linedefblockmap_t* allocBlockmap(void)
{
    return Z_Calloc(sizeof(linedefblockmap_t), PU_STATIC, 0);
}

static void freeBlockmap(linedefblockmap_t* bmap)
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

static void listPushFront(linklist_t* list, linedef_t* lineDef)
{
    listnode_t* node = allocListNode();
    
    node->data = lineDef;

    node->next = list->head;
    list->head = node;

    list->size += 1;
}

static boolean listRemove(linklist_t* list, linedef_t* lineDef)
{
    if(list->head)
    {
        listnode_t** node = &list->head;

        do
        {
            listnode_t** next = &(*node)->next;

            if((*node)->data == lineDef)
            {
                freeListNode(*node);
                *node = *next;
                list->size -= 1;
                return true; // LineDef was unlinked.
            }

            node = next;
        } while(*node);
    }

    return false; // LineDef was not linked.
}

static uint listSize(linklist_t* list)
{
    return list->size;
}

static boolean listSearch(linklist_t* list, linedef_t* lineDef)
{
    if(list->head)
    {
        listnode_t** node = &list->head;

        do
        {
            listnode_t** next = &(*node)->next;

            if((*node)->data == lineDef)
                return true;

            node = next;
        } while(*node);
    }

    return false;
}

static void linkLineDefToBlock(linedefblockmap_t* blockmap, uint x, uint y,
                               linedef_t* lineDef)
{
    linklist_t* list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);

    if(!list)
        list = Gridmap_SetBlock(blockmap->gridmap, x, y, allocList());

    listPushFront(list, lineDef);
}

static boolean unlinkLineDefFromBlock(linedefblockmap_t* blockmap, uint x, uint y,
                                      linedef_t* lineDef)
{
    linklist_t* list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);

    if(list)
    {
        boolean result = listRemove(list, lineDef);
        if(result && !list->head)
        {
            freeList(list);
            Gridmap_SetBlock(blockmap->gridmap, x, y, NULL);
        }

        return result;
    }

    return false;
}

static boolean isLineDefLinkedToBlock(linedefblockmap_t* blockmap, uint x, uint y,
                                      linedef_t* lineDef)
{
    linklist_t* list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);
    if(list)
        return listSearch(list, lineDef);
    return false;
}

static boolean iterateLineDefs(void* ptr, void* context)
{
    linklist_t* list = (linklist_t*) ptr;
    iteratelinedefs_args_t* args = (iteratelinedefs_args_t*) context;

    if(list)
    {
        listnode_t* node = list->head;

        while(node)
        {
            listnode_t* next = node->next;

            if(node->data)
            {
                linedef_t* lineDef = (linedef_t*) node->data;

                if(lineDef->validCount != args->localValidCount)
                {
                    void* ptr;

                    lineDef->validCount = args->localValidCount;

                    if(args->retObjRecord)
                        ptr = (void*) P_ObjectRecord(DMU_LINEDEF, lineDef);
                    else
                        ptr = (void*) lineDef;

                    if(!args->func(ptr, args->context))
                        return false;
                }
            }

            node = next;
        }
    }

    return true;
}

static boolean freeLineDefBlockData(void* data, void* context)
{
    linklist_t* list = (linklist_t*) data;

    if(list)
        freeList(list);

    return true; // Continue iteration.
}

static void boxToBlocks(linedefblockmap_t* bmap, uint blockBox[4], const arvec2_t box)
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
           (blockmap->aabb[1][0] - blockmap->aabb[0][0]) / width,
           (blockmap->aabb[1][1] - blockmap->aabb[0][1]) / height);

    blockmap->gridmap = M_CreateGridmap(width, height, PU_STATIC);

    return blockmap;
}

void P_DestroyLineDefBlockmap(linedefblockmap_t* blockmap)
{
    assert(blockmap);

    Gridmap_Iterate(blockmap->gridmap, freeLineDefBlockData, NULL);

    M_DestroyGridmap(blockmap->gridmap);
    Z_Free(blockmap);
}

void LineDefBlockmap_BoxToBlocks(linedefblockmap_t* blockmap, uint blockBox[4],
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
boolean LineDefBlockmap_Block2f(linedefblockmap_t* blockmap, uint dest[2], float x, float y)
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
boolean LineDefBlockmap_Block2fv(linedefblockmap_t* blockmap, uint dest[2], const float pos[2])
{
    return LineDefBlockmap_Block2f(blockmap, dest, pos[0], pos[1]);
}

static void tryLinkLineDefToBlock(linedefblockmap_t* blockmap, uint x, uint y, linedef_t* lineDef)
{
    if(isLineDefLinkedToBlock(blockmap, x, y, lineDef))
        return; // Already linked.

    linkLineDefToBlock(blockmap, x, y, lineDef);
}

void LineDefBlockmap_Link(linedefblockmap_t* blockmap, linedef_t* lineDef)
{
#define BLKSHIFT                7 // places to shift rel position for cell num
#define BLKMASK                 ((1<<BLKSHIFT)-1) // mask for rel position within cell

    uint i, blockBox[2][2], dimensions[2];
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

    aabb[0][0] = MIN_OF(vtx1[0], vtx2[0]);
    aabb[0][1] = MIN_OF(vtx1[1], vtx2[1]);

    aabb[1][0] = MAX_OF(vtx1[0], vtx2[0]);
    aabb[1][1] = MAX_OF(vtx1[1], vtx2[1]);

    LineDefBlockmap_Block2f(blockmap, blockBox[0], aabb[0][0], aabb[0][1]);
    LineDefBlockmap_Block2f(blockmap, blockBox[1], aabb[1][0], aabb[1][1]);

    delta[0] = vtx2[0] - vtx1[0];
    delta[1] = vtx2[1] - vtx1[1];

    vert = !delta[0];
    horiz = !delta[1];

    slopePos = (delta[0] ^ delta[1]) > 0;
    slopeNeg = (delta[0] ^ delta[1]) < 0;

    // The lineDef always belongs to the blocks containing its endpoints
    linkLineDefToBlock(blockmap, blockBox[0][0], blockBox[0][1], lineDef);
    if(blockBox[0][0] != blockBox[1][0] || blockBox[0][1] != blockBox[1][1])
        linkLineDefToBlock(blockmap, blockBox[1][0], blockBox[1][1], lineDef);

    // For each column, see where the lineDef along its left edge, which
    // it contains, intersects the LineDef. We don't want to interesect
    // vertical lines with columns.
    if(!vert)
    {
        for(i = blockBox[0][0]; i <= blockBox[1][0]; ++i)
        {
            // intersection of LineDef with x=origin[0]+(i<<BLKSHIFT)
            // (y-vtx1[1])*delta[0] = delta[1]*(x-vtx1[0])
            // y = delta[1]*(x-vtx1[0])+vtx1[1]*delta[0];
            int x = origin[0] + (i << BLKSHIFT);
            int y, yb, yp;

            // Does the lineDef touch this column at all?
            if(x < aabb[0][0] || x > aabb[1][0])
                continue;

            y = (delta[1] * (x - vtx1[0])) / delta[0] + vtx1[1];
            yb = (y - origin[1]) >> BLKSHIFT; // block row number

            // Outside the blockmap?
            if(yb < 0 || yb > (signed) (dimensions[1]) + 1)
                continue;

            // The cell that contains the intersection point is always added.
            tryLinkLineDefToBlock(blockmap, i, yb, lineDef);

            // If the intersection is at a corner it depends on the slope
            // (and whether the lineDef extends past the intersection) which
            // blocks are hit.

            // Where does the intersection occur?
            yp = (y - origin[1]) & BLKMASK; // y position within block
            if(yp == 0)
            {
                // Intersection occured at a corner
                if(slopeNeg) //   \ - blocks x,y-, x-,y
                {
                    if(yb > 0 && aabb[0][1] < y)
                        tryLinkLineDefToBlock(blockmap, i, yb - 1, lineDef);

                    if(i > 0 && aabb[0][0] < x)
                        tryLinkLineDefToBlock(blockmap, i - 1, yb, lineDef);
                }
                else if(slopePos) //   / - block x-,y-
                {
                    if(yb > 0 && i > 0 && aabb[0][0] < x)
                        tryLinkLineDefToBlock(blockmap, i - 1, yb - 1, lineDef);
                }
                else if(horiz) //   - - block x-,y
                {
                    if(i > 0 && aabb[0][0] < x)
                        tryLinkLineDefToBlock(blockmap, i - 1, yb, lineDef);
                }
            }
            else if(i > 0 && aabb[0][0] < x)
            {
                // Else not at corner: x-,y
                tryLinkLineDefToBlock(blockmap, i - 1, yb, lineDef);
            }
        }
    }

    // For each row, see where the lineDef along its bottom edge, which
    // it contains, intersects the LineDef.
    if(!horiz)
    {
        for(i = blockBox[0][1]; i <= blockBox[1][1]; ++i)
        {
            // intersection of LineDef with y=origin[1]+(i<<BLKSHIFT)
            // (x,y) on LineDef satisfies: (y-vtx1[1])*delta[0] = delta[1]*(x-vtx1[0])
            // x = delta[0]*(y-vtx1[1])/delta[1]+vtx1[0];
            int y = origin[1] + (i << BLKSHIFT); // (x,y) is intersection
            int x, xb, xp;

            // Touches this row?
            if(y < aabb[0][1] || y > aabb[1][1])
                continue;

            x = (delta[0] * (y - vtx1[1])) / delta[1] + vtx1[0];
            xb = (x - origin[0]) >> BLKSHIFT; // block column number

            // Outside the blockmap?
            if(xb < 0 || xb > (signed) (dimensions[0]) + 1)
                continue;

            tryLinkLineDefToBlock(blockmap, xb, i, lineDef);

            // If the intersection is at a corner it depends on the slope
            // (and whether the lineDef extends past the intersection) which
            // blocks are hit

            // Where does the intersection occur?
            xp = (x - origin[0]) & BLKMASK; // x position within block
            if(xp == 0)
            {
                // Intersection occured at a corner
                if(slopeNeg) //   \ - blocks x,y-, x-,y
                {
                    if(i > 0 && aabb[0][1] < y)
                        tryLinkLineDefToBlock(blockmap, xb, i - 1, lineDef);

                    if(xb > 0 && aabb[0][0] < x)
                        tryLinkLineDefToBlock(blockmap, xb - 1, i, lineDef);
                }
                else if(vert) //   | - block x,y-
                {
                    if(i > 0 && aabb[0][1] < y)
                        tryLinkLineDefToBlock(blockmap, xb, i - 1, lineDef);
                }
                else if(slopePos) //   / - block x-,y-
                {
                    if(xb > 0 && i > 0 && aabb[0][1] < y)
                        tryLinkLineDefToBlock(blockmap, xb - 1, i - 1, lineDef);
                }
            }
            else if(i > 0 && aabb[0][1] < y)
            {
                // Else not on a corner: x, y-
                tryLinkLineDefToBlock(blockmap, xb, i - 1, lineDef);
            }
        }
    }

#undef BLKSHIFT
#undef BLKMASK
}

boolean LineDefBlockmap_Unlink(linedefblockmap_t* blockmap, linedef_t* lineDef)
{
    uint x, y, dimensions[2];

    assert(blockmap);
    assert(lineDef);

    Gridmap_Dimensions(blockmap->gridmap, dimensions);

    // @optimize Keep a record of linedef to block links.
    for(y = 0; y < dimensions[1]; ++y)
        for(x = 0; x < dimensions[0]; ++x)
            unlinkLineDefFromBlock(blockmap, x, y, lineDef);

    return true;    
}

void LineDefBlockmap_Bounds(linedefblockmap_t* blockmap, pvec2_t min, pvec2_t max)
{
    assert(blockmap);

    if(min)
        V2_Copy(min, blockmap->aabb[0]);
    if(max)
        V2_Copy(max, blockmap->aabb[1]);
}

void LineDefBlockmap_BlockSize(linedefblockmap_t* blockmap, pvec2_t blockSize)
{
    assert(blockmap);
    assert(blockSize);

    V2_Copy(blockSize, blockmap->blockSize);
}

void LineDefBlockmap_Dimensions(linedefblockmap_t* blockmap, uint v[2])
{
    assert(blockmap);

    Gridmap_Dimensions(blockmap->gridmap, v);
}

uint LineDefBlockmap_NumInBlock(linedefblockmap_t* blockmap, uint x, uint y)
{
    linklist_t* list;

    assert(blockmap);

    list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);
    if(list)
        return listSize(list);

    return 0;
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

    return iterateLineDefs(Gridmap_Block(blockmap->gridmap, block[0], block[1]),
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

boolean LineDefBlockmap_PathTraverse(linedefblockmap_t* blockmap, const uint originBlock[2],
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

    /**
     * Step through map blocks.
     *
     * Count is present because DOOM fails to check the case where the ray
     * passes diagonally through a vertex into another block.
     *
     * @todo Fix the problem. We only need to retain the original behaviour
     * with the mobj blockmap in DOOM, Heretic and Hexen.
     */
    block[0] = originBlock[0];
    block[1] = originBlock[1];
    step[0] = FLT2FIX(delta[0]);
    step[1] = FLT2FIX(delta[1]);
    for(count = 0; count < 64; ++count)
    {
        if(!LineDefBlockmap_Iterate(blockmap, block, PIT_AddLineIntercepts, 0, false))
            return false; // Early out

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
