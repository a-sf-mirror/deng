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
#include "api/aabox.h"

typedef uint QuadtreeCoord;
typedef QuadtreeCoord QuadtreeCell[2];
typedef const QuadtreeCoord const_QuadtreeCell[2];
typedef AABoxu QuadtreeCellBlock;

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
        explicit TreeBase(QuadtreeCoord x = 0, QuadtreeCoord y = 0)
        {
            origin[X] = x;
            origin[Y] = y;
        }

        virtual ~TreeBase() {}

        inline QuadtreeCoord x() const { return origin[X]; }
        inline QuadtreeCoord y() const { return origin[Y]; }
        inline const QuadtreeCell& xy() const { return origin; }

        virtual QuadtreeCoord size() const =0;

    protected:
        /// Origin of this node in Gridmap space [x, y].
        QuadtreeCell origin;
    };

    /// TreeLeaf. Represents a subspace leaf within the quadtree.
    class TreeLeaf : public TreeBase
    {
    public:
        inline QuadtreeCoord size() const { return 1; }

        inline void* value() const { return value_; }

        TreeLeaf& setValue(void* newValue)
        {
            // Exisiting data value for this leaf?
            if(value_)
            {
                Z_Free(value_);
            }
            value_ = newValue;
            return *this;
        }

        friend class Quadtree;

    private:
        explicit TreeLeaf(QuadtreeCoord x = 0, QuadtreeCoord y = 0, void* value = 0)
            : TreeBase(x, y), value_(value)
        {}

        explicit TreeLeaf(const_QuadtreeCell mcell, void* value = 0)
            : TreeBase(mcell[X], mcell[Y]), value_(value)
        {}

        ~TreeLeaf()
        {
            if(value_) Z_Free(value_);
        }

    private:
        /// Data value at this tree leaf.
        void* value_;
    };

    /// TreeNode. Represents a subspace node within the quadtree.
    class TreeNode : public TreeBase
    {
    public:
        inline QuadtreeCoord size() const { return size_; }

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
                throw de::Error("Quadtree::TreeNode::child", QString("Invalid Quadrant %1").arg(int(q)));
            }
        }

        /// @note Does nothing about any existing child tree. Caller must ensure that this
        ///       is logical and does not result in memory leakage.
        TreeNode& setChild(Quadrant q, TreeBase* newChild)
        {
            children[q] = newChild;
            return *this;
        }

        friend class Quadtree;

    private:
        explicit TreeNode(QuadtreeCoord x = 0, QuadtreeCoord y = 0, QuadtreeCoord size = 0)
            : TreeBase(x, y), size_(size)
        {
            children[TopLeft] = children[TopRight] = children[BottomLeft] = children[BottomRight] = 0;
        }

        TreeNode(const_QuadtreeCell origin, QuadtreeCoord size = 0)
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

    private:
        /// Size of this cell in Gridmap space (width=height).
        QuadtreeCoord size_;

        /// Child cells of this, one for each subquadrant.
        TreeBase* children[4];
    };

    /// Parameters for the traverse() method.
    /// @todo Refactor me away.
    struct traverse_parameters_t
    {
        bool leafOnly;
        int (*callback) (TreeBase* node, void* parameters);
        void* callbackParameters;
    };

