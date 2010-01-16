/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_spec.h: World texture animation, height or lighting changes
 * according to adjacent sectors, respective utility functions, etc.
 *
 * Line Tag handling. Line and Sector triggers.
 *
 * Events are operations triggered by using, crossing, or shooting special
 * lines, or by timed thinkers.
 */

#ifndef __P_SPEC__
#define __P_SPEC__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "d_player.h"

// Base plane ids.
enum {
    PLN_FLOOR,
    PLN_CEILING
};

// Special activation types:
#define SPAC_CROSS              0 // Player crosses linedef.
#define SPAC_USE                1 // Player uses linedef.
#define SPAC_IMPACT             3 // Projectile hits linedef.

// at game start
void            P_InitPicAnims(void);

boolean         P_ActivateLine(linedef_t *ld, mobj_t *mo, int side,
                               int activationType);

void            P_PlayerInSpecialSector(player_t *player);

typedef enum {
    ok,
    crushed,
    pastdest
} result_e;

typedef enum {
    build8, // slowly build by 8
    turbo16 // quickly build by 16
} stair_e;

int             EV_BuildStairs(linedef_t *line, stair_e type);

result_e    T_MovePlane(sector_t* sector, float speed, float dest,
                        int crush, int floorOrCeiling, int direction);
int             EV_DoDonut(linedef_t *line);

boolean     P_UseSpecialLine2(mobj_t* mo, linedef_t* line, int side);

#endif
