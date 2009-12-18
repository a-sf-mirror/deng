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

typedef struct vertex_s {
    struct hedge_s* hEdge;
    double          pos[2]; // @todo Move out of the base class.
    void*           data;
} vertex_t;

typedef struct hedge_s {
    struct vertex_s* vertex;
    struct hedge_s* twin;
    struct hedge_s* next;
    struct hedge_s* prev;
    struct face_s*  face;
    void*           data;
} hedge_t;

typedef struct face_s {
    hedge_t*        hEdge; // First half-edge of this face.
    void*           data;
} face_t;

typedef struct {
    uint            _numVertices;
    vertex_t**      vertices;

    uint            _numHEdges;
    hedge_t**       _hEdges;

    uint            _numFaces;
    face_t**        _faces;
} halfedgeds_t;

halfedgeds_t*       P_CreateHalfEdgeDS(void);
void                P_DestroyHalfEdgeDS(halfedgeds_t* halfEdgeDS);

vertex_t*           HalfEdgeDS_CreateVertex(halfedgeds_t* halfEdgeDS);
hedge_t*            HalfEdgeDS_CreateHEdge(halfedgeds_t* halfEdgeDS, vertex_t* vertex);
face_t*             HalfEdgeDS_CreateFace(halfedgeds_t* halfEdgeDS);

uint                HalfEdgeDS_NumVertices(halfedgeds_t* halfEdgeDS);
uint                HalfEdgeDS_NumHEdges(halfedgeds_t* halfEdgeDS);
uint                HalfEdgeDS_NumFaces(halfedgeds_t* halfEdgeDS);
int                 HalfEdgeDS_IterateVertices(halfedgeds_t* halfEdgeDS, int (*callback) (vertex_t*, void*), void* context);
int                 HalfEdgeDS_IterateHEdges(halfedgeds_t* halfEdgeDS, int (*callback) (hedge_t*, void*), void* context);
int                 HalfEdgeDS_IterateFaces(halfedgeds_t* halfEdgeDS, int (*callback) (face_t*, void*), void* context);

hedge_t*            HEdge_Split(halfedgeds_t* halfEdgeDS, hedge_t* oldHEdge);

void                Face_LinkHEdge(face_t* face, hedge_t* hEdge);
void                Face_UnlinkHEdge(face_t* face, hedge_t* hEdge);
void                Face_ClockwiseOrder(face_t* face);
void                Face_SwitchToHEdgeLinks(face_t* face);

void                testVertexHEdgeRings(vertex_t* v);
#endif /* DOOMSDAY_HALFEDGEDS_H */
