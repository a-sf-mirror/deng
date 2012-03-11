/**\file r_lgrid.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Ambient world lighting (smoothed sector lighting).
 */

#ifndef LIBDENG_REFRESH_LIGHT_GRID_H
#define LIBDENG_REFRESH_LIGHT_GRID_H

void LG_Register(void);
void LG_InitForMap(void);
void LG_Update(void);

/**
 * Called when a setting is changed which affects the lightgrid.
 */
void LG_MarkAllForUpdate(void);

void LG_SectorChanged(Sector* sector);

/**
 * Calculate the light color for a 3D point in the world.
 *
 * @param point  3D point.
 * @param color  Evaluated color of the point (return value).
 */
void LG_Evaluate(const vectorcomp_t point[3], vectorcomp_t destColor[3]);

/**
 * Calculate the light level for a 3D point in the world.
 *
 * @param point  3D point.
 * @return  Evaluated light level of the point.
 */
float LG_EvaluateLightLevel(const vectorcomp_t point[3]);

void LG_Debug(void);

#endif/* LIBDENG_REFRESH_LIGHT_GRID_H */
