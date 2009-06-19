/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * r_local.h:
 */

#ifndef __R_LOCAL_H__
#define __R_LOCAL_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "jhexen.h"
#include "s_sequence.h"

#define ANGLETOSKYSHIFT         22 // sky map is 256*128*4 maps.

#define BASEYCENTER             100

#define MAXWIDTH                1120
#define MAXHEIGHT               832

#define PI                      3.141592657

#define CENTERY                 (SCREENHEIGHT/2)

#define MINZF                   (4)

#define FIELDOFVIEW             2048 // fineangles in the SCREENWIDTH wide window.

// Lighting constants.
#define LIGHTLEVELS             16
#define LIGHTSEGSHIFT           4
#define MAXLIGHTSCALE           48
#define LIGHTSCALESHIFT         12
#define MAXLIGHTZ               128
#define LIGHTZSHIFT             20
#define NUMCOLORMAPS            32 // Number of diminishing.
#define INVERSECOLORMAP         32

// Player colors.
#define AM_PLR1_COLOR           157 // Blue
#define AM_PLR2_COLOR           177 // Red
#define AM_PLR3_COLOR           137 // Yellow
#define AM_PLR4_COLOR           198 // Green
#define AM_PLR5_COLOR           215 // Jade
#define AM_PLR6_COLOR           32 // White
#define AM_PLR7_COLOR           106 // Hazel
#define AM_PLR8_COLOR           234 // Purple

// Map data is in the main engine, so these are helpers...
extern angle_t  clipangle;

extern int viewangletox[FINEANGLES / 2];
extern angle_t xtoviewangle[SCREENWIDTH + 1];
extern fixed_t finetangent[FINEANGLES / 2];

extern fixed_t rw_distance;
extern angle_t rw_normalangle;
extern int centerx, centery;
extern int flyheight;

extern int sscount, linecount, loopcount;
extern int extralight;

void            R_InitData(void);
void            R_UpdateData(void);

extern fixed_t  pspritescale, pspriteiscale;

void            R_UpdateTranslationTables(void);

#endif
