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

#ifndef LIBDENG2_NODEBUILDER_H
#define LIBDENG2_NODEBUILDER_H

#include "deng.h"

#include "../BSPEdge"
#include "../BSPIntersection"
#include "../BSPSuperBlock"
#include "../BinaryTree"

namespace de
{
    #define IFFY_LEN            4.0

    // Smallest degrees between two angles before being considered equal.
    #define ANG_EPSILON         (1.0 / 1024.0)

    typedef ddouble angle_g;  // Degrees, 0 is E, 90 is N

    /**
     * BSP node builder.
     * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
     * SourceForge: http://sourceforge.net/projects/glbsp/
     */
    typedef struct nodebuilder_s {
        dint            bspFactor;
        binarytree_t*   rootNode;
        dsize           numHalfEdgeInfo;
        struct hedge_info_s** halfEdgeInfo;

        struct map_s*   _map;
        struct cutlist_s* _cutList;
        struct superblock_s* _superBlockmap;

        struct superblock_s* _quickAllocSupers;
    } nodebuilder_t;

    nodebuilder_t*  P_CreateNodeBuilder(struct map_s* map, dint bspFactor);
    void            P_DestroyNodeBuilder(nodebuilder_t* nb);

    void            NodeBuilder_Build(nodebuilder_t* nb);

    // @todo Should be private to nodebuilder_t
    void            NodeBuilder_ConnectGaps(nodebuilder_t* nb, ddouble x, ddouble y, ddouble dX, ddouble dY,
                                            const hedge_t* partHEdge, struct superblock_s* rightList,
                                            struct superblock_s* leftList);
    hedge_t*        NodeBuilder_CreateHEdge(nodebuilder_t* nb, struct linedef_s* line, struct linedef_s* sourceLine,
                                            vertex_t* start, struct sector_s* sec, bool back);
    hedge_t*        NodeBuilder_SplitHEdge(nodebuilder_t* nb, hedge_t* oldHEdge, ddouble x, ddouble y);
    void            BSP_UpdateHEdgeInfo(const hedge_t* hEdge);
}

#endif /* LIBDENG2_NODEBUILDER_H */
