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

#include <stdlib.h>

#include "de_base.h"
#include "de_play.h"

#include "halfedgeds.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct hedge_node_s {
    hedge_t*    hEdge;
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
    return Z_Calloc(sizeof(vertex_t), PU_STATIC, 0);
}

static __inline void freeVertex(vertex_t* vertex)
{
    Z_Free(vertex);
}

static __inline hedge_t* allocHEdge(void)
{
    return Z_Calloc(sizeof(hedge_t), PU_STATIC, 0);
}

static __inline void freeHEdge(hedge_t* hEdge)
{
    Z_Free(hEdge);
}

static __inline face_t* allocFace(void)
{
    return Z_Calloc(sizeof(face_t), PU_STATIC, 0);
}

static __inline void freeFace(face_t* face)
{
    Z_Free(face);
}

halfedgeds_t* P_CreateHalfEdgeDS(void)
{
    halfedgeds_t* halfEdgeDS = Z_Calloc(sizeof(*halfEdgeDS), PU_STATIC, 0);
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

        Z_Free(halfEdgeDS->_faces);
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

        Z_Free(halfEdgeDS->_hEdges);
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

        Z_Free(halfEdgeDS->vertices);
    }
    halfEdgeDS->vertices = NULL;
    halfEdgeDS->_numVertices = 0;
}

vertex_t* HalfEdgeDS_CreateVertex(halfedgeds_t* halfEdgeDS)
{
    vertex_t* vtx;

    assert(halfEdgeDS);

    vtx = allocVertex();
    halfEdgeDS->vertices = Z_Realloc(halfEdgeDS->vertices,
        sizeof(vtx) * ++halfEdgeDS->_numVertices, PU_STATIC);
    halfEdgeDS->vertices[halfEdgeDS->_numVertices-1] = vtx;

    vtx->data = Z_Calloc(sizeof(mvertex_t), PU_STATIC, 0);
    ((mvertex_t*) vtx->data)->index = halfEdgeDS->_numVertices; // 1-based index, 0 = NIL.

    return vtx;
}

hedge_t* HalfEdgeDS_CreateHEdge(halfedgeds_t* halfEdgeDS)
{
    hedge_t* hEdge;

    assert(halfEdgeDS);

    hEdge = allocHEdge();
    halfEdgeDS->_hEdges = Z_Realloc(halfEdgeDS->_hEdges,
        sizeof(hedge_t*) * ++halfEdgeDS->_numHEdges, PU_STATIC);
    halfEdgeDS->_hEdges[halfEdgeDS->_numHEdges - 1] = hEdge;

    return hEdge;
}

face_t* HalfEdgeDS_CreateFace(halfedgeds_t* halfEdgeDS)
{
    face_t* face;

    assert(halfEdgeDS);

    face = allocFace();
    halfEdgeDS->_faces = Z_Realloc(halfEdgeDS->_faces,
        sizeof(face_t*) * ++halfEdgeDS->_numFaces, PU_STATIC);
    halfEdgeDS->_faces[halfEdgeDS->_numFaces - 1] = face;

    return face;
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

static hedge_node_t* allocHEdgeNode(void)
{
    return malloc(sizeof(hedge_node_t));
}

static void freeHEdgeNode(hedge_node_t* node)
{
    free(node);
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
        hedge_node_t* tmp = ((hedge_node_t*) face->hEdge)->prev;

        node->next = (hedge_node_t*) face->hEdge;
        node->next->prev = node;

        tmp->next = node;
        node->prev = tmp;

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
