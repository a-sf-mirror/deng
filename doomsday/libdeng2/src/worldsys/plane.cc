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

#include "de/Plane"

using namespace de;

void Plane::resetHeightTracking()
{
    visHeightDelta = 0;
    oldHeight[0] = oldHeight[1] = height;
}

void Plane::updateHeightTracking()
{
    oldHeight[0] = oldHeight[1];
    oldHeight[1] = height;

    if(oldHeight[0] != oldHeight[1])
        if(fabs(oldHeight[0] - oldHeight[1]) >= MAX_SMOOTH_MOVE)
        {
            // Too fast: make an instantaneous jump.
            oldHeight[0] = oldHeight[1];
        }
}

void Plane::interpolateHeight(dfloat frameTimePos)
{
    visHeightDelta = oldHeight[0] * (1 - frameTimePos) + height * frameTimePos - height;
    // Visible plane height.
    visHeight = height + visHeightDelta;
}

#if 0
/**
 * Update the plane, property is selected by DMU_* name.
 */
bool Plane::setProperty(const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_HEIGHT:
        DMU_SetValue(DMT_PLANE_HEIGHT, &height, args, 0);
        if(!ddMapSetup)
        {
            Map* map = P_CurrentMap();
            if(!map->_watchedPlaneList)
                map->_watchedPlaneList = P_CreatePlaneList();

            PlaneList_Add(map->_watchedPlaneList, plane);

            for(uint i = 0; i < map->numSectors; ++i)
            {
                Sector* sec = map->sectors[i];
                for(uint j = 0; j < sec->planeCount; ++j)
                {
                    if(sec->planes[j] == plane)
                        R_MarkDependantSurfacesForDecorationUpdate(sec);
                }
            }
        }
        break;
    case DMU_TARGET_HEIGHT:
        DMU_SetValue(DMT_PLANE_TARGET, &target, args, 0);
        break;
    case DMU_SPEED:
        DMU_SetValue(DMT_PLANE_SPEED, &speed, args, 0);
        break;
    default:
        Con_Error("Plane_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a plane property, selected by DMU_* name.
 */
bool Plane::getProperty(setargs_t* args) const
{
    switch(args->prop)
    {
    case DMU_HEIGHT:
        DMU_GetValue(DMT_PLANE_HEIGHT, &height, args, 0);
        break;
    case DMU_TARGET_HEIGHT:
        DMU_GetValue(DMT_PLANE_TARGET, &target, args, 0);
        break;
    case DMU_SPEED:
        DMU_GetValue(DMT_PLANE_SPEED, &speed, args, 0);
        break;
    default:
        Con_Error("Plane_GetProperty: No property %s.\n", DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
#endif
