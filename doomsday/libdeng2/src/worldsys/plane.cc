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
 * r_plane.c: World planes.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// $smoothplane: Maximum speed for a smoothed plane.
#define MAX_SMOOTH_PLANE_MOVE   (64)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Plane_ResetHeightTracking(plane_t* plane)
{
    if(!plane)
        return;

    plane->visHeightDelta = 0;
    plane->oldHeight[0] = plane->oldHeight[1] = plane->height;
}

void Plane_UpdateHeightTracking(plane_t* plane)
{
    if(!plane)
        return;

    plane->oldHeight[0] = plane->oldHeight[1];
    plane->oldHeight[1] = plane->height;

    if(plane->oldHeight[0] != plane->oldHeight[1])
        if(fabs(plane->oldHeight[0] - plane->oldHeight[1]) >=
           MAX_SMOOTH_PLANE_MOVE)
        {
            // Too fast: make an instantaneous jump.
            plane->oldHeight[0] = plane->oldHeight[1];
        }
}

void Plane_InterpolateHeight(plane_t* plane)
{
    if(!plane)
        return;

    plane->visHeightDelta = plane->oldHeight[0] * (1 - frameTimePos) +
                plane->height * frameTimePos -
                plane->height;

    // Visible plane height.
    plane->visHeight = plane->height + plane->visHeightDelta;
}

/**
 * Update the plane, property is selected by DMU_* name.
 */
boolean Plane_SetProperty(plane_t* plane, const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_HEIGHT:
        DMU_SetValue(DMT_PLANE_HEIGHT, &plane->height, args, 0);
        if(!ddMapSetup)
        {
            uint i;
            Map* map = P_CurrentMap();

            if(!map->_watchedPlaneList)
                map->_watchedPlaneList = P_CreatePlaneList();

            PlaneList_Add(map->_watchedPlaneList, plane);

            for(i = 0; i < map->numSectors; ++i)
            {
                Sector* sec = map->sectors[i];

                uint j;

                for(j = 0; j < sec->planeCount; ++j)
                {
                    if(sec->planes[j] == plane)
                        R_MarkDependantSurfacesForDecorationUpdate(sec);
                }
            }
        }
        break;
    case DMU_TARGET_HEIGHT:
        DMU_SetValue(DMT_PLANE_TARGET, &plane->target, args, 0);
        break;
    case DMU_SPEED:
        DMU_SetValue(DMT_PLANE_SPEED, &plane->speed, args, 0);
        break;
    default:
        Con_Error("Plane_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a plane property, selected by DMU_* name.
 */
boolean Plane_GetProperty(const plane_t* plane, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_HEIGHT:
        DMU_GetValue(DMT_PLANE_HEIGHT, &plane->height, args, 0);
        break;
    case DMU_TARGET_HEIGHT:
        DMU_GetValue(DMT_PLANE_TARGET, &plane->target, args, 0);
        break;
    case DMU_SPEED:
        DMU_GetValue(DMT_PLANE_SPEED, &plane->speed, args, 0);
        break;
    default:
        Con_Error("Plane_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
