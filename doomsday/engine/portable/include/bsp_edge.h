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

#define IFFY_LEN            4.0

// Smallest distance between two points before being considered equal.
#define DIST_EPSILON        (1.0 / 128.0)

// Smallest degrees between two angles before being considered equal.
#define ANG_EPSILON         (1.0 / 1024.0)

typedef struct {
    // The superblock that contains this half-edge, or NULL if the half-edge
    // is no longer in any superblock (e.g., now in a leaf).
    struct superblock_s* block;
    // The leaf that contains this half-edge, or NULL if the half-edge is
    // not yet in a leaf (e.g., it is in a superblock).
    struct bspleafdata_s* leaf;

    // Precomputed data for faster calculations.
    double              pSX, pSY;
    double              pEX, pEY;
    double              pDX, pDY;

    double              pLength;
    double              pAngle;
    double              pPara;
    double              pPerp;

    // LineDef that this half-edge goes along, or NULL if miniseg.
    linedef_t*          lineDef;
    hedge_t*            lprev, *lnext; // Previous and next half-edges along linedef side.

    // LineDef that this half-edge initially comes from.
    // For "real" half-edges, this is just the same as the 'linedef' field
    // above. For "miniedges", this is the linedef of the partition line.
    linedef_t*          sourceLine;

    sector_t*           sector; // Adjacent sector or, NULL if minihedge / twin on single sided linedef.
    byte                side; // 0 for right, 1 for left.
} bsp_hedgeinfo_t;

void        BSP_InitHEdgeAllocator(void);
void        BSP_ShutdownHEdgeAllocator(void);

hedge_t*    BSP_CreateHEdge(linedef_t* line, linedef_t* sourceLine,
                            vertex_t* start, sector_t* sec, boolean back);

hedge_t*    HEdge_Split(hedge_t* oldHEdge, double x, double y);

void        BSP_UpdateHEdgeInfo(const hedge_t* hEdge);
#endif
