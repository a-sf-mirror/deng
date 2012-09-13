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

#include <cmath>

#include "de_base.h"

#include <de/Error>
#include <de/LegacyCore>
#include <de/String>
#include <de/memoryzone.h>

#include "gridmap.h"

/// Space dimension ordinals.
enum { X = 0, Y };

/**
 * TreeCell. Used to represent a subquadrant within the owning Gridmap.
 */
class TreeCell
{
public:
    /// Child quadrant identifiers.
    enum Quadrant
    {
        TopLeft = 0,
        TopRight,
        BottomLeft,
        BottomRight
    };

public:
    explicit TreeCell(GridmapCoord x = 0, GridmapCoord y = 0, GridmapCoord size = 0)
        : userData_(0), size_(size)
    {
        origin[X] = x;
        origin[Y] = y;
        children[TopLeft] = children[TopRight] = children[BottomLeft] = children[BottomRight] = 0;
    }

    TreeCell(const_GridmapCell origin_, GridmapCoord size = 0)
        : userData_(0), size_(size)
    {
        origin[X] = origin_[X];
        origin[Y] = origin_[Y];
        children[TopLeft] = children[TopRight] = children[BottomLeft] = children[BottomRight] = 0;
    }

    /// @return  @c true= @a tree is a a leaf (i.e., equal to a unit in Gridmap space).
    inline bool isLeaf() const { return size_ == 1; }

    inline GridmapCoord x() const { return origin[X]; }
    inline GridmapCoord y() const { return origin[Y]; }
    inline const GridmapCell& xy() const { return origin; }

    inline GridmapCoord size() const { return size_; }

    inline TreeCell* topLeft() const { return children[TopLeft]; }
    inline TreeCell* topRight() const { return children[TopRight]; }
    inline TreeCell* bottomLeft() const { return children[BottomLeft]; }
    inline TreeCell* bottomRight() const { return children[BottomRight]; }

    inline TreeCell* child(Quadrant q) const
    {
        switch(q)
        {
        case TopLeft:       return topLeft();
        case TopRight:      return topRight();
        case BottomLeft:    return bottomLeft();
        case BottomRight:   return bottomRight();
        default:
            throw de::Error("Gridmap::TreeCell::child", QString("Invalid Quadrant %1").arg(int(q)));
        }
    }

    /// @note Does nothing about any existing child tree. Caller must ensure that this
    ///       is logical and does not result in memory leakage.
    TreeCell& setChild(Quadrant q, TreeCell* newChild)
    {
        children[q] = newChild;
        return *this;
    }

    inline void* userData() const { return userData_; }

    TreeCell& setUserData(void* newUserData)
    {
        userData_ = newUserData;
        return *this;
    }

private:
    /// User data associated with the cell. Note that only leafs can have
    /// associated user data.
    void* userData_;

    /// Origin of this cell in Gridmap space [x,y].
    GridmapCell origin;

    /// Size of this cell in Gridmap space (width=height).
    GridmapCoord size_;

    /// Child cells of this, one for each subquadrant.
    TreeCell* children[4];
};

/**
 * Depth-first traversal of the children of this tree, making a callback
 * for each cell. Iteration ends when all selected cells have been visited
 * or a callback returns a non-zero value.
 *
 * @param tree          TreeCell to traverse.
 * @param leafOnly      Caller is only interested in leaves.
 * @param callback      Callback function.
 * @param parameters    Passed to the callback.
 *
 * @return  Zero iff iteration completed wholly, else the value returned by the
 *          last callback made.
 */
static int iterateCell(TreeCell& tree, bool leafOnly,
    int (*callback) (TreeCell& tree, void* parameters), void* parameters = 0)
{
    DENG2_ASSERT(callback);

    int result = false; // Continue traversal.
    if(!tree.isLeaf())
    {
        if(tree.topLeft())
        {
            result = iterateCell(*tree.topLeft(), leafOnly, callback, parameters);
            if(result) return result;
        }
        if(tree.topRight())
        {
            result = iterateCell(*tree.topRight(), leafOnly, callback, parameters);
            if(result) return result;
        }
        if(tree.bottomLeft())
        {
            result = iterateCell(*tree.bottomLeft(), leafOnly, callback, parameters);
            if(result) return result;
        }
        if(tree.bottomRight())
        {
            result = iterateCell(*tree.bottomRight(), leafOnly, callback, parameters);
            if(result) return result;
        }
    }
    if(!leafOnly || tree.isLeaf())
    {
        result = callback(tree, parameters);
    }
    return result;
}

