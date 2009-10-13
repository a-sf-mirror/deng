/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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

#ifndef __R_DEFS__
#define __R_DEFS__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "p_xg.h"

#define SP_floororigheight  planes[PLN_FLOOR].origHeight
#define SP_ceilorigheight   planes[PLN_CEILING].origHeight

// Stair build flags.
#define BL_BUILT            0x1
#define BL_WAS_BUILT        0x2
#define BL_SPREADED         0x4

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

/**
 * xline_t flags:
 */

#define ML_BLOCKMONSTERS        2 // Blocks monsters only.
#define ML_SECRET               32 // In AutoMap: don't map as two sided: IT'S A SECRET!
#define ML_SOUNDBLOCK           64 // Sound rendering: don't let sound cross two of these.
#define ML_DONTDRAW             128 // Don't draw on the automap at all.
#define ML_MAPPED               256 // Set if already seen, thus drawn in automap.

// FIXME! DJS - This is important!
// Doom64tc unfortunetly used non standard values for the linedef flags
// it implemented from BOOM. It will make life simpler if we simply
// update the Doom64TC IWAD rather than carry this on much further as
// once jDoom64 is released with 1.9.0 I imagine we'll see a bunch
// PWADs start cropping up.

//#define ML_PASSUSE            512 // Allows a USE action to pass through a linedef with a special
//#define ML_ALLTRIGGER         1024 // If set allows any mobj to trigger the linedef's special
//#define ML_INVALID            2048 // If set ALL flags NOT in DOOM v1.9 will be zeroed upon map load. ML_BLOCKING -> ML_MAPPED inc will persist.
//#define VALIDMASK             0x000001ff

#define ML_ALLTRIGGER           512 // Anything can use linedef if this is set - kaiser
#define ML_PASSUSE              1024
#define ML_BLOCKALL             2048

typedef struct xline_s {
    short           special;
    short           tag;
    short           flags;
    // Has been rendered at least once and needs to appear in the map,
    // for each player.
    boolean         mapped[MAXPLAYERS];
    int             validCount;

    int             origID; // Original ID from the archived map format.

    // jDoom64 specific:
    short           useOn;

    // Extended generalized lines.
    xgline_t*       xg;
} xline_t;

// Our private map data structures.
extern xsector_t* xsectors;
extern xline_t* xlines;

// If true we are in the process of setting up a map.
extern boolean mapSetup;

xline_t*        P_ToXLine(linedef_t* line);
xsector_t*      P_ToXSector(sector_t* sector);
xsector_t*      P_ToXSectorOfSubsector(subsector_t* subsector);

xline_t*        P_GetXLine(uint index);
xsector_t*      P_GetXSector(uint index);
#endif
