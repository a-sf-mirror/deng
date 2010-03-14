/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_ceiling.h: Common playsim routines relating to ceilings.
 */

#ifndef __COMMON_THINKER_CEILING_H__
#define __COMMON_THINKER_CEILING_H__

typedef enum {
    CS_DOWN,
    CS_UP
} ceilingstate_e;

typedef enum {
    CT_LOWERTOFLOOR,
    CT_RAISETOHIGHEST,
    CT_LOWERANDCRUSH,
    CT_CRUSHANDRAISE,
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    CT_CRUSHANDRAISEFAST,
#endif
#if __JDOOM__ || __JDOOM64__
    CT_SILENTCRUSHANDRAISE,
#endif
#if __JDOOM64__
    CT_CUSTOM,
#endif
#if __JHEXEN__
    CT_LOWERBYVALUE,
    CT_RAISEBYVALUE,
    CT_CRUSHRAISEANDSTAY,
    CT_MOVETOVALUEMUL8,
#endif
    NUMCEILINGTYPES
} ceilingtype_e;

typedef struct {
    thinker_t       thinker;
    ceilingtype_e   type;
    sector_t*       sector;
    float           bottomHeight;
    float           topHeight;
    float           speed;
    boolean         crush;
    ceilingstate_e  state;
    ceilingstate_e  oldState;
    int             tag; // id.
} ceiling_t;

#define CEILSPEED           (1)
#define CEILWAIT            (150)

void        T_MoveCeiling(ceiling_t* c);
#if __JHEXEN__
int         EV_DoCeiling(linedef_t* line, byte* args, ceilingtype_e type);
#else
int         EV_DoCeiling(linedef_t* li, ceilingtype_e type);
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
int         P_CeilingActivate(GameMap* map, short tag);
#endif
int         P_CeilingDeactivate(GameMap* map, short tag);

#endif
