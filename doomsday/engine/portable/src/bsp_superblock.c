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

/**
 * bsp_superblock.c: GL-friendly BSP node builder. Half-edge blockmap.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

/**
 * \notes
 * To be able to divide the nodes down, evalPartition must decide which is
 * the best half-edge to use as a nodeline. It does this by selecting the
 * line with least splits and has least difference of hald-edges on either
 * side of it.
 *
 * Credit to Raphael Quinet and DEU, this algorithm is a copy of the
 * nodeline picker used in DEU5beta. I am using this method because the
 * method I (AJA) originally used was not so good.
 *
 * Rewritten by Lee Killough to significantly improve performance, while not
 * affecting results one bit in >99% of cases (some tiny differences due to
 * roundoff error may occur, but they are insignificant).
 *
 * Rewritten again by Andrew Apted (-AJA-), 1999-2000.
 * Rewritten yet again by Daniel Swanson, 2007-2008.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_play.h"
#include "de_misc.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct evalinfo_s {
    int         cost;
    int         splits;
    int         iffy;
    int         nearMiss;
    int         realLeft;
    int         realRight;
    int         miniLeft;
    int         miniRight;
} evalinfo_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static __inline void calcIntersection(const hedge_t* cur,
                                      const bspartition_t* part,
                                      double perpC, double perpD,
                                      double* x, double* y);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static superblock_t* quickAllocSupers;

// CODE --------------------------------------------------------------------

/**
 * Init the superblock allocator.
 */
void BSP_InitSuperBlockAllocator(void)
{
    quickAllocSupers = NULL;
}

/**
 * Free all the superblocks on the quick-alloc list.
 */
void BSP_ShutdownSuperBlockAllocator(void)
{
    while(quickAllocSupers)
    {
        superblock_t*   block = quickAllocSupers;

        quickAllocSupers = block->subs[0];
        M_Free(block);
    }
}

static hedge_node_t* allocHEdgeNode(void)
{
    return M_Malloc(sizeof(hedge_node_t));
}

static void freeHEdgeNode(hedge_node_t* node)
{
    M_Free(node);
}

/**
 * Link the given half-edge into the given bspleaf.
 */
void BSPLeaf_LinkHEdge(bspleafdata_t* leaf, hedge_t* hEdge)
{
    hedge_node_t*       node;

    if(!hEdge || !leaf)
        return;

#if _DEBUG
// Ensure hedge is not already in this leaf.
if((node = leaf->hEdges))
{
    do
    {
        if(node->hEdge == hEdge)
            Con_Error("BSPLeaf_LinkHEdge: HEdge %p already linked to "
                      "leaf %p!", hEdge, leaf);
    } while((node = node->next));
}
#endif

    node = allocHEdgeNode();
    node->hEdge = hEdge;

    node->next = leaf->hEdges;
    leaf->hEdges = node;
}

void BSPLeaf_UnLinkHEdge(bspleafdata_t* leaf, hedge_t* hEdge)
{
    hedge_node_t*       node;

    if(!hEdge || !leaf || !leaf->hEdges)
        return;

    if(leaf->hEdges->hEdge == hEdge)
    {
        node = leaf->hEdges;

        leaf->hEdges = leaf->hEdges->next;

        freeHEdgeNode(node);
        return;
    }

    node = leaf->hEdges;
    do
    {
        if(node->next && node->next->hEdge == hEdge)
        {
            hedge_node_t*       p = node->next;
            node->next = node->next->next;
            freeHEdgeNode(p);
            break;
        }
    } while((node = node->next));
}

/**
 * Acquire memory for a new superblock.
 */
static superblock_t* allocSuperBlock(void)
{
    superblock_t*       superblock;

    if(quickAllocSupers == NULL)
        return M_Calloc(sizeof(superblock_t));

    superblock = quickAllocSupers;
    quickAllocSupers = superblock->subs[0];

    // Clear out any old rubbish.
    memset(superblock, 0, sizeof(*superblock));

    return superblock;
}

