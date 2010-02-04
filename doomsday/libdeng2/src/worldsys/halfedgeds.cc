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
#include "de/Vertex" // temp

using namespace de;

namespace de
{
    typedef struct hedge_node_s {
        HalfEdge* hEdge;
        struct hedge_node_s* next, *prev;
    } hedge_node_t;
}

static __inline Vertex* allocVertex(void)
{
    return reinterpret_cast<Vertex*>(std::calloc(sizeof(Vertex), 1));
}

static __inline void freeVertex(Vertex* vertex)
{
    free(vertex);
}

static __inline HalfEdge* allocHEdge(void)
{
    return reinterpret_cast<HalfEdge*>(std::calloc(sizeof(HalfEdge), 1));
}

static __inline void freeHEdge(HalfEdge* hEdge)
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

static __inline Face* allocFace(void)
{
    return reinterpret_cast<Face*>(std::calloc(sizeof(Face), 1));
}

static __inline void freeFace(Face* face)
{
    std::free(face);
}

HalfEdgeDS::HalfEdgeDS()
  :  _numHEdges(0),
     _hEdges(0),
     _numFaces(0),
     _faces(0)
{}

HalfEdgeDS::~HalfEdgeDS()
{
    if(_faces)
    {
        for(duint i = 0; i < _numFaces; ++i)
        {
            Face* face = _faces[i];
            freeFace(face);
        }
        std::free(_faces);
    }
    _faces = NULL;
    _numFaces = 0;

    if(_hEdges)
    {
        for(duint i = 0; i < _numHEdges; ++i)
        {
            HalfEdge* hEdge = _hEdges[i];
            freeHEdge(hEdge);
        }
        std::free(_hEdges);
    }
    _hEdges = NULL;
    _numHEdges = 0;

    if(vertices)
    {
        for(duint i = 0; i < _numVertices; ++i)
        {
            Vertex* vertex = vertices[i];
            freeVertex(vertex);
        }
        std::free(vertices);
    }
    vertices = NULL;
    _numVertices = 0;
}

Vertex* HalfEdgeDS::createVertex()
{
    Vertex* vtx = allocVertex();
    vertices = reinterpret_cast<Vertex**>(std::realloc(vertices, sizeof(vtx) * ++_numVertices));
    vertices[_numVertices-1] = vtx;

    vtx->data = reinterpret_cast<mvertex_t*>(Z_Calloc(sizeof(mvertex_t), PU_STATIC, 0));
    ((mvertex_t*) vtx->data)->index = _numVertices; // 1-based index, 0 = NIL.
    return vtx;
}

HalfEdge* HalfEdgeDS::createHEdge(Vertex* vertex)
{
    assert(vertex);

    HalfEdge* hEdge = allocHEdge();
    _hEdges = reinterpret_cast<HalfEdge**>(std::realloc(_hEdges, sizeof(HalfEdge*) * ++_numHEdges));
    _hEdges[_numHEdges - 1] = hEdge;

    hEdge->vertex = vertex;
    if(!vertex->hEdge)
        vertex->hEdge = hEdge;
    hEdge->twin = NULL;
    hEdge->next = hEdge->prev = hEdge;
    hEdge->face = NULL;

    return hEdge;
}

Face* HalfEdgeDS::createFace()
{
    Face* face = allocFace();
    _faces = reinterpret_cast<Face**>(std::realloc(_faces, sizeof(Face*) * ++_numFaces));
    _faces[_numFaces - 1] = face;
    return face;
}

duint HalfEdgeDS::numVertices() const
{
    return _numVertices;
}

duint HalfEdgeDS::numHEdges() const
{
    return _numHEdges;
}

duint HalfEdgeDS::numFaces() const
{
    return _numFaces;
}

dint HalfEdgeDS::iterateVertices(dint (*callback) (Vertex*, void*), void* context)
{
    assert(callback);

    dint result = 1;
    duint i = 0;
    while(i < _numVertices && (result = callback(vertices[i++], context)) != 0);
    return result;
}

