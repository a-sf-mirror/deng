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

#ifndef LIBDENG_BSPSUPERBLOCK_H
#define LIBDENG_BSPSUPERBLOCK_H

#include "deng.h"

#include "../HalfEdgeDS"

namespace de
{
    /**
     * Blockmap for half-edges
     *
     * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
     * SourceForge: http://sourceforge.net/projects/glbsp/
     */
    class SuperBlockmap
    {
    public:
        /// Bounding box coordinates.
        enum {
            BOXTOP      = 0,
            BOXBOTTOM   = 1,
            BOXLEFT     = 2,
            BOXRIGHT    = 3,
            BOXFLOOR    = 4,
            BOXCEILING  = 5
        };

        // Smallest distance between two points before being considered equal.
        #define DIST_EPSILON (1.0 / 128.0)

        struct ListNode {
            HalfEdge* hEdge;
            ListNode* next;
        };

    public:
        /// Parent of this block, or NULL for a top-level block.
        SuperBlockmap* parent;

        /// Sub-blocks. NULL when empty. [0] has the lower coordinates, and
        /// [1] has the higher coordinates. Division of a square always
        /// occurs horizontally (e.g. 512x512 -> 256x512 -> 256x256).
        SuperBlockmap* subs[2];

        /// Coordinates on map for this block, from lower-left corner to
        /// upper-right corner. Pseudo-inclusive, i.e (x,y) is inside block
        /// if and only if BOXLEFT <= x < BOXRIGHT and BOXBOTTOM <= y < BOXTOP.
        dint bbox[4];

        /// Number of real half-edges and minihedges contained by this block
        /// (including all sub-blocks below it).
        duint realNum;
        duint miniNum;

        /// LIFO stack of half-edges completely contained by this block.
        ListNode* _hEdges;

        SuperBlockmap();
        ~SuperBlockmap();

        /**
         * Link the given half-edge into the given SuperBlock.
         */
        void push(HalfEdge* hEdge);

        HalfEdge* pop();

        /**
         * Increase the counts within the SuperBlock, to account for the given
         * half-edge being split.
         */
        void incHalfEdgeCounts(bool lineLinked);

        /**
         * Retrieve the Axis-aligned Bounding box for all HalfEdges in this and
         * all child SuperBlockmaps.
         */
        void aaBounds(dfloat bbox[4]) const;

#if _DEBUG
        void print() const;
#endif
    };

    // @todo Should be private to NodeBuilder
    HalfEdge* SuperBlock_PickPartition(const SuperBlockmap* hEdgeList, dint factor);
}

#endif /* LIBDENG_BSPSUPERBLOCK_H */