static GridmapCoord ceilPow2(GridmapCoord unit);

/**
 * Gridmap implementation. Designed around that of a Region Quadtree
 * with inherent sparsity and compression potential.
 */
struct Gridmap::Instance
{
    /// Dimensions of the space we are indexing (in cells).
    GridmapCell dimensions;

    /// Zone memory tag used for both the Gridmap and user data.
    int zoneTag;

    /// Size of the memory block to be allocated for each leaf.
    size_t sizeOfCell;

    /// Root tree for our Quadtree. Allocated along with the Gridmap instance.
    TreeCell root;

    Instance(GridmapCoord width, GridmapCoord height, size_t _sizeOfCell, int _zoneTag)
        : zoneTag(_zoneTag), sizeOfCell(_sizeOfCell),
          // Quadtree must subdivide the space equally into 1x1 unit cells.
          root(0, 0, ceilPow2(MAX_OF(width, height)))
    {
        dimensions[X] = width;
        dimensions[Y] = height;
    }

    ~Instance()
    {
        deleteTree();
    }

    /**
     * Construct and initialize a new TreeCell.
     *
     * @param x            X coordinate in Gridmap space.
     * @param y            Y coordinate in Gridmap space.
     * @param size         Size in Gridmap space units.
     * @param zoneTag      Zone memory tag to associate with the new TreeCell.
     *
     * @return  Newly allocated and initialized TreeCell instance.
     */
    TreeCell* newCell(const_GridmapCell mcell, GridmapCoord size, int zoneTag)
    {
        void* region = Z_Malloc(sizeof TreeCell, zoneTag, NULL);
        if(!region) throw de::Error("Gridmap::newCell", QString("Failed on allocation of %1 bytes for new Cell").arg((unsigned long) sizeof TreeCell));
        return new (region) TreeCell(mcell, size);
    }

    inline TreeCell* newCell(GridmapCoord x, GridmapCoord y, GridmapCoord size, int zoneTag)
    {
        GridmapCoord mcell[2] = { x, y };
        return newCell(mcell, size, zoneTag);
    }

    static void deleteCell(TreeCell* tree)
    {
        DENG2_ASSERT(tree);
        // Deletion is a depth-first traversal.
        if(tree->topLeft())     deleteCell(tree->topLeft());
        if(tree->topRight())    deleteCell(tree->topRight());
        if(tree->bottomLeft())  deleteCell(tree->bottomLeft());
        if(tree->bottomRight()) deleteCell(tree->bottomRight());
        if(tree->userData())
        {
            Z_Free(tree->userData());
        }
        tree->~TreeCell();
        Z_Free(tree);
    }

    void deleteTree()
    {
        // The root tree is allocated along with Gridmap.
        if(root.topLeft())     deleteCell(root.topLeft());
        if(root.topRight())    deleteCell(root.topRight());
        if(root.bottomLeft())  deleteCell(root.bottomLeft());
        if(root.bottomRight()) deleteCell(root.bottomRight());

        if(root.userData())
        {
            Z_Free(root.userData());
        }
    }

    TreeCell* findLeafDescend(TreeCell* tree, GridmapCoord x, GridmapCoord y, bool alloc)
    {
        DENG2_ASSERT(tree);

        if(tree->isLeaf())
        {
            return tree;
        }

        // Into which quadrant do we need to descend?
        TreeCell::Quadrant q;
        if(x < tree->x() + (tree->size() >> 1))
        {
            q = (y < tree->y() + (tree->size() >> 1))? TreeCell::TopLeft  : TreeCell::BottomLeft;
        }
        else
        {
            q = (y < tree->y() + (tree->size() >> 1))? TreeCell::TopRight : TreeCell::BottomRight;
        }

        // Has this quadrant been initialized yet?
        if(!tree->child(q))
        {
            GridmapCoord subOrigin[2], subSize;

            // Are we allocating cells?
            if(!alloc) return NULL;

            // Subdivide this tree and construct the new.
            subSize = tree->size() >> 1;
            switch(q)
            {
            case TreeCell::TopLeft:
                subOrigin[X] = tree->x();
                subOrigin[Y] = tree->y();
                break;
            case TreeCell::TopRight:
                subOrigin[X] = tree->x() + subSize;
                subOrigin[Y] = tree->y();
                break;
            case TreeCell::BottomLeft:
                subOrigin[X] = tree->x();
                subOrigin[Y] = tree->y() + subSize;
                break;
            case TreeCell::BottomRight:
                subOrigin[X] = tree->x() + subSize;
                subOrigin[Y] = tree->y() + subSize;
                break;
            default:
                throw de::Error("Gridmap::findLeafDescend", QString("Invalid quadrant %1").arg(int(q)));
            }
            tree->setChild(q, newCell(subOrigin, subSize, zoneTag));
        }

        return findLeafDescend(tree->child(q), x, y, alloc);
    }

