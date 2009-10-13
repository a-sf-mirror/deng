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

#ifndef DOOMSDAY_HALFEDGEDS_H
#define DOOMSDAY_HALFEDGEDS_H

typedef struct face_s {
    struct hedge_s*     hEdge; // First half-edge of this face.
    void*               data;
} face_t;

typedef struct hedge_s {
    struct vertex_s* vertex;
    struct hedge_s* twin;
    struct hedge_s* next;
    struct hedge_s* prev;
    struct face_s*  face;
    void*           data;
} hedge_t;

typedef struct {
    uint            numHEdges;
    hedge_t**       hEdges;

    uint            numFaces;
    face_t**        faces;
} halfedgeds_t;

#endif /* DOOMSDAY_HALFEDGEDS_H */
