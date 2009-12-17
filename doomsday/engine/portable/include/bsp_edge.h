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
 * bsp_edge.h: Choose the best half-edge to use for a node line.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

#ifndef __BSP_EDGE_H__
#define __BSP_EDGE_H__

#include "halfedgeds.h"

typedef struct bsp_hedgeinfo_s {
    // The SuperBlock that contains this half-edge, or NULL if the half-edge
    // is no longer in any SuperBlock (e.g., now in a leaf).
    struct superblock_s* block;

    // Precomputed data for faster calculations.
    double              pDX, pDY;
    double              pLength;
    double              pAngle;
    double              pPara;
    double              pPerp;

    // LineDef that this half-edge goes along, or NULL if miniseg.
    struct linedef_s*   lineDef;
    hedge_t*            lprev, *lnext; // Previous and next half-edges along linedef side.

    // LineDef that this half-edge initially comes from.
    // For "real" half-edges, this is just the same as the 'linedef' field
    // above. For "miniedges", this is the linedef of the partition line.
    struct linedef_s*   sourceLine;

    struct sector_s*    sector; // Adjacent sector or, NULL if minihedge / twin on single sided linedef.
    byte                side; // 0 for right, 1 for left.
} bsp_hedgeinfo_t;


#endif
