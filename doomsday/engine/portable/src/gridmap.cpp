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

/// Space dimension ordinals.
enum { X = 0, Y };

class TreeBase
{
public:
    explicit TreeBase(GridmapCoord x = 0, GridmapCoord y = 0)
    {
        origin[X] = x;
        origin[Y] = y;
    }

    virtual ~TreeBase() {}

    inline GridmapCoord x() const { return origin[X]; }
    inline GridmapCoord y() const { return origin[Y]; }
    inline const GridmapCell& xy() const { return origin; }

    virtual GridmapCoord size() const =0;

protected:
    /// Origin of this node in Gridmap space [x, y].
    GridmapCell origin;
};

class TreeLeaf : public TreeBase
{
public:
    explicit TreeLeaf(GridmapCoord x = 0, GridmapCoord y = 0, void* userData = 0)
        : TreeBase(x, y), userData_(userData)
    {}

    explicit TreeLeaf(const_GridmapCell mcell, void* userData = 0)
        : TreeBase(mcell[X], mcell[Y]), userData_(userData)
    {}

    ~TreeLeaf()
    {
        if(userData_) Z_Free(userData_);
    }

    inline GridmapCoord size() const { return 1; }

    inline void* userData() const { return userData_; }

    TreeLeaf& setUserData(void* newUserData)
    {
        // Exisiting user data for this leaf?
        if(userData_)
        {
            Z_Free(userData_);
        }
        userData_ = newUserData;
        return *this;
    }

private:
    /// User data associated with the cell. Note that only leafs can have
    /// associated user data.
    void* userData_;
};

/**
 * TreeCell. Used to represent a subquadrant within the owning Gridmap.
 */
class TreeNode : public TreeBase
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
    explicit TreeNode(GridmapCoord x = 0, GridmapCoord y = 0, GridmapCoord size = 0)
        : TreeBase(x, y), size_(size)
    {
        children[TopLeft] = children[TopRight] = children[BottomLeft] = children[BottomRight] = 0;
    }

    TreeNode(const_GridmapCell origin, GridmapCoord size = 0)
        : TreeBase(origin[X], origin[Y]), size_(size)
    {
        children[TopLeft] = children[TopRight] = children[BottomLeft] = children[BottomRight] = 0;
    }

    ~TreeNode()
    {
        // Deletion is a depth-first traversal.
        if(children[TopLeft])     delete children[TopLeft];
        if(children[TopRight])    delete children[TopRight];
        if(children[BottomLeft])  delete children[BottomLeft];
        if(children[BottomRight]) delete children[BottomRight];
    }

    inline GridmapCoord size() const { return size_; }

    inline TreeBase* topLeft() const { return children[TopLeft]; }
    inline TreeBase* topRight() const { return children[TopRight]; }
    inline TreeBase* bottomLeft() const { return children[BottomLeft]; }
    inline TreeBase* bottomRight() const { return children[BottomRight]; }

    inline TreeBase* child(Quadrant q) const
    {
        switch(q)
        {
        case TopLeft:       return topLeft();
        case TopRight:      return topRight();
        case BottomLeft:    return bottomLeft();
        case BottomRight:   return bottomRight();
        default:
            throw de::Error("Gridmap::TreeNode::child", QString("Invalid Quadrant %1").arg(int(q)));
        }
    }

    /// @note Does nothing about any existing child tree. Caller must ensure that this
    ///       is logical and does not result in memory leakage.
    TreeNode& setChild(Quadrant q, TreeBase* newChild)
    {
        children[q] = newChild;
        return *this;
    }

private:
    /// Size of this cell in Gridmap space (width=height).
    GridmapCoord size_;

    /// Child cells of this, one for each subquadrant.
    TreeBase* children[4];
};

static inline bool isLeaf(TreeBase& tree)
{
    return tree.size() == 1;
}

struct traversetree_parameters_t
{
    bool leafOnly;
    int (*callback) (TreeBase* node, void* parameters);
    void* callbackParameters;
};