/**
 * Free all memory allocated for the specified superblock.
 */
static void freeSuperBlock(superblock_t* superblock)
{
    uint                num;

    if(superblock->hEdges)
    {
        // This can happen, but only under abnormal circumstances.
#if _DEBUG
Con_Error("FreeSuper: Superblock contains half-edges!");
#endif
        superblock->hEdges = NULL;
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        if(superblock->subs[num])
            freeSuperBlock(superblock->subs[num]);
    }

    // Add block to quick-alloc list. Note that subs[0] is used for
    // linking the blocks together.
    superblock->subs[0] = quickAllocSupers;
    quickAllocSupers = superblock;
}

superblock_t* BSP_SuperBlockCreate(void)
{
    return allocSuperBlock();
}

void BSP_SuperBlockDestroy(superblock_t* superblock)
{
    freeSuperBlock(superblock);
}

/**
 * Link the given half-edge into the given superblock.
 */
void SuperBlock_LinkHEdge(superblock_t* superblock, hedge_t* hEdge)
{
    hedge_node_t*       node;

    if(!hEdge || !superblock)
        return;

#if _DEBUG
// Ensure hedge is not already in this superblock.
if((node = superblock->hEdges))
{
    do
    {
        if(node->hEdge == hEdge)
            Con_Error("SuperBlock_LinkHEdge: HEdge %p already linked to "
                      "superblock %p!", hEdge, superblock);
    } while((node = node->next));
}
#endif

    node = allocHEdgeNode();

    node->hEdge = hEdge;
    ((bsp_hedgeinfo_t*) hEdge->data)->block = superblock;

    node->next = superblock->hEdges;
    superblock->hEdges = node;
}

void SuperBlock_UnLinkHEdge(superblock_t* superblock, hedge_t* hEdge)
{
    hedge_node_t*       node;

    if(!hEdge || !superblock || !superblock->hEdges)
        return;

    if(superblock->hEdges->hEdge == hEdge)
    {
        node = superblock->hEdges;

        superblock->hEdges = superblock->hEdges->next;

        freeHEdgeNode(node);
        return;
    }

    node = superblock->hEdges;
    do
    {
        if(node->next && node->next->hEdge == hEdge)
        {
            hedge_node_t*       p = node->next;
            node->next = node->next->next;
            freeHEdgeNode(p);
            break;
        }
    } while((node = node->next));
}

/**
 * Increase the counts within the superblock, to account for the given
 * half-edge being split.
 */
void SuperBlock_IncHEdgeCounts(superblock_t* superblock, boolean lineLinked)
{
    do
    {
        if(lineLinked)
            superblock->realNum++;
        else
            superblock->miniNum++;

        superblock = superblock->parent;
    } while(superblock != NULL);
}

static void makeIntersection(cutlist_t* cutList, vertex_t* vert,
                             const bspartition_t* part, boolean selfRef)
{
    intersection_t*     cut = BSP_CutListFindIntersection(cutList, vert);

    if(!cut)
    {
        cut = BSP_IntersectionCreate(vert, part, selfRef);
        BSP_CutListInsertIntersection(cutList, cut);
    }
}

/**
 * Partition the given edge and perform any further necessary action (moving
 * it into either the left list, right list, or splitting it).
 *
 * Take the given half-edge 'cur', compare it with the partition line, and
 * determine it's fate: moving it into either the left or right lists
 * (perhaps both, when splitting it in two). Handles the twin as well.
 * Updates the intersection list if the half-edge lies on or crosses the
 * partition line.
 *
 * \note
 * I have rewritten this routine based on evalPartition() (which I've also
 * reworked, heavily). I think it is important that both these routines
 * follow the exact same logic when determining which half-edges should go
 * left, right or be split. - AJA
 */
