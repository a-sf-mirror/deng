/**
 * @file gridmap.cpp
 * Gridmap implementation. @ingroup data
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

#include <de/Error>
#include <de/LegacyCore>
#include <de/String>
#include <de/memoryzone.h>

#include "gridmap.h"

Gridmap::Gridmap(GridmapCoord width, GridmapCoord height)
    : grid(width, height)
{}

GridmapCoord Gridmap::width() const
{
    return grid.width();
}

GridmapCoord Gridmap::height() const
{
    return grid.height();
}

const GridmapCell& Gridmap::widthHeight() const
{
    return grid.widthHeight();
}

bool Gridmap::clipCell(GridmapCell& cell) const
{
    return grid.clipCell(cell);
}

bool Gridmap::clipBlock(GridmapCellBlock& block) const
{
    return grid.clipBlock(block);
}

bool Gridmap::leafAtCell(const_GridmapCell mcell)
{
    if(!grid.leafAtCell(mcell)) return false;
    return !!grid.cell(mcell);
}

void* Gridmap::cell(const_GridmapCell mcell)
{
    if(!grid.leafAtCell(mcell)) return 0;
    return grid.cell(mcell);
}

Gridmap& Gridmap::setCell(const_GridmapCell mcell, void* userData)
{
    grid.setCell(mcell, userData);
    return *this;
}

struct actioncallback_paramaters_t
{
    Gridmap::Gridmap_IterateCallback callback;
    void* callbackParameters;
};

/**
 * Callback actioner. Executes the callback and then returns the result
 * to the current iteration to determine if it should continue.
 */
static int actionCallback(Gridmap::DataGrid::TreeBase* node, void* parameters)
{
    Gridmap::DataGrid::TreeLeaf* leaf = static_cast<Gridmap::DataGrid::TreeLeaf*>(node);
    actioncallback_paramaters_t* p = (actioncallback_paramaters_t*) parameters;
    if(leaf->value())
    {
        return p->callback(leaf->value(), p->callbackParameters);
    }
    return 0; // Continue traversal.
}

int Gridmap::iterate(Gridmap_IterateCallback callback, void* parameters)
{
    actioncallback_paramaters_t p;
    p.callback = callback;
    p.callbackParameters = parameters;
    return grid.iterateLeafs(actionCallback, (void*)&p);
}

int Gridmap::blockIterate(GridmapCellBlock const& block_, Gridmap_IterateCallback callback, void* parameters)
{
    // Clip coordinates to our boundary dimensions (the underlying
    // Quadtree is normally larger than this so we cannot use the
    // dimensions of the root cell here).
    GridmapCellBlock block = block_;
    clipBlock(block);

    // Process all leafs in the block.
    /// @optimize: We could avoid repeatedly descending the tree...
    GridmapCell mcell;
    DataGrid::TreeLeaf* leaf;
    int result;
    for(mcell[1] = block.minY; mcell[1] <= block.maxY; ++mcell[1])
    for(mcell[0] = block.minX; mcell[0] <= block.maxX; ++mcell[0])
    {
        leaf = grid.findLeaf(mcell, false);
        if(!leaf || !leaf->value()) continue;

        result = callback(leaf->value(), parameters);
        if(result) return result;
    }
    return false;
}
