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

#include "de_base.h"
#include "de_play.h"

#include "halfedgeds.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

vertex_t* HalfEdgeDS_CreateVertex(halfedgeds_t* halfEdgeDS)
{
    vertex_t* vtx = Z_Calloc(sizeof(*vtx), PU_STATIC, 0);

    halfEdgeDS->vertices = Z_Realloc(halfEdgeDS->vertices,
        sizeof(vtx) * (++halfEdgeDS->numVertices + 1), PU_STATIC);
    halfEdgeDS->vertices[halfEdgeDS->numVertices-1] = vtx;
    halfEdgeDS->vertices[halfEdgeDS->numVertices] = NULL;

    vtx->data = Z_Calloc(sizeof(mvertex_t), PU_STATIC, 0);
    ((mvertex_t*) vtx->data)->index = halfEdgeDS->numVertices; // 1-based index, 0 = NIL.

    return vtx;
}

/**
 * @note Only releases memory for the data structure itself, any objects linked
 * to the component parts of the data structure will remain (therefore this is
 * the caller's responsibility).
 */
void P_DestroyHalfEdgeDS(halfedgeds_t* halfEdgeDS)
{
    if(halfEdgeDS->faces)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->numFaces; ++i)
        {
            face_t* face = halfEdgeDS->faces[i];
            Z_Free(face);
        }

        Z_Free(halfEdgeDS->faces);
    }
    halfEdgeDS->faces = NULL;
    halfEdgeDS->numFaces = 0;

    if(halfEdgeDS->hEdges)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->numHEdges; ++i)
        {
            hedge_t* hEdge = halfEdgeDS->hEdges[i];
            Z_Free(hEdge);
        }

        Z_Free(halfEdgeDS->hEdges);
    }
    halfEdgeDS->hEdges = NULL;
    halfEdgeDS->numHEdges = 0;

    if(halfEdgeDS->vertices)
    {
        uint i;

        for(i = 0; i < halfEdgeDS->numVertices; ++i)
        {
            vertex_t* vertex = halfEdgeDS->vertices[i];
            Z_Free(vertex);
        }

        Z_Free(halfEdgeDS->vertices);
    }
    halfEdgeDS->vertices = NULL;
    halfEdgeDS->numVertices = 0;
}
