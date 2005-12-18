// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//  AutoMap module.
//
// Based on code by ID and Raven, modified by Skyjake
//
// 08/01/2005 DJS - First pass. Combined all game/am_map src files. Lots of hacky compiler directives...
//-----------------------------------------------------------------------------

#ifndef __AMMAP_H__
#define __AMMAP_H__

// DJS - Common\f_infine.c is calling here when compiling jHexen looking for this...
#ifdef __JHEXEN__
#include "jHexen/p_local.h"
#elif __JSTRIFE__
#include "jStrife/p_local.h"
#endif


#ifndef __JDOOM__
extern boolean  automapactive;  // Common\f_infine.c is looking for this if not jDoom... ??

// DJS - defined in Include\jDoom\Mn_def.h in all games but jDoom
#define LINEHEIGHT_A 10

 typedef ddvertex_t mpoint_t;
#endif

// Caleld by Menu
void    M_DrawMAP(void);

// Called by Init
void AM_Register(void);

// Called by main loop.
void            AM_Ticker(void);

// called instead of view drawer if automap active.
void            AM_Drawer(void);

// Called to force the automap to quit
// if the level is completed while it is up.
void            AM_Stop(void);


// Used by ST StatusBar stuff.
#define AM_MSGHEADER (('a'<<24)+('m'<<16))
#define AM_MSGENTERED (AM_MSGHEADER | ('e'<<8))
#define AM_MSGEXITED (AM_MSGHEADER | ('x'<<8))

// Keys
#define AM_STARTKEY DDKEY_TAB
#define AM_ENDKEY   DDKEY_TAB

#define AM_NUMMARKPOINTS 10

// the following is crap
#define LINE_NEVERSEE ML_DONTDRAW

// Counter Cheat flags.
#define CCH_KILLS           0x1
#define CCH_ITEMS           0x2
#define CCH_SECRET          0x4
#define CCH_KILLS_PRCNT         0x8
#define CCH_ITEMS_PRCNT         0x10
#define CCH_SECRET_PRCNT        0x20


#ifdef __JDOOM__
// For use if I do walls with outsides/insides
#define BLUES       (256-4*16+8)
#define YELLOWRANGE 1
#define BLACK       0
#define REDS        (256-5*16)
#define REDRANGE    16
#define BLUERANGE   8
#define GREENS      (7*16)
#define GREENRANGE  16
#define GRAYS       (6*16)
#define GRAYSRANGE  16
#define BROWNS      (4*16)
#define BROWNRANGE  16
#define YELLOWS     (256-32+7)
#define WHITE       (256-47)
#define YOURCOLORS      WHITE
#define YOURRANGE       0
#define WALLRANGE       REDRANGE
#define TSWALLCOLORS        GRAYS
#define TSWALLRANGE     GRAYSRANGE
#define FDWALLRANGE     BROWNRANGE
#define CDWALLRANGE     YELLOWRANGE
#define THINGCOLORS     GREENS
#define THINGRANGE      GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS      (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE       0
#define XHAIRCOLORS     GRAYS
#define WALLCOLORS      REDS
#define FDWALLCOLORS        BROWNS
#define CDWALLCOLORS        YELLOWS

#define BORDEROFFSET 3

// Automap colors
#define BACKGROUND      BLACK

// Keys for Baby Mode
#define KEY1   197      //  Blue Key
#define KEY2   (256-5*16)   //  Red Key
#define KEY3   (256-32+7)   //  Yellow Key
#define KEY4   (256-32+7)   //  Yellow Skull
#define KEY5   (256-5*16)   //  Red Skull
#define KEY6   197      //  Blue Skull

#define NUMBEROFKEYS 6

#define BORDERGRAPHIC "brdr_b"

#define MARKERPATCHES (sprintf(namebuf, "AMMNUM%d", i))     // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif


#ifdef __JHERETIC__

