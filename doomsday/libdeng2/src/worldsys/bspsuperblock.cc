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

#include "de/NodeBuilder"
#include "de/BSPEdge"
#include "de/BSPSuperBlock"
#include "de/Log"

using namespace de;

namespace de
{
    typedef struct evalinfo_s {
        dint cost;
        dint splits;
        dint iffy;
        dint nearMiss;
        dint realLeft;
        dint realRight;
        dint miniLeft;
        dint miniRight;
    } evalinfo_t;
}

static SuperBlockmap::ListNode* allocListNode(void)
{
    return reinterpret_cast<SuperBlockmap::ListNode*>(std::malloc(sizeof(SuperBlockmap::ListNode)));
}

static void freeListNode(SuperBlockmap::ListNode* node)
{
    std::free(node);
}

/**
 * Free all memory allocated for the specified superblock.
 */
static void freeSuperBlockmap(SuperBlockmap* blockmap)
{
    while(blockmap->pop());

    // Recursively handle sub-blocks.
    for(duint num = 0; num < 2; ++num)
    {
        if(blockmap->subs[num])
            freeSuperBlockmap(blockmap->subs[num]);
    }
    delete blockmap;
}

SuperBlockmap::SuperBlockmap()
{}

SuperBlockmap::~SuperBlockmap()
{
    while(pop());

    // Recursively handle sub-blocks.
    for(duint num = 0; num < 2; ++num)
    {
        if(subs[num])
            freeSuperBlockmap(subs[num]);
    }
}

void SuperBlockmap::push(HalfEdge* hEdge)
{
    assert(hEdge);

    ListNode* node;

#if _DEBUG
// Ensure hedge is not already in this SuperBlock.
if((node = _hEdges))
{
    do
    {
        assert(node->hEdge != hEdge);
    } while((node = node->next));
}
#endif

    node = allocListNode();
    node->hEdge = hEdge;
    node->next = _hEdges;
    _hEdges = node;
}

HalfEdge* SuperBlockmap::pop()
{
    if(_hEdges)
    {
        ListNode* node = _hEdges;
        HalfEdge* hEdge = node->hEdge;
        _hEdges = node->next;

        freeListNode(node);
        return hEdge;
    }
    return NULL;
}

void SuperBlockmap::incHalfEdgeCounts(bool lineLinked)
{
    if(lineLinked)
        realNum++;
    else
        miniNum++;
    if(parent)
        parent->incHalfEdgeCounts(lineLinked);
}

static void findBoundsWorker(const SuperBlockmap* block, dfloat* bbox)
{
    for(SuperBlockmap::ListNode* n = block->_hEdges; n; n = n->next)
    {
        const HalfEdge* cur = n->hEdge;
        const Vector2f l = cur->vertex->pos.min(cur->twin->vertex->pos);
        const Vector2f h = cur->vertex->pos.max(cur->twin->vertex->pos);

        if(l.x < bbox[SuperBlockmap::BOXLEFT])
            bbox[SuperBlockmap::BOXLEFT] = l.x;
        else if(l.x > bbox[SuperBlockmap::BOXRIGHT])
            bbox[SuperBlockmap::BOXRIGHT] = l.x;
        if(l.y < bbox[SuperBlockmap::BOXBOTTOM])
            bbox[SuperBlockmap::BOXBOTTOM] = l.y;
        else if(l.y > bbox[SuperBlockmap::BOXTOP])
            bbox[SuperBlockmap::BOXTOP] = l.y;

        if(h.x < bbox[SuperBlockmap::BOXLEFT])
            bbox[SuperBlockmap::BOXLEFT] = h.x;
        else if(h.x > bbox[SuperBlockmap::BOXRIGHT])
            bbox[SuperBlockmap::BOXRIGHT] = h.x;
        if(h.y < bbox[SuperBlockmap::BOXBOTTOM])
            bbox[SuperBlockmap::BOXBOTTOM] = h.y;
        else if(h.y > bbox[SuperBlockmap::BOXTOP])
            bbox[SuperBlockmap::BOXTOP] = h.y;
    }

    // Recursively handle sub-blocks.
    for(duint num = 0; num < 2; ++num)
    {
        if(block->subs[num])
            findBoundsWorker(block->subs[num], bbox);
    }
}

