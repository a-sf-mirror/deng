/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * m_nodepile.c: Specialized Node Allocation.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define NP_MAX_NODES    65535   // Indices are shorts.

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Initialize (alloc) the nodepile with n nodes.
 *
 * @param initial       Number of nodes to allocate.
 */
NodePile* P_CreateNodePile(int initial)
{
    NodePile* pile = Z_Malloc(sizeof(*pile), PU_STATIC, 0);
    size_t size;

    // Allocate room for at least two nodes.
    // Node zero is never used.
    if(initial < 2)
        initial = 2;

    size = sizeof(*pile->nodes) * initial;
    pile->nodes = Z_Calloc(size, PU_STATIC, 0);
    pile->count = initial;
    // Index #1 is the first.
    pile->pos = 1;

    return pile;
}

/**
 * Destroy all nodes in the specified pile and then the pile itself.
 *
 * @param pile          Node pile to be destroyed.
 */
void P_DestroyNodePile(NodePile* pile)
{
    assert(pile);

    if(pile->nodes)
        Z_Free(pile->nodes);
    Z_Free(pile);
}

/**
 * Adds a new node to a node pile.
 * Pos always has the index of the next node to check when allocating
 * a new node. Pos shouldn't be accessed outside this routine because its
 * value may prove to be outside the valid range.
 *
 * @param pile          Ptr to nodepile to add the node to.
 * @param ptr           Data to attach to the new node.
 */
NodePileIndex NP_New(NodePile* pile, void* ptr)
{
    LinkNode* node;
    LinkNode* end = pile->nodes + pile->count;
    int i, newcount;
    LinkNode* newlist;
    boolean found;

    pile->pos %= pile->count;
    node = pile->nodes + pile->pos++;

    // Scan for an unused node, starting from current pos.
    i = 0;
    found = false;
    while(i < pile->count - 1 && !found)
    {
        if(node == end)
            node = pile->nodes + 1; // Wrap back to #1.
        if(!node->ptr)
        {
            // This is the one!
            found = true;
        }
        else
        {
            i++;
            node++;
            pile->pos++;
        }
    }

    if(!found)
    {
        // Damned, we ran out of nodes. Let's allocate more.
        if(pile->count == NP_MAX_NODES)
        {
            // This happens *theoretically* only in freakishly complex
            // maps with lots and lots of mobjs.
            Con_Error("NP_New: Out of linknodes! Contact the developer.\n");
        }

        // Double the number of nodes, but add at most 1024.
        if(pile->count >= 1024)
            newcount = pile->count + 1024;
        else
            newcount = pile->count * 2;
        if(newcount > NP_MAX_NODES)
            newcount = NP_MAX_NODES;

        newlist = Z_Malloc(sizeof(*newlist) * newcount, PU_STATIC, 0);
        memcpy(newlist, pile->nodes, sizeof(*pile->nodes) * pile->count);
        memset(newlist + pile->count, 0,
               (newcount - pile->count) * sizeof(*newlist));

        // Get rid of the old list and start using the new one.
        Z_Free(pile->nodes);
        pile->nodes = newlist;
        pile->pos = pile->count + 1;
        node = pile->nodes + pile->count;
        pile->count = newcount;
    }

    node->ptr = ptr;
    // Make it point to itself by default (a root, basically).
    node->next = node->prev = node - pile->nodes;
    return node->next; // Well, node's index, really.
}

/**
 * Links the node to the beginning of the ring.
 *
 * @param pile          Nodepile ring to work with.
 * @param node          Node to be linked.
 * @param root          The root node to link the node to.
 */
void NP_Link(NodePile* pile, NodePileIndex node, NodePileIndex root)
{
    LinkNode* nd = pile->nodes;

    nd[node].prev = root;
    nd[node].next = nd[root].next;
    nd[root].next = nd[nd[node].next].prev = node;
}

/**
 * Unlink a node from the ring (make it a root node)
 *
 * @param pile          Nodepile ring to work with.
 * @param node          Node to be unlinked.
 */
void NP_Unlink(NodePile* pile, NodePileIndex node)
{
    LinkNode* nd = pile->nodes;

    // Try deciphering that! :-) (d->n->p = d->p, d->p->n = d->n)
    nd[nd[nd[node].next].prev = nd[node].prev].next = nd[node].next;

    // Make it link to itself (a root, basically).
    nd[node].next = nd[node].prev = node;
}

#if 0 // This is now a macro.
/**
 * Caller must unlink first.
 */
void NP_Dismiss(NodePile *pile, NodePileIndex node)
{
    pile->nodes[node].ptr = 0;
}
#endif
