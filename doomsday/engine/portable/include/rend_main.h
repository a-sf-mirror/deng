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
 * rend_main.h: Rendering Subsystem
 */

#ifndef __DOOMSDAY_REND_MAIN_H__
#define __DOOMSDAY_REND_MAIN_H__

#include <math.h>
#include "rend_list.h"
#include "r_things.h"

extern byte smoothTexAnim;
extern int missileBlend;
extern boolean usingFog;
extern float fogColor[4];
extern int gameDrawHUD;
extern int useShinySurfaces;

extern int devRendSkyMode;
extern byte devRendSkyAlways;
extern byte devNoTexFix;
extern byte devBlockmap;
extern float devBlockmapSize;

void            Rend_Register(void);
void            Rend_Init(void);
void            Rend_Reset(void);

void            Rend_RenderMap(struct map_s* map);

void            Rend_ModelViewMatrix(boolean use_angles);

void            Rend_RenderLightModRange(void);
#endif
