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

/**
 * LineDef flags:
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

#endif
