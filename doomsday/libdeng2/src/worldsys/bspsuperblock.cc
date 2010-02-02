/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

#include "de/BSPNode"
#include "de/BSPEdge"
#include "de/BSPSuperBlock"
#include "de/Log"

using namespace de;

namespace de
{
    // Smallest distance between two points before being considered equal.
    #define DIST_EPSILON        (1.0 / 128.0)

    typedef struct superblock_listnode_s {
        hedge_t*        hEdge;
        struct superblock_listnode_s* next;
    } superblock_listnode_t;

    typedef struct evalinfo_s {
        dint         cost;
        dint         splits;
        dint         iffy;
        dint         nearMiss;
        dint         realLeft;
        dint         realRight;
        dint         miniLeft;
        dint         miniRight;
    } evalinfo_t;
}

static superblock_listnode_t* allocListNode(void)
{
    return reinterpret_cast<superblock_listnode_t*>(std::malloc(sizeof(superblock_listnode_t)));
}

static void freeListNode(superblock_listnode_t* node)
{
    std::free(node);
}

/**
 * Free all memory allocated for the specified superblock.
 */
static void freeSuperBlock(superblock_t* block)
{
    // Recursively handle sub-blocks.
    for(duint num = 0; num < 2; ++num)
    {
        if(block->subs[num])
            freeSuperBlock(block->subs[num]);
    }
    std::free(block);
}

superblock_t* BSP_CreateSuperBlock(void)
{
    return reinterpret_cast<superblock_t*>(std::calloc(1, sizeof(superblock_t)));
}

void BSP_DestroySuperBlock(superblock_t* block)
{
    assert(block);
    freeSuperBlock(block);
}

/**
 * Link the given half-edge into the given SuperBlock.
 */
void SuperBlock_PushHEdge(superblock_t* block, hedge_t* hEdge)
{
    superblock_listnode_t* node;

    assert(block);
    assert(hEdge);

#if _DEBUG
// Ensure hedge is not already in this SuperBlock.
if((node = block->_hEdges))
{
    do
    {
        assert(node->hEdge != hEdge);
    } while((node = node->next));
}
#endif

    node = allocListNode();
    node->hEdge = hEdge;
    node->next = block->_hEdges;
    block->_hEdges = node;
}

hedge_t* SuperBlock_PopHEdge(superblock_t* block)
{
    assert(block);
    if(block->_hEdges)
    {
        superblock_listnode_t* node = block->_hEdges;
        hedge_t* hEdge = node->hEdge;
        block->_hEdges = node->next;
        freeListNode(node);
        return hEdge;
    }
    return NULL;
}

/**
 * Increase the counts within the SuperBlock, to account for the given
 * half-edge being split.
 */
void SuperBlock_IncHEdgeCounts(superblock_t* block, bool lineLinked)
{
    do
    {
        if(lineLinked)
            block->realNum++;
        else
            block->miniNum++;
        block = block->parent;
    } while(block != NULL);
}

/**
 * To be able to divide the nodes down, evalPartition must decide which is
 * the best half-edge to use as a nodeline. It does this by selecting the
 * line with least splits and has least difference of hald-edges on either
 * side of it.
 *
 * @return              @c true, if a "bad half-edge" was found early.
 */