void SuperBlockmap::aaBounds(dfloat bbox[4]) const
{
    bbox[BOXTOP] = bbox[BOXRIGHT] = MINFLOAT;
    bbox[BOXBOTTOM] = bbox[BOXLEFT] = MAXFLOAT;
    findBoundsWorker(this, bbox);
}

#if _DEBUG
void SuperBlockmap::print() const
{
    for(const ListNode* n = _hEdges; n; n = n->next)
    {
        const HalfEdge* hEdge = n->hEdge;
        const HalfEdgeInfo* data = reinterpret_cast<HalfEdgeInfo*>(hEdge->data);

        LOG_DEBUG("Build: %s %p sector=%d %s -> %s")
            << (data->lineDef? "NORM" : "MINI") << hEdge << data->sector->buildData.index
            << hEdge->vertex->pos << hEdge->twin->vertex->pos;
    }

    for(dint num = 0; num < 2; ++num)
    {
        if(subs[num])
            subs[num]->print();
    }
}
#endif

//////////////////// Following code does not belong in this file /////////////////////

/**
 * To be able to divide the nodes down, evalPartition must decide which is
 * the best half-edge to use as a nodeline. It does this by selecting the
 * line with least splits and has least difference of hald-edges on either
 * side of it.
 *
 * @return              @c true, if a "bad half-edge" was found early.
 */
static dint evalPartitionWorker(const SuperBlockmap* hEdgeList,
                                HalfEdge* partHEdge, dint factor, dint bestCost,
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

    SuperBlockmap::ListNode* n;
    ddouble qnty, a, b, fa, fb;
    HalfEdgeInfo* part = (HalfEdgeInfo*) partHEdge->data;
    dint num;

    /**
     * This is the heart of my SuperBlock idea, it tests the _whole_ block
     * against the partition line to quickly handle all the half-edges
     * within it at once. Only when the partition line intercepts the box do
     * we need to go deeper into it - AJA.
     */
    num = P_BoxOnLineSide3(hEdgeList->bbox, partHEdge->vertex->pos.x,
                           partHEdge->vertex->pos.y,
                           part->pDelta.x, part->pDelta.y, part->pPerp,
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
        HalfEdge* otherHEdge = n->hEdge;
        HalfEdgeInfo* other = (HalfEdgeInfo*) otherHEdge->data;

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
            a = M_PerpDist(part->pDelta.x, part->pDelta.y, part->pPerp, part->pLength,
                           otherHEdge->vertex->pos.x, otherHEdge->vertex->pos.y);
            b = M_PerpDist(part->pDelta.x, part->pDelta.y, part->pPerp, part->pLength,
                           otherHEdge->twin->vertex->pos.x, otherHEdge->twin->vertex->pos.y);

            fa = fabs(a);
            fb = fabs(b);
        }

        // Check for being on the same line.
        if(fa <= DIST_EPSILON && fb <= DIST_EPSILON)
        {   // This half-edge runs along the same line as the partition.
            // Check whether it goes in the same direction or the opposite.
            if(other->pDelta.x * part->pDelta.x + other->pDelta.y * part->pDelta.y < 0)
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
static dint evalPartition(const SuperBlockmap* hEdgeList, HalfEdge* part,
                          dint factor, dint bestCost)
{
    HalfEdgeInfo* data = (HalfEdgeInfo*) part->data;
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
    if(data->pDelta.x != 0 && data->pDelta.y != 0)
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
static bool pickHEdgeWorker(const SuperBlockmap* partList,
                               const SuperBlockmap* hEdgeList,
                               dint factor, HalfEdge** best, dint* bestCost)
{
    dint num, cost;
    SuperBlockmap::ListNode* n;

    // Test each half-edge as a potential partition.
    for(n = partList->_hEdges; n; n = n->next)
    {
        HalfEdge* hEdge = n->hEdge;
        HalfEdgeInfo* data = (HalfEdgeInfo*) hEdge->data;

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
 * @return              HalfEdge to use as a partition iff one suitable was found.
 */
HalfEdge* de::SuperBlock_PickPartition(const SuperBlockmap* hEdgeList, dint factor)
{
    dint bestCost = INT_MAX;
    HalfEdge* best = NULL;

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
