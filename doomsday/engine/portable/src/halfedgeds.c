/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008-2009 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <stdlib.h>

#include "de_play.h" // @todo Needed for mvertex_t which should not be referenced here anyway.

#include "halfedgeds.h"

// MACROS ------------------------------------------------------------------

#define PI_D            3.14159265358979323846
// Smallest degrees between two angles before being considered equal.
#define ANG_EPSILON     (1.0 / 1024.0)

// TYPES -------------------------------------------------------------------

typedef struct hedge_node_s {
    hedge_t*        hEdge;
    struct hedge_node_s* next, *prev;
} hedge_node_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static __inline vertex_t* allocVertex(void)
{
    return calloc(sizeof(vertex_t), 1);
}

static __inline void freeVertex(vertex_t* vertex)
{
    free(vertex);
}

static __inline hedge_t* allocHEdge(void)
{
    return calloc(sizeof(hedge_t), 1);
}

static __inline void freeHEdge(hedge_t* hEdge)
{
    free(hEdge);
}

static __inline hedge_node_t* allocHEdgeNode(void)
{
    return malloc(sizeof(hedge_node_t));
}

static __inline void freeHEdgeNode(hedge_node_t* node)
{
    free(node);
}

static __inline face_t* allocFace(void)
{
    return calloc(sizeof(face_t), 1);
}

static __inline void freeFace(face_t* face)
{
    free(face);
}

/**
 * Translate (dx, dy) into an angle value (degrees).
 */
static double SlopeToAngle(double dx, double dy)
{
    double angle;

    if(dx == 0)
        return (dy > 0? 90.0 : 270.0);

    angle = atan2((double) dy, (double) dx) * 180.0 / PI_D;

    if(angle < 0)
        angle += 360.0;

    return angle;
}

halfedgeds_t* P_CreateHalfEdgeDS(void)
{
    halfedgeds_t* halfEdgeDS = calloc(sizeof(*halfEdgeDS), 1);
    return halfEdgeDS;
}

/**
 * @note Only releases memory for the data structure itself, any objects linked
 * to the component parts of the data structure will remain (therefore this is
 * the caller's responsibility).
 */
void P_DestroyHalfEdgeDS(halfedgeds_t* halfEdgeDS)
{
    if(halfEdgeDS->_faces)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->_numFaces; ++i)
        {
            face_t* face = halfEdgeDS->_faces[i];
            freeFace(face);
        }

        free(halfEdgeDS->_faces);
    }
    halfEdgeDS->_faces = NULL;
    halfEdgeDS->_numFaces = 0;

    if(halfEdgeDS->_hEdges)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->_numHEdges; ++i)
        {
            hedge_t* hEdge = halfEdgeDS->_hEdges[i];
            freeHEdge(hEdge);
        }

        free(halfEdgeDS->_hEdges);
    }
    halfEdgeDS->_hEdges = NULL;
    halfEdgeDS->_numHEdges = 0;

    if(halfEdgeDS->vertices)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->_numVertices; ++i)
        {
            vertex_t* vertex = halfEdgeDS->vertices[i];
            freeVertex(vertex);
        }

        free(halfEdgeDS->vertices);
    }
    halfEdgeDS->vertices = NULL;
    halfEdgeDS->_numVertices = 0;
}

vertex_t* HalfEdgeDS_CreateVertex(halfedgeds_t* halfEdgeDS)
{
    assert(halfEdgeDS);
    {
    vertex_t* vtx = allocVertex();
    halfEdgeDS->vertices = realloc(halfEdgeDS->vertices,
        sizeof(vtx) * ++halfEdgeDS->_numVertices);
    halfEdgeDS->vertices[halfEdgeDS->_numVertices-1] = vtx;

    vtx->data = Z_Calloc(sizeof(mvertex_t), PU_STATIC, 0);
    ((mvertex_t*) vtx->data)->index = halfEdgeDS->_numVertices; // 1-based index, 0 = NIL.

    return vtx;
    }
}