static dint evalPartitionWorker(const superblock_t* hEdgeList,
                               hedge_t* partHEdge, dint factor, dint bestCost,
                               evalinfo_t* info)
{
#define ADD_LEFT()  \
      do {  \
        if (other->lineDef) info->realLeft += 1;  \
        else                info->miniLeft += 1;  \
      } while (0)

#define ADD_RIGHT()  \
      do {  \
        if (other->lineDef) info->realRight += 1;  \
        else                info->miniRight += 1;  \
      } while (0)

    superblock_listnode_t* n;
    ddouble qnty, a, b, fa, fb;
    hedge_info_t* part = (hedge_info_t*) partHEdge->data;
    dint num;

    /**
     * This is the heart of my SuperBlock idea, it tests the _whole_ block
     * against the partition line to quickly handle all the half-edges
     * within it at once. Only when the partition line intercepts the box do
     * we need to go deeper into it - AJA.
     */
    num = P_BoxOnLineSide3(hEdgeList->bbox, partHEdge->vertex->pos[VX],
                           partHEdge->vertex->pos[VY],
                           part->pDX, part->pDY, part->pPerp,
                           part->pLength, DIST_EPSILON);
    if(num < 0)
    {   // Left.
        info->realLeft += hEdgeList->realNum;
        info->miniLeft += hEdgeList->miniNum;

        return false;
    }
    else if(num > 0)
    {   // Right.
        info->realRight += hEdgeList->realNum;
        info->miniRight += hEdgeList->miniNum;

        return false;
    }

    // Check partition against all half-edges.
    for(n = hEdgeList->_hEdges; n; n = n->next)
    {
        hedge_t* otherHEdge = n->hEdge;
        hedge_info_t* other = (hedge_info_t*) otherHEdge->data;

        // This is the heart of my pruning idea - it catches
        // "bad half-edges" early on - LK.

        if(info->cost > bestCost)
            return true;

        // Get state of lines' relation to each other.
        if(other->sourceLine == part->sourceLine)
        {
            a = b = fa = fb = 0;
        }
        else
        {
            a = M_PerpDist(part->pDX, part->pDY, part->pPerp, part->pLength,
                           otherHEdge->vertex->pos[VX], otherHEdge->vertex->pos[VY]);
            b = M_PerpDist(part->pDX, part->pDY, part->pPerp, part->pLength,
                           otherHEdge->twin->vertex->pos[VX], otherHEdge->twin->vertex->pos[VY]);

            fa = fabs(a);
            fb = fabs(b);
        }

        // Check for being on the same line.
        if(fa <= DIST_EPSILON && fb <= DIST_EPSILON)
        {   // This half-edge runs along the same line as the partition.
            // Check whether it goes in the same direction or the opposite.
            if(other->pDX * part->pDX + other->pDY * part->pDY < 0)
            {
                ADD_LEFT();
            }
            else
            {
                ADD_RIGHT();
            }

            continue;
        }

        // Check for right side.
        if(a > -DIST_EPSILON && b > -DIST_EPSILON)
        {
            ADD_RIGHT();

            // Check for a near miss.
            if((a >= IFFY_LEN && b >= IFFY_LEN) ||
               (a <= DIST_EPSILON && b >= IFFY_LEN) ||
               (b <= DIST_EPSILON && a >= IFFY_LEN))
            {
                continue;
            }

            info->nearMiss++;

            /**
             * Near misses are bad, since they have the potential to cause
             * really short minihedges to be created in future processing.
             * Thus the closer the near miss, the higher the cost - AJA.
             */

            if(a <= DIST_EPSILON || b <= DIST_EPSILON)
                qnty = IFFY_LEN / de::max(a, b);
            else
                qnty = IFFY_LEN / de::min(a, b);

            info->cost += (dint) (100 * factor * (qnty * qnty - 1.0));
            continue;
        }

        // Check for left side.
        if(a < DIST_EPSILON && b < DIST_EPSILON)
        {
            ADD_LEFT();

            // Check for a near miss.
            if((a <= -IFFY_LEN && b <= -IFFY_LEN) ||
               (a >= -DIST_EPSILON && b <= -IFFY_LEN) ||
               (b >= -DIST_EPSILON && a <= -IFFY_LEN))
            {
                continue;
            }

            info->nearMiss++;

            // The closer the miss, the higher the cost (see note above).
            if(a >= -DIST_EPSILON || b >= -DIST_EPSILON)
                qnty = IFFY_LEN / -de::min(a, b);
            else
                qnty = IFFY_LEN / -de::max(a, b);

            info->cost += (dint) (70 * factor * (qnty * qnty - 1.0));
            continue;
        }

        /**
         * When we reach here, we have a and b non-zero and opposite sign,
         * hence this half-edge will be split by the partition line.
         */

        info->splits++;
        info->cost += 100 * factor;

        /**
         * Check if the split point is very close to one end, which is quite
         * an undesirable situation (producing really short edges). This is
         * perhaps _one_ source of those darn slime trails. Hence the name
         * "IFFY segs", and a rather hefty surcharge - AJA.
         */
        if(fa < IFFY_LEN || fb < IFFY_LEN)
        {
            info->iffy++;

            // The closer to the end, the higher the cost.
            qnty = IFFY_LEN / de::min(fa, fb);
            info->cost += (dint) (140 * factor * (qnty * qnty - 1.0));
        }
    }

    // Handle sub-blocks recursively.
    for(num = 0; num < 2; ++num)
    {
        if(!hEdgeList->subs[num])
            continue;

        if(evalPartitionWorker(hEdgeList->subs[num], partHEdge, factor, bestCost, info))
            return true;
    }

    // No "bad half-edge" was found. Good.
    return false;

#undef ADD_LEFT
#undef ADD_RIGHT
}

