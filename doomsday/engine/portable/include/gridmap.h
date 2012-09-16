/**
 * @file gridmap.h
 * Gridmap. An abstract class designed for mapping objects into a two
 * dimensional spatial index.
 *
 * Gridmap's implementation allows that the whole space is indexable
 * however cells within it need not be populated. Therefore Gridmap may
 * be considered a "sparse" structure as it allows the user to construct
 * the space piece-wise or, leave it deliberately incomplete.
 *
 * @ingroup data
 *
 * @authors Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_DATA_GRIDMAP_H
#define LIBDENG_DATA_GRIDMAP_H

#include "dd_types.h"
#include "api/aabox.h"
#include "quadtree.h"

typedef uint GridmapCoord;
typedef GridmapCoord GridmapCell[2];
typedef const GridmapCoord const_GridmapCell[2];
typedef AABoxu GridmapCellBlock;

class Gridmap
{
public:
    typedef Quadtree<void*> DataGrid;

    /**
     * Gridmap implementation. Implemented in terms of a Region Quadtree for its
     * inherent sparsity and compression potential.
     */
    DataGrid grid;

    typedef int (*Gridmap_IterateCallback) (void* cellData, void* parameters);

public:
    Gridmap(GridmapCoord width, GridmapCoord height);

    /**
     * Iterate over populated cells in the Gridmap making a callback for each. Iteration ends
     * when all cells have been visited or @a callback returns non-zero.
     *
     * @param parameters     Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int iterate(Gridmap_IterateCallback callback, void* parameters = 0);

    /**
     * Iterate over a block of populated cells in the Gridmap making a callback for each.
     * Iteration ends when all selected cells have been visited or @a callback returns non-zero.
     *
     * @param min            Minimal coordinates for the top left cell.
     * @param max            Maximal coordinates for the bottom right cell.
     * @param callback       Callback function ptr.
     * @param parameters     Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int blockIterate(GridmapCellBlock const& block, Gridmap_IterateCallback callback, void* parameters = 0);

    /**
     * Same as Gridmap::BlockIterate except cell block coordinates are expressed with
     * independent X and Y coordinate arguments. For convenience.
     */
    inline int blockIterate(GridmapCoord minX, GridmapCoord minY, GridmapCoord maxX, GridmapCoord maxY,
                            Gridmap_IterateCallback callback, void* parameters = 0)
    {
        GridmapCellBlock block = GridmapCellBlock(minX, minY, maxX, maxY);
        return blockIterate(block, callback, parameters);
    }
};

#endif /// LIBDENG_DATA_GRIDMAP_H
