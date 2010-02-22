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

#ifndef LIBDENG_SUPERBLOCK_H
#define LIBDENG_SUPERBLOCK_H

#include "deng.h"

#include "../HalfEdgeDS"
#include "../MapRectangle"

#include <deque>

namespace de
{
    class SuperBlock
    {
    public:
        /// Bounding box coordinates.
        enum {
            BOXTOP      = 0,
            BOXBOTTOM   = 1,
            BOXLEFT     = 2,
            BOXRIGHT    = 3
        };

    public:
        /// Parent of this block, or NULL for a top-level block.
        SuperBlock* parent;

        /// Sub-blocks. NULL when empty. Right has the lower coordinates, and
        /// Left has the higher coordinates. Division of a square always
        /// occurs horizontally (e.g. 512x512 -> 256x512 -> 256x256).
        SuperBlock* leftChild, *rightChild;

        /// Coordinates on map for this block, from lower-left corner to
        /// upper-right corner. Pseudo-inclusive, i.e (x,y) is inside block
        /// if and only if BOXLEFT <= x < BOXRIGHT and BOXBOTTOM <= y < BOXTOP.
        dint bbox[4];

        /// Number of real half-edges and minihedges contained by this block
        /// (including all sub-blocks below it).
        duint realNum;
        duint miniNum;

        SuperBlock();
        ~SuperBlock();

        /**
         * Link the given half-edge into the SuperBlock.
         */
        void push(HalfEdge* halfEdge);

        HalfEdge* pop();

        /**
         * Increase the counts within the SuperBlock, to account for the given
         * half-edge being split.
         */
        void incHalfEdgeCounts(bool lineLinked);

        /**
         * Retrieve the Axis-aligned Bounding box for all HalfEdges in this and
         * all child SuperBlocks.
         */
        MapRectanglef aaBounds() const;

        /// LIFO stack of half-edges completely contained by this block.
        /// Uses std::deque internally as friends need the ability to traverse (linearly).
        typedef std::deque<HalfEdge*> HalfEdges;
        HalfEdges halfEdges;

#if _DEBUG
    public:
        void print() const;
#endif
    };
}

#endif /* LIBDENG_SUPERBLOCK_H */
