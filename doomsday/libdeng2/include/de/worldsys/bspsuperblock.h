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

#ifndef LIBDENG_BSPSUPERBLOCK_H
#define LIBDENG_BSPSUPERBLOCK_H

#include "deng.h"

#include "../HalfEdgeDS"

namespace de
{
    /**
     * bsp_superblock.h: Blockmap for half-edges
     *
     * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
     * SourceForge: http://sourceforge.net/projects/glbsp/
     */
    typedef struct superblock_s {
        // Bounding box coordinates.
        enum {
            BOXTOP      = 0,
            BOXBOTTOM   = 1,
            BOXLEFT     = 2,
            BOXRIGHT    = 3,
            BOXFLOOR    = 4,
            BOXCEILING  = 5
        };

        // Parent of this block, or NULL for a top-level block.
        struct superblock_s* parent;
        // Sub-blocks. NULL when empty. [0] has the lower coordinates, and
        // [1] has the higher coordinates. Division of a square always
        // occurs horizontally (e.g. 512x512 -> 256x512 -> 256x256).
        struct superblock_s* subs[2];

        // Coordinates on map for this block, from lower-left corner to
        // upper-right corner. Pseudo-inclusive, i.e (x,y) is inside block
        // if and only if BOXLEFT <= x < BOXRIGHT and BOXBOTTOM <= y < BOXTOP.
        dint            bbox[4];

        // Number of real half-edges and minihedges contained by this block
        // (including all sub-blocks below it).
        duint           realNum;
        duint           miniNum;

        // LIFO stack of half-edges completely contained by this block.
        struct superblock_listnode_s* _hEdges;
    } superblock_t;

    superblock_t*   BSP_CreateSuperBlock(void);
    void            BSP_DestroySuperBlock(superblock_t* block);

    void            SuperBlock_PushHEdge(superblock_t* SuperBlock, hedge_t* hEdge);
    hedge_t*        SuperBlock_PopHEdge(superblock_t* block);
    #if _DEBUG
    void            SuperBlock_PrintHEdges(superblock_t* SuperBlock);
    #endif

    void            SuperBlock_IncHEdgeCounts(superblock_t* SuperBlock, bool lineLinked);

    hedge_t*        SuperBlock_PickPartition(const superblock_t* SuperBlock, dint factor);

    // @todo Should be private to nodebuilder_t
    void            BSP_AddHEdgeToSuperBlock(superblock_t* block, hedge_t* hEdge);
    void            BSP_FindAABBForHEdges(const superblock_t* hEdgeList, dfloat* bbox);
}

#endif /* LIBDENG_BSPSUPERBLOCK_H */
