/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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
 * d_refresh.h:
 */

#ifndef __D_REFRESH_H__
#define __D_REFRESH_H__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "p_mobj.h"

extern float quitDarkenOpacity;

void            D_Display(int layer);
void            D_Display2(void);
void            R_SetViewSize(int blocks);

void            R_DrawSpecialFilter(int pnum);
void            R_DrawMapTitle(void);

void            P_SetDoomsdayFlags(mobj_t* mo);
void            R_SetAllDoomsdayFlags(void);
boolean         R_GetFilterColor(float rgba[4], int filter);
#endif
