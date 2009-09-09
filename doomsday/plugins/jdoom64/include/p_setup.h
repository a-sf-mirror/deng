/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * p_setup.h: Map setup routines
 */

#ifndef __P_SETUP_H__
#define __P_SETUP_H__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

/**
 * Game-specific DMU object type constants.
 *
 * Do not change the numerical values of the constants (they are used during
 * serialization for identification)! Add new constants at the end.
 */
enum {
    DMU_THINKER_CEILMOVER = DMU_FIRST_GAME_TYPE,
    DMU_THINKER_DOOR,
    DMU_THINKER_FLOORMOVER,
    DMU_THINKER_LIGHTGLOW,
    DMU_THINKER_LIGHTBLINK,
    DMU_THINKER_LIGHTFLASH,
    DMU_THINKER_LIGHTFLICK,
    DMU_THINKER_LIGHTSTROBE,
    DMU_THINKER_MATCHANGER,
    DMU_THINKER_PLANEMOVER,
    DMU_THINKER_PLATFORM,
    DMU_THINKER_XLINE,
    DMU_THINKER_XSECTOR
};

// Map objects and their properties:
enum {
    MO_NONE = 0,
    MO_THING,
    MO_XLINEDEF,
    MO_XSECTOR,
    MO_LIGHT,
    MO_X,
    MO_Y,
    MO_Z,
    MO_ID,
    MO_ANGLE,
    MO_TYPE,
    MO_DOOMEDNUM,
    MO_USETYPE,
    MO_FLAGS,
    MO_TAG,
    MO_DRAWFLAGS,
    MO_TEXFLAGS,
    MO_COLORR,
    MO_COLORG,
    MO_COLORB,
    MO_FLOORCOLOR,
    MO_CEILINGCOLOR,
    MO_UNKNOWNCOLOR,
    MO_WALLTOPCOLOR,
    MO_WALLBOTTOMCOLOR,
    MO_XX0,
    MO_XX1,
    MO_XX2
};

void            P_Init(void);
void            P_RegisterMapObjs(void);

int             P_HandleMapDataPropertyValue(uint id, int dtype, int prop,
                                             valuetype_t type, void* data);
int             P_HandleMapObjectStatusReport(int code, uint id, int dtype,
                                              void* data);

boolean         P_MapExists(int episode, int map);

#endif
