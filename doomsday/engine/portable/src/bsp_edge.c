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

/**
 * Update the precomputed members of the hEdge.
 */
void BSP_UpdateHEdgeInfo(const hedge_t* hEdge)
{
    bsp_hedgeinfo_t* info = (bsp_hedgeinfo_t*) hEdge->data;

    info->pSX = hEdge->vertex->pos[VX];
    info->pSY = hEdge->vertex->pos[VY];
    info->pEX = hEdge->twin->vertex->pos[VX];
    info->pEY = hEdge->twin->vertex->pos[VY];
    info->pDX = info->pEX - info->pSX;
    info->pDY = info->pEY - info->pSY;

    info->pLength = M_Length(info->pDX, info->pDY);
    info->pAngle  = M_SlopeToAngle(info->pDX, info->pDY);

    if(info->pLength <= 0)
        Con_Error("HEdge %p has zero p_length.", hEdge);

    info->pPerp =  info->pSY * info->pDX - info->pSX * info->pDY;
    info->pPara = -info->pSX * info->pDX - info->pSY * info->pDY;
}

static void attachHEdgeInfo(hedge_t* hEdge, linedef_t* line,
                            linedef_t* sourceLine, sector_t* sec,
                            boolean back)
{
    assert(hEdge);
    {
    bsp_hedgeinfo_t* info = Z_Calloc(sizeof(bsp_hedgeinfo_t), PU_STATIC, 0);
    info->lineDef = line;
    info->lprev = info->lnext = NULL;
    info->side = (back? 1 : 0);
    info->sector = sec;
    info->sourceLine = sourceLine;
    hEdge->data = info;
    }
}

hedge_t* BSP_CreateHEdge(linedef_t* line, linedef_t* sourceLine,
                         vertex_t* start, sector_t* sec, boolean back)
{
    hedge_t* hEdge = HalfEdgeDS_CreateHEdge(Map_HalfEdgeDS(editMap), start);

    attachHEdgeInfo(hEdge, line, sourceLine, sec, back);
    return hEdge;
}

/**
 * \note
 * If the half-edge has a twin, it is also split and is inserted into the
 * same list as the original (and after it), thus all half-edges (except the
 * one we are currently splitting) must exist on a singly-linked list
 * somewhere.
 *
 * \note
 * We must update the count values of any SuperBlock that contains the
 * half-edge (and/or backseg), so that future processing is not messed up by
 * incorrect counts.
 */
hedge_t* BSP_SplitHEdge(hedge_t* oldHEdge, double x, double y)
{
    assert(oldHEdge);
    {
    hedge_t* newHEdge = HEdge_Split(Map_HalfEdgeDS(editMap), oldHEdge, x, y);

    attachHEdgeInfo(newHEdge, ((bsp_hedgeinfo_t*) oldHEdge->data)->lineDef, ((bsp_hedgeinfo_t*) oldHEdge->data)->sourceLine, ((bsp_hedgeinfo_t*) oldHEdge->data)->sector, ((bsp_hedgeinfo_t*) oldHEdge->data)->side);
    attachHEdgeInfo(newHEdge->twin, ((bsp_hedgeinfo_t*) oldHEdge->twin->data)->lineDef, ((bsp_hedgeinfo_t*) oldHEdge->twin->data)->sourceLine, ((bsp_hedgeinfo_t*) oldHEdge->twin->data)->sector, ((bsp_hedgeinfo_t*) oldHEdge->twin->data)->side);

    memcpy(newHEdge->data, oldHEdge->data, sizeof(bsp_hedgeinfo_t));
    memcpy(newHEdge->twin->data, oldHEdge->twin->data, sizeof(bsp_hedgeinfo_t));

    { // Update along-linedef relationships.
    if(((bsp_hedgeinfo_t*)oldHEdge->data)->lnext)
        ((bsp_hedgeinfo_t*) ((bsp_hedgeinfo_t*) oldHEdge->data)
            ->lnext->data)->lprev = newHEdge;

    if(((bsp_hedgeinfo_t*)oldHEdge->twin->data)->lprev)
        ((bsp_hedgeinfo_t*) ((bsp_hedgeinfo_t*) oldHEdge->twin->data)
            ->lprev->data)->lnext = newHEdge->twin;

    ((bsp_hedgeinfo_t*) oldHEdge->data)->lnext = newHEdge;
    ((bsp_hedgeinfo_t*) newHEdge->data)->lprev = oldHEdge;
    ((bsp_hedgeinfo_t*) newHEdge->twin->data)->lnext = oldHEdge->twin;
    ((bsp_hedgeinfo_t*) oldHEdge->twin->data)->lprev = newHEdge->twin;

    if(((bsp_hedgeinfo_t*) oldHEdge->data)->lineDef)
    {
        linedef_t* lineDef = ((bsp_hedgeinfo_t*) oldHEdge->data)->lineDef;
        if(((bsp_hedgeinfo_t*) oldHEdge->data)->side == FRONT)
        {
            if(lineDef->hEdges[1] == oldHEdge)
                lineDef->hEdges[1] = newHEdge;
        }
        else
        {
            if(lineDef->hEdges[0] == oldHEdge->twin)
                lineDef->hEdges[0] = newHEdge->twin;
        }
    }
    }

    BSP_UpdateHEdgeInfo(oldHEdge);
    BSP_UpdateHEdgeInfo(oldHEdge->twin);
    BSP_UpdateHEdgeInfo(newHEdge);
    BSP_UpdateHEdgeInfo(newHEdge->twin);

    if(!oldHEdge->twin->face && ((bsp_hedgeinfo_t*)oldHEdge->twin->data)->block)
    {
        SuperBlock_IncHEdgeCounts(((bsp_hedgeinfo_t*)oldHEdge->twin->data)->block, ((bsp_hedgeinfo_t*) newHEdge->twin->data)->lineDef != NULL);
        SuperBlock_PushHEdge(((bsp_hedgeinfo_t*)oldHEdge->twin->data)->block, newHEdge->twin);
        ((bsp_hedgeinfo_t*) newHEdge->twin->data)->block = ((bsp_hedgeinfo_t*)oldHEdge->twin->data)->block;
    }

    return newHEdge;
    }
}
