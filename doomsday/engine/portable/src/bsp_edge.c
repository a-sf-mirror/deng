/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 *\author Copyright © 1998 Raphael Quinet <raphael.quinet@eed.ericsson.se>
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

/**
 * bsp_edge.c: GL-friendly BSP node builder, half-edges.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_misc.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static __inline hedge_t* allocHEdge(void)
{
    return Z_Calloc(sizeof(hedge_t), PU_STATIC, 0);
}

static __inline void freeHEdge(hedge_t* hEdge)
{
    Z_Free(hEdge);
}

static __inline hedge_t* createHEdge(void)
{
    hedge_t* hEdge = allocHEdge();

    hEdge->data = Z_Calloc(sizeof(bsp_hedgeinfo_t), PU_STATIC, 0);
    return hEdge;
}

static void copyHEdge(hedge_t* dst, const hedge_t* src)
{
    dst->vertex = src->vertex;
    dst->twin = src->twin;
    dst->next = src->next;
    dst->prev = src->prev;
    dst->face = src->face;

    memcpy(dst->data, src->data, sizeof(bsp_hedgeinfo_t));
}

/**
 * Update the precomputed members of the hEdge.
 */
void BSP_UpdateHEdgeInfo(const hedge_t* hEdge)
{
    bsp_hedgeinfo_t* data = (bsp_hedgeinfo_t*) hEdge->data;

    data->pSX = hEdge->vertex->pos[VX];
    data->pSY = hEdge->vertex->pos[VY];
    data->pEX = hEdge->twin->vertex->pos[VX];
    data->pEY = hEdge->twin->vertex->pos[VY];
    data->pDX = data->pEX - data->pSX;
    data->pDY = data->pEY - data->pSY;

    data->pLength = M_Length(data->pDX, data->pDY);
    data->pAngle  = M_SlopeToAngle(data->pDX, data->pDY);

    if(data->pLength <= 0)
        Con_Error("HEdge %p has zero p_length.", hEdge);

    data->pPerp =  data->pSY * data->pDX - data->pSX * data->pDY;
    data->pPara = -data->pSX * data->pDX - data->pSY * data->pDY;
}

/**
 * Create a new half-edge.
 */
hedge_t* HEdge_Create(linedef_t* line, linedef_t* sourceLine,
                      vertex_t* start, sector_t* sec, boolean back)
{
    hedge_t* hEdge = createHEdge();

    hEdge->vertex = start;
    if(!start->hEdge)
        start->hEdge = hEdge;
    hEdge->twin = NULL;
    hEdge->next = hEdge->prev = hEdge;
    hEdge->face = NULL;

    {
    bsp_hedgeinfo_t* data = (bsp_hedgeinfo_t*) hEdge->data;

    data->lineDef = line;
    data->side = (back? 1 : 0);
    data->sector = sec;
    data->sourceLine = sourceLine;
    data->index = -1;
    data->lprev = data->lnext = NULL;
    }

    return hEdge;
}

/**
 * Destroys the given half-edge.
 *
 * @param hEdge         Ptr to the half-edge to be destroyed.
 */
void HEdge_Destroy(hedge_t* hEdge)
{
    if(hEdge)
    {
        freeHEdge(hEdge);
    }
}

/**
 * Splits the given half-edge at the point (x,y). The new half-edge is
 * returned. The old half-edge is shortened (the original start vertex is
 * unchanged), the new half-edge becomes the cut-off tail (keeping the
 * original end vertex).
 *
 * \note
 * If the half-edge has a twin, it is also split and is inserted into the
 * same list as the original (and after it), thus all half-edges (except the
 * one we are currently splitting) must exist on a singly-linked list
 * somewhere.
 *
 * \note
 * We must update the count values of any superblock that contains the
 * half-edge (and/or backseg), so that future processing is not messed up by
 * incorrect counts.
 */
