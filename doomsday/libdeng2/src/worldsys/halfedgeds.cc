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

#include <math.h>
#include <cstdlib>

#include "de/HalfEdgeDS"

using namespace de;

#define PI_D            3.14159265358979323846
// Smallest degrees between two angles before being considered equal.
#define ANG_EPSILON     (1.0 / 1024.0)

typedef struct hedge_node_s {
    hedge_t*        hEdge;
    struct hedge_node_s* next, *prev;
} hedge_node_t;

static __inline vertex_t* allocVertex(void)
{
    return reinterpret_cast<vertex_t*>(std::calloc(sizeof(vertex_t), 1));
}

static __inline void freeVertex(vertex_t* vertex)
{
    free(vertex);
}

static __inline hedge_t* allocHEdge(void)
{
    return reinterpret_cast<hedge_t*>(std::calloc(sizeof(hedge_t), 1));
}

static __inline void freeHEdge(hedge_t* hEdge)
{
    std::free(hEdge);
}

static __inline hedge_node_t* allocHEdgeNode(void)
{
    return reinterpret_cast<hedge_node_t*>(std::malloc(sizeof(hedge_node_t)));
}

static __inline void freeHEdgeNode(hedge_node_t* node)
{
    std::free(node);
}

static __inline face_t* allocFace(void)
{
    return reinterpret_cast<face_t*>(std::calloc(sizeof(face_t), 1));
}

static __inline void freeFace(face_t* face)
{
    std::free(face);
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
    halfedgeds_t* halfEdgeDS = reinterpret_cast<halfedgeds_t*>(std::calloc(sizeof(*halfEdgeDS), 1));
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
        for(duint i = 0; i < halfEdgeDS->_numFaces; ++i)
        {
            face_t* face = halfEdgeDS->_faces[i];
            freeFace(face);
        }
        std::free(halfEdgeDS->_faces);
    }
    halfEdgeDS->_faces = NULL;
    halfEdgeDS->_numFaces = 0;

    if(halfEdgeDS->_hEdges)
    {
        for(duint i = 0; i < halfEdgeDS->_numHEdges; ++i)
        {
            hedge_t* hEdge = halfEdgeDS->_hEdges[i];
            freeHEdge(hEdge);
        }
        std::free(halfEdgeDS->_hEdges);
    }
    halfEdgeDS->_hEdges = NULL;
    halfEdgeDS->_numHEdges = 0;

    if(halfEdgeDS->vertices)
    {
        for(duint i = 0; i < halfEdgeDS->_numVertices; ++i)
        {
            vertex_t* vertex = halfEdgeDS->vertices[i];
            freeVertex(vertex);
        }
        std::free(halfEdgeDS->vertices);
    }
    halfEdgeDS->vertices = NULL;
    halfEdgeDS->_numVertices = 0;
}

vertex_t* de::HalfEdgeDS_CreateVertex(halfedgeds_t* halfEdgeDS)
{
    assert(halfEdgeDS);

    vertex_t* vtx = allocVertex();
    halfEdgeDS->vertices = reinterpret_cast<vertex_t**>(std::realloc(halfEdgeDS->vertices,
        sizeof(vtx) * ++halfEdgeDS->_numVertices));
    halfEdgeDS->vertices[halfEdgeDS->_numVertices-1] = vtx;

    vtx->data = reinterpret_cast<mvertex_t*>(Z_Calloc(sizeof(mvertex_t), PU_STATIC, 0));
    ((mvertex_t*) vtx->data)->index = halfEdgeDS->_numVertices; // 1-based index, 0 = NIL.

    return vtx;
}

hedge_t* de::HalfEdgeDS_CreateHEdge(halfedgeds_t* halfEdgeDS, vertex_t* vertex)
{
    assert(halfEdgeDS);
    assert(vertex);

    hedge_t* hEdge = allocHEdge();
    halfEdgeDS->_hEdges = reinterpret_cast<hedge_t**>(std::realloc(halfEdgeDS->_hEdges,
        sizeof(hedge_t*) * ++halfEdgeDS->_numHEdges));
    halfEdgeDS->_hEdges[halfEdgeDS->_numHEdges - 1] = hEdge;

    hEdge->vertex = vertex;
    if(!vertex->hEdge)
        vertex->hEdge = hEdge;
    hEdge->twin = NULL;
    hEdge->next = hEdge->prev = hEdge;
    hEdge->face = NULL;

    return hEdge;
}

face_t* HalfEdgeDS_CreateFace(halfedgeds_t* halfEdgeDS)
{
    assert(halfEdgeDS);

    face_t* face = allocFace();
    halfEdgeDS->_faces = reinterpret_cast<face_t**>(std::realloc(halfEdgeDS->_faces,
        sizeof(face_t*) * ++halfEdgeDS->_numFaces));
    halfEdgeDS->_faces[halfEdgeDS->_numFaces - 1] = face;

    return face;
}

duint HalfEdgeDS_NumVertices(halfedgeds_t* halfEdgeDS)
{
    assert(halfEdgeDS);
    return halfEdgeDS->_numVertices;
}

duint HalfEdgeDS_NumHEdges(halfedgeds_t* halfEdgeDS)
{
    assert(halfEdgeDS);
    return halfEdgeDS->_numHEdges;
}

duint HalfEdgeDS_NumFaces(halfedgeds_t* halfEdgeDS)
{
    assert(halfEdgeDS);
    return halfEdgeDS->_numFaces;
}

