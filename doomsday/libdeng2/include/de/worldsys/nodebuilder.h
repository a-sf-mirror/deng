/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_NODEBUILDER_H
#define LIBDENG2_NODEBUILDER_H

#include "deng.h"

#include "../Error"
#include "../Vector"
#include "../HalfEdgeDS"
#include "../SuperBlock"
#include "../BinaryTree"
#include "../Node"

#include <list>

namespace de
{
    #define IFFY_LEN            4.0

    // Smallest degrees between two angles before being considered equal.
    #define ANG_EPSILON         (1.0 / 1024.0)

    // Smallest distance between two points before being considered equal.
    #define DIST_EPSILON        (1.0 / 128.0)

    typedef ddouble angle_g;  // Degrees, 0 is E, 90 is N

    class Map;
    class LineDef;
    class Sector;
    struct HalfEdgeInfo;

    /**
     * BSP node builder.
     * Originally based on glBSP 2.24 (in turn, based on BSP 2.3), which is
     * hosted on SourceForge: http://sourceforge.net/projects/glbsp/
     */
    class NodeBuilder
    {
    private:
        /// No suitable partition found for the set of half-edges.
        /// Thrown when the set is considered to be convex. @ingroup errors
        DEFINE_ERROR(NoSuitablePartitionError);

        struct BSPartition : Node::Partition
        {
            LineDef* lineDef;
            LineDef* sourceLineDef;
            ddouble length;
            ddouble perp;
            ddouble para;

            BSPartition(const Vector2d& point, const Vector2d& direction,
                LineDef* lineDef, LineDef* sourceLineDef, ddouble length,
                ddouble perp, ddouble para)
              : Node::Partition(point, direction), lineDef(lineDef),
                sourceLineDef(sourceLineDef), length(length), perp(perp),
                para(para) {};
        };

        /**
         * An "intersection" remembers the half-edge that intercepts the partition.
         */
        struct Intersection {
            HalfEdge* halfEdge;
            // How far along the partition line the intercept point is. Zero is at
            // the partition half-edge's start point, positive values move in the
            // same direction as the partition's direction, and negative values move
            // in the opposite direction.
            ddouble distance;

            Intersection(HalfEdge* _halfEdge, ddouble _distance)
                : halfEdge(_halfEdge), distance(_distance) {};
        };
        typedef std::list<Intersection> Intersections;

        typedef std::vector<HalfEdgeInfo*> HalfEdgeInfos;

    public:
        typedef BinaryTree<void*> BSPTree;
        BSPTree* bspTree;

        NodeBuilder(Map& map, dint splitFactor=7);
        ~NodeBuilder();

        void build();

        HalfEdge& createHalfEdge(LineDef* line, LineDef* sourceLine, Vertex* start, Sector* sec, bool back);

        /**
         * @note
         * If the half-edge has a twin, it is also split and is inserted into the
         * same list as the original (and after it), thus all half-edges (except the
         * one we are currently splitting) must exist on a singly-linked list
         * somewhere.
         *
         * @note
         * We must update the count values of any SuperBlock that contains the
         * half-edge (and/or backseg), so that future processing is not messed up by
         * incorrect counts.
         */
        HalfEdge& splitHalfEdge(HalfEdge& halfEdge, ddouble x, ddouble y);

        void updateHalfEdgeInfo(const HalfEdge& halfEdge);

    private:
        /**
         * Initially create all half-edges, one for each side of a linedef.
         */
        void createInitialHalfEdges();

        void createInitialHalfEdgesAndAddtoSuperBlock(SuperBlock* superBlock);

        /**
         * Add the given half-edge to the specified blockmap.
         */
        void addHalfEdgeToSuperBlock(SuperBlock* superBlock, HalfEdge* halfEdge);

        /**
         * Find the best half-edge in the list to use as a partition.
         *
         * @param hEdgeList     List of half-edges to choose from.
         *
         * @return              The new partition.
         */
        BSPartition pickPartition(const SuperBlock* hEdgeList);

