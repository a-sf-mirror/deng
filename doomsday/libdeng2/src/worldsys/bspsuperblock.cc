/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
 * Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/NodeBuilder"
#include "de/BSPSuperBlock"
#include "de/Log"

using namespace de;

SuperBlock::SuperBlock()
{}

SuperBlock::~SuperBlock()
{
    halfEdges.clear();
    // Recursively handle sub-blocks.
    for(duint num = 0; num < 2; ++num)
    {
        if(subs[num])
            delete subs[num];
    }
}

void SuperBlock::push(HalfEdge* hEdge)
{
    assert(hEdge);
#if _DEBUG
// Ensure hEdge is not already in this SuperBlock. *Should* be impossible.
FOR_EACH(i, halfEdges, HalfEdges::iterator)
    assert(*i != hEdge);
#endif
    halfEdges.push_back(hEdge);
}

HalfEdge* SuperBlock::pop()
{
    if(!halfEdges.empty())
    {
        HalfEdge* hEdge = halfEdges.back();
        halfEdges.pop_back();
        return hEdge;
    }
    return NULL;
}

void SuperBlock::incHalfEdgeCounts(bool lineLinked)
{
    if(lineLinked)
        realNum++;
    else
        miniNum++;
    if(parent)
        parent->incHalfEdgeCounts(lineLinked);
}

static void findBoundsWorker(const SuperBlock* block, dfloat* bbox)
{
    FOR_EACH(i, block->halfEdges, SuperBlock::HalfEdges::const_iterator)
    {
        const HalfEdge* cur = (*i);
        const Vector2f l = cur->vertex->pos.min(cur->twin->vertex->pos);
        const Vector2f h = cur->vertex->pos.max(cur->twin->vertex->pos);

        if(l.x < bbox[SuperBlock::BOXLEFT])
            bbox[SuperBlock::BOXLEFT] = l.x;
        else if(l.x > bbox[SuperBlock::BOXRIGHT])
            bbox[SuperBlock::BOXRIGHT] = l.x;
        if(l.y < bbox[SuperBlock::BOXBOTTOM])
            bbox[SuperBlock::BOXBOTTOM] = l.y;
        else if(l.y > bbox[SuperBlock::BOXTOP])
            bbox[SuperBlock::BOXTOP] = l.y;

        if(h.x < bbox[SuperBlock::BOXLEFT])
            bbox[SuperBlock::BOXLEFT] = h.x;
        else if(h.x > bbox[SuperBlock::BOXRIGHT])
            bbox[SuperBlock::BOXRIGHT] = h.x;
        if(h.y < bbox[SuperBlock::BOXBOTTOM])
            bbox[SuperBlock::BOXBOTTOM] = h.y;
        else if(h.y > bbox[SuperBlock::BOXTOP])
            bbox[SuperBlock::BOXTOP] = h.y;
    }

    // Recursively handle sub-blocks.
    for(duint num = 0; num < 2; ++num)
    {
        if(block->subs[num])
            findBoundsWorker(block->subs[num], bbox);
    }
}

MapRectangle SuperBlock::aaBounds() const
{
    dfloat bbox[4];
    bbox[BOXTOP] = bbox[BOXRIGHT] = MINFLOAT;
    bbox[BOXBOTTOM] = bbox[BOXLEFT] = MAXFLOAT;
    findBoundsWorker(this, bbox);
    return MapRectangle(bbox);
}