hedge_t* HalfEdgeDS_CreateHEdge(halfedgeds_t* halfEdgeDS, vertex_t* vertex)
{
    assert(halfEdgeDS);
    assert(vertex);
    {
    hedge_t* hEdge = allocHEdge();
    halfEdgeDS->_hEdges = realloc(halfEdgeDS->_hEdges,
        sizeof(hedge_t*) * ++halfEdgeDS->_numHEdges);
    halfEdgeDS->_hEdges[halfEdgeDS->_numHEdges - 1] = hEdge;

    hEdge->vertex = vertex;
    if(!vertex->hEdge)
        vertex->hEdge = hEdge;
    hEdge->twin = NULL;
    hEdge->next = hEdge->prev = hEdge;
    hEdge->face = NULL;

    return hEdge;
    }
}

face_t* HalfEdgeDS_CreateFace(halfedgeds_t* halfEdgeDS)
{
    assert(halfEdgeDS);
    {
    face_t* face = allocFace();
    halfEdgeDS->_faces = realloc(halfEdgeDS->_faces,
        sizeof(face_t*) * ++halfEdgeDS->_numFaces);
    halfEdgeDS->_faces[halfEdgeDS->_numFaces - 1] = face;

    return face;
    }
}

uint HalfEdgeDS_NumVertices(halfedgeds_t* halfEdgeDS)
{
    assert(halfEdgeDS);
    return halfEdgeDS->_numVertices;
}

uint HalfEdgeDS_NumHEdges(halfedgeds_t* halfEdgeDS)
{
    assert(halfEdgeDS);
    return halfEdgeDS->_numHEdges;
}

uint HalfEdgeDS_NumFaces(halfedgeds_t* halfEdgeDS)
{
    assert(halfEdgeDS);
    return halfEdgeDS->_numFaces;
}

int HalfEdgeDS_IterateVertices(halfedgeds_t* halfEdgeDS,
                               int (*callback) (vertex_t*, void*),
                               void* context)
{
    assert(halfEdgeDS);
    {
    int result = 1;
    uint i = 0;
    while(i < halfEdgeDS->_numVertices &&
          (result = callback(halfEdgeDS->vertices[i++], context)) != 0);
    return result;
    }
}

int HalfEdgeDS_IterateHEdges(halfedgeds_t* halfEdgeDS,
                             int (*callback) (hedge_t*, void*),
                             void* context)
{
    assert(halfEdgeDS);
    {
    int result = 1;
    uint i = 0;
    while(i < halfEdgeDS->_numHEdges &&
          (result = callback(halfEdgeDS->_hEdges[i++], context)) != 0);
    return result;
    }
}

int HalfEdgeDS_IterateFaces(halfedgeds_t* halfEdgeDS,
                            int (*callback) (face_t*, void*),
                            void* context)
{
    assert(halfEdgeDS);
    {
    int result = 1;
    uint i = 0;
    while(i < halfEdgeDS->_numFaces &&
          (result = callback(halfEdgeDS->_faces[i++], context)) != 0);
    return result;
    }
}

#if _DEBUG
void testVertexHEdgeRings(vertex_t* v)
{
    byte i = 0;

    // Two passes. Pass one = counter clockwise, Pass two = clockwise.
    for(i = 0; i < 2; ++i)
    {
        hedge_t* hEdge, *base;

        hEdge = base = v->hEdge;
        do
        {
            if(hEdge->vertex != v)
                Con_Error("testVertexHEdgeRings: break on hEdge->vertex.");

            {
            hedge_t* other, *base2;
            boolean found = false;

            other = base2 = hEdge->vertex->hEdge;
            do
            {
                 if(other == hEdge)
                     found = true;
            } while((other = i ? other->prev->twin : other->twin->next) != base2);

            if(!found)
                Con_Error("testVertexHEdgeRings: break on vertex->hEdge.");
            }
        } while((hEdge = i ? hEdge->prev->twin : hEdge->twin->next) != base);
    }
}
#endif

/**
 * Splits the given half-edge at the point (x,y). The new half-edge is
 * returned. The old half-edge is shortened (the original start vertex is
 * unchanged), the new half-edge becomes the cut-off tail (keeping the
 * original end vertex).
 */