// For use if I do walls with outsides/insides
#define REDS        12*8
#define REDRANGE    1              //16
#define BLUES       (256-4*16+8)
#define BLUERANGE   1              //8
#define GREENS      (33*8)
#define GREENRANGE  1              //16
#define GRAYS       (5*8)
#define GRAYSRANGE  1              //16
#define BROWNS      (14*8)
#define BROWNRANGE  1              //16
#define YELLOWS     10*8
#define YELLOWRANGE 1
#define BLACK       0
#define WHITE       4*8
#define PARCH       13*8-1
#define WALLCOLORS      REDS
#define FDWALLCOLORS        BROWNS
#define CDWALLCOLORS        YELLOWS
#define YOURCOLORS      WHITE
#define YOURRANGE       0
#define WALLRANGE       REDRANGE
#define TSWALLCOLORS        GRAYS
#define TSWALLRANGE     GRAYSRANGE
#define FDWALLRANGE     BROWNRANGE
#define CDWALLRANGE     YELLOWRANGE
#define THINGCOLORS     GREENS
#define THINGRANGE      GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS      (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE       0
#define XHAIRCOLORS     GRAYS

#define BLOODRED  150

// Automap colors
#define BACKGROUND  PARCH
#define YOURCOLORS  WHITE
#define YOURRANGE   0

#define WALLRANGE   REDRANGE
#define TSWALLCOLORS    GRAYS
#define TSWALLRANGE GRAYSRANGE

#define FDWALLRANGE BROWNRANGE

#define CDWALLRANGE YELLOWRANGE
#define THINGCOLORS GREENS
#define THINGRANGE  GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS  (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE   0
#define XHAIRCOLORS GRAYS

#define BORDEROFFSET 4

// Keys for Baby Mode
#define KEY1   144  // HERETIC - Green Key
#define KEY2   197  // HERETIC - Yellow Key
#define KEY3   220  // HERETIC - Blue Key

#define NUMBEROFKEYS 3

#define BORDERGRAPHIC "bordb"

#define MARKERPATCHES (sprintf(namebuf, "FONTA%d", (16 +i) ))       // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif


#ifdef __JHEXEN__
// For use if I do walls with outsides/insides
#define REDS        12*8
#define REDRANGE    1              //16
#define BLUES       (256-4*16+8)
#define BLUERANGE   1              //8
#define GREENS      (33*8)
#define GREENRANGE  1              //16
#define GRAYS       (5*8)
#define GRAYSRANGE  1              //16
#define BROWNS      (14*8)
#define BROWNRANGE  1              //16
#define YELLOWS     10*8
#define YELLOWRANGE 1
#define BLACK       0
#define WHITE       4*8
#define PARCH       13*8-1
#define BLOODRED    177

// Automap colors
#define BACKGROUND  PARCH
#define YOURCOLORS  WHITE
#define YOURRANGE   0
#define WALLCOLORS  83      // REDS
#define WALLRANGE   REDRANGE
#define TSWALLCOLORS    GRAYS
#define TSWALLRANGE GRAYSRANGE
#define FDWALLCOLORS    96      // BROWNS
#define FDWALLRANGE BROWNRANGE
#define CDWALLCOLORS    107     // YELLOWS
#define CDWALLRANGE YELLOWRANGE
#define THINGCOLORS GREENS
#define THINGRANGE  GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS  (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE   0
#define XHAIRCOLORS GRAYS

#define BORDEROFFSET 4

// Automap colors
#define AM_PLR1_COLOR 157       // Blue
#define AM_PLR2_COLOR 177       // Red
#define AM_PLR3_COLOR 137       // Yellow
#define AM_PLR4_COLOR 198       // Green
#define AM_PLR5_COLOR 215       // Jade
#define AM_PLR6_COLOR 32        // White
#define AM_PLR7_COLOR 106       // Hazel
#define AM_PLR8_COLOR 234       // Purple

#define KEY1   197  // HEXEN -
#define KEY2   144  // HEXEN -
#define KEY3   220  // HEXEN -

#define NUMBEROFKEYS 3

#define BORDERGRAPHIC "bordb"

#define MARKERPATCHES (sprintf(namebuf, "FONTA%d", (16 +i) ))       // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif

