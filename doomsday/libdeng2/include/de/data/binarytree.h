/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_BINARYTREE_H
#define LIBDENG2_BINARYTREE_H

#include "deng.h"

#include <cstdlib>

namespace de
{
    /**
     * A fairly standard binary tree implementation.
     */
    template <class T>
    class BinaryTree
    {
    public:
        /**
         * Construct a new tree (resultant tree is a "leaf").
         */
        BinaryTree();

        /**
         * Construct a new tree, setting just the user data (resultant tree is a "leaf").
         *
         * @param data          User-data to be associated with the new (sub)tree.
         */
        BinaryTree(T data);

        /**
         * Construct a new tree, setting the user data plus right and left children.
         *
         * @param data          User-data to be associated with the new tree.
         * @param initialRight  Ptr to the tree to link as the right child.
         * @param initialLeft   Ptr to the tree to link as the left child.
         */
        BinaryTree(T data, BinaryTree* initialRight, BinaryTree* initialLeft);

        ~BinaryTree();

        /**
         * Retrieve a child of the tree.
         *
         * @param left          @c true= retrieve the left child.
         *                      @c false= retrieve the right child.
         * @return              Ptr to the requested child if present ELSE @c NULL.
         */
        BinaryTree* child(bool left) const { return left? _leftChild : _rightChild; }
        BinaryTree* right() const { return _rightChild; }
        BinaryTree* left() const { return _leftChild; }

        /**
         * Set a child of the tree.
         *
         * @param left          @c true= set the left child.
         *                      @c false= set the right child.
         * @param subTree       Ptr to the (child) tree to be linked or @c NULL.
         */
        void setChild(bool left, BinaryTree* subTree);
        void setRight(BinaryTree* subTree) { setChild(false, subTree); }
        void setLeft(BinaryTree* subTree) { setChild(true, subTree); }

        /**
         * Retrieve the user data associated with the specified (sub)tree.
         *
         * @return              Ptr to the user data if present ELSE @c NULL.
         */
        T data() const { return _data; }

        /**
         * Set the user data assoicated with the specified (sub)tree.
         *
         * @param data          Ptr to the user data.
         */
        void setData(T data) { _data = data; }

        /**
         * Is this tree a "leaf" (has no children)?
         *
         * @return              @c true iff this tree is a leaf.
         */
        bool isLeaf() const { return !(_rightChild || _leftChild); }

        /**
         * Calculate the height of the tree.
         *
         * @return              Height of the tree.
         */
        dsize height() const { return findHeight(); }

        /**
         * Traverse the tree in "Preorder".
         *
         * Make a callback for all nodes of the tree (including the root).
         * Traversal continues until all nodes have been visited or a callback
         * returns @c false; at which point traversal is aborted.
         *
         * @param callback      Function to call for each object of the tree.
         * @param paramaters    Used to pass additional data to the callback.
         *
         * @return              @c true, iff all callbacks return @c true;
         */
        bool preOrder(bool (C_DECL *callback) (BinaryTree* tree, void* data), void* paramaters = 0);

        /**
         * Traverse the tree in "Inorder".
         *
         * Make a callback for all nodes of the tree (including the root).
         * Traversal continues until all nodes have been visited or a callback
         * returns @c false; at which point traversal is aborted.
         *
         * @param callback      Function to call for each object of the tree.
         * @param paramaters    Used to pass additional data to the callback.
         *
         * @return              @c true, iff all callbacks return @c true;
         */
        bool inOrder(bool (C_DECL *callback) (BinaryTree* tree, void* data), void* paramaters = 0);

        /**
         * Traverse the tree in "Postorder".
         *
         * Make a callback for all nodes of the tree (including the root).
         * Traversal continues until all nodes have been visited or a callback
         * returns @c false; at which point traversal is aborted.
         *
         * @param callback      Function to call for each object of the tree.
         * @param paramaters    Used to pass additional data to the callback.
         *
         * @return              @c true, iff all callbacks return @c true;
         */
        bool postOrder(bool (C_DECL *callback) (BinaryTree* tree, void* data), void* paramaters = 0);

    private:
        /// User data associated with this tree.
        T _data;

        /// Right and Left subtrees.
        BinaryTree* _rightChild, *_leftChild;

        dsize findHeight() const;
    };

    template <class T>
    BinaryTree<T>::BinaryTree() : _data(0), _rightChild(0), _leftChild(0)
    {}

    template <class T>
    BinaryTree<T>::BinaryTree(T data) : _data(data), _rightChild(0), _leftChild(0)
    {}

    template <class T>
    BinaryTree<T>::BinaryTree(T data, BinaryTree* initialRight, BinaryTree* initialLeft)
      :  _data(data), _rightChild(initialRight), _leftChild(initialLeft)
    {}

    template <class T>
    BinaryTree<T>::~BinaryTree()
    {
        if(_rightChild) delete _rightChild;
        if(_leftChild)  delete _leftChild;
    }

    template <class T>
    void BinaryTree<T>::setChild(bool left, BinaryTree* subTree)
    {
        if(left)
            _leftChild = subTree;
        else
            _rightChild = subTree;
    }

    template <class T>
    dsize BinaryTree<T>::findHeight() const
    {
        if(!isLeaf())
        {
            dsize right;
            if(_rightChild)
                right = _rightChild->findHeight();
            else
                right = 0;

            dsize left;
            if(_leftChild)
                left = _leftChild->findHeight();
            else
                left = 0;

            return max(left, right) + 1;
        }

        return 0;
    }

    template <class T>
    bool BinaryTree<T>::preOrder(bool (C_DECL *callback) (BinaryTree* tree, void* data), void* data)
    {
        // Visit this node.
        if(!callback(this, data))
            return false;

        if(!isLeaf())
        {
            if(_rightChild && !_rightChild->preOrder(callback, data))
                return false;

            if(_leftChild && !_leftChild->preOrder(callback, data))
                return false;
        }

        return true;
    }

    template <class T>
    bool BinaryTree<T>::inOrder(bool (C_DECL *callback) (BinaryTree* tree, void* data), void* data)
    {
        if(!isLeaf())
        {
            if(_rightChild && !_rightChild->inOrder(callback, data))
                return false;
        }

        // Visit this node.
        if(!callback(this, data))
            return false;

        if(!isLeaf())
        {
            if(_leftChild && !_leftChild->inOrder(callback, data))
                return false;
        }

        return true;
    }

    template <class T>
    bool BinaryTree<T>::postOrder(bool (C_DECL *callback) (BinaryTree* tree, void* data), void* data)
    {
        if(!isLeaf())
        {
            if(_rightChild && !_rightChild->postOrder(callback, data))
                return false;

            if(_leftChild && !_leftChild->postOrder(callback, data))
                return false;
        }

        // Visit this node.
        if(!callback(this, data))
            return false;

        return true;
    }
}

#endif /* LIBDENG2_BINARYTREE_H */
