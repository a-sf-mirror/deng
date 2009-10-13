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
 * r_defs.h: shared data struct definitions.
 */

#ifndef __R_DEFS_H__
#define __R_DEFS_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

// Screenwidth.
#include "doomdef.h"

#include "p_xg.h"

#define SP_floororigheight      planes[PLN_FLOOR].origHeight
#define SP_ceilorigheight       planes[PLN_CEILING].origHeight

// Stair build flags.
#define BL_BUILT        0x1
#define BL_WAS_BUILT    0x2
#define BL_SPREADED     0x4

typedef struct xsector_s {
    short           special;
    short           tag;

    // 0 = untraversed, 1,2 = sndlines -1
    int             soundTraversed;

    // thing that made a sound (or null)
    struct mobj_s*  soundTarget;

    // thinker_t for reversable actions
    void*           specialData;

    byte            blFlags; // Used during stair building.

    // stone, metal, heavy, etc...
    byte            seqType;       // NOT USED ATM

    int             origID; // Original ID from the archived map format.

    struct {
        float       origHeight;
    } planes[2];    // {floor, ceiling}

    float           origLight;
    float           origRGB[3];
    xgsector_t*     xg;
} xsector_t;

typedef struct xline_s {
    short           special;
    short           tag;
    short           flags;
    // Has been rendered at least once and needs to appear in the map,
    // for each player.
    boolean         mapped[MAXPLAYERS];
    int             validCount;

    int             origID; // Original ID from the archived map format.

    // Extended generalized lines.
    xgline_t*       xg;
} xline_t;

extern xline_t* xlines;
extern xsector_t* xsectors;

xline_t*    P_ToXLine(linedef_t* line);
xline_t*    P_GetXLine(uint index);
xsector_t*  P_ToXSector(sector_t* sector);
xsector_t*  P_GetXSector(uint index);
xsector_t*  P_ToXSectorOfSubsector(subsector_t* subsector);
#endif