int HalfEdgeDS_IterateVertices(halfedgeds_t* halfEdgeDS,
                               int (*callback) (vertex_t*, void*),
                               void* context)
{
    assert(halfEdgeDS);

    int result = 1;
    duint i = 0;
    while(i < halfEdgeDS->_numVertices &&
          (result = callback(halfEdgeDS->vertices[i++], context)) != 0);
    return result;
}

int HalfEdgeDS_IterateHEdges(halfedgeds_t* halfEdgeDS,
                             int (*callback) (hedge_t*, void*),
                             void* context)
{
    assert(halfEdgeDS);

    int result = 1;
    duint i = 0;
    while(i < halfEdgeDS->_numHEdges &&
          (result = callback(halfEdgeDS->_hEdges[i++], context)) != 0);
    return result;
}

int HalfEdgeDS_IterateFaces(halfedgeds_t* halfEdgeDS,
                            int (*callback) (face_t*, void*),
                            void* context)
{
    assert(halfEdgeDS);

    int result = 1;
    duint i = 0;
    while(i < halfEdgeDS->_numFaces &&
          (result = callback(halfEdgeDS->_faces[i++], context)) != 0);
    return result;
}

#if _DEBUG
void de::testVertexHEdgeRings(vertex_t* v)
{
    dbyte i = 0;

    // Two passes. Pass one = counter clockwise, Pass two = clockwise.
    for(i = 0; i < 2; ++i)
    {
        hedge_t* hEdge, *base;

        hEdge = base = v->hEdge;
        do
        {
            assert(hEdge->vertex == v);

            hedge_t* other, *base2;
            bool found = false;

            other = base2 = hEdge->vertex->hEdge;
            do
            {
                 if(other == hEdge)
                     found = true;
            } while((other = i ? other->prev->twin : other->twin->next) != base2);

            assert(found);

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

static bool getAveragedCoords(face_t* face, double* x, double* y)
{
    dsize total = 0;
    double avg[2];
    const hedge_node_t* node;

    if(!x || !y)
        return false;

    avg[VX] = avg[VY] = 0;

    node = (hedge_node_t*) face->hEdge;
    do
    {
        hedge_t* hEdge = node->hEdge;
        avg[VX] += hEdge->vertex->pos[VX];
        avg[VY] += hEdge->vertex->pos[VY];

        avg[VX] += hEdge->twin->vertex->pos[VX];
        avg[VY] += hEdge->twin->vertex->pos[VY];

        total += 2;
    } while((node = node->next) != (hedge_node_t*) face->hEdge);

    if(total > 0)
    {
        *x = avg[VX] / total;
        *y = avg[VY] / total;
        return true;
    }

    return false;
}

/**
 * Sort the list of half-edges in the leaf into clockwise order, based on
 * their position/orientation around the mid point in the leaf.
 */
static void sortHEdgesByAngleAroundMidPoint(face_t* face)
{
    hedge_node_t* node;
    double midPoint[2];

    if(!face->hEdge ||
       ((hedge_node_t*) face->hEdge)->next == (hedge_node_t*) face->hEdge)
        return;

    getAveragedCoords(face, &midPoint[0], &midPoint[1]);

    node = (hedge_node_t*) face->hEdge;
    for(;;)
    {
        const hedge_t* hEdgeA = node->hEdge;
        const hedge_t* hEdgeB = node->next->hEdge;
        double angle1, angle2;

        if(node->next == (hedge_node_t*) face->hEdge)
            break; // Sorted.

        angle1 = SlopeToAngle(hEdgeA->vertex->pos[0] - midPoint[0],
                              hEdgeA->vertex->pos[1] - midPoint[1]);
        angle2 = SlopeToAngle(hEdgeB->vertex->pos[0] - midPoint[0],
                              hEdgeB->vertex->pos[1] - midPoint[1]);

        if(angle1 + ANG_EPSILON < angle2)
        {   // Swap them.
            hedge_node_t* other = node->next;

            if(hEdgeA == ((hedge_node_t*) face->hEdge)->hEdge)
                face->hEdge = reinterpret_cast<hedge_t*>(other);

            node->prev->next = node->next;
            node->next->prev = node->prev;

            node->next->next->prev = node;
            node->next = node->next->next;

            node->prev = other;
            other->next = node;

            node = (hedge_node_t*) face->hEdge;
        }
        else
        {
            node = node->next;
        }
    }

/*#if _DEBUG
{
const hedge_node_t* node;

Con_Message("Sorted half-edges around (%1.1f,%1.1f)\n", x, y);

node = (hedge_node_t*) face->hEdge;
do
{
    const hedge_t* hEdge = node->hEdge;
    double angle = SlopeToAngle(hEdge->vertex->pos[0] - midPoint[0],
                                hEdge->vertex->pos[1] - midPoint[1]);

    Con_Message("  half-edge %p: Angle %1.6f  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                hEdge, angle, (float) hEdge->vertex->pos[0], (float) hEdge->vertex->pos[1],
                (float) hEdge->twin->vertex->pos[0], (float) hEdge->twin->vertex->pos[1]);
} while((node = node->next) != face->hEdge);
}
#endif*/
}

void Face_SwitchToHEdgeLinks(face_t* face)
{
    assert(face);

    hedge_t* firstHEdge;
    hedge_node_t* node, *next;

    sortHEdgesByAngleAroundMidPoint(face);

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
        assert(node->hEdge != hEdge);
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