void BSP_DivideOneHEdge(hedge_t* curHEdge, const bspartition_t* part,
                        superblock_t* rightList, superblock_t* leftList,
                        cutlist_t* cutList)
{
    hedge_t*            newHEdge;
    double              x, y;
    double              a, b;
    bsp_hedgeinfo_t*         data = ((bsp_hedgeinfo_t*) curHEdge->data);
    boolean             selfRef =
        (data->lineDef? (data->lineDef->buildData.mlFlags & MLF_SELFREF) : false);

    // Get state of lines' relation to each other.
    a = M_PerpDist(part->pDX, part->pDY, part->pPerp, part->length,
                   data->pSX, data->pSY);
    b = M_PerpDist(part->pDX, part->pDY, part->pPerp, part->length,
                   data->pEX, data->pEY);

    if(data->sourceLine == part->sourceLine)
        a = b = 0;

    // Check for being on the same line.
    if(fabs(a) <= DIST_EPSILON && fabs(b) <= DIST_EPSILON)
    {
        makeIntersection(cutList, curHEdge->v[0], part, selfRef);
        makeIntersection(cutList, curHEdge->v[1], part, selfRef);

        // This seg runs along the same line as the partition. Check
        // whether it goes in the same direction or the opposite.
        if(data->pDX * part->pDX + data->pDY * part->pDY < 0)
        {
            BSP_AddHEdgeToSuperBlock(leftList, curHEdge);
        }
        else
        {
            BSP_AddHEdgeToSuperBlock(rightList, curHEdge);
        }

        return;
    }

    // Check for right side.
    if(a > -DIST_EPSILON && b > -DIST_EPSILON)
    {
        if(a < DIST_EPSILON)
            makeIntersection(cutList, curHEdge->v[0], part, selfRef);
        else if(b < DIST_EPSILON)
            makeIntersection(cutList, curHEdge->v[1], part, selfRef);

        BSP_AddHEdgeToSuperBlock(rightList, curHEdge);
        return;
    }

    // Check for left side.
    if(a < DIST_EPSILON && b < DIST_EPSILON)
    {
        if(a > -DIST_EPSILON)
            makeIntersection(cutList, curHEdge->v[0], part, selfRef);
        else if(b > -DIST_EPSILON)
            makeIntersection(cutList, curHEdge->v[1], part, selfRef);

        BSP_AddHEdgeToSuperBlock(leftList, curHEdge);
        return;
    }

    // When we reach here, we have a and b non-zero and opposite sign,
    // hence this edge will be split by the partition line.

    calcIntersection(curHEdge, part, a, b, &x, &y);
    newHEdge = HEdge_Split(curHEdge, x, y);
    makeIntersection(cutList, curHEdge->v[1], part, selfRef);

    if(a < 0)
    {
        BSP_AddHEdgeToSuperBlock(leftList,  curHEdge);
        BSP_AddHEdgeToSuperBlock(rightList, newHEdge);
    }
    else
    {
        BSP_AddHEdgeToSuperBlock(rightList, curHEdge);
        BSP_AddHEdgeToSuperBlock(leftList,  newHEdge);
    }
}

static void partitionHEdges(superblock_t* hEdgeList,
                            const bspartition_t* part,
                            superblock_t* rights, superblock_t* lefts,
                            cutlist_t* cutList)
{
    uint                num;

    while(hEdgeList->hEdges)
    {
        hedge_t*            hEdge = hEdgeList->hEdges->hEdge;

        SuperBlock_UnLinkHEdge(hEdgeList, hEdge);
        ((bsp_hedgeinfo_t*) hEdge->data)->block = NULL;

        BSP_DivideOneHEdge(hEdge, part, rights, lefts, cutList);
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        superblock_t*       a = hEdgeList->subs[num];

        if(a)
        {
            partitionHEdges(a, part, rights, lefts, cutList);

            if(a->realNum + a->miniNum > 0)
                Con_Error("BSP_SeparateHEdges: child %d not empty!", num);

            BSP_SuperBlockDestroy(a);
            hEdgeList->subs[num] = NULL;
        }
    }

    hEdgeList->realNum = hEdgeList->miniNum = 0;
}