hedge_t* HEdge_Split(halfedgeds_t* halfEdgeDS, hedge_t* oldHEdge)
{
    hedge_t* newHEdge;
    vertex_t* newVert;

#if _DEBUG
testVertexHEdgeRings(oldHEdge->vertex);
testVertexHEdgeRings(oldHEdge->twin->vertex);
#endif

    newVert = HalfEdgeDS_CreateVertex(halfEdgeDS);

    newHEdge = HalfEdgeDS_CreateHEdge(halfEdgeDS, newVert);
    newHEdge->twin = HalfEdgeDS_CreateHEdge(halfEdgeDS, oldHEdge->twin->vertex);
    newHEdge->twin->twin = newHEdge;

    // Update right neighbour back links of oldHEdge and its twin.
    newHEdge->next = oldHEdge->next;
    oldHEdge->next->prev = newHEdge;

    newHEdge->twin->prev = oldHEdge->twin->prev;
    newHEdge->twin->prev->next = newHEdge->twin;

    // Update the vertex links.
    newHEdge->vertex = newVert;
    newVert->hEdge = newHEdge;

    newHEdge->twin->vertex = oldHEdge->twin->vertex;
    oldHEdge->twin->vertex = newVert;

    if(newHEdge->twin->vertex->hEdge == oldHEdge->twin)
        newHEdge->twin->vertex->hEdge = newHEdge->twin;

    // Link oldHEdge with newHEdge and their twins.
    oldHEdge->next = newHEdge;
    newHEdge->prev = oldHEdge;

    oldHEdge->twin->prev = newHEdge->twin;
    newHEdge->twin->next = oldHEdge->twin;

    // Copy face data from oldHEdge to newHEdge and their twins.
    //if(oldHEdge->face)
    //    Face_LinkHEdge(oldHEdge->face, newHEdge);
    if(oldHEdge->twin->face)
        Face_LinkHEdge(oldHEdge->twin->face, newHEdge->twin);

#if _DEBUG
testVertexHEdgeRings(oldHEdge->vertex);
testVertexHEdgeRings(newHEdge->vertex);
testVertexHEdgeRings(newHEdge->twin->vertex);
#endif

    return newHEdge;
}

static boolean getAveragedCoords(face_t* face, double* x, double* y)
{
    size_t total = 0;
    double avg[2];
    const hedge_t* firstHEdge, *hEdge;

    if(!x || !y)
        return false;

    avg[VX] = avg[VY] = 0;

    hEdge = firstHEdge = face->hEdge;
    do
    {
        avg[VX] += hEdge->vertex->pos[VX];
        avg[VY] += hEdge->vertex->pos[VY];

        avg[VX] += hEdge->twin->vertex->pos[VX];
        avg[VY] += hEdge->twin->vertex->pos[VY];

        total += 2;
    } while((hEdge = hEdge->next) != firstHEdge);

    if(total > 0)
    {
        *x = avg[VX] / total;
        *y = avg[VY] / total;
        return true;
    }

    return false;
}

/**
 * Sort half-edges by angle (from the middle point to the start vertex).
 */
static void sortHEdgesByAngleAroundMidPoint(face_t* face)
{
    hedge_t* hEdge;
    double midPoint[2];

    if(!face->hEdge || face->hEdge->next == face->hEdge)
        return;

    getAveragedCoords(face, &midPoint[0], &midPoint[1]);

    hEdge = face->hEdge;
    for(;;)
    {
        const hedge_t* hEdgeA = hEdge;
        const hedge_t* hEdgeB = hEdge->next;
        double angle1, angle2;

        if(hEdge->next == face->hEdge)
            break; // Sorted.

        angle1 = SlopeToAngle(hEdgeA->vertex->pos[0] - midPoint[0],
                              hEdgeA->vertex->pos[1] - midPoint[1]);
        angle2 = SlopeToAngle(hEdgeB->vertex->pos[0] - midPoint[0],
                              hEdgeB->vertex->pos[1] - midPoint[1]);

        if(angle1 + ANG_EPSILON < angle2)
        {   // Swap them.
            hedge_t* other = hEdge->next;

            if(hEdge == face->hEdge)
                face->hEdge = hEdge->next;

            hEdge->prev->next = hEdge->next;
            hEdge->next->prev = hEdge->prev;

            hEdge->next->next->prev = hEdge;
            hEdge->next = hEdge->next->next;

            hEdge->prev = other;
            other->next = hEdge;

            hEdge = face->hEdge;
        }
        else
        {
            hEdge = hEdge->next;
        }
    }
}

