/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * rend_sky.h: Sky Sphere and 3D Models
 */

#ifndef DOOMSDAY_SKY_H
#define DOOMSDAY_SKY_H

// Sky hemispheres.
#define SKYHEMI_UPPER       0x1
#define SKYHEMI_LOWER       0x2

extern int skyDetail, skySimple, skyColumns, skyRows, skyFull;
extern float skyDist;
extern boolean skyUpdateSphere;

// Initialization:
void            Rend_SkyRegister(void);

void            Rend_ShutdownSky(void);

void            Rend_SetSkyDetail(int quarterDivs, int rows);

void            Rend_RenderSky(struct sky_s* sky, int hemis);

#endif /* DOOMSDAY_SKY_H */