/**
 * Remove all the half-edges from the list, partitioning them into the left
 * or right lists based on the given partition line. Adds any intersections
 * onto the intersection list as it goes.
 */
void BSP_PartitionHEdges(superblock_t* hEdgeList, const bspartition_t* part,
                         superblock_t* rights, superblock_t* lefts,
                         cutlist_t* cutList)
{
    partitionHEdges(hEdgeList, part, rights, lefts, cutList);

    // Sanity checks...
    if(rights->realNum + rights->miniNum == 0)
        Con_Error("BuildNodes: Separated halfedge-list has no right side.");

    if(lefts->realNum + lefts->miniNum == 0)
        Con_Error("BuildNodes: Separated halfedge-list has no left side.");

    BSP_AddMiniHEdges(part, rights, lefts, cutList);
}

/**
 * @return              @c true, if a "bad half-edge" was found early.
 */
static int evalPartitionWorker(const superblock_t* hEdgeList,
                               hedge_t* partHEdge, int bestCost,
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

    int                 num, factor = bspFactor;
    hedge_node_t*       n;
    double              qnty, a, b, fa, fb;
    bsp_hedgeinfo_t*    part = (bsp_hedgeinfo_t*) partHEdge->data;

    /**
     * This is the heart of my superblock idea, it tests the _whole_ block
     * against the partition line to quickly handle all the half-edges
     * within it at once. Only when the partition line intercepts the box do
     * we need to go deeper into it - AJA.
     */
    num = P_BoxOnLineSide3(hEdgeList->bbox, part->pSX, part->pSY,
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
    for(n = hEdgeList->hEdges; n; n = n->next)
    {
        hedge_t*            otherHEdge = n->hEdge;
        bsp_hedgeinfo_t*    other = (bsp_hedgeinfo_t*) otherHEdge->data;

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
                           other->pSX, other->pSY);
            b = M_PerpDist(part->pDX, part->pDY, part->pPerp, part->pLength,
                           other->pEX, other->pEY);

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
                qnty = IFFY_LEN / MAX_OF(a, b);
            else
                qnty = IFFY_LEN / MIN_OF(a, b);

            info->cost += (int) (100 * factor * (qnty * qnty - 1.0));
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
                qnty = IFFY_LEN / -MIN_OF(a, b);
            else
                qnty = IFFY_LEN / -MAX_OF(a, b);

            info->cost += (int) (70 * factor * (qnty * qnty - 1.0));
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
            qnty = IFFY_LEN / MIN_OF(fa, fb);
            info->cost += (int) (140 * factor * (qnty * qnty - 1.0));
        }
    }

    // Handle sub-blocks recursively.
    for(num = 0; num < 2; ++num)
    {
        if(!hEdgeList->subs[num])
            continue;

        if(evalPartitionWorker(hEdgeList->subs[num], partHEdge, bestCost, info))
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
static int evalPartition(const superblock_t* hEdgeList, hedge_t* part,
                         int bestCost)
{
    evalinfo_t          info;
    bsp_hedgeinfo_t*         data = (bsp_hedgeinfo_t*) part->data;

    // Initialize info structure.
    info.cost   = 0;
    info.splits = 0;
    info.iffy   = 0;
    info.nearMiss  = 0;

    info.realLeft  = 0;
    info.realRight = 0;
    info.miniLeft  = 0;
    info.miniRight = 0;

    if(evalPartitionWorker(hEdgeList, part, bestCost, &info))
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
    info.cost += 100 * ABS(info.realLeft - info.realRight);

    // Allow minihedge counts to affect the outcome, but only to a lesser
    // degree than real half-edges - AJA.
    info.cost += 50 * ABS(info.miniLeft - info.miniRight);

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
static boolean pickHEdgeWorker(const superblock_t* partList,
                               const superblock_t* hEdgeList,
                               hedge_t** best, int* bestCost)
{
    int                 num, cost;
    hedge_node_t*       n;

    // Test each half-edge as a potential partition.
    for(n = partList->hEdges; n; n = n->next)
    {
        hedge_t*            hEdge = n->hEdge;
        bsp_hedgeinfo_t*    data = (bsp_hedgeinfo_t*) hEdge->data;

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

        // Only test half-edges from the same linedef once per round of
        // partition picking (they are collinear).
        if(data->lineDef->validCount == validCount)
            continue;
        data->lineDef->validCount = validCount;

        cost = evalPartition(hEdgeList, hEdge, *bestCost);

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
            pickHEdgeWorker(partList->subs[num], hEdgeList, best, bestCost);
    }

    return true;
}

/**
 * Find the best half-edge in the list to use as a partition.
 *
 * @param hEdgeList     List of half-edges to choose from.
 * @param depth         Current node depth.
 * @param partition     Ptr to partition to be updated with the results.
 *
 * @return              @c true, iff a suitable partition was found.
 */
boolean SuperBlock_PickPartition(const superblock_t* hEdgeList,
                                 size_t depth, bspartition_t* partition)
{
    int                 bestCost = INT_MAX;
    hedge_t*            best = NULL;

/*#if _DEBUG
Con_Message("BSP_PickPartition: Begun (depth %lu)\n", (unsigned long) depth);
#endif*/

    validCount++;
    if(false == pickHEdgeWorker(hEdgeList, hEdgeList, &best, &bestCost))
    {
        // \hack BuildNodes will detect the cancellation.
        return false;
    }

    // Finished, return the best partition.
    if(best)
    {
        bsp_hedgeinfo_t*         data = (bsp_hedgeinfo_t*) best->data;

/*if _DEBUG
Con_Message("BSP_PickPartition: Best has score %d.%02d  (%1.1f,%1.1f) -> "
            "(%1.1f,%1.1f)\n", bestCost / 100, bestCost % 100,
            best->v[0]->V_pos[VX], best->v[0]->V_pos[VY],
            best->v[1]->V_pos[VX], best->v[1]->V_pos[VY]);
#endif*/
        assert(data->lineDef);

        partition->x  = data->lineDef->buildData.v[data->side]->buildData.pos[VX];
        partition->y  = data->lineDef->buildData.v[data->side]->buildData.pos[VY];
        partition->dX = data->lineDef->buildData.v[data->side^1]->buildData.pos[VX] - partition->x;
        partition->dY = data->lineDef->buildData.v[data->side^1]->buildData.pos[VY] - partition->y;
        partition->lineDef = data->lineDef;
        partition->sourceLine = data->sourceLine;

        partition->pDX = data->pDX;
        partition->pDY = data->pDY;
        partition->pSX = data->pSX;
        partition->pSY = data->pSY;
        partition->pPara = data->pPara;
        partition->pPerp = data->pPerp;
        partition->length = data->pLength;
        return true;
    }

/*#if _DEBUG
Con_Message("BSP_PickPartition: No best found!\n");
#endif*/

    return false;
}

static void findLimitWorker(superblock_t* block, float* bbox)
{
    uint                num;
    hedge_node_t*       n;

    for(n = block->hEdges; n; n = n->next)
    {
        hedge_t*            cur = n->hEdge;
        double              x1 = cur->v[0]->buildData.pos[VX];
        double              y1 = cur->v[0]->buildData.pos[VY];
        double              x2 = cur->v[1]->buildData.pos[VX];
        double              y2 = cur->v[1]->buildData.pos[VY];
        float               lx = (float) MIN_OF(x1, x2);
        float               ly = (float) MIN_OF(y1, y2);
        float               hx = (float) MAX_OF(x1, x2);
        float               hy = (float) MAX_OF(y1, y2);

        if(lx < bbox[BOXLEFT])
            bbox[BOXLEFT] = lx;
        else if(lx > bbox[BOXRIGHT])
            bbox[BOXRIGHT] = lx;
        if(ly < bbox[BOXBOTTOM])
            bbox[BOXBOTTOM] = ly;
        else if(ly > bbox[BOXTOP])
            bbox[BOXTOP] = ly;

        if(hx < bbox[BOXLEFT])
            bbox[BOXLEFT] = hx;
        else if(hx > bbox[BOXRIGHT])
            bbox[BOXRIGHT] = hx;
        if(hy < bbox[BOXBOTTOM])
            bbox[BOXBOTTOM] = hy;
        else if(hy > bbox[BOXTOP])
            bbox[BOXTOP] = hy;
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        if(block->subs[num])
            findLimitWorker(block->subs[num], bbox);
    }
}

static void findLimits(superblock_t* hEdgeList, float* bbox)
{
    bbox[BOXTOP] = bbox[BOXRIGHT] = DDMINFLOAT;
    bbox[BOXBOTTOM] = bbox[BOXLEFT] = DDMAXFLOAT;
    findLimitWorker(hEdgeList, bbox);
}

/**
 * Find the extremes of a box containing all half-edges.
 */
void BSP_FindNodeBounds(bspnodedata_t* node, superblock_t* hEdgesRightList,
                        superblock_t* hEdgesLeftList)
{
    findLimits(hEdgesLeftList, &node->bBox[LEFT][0]);
    findLimits(hEdgesRightList, &node->bBox[RIGHT][0]);
}

/**
 * Calculate the intersection location between the current half-edge and the
 * partition. Takes advantage of some common situations like horizontal and
 * vertical lines to choose a 'nicer' intersection point.
 */
static __inline void calcIntersection(const hedge_t* cur,
                                      const bspartition_t* part,
                                      double perpC, double perpD,
                                      double* x, double* y)
{
    double              ds;
    bsp_hedgeinfo_t*         data = (bsp_hedgeinfo_t*) cur->data;

    // Horizontal partition against vertical half-edge.
    if(part->pDY == 0 && data->pDX == 0)
    {
        *x = data->pSX;
        *y = part->pSY;
        return;
    }

    // Vertical partition against horizontal half-edge.
    if(part->pDX == 0 && data->pDY == 0)
    {
        *x = part->pSX;
        *y = data->pSY;
        return;
    }

    // 0 = start, 1 = end.
    ds = perpC / (perpC - perpD);

    if(data->pDX == 0)
        *x = data->pSX;
    else
        *x = data->pSX + (data->pDX * ds);

    if(data->pDY == 0)
        *y = data->pSY;
    else
        *y = data->pSY + (data->pDY * ds);
}

/**
 * For debugging.
 */
#if _DEBUG
void SuperBlock_PrintHEdges(superblock_t* superblock)
{
    int                 num;
    const hedge_node_t* n;

    for(n = superblock->hEdges; n; n = n->next)
    {
        const hedge_t*      hEdge = n->hEdge;
        const bsp_hedgeinfo_t* data = hEdge->data;

        Con_Message("Build: %s %p sector=%d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                    (data->lineDef? "NORM" : "MINI"), hEdge,
                    data->sector->buildData.index,
                    hEdge->v[0]->buildData.pos[VX], hEdge->v[0]->buildData.pos[VY],
                    hEdge->v[1]->buildData.pos[VX], hEdge->v[1]->buildData.pos[VY]);
    }

    for(num = 0; num < 2; ++num)
    {
        if(superblock->subs[num])
            SuperBlock_PrintHEdges(superblock->subs[num]);
    }
}

static void testSuperWorker(superblock_t* block, int* real, int* mini)
{
    int                 num;
    const hedge_node_t* n;

    for(n = block->hEdges; n; n = n->next)
    {
        const hedge_t*      cur = n->hEdge;

        if(((bsp_hedgeinfo_t*) cur->data)->lineDef)
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
    int                 realNum = 0;
    int                 miniNum = 0;

    testSuperWorker(block, &realNum, &miniNum);

    if(realNum != block->realNum || miniNum != block->miniNum)
        Con_Error("testSuper: Failed, block=%p %d/%d != %d/%d",
                  block, block->realNum, block->miniNum, realNum,
                  miniNum);
}
#endif
