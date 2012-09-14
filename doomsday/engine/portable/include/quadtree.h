/**
 * @file quadtree.h
 * Region quadtree data structure.
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

#ifndef LIBDENG_DATA_QUADTREE_H
#define LIBDENG_DATA_QUADTREE_H

#include "dd_types.h"
#include "gridmapcellblock.h"

class Quadtree
{
public:
    /// Space dimension ordinals.
    enum { X = 0, Y };

    /// Child quadrant identifiers.
    enum Quadrant
    {
        TopLeft = 0,
        TopRight,
        BottomLeft,
        BottomRight
    };

    /// Abstract class used as the base for all tree objects.
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

    /// TreeLeaf. Represents a subspace leaf within the quadtree.
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

    /// TreeNode. Represents a subspace node within the quadtree.
    class TreeNode : public TreeBase
    {
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

    /// Parameters for the traverseTree() method.
    /// @todo Refactor me away.
    struct traversetree_parameters_t
    {
        bool leafOnly;
        int (*callback) (Quadtree::TreeBase* node, void* parameters);
        void* callbackParameters;
    };

public:
    Quadtree(GridmapCoord width, GridmapCoord height)
        : // Quadtree must subdivide the space equally into 1x1 unit cells.
          root_(0, 0, ceilPow2(MAX_OF(width, height)))
    {
        dimensions[X] = width;
        dimensions[Y] = height;
    }

    operator TreeBase&() { return root(); }
    operator TreeBase const&() const { return root(); }

    TreeBase& root() { return root_; }
    TreeBase const& root() const { return root_; }

    GridmapCoord width() const { return dimensions[X]; }

    GridmapCoord height() const { return dimensions[Y]; }

    const GridmapCell& widthHeight() const { return dimensions; }

    bool clipCell(GridmapCell& cell) const
    {
        bool adjusted = false;
        if(cell[X] >= dimensions[X])
        {
            cell[X] = dimensions[X]-1;
            adjusted = true;
        }
        if(cell[Y] >= dimensions[Y])
        {
            cell[Y] = dimensions[Y]-1;
            adjusted = true;
        }
        return adjusted;
    }

    bool clipBlock(GridmapCellBlock& block) const
    {
        bool adjusted = false;
        if(block.minX >= dimensions[X])
        {
            block.minX = dimensions[X]-1;
            adjusted = true;
        }
        if(block.minY >= dimensions[Y])
        {
            block.minY = dimensions[Y]-1;
            adjusted = true;
        }
        if(block.maxX >= dimensions[X])
        {
            block.maxX = dimensions[X]-1;
            adjusted = true;
        }
        if(block.maxY >= dimensions[Y])
        {
            block.maxY = dimensions[Y]-1;
            adjusted = true;
        }
        return adjusted;
    }

    void* cell(const_GridmapCell mcell)
    {
        // Outside our boundary?
        if(mcell[X] >= dimensions[X] || mcell[Y] >= dimensions[Y]) return NULL;

        // Exisiting user data for this leaf?
        TreeLeaf* leaf = findLeafDescend(root_, mcell, false);
        if(!leaf || !leaf->userData()) return 0;

        return leaf->userData();
    }

    Quadtree& setCell(const_GridmapCell mcell, void* userData)
    {
        // Outside our boundary?
        if(mcell[X] >= dimensions[X] || mcell[Y] >= dimensions[Y]) return *this;

        TreeLeaf* leaf = findLeafDescend(root_, mcell, true);
        DENG2_ASSERT(leaf);
        leaf->setUserData(userData);
        return *this;
    }

    TreeLeaf* findLeaf(const_GridmapCell mcell, bool alloc)
    {
        return findLeafDescend(root_, mcell, alloc);
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
            Quadrant q;
            if(mcell[X] < node.x() + subSize)
            {
                q = (mcell[Y] < node.y() + subSize)? TopLeft  : BottomLeft;
            }
            else
            {
                q = (mcell[Y] < node.y() + subSize)? TopRight : BottomRight;
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
                case TopLeft:
                    subOrigin[X] = node.x();
                    subOrigin[Y] = node.y();
                    break;
                case TopRight:
                    subOrigin[X] = node.x() + subSize;
                    subOrigin[Y] = node.y();
                    break;
                case BottomLeft:
                    subOrigin[X] = node.x();
                    subOrigin[Y] = node.y() + subSize;
                    break;
                case BottomRight:
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
    static int traverseTree(Quadtree::TreeBase* tree, traversetree_parameters_t const& p)
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

    static inline bool isLeaf(TreeBase& tree)
    {
        return tree.size() == 1;
    }

    static GridmapCoord ceilPow2(GridmapCoord unit)
    {
        GridmapCoord cumul;
        for(cumul = 1; unit > cumul; cumul <<= 1);
        return cumul;
    }

private:
    /// Root of the Quadtree. Allocated along with *this* instance.
    TreeNode root_;

    /// Dimensions of the space we are indexing (in cells).
    GridmapCell dimensions;
};

#endif /// LIBDENG_DATA_QUADTREE_H