    TreeCell* findLeaf(GridmapCoord x, GridmapCoord y, bool alloc)
    {
        return findLeafDescend(&root, x, y, alloc);
    }
};

Gridmap::Gridmap(GridmapCoord width, GridmapCoord height, size_t sizeOfCell, int zoneTag)
{
    void* region = Z_Calloc(sizeof Instance, zoneTag, 0);
    if(!region)
    {
        throw de::Error("Gridmap::Gridmap", QString("Failed allocating %1 bytes for private Instance").arg((unsigned long) sizeof Instance));
    }
    d = new (region) Instance(width, height, sizeOfCell, zoneTag);
}

Gridmap::~Gridmap()
{
    Z_Free(d);
}

TreeCell& Gridmap::root()
{
    return d->root;
}

GridmapCoord Gridmap::width() const
{
    return d->dimensions[X];
}

GridmapCoord Gridmap::height() const
{
    return d->dimensions[Y];
}

const GridmapCell& Gridmap::widthHeight() const
{
    return d->dimensions;
}

bool Gridmap::clipBlock(GridmapCellBlock& block) const
{
    bool adjusted = false;
    if(block.minX >= d->dimensions[X])
    {
        block.minX = d->dimensions[X]-1;
        adjusted = true;
    }
    if(block.minY >= d->dimensions[Y])
    {
        block.minY = d->dimensions[Y]-1;
        adjusted = true;
    }
    if(block.maxX >= d->dimensions[X])
    {
        block.maxX = d->dimensions[X]-1;
        adjusted = true;
    }
    if(block.maxY >= d->dimensions[Y])
    {
        block.maxY = d->dimensions[Y]-1;
        adjusted = true;
    }
    return adjusted;
}

TreeCell* Gridmap::findLeaf(GridmapCoord x, GridmapCoord y, bool alloc)
{
    return d->findLeafDescend(&d->root, x, y, alloc);
}

void* Gridmap::cell(const_GridmapCell mcell, bool alloc)
{
    // Outside our boundary?
    if(mcell[X] >= d->dimensions[X] || mcell[Y] >= d->dimensions[Y]) return NULL;

    // Try to locate this leaf (may fail if not present and we are
    // not allocating user data (there will be no corresponding cell)).
    TreeCell* tree = findLeaf(mcell[X], mcell[Y], alloc);
    if(!tree) return 0;

    // Exisiting user data for this cell?
    if(tree->userData()) return tree->userData();

    // Allocate new user data?
    if(!alloc) return 0;

    tree->setUserData(Z_Calloc(d->sizeOfCell, d->zoneTag, 0));
    return tree->userData();
}

static GridmapCoord ceilPow2(GridmapCoord unit)
{
    GridmapCoord cumul;
    for(cumul = 1; unit > cumul; cumul <<= 1);
    return cumul;
}

Gridmap* Gridmap::create(GridmapCoord width, GridmapCoord height, size_t cellSize, int zoneTag)
{
    Gridmap* gm = 0;
    try
    {
        void* region = Z_Calloc(sizeof Gridmap, zoneTag, 0);
        if(!region)
        {
            throw de::Error("Gridmap_New", QString("Failed on allocation of %1 bytes for new de::Gridmap.").arg((unsigned long) sizeof Gridmap));
        }
        gm = new (region) Gridmap(width, height, cellSize, zoneTag);
    }
    catch(de::Error& er)
    {
        QString msg = er.asText();
        LegacyCore_FatalError(msg.toUtf8().constData());
    }
    return reinterpret_cast<Gridmap*>(gm);
}