/**
 * Sort the list of half-edges in the leaf into clockwise order, based on
 * their position/orientation around the mid point in the leaf.
 */
void Face_ClockwiseOrder(face_t* face)
{
    assert(face);
    {
    sortHEdgesByAngleAroundMidPoint(face);

/*#if _DEBUG
{
const hedge_t* hEdge;

Con_Message("Sorted half-edges around (%1.1f,%1.1f)\n", x, y);

hEdge = face->hEdge;
do
{
    const hedge_t* hEdge = hEdge->hEdge;
    double angle = SlopeToAngle(hEdge->vertex->pos[0] - midPoint[0],
                                hEdge->vertex->pos[1] - midPoint[1]);

    Con_Message("  half-edge %p: Angle %1.6f  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                hEdge, angle, (float) hEdge->vertex->pos[0], (float) hEdge->vertex->pos[1],
                (float) hEdge->twin->vertex->pos[0], (float) hEdge->twin->vertex->pos[1]);
} while((hEdge = hEdge->next) != face->hEdge);
}
#endif*/
    }
}

void Face_SwitchToHEdgeLinks(face_t* face)
{
    assert(face);
    {
    hedge_t* firstHEdge;
    hedge_node_t* node, *next;

    // Copy order from face.
    node = (hedge_node_t*) face->hEdge;
    do
    {
        hedge_t* hEdge = node->hEdge;

        hEdge->next = node->next->hEdge;
        hEdge->next->prev = hEdge;
    } while((node = node->next) != (hedge_node_t*) face->hEdge);

    // Delete the nodes and switch to the hedge links.
    firstHEdge = ((hedge_node_t*) face->hEdge)->hEdge;
    node = (hedge_node_t*) face->hEdge;
    do
    {
        next = node->next;
        free(node);
    } while((node = next) != (hedge_node_t*) face->hEdge);

    face->hEdge = firstHEdge;
    }
}

void Face_LinkHEdge(face_t* face, hedge_t* hEdge)
{
    hedge_node_t* node;

    assert(face);
    assert(hEdge);

#if _DEBUG
// Ensure hedge is not already linked.
if(face->hEdge)
{
    node = (hedge_node_t*) face->hEdge;
    do
    {
        if(node->hEdge == hEdge)
            Con_Error("Face_LinkHEdge: HEdge %p already linked to "
                      "face %p!", hEdge, face);
    } while((node = node->next) != (hedge_node_t*) face->hEdge);
}
#endif

    node = allocHEdgeNode();
    node->hEdge = hEdge;

    if(face->hEdge)
    {
        node->prev = ((hedge_node_t*) face->hEdge)->prev;
        node->next = ((hedge_node_t*) face->hEdge);

        node->prev->next = node;
        node->next->prev = node;

        face->hEdge = (hedge_t*) node;
    }
    else
    {
        node->next = node->prev = node;
        face->hEdge = (hedge_t*) node;
    }

    hEdge->face = face;
}

void Face_UnlinkHEdge(face_t* face, hedge_t* hEdge)
{
    hedge_node_t* node;

    assert(hEdge);
    assert(face);

    if(!face->hEdge)
        return;

    if(((hedge_node_t*) face->hEdge) == ((hedge_node_t*)face->hEdge)->next)
    {
        if(((hedge_node_t*) face->hEdge)->hEdge == hEdge)
        {
            freeHEdgeNode((hedge_node_t*) face->hEdge);
            face->hEdge = NULL;

            hEdge->face = NULL;
        }

        return;
    }

    node = (hedge_node_t*) face->hEdge;
    do
    {
        if(node->next && node->next->hEdge == hEdge)
        {
            hedge_node_t* p = node->next;

            node->next = node->next->next;
            node->next->prev = node;

            if((hedge_node_t*) face->hEdge == p)
                face->hEdge = (hedge_t*) p->next;

            freeHEdgeNode(p);

            hEdge->face = NULL;
            break;
        }
    } while((node = node->next) != (hedge_node_t*) face->hEdge);
}
