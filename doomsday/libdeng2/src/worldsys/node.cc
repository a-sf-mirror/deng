/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
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

/**
 * node.c: Parition nodes. Used with the BSP.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

node_t* P_CreateNode(float x, float y, float dX, float dY, float rightAABB[4],
                     float leftAABB[4])
{
    node_t* node = Z_Calloc(sizeof(*node), PU_STATIC, 0);

    node->partition.x = x;
    node->partition.y = y;
    node->partition.dX = dX;
    node->partition.dY = dY;
    memcpy(node->bBox[RIGHT], rightAABB, sizeof(node->bBox[RIGHT]));
    memcpy(node->bBox[LEFT], leftAABB, sizeof(node->bBox[LEFT]));
    return node;
}

void P_DestroyNode(node_t* node)
{
    assert(node);
    Z_Free(node);
}