void Gridmap::destroy(Gridmap* gm)
{
    if(gm)
    {
        gm->~Gridmap();
        Z_Free(gm);
    }
}

typedef struct {
    Gridmap::Gridmap_IterateCallback callback;
    void* callbackParameters;
} actioncallback_paramaters_t;

/**
 * Callback actioner. Executes the callback and then returns the result
 * to the current iteration to determine if it should continue.
 */
static int actionCallback(TreeCell& tree, void* parameters)
{
    actioncallback_paramaters_t* p = (actioncallback_paramaters_t*) parameters;
    DENG2_ASSERT(p);
    if(tree.userData())
        return p->callback(tree.userData(), p->callbackParameters);
    return 0; // Continue traversal.
}

int Gridmap::iterate(Gridmap_IterateCallback callback, void* parameters)
{
    actioncallback_paramaters_t p;
    p.callback = callback;
    p.callbackParameters = parameters;
    return iterateCell(d->root, true/*only leaves*/, actionCallback, (void*)&p);
}

int Gridmap::blockIterate(GridmapCellBlock const& block_, Gridmap_IterateCallback callback, void* parameters)
{
    // Clip coordinates to our boundary dimensions (the underlying
    // Quadtree is normally larger than this so we cannot use the
    // dimensions of the root cell here).
    GridmapCellBlock block = block_;
    clipBlock(block);

    // Traverse cells in the block.
    /// @optimize: We could avoid repeatedly descending the tree...
    GridmapCoord x, y;
    TreeCell* tree;
    int result;
    for(y = block.minY; y <= block.maxY; ++y)
    for(x = block.minX; x <= block.maxX; ++x)
    {
        tree = findLeaf(x, y, false);
        if(!tree || !tree->userData()) continue;

        result = callback(tree->userData(), parameters);
        if(result) return result;
    }
    return false;
}

/**
 * Gridmap Visual - for debugging
 */

#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "m_vector.h"

#define UNIT_WIDTH                     1
#define UNIT_HEIGHT                    1

static int drawCell(TreeCell& tree, void* /*parameters*/)
{
    vec2f_t topLeft, bottomRight;

    V2f_Set(topLeft, UNIT_WIDTH * tree.x(), UNIT_HEIGHT * tree.y());
    V2f_Set(bottomRight, UNIT_WIDTH  * (tree.x() + tree.size()),
                         UNIT_HEIGHT * (tree.y() + tree.size()));

    glBegin(GL_LINE_LOOP);
        glVertex2fv((GLfloat*)topLeft);
        glVertex2f(bottomRight[X], topLeft[Y]);
        glVertex2fv((GLfloat*)bottomRight);
        glVertex2f(topLeft[X], bottomRight[Y]);
    glEnd();
    return 0; // Continue iteration.
}

void Gridmap_DebugDrawer(Gridmap* gm)
{
    // We'll be changing the color, so query the current and restore later.
    GLfloat oldColor[4];
    glGetFloatv(GL_CURRENT_COLOR, oldColor);

    /**
     * Draw our Quadtree.
     */
    TreeCell& root = *gm;
    glColor4f(1.f, 1.f, 1.f, 1.f / root.size());
    iterateCell(root, false/*all cells*/, drawCell);

    /**
     * Draw our bounds.
     */
    vec2f_t start, end;
    V2f_Set(start, 0, 0);
    V2f_Set(end, UNIT_WIDTH * gm->width(), UNIT_HEIGHT * gm->height());

    glColor3f(1, .5f, .5f);
    glBegin(GL_LINES);
        glVertex2f(start[X], start[Y]);
        glVertex2f(  end[X], start[Y]);

        glVertex2f(  end[X], start[Y]);
        glVertex2f(  end[X],   end[Y]);

        glVertex2f(  end[X],   end[Y]);
        glVertex2f(start[X],   end[Y]);

        glVertex2f(start[X],   end[Y]);
        glVertex2f(start[X], start[Y]);
    glEnd();

    // Restore GL state.
    glColor4fv(oldColor);
}

#undef UNIT_HEIGHT
#undef UNIT_WIDTH