public:
    Quadtree(QuadtreeCoord width, QuadtreeCoord height)
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

    QuadtreeCoord width() const { return dimensions[X]; }

    QuadtreeCoord height() const { return dimensions[Y]; }

    const QuadtreeCell& widthHeight() const { return dimensions; }

    bool clipCell(QuadtreeCell& cell) const
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

    bool clipBlock(QuadtreeCellBlock& block) const
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

    bool leafAtCell(const_QuadtreeCell mcell)
    {
        // Outside our boundary?
        if(mcell[X] >= dimensions[X] || mcell[Y] >= dimensions[Y]) return false;

        // A leaf with an exisiting data value?
        TreeLeaf* leaf = findLeafDescend(root_, mcell, false);
        return (leaf && leaf->value());
    }

    void* cell(const_QuadtreeCell mcell)
    {
        // Outside our boundary?
        DENG2_ASSERT(mcell[X] < dimensions[X] && mcell[Y] < dimensions[Y]);

        // Exisiting data for this leaf?
        TreeLeaf* leaf = findLeafDescend(root_, mcell, false);
        if(!leaf || !leaf->value()) return 0;

        return leaf->value();
    }

    Quadtree& setCell(const_QuadtreeCell mcell, void* newValue)
    {
        // Outside our boundary?
        DENG2_ASSERT(mcell[X] < dimensions[X] && mcell[Y] < dimensions[Y]);

        TreeLeaf* leaf = findLeafDescend(root_, mcell, true);
        DENG2_ASSERT(leaf);
        leaf->setValue(newValue);
        return *this;
    }

    TreeLeaf* findLeaf(const_QuadtreeCell mcell, bool alloc)
    {
        return findLeafDescend(root_, mcell, alloc);
    }

    static inline bool isLeaf(TreeBase& tree)
    {
        return tree.size() == 1;
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
    static int traverse(TreeBase* tree, traverse_parameters_t const& p)
    {
        int result;
        if(!isLeaf(*tree))
        {
            TreeNode* node = static_cast<TreeNode*>(tree);
            if(node->topLeft())
            {
                result = traverse(node->topLeft(), p);
                if(result) return result;
            }
            if(node->topRight())
            {
                result = traverse(node->topRight(), p);
                if(result) return result;
            }
            if(node->bottomLeft())
            {
                result = traverse(node->bottomLeft(), p);
                if(result) return result;
            }
            if(node->bottomRight())
            {
                result = traverse(node->bottomRight(), p);
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

    /**
     * Iterate over all leaf nodes in the quadtree, making a callback for each.
     * Iteration ends when all leafs have been visited or a callback returns a
     * non-zero value.
     *
     * @param callback      Callback function.
     * @param parameters    Passed to the callback.
     *
     * @return  Zero iff iteration completed wholly, else the value returned by
     *          the last callback made.
     */
    int iterateLeafs(int (*callback) (TreeBase* node, void* parameters), void* parameters)
    {
        traverse_parameters_t travParms;
        travParms.leafOnly = true;
        travParms.callback = callback;
        travParms.callbackParameters = parameters;
        return traverse(&root_, travParms);
    }

private:
    /**
     * Construct and initialize a new (sub)tree.
     *
     * @param mcell        Cell coordinates in Gridmap space.
     * @param size         Size in Gridmap space units.
     *
     * @return  Newly allocated and initialized (sub)tree.
     */
    static TreeBase* newTree(const_QuadtreeCell mcell, QuadtreeCoord size)
    {
        if(size != 1)
        {
            return new TreeNode(mcell, size);
        }
        // Its a leaf.
        return new TreeLeaf(mcell);
    }

    static TreeLeaf* findLeafDescend(TreeBase& tree, const_QuadtreeCell mcell, bool alloc)
    {
        // Have we reached a leaf?
        if(!isLeaf(tree))
        {
            TreeNode& node = static_cast<TreeNode&>(tree);

            // Into which quadrant do we need to descend?
            QuadtreeCoord subSize = node.size() >> 1;
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
                QuadtreeCoord subOrigin[2];
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
                    throw de::Error("Quadtree::findLeafDescend", QString("Invalid quadrant %1").arg(int(q)));
                }

                child = newTree(subOrigin, subSize);
                node.setChild(q, child);
            }

            return findLeafDescend(*child, mcell, alloc);
        }

        // Found a leaf.
        return static_cast<TreeLeaf*>(&tree);
    }

    static QuadtreeCoord ceilPow2(QuadtreeCoord unit)
    {
        QuadtreeCoord cumul;
        for(cumul = 1; unit > cumul; cumul <<= 1);
        return cumul;
    }

private:
    /// Root of the Quadtree. Allocated along with *this* instance.
    TreeNode root_;

    /// Dimensions of the space we are indexing (in cells).
    QuadtreeCell dimensions;
};

#endif /// LIBDENG_DATA_QUADTREE_H
