/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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
 * bsp_node.h: BSP node builder. Recursive node creation.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

#ifndef __BSP_NODE_H__
#define __BSP_NODE_H__

#include "bsp_edge.h"
#include "bsp_intersection.h"
#include "m_binarytree.h"

typedef struct bspartition_s {
    double              x, y;
    double              dX, dY;
    double              length;
    linedef_t*          lineDef; // Not NULL if partition originated from a linedef.
    linedef_t*          sourceLine;

    double              pSX, pSY;
    double              pDX, pDY;
    double              pPara, pPerp;
} bspartition_t;

boolean     BuildNodes(struct superblock_s* hEdgeList, binarytree_t** parent,
                       size_t depth, cutlist_t* cutList);
void        BSP_AddHEdgeToSuperBlock(struct superblock_s* block,
                                     hedge_t* hEdge);

void        ClockwiseBspTree(binarytree_t* rootNode);

void        SaveMap(map_t* map, void* rootNode);
#endif