dint HalfEdgeDS::iterateHEdges(dint (*callback) (HalfEdge*, void*), void* context)
{
    assert(callback);

    dint result = 1;
    duint i = 0;
    while(i < _numHEdges && (result = callback(_hEdges[i++], context)) != 0);
    return result;
}

dint HalfEdgeDS::iterateFaces(dint (*callback) (Face*, void*), void* context)
{
    assert(callback);

    dint result = 1;
    duint i = 0;
    while(i < _numFaces && (result = callback(_faces[i++], context)) != 0);
    return result;
}

#if _DEBUG
void de::testVertexHEdgeRings(Vertex* v)
{
    dbyte i = 0;

    // Two passes. Pass one = counter clockwise, Pass two = clockwise.
    for(i = 0; i < 2; ++i)
    {
        HalfEdge* hEdge, *base;

        hEdge = base = v->hEdge;
        do
        {
            assert(hEdge->vertex == v);

            HalfEdge* other, *base2;
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
HalfEdge* de::HEdge_Split(HalfEdgeDS& halfEdgeDS, HalfEdge* oldHEdge)
{
    HalfEdge* newHEdge;
    Vertex* newVert;

#if _DEBUG
testVertexHEdgeRings(oldHEdge->vertex);
testVertexHEdgeRings(oldHEdge->twin->vertex);
#endif

    newVert = halfEdgeDS.createVertex();

    newHEdge = halfEdgeDS.createHEdge(newVert);
    newHEdge->twin = halfEdgeDS.createHEdge(oldHEdge->twin->vertex);
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
        oldHEdge->twin->face->linkHEdge(newHEdge->twin);

#if _DEBUG
testVertexHEdgeRings(oldHEdge->vertex);
testVertexHEdgeRings(newHEdge->vertex);
testVertexHEdgeRings(newHEdge->twin->vertex);
#endif

    return newHEdge;
}

bool Face::getAveragedCoords(ddouble* x, ddouble* y)
{
    dsize total = 0;
    ddouble avg[2];
    const hedge_node_t* node;

    if(!x || !y)
        return false;

    avg[VX] = avg[VY] = 0;

    node = (hedge_node_t*) hEdge;
    do
    {
        HalfEdge* hEdge = node->hEdge;
        avg[VX] += hEdge->vertex->pos[VX];
        avg[VY] += hEdge->vertex->pos[VY];

        avg[VX] += hEdge->twin->vertex->pos[VX];
        avg[VY] += hEdge->twin->vertex->pos[VY];

        total += 2;
    } while((node = node->next) != (hedge_node_t*) hEdge);

    if(total > 0)
    {
        *x = avg[VX] / total;
        *y = avg[VY] / total;
        return true;
    }

    return false;
}

void Face::sortHEdgesByAngleAroundMidPoint()
{
    hedge_node_t* node;
    ddouble midPoint[2];

    if(!hEdge ||
       ((hedge_node_t*) hEdge)->next == (hedge_node_t*) hEdge)
        return;

    getAveragedCoords(&midPoint[0], &midPoint[1]);

    node = (hedge_node_t*) hEdge;
    for(;;)
    {
        const HalfEdge* hEdgeA = node->hEdge;
        const HalfEdge* hEdgeB = node->next->hEdge;
        ddouble angle1, angle2;

        if(node->next == (hedge_node_t*) hEdge)
            break; // Sorted.

        angle1 = slopeToAngle(hEdgeA->vertex->pos[0] - midPoint[0],
                              hEdgeA->vertex->pos[1] - midPoint[1]);
        angle2 = slopeToAngle(hEdgeB->vertex->pos[0] - midPoint[0],
                              hEdgeB->vertex->pos[1] - midPoint[1]);

        if(angle1 + ANG_EPSILON < angle2)
        {   // Swap them.
            hedge_node_t* other = node->next;

            if(hEdgeA == ((hedge_node_t*) hEdge)->hEdge)
                hEdge = reinterpret_cast<HalfEdge*>(other);

            node->prev->next = node->next;
            node->next->prev = node->prev;

            node->next->next->prev = node;
            node->next = node->next->next;

            node->prev = other;
            other->next = node;

            node = (hedge_node_t*) hEdge;
        }
        else
        {
            node = node->next;
        }
    }

/*#if _DEBUG
{
const hedge_node_t* node;

LOG_MESSAGE("Sorted half-edges around (%1.1f,%1.1f).") << x << y;

node = (hedge_node_t*) hEdge;
do
{
    const HalfEdge* other = node->hEdge;
    ddouble angle = slopeToAngle(other->vertex->pos[0] - midPoint[0],
                                other->vertex->pos[1] - midPoint[1]);

    LOG_MESSAGE("  half-edge %p: Angle %1.6f  (%1.1f,%1.1f) -> (%1.1f,%1.1f).")
        << other << angle << dfloat(other->vertex->pos[0])
        << dfloat(other->vertex->pos[1]) << dfloat(other->twin->vertex->pos[0])
        << dfloat(other->twin->vertex->pos[1]);
} while((node = node->next) != hEdge);
}
#endif*/
}

void Face::switchToHEdgeLinks()
{
    HalfEdge* firstHEdge;
    hedge_node_t* node, *next;

    sortHEdgesByAngleAroundMidPoint();

    // Copy order from face.
    node = (hedge_node_t*) hEdge;
    do
    {
        HalfEdge* other = node->hEdge;

        other->next = node->next->hEdge;
        other->next->prev = other;
    } while((node = node->next) != (hedge_node_t*) hEdge);

    // Delete the nodes and switch to the hedge links.
    firstHEdge = ((hedge_node_t*) hEdge)->hEdge;
    node = (hedge_node_t*) hEdge;
    do
    {
        next = node->next;
        free(node);
    } while((node = next) != (hedge_node_t*) hEdge);

    hEdge = firstHEdge;
}

void Face::linkHEdge(HalfEdge* _hEdge)
{
    hedge_node_t* node;

    assert(_hEdge);

#if _DEBUG
// Ensure hedge is not already linked.
if(hEdge)
{
    node = (hedge_node_t*) hEdge;
    do
    {
        assert(node->hEdge != _hEdge);
    } while((node = node->next) != (hedge_node_t*) hEdge);
}
#endif

    node = allocHEdgeNode();
    node->hEdge = _hEdge;

    if(hEdge)
    {
        node->prev = ((hedge_node_t*) hEdge)->prev;
        node->next = ((hedge_node_t*) hEdge);

        node->prev->next = node;
        node->next->prev = node;

        hEdge = (HalfEdge*) node;
    }
    else
    {
        node->next = node->prev = node;
        hEdge = (HalfEdge*) node;
    }

    _hEdge->face = this;
}

void Face::unlinkHEdge(HalfEdge* _hEdge)
{
    hedge_node_t* node;

    assert(_hEdge);

    if(!hEdge)
        return;

    if(((hedge_node_t*) hEdge) == ((hedge_node_t*)hEdge)->next)
    {
        if(((hedge_node_t*) hEdge)->hEdge == _hEdge)
        {
            freeHEdgeNode((hedge_node_t*) hEdge);
            hEdge = NULL;
            _hEdge->face = NULL;
        }
        return;
    }

    node = (hedge_node_t*) hEdge;
    do
    {
        if(node->next && node->next->hEdge == _hEdge)
        {
            hedge_node_t* p = node->next;

            node->next = node->next->next;
            node->next->prev = node;

            if((hedge_node_t*) hEdge == p)
                hEdge = (HalfEdge*) p->next;

            freeHEdgeNode(p);

            _hEdge->face = NULL;
            break;
        }
    } while((node = node->next) != (hedge_node_t*) hEdge);
}
