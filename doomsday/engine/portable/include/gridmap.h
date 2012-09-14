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

typedef uint GridmapCoord;
typedef GridmapCoord GridmapCell[2];
typedef const GridmapCoord const_GridmapCell[2];
typedef AABoxu GridmapCellBlock;

class Gridmap
{
public:
    typedef int (*Gridmap_IterateCallback) (void* cellData, void* parameters);

public:
    /**
     * @param width          X dimension in cells.
     * @param height         Y dimension in cells.
     */
    Gridmap(GridmapCoord width, GridmapCoord height);
    ~Gridmap();

    /// @return  Width of the Gridmap in cells.
    GridmapCoord width() const;

    /// @return  Height of the Gridmap in cells.
    GridmapCoord height() const;

    /// @return  [width, height] of the Gridmap in cells.
    const GridmapCell& widthHeight() const;

    bool Gridmap::clipCell(GridmapCell& cell) const;

    /**
     * Clip the cell coordinates in @a block vs the dimensions of this Gridmap so that they
     * are inside the boundary this defines.
     *
     * @param block           Block coordinates to be clipped.
     *
     * @return  @c true iff the block coordinates were changed.
     */
    bool clipBlock(GridmapCellBlock& block) const;

    /**
     * Retrieve the user data associated with the identified cell.
     *
     * @param mcells         XY coordinates of the cell whose data to retrieve.
     *
     * @return  User data for the identified cell else @c NULL if an invalid reference.
     */
    void* cell(const_GridmapCell mcell);
    inline void* cell(GridmapCoord x, GridmapCoord y)
    {
        GridmapCell mcell = { x, y };
        return cell(mcell);
    }

    Gridmap& setCell(const_GridmapCell mcell, void* userData);
    inline Gridmap& setCell(GridmapCoord x, GridmapCoord y, void* userData)
    {
        GridmapCell mcell = { x, y };
        return setCell(mcell, userData);
    }

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

    friend void Gridmap_DebugDrawer(Gridmap const& gridmap);

private:
    struct Instance;
    Instance* d;
};

/**
 * Render a visual for this Gridmap to assist in debugging (etc...).
 *
 * This visualizer assumes that the caller has already configured the GL render state
 * (projection matrices, scale, etc...) as desired prior to calling. This function
 * guarantees to restore the previous GL state if any changes are made to it.
 *
 * @note Internally this visual uses fixed unit dimensions [1x1] for cells, therefore
 *       the caller should scale the appropriate matrix to scale this visual as desired.
 *
 * @param gridmap         Gridmap instance.
 */
void Gridmap_DebugDrawer(Gridmap const& gridmap);

#endif /// LIBDENG_DATA_GRIDMAP_H
