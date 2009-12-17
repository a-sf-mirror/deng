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
 * bsp_intersection.h: Segment intersections.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

#ifndef BSP_INTERSECTION_H
#define BSP_INTERSECTION_H

// The intersection list is kept sorted by along_dist, in ascending order.
typedef struct cutlist_s {
    struct cnode_s* head;
    struct cnode_s* unused;
} cutlist_t;

cutlist_t*      BSP_CutListCreate(void);
void            BSP_CutListDestroy(cutlist_t* cutList);

void            CutList_Intersect(cutlist_t* list, struct vertex_s* vertex, double distance);
boolean         CutList_Find(cutlist_t* cutList, struct vertex_s* v);
void            CutList_Reset(cutlist_t* cutList);

// @todo Should be private to nodebuilder_t
void            BSP_MergeOverlappingIntersections(cutlist_t* list);
#endif /* BSP_INTERSECTION_H */
