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
 * p_start.h: Common playsim code relating to the (re)spawn of map objects.
 */

#ifndef __COMMON_PLAYSTART_H__
#define __COMMON_PLAYSTART_H__

#if __JHEXEN__
# include "xddefs.h"
#endif

void            P_Init(void);
mobjtype_t      P_DoomEdNumToMobjType(int doomEdNum);
void            P_MoveThingsOutOfWalls(struct map_s* map);
#if __JHERETIC__
void            P_TurnGizmosAwayFromDoors(struct map_s* map);
#endif

void            P_RebornPlayer(int plrNum);

boolean         P_CheckSpot(struct map_s* map, float x, float y);
#endif
