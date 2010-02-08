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

#include "../BSPIntersection"
#include "../BSPSuperBlock"
#include "../BinaryTree"
#include "../Node"
#include "../Map"

namespace de
{
    #define IFFY_LEN            4.0

    // Smallest degrees between two angles before being considered equal.
    #define ANG_EPSILON         (1.0 / 1024.0)

    // Smallest distance between two points before being considered equal.
    #define DIST_EPSILON        (1.0 / 128.0)

    typedef ddouble angle_g;  // Degrees, 0 is E, 90 is N

    struct HalfEdgeInfo
    {
        /// The SuperBlock that contains this half-edge, or NULL if the half-edge
        /// is no longer in any SuperBlock (e.g., now in a leaf).
        SuperBlock* superBlock;

        // Precomputed data for faster calculations.
        Vector2d direction;
        ddouble length;
        ddouble angle;
        ddouble parallelDistance;
        ddouble perpendicularDistance;

        // LineDef that this half-edge goes along, or NULL if miniseg.
        LineDef* lineDef;

        // LineDef that this half-edge initially comes from.
        // For "real" half-edges, this is just the same as the 'linedef' field
        // above. For "miniedges", this is the linedef of the partition line.
        LineDef* sourceLineDef;

        Sector* sector; // Adjacent sector or, NULL if minihedge / twin on single sided linedef.

        /// @c true = this is on the backside of the edge.
        bool back;
    };

    /**
     * BSP node builder.
     * Originally based on glBSP 2.24 (in turn, based on BSP 2.3), which is
     * hosted on SourceForge: http://sourceforge.net/projects/glbsp/
     */
    class NodeBuilder
    {
    private:
        struct BSPartition : Partition
        {
            LineDef* lineDef;
            LineDef* sourceLineDef;
            ddouble length;
            ddouble perp;
            ddouble para;

            BSPartition() : Partition() {};
        };

    public:
        BinaryTree<void*>* rootNode;

        dsize numHalfEdgeInfo;
        HalfEdgeInfo** halfEdgeInfo;

        NodeBuilder(Map& map, dint splitFactor=7);
        ~NodeBuilder();

        void build();

        void connectGaps(const BSPartition& partition, SuperBlock* rightList, SuperBlock* leftList);
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

        void createInitialHalfEdgesAndAddtoRootSuperBlock();

        /**
         * Add the given half-edge to the specified blockmap.
         */
        void addHalfEdgeToSuperBlock(SuperBlock* superBlock, HalfEdge* halfEdge);

        bool pickPartition(const SuperBlock* hEdgeList, BSPartition& partition);

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
        BinaryTree<void*>* buildNodes(SuperBlock* hEdgeList);

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
            SuperBlock* rights, SuperBlock* lefts);

        /**
         * Remove all the half-edges from the list, partitioning them into the left
         * or right lists based on the given partition line. Adds any intersections
         * onto the intersection list as it goes.
         */
        void partitionHalfEdges(const BSPartition& partition, SuperBlock* hEdgeList,
            SuperBlock** right, SuperBlock** left);

        void copyHalfEdgeListFromSuperBlock(Face& face, SuperBlock* block);

        void attachHalfEdgeInfo(HalfEdge& halfEdge, LineDef* line,
            LineDef* sourceLine, Sector* sec, bool back);

        /**
         * Create a new leaf from a list of half-edges.
         */
        Face& createBSPLeaf(Face& face, SuperBlock* hEdgeList);

        /**
         * Free all the SuperBlocks on the quick-alloc list.
         */
        void destroyUnusedSuperBlocks();

        /**
         * Free all memory allocated for the specified SuperBlock.
         */
        void moveSuperBlockToQuickAllocList(SuperBlock* block);

        /**
         * Acquire memory for a new SuperBlock.
         */
        SuperBlock* createSuperBlock();

        void destroySuperBlock(SuperBlock* block);

        void createRootSuperBlock();

        void destroyRootSuperBlock();

    private:
        dint _splitFactor;

        Map& _map;
        CutList _cutList;
        SuperBlock* _rootSuperBlock;
        SuperBlock* _quickAllocSuperBlocks;
    };
}

#endif /* LIBDENG2_NODEBUILDER_H */
