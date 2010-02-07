/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2009-2010 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_NODEPILE_H
#define LIBDENG2_NODEPILE_H

#include "deng.h"

namespace de
{
    #define NP_ROOT_NODE ((void*) -1)

    /**
     * NodePiles are used when linking Things to LineDefs.
     *
     * Each Thing has a ring of linknodes, each node pointing to a LineDef
     * the Thing has been linked to.
     * Correspondingly each LineDef has a ring of nodes, with pointers to the
     * Things that are linked to that particular LineDef. This way it is
     * possible that a single Thing is linked simultaneously to multiple
     * LineDefs (which is common).
     */
    class NodePile
    {
    public:
        typedef dushort Index;
        static const dushort MAX_NODES = MAXUSHORT;

        struct LinkNode {
            Index prev, next;
            void* ptr;
            dint data;
        };

    public:
        /**
         * @param initial       Number of nodes to allocate.
         */
        explicit NodePile(dushort initial);

        /**
         * @note Deletes all nodes in the.
         */
        ~NodePile();

        /**
         * Adds a new node to the pile.
         * Pos always has the index of the next node to check when allocating
         * a new node. Pos shouldn't be accessed outside this routine because its
         * value may prove to be outside the valid range.
         *
         * @param ptr           Data to attach to the new node.
         */
        Index newIndex(void* ptr);

        /**
         * Links the node to the beginning of the ring.
         *
         * @param node          Node to be linked.
         * @param root          The root node to link the node to.
         */
        void link(Index node, Index root);

        /**
         * Unlink a node from the ring (make it a root node)
         *
         * @param pile          Nodepile ring to work with.
         * @param node          Node to be unlinked.
         */
        void unlink(Index node);

        /**
         * @pre Caller must unlink first!
         */
        void dismiss(Index node);

    public: /// @todo Should be private!
        dushort count;
        dushort pos;
        LinkNode* nodes;
    };
}

#endif /* LIBDENG2_NODEPILE_H */
