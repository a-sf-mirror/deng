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

static __inline edgetip_t* allocEdgeTip(void)
{
    return M_Calloc(sizeof(edgetip_t));
}

static __inline void freeEdgeTip(edgetip_t* tip)
{
    M_Free(tip);
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
    hEdge->twin = NULL;
    hEdge->next = hEdge->prev = NULL;
    hEdge->face = NULL;

    {
    bsp_hedgeinfo_t*         data = (bsp_hedgeinfo_t*) hEdge->data;

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
        bsp_hedgeinfo_t*    oldInfo =(bsp_hedgeinfo_t*)oldHEdge->twin->data;

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

void BSP_CreateVertexEdgeTip(vertex_t* vert, double dx, double dy,
                             hedge_t* back, hedge_t* front)
{
    edgetip_t* tip = allocEdgeTip();
    edgetip_t* after;

    tip->angle = M_SlopeToAngle(dx, dy);
    tip->ET_edge[BACK]  = back;
    tip->ET_edge[FRONT] = front;

    // Find the correct place (order is increasing angle).
    for(after = ((mvertex_t*) vert->data)->tipSet; after && after->ET_next;
        after = after->ET_next);

    while(after && tip->angle + ANG_EPSILON < after->angle)
        after = after->ET_prev;

    // Link it in.
    if(after)
        tip->ET_next = after->ET_next;
    else
        tip->ET_next = ((mvertex_t*) vert->data)->tipSet;
    tip->ET_prev = after;

    if(after)
    {
        if(after->ET_next)
            after->ET_next->ET_prev = tip;

        after->ET_next = tip;
    }
    else
    {
        if(((mvertex_t*) vert->data)->tipSet)
            ((mvertex_t*) vert->data)->tipSet->ET_prev = tip;

        ((mvertex_t*) vert->data)->tipSet = tip;
    }
}

void BSP_DestroyVertexEdgeTip(edgetip_t* tip)
{
    if(tip)
    {
        freeEdgeTip(tip);
    }
}

/**
 * Check whether a line with the given delta coordinates and beginning
 * at this vertex is open. Returns a sector reference if it's open,
 * or NULL if closed (void space or directly along a linedef).
 */
sector_t* BSP_VertexCheckOpen(vertex_t* vertex, double dX, double dY)
{
    edgetip_t* tip;
    angle_g angle = M_SlopeToAngle(dX, dY);

    // First check whether there's a wall_tip that lies in the exact
    // direction of the given direction (which is relative to the
    // vertex).
    for(tip = ((mvertex_t*) vertex->data)->tipSet; tip; tip = tip->ET_next)
    {
        angle_g diff = fabs(tip->angle - angle);

        if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
        {   // Yes, found one.
            return NULL;
        }
    }

    // OK, now just find the first wall_tip whose angle is greater than
    // the angle we're interested in. Therefore we'll be on the FRONT
    // side of that tip edge.
    for(tip = ((mvertex_t*) vertex->data)->tipSet; tip; tip = tip->ET_next)
    {
        if(angle + ANG_EPSILON < tip->angle)
        {   // Found it.
            return (tip->ET_edge[FRONT]?
                ((bsp_hedgeinfo_t*) tip->ET_edge[FRONT]->data)->sector : NULL);
        }

        if(!tip->ET_next)
        {
            // No more tips, thus we must be on the BACK side of the tip
            // with the largest angle.
            return (tip->ET_edge[BACK]?
                ((bsp_hedgeinfo_t*) tip->ET_edge[BACK]->data)->sector : NULL);
        }
    }

    Con_Error("Vertex %d has no tips !", ((mvertex_t*) vertex->data)->index);
    return NULL;
}
