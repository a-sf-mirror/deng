/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * r_subsector.c: World subsectors.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Subsector_UpdateMidPoint(subsector_t* subsector)
{
    hedge_t* hEdge;

    assert(subsector);

    // Find the center point. First calculate the bounding box.
    if((hEdge = subsector->face->hEdge))
    {
        vertex_t* vtx;

        vtx = hEdge->HE_v1;
        subsector->bBox[0].pos[VX] = subsector->bBox[1].pos[VX] = subsector->midPoint.pos[VX] = vtx->pos[VX];
        subsector->bBox[0].pos[VY] = subsector->bBox[1].pos[VY] = subsector->midPoint.pos[VY] = vtx->pos[VY];

        while((hEdge = hEdge->next) != subsector->face->hEdge)
        {
            vtx = hEdge->HE_v1;

            if(vtx->pos[VX] < subsector->bBox[0].pos[VX])
                subsector->bBox[0].pos[VX] = vtx->pos[VX];
            if(vtx->pos[VY] < subsector->bBox[0].pos[VY])
                subsector->bBox[0].pos[VY] = vtx->pos[VY];
            if(vtx->pos[VX] > subsector->bBox[1].pos[VX])
                subsector->bBox[1].pos[VX] = vtx->pos[VX];
            if(vtx->pos[VY] > subsector->bBox[1].pos[VY])
                subsector->bBox[1].pos[VY] = vtx->pos[VY];

            subsector->midPoint.pos[VX] += vtx->pos[VX];
            subsector->midPoint.pos[VY] += vtx->pos[VY];
        }

        subsector->midPoint.pos[VX] /= subsector->hEdgeCount; // num vertices.
        subsector->midPoint.pos[VY] /= subsector->hEdgeCount;
    }

    // Calculate the worldwide grid offset.
    subsector->worldGridOffset[VX] = fmod(subsector->bBox[0].pos[VX], 64);
    subsector->worldGridOffset[VY] = fmod(subsector->bBox[1].pos[VY], 64);
}

/**
 * Update the subsector, property is selected by DMU_* name.
 */
boolean Subsector_SetProperty(subsector_t* subsector, const setargs_t* args)
{
    Con_Error("Subsector_SetProperty: Property %s is not writable.\n",
              DMU_Str(args->prop));

    return true; // Continue iteration.
}

/**
 * Get the value of a subsector property, selected by DMU_* name.
 */
boolean Subsector_GetProperty(const subsector_t* subsector, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_SECTOR:
        {
        sector_t* sec = subsector->sector;
        objectrecord_t* r = P_ObjectRecord(DMU_SECTOR, sec);
        DMU_GetValue(DMT_SUBSECTOR_SECTOR, &r, args, 0);
        break;
        }
    default:
        Con_Error("Subsector_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
