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

#include "deng.h"

namespace de
{
    enum {
        VX = 0,
        VY,
        VZ
    };

    typedef struct vertex_s {
        class HalfEdge* hEdge;
        ddouble pos[2]; // @todo Move out of the base class.
        void* data;
    } Vertex;

    void testVertexHEdgeRings(Vertex* v);

    class HalfEdge
    {
    public:
        Vertex* vertex;
        HalfEdge* twin;
        HalfEdge* next;
        HalfEdge* prev;
        class Face* face;
        void* data;
    };

    class Face
    {
    public:
        HalfEdge* hEdge; // First half-edge of this face.
        void* data;

        void linkHEdge(HalfEdge* hEdge);
        void unlinkHEdge(HalfEdge* hEdge);
        void switchToHEdgeLinks();
        bool getAveragedCoords(ddouble* x, ddouble* y);

        /**
         * Sort the list of half-edges in the leaf into clockwise order, based on
         * their position/orientation around the mid point in the leaf.
         */
        void sortHEdgesByAngleAroundMidPoint();
    };

    /// Smallest degrees between two angles before being considered equal.
    static const ddouble ANG_EPSILON = (1.0 / 1024.0);

    class HalfEdgeDS
    {
    public:
        duint _numVertices;
        Vertex** vertices;

        HalfEdgeDS();

        /**
         * \note Only releases memory for the data structure itself, any objects linked
         * to the component parts of the data structure will remain (therefore this is
         * the responsibility of the owner).
         */
        ~HalfEdgeDS();

        Vertex* createVertex();
        HalfEdge* createHEdge(Vertex* vertex);
        Face* createFace();

        duint numVertices() const;
        duint numHEdges() const;
        duint numFaces() const;
        dint iterateVertices(dint (*callback) (Vertex*, void*), void* context);
        dint iterateHEdges(dint (*callback) (HalfEdge*, void*), void* context);
        dint iterateFaces(dint (*callback) (Face*, void*), void* context);

    private:
        duint _numHEdges;
        HalfEdge** _hEdges;

        duint _numFaces;
        Face** _faces;
    };

    HalfEdge* HEdge_Split(HalfEdgeDS& halfEdgeDS, HalfEdge* oldHEdge);
}

#endif /* DOOMSDAY_HALFEDGEDS_H */
