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
#include "quadtree.h"

typedef Quadtree DataGrid;

/**
 * Gridmap implementation. Implemented in terms of a Region Quadtree for its
 * inherent sparsity and compression potential.
 */
struct Gridmap::Instance
{
    DataGrid grid;
    Instance(GridmapCoord width, GridmapCoord height) : grid(width, height) {}
};

Gridmap::Gridmap(GridmapCoord width, GridmapCoord height)
{
    d = new Instance(width, height);
}

Gridmap::~Gridmap()
{
    delete d;
}

GridmapCoord Gridmap::width() const
{
    return d->grid.width();
}

GridmapCoord Gridmap::height() const
{
    return d->grid.height();
}

const GridmapCell& Gridmap::widthHeight() const
{
    return d->grid.widthHeight();
}

bool Gridmap::clipCell(GridmapCell& cell) const
{
    return d->grid.clipCell(cell);
}

bool Gridmap::clipBlock(GridmapCellBlock& block) const
{
    return d->grid.clipBlock(block);
}

bool Gridmap::leafAtCell(const_GridmapCell mcell) const
{
    return !!d->grid.cell(mcell);
}

void* Gridmap::cell(const_GridmapCell mcell) const
{
    return d->grid.cell(mcell);
}

Gridmap& Gridmap::setCell(const_GridmapCell mcell, void* userData)
{
    d->grid.setCell(mcell, userData);
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
static int actionCallback(DataGrid::TreeBase* node, void* parameters)
{
    DataGrid::TreeLeaf* leaf = static_cast<DataGrid::TreeLeaf*>(node);
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
    return d->grid.iterateLeafs(actionCallback, (void*)&p);
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
        leaf = d->grid.findLeaf(mcell, false);
        if(!leaf || !leaf->value()) continue;

        result = callback(leaf->value(), parameters);
        if(result) return result;
    }
    return false;
}

/**
 * Gridmap Visual - for debugging
 */

#include "de_base.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "m_vector.h"

#define UNIT_WIDTH                     1
#define UNIT_HEIGHT                    1

static int drawCell(DataGrid::TreeBase* node, void* /*parameters*/)
{
    vec2f_t topLeft, bottomRight;

    V2f_Set(topLeft, UNIT_WIDTH * node->x(), UNIT_HEIGHT * node->y());
    V2f_Set(bottomRight, UNIT_WIDTH  * (node->x() + node->size()),
                         UNIT_HEIGHT * (node->y() + node->size()));

    glBegin(GL_LINE_LOOP);
        glVertex2fv((GLfloat*)topLeft);
        glVertex2f(bottomRight[0], topLeft[1]);
        glVertex2fv((GLfloat*)bottomRight);
        glVertex2f(topLeft[0], bottomRight[1]);
    glEnd();
    return 0; // Continue iteration.
}

void Gridmap_DebugDrawer(Gridmap const& gm)
{
    // We'll be changing the color, so query the current and restore later.
    GLfloat oldColor[4];
    glGetFloatv(GL_CURRENT_COLOR, oldColor);

    /**
     * Draw our Quadtree.
     */
    DataGrid::TreeBase const& root = gm.d->grid;
    glColor4f(1.f, 1.f, 1.f, 1.f / root.size());

    DataGrid::traverse_parameters_t travParms;
    travParms.leafOnly = false;
    travParms.callback = drawCell;
    travParms.callbackParameters = 0;
    DataGrid::traverse(&const_cast<DataGrid::TreeBase&>(root), travParms);

    /**
     * Draw our bounds.
     */
    vec2f_t start, end;
    V2f_Set(start, 0, 0);
    V2f_Set(end, UNIT_WIDTH * gm.width(), UNIT_HEIGHT * gm.height());

    glColor3f(1, .5f, .5f);
    glBegin(GL_LINES);
        glVertex2f(start[0], start[1]);
        glVertex2f(  end[0], start[1]);

        glVertex2f(  end[0], start[1]);
        glVertex2f(  end[0],   end[1]);

        glVertex2f(  end[0],   end[1]);
        glVertex2f(start[0],   end[1]);

        glVertex2f(start[0],   end[1]);
        glVertex2f(start[0], start[1]);
    glEnd();

    // Restore GL state.
    glColor4fv(oldColor);
}

#undef UNIT_HEIGHT
#undef UNIT_WIDTH