/**
 * Evaluate a partition and determine the cost, taking into account the
 * number of splits and the difference between left and right.
 *
 * @return              The computed cost, or a negative value if the edge
 *                      should be skipped altogether.
 */
static dint evalPartition(const superblock_t* hEdgeList, hedge_t* part,
                          dint factor, dint bestCost)
{
    hedge_info_t* data = (hedge_info_t*) part->data;
    evalinfo_t info;

    // Initialize info structure.
    info.cost   = 0;
    info.splits = 0;
    info.iffy   = 0;
    info.nearMiss  = 0;

    info.realLeft  = 0;
    info.realRight = 0;
    info.miniLeft  = 0;
    info.miniRight = 0;

    if(evalPartitionWorker(hEdgeList, part, factor, bestCost, &info))
        return -1;

    // Make sure there is at least one real seg on each side.
    if(!info.realLeft || !info.realRight)
    {
/*#if _DEBUG
Con_Message("Eval : No real half-edges on %s%sside\n",
            (info.realLeft? "" : "left "),
            (info.realRight? "" : "right "));
#endif*/

        return -1;
    }

    // Increase cost by the difference between left and right.
    info.cost += 100 * de::abs(info.realLeft - info.realRight);

    // Allow minihedge counts to affect the outcome, but only to a lesser
    // degree than real half-edges - AJA.
    info.cost += 50 * de::abs(info.miniLeft - info.miniRight);

    // Another little twist, here we show a slight preference for partition
    // lines that lie either purely horizontally or purely vertically - AJA.
    if(data->pDX != 0 && data->pDY != 0)
        info.cost += 25;

/*#if _DEBUG
Con_Message("Eval %p: splits=%d iffy=%d near=%d left=%d+%d right=%d+%d "
            "cost=%d.%02d\n", part, info.splits, info.iffy, info.nearMiss,
            info.realLeft, info.miniLeft, info.realRight, info.miniRight,
            info.cost / 100, info.cost % 100);
#endif*/

    return info.cost;
}

/**
 * @return              @c false, if cancelled.
 */
static bool pickHEdgeWorker(const superblock_t* partList,
                               const superblock_t* hEdgeList,
                               dint factor, hedge_t** best, dint* bestCost)
{
    dint num, cost;
    superblock_listnode_t* n;

    // Test each half-edge as a potential partition.
    for(n = partList->_hEdges; n; n = n->next)
    {
        hedge_t* hEdge = n->hEdge;
        hedge_info_t* data = (hedge_info_t*) hEdge->data;

/*#if _DEBUG
Con_Message("BSP_PickHEdge: %sSEG %p sector=%d  (%1.1f,%1.1f) -> "
            "(%1.1f,%1.1f)\n", (data->lineDef? "" : "MINI"), hEdge,
            (data->sector? data->sector->index : -1),
            hEdge->v[0]->V_pos[VX], hEdge->v[0]->V_pos[VY],
            hEdge->v[1]->V_pos[VX], hEdge->v[1]->V_pos[VY]);
#endif*/

        // Ignore minihedges as partition candidates.
        if(!data->lineDef)
            continue;

        if(data->lineDef && !data->sector)
            continue;

        // Only test half-edges from the same linedef once per round of
        // partition picking (they are collinear).
        if(data->lineDef->validCount == validCount)
            continue;
        data->lineDef->validCount = validCount;

        cost = evalPartition(hEdgeList, hEdge, factor, *bestCost);

        // half-edge unsuitable or too costly?
        if(cost < 0 || cost >= *bestCost)
            continue;

        // We have a new better choice.
        (*bestCost) = cost;

        // Remember which half-edge.
        (*best) = hEdge;
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        if(partList->subs[num])
            pickHEdgeWorker(partList->subs[num], hEdgeList, factor, best, bestCost);
    }

    return true;
}

/**
 * Find the best half-edge in the list to use as a partition.
 *
 * @param hEdgeList     List of half-edges to choose from.
 *
 * @return              hedge_t to use as a partition iff one suitable was found.
 */
hedge_t* SuperBlock_PickPartition(const superblock_t* hEdgeList, dint factor)
{
    dint bestCost = INT_MAX;
    hedge_t* best = NULL;

    validCount++;
    if(false == pickHEdgeWorker(hEdgeList, hEdgeList, factor, &best, &bestCost))
    {
        // \hack BuildNodes will detect the cancellation.
        return NULL;
    }

/*if _DEBUG
if(best)
    Con_Message("BSP_PickPartition: Best has score %d.%02d  (%1.1f,%1.1f) -> "
                "(%1.1f,%1.1f)\n", bestCost / 100, bestCost % 100,
                best->v[0]->V_pos[VX], best->v[0]->V_pos[VY],
                best->v[1]->V_pos[VX], best->v[1]->V_pos[VY]);
else
    Con_Message("BSP_PickPartition: No best found!\n");
#endif*/

    // Finished, return the best partition if found.
    return best;
}