#ifdef __JSTRIFE__
// For use if I do walls with outsides/insides
#define REDS        12*8
#define REDRANGE    1              //16
#define BLUES       (256-4*16+8)
#define BLUERANGE   1              //8
#define GREENS      (33*8)
#define GREENRANGE  1              //16
#define GRAYS       (5*8)
#define GRAYSRANGE  1              //16
#define BROWNS      (14*8)
#define BROWNRANGE  1              //16
#define YELLOWS     10*8
#define YELLOWRANGE 1
#define BLACK       0
#define WHITE       4*8
#define PARCH       13*8-1
#define BLOODRED    177

// Automap colors
#define BACKGROUND  PARCH
#define YOURCOLORS  WHITE
#define YOURRANGE   0
#define WALLCOLORS  83      // REDS
#define WALLRANGE   REDRANGE
#define TSWALLCOLORS    GRAYS
#define TSWALLRANGE GRAYSRANGE
#define FDWALLCOLORS    96      // BROWNS
#define FDWALLRANGE BROWNRANGE
#define CDWALLCOLORS    107     // YELLOWS
#define CDWALLRANGE YELLOWRANGE
#define THINGCOLORS GREENS
#define THINGRANGE  GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS  (GRAYS + GRAYSRANGE/2)
#define GRIDRANGE   0
#define XHAIRCOLORS GRAYS

#define BORDEROFFSET 4

// Automap colors
#define AM_PLR1_COLOR 157       // Blue
#define AM_PLR2_COLOR 177       // Red
#define AM_PLR3_COLOR 137       // Yellow
#define AM_PLR4_COLOR 198       // Green
#define AM_PLR5_COLOR 215       // Jade
#define AM_PLR6_COLOR 32        // White
#define AM_PLR7_COLOR 106       // Hazel
#define AM_PLR8_COLOR 234       // Purple

#define KEY1   197  // HEXEN -
#define KEY2   144  // HEXEN -
#define KEY3   220  // HEXEN -

#define NUMBEROFKEYS 3

#define BORDERGRAPHIC "bordb"

#define MARKERPATCHES (sprintf(namebuf, "FONTA%d", (16 +i) ))       // DJS - Patches used for marking the automap, a bit of a hack I suppose

#endif


#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.1.2.2  2005/12/18 15:06:02  danij
// Updated to use DMU.
// See comments in src files for notes on changes.
//
// Revision 1.1.2.1  2005/11/27 17:42:07  skyjake
// Breaking everything with the new Map Update API (=DMU) (only declared, not
// implemented yet)
//
// - jDoom and jHeretic do not compile
// - jHexen compiles by virtue of #ifdef TODO_MAP_UPDATE, which removes all the
// portions of code that would not compile
// - none of the games work, because DMU has not been implemented or used in any
// of the games
//
// Map data is now hidden from the games. The line_t, seg_t and structs are
// defined only as void*. The functions in the Map Update API (P_Set*, P_Get*,
// P_Callback, P_ToPtr, P_ToIndex) are used for reading and writing the map data
// parameters. There are multiple versions of each function so that using them is
// more convenient in terms of data types.
//
// P_Callback can be used for having the engine call a callback function for each
// of the selected map data objects.
//
// The API is not finalized yet.
//
// The DMU_* constants defined in dd_share.h are used as the 'type' and 'prop'
// parameters.
//
// The games require map data in numerous places of the code. All of these should
// be converted to work with DMU in the most sensible fashion (a direct
// conversion may not always make the most sense). E.g., jHexen has
// some private map data for sound playing, etc. The placement of this data is
// not certain at the moment, but it can remain private to the games if
// necessary.
//
// Games can build their own map changing routines on DMU as they see fit. The
// engine will only provide a generic API, as defined in doomsday.h currently.
//
// Revision 1.1  2005/05/29 05:14:15  danij
// Commonised automap code. Automap window display. Automap keys are bindable. Colours can be configured. Automap menu to configure options.
//
// Revision 1.5  2004/05/29 18:19:58  skyjake
// Refined indentation style
//
// Revision 1.4  2004/05/29 09:53:11  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.3  2004/05/28 17:16:34  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.1.2.1  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.1.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.1  2003/02/26 19:18:22  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:12  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------
