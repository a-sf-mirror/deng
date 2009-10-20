/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_NODEPILE_H
#define DOOMSDAY_NODEPILE_H

#define NP_ROOT_NODE ((void*) -1)

/**
 * Linknodes are used when linking mobjs to lines. Each mobj has a ring
 * of linknodes, each node pointing to a line the mobj has been linked to.
 * Correspondingly each line has a ring of nodes, with pointers to the
 * mobjs that are linked to that particular line. This way it is possible
 * that a single mobj is linked simultaneously to multiple lines (which
 * is common).
 *
 * All these rings are maintained by Mobj_(Un)Link().
 */
typedef struct linknode_s {
    nodeindex_t     prev, next;
    void*           ptr;
    int             data;
} linknode_t;

typedef struct nodepile_s {
    int             count;
    int             pos;
    struct linknode_s* nodes;
} nodepile_t;

nodepile_t*     P_CreateNodePile(int initial);
void            P_DestroyNodePile(nodepile_t* pile);

nodeindex_t     NP_New(nodepile_t* pile, void* ptr);
void            NP_Link(nodepile_t* pile, nodeindex_t node, nodeindex_t root);
void            NP_Unlink(nodepile_t* pile, nodeindex_t node);

#define NP_Dismiss(pile, node) (pile->nodes[node].ptr = 0)

#endif /* DOOMSDAY_NODEPILE_H */
