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
 * r_lindef.h: World linedefs.
 */

#ifndef __DOOMSDAY_WORLD_LINEDEF_H__
#define __DOOMSDAY_WORLD_LINEDEF_H__

#include "r_data.h"
#include "p_dmu.h"

float           LineDef_LightLevelDelta(const linedef_t* lineDef);
void            LineDef_UnitVector(const linedef_t* lineDef, float* unitvec);

int             LineDef_PointOnSide(const linedef_t* lineDef, float x, float y);
int             LineDef_BoxOnSide(const linedef_t* lineDef, const float* tmbox);
int             LineDef_BoxOnSide2(const linedef_t* lineDef, float xl, float xh, float yl, float yh);

void            LineDef_ConstructDivline(const linedef_t* lineDef, divline_t* dl);

boolean         LineDef_GetProperty(const linedef_t* lineDef, setargs_t* args);
boolean         LineDef_SetProperty(linedef_t* lineDef, const setargs_t* args);

boolean         LineDef_IterateMobjs(linedef_t* line, boolean (*func) (struct mobj_s*, void*), void* data);

#endif
