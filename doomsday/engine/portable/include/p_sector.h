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

#ifndef DOOMSDAY_MAP_SECTOR_H
#define DOOMSDAY_MAP_SECTOR_H

#include "r_data.h"
#include "p_dmu.h"

void            Sector_UpdateSoundEnvironment(sector_t* sec);
float           Sector_LightLevel(sector_t* sec);
void            Sector_UpdateBounds(sector_t* sec);
void            Sector_Bounds(sector_t* sec, float* min, float* max);
boolean         Sector_PlanesChanged(sector_t* sec);

boolean         Sector_PointInside(const sector_t* sec, float x, float y);
boolean         Sector_PointInside2(const sector_t* sec, float x, float y);

boolean         Sector_GetProperty(const sector_t* sec, setargs_t* args);
boolean         Sector_SetProperty(sector_t* sec, const setargs_t* args);

boolean         Sector_IterateMobjsTouching(sector_t* sector, int (*func) (void*, void*), void* data);

#endif /* DOOMSDAY_MAP_SECTOR */