#if _DEBUG
void SuperBlock::print() const
{
    FOR_EACH(i, halfEdges, HalfEdges::const_iterator)
    {
        const HalfEdge* hEdge = (*i);
        const HalfEdgeInfo* data = reinterpret_cast<HalfEdgeInfo*>(hEdge->data);

        LOG_MESSAGE("Build: %s %p sector=%d %s -> %s")
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

/**
 * To be able to divide the nodes down, evalPartition must decide which is
 * the best half-edge to use as a nodeline. It does this by selecting the
 * line with least splits and has least difference of hald-edges on either
 * side of it.
 *
 * @return              @c true, if a "bad half-edge" was found early.
 */
static dint evalPartitionWorker(const SuperBlock* hEdgeList,
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
                           part->direction.x, part->direction.y, part->perpendicularDistance,
                           part->length, DIST_EPSILON);
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
    FOR_EACH(i, hEdgeList->halfEdges, SuperBlock::HalfEdges::const_iterator)
    {
        const HalfEdge* otherHEdge = (*i);
        HalfEdgeInfo* other = (HalfEdgeInfo*) otherHEdge->data;

        // This is the heart of my pruning idea - it catches
        // "bad half-edges" early on - LK.

        if(info->cost > bestCost)
            return true;

        // Get state of lines' relation to each other.
        if(other->sourceLineDef == part->sourceLineDef)
        {
            a = b = fa = fb = 0;
        }
        else
        {
            a = M_PerpDist(part->direction.x, part->direction.y, part->perpendicularDistance, part->length,
                           otherHEdge->vertex->pos.x, otherHEdge->vertex->pos.y);
            b = M_PerpDist(part->direction.x, part->direction.y, part->perpendicularDistance, part->length,
                           otherHEdge->twin->vertex->pos.x, otherHEdge->twin->vertex->pos.y);

            fa = fabs(a);
            fb = fabs(b);
        }

        // Check for being on the same line.
        if(fa <= DIST_EPSILON && fb <= DIST_EPSILON)
        {   // This half-edge runs along the same line as the partition.
            // Check whether it goes in the same direction or the opposite.
            if(other->direction.x * part->direction.x + other->direction.y * part->direction.y < 0)
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
static dint evalPartition(const SuperBlock* hEdgeList, HalfEdge* part,
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
LOG_MESSAGE("Eval : No real half-edges on %s%sside")
    << (info.realLeft? "" : "left ") << (info.realRight? "" : "right ");
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
    if(data->direction.x != 0 && data->direction.y != 0)
        info.cost += 25;

/*#if _DEBUG
LOG_MESSAGE("Eval %p: splits=%d iffy=%d near=%d left=%d+%d right=%d+%d cost=%d.%02d")
    << part << info.splits << info.iffy << info.nearMiss << info.realLeft
    << info.miniLeft << info.realRight << info.miniRight << (info.cost / 100)
    << (info.cost % 100);
#endif*/

    return info.cost;
}

/**
 * @return              @c false, if cancelled.
 */
static bool pickHEdgeWorker(const SuperBlock* partList,
    const SuperBlock* hEdgeList, dint factor, HalfEdge** best, dint* bestCost)
{
    dint cost;

    // Test each half-edge as a potential partition.
    FOR_EACH(i, partList->halfEdges, SuperBlock::HalfEdges::const_iterator)
    {
        HalfEdge* hEdge = (*i);
        HalfEdgeInfo* data = (HalfEdgeInfo*) hEdge->data;

/*#if _DEBUG
LOG_MESSAGE("pickHEdgeWorker: %sSEG %p sector=%d %s -> %s")
    << (data->lineDef? "" : "MINI") <<  hEdge
    << (data->sector? data->sector->index : -1)
    << hEdge->vertex.pos, hEdge->twin->vertex.pos;
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
    for(dint num = 0; num < 2; ++num)
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
 * @return              @true iff a new partition is found (and necessary).
 */
bool NodeBuilder::pickPartition(const SuperBlock* hEdgeList, BSPartition& bsp)
{
    dint bestCost = INT_MAX;
    HalfEdge* best = NULL;

    validCount++;
    if(pickHEdgeWorker(hEdgeList, hEdgeList, _splitFactor, &best, &bestCost))
    {   // Finished, return the best partition if found.
        const HalfEdgeInfo* info = reinterpret_cast<HalfEdgeInfo*>(best->data);

        bsp.point = best->vertex->pos;
        bsp.direction = best->twin->vertex->pos - best->vertex->pos;
        bsp.lineDef = info->lineDef;
        bsp.sourceLineDef = info->sourceLineDef;
        bsp.perp = info->perpendicularDistance;
        bsp.para = info->parallelDistance;
        bsp.length = info->length;
        return true;
    }
    return false; // BuildNodes will detect the cancellation.
}