hedge_t* HEdge_Split(hedge_t* oldHEdge, double x, double y)
{
    hedge_t* newHEdge;
    bsp_hedgeinfo_t* newData, *oldData = (bsp_hedgeinfo_t*) oldHEdge->data;
    vertex_t* newVert;

/*#if _DEBUG
if(oldHEdge->lineDef)
    Con_Message("Splitting LineDef %d (%p) at (%1.1f,%1.1f)\n",
                oldData->lineDef->index, oldHEdge, x, y);
else
    Con_Message("Splitting MiniHEdge %p at (%1.1f,%1.1f)\n", oldHEdge, x, y);
#endif*/

    // Update superblock, if needed.
    if(oldData->block)
        SuperBlock_IncHEdgeCounts(oldData->block, oldData->lineDef != NULL);

    /**
     * Create a new vertex (with correct wall_tip info) for the split that
     * happens along the given half-edge at the given location.
     */
    newVert = HalfEdgeDS_CreateVertex(Map_HalfEdgeDS(editMap));
    newVert->pos[VX] = x;
    newVert->pos[VY] = y;
    ((mvertex_t*) newVert->data)->refCount = (oldHEdge->twin? 4 : 2);

    // Compute wall_tip info.
    BSP_CreateVertexEdgeTip(newVert, -oldData->pDX, -oldData->pDY,
                            oldHEdge, oldHEdge->twin);
    BSP_CreateVertexEdgeTip(newVert, oldData->pDX, oldData->pDY,
                            oldHEdge->twin, oldHEdge);

    newHEdge = createHEdge();
    // Copy the old half-edge info.
    copyHEdge(newHEdge, oldHEdge);

    newData = (bsp_hedgeinfo_t*) newHEdge->data;

    if(((bsp_hedgeinfo_t*)oldHEdge->data)->lnext)
        ((bsp_hedgeinfo_t*) ((bsp_hedgeinfo_t*) oldHEdge->data)
            ->lnext->data)->lprev = newHEdge;
    oldData->lnext = newHEdge;
    newData->lprev = oldHEdge;

    newHEdge->prev = oldHEdge;
    oldHEdge->next = newHEdge;

    newHEdge->vertex = newVert;

/*#if _DEBUG
Con_Message("Splitting Vertex is %04X at (%1.1f,%1.1f)\n",
            newVert->index, newVert->V_pos[VX], newVert->V_pos[VY]);
#endif*/

    // Handle the twin.
    if(oldHEdge->twin)
    {
/*#if _DEBUG
Con_Message("Splitting hEdge->twin %p\n", oldHEdge->twin);
#endif*/

        newHEdge->twin = createHEdge();

        // Copy edge info.
        copyHEdge(newHEdge->twin, oldHEdge->twin);

        // It is important to keep the twin relationship valid.
        newHEdge->twin->twin = newHEdge;

        if(((bsp_hedgeinfo_t*)oldHEdge->twin->data)->lprev)
            ((bsp_hedgeinfo_t*) ((bsp_hedgeinfo_t*) oldHEdge->twin->data)
                ->lprev->data)->lnext = newHEdge->twin;

        newHEdge->twin->next = oldHEdge->twin;
        oldHEdge->twin->prev = newHEdge->twin;

        ((bsp_hedgeinfo_t*) newHEdge->twin->data)->lnext = oldHEdge->twin;
        ((bsp_hedgeinfo_t*) oldHEdge->twin->data)->lprev = newHEdge->twin;

        oldHEdge->twin->vertex = newVert;
    }

    BSP_UpdateHEdgeInfo(oldHEdge);
    BSP_UpdateHEdgeInfo(newHEdge);
    if(oldHEdge->twin)
    {
        bsp_hedgeinfo_t* oldInfo =(bsp_hedgeinfo_t*)oldHEdge->twin->data;

        BSP_UpdateHEdgeInfo(oldHEdge->twin);
        BSP_UpdateHEdgeInfo(newHEdge->twin);

        // Update superblock, if needed.
        if(oldInfo->block)
        {
            SuperBlock_IncHEdgeCounts(oldInfo->block, oldInfo->lineDef != NULL);
            SuperBlock_LinkHEdge(oldInfo->block, newHEdge->twin);
        }
        else if(oldInfo->leaf)
        {
            // Link it into list for the leaf.
            BSPLeaf_LinkHEdge(oldInfo->leaf, newHEdge->twin);
        }
    }

    return newHEdge;
}
