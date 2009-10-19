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
    vertex_t* vtx = Z_Malloc(sizeof(*vtx), PU_STATIC, 0);

    halfEdgeDS->vertices = Z_Realloc(halfEdgeDS->vertices,
        sizeof(vtx) * (++halfEdgeDS->numVertices + 1), PU_STATIC);
    halfEdgeDS->vertices[halfEdgeDS->numVertices-1] = vtx;
    halfEdgeDS->vertices[halfEdgeDS->numVertices] = NULL;

    vtx->data = Z_Calloc(sizeof(mvertex_t), PU_STATIC, 0);
    ((mvertex_t*) vtx->data)->index = halfEdgeDS->numVertices; // 1-based index, 0 = NIL.

    return vtx;
}
