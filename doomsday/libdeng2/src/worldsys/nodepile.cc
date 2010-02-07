/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#include "de/Log"
#include "de/NodePile"

using namespace de;

NodePile::NodePile(dushort initial)
{
    // Allocate room for at least two nodes. Node zero is never used.
    if(initial < 2)
        initial = 2;
    nodes = reinterpret_cast<LinkNode*>(std::calloc(1, initial * sizeof(LinkNode)));
    count = initial;
    pos = 1; // Index #1 is the first.
}

NodePile::~NodePile()
{
    if(nodes) std::free(nodes);
}

NodePile::Index NodePile::newIndex(void* ptr)
{
    LinkNode* node;
    LinkNode* end = nodes + count;
    LinkNode* newlist;

    pos %= count;
    node = nodes + pos++;

    // Scan for an unused node, starting from current pos.
    bool found = false;
    dushort i = 0, newcount;
    while(i < count - 1 && !found)
    {
        if(node == end)
            node = nodes + 1; // Wrap back to #1.
        if(!node->ptr)
        {   // This is the one!
            found = true;
        }
        else
        {
            i++;
            node++;
            pos++;
        }
    }

    if(!found)
    {
        // Damned, we ran out of nodes. Let's allocate more.
        if(count == MAX_NODES)
        {
            // This happens *theoretically* only in freakishly complex
            // maps with lots and lots of mobjs.
            LOG_ERROR("NodePile::newIndex: Out of linknodes! Contact the developer.");
        }

        // Double the number of nodes, but add at most 1024.
        if(count >= 1024)
            newcount = count + 1024;
        else
            newcount = count * 2;
        if(newcount > MAX_NODES)
            newcount = MAX_NODES;

        newlist = reinterpret_cast<LinkNode*>(std::malloc(sizeof(LinkNode) * newcount));
        memcpy(newlist, nodes, sizeof(*nodes) * count);
        memset(newlist + count, 0, (newcount - count) * sizeof(LinkNode));

        // Get rid of the old list and start using the new one.
        std::free(nodes);
        nodes = newlist;
        pos = count + 1;
        node = nodes + count;
        count = newcount;
    }

    node->ptr = ptr;
    // Make it point to itself by default (a root, basically).
    node->next = node->prev = node - nodes;
    return node->next; // Well, node's index, really.
}

void NodePile::link(Index node, Index root)
{
    LinkNode* nd = nodes;

    nd[node].prev = root;
    nd[node].next = nd[root].next;
    nd[root].next = nd[nd[node].next].prev = node;
}

void NodePile::unlink(Index node)
{
    LinkNode* nd = nodes;

    // Try deciphering that! :-) (d->n->p = d->p, d->p->n = d->n)
    nd[nd[nd[node].next].prev = nd[node].prev].next = nd[node].next;

    // Make it link to itself (a root, basically).
    nd[node].next = nd[node].prev = node;
}

void NodePile::dismiss(Index node)
{
    nodes[node].ptr = 0;
}