        void pickPartitionWorker(const SuperBlock* partList,
            const SuperBlock* hEdgeList, dint factor, HalfEdge** best, dint* bestCost,
            dint validCount);

        /**
         * Evaluate a partition and determine the cost, taking into account the
         * number of splits and the difference between left and right.
         *
         * @return              The computed cost, or a negative value if the edge
         *                      should be skipped altogether.
         */
        dint evalPartition(const SuperBlock* hEdgeList, const BSPartition& bsp,
            dint factor, dint bestCost);

        struct EvalPartitionParamaters {
            dint cost;
            dint splits;
            dint iffy;
            dint nearMiss;
            dint realLeft;
            dint realRight;
            dint miniLeft;
            dint miniRight;
        };

        /**
         * To be able to divide the nodes down, evalPartition must decide which is
         * the best half-edge to use as a nodeline. It does this by selecting the
         * line with least splits and has least difference of half-edges on either
         * side of it.
         *
         * @return              @c true, if a "bad half-edge" was found early.
         */
        bool evalPartition2(const SuperBlock* hEdgeList,
            const BSPartition& bsp, dint factor, dint bestCost,
            EvalPartitionParamaters& params);

        /**
         * Takes the half-edge list and determines if it is convex, possibly
         * converting it into a subsector. Otherwise, the list is divided into two
         * halves and recursion will continue on the new sub list.
         *
         * @param hEdgeList     Ptr to the list of half edges at the current node.
         * @param cutList       Ptr to the cutlist to use for storing new segment
         *                      intersections (cuts).
         * @return              Ptr to the newly created subtree ELSE @c NULL.
         */
        BSPTree* buildNodes(SuperBlock* hEdgeList);

        /**
         * Analyze the intersection list, and add any needed minihedges to the given
         * half-edge lists (one minihedge on each side).
         */
        void createHalfEdgesAlongPartition(const BSPartition& partition, SuperBlock* bRight, SuperBlock* bLeft);

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
        void divideHalfEdge(const BSPartition& partition, HalfEdge& curHalfEdge,
           SuperBlock* bRight, SuperBlock* bLeft);

        void divideHalfEdges(const BSPartition& partition, SuperBlock* hEdgeList,
            SuperBlock* right, SuperBlock* left);

        /**
         * Remove all the half-edges from the list, partitioning them into the left
         * or right lists based on the given partition line. Adds any intersections
         * onto the intersection list as it goes.
         */
        void partitionHalfEdges(const BSPartition& partition, SuperBlock* hEdgeList,
            SuperBlock** rights, SuperBlock** lefts);

        void copyHalfEdgeListFromSuperBlock(Face& face, SuperBlock* block);

        void attachHalfEdgeInfo(HalfEdge& halfEdge, LineDef* line,
            LineDef* sourceLine, Sector* sec, bool back);

        /**
         * Create a new leaf from a list of half-edges.
         */
        Face& createBSPLeaf(Face& face, SuperBlock* hEdgeList);

        void insertIntersection(HalfEdge* halfEdge, ddouble distance);

        /**
         * Search the list for an intersection, if found; return it.
         *
         * @param halfEdge         Ptr to the intercept half-edge to look for.
         *
         * @return              @c true iff an intersection is found ELSE @c false;
         */
        bool findIntersection(const HalfEdge* halfEdge) const;

        void mergeIntersectionOverlaps();

        void connectIntersectionGaps(const BSPartition& partition, SuperBlock* rightList, SuperBlock* leftList);

    private:
        dint _splitFactor;

        Map& _map;

        Intersections _intersections;
        HalfEdgeInfos _halfEdgeInfo;

        /// Used by pickPartitionWorker to exclude all half-edges along a lineDef
        /// for subsequent consideration in the current cycle.
        dint _validCount;
    };
}

#endif /* LIBDENG2_NODEBUILDER_H */
