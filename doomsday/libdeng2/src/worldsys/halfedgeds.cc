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

static __inline hedge_node_t* allocHEdgeNode(void)
{
    return reinterpret_cast<hedge_node_t*>(std::malloc(sizeof(hedge_node_t)));
}

static __inline void freeHEdgeNode(hedge_node_t* node)
{
    std::free(node);
}

HalfEdgeDS::~HalfEdgeDS()
{
    FOR_EACH(i, _faces, Faces::iterator)
        delete *i;
    _faces.clear();

    FOR_EACH(i, _halfEdges, HalfEdges::iterator)
        delete *i;
    _halfEdges.clear();

    FOR_EACH(i, vertices, Vertices::iterator)
    {
        Vertex* vertex = *i;
        if(vertex->data) std::free(vertex->data);
        delete vertex;
    }
    vertices.clear();
}

Vertex& HalfEdgeDS::createVertex()
{
    Vertex* vtx = new Vertex();
    vtx->data = reinterpret_cast<MVertex*>(std::calloc(1, sizeof(MVertex)));
    ((MVertex*) vtx->data)->index = vertices.size() + 1; // 1-based index, 0 = NIL.
    vertices.push_back(vtx);
    return *vtx;
}

HalfEdge& HalfEdgeDS::createHalfEdge(Vertex& vertex)
{
    HalfEdge* hEdge = new HalfEdge();
    hEdge->vertex = &vertex;
    if(!vertex.hEdge)
        vertex.hEdge = hEdge;
    hEdge->twin = NULL;
    hEdge->next = hEdge->prev = hEdge;
    hEdge->face = NULL;

    _halfEdges.push_back(hEdge);
    return *hEdge;
}

Face& HalfEdgeDS::createFace()
{
    Face* face = new Face();
    _faces.push_back(face);
    return *face;
}

bool HalfEdgeDS::iterateVertices(bool (*callback) (Vertex*, void*), void* paramaters)
{
    assert(callback);

    FOR_EACH(i, vertices, Vertices::iterator)
    {
        Vertex* vertex = *i;
        if(vertex && !callback(vertex, paramaters))
            return false;
    }
    return true;
}

bool HalfEdgeDS::iterateHalfEdges(bool (*callback) (HalfEdge*, void*), void* paramaters)
{
    assert(callback);

    FOR_EACH(i, _halfEdges, HalfEdges::iterator)
    {
        HalfEdge* halfEdge = *i;
        if(halfEdge && !callback(halfEdge, paramaters))
            return false;
    }
    return true;
}

bool HalfEdgeDS::iterateFaces(bool (*callback) (Face*, void*), void* paramaters)
{
    assert(callback);

    FOR_EACH(i, _faces, Faces::iterator)
    {
        Face* face = *i;
        if(face && !callback(face, paramaters))
            return false;
    }
    return true;
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

HalfEdge& de::HEdge_Split(HalfEdgeDS& halfEdgeDS, HalfEdge& oldHEdge)
{
#if _DEBUG
testVertexHEdgeRings(oldHEdge.vertex);
testVertexHEdgeRings(oldHEdge.twin->vertex);
#endif

    Vertex& newVert = halfEdgeDS.createVertex();

    HalfEdge& newHEdge = halfEdgeDS.createHalfEdge(newVert);
    newHEdge.twin = &halfEdgeDS.createHalfEdge(*oldHEdge.twin->vertex);
    newHEdge.twin->twin = &newHEdge;

    // Update right neighbour back links of oldHEdge and its twin.
    newHEdge.next = oldHEdge.next;
    oldHEdge.next->prev = &newHEdge;

    newHEdge.twin->prev = oldHEdge.twin->prev;
    newHEdge.twin->prev->next = newHEdge.twin;

    // Update the vertex links.
    newHEdge.vertex = &newVert;
    newVert.hEdge = &newHEdge;

    newHEdge.twin->vertex = oldHEdge.twin->vertex;
    oldHEdge.twin->vertex = &newVert;

    if(newHEdge.twin->vertex->hEdge == oldHEdge.twin)
        newHEdge.twin->vertex->hEdge = newHEdge.twin;

    // Link oldHEdge with newHEdge and their twins.
    oldHEdge.next = &newHEdge;
    newHEdge.prev = &oldHEdge;

    oldHEdge.twin->prev = newHEdge.twin;
    newHEdge.twin->next = oldHEdge.twin;

    // Copy face data from oldHEdge to newHEdge and their twins.
    //if(oldHEdge.face)
    //    Face_LinkHEdge(oldHEdge.face, newHEdge);
    if(oldHEdge.twin->face)
        oldHEdge.twin->face->linkHEdge(newHEdge.twin);

#if _DEBUG
testVertexHEdgeRings(oldHEdge.vertex);
testVertexHEdgeRings(newHEdge.vertex);
testVertexHEdgeRings(newHEdge.twin->vertex);
#endif

    return newHEdge;
}

Vector2d getAveragedCoords(const Face& face)
{
    Vector2d avg = Vector2d(0, 0);
    const hedge_node_t* node = (hedge_node_t*) face.hEdge;
    dsize total = 0;
    do
    {
        const HalfEdge* hEdge = node->hEdge;
        avg += hEdge->vertex->pos;
        ++total;
    } while((node = node->next) != (hedge_node_t*) face.hEdge);

    if(total != 0)
    {
        avg.x /= total;
        avg.y /= total;
    }

    return Vector2d(avg);
}

void Face::sortHEdgesByAngleAroundMidPoint()
{
    hedge_node_t* node;

    if(!hEdge ||
       ((hedge_node_t*) hEdge)->next == (hedge_node_t*) hEdge)
        return;

    Vector2d midPoint = getAveragedCoords(*this);

    node = (hedge_node_t*) hEdge;
    for(;;)
    {
        if(node->next == (hedge_node_t*) hEdge)
            break; // Sorted.

        const HalfEdge* hEdgeA = node->hEdge;
        const HalfEdge* hEdgeB = node->next->hEdge;

        const Vector2d deltaA = hEdgeA->vertex->pos - midPoint;
        const ddouble angle1 = slopeToAngle(deltaA.x, deltaA.y);

        const Vector2d deltaB = hEdgeB->vertex->pos - midPoint;
        const ddouble angle2 = slopeToAngle(deltaB.x, deltaB.y);

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
LOG_MESSAGE("Sorted half-edges around %s.") << midPoint;

const hedge_node_t* node = (hedge_node_t*) hEdge;
do
{
    const HalfEdge* other = node->hEdge;
    const Vector2d delta = other->vertex->pos - midPoint;
    ddouble angle = slopeToAngle(delta.x, delta.y);

    LOG_MESSAGE("  half-edge %p: Angle %1.6f %s -> s.")
        << other << angle << other->vertex->pos << other->twin->vertex->pos;
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
