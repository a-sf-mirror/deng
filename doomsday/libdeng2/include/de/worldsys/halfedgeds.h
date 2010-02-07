/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2009-2010 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DOOMSDAY_HALFEDGEDS_H
#define DOOMSDAY_HALFEDGEDS_H

#include "deng.h"

#include "../Error"
#include "../Vector"

#include <vector>

namespace de
{
    class HalfEdge;
    class Face;

    /**
     * Public vertex representation. Users of this class should use this
     * as their base for vertices.
     */
    class Vertex
    {
    public:
        HalfEdge* hEdge;
        Vector2d pos; // @todo Move out of the base class.
        void* data;
    };

    void testVertexHEdgeRings(Vertex* v);

    /**
     * Public halfedge representation. Users of this class should use this
     * as their base for halfedges.
     */
    class HalfEdge
    {
    public:
        Vertex* vertex;
        HalfEdge* twin;
        HalfEdge* next;
        HalfEdge* prev;
        Face* face;
        void* data;
    };

    /**
     * Public face representation. Users of this class should use this
     * as their base for faces.
     */
    class Face
    {
    public:
        HalfEdge* hEdge; // First half-edge of this face.
        void* data;

        void linkHEdge(HalfEdge* hEdge);
        void unlinkHEdge(HalfEdge* hEdge);
        void switchToHEdgeLinks();

        /**
         * Sort the list of half-edges in the leaf into clockwise order, based on
         * their position/orientation around the mid point in the leaf.
         */
        void sortHEdgesByAngleAroundMidPoint();
    };

    Vector2d getAveragedCoords(const Face& face);

    /// Smallest degrees between two angles before being considered equal.
    static const ddouble ANG_EPSILON = (1.0 / 1024.0);

    /**
     *
     */
    class HalfEdgeDS
    {
    public:
        typedef std::vector<Vertex*> Vertices;
        typedef std::vector<HalfEdge*> HalfEdges;
        typedef std::vector<Face*> Faces;

    public:
        Vertices vertices;

        HalfEdgeDS();

        /**
         * \note Only releases memory for the data structure itself, any objects linked
         * to the component parts of the data structure will remain (therefore this is
         * the responsibility of the owner).
         */
        ~HalfEdgeDS();

        Vertex& createVertex();
        HalfEdge& createHalfEdge(Vertex& vertex);
        Face& createFace();

        Vertices::size_type numVertices() const { return vertices.size(); }
        HalfEdges::size_type numHalfEdges() const { return _halfEdges.size(); }
        Faces::size_type numFaces() const { return _faces.size(); }

        /**
         * Returns all HalfEdges.
         */
        const HalfEdges& halfEdges() const { return _halfEdges; }

        bool iterateVertices(bool (*callback) (Vertex*, void*), void* paramaters = 0);
        bool iterateHalfEdges(bool (*callback) (HalfEdge*, void*), void* paramaters = 0);
        bool iterateFaces(bool (*callback) (Face*, void*), void* paramaters = 0);

    private:
        HalfEdges _halfEdges;
        Faces _faces;
    };

    /**
     * Splits the given half-edge at the point (x,y). The new half-edge is
     * returned. The old half-edge is shortened (the original start vertex is
     * unchanged), the new half-edge becomes the cut-off tail (keeping the
     * original end vertex).
     */
    HalfEdge& HEdge_Split(HalfEdgeDS& halfEdgeDS, HalfEdge& oldHEdge);
}

#endif /* DOOMSDAY_HALFEDGEDS_H */