static void findLimitWorker(const superblock_t* block, dfloat* bbox)
{
    duint num;
    superblock_listnode_t* n;

    for(n = block->_hEdges; n; n = n->next)
    {
        hedge_t* cur = n->hEdge;
        ddouble x1 = cur->vertex->pos[VX];
        ddouble y1 = cur->vertex->pos[VY];
        ddouble x2 = cur->twin->vertex->pos[VX];
        ddouble y2 = cur->twin->vertex->pos[VY];
        dfloat lx = (dfloat) de::min(x1, x2);
        dfloat ly = (dfloat) de::min(y1, y2);
        dfloat hx = (dfloat) de::max(x1, x2);
        dfloat hy = (dfloat) de::max(y1, y2);

        if(lx < bbox[superblock_t::BOXLEFT])
            bbox[superblock_t::BOXLEFT] = lx;
        else if(lx > bbox[superblock_t::BOXRIGHT])
            bbox[superblock_t::BOXRIGHT] = lx;
        if(ly < bbox[superblock_t::BOXBOTTOM])
            bbox[superblock_t::BOXBOTTOM] = ly;
        else if(ly > bbox[superblock_t::BOXTOP])
            bbox[superblock_t::BOXTOP] = ly;

        if(hx < bbox[superblock_t::BOXLEFT])
            bbox[superblock_t::BOXLEFT] = hx;
        else if(hx > bbox[superblock_t::BOXRIGHT])
            bbox[superblock_t::BOXRIGHT] = hx;
        if(hy < bbox[superblock_t::BOXBOTTOM])
            bbox[superblock_t::BOXBOTTOM] = hy;
        else if(hy > bbox[superblock_t::BOXTOP])
            bbox[superblock_t::BOXTOP] = hy;
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        if(block->subs[num])
            findLimitWorker(block->subs[num], bbox);
    }
}

/**
 * Find the extremes of an axis aligned box containing all half-edges.
 */
void BSP_FindAABBForHEdges(const superblock_t* hEdgeList, dfloat* bbox)
{
    bbox[superblock_t::BOXTOP] = bbox[superblock_t::BOXRIGHT] = MINFLOAT;
    bbox[superblock_t::BOXBOTTOM] = bbox[superblock_t::BOXLEFT] = MAXFLOAT;
    findLimitWorker(hEdgeList, bbox);
}

/**
 * For debugging.
 */
#if _DEBUG
void de::SuperBlock_PrintHEdges(superblock_t* block)
{
    const superblock_listnode_t* n;
    dint num;

    for(n = block->_hEdges; n; n = n->next)
    {
        const hedge_t* hEdge = n->hEdge;
        const hedge_info_t* data = reinterpret_cast<hedge_info_t*>(hEdge->data);

        LOG_DEBUG("Build: %s %p sector=%d (%1.1f,%1.1f) -> (%1.1f,%1.1f)")
            << (data->lineDef? "NORM" : "MINI") << hEdge << data->sector->buildData.index
            << (dfloat) hEdge->vertex->pos[VX] << (dfloat) hEdge->vertex->pos[VY]
            << (dfloat) hEdge->twin->vertex->pos[VX] << (dfloat) hEdge->twin->vertex->pos[VY];
    }

    for(num = 0; num < 2; ++num)
    {
        if(block->subs[num])
            SuperBlock_PrintHEdges(block->subs[num]);
    }
}

static void testSuperWorker(superblock_t* block, duint* real, duint* mini)
{
    const superblock_listnode_t* n;
    duint num;

    for(n = block->_hEdges; n; n = n->next)
    {
        const hedge_t* cur = n->hEdge;

        if(((hedge_info_t*) cur->data)->lineDef)
            (*real) += 1;
        else
            (*mini) += 1;
    }

    for(num = 0; num < 2; ++num)
    {
        if(block->subs[num])
            testSuperWorker(block->subs[num], real, mini);
    }
}

/**
 * For debugging.
 */
void testSuper(superblock_t* block)
{
    duint realNum = 0, miniNum = 0;
    testSuperWorker(block, &realNum, &miniNum);
    assert(realNum == block->realNum);
    assert(miniNum == block->miniNum);
}
#endif
