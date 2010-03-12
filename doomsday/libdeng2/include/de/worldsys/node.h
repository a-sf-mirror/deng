/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_NODE_H
#define LIBDENG2_NODE_H

#include "../Vector"
#include "../Line"
#include "../MapRectangle"

namespace de
{
    class Node
    {
    public:
        typedef Line2d Partition;

    public:
        Partition partition;
        MapRectangled rightBBox; // Bounding box for right child.
        MapRectangled leftBBox; // Bounding box for left child.

        Node(const Partition& _partition, const MapRectangled& _rightBBox, const MapRectangled& _leftBBox)
          : partition(_partition), rightBBox(_rightBBox), leftBBox(_leftBBox) {};
        Node(dfloat x, dfloat y, dfloat dX, dfloat dY, const MapRectangled& _rightBBox, const MapRectangled& _leftBBox)
          : partition(x, y, dX, dY),
            rightBBox(_rightBBox), leftBBox(_leftBBox) {};
    };
}

#endif /* LIBDENG2_NODE_H */