/**
 * Depth-first traversal of the children of this tree, making a callback
 * for each cell. Iteration ends when all selected cells have been visited
 * or a callback returns a non-zero value.
 *
 * @param tree          Tree to traverse.
 * @param leafOnly      @c true= Caller is only interested in leaves.
 * @param callback      Callback function.
 * @param parameters    Passed to the callback.
 *
 * @return  Zero iff iteration completed wholly, else the value returned by the
 *          last callback made.
 */
static int traverseTree(TreeBase* tree, traversetree_parameters_t const& p)
{
    int result;
    if(!isLeaf(*tree))
    {
        TreeNode* node = static_cast<TreeNode*>(tree);
        if(node->topLeft())
        {
            result = traverseTree(node->topLeft(), p);
            if(result) return result;
        }
        if(node->topRight())
        {
            result = traverseTree(node->topRight(), p);
            if(result) return result;
        }
        if(node->bottomLeft())
        {
            result = traverseTree(node->bottomLeft(), p);
            if(result) return result;
        }
        if(node->bottomRight())
        {
            result = traverseTree(node->bottomRight(), p);
            if(result) return result;
        }
    }
    if(!p.leafOnly || isLeaf(*tree))
    {
        result = p.callback(tree, p.callbackParameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

static GridmapCoord ceilPow2(GridmapCoord unit)
{
    GridmapCoord cumul;
    for(cumul = 1; unit > cumul; cumul <<= 1);
    return cumul;
}

/**
 * Gridmap implementation. Designed around that of a Region Quadtree
 * with inherent sparsity and compression potential.
 */
struct Gridmap::Instance
{
    /// Dimensions of the space we are indexing (in cells).
    GridmapCell dimensions;

    /// Root of the Quadtree. Allocated along with *this* instance.
    TreeNode root;

    Instance(GridmapCoord width, GridmapCoord height)
        : // Quadtree must subdivide the space equally into 1x1 unit cells.
          root(0, 0, ceilPow2(MAX_OF(width, height)))
    {
        dimensions[X] = width;
        dimensions[Y] = height;
    }

    /**
     * Construct and initialize a new (sub)tree.
     *
     * @param mcell        Cell coordinates in Gridmap space.
     * @param size         Size in Gridmap space units.
     *
     * @return  Newly allocated and initialized (sub)tree.
     */
    static TreeBase* newTree(const_GridmapCell mcell, GridmapCoord size)
    {
        if(size != 1)
        {
            return new TreeNode(mcell, size);
        }
        // Its a leaf.
        return new TreeLeaf(mcell);
    }

    static TreeLeaf* findLeafDescend(TreeBase& tree, const_GridmapCell mcell, bool alloc)
    {
        // Have we reached a leaf?
        if(!isLeaf(tree))
        {
            TreeNode& node = static_cast<TreeNode&>(tree);

            // Into which quadrant do we need to descend?
            GridmapCoord subSize = node.size() >> 1;
            TreeNode::Quadrant q;
            if(mcell[X] < node.x() + subSize)
            {
                q = (mcell[Y] < node.y() + subSize)? TreeNode::TopLeft  : TreeNode::BottomLeft;
            }
            else
            {
                q = (mcell[Y] < node.y() + subSize)? TreeNode::TopRight : TreeNode::BottomRight;
            }

            // Has this quadrant been initialized yet?
            TreeBase* child = node.child(q);
            if(!child)
            {
                // Are we allocating subtrees?
                if(!alloc) return 0;

                // Subdivide this tree and construct the new.
                GridmapCoord subOrigin[2];
                switch(q)
                {
                case TreeNode::TopLeft:
                    subOrigin[X] = node.x();
                    subOrigin[Y] = node.y();
                    break;
                case TreeNode::TopRight:
                    subOrigin[X] = node.x() + subSize;
                    subOrigin[Y] = node.y();
                    break;
                case TreeNode::BottomLeft:
                    subOrigin[X] = node.x();
                    subOrigin[Y] = node.y() + subSize;
                    break;
                case TreeNode::BottomRight:
                    subOrigin[X] = node.x() + subSize;
                    subOrigin[Y] = node.y() + subSize;
                    break;
                default:
                    throw de::Error("Gridmap::findLeafDescend", QString("Invalid quadrant %1").arg(int(q)));
                }

                child = newTree(subOrigin, subSize);
                node.setChild(q, child);
            }

            return findLeafDescend(*child, mcell, alloc);
        }

        // Found a leaf.
        return static_cast<TreeLeaf*>(&tree);
    }
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

bool Gridmap::clipCell(GridmapCell& cell) const
{
    bool adjusted = false;
    if(cell[X] >= d->dimensions[X])
    {
        cell[X] = d->dimensions[X]-1;
        adjusted = true;
    }
    if(cell[Y] >= d->dimensions[Y])
    {
        cell[Y] = d->dimensions[Y]-1;
        adjusted = true;
    }
    return adjusted;
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

void* Gridmap::cell(const_GridmapCell mcell)
{
    // Outside our boundary?
    if(mcell[X] >= d->dimensions[X] || mcell[Y] >= d->dimensions[Y]) return NULL;

    // Exisiting user data for this leaf?
    TreeLeaf* leaf = d->findLeafDescend(d->root, mcell, false);
    if(!leaf || !leaf->userData()) return 0;

    return leaf->userData();
}

Gridmap& Gridmap::setCell(const_GridmapCell mcell, void* userData)
{
    // Outside our boundary?
    if(mcell[X] >= d->dimensions[X] || mcell[Y] >= d->dimensions[Y]) return *this;

    TreeLeaf* leaf = d->findLeafDescend(d->root, mcell, true);
    DENG2_ASSERT(leaf);
    leaf->setUserData(userData);
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
static int actionCallback(TreeBase* node, void* parameters)
{
    TreeLeaf* leaf = static_cast<TreeLeaf*>(node);
    actioncallback_paramaters_t* p = (actioncallback_paramaters_t*) parameters;
    if(leaf->userData())
    {
        return p->callback(leaf->userData(), p->callbackParameters);
    }
    return 0; // Continue traversal.
}

int Gridmap::iterate(Gridmap_IterateCallback callback, void* parameters)
{
    actioncallback_paramaters_t p;
    p.callback = callback;
    p.callbackParameters = parameters;

    traversetree_parameters_t travParms;
    travParms.leafOnly = true;
    travParms.callback = actionCallback;
    travParms.callbackParameters = (void*)&p;
    return traverseTree(&d->root, travParms);
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
    TreeLeaf* leaf;
    int result;
    for(mcell[Y] = block.minY; mcell[Y] <= block.maxY; ++mcell[Y])
    for(mcell[X] = block.minX; mcell[X] <= block.maxX; ++mcell[X])
    {
        leaf = d->findLeafDescend(d->root, mcell, false);
        if(!leaf || !leaf->userData()) continue;

        result = callback(leaf->userData(), parameters);
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

static int drawCell(TreeBase* node, void* /*parameters*/)
{
    vec2f_t topLeft, bottomRight;

    V2f_Set(topLeft, UNIT_WIDTH * node->x(), UNIT_HEIGHT * node->y());
    V2f_Set(bottomRight, UNIT_WIDTH  * (node->x() + node->size()),
                         UNIT_HEIGHT * (node->y() + node->size()));

    glBegin(GL_LINE_LOOP);
        glVertex2fv((GLfloat*)topLeft);
        glVertex2f(bottomRight[X], topLeft[Y]);
        glVertex2fv((GLfloat*)bottomRight);
        glVertex2f(topLeft[X], bottomRight[Y]);
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
    TreeNode& root = const_cast<TreeNode&>(gm.d->root);
    glColor4f(1.f, 1.f, 1.f, 1.f / root.size());

    traversetree_parameters_t travParms;
    travParms.leafOnly = false;
    travParms.callback = drawCell;
    travParms.callbackParameters = 0;
    traverseTree(&root, travParms);

    /**
     * Draw our bounds.
     */
    vec2f_t start, end;
    V2f_Set(start, 0, 0);
    V2f_Set(end, UNIT_WIDTH * gm.width(), UNIT_HEIGHT * gm.height());

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
