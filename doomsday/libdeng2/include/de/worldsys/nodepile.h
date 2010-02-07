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
        typedef dshort Index;

        struct LinkNode {
            Index prev, next;
            void* ptr;
            dint data;
        };

    public:
        NodePile(dint initial);
        ~NodePile();

        Index newIndex(void* ptr);
        void link(Index node, Index root);
        void unlink(Index node);

    public: /// @todo Should be private!
        dint count;
        dint pos;
        LinkNode* nodes;

        void dismiss(Index node) { nodes[node].ptr = 0; };
    };
}

#endif /* LIBDENG2_NODEPILE_H */
