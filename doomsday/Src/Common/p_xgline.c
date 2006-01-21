/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * p_xgline.c: Extended Generalized Line Types.
 *
 * Implements all XG line interactions on a map
 */

// HEADER FILES ------------------------------------------------------------

#include <time.h>
#include <stdarg.h>

#ifdef __JDOOM__
#  include "doomdef.h"
#  include "p_local.h"
#  include "doomstat.h"
#  include "d_config.h"
#  include "s_sound.h"
#  include "m_random.h"
#  include "p_inter.h"
#  include "r_defs.h"
#  include "g_game.h"
#endif

#ifdef __JHERETIC__
#  include "jHeretic/Doomdef.h"
#  include "jHeretic/P_local.h"
#  include "h_config.h"
#  include "jHeretic/Soundst.h"
#  include "jHeretic/G_game.h"
#endif

#ifdef __JSTRIFE__
#  include "jStrife/h2def.h"
#  include "jStrife/p_local.h"
#  include "jStrife/d_config.h"
#  include "jStrife/sounds.h"
#endif

#include "dmu_lib.h"

#include "Common/p_mapsetup.h"
#include "d_net.h"
#include "p_xgline.h"
#include "p_xgsec.h"

// MACROS ------------------------------------------------------------------

#define XLTIMER_STOPPED 1    // Timer stopped.

#define EVTYPESTR(evtype) (evtype == XLE_CHAIN? "CHAIN" \
        : evtype == XLE_CROSS? "CROSS" \
        : evtype == XLE_USE? "USE" \
        : evtype == XLE_SHOOT? "SHOOT" \
        : evtype == XLE_HIT? "HIT" \
        : evtype == XLE_TICKER? "TICKER" \
        : evtype == XLE_AUTO? "AUTO" \
        : evtype == XLE_FORCED? "FORCED" \
        : evtype == XLE_FUNC? "FUNCTION" : "???")

#define LREFTYPESTR(reftype) (reftype == LREF_NONE? "NONE" \
        : reftype == LREF_SELF? "SELF" \
        : reftype == LREF_TAGGED? "TAGGED LINES" \
        : reftype == LREF_LINE_TAGGED? "LINE TAGGED LINES" \
        : reftype == LREF_ACT_TAGGED? "ACT TAGGED LINES" \
        : reftype == LREF_INDEX? "INDEXED LINE" \
        : reftype == LREF_ALL? "ALL LINES" : "???")

#define LPREFTYPESTR(reftype) (reftype == LPREF_NONE? "NONE" \
        : reftype == LPREF_MY_FLOOR? "MY FLOOR" \
        : reftype == LPREF_TAGGED_FLOORS? "TAGGED FLOORS" \
        : reftype == LPREF_LINE_TAGGED_FLOORS? "LINE TAGGED FLOORS" \
        : reftype == LPREF_ACT_TAGGED_FLOORS? "ACT TAGGED FLOORS" \
        : reftype == LPREF_INDEX_FLOOR? "INDEXED FLOOR" \
        : reftype == LPREF_ALL_FLOORS? "ALL FLOORS" \
        : reftype == LPREF_MY_CEILING? "MY CEILING" \
        : reftype == LPREF_TAGGED_CEILINGS? "TAGGED CEILINGS" \
        : reftype == LPREF_LINE_TAGGED_CEILINGS? "LINE TAGGED CEILINGS" \
        : reftype == LPREF_ACT_TAGGED_CEILINGS? "ACT TAGGED CEILINGS" \
        : reftype == LPREF_INDEX_CEILING? "INDEXED CEILING" \
        : reftype == LPREF_ALL_CEILINGS? "ALL CEILINGS" \
        : reftype == LPREF_SPECIAL? "SPECIAL" \
        : reftype == LPREF_BACK_FLOOR? "BACK FLOOR" \
        : reftype == LPREF_BACK_CEILING? "BACK CEILING" \
        : reftype == LPREF_THING_EXIST_FLOORS? "SECTORS WITH THING - FLOOR" \
        : reftype == LPREF_THING_EXIST_CEILINGS? "SECTORS WITH THING - CEILING" \
        : reftype == LPREF_THING_NOEXIST_FLOORS? "SECTORS WITHOUT THING - FLOOR" \
        : reftype == LPREF_THING_NOEXIST_CEILINGS? "SECTORS WITHOUT THING - CEILING" : "???")

#define LSREFTYPESTR(reftype) (reftype == LSREF_NONE? "NONE" \
        : reftype == LSREF_MY? "MY SECTOR" \
        : reftype == LSREF_TAGGED? "TAGGED SECTORS" \
        : reftype == LSREF_LINE_TAGGED? "LINE TAGGED SECTORS" \
        : reftype == LSREF_ACT_TAGGED? "ACT TAGGED SECTORS" \
        : reftype == LSREF_INDEX? "INDEXED SECTOR" \
        : reftype == LSREF_ALL? "ALL SECTORS" \
        : reftype == LSREF_BACK? "BACK SECTOR" \
        : reftype == LSREF_THING_EXIST? "SECTORS WITH THING" \
        : reftype == LSREF_THING_NOEXIST? "SECTORS WITHOUT THING" : "???")

#define GET_TXT(x)    ((*gi.text)[x].text)

#define TO_DMU_TOP_COLOR(x) (x == 0? DMU_TOP_COLOR_RED \
        : x == 1? DMU_TOP_COLOR_GREEN \
        : DMU_TOP_COLOR_BLUE)

#define TO_DMU_MIDDLE_COLOR(x) (x == 0? DMU_MIDDLE_COLOR_RED \
        : x == 1? DMU_MIDDLE_COLOR_GREEN \
        : x == 2? DMU_MIDDLE_COLOR_BLUE \
        : DMU_MIDDLE_COLOR_ALPHA)

#define TO_DMU_BOTTOM_COLOR(x) (x == 0? DMU_BOTTOM_COLOR_RED \
        : x == 1? DMU_BOTTOM_COLOR_GREEN \
        : DMU_BOTTOM_COLOR_BLUE)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------
boolean         G_ValidateMap(int *episode, int *map);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    XL_ChangeTexture(line_t *line, int sidenum, int section, int texture,
                         blendmode_t blend, byte rgba[4], int flags);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

int C_DECL XL_DoChainSequence();
int C_DECL XL_DoDamage();
int C_DECL XL_DoPower();
int C_DECL XL_DoKey();
int C_DECL XL_DoExplode();
int C_DECL XL_DoCommand();

int C_DECL XLTrav_ChangeLineType();
int C_DECL XLTrav_Activate();
int C_DECL XLTrav_Music();
int C_DECL XLTrav_LineCount();
int C_DECL XLTrav_EndLevel();
int C_DECL XLTrav_DisableLine();
int C_DECL XLTrav_EnableLine();
int C_DECL XLTrav_ChangeWallTexture();
int C_DECL XLTrav_LineTeleport();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int nextmap;
extern boolean xgdatalumps;

extern int PlayerColors[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

struct mobj_s dummything;
int     xgDev = 0;    // Print dev messages.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static linetype_t typebuffer;
static char msgbuf[80];

/* ADD NEW XG CLASSES TO THE END - ORIGINAL INDICES MUST STAY THE SAME!!! */
xgclass_t xgClasses[NUMXGCLASSES] =
{
    { NULL, NULL, TRAV_NONE, 0, TXT_XGCLASS000 },
      // Dummy class (has no functions but enables use of secondary actions) (no params)

    { XL_DoChainSequence, NULL, TRAV_NONE, 0, 1, 0, TXT_XGCLASS001,
      // Excute a chain of other XG line types (a zero ends the list)
       {{XGPF_INT, "Chain Flags", "chsf_", 0},              // ip0: (chsf_) chain sequence flags
        {XGPF_INT, "Line Type 0", "", -1},                  // ip1: Type to execute
        {XGPF_INT, "Line Type 1", "", -1},                  // ip2:  ""  ""  ""
        {XGPF_INT, "Line Type 2", "", -1},                  // ip3:  ""  ""  ""
        {XGPF_INT, "Line Type 3", "", -1},                  // ip4:  ""  ""  ""
        {XGPF_INT, "Line Type 4", "", -1},                  // ip5:  ""  ""  ""
        {XGPF_INT, "Line Type 5", "", -1},                  // ip6:  ""  ""  ""
        {XGPF_INT, "Line Type 6", "", -1},                  // ip7:  ""  ""  ""
        {XGPF_INT, "Line Type 7", "", -1},                  // ip8:  ""  ""  ""
        {XGPF_INT, "Line Type 8", "", -1},                  // ip9:  ""  ""  ""
        {XGPF_INT, "Line Type 9", "", -1},                  // ip10: ""  ""  ""
        {XGPF_INT, "Line Type 10", "", -1},                 // ip11: ""  ""  ""
        {XGPF_INT, "Line Type 11", "", -1},                 // ip12: ""  ""  ""
        {XGPF_INT, "Line Type 12", "", -1},                 // ip13: ""  ""  ""
        {XGPF_INT, "Line Type 13", "", -1},                 // ip14: ""  ""  ""
        {XGPF_INT, "Line Type 14", "", -1},                 // ip15: ""  ""  ""
        {XGPF_INT, "Line Type 15", "", -1},                 // ip16: ""  ""  ""
        {XGPF_INT, "Line Type 16", "", -1},                 // ip17: ""  ""  ""
        {XGPF_INT, "Line Type 17", "", -1},                 // ip18: ""  ""  ""
        {XGPF_INT, "Line Type 18", "", -1} }},              // ip19: ""  ""  ""

    { XSTrav_MovePlane, XS_InitMovePlane, TRAV_PLANES, 0, 1, 0, TXT_XGCLASS002,
      // Move one or more planes. Optionaly change textures/types on start/end
       {{XGPF_INT, "Target Ref", "lpref_", 0},              // ip0: (plane ref) plane(s) to move.
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Destination Ref", "spref_", 2},         // ip2: destination height type (zero, relative to current, surrounding highest/lowest floor/ceiling)
        {XGPF_INT, "Move Flags", "pmf_", 3},                // ip3: flags (PMF_*)
        {XGPF_INT, "Start Sound", "", 4 | MAP_SND},         // ip4: start sound
        {XGPF_INT, "End Sound", "", 5 | MAP_SND},           // ip5: end sound
        {XGPF_INT, "Move Sound", "", 6 | MAP_SND},          // ip6: move sound
        {XGPF_INT, "Start Texture Ref", "spref_", 7},       // ip7: start texture origin (uses same ids as i2) (spec: use ip8 as tex num)
        {XGPF_INT, "Start Texture Num", "", 8 | MAP_FLAT},  // ip8: data component or number/name of flat
        {XGPF_INT, "End Texture Ref", "spref_", 9},         // ip9: end texture origin (uses same ids as i2) (spec: use ip10 as tex num)
        {XGPF_INT, "End Texture Num", "", 10 | MAP_FLAT},   // ip10: data component or number/name of flat
        {XGPF_INT, "Start Type Ref", "lpref_", 11},         // ip11: (plane ref) start sector type (spec: use i12 as type ID)
        {XGPF_INT, "Start Type Num", "", -1},               // ip12: data component or type ID
        {XGPF_INT, "End Type Ref", "lpref_", 13},           // ip13: (plane ref) end sector type (spec: use i14 as type ID)
        {XGPF_INT, "End Type Num", "", -1} }},              // ip14: data component or type ID

    { XSTrav_BuildStairs, XS_InitStairBuilder, TRAV_PLANES, 0, 1, 0, TXT_XGCLASS003,
      // Moves one or more planes, incrementing their height with each move
       {{XGPF_INT, "Target Ref", "lpref_", 0},              // ip0: (plane ref) plane to start from
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Spread Texture", "", -1},               // ip2: (true/false) stop when texture changes
        {XGPF_INT, "Spread Build", "", -1},                 // ip3: (true/false) spread build?
        {XGPF_INT, "Start Sound", "", 4 | MAP_SND},         // ip4: start build sound (doesn't wait)
        {XGPF_INT, "Step Start Sound", "", 5 | MAP_SND},    // ip5: step start sound
        {XGPF_INT, "Step End Sound", "", 6 | MAP_SND},      // ip6: step end sound
        {XGPF_INT, "Step Move Sound", "", 7 | MAP_SND} }},  // ip7: step move sound

    { XL_DoDamage, NULL, TRAV_NONE, 0, 1, 0, TXT_XGCLASS004,
      // Deals health damage to the activator
       {{XGPF_INT, "Min Delta", "", -1},                    // ip0: min damage delta
        {XGPF_INT, "Max Delta", "", -1},                    // ip1: max damage delta
        {XGPF_INT, "Min Limit", "", -1},                    // ip2: min limit (wont damage if health bellow)
        {XGPF_INT, "Max Limit", "", -1} }},                 // ip3: max limit (wont damage if health above)

    { XL_DoPower, NULL, TRAV_NONE, 0, 1, 0, TXT_XGCLASS005,
      // Deals armor damage to the activator (must be a player)
       {{XGPF_INT, "Min Delta", "", -1},                    // ip0: min power delta
        {XGPF_INT, "Max Delta", "", -1},                    // ip1: max power delta
        {XGPF_INT, "Min Limit", "", -1},                    // ip2: min limit
        {XGPF_INT, "Max Limit", "", -1} }},                 // ip3: max limit

    { XLTrav_ChangeLineType, NULL, TRAV_LINES, 0, 1, 0, TXT_XGCLASS006,
      // Changes a line's type (must be an XG type)
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Line Type", "", -1} }},                 // ip2: new type (must be an XG line type)

    { XSTrav_SectorType, NULL, TRAV_SECTORS, 0, 1, 0, TXT_XGCLASS007,
      // Changes a sector's type (must be an XG type)
       {{XGPF_INT, "Target Ref", "lsref_", 0},              // ip0: (sector ref) sector(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Sector Type", "", -1} }},               // ip2: new type (zero or an XG sector type)

    { XSTrav_SectorLight, NULL, TRAV_SECTORS, 0, 1, 0, TXT_XGCLASS008,
      // Change the light level and/or color  of the target sector(s).
       {{XGPF_INT, "Target Ref", "lsref_", 0},              // ip0: (sector ref) sector(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Change Light", "", -1},                 // ip2: if non-zero light level will be changed
        {XGPF_INT, "Change Color", "", -1},                 // ip3: if non-zero colour will be changed
        {XGPF_INT, "Light Ref", "lightref_", 4},            // ip4: (light ref) sector to get the initial light delta from.
                                                            //      lightref_none makes ip5 an absolute value
        {XGPF_INT, "Light Delta", "", -1},                  // ip5: offset to the delta or absolute value
        {XGPF_INT, "Color Ref", "lightref_", 6},            // ip6: (light ref) sector to get the initial colour deltas from.
                                                            //      lightref_none makes ip7-9 absolute values
        {XGPF_INT, "Red Delta", "", -1},                    // ip7: offset to red delta
        {XGPF_INT, "Green Delta", "", -1},                  // ip8: offset to green delta
        {XGPF_INT, "Blue Delta", "", -1} }},                // ip9: offset to blue delta

    { XLTrav_Activate, NULL, TRAV_LINES, 0, 1, 0, TXT_XGCLASS009,
      // Sends a chain event to all the referenced lines
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to activate
        {XGPF_INT, "Target Num", "", -1} }},                // ip1:

    { XL_DoKey, NULL, TRAV_NONE, 0, 1, 0, TXT_XGCLASS010,
      // Gives/takes keys to/from the activator (must be a player)
      // Params are bitfields! Bit 1 (0x1) corresponds key 1, bit 2 (0x2) key 2, etc.
       {{XGPF_INT, "Give Keys", "", -1},                    // ip0: keys to give
        {XGPF_INT, "Take Keys", "", -1} }},                 // ip1: keys to take away.

    { XLTrav_Music, NULL, TRAV_LINES, 2, 3, 0, TXT_XGCLASS011,
      // Changes the music track being played
       {{XGPF_INT, "Song ID", "ldref_", 0 | MAP_MUS},       // ip0: song id/name or (line data ref from ip2)
        {XGPF_INT, "Play Looped", "", -1},                  // ip1: non-zero means play looped
        {XGPF_INT, "Data Ref", "lref_", 2},                 // ip2: (line ref) used with line data ref eg set music track to line-tag
        {XGPF_INT, "Data Num", "", -1} }},                  // ip3:

    { XLTrav_LineCount, NULL, TRAV_LINES, 0, 1, 0, TXT_XGCLASS012,
      // Changes the XG line(s)' internal activation counter
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Set Absolute", "", -1},                 // ip2: non-zero makes ip3 absolute
        {XGPF_INT, "Count Delta", "", -1} }},               // ip3: count delta or absolute

    { XLTrav_EndLevel, NULL, TRAV_LINES, 1, 2, 0, TXT_XGCLASS013,
      // Ends the current level
       {{XGPF_INT, "Secret Exit", "", -1},                  // ip0: non-zero goto secret level
        {XGPF_INT, "Data Ref", "lref_", 1},                 // ip1: (line ref) line to acquire (line data ref) from
        {XGPF_INT, "Data Num", "", -1},                     // ip2:
        {XGPF_INT, "Goto Level", "ldref_", 3} }},           // ip3: level ID or (line data ref from ip1)

    { XLTrav_DisableLine, NULL, TRAV_LINES, 0, 1, 0, TXT_XGCLASS014,
      // Disables the referenced line(s) if active
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to disable
        {XGPF_INT, "Target Num", "", -1} }},                // ip1:

    { XLTrav_EnableLine, NULL, TRAV_LINES, 0, 1, 0, TXT_XGCLASS015,
      // Enables the referenced line(s) if active
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to enable
        {XGPF_INT, "Target Num", "", -1} }},                // ip1:

    { XL_DoExplode, NULL, TRAV_NONE, 0, 1, 0, TXT_XGCLASS016 },
      // Explodes the activator (no params)

    { XSTrav_PlaneTexture, NULL, TRAV_PLANES, 0, 1, 0, TXT_XGCLASS017,
      // Change the texture and/or surface color of a plane
       {{XGPF_INT, "Target Ref", "lpref_", 0},              // ip0 : (plane ref) plane(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1 : ref data
        {XGPF_INT, "Texture Ref", "spref_", 2},             // ip2 : Texture ref
        {XGPF_INT, "Texture Num", "", 3 | MAP_FLAT},        // ip3 : texture number (flat), used with SPREF_NONE
        {XGPF_INT, "Red Delta", "", -1},                    // ip4 : plane surface color (red)
        {XGPF_INT, "Green Delta", "", -1},                  // ip5 : "" (green)
        {XGPF_INT, "Blue Delta", "", -1} }},                // ip6 : "" (blue)

    { XLTrav_ChangeWallTexture, NULL, TRAV_LINES, 0, 1, 0, TXT_XGCLASS018,
      // Changes texture(s) on the referenced line(s).
      // Changes surface colour(s), alpha, mid textue blendmode and sidedef flags
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Side Num", "", -1},                     // ip2: non-zero change the back side
        {XGPF_INT, "Top Texture", "", 3 | MAP_TEX},         // ip3: top texture to change to (blank indicates no change)
        {XGPF_INT, "Middle Texture", "", 4 | MAP_TEX},      // ip4: middle texture to change to (blank indicates no change)
        {XGPF_INT, "Bottom Texture", "", 5 | MAP_TEX},      // ip5: bottom texture to change to (blank indicates no change)
        {XGPF_INT, "Set Mid If None", "", -1},              // ip6: set mid texture even if previously zero
        {XGPF_INT, "Sidedef Flags", "sdf_", 7},             // ip7: (sdf_) sidedef flags (used with surface colour blending, fullbright etc)
        {XGPF_INT, "Middle Blendmode", "bm_", 8},           // ip8: (bm_) middle texture blendmode
        {XGPF_INT, "Top Red Delta", "", -1},                // ip9:
        {XGPF_INT, "Top Green Delta", "", -1},              // ip10:
        {XGPF_INT, "Top Blue Delta", "", -1},               // ip11:
        {XGPF_INT, "Middle Red Delta", "", -1},             // ip12:
        {XGPF_INT, "Middle Green Delta", "", -1},           // ip13:
        {XGPF_INT, "Middle Blue Delta", "", -1},            // ip14:
        {XGPF_INT, "Middle Alpha Delta", "", -1},           // ip15:
        {XGPF_INT, "Bottom Red Delta", "", -1},             // ip16:
        {XGPF_INT, "Bottom Green Delta", "", -1},           // ip17:
        {XGPF_INT, "Bottom Blue Delta", "", -1} }},         // ip18:

    { XL_DoCommand, NULL, TRAV_NONE, 0, 1, 0, TXT_XGCLASS019 },
      // Executes a console command (CCmd)

    { XSTrav_SectorSound, NULL, TRAV_SECTORS, 0, 1, 0, TXT_XGCLASS020,
      // Plays a sound in sector(s)
       {{XGPF_INT, "Target Ref", "lsref_", 0},              // ip0: (sector ref) sector(s) to play the sound in
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Sound ID", "", 2 | MAP_SND} }},         // ip2: sound name/id to play

    { XSTrav_MimicSector, NULL, TRAV_SECTORS, 0, 1, 0, TXT_XGCLASS021,
      // Copies all properties from target sector to destination sector(s)
       {{XGPF_INT, "Target Ref", "lsref_", 0},              // ip0: (sector ref) sector(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Mimic Ref", "spref_", 2},               // ip2: (spref) sector to mimic
        {XGPF_INT, "Mimic Num", "", -1} }},                 // ip3:

    { XSTrav_Teleport, NULL, TRAV_SECTORS, 0, 1, 0, TXT_XGCLASS022,
      // Teleports the activator to the first teleport exit in the target sector
       {{XGPF_INT, "Target Ref", "lsref_", 0},              // ip0: (sector ref) sector(s) to teleport to (first acceptable target is used)
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "No Flash", "", -1},                     // ip2: non-zero = no flash (or sound)
        {XGPF_INT, "No Sound", "", -1},                     // ip3: non-zero = no sound
        {XGPF_INT, "Always Stomp", "", -1} }},              // ip4: non-zero = Always telefrag

    { XLTrav_LineTeleport, NULL, TRAV_LINES, 0, 1, 1 | XLE_CROSS, TXT_XGCLASS023,
      // Teleports the activator to the referenced line
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) teleport destination
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "No Flash", "", -1},                     // ip2: non-zero = spawn MT_TFOG
        {XGPF_INT, "Teleport Sound", "", 3 | MAP_SND},      // ip3: sound ID/name to play (or silent)
        {XGPF_INT, "Exit Side", "", -1},                    // ip4: non-zero = exit from the back of the target line
        {XGPF_INT, "Always Stomp", "", -1} }}               // ip5: non-zero = Always telefrag
};

// CODE --------------------------------------------------------------------

cvar_t xgCVars[] =
{
    {"xg-dev", 0, CVT_INT, &xgDev, 0, 1, "1=Print XG debug messages."},
    {NULL}
};

ccmd_t  xgCCmds[] =
{
    {"movefloor",  CCmdMovePlane,     "Move a sector's floor plane.", 0 },
    {"moveceil",   CCmdMovePlane,     "Move a sector's ceiling plane.", 0 },
    {"movesec",    CCmdMovePlane,     "Move a sector's both planes.", 0 },
    {NULL}
};

/*
 * Register XG CCmds & CVars (called during pre init)
 */
void XG_Register(void)
{
    int     i;

    for(i = 0; xgCVars[i].name; i++)
        Con_AddVariable(xgCVars + i);
    for(i = 0; xgCCmds[i].name; i++)
        Con_AddCommand(xgCCmds + i);
}

/*
 * Debug message printer.
 */
void XG_Dev(const char *format, ...)
{
    static char buffer[2000];
    va_list args;

    if(!xgDev)
        return;
    va_start(args, format);
    vsprintf(buffer, format, args);
    strcat(buffer, "\n");
    Con_Message(buffer);
    va_end(args);
}

// Init XG data for the level.
void XG_Init(void)
{
    XL_Init();    // Init lines.
    XS_Init();    // Init sectors.
}

void XG_Ticker(void)
{
    XS_Ticker();    // Think for sectors.

    // Clients rely on the server, they don't do XG themselves.
    if(IS_CLIENT)
        return;

    XL_Ticker();    // Think for lines.
}

/*
 * This is called during an engine reset. Disables all XG functionality!
 */
void XG_Update(void)
{
    // Clients rely on the server, they don't do XG themselves.
    if(IS_CLIENT)
        return;

    XG_ReadTypes();
    XS_Update();
    XL_Update();
}

/*
 * Adds the given binary format line type to the generated types array
 */
int XL_AddAutoGenType(linetype_t *newtype)
{
    return true;
}

/*
 * Converts a line ID number to a line type
 * (BOOM support)
 */
int XL_AutoGenType(int id, linetype_t *outptr)
{
    linetype_t *l;

    return false; // Cos we don't work yet

    // Do the magic
    // is the ID in the range 12160 -> 32768
    if(!(id >= 12160 && id <= 32768))
        return false;

    // Have we already generated a line of this type?
    if(1)
    {
        // Then return a ptr to it
        outptr = l;
        return true;
    }

    // Which bitfield format are we generating from?
    if(1) //XG format
    {
        /*
         // Generate the new line type
        l->id = id;
        l->flags = def->flags[0];
        l->flags2 = def->flags[1];
        l->flags3 = def->flags[2];
        l->line_class = def->line_class;
        l->act_type = def->act_type;
        l->act_count = def->act_count;
        l->act_time = def->act_time;
     //   l->act_tag = def->act_tag;
        for(i = 0; i < 10; i++)
        {
            if(i == 9)
                l->aparm[i] = Def_GetMobjNum(def->aparm9);
            else
                l->aparm[i] = def->aparm[i];
        }
     //   l->ticker_start = def->ticker_start;
     //   l->ticker_end = def->ticker_end;
     //   l->ticker_interval = def->ticker_interval;
        l->act_sound = Friendly(Def_GetSoundNum(def->act_sound));
        l->deact_sound = Friendly(Def_GetSoundNum(def->deact_sound));
        l->ev_chain = def->ev_chain;
        l->act_chain = def->act_chain;
        l->deact_chain = def->deact_chain;
        l->act_linetype = def->act_linetype;
        l->deact_linetype = def->deact_linetype;
        l->wallsection = def->wallsection;
        l->act_tex = Friendly(R_CheckTextureNumForName(def->act_tex));
        l->deact_tex = Friendly(R_CheckTextureNumForName(def->deact_tex));
        l->act_msg = def->act_msg;
        l->deact_msg = def->deact_msg;
        l->texmove_angle = def->texmove_angle;
        l->texmove_speed = def->texmove_speed;*/

        // Add it to the generated line types array
        XL_AddAutoGenType(l);
        outptr = l;
    }
#if __JDOOM__
    else // BOOM format
    {
        // Generate the new line type
        outptr = l;
    }
#endif

    return true;
 }

/*
 * Returns true if the type is defined.
 */
linetype_t *XL_GetType(int id)
{
    linetype_t *ptr;

    // Try finding it from the DDXGDATA lump.
    ptr = XG_GetLumpLine(id);
    if(ptr)
    {
        // Got it!
        memcpy(&typebuffer, ptr, sizeof(*ptr));
        return &typebuffer;
    }
    // Does Doomsday have a definition for this?
    if(Def_Get(DD_DEF_LINE_TYPE, (char *) id, &typebuffer))
        return &typebuffer;

    // Is this a type we can generate automatically?
    if(XL_AutoGenType(id, &typebuffer))
        return &typebuffer;

    // A definition was not found.
    return NULL;
}

int XG_RandomInt(int min, int max)
{
    float   x;

    if(max == min)
        return max;
    x = M_Random() / 256.0f;    // Never reaches 1.
    return (int) (min + x * (max - min) + x);
}

float XG_RandomPercentFloat(float value, int percent)
{
    float   i = (2 * M_Random() / 255.0f - 1) * percent / 100.0f;

    return value * (1 + i);
}

/*
 * Looks for line type definition and sets the
 * line type if one is found.
 */
void XL_SetLineType(line_t *line, int id)
{
    int lineid = P_ToIndex(DMU_LINE, line);
    xline_t *xline;

    xline = &xlines[lineid];

    if(XL_GetType(id))
    {
        xline->special = id;
        // Allocate memory for the line type data.
        if(!xline->xg)
            xline->xg = Z_Malloc(sizeof(xgline_t), PU_LEVEL, 0);
        // Init the extended line state.
        xline->xg->disabled = false;
        xline->xg->timer = 0;
        xline->xg->ticker_timer = 0;
        memcpy(&xline->xg->info, &typebuffer, sizeof(linetype_t));
        // Initial active state.
        xline->xg->active = (typebuffer.flags & LTF_ACTIVE) != 0;
        xline->xg->activator = &dummything;

        XG_Dev("XL_SetLineType: Line %i (%s), ID %i.", lineid,
               GET_TXT(xgClasses[xline->xg->info.line_class].className), id);

    }
    else if(id)
    {
        XG_Dev("XL_SetLineType: Line %i, type %i NOT DEFINED.", lineid, id);
    }
}

/*
 * Initialize extended lines for the map.
 */
void XL_Init(void)
{
    int    i;

    memset(&dummything, 0, sizeof(dummything));

    // Clients rely on the server, they don't do XG themselves.
    if(IS_CLIENT)
        return;

    for(i = 0; i < numlines; i++)
    {
        xlines[i].xg = 0;
        XL_SetLineType(P_ToPtr(DMU_LINE, i), xlines[i].special);
    }
}

/*
 * Executes the specified function on all planes that match
 * the reference (reftype). If a function fails it returns false.
 */
int XL_TraversePlanes(line_t *line, int reftype, int ref, void *data,
                      void *context, boolean travsectors, mobj_t *activator,
                      int (C_DECL *func)())
{
    int     i;
    int     lineid = P_ToIndex(DMU_LINE, line);
    mobj_t *mo;
    boolean    ok;
    sector_t *frontsector, *backsector;
    char    buff[50];

    if(ref)
        sprintf(buff,": %i",ref);

    XG_Dev("XL_Traverse%s: Line %i, ref (%s%s)",
           travsectors? "Sectors":"Planes", lineid,
           travsectors? LSREFTYPESTR(reftype) : LPREFTYPESTR(reftype), ref? buff: "");

    if(reftype == LPREF_NONE)
        return false;        // This is not a reference!

    frontsector = P_GetPtrp(DMU_LINE, line, DMU_FRONT_SECTOR);
    backsector = P_GetPtrp(DMU_LINE, line, DMU_BACK_SECTOR);

    // References to a single plane
    if(reftype == LPREF_MY_FLOOR)
        return func(frontsector, false, data, context, activator);
    if(reftype == LPREF_BACK_FLOOR)
        if(backsector)
            return func(backsector, false, data, context, activator);
        else
            XG_Dev("  Line %i has no back sector!", lineid);
    if(reftype == LPREF_MY_CEILING)
        return func(frontsector, true, data, context, activator);
    if(reftype == LPREF_BACK_CEILING)
        if(backsector)
            return func(backsector, true, data, context, activator);
        else
            XG_Dev("  Line %i has no back sector!", lineid);
    if(reftype == LPREF_INDEX_FLOOR)
        return func(P_ToPtr(DMU_SECTOR, ref), false, data, context, activator);
    if(reftype == LPREF_INDEX_CEILING)
        return func(P_ToPtr(DMU_SECTOR, ref), true, data, context, activator);

    // References to multiple planes
    for(i = 0; i < numsectors; i++)
    {
        if(reftype == LPREF_ALL_FLOORS || reftype == LPREF_ALL_CEILINGS)
        {
            if(!func
               (P_ToPtr(DMU_SECTOR, i), reftype == LPREF_ALL_CEILINGS, data,
                context, activator))
                return false;
        }
        if((reftype == LPREF_TAGGED_FLOORS || reftype == LPREF_TAGGED_CEILINGS)
           && xsectors[i].tag == ref)
        {
            if(!func
               (P_ToPtr(DMU_SECTOR, i), reftype == LPREF_TAGGED_CEILINGS, data,
                context, activator))
                return false;
        }
        if((reftype == LPREF_LINE_TAGGED_FLOORS ||
            reftype == LPREF_LINE_TAGGED_CEILINGS) &&
            xsectors[i].tag == xlines[lineid].tag)
        {
            if(!func
               (P_ToPtr(DMU_SECTOR, i), reftype == LPREF_LINE_TAGGED_CEILINGS, data,
                context, activator))
                return false;
        }
        if((reftype == LPREF_ACT_TAGGED_FLOORS ||
            reftype == LPREF_ACT_TAGGED_CEILINGS) && xsectors[i].xg &&
            xsectors[i].xg->info.act_tag == ref)
        {
            if(!func
               (P_ToPtr(DMU_SECTOR, i), reftype == LPREF_ACT_TAGGED_CEILINGS, data,
               context, activator))
               return false;
        }
        // Reference all sectors with (at least) one mobj of specified type inside
        if(reftype == LPREF_THING_EXIST_FLOORS ||
            reftype == LPREF_THING_EXIST_CEILINGS)
        {
            ok = true;

            for(mo = P_GetPtr(DMU_SECTOR, i, DMU_THINGS); ok, mo; mo = mo->snext)
            {
                if(mo->type == xlines[lineid].xg->info.aparm[9])
                {
                    XG_Dev("  Thing of type %i found in sector id %i.",
                           xlines[lineid].xg->info.aparm[9], i);

                    if(!func
                       (P_ToPtr(DMU_SECTOR, i), reftype == LPREF_THING_EXIST_CEILINGS,
                        data, context, activator))
                        return false;

                    ok = false;
                }
            }
        }
        // Reference all sectors with NONE of the specified mobj type inside
        if(reftype == LPREF_THING_NOEXIST_FLOORS ||
            reftype == LPREF_THING_NOEXIST_CEILINGS)
        {
            ok = true;

            for(mo = P_GetPtr(DMU_SECTOR, i, DMU_THINGS); ok, mo; mo = mo->snext)
            {
                if(mo->type != xlines[lineid].xg->info.aparm[9])
                    continue;
                else
                    ok = false;
            }

            if(ok)
            {
                XG_Dev("  No things of type %i found in sector id %i.",
                           xlines[lineid].xg->info.aparm[9], i);

                if(!func
                   (P_ToPtr(DMU_SECTOR, i), reftype == LPREF_THING_NOEXIST_CEILINGS,
                    data, context, activator))
                    return false;
            }
        }
    }
    return true;
}

/*
 * Executes the specified function on all lines that match
 * the reference (reftype). Returns false if func returns false,
 * otherwise true. Stops checking when false is returned.
 */
int XL_TraverseLines(line_t *line, int rtype, int ref, void *data,
                     void *context, mobj_t * activator, int (C_DECL *func)())
{
    int    i;
    int    reftype = rtype;
    int    lineid = P_ToIndex(DMU_LINE, line);
    char   buff[50];

    // Binary XG data from DD_XGDATA uses the old flag values.
    // Add one to the ref type.
    if(xgdatalumps)
        reftype+= 1;

    if(ref)
        sprintf(buff," : %i",ref);

    XG_Dev("XL_TraverseLines: Line %i, ref (%s%s)",
           lineid, LREFTYPESTR(reftype), ref? buff : "");

    if(reftype == LREF_NONE)
        return func(NULL, true, data, context, activator);  // Not a real reference

    // References to single lines
    if(reftype == LREF_SELF) // Traversing self is simple.
        return func(line, true, data, context, activator);
    if(reftype == LREF_INDEX)
        return func(P_ToPtr(DMU_LINE, ref), true, data, context, activator);

    // References to multiple lines
    for(i = 0; i < numlines; i++)
    {
        if(reftype == LREF_ALL)
        {
            if(!func(P_ToPtr(DMU_LINE, i), true, data, context, activator))
                return false;
        }
        if(reftype == LREF_TAGGED)
        {
            if(xlines[i].tag == ref)
                if(!func(P_ToPtr(DMU_LINE, i), true, data, context, activator))
                    return false;
        }
        if(reftype == LREF_LINE_TAGGED)
        {
            // Ref is true if line itself should be excluded.
            if(xlines[i].tag == xlines[lineid].tag && (!ref || P_ToPtr(DMU_LINE, i) != line))
                if(!func(P_ToPtr(DMU_LINE, i), true, data, context, activator))
                    return false;
        }
        if(reftype == LREF_ACT_TAGGED)
        {
            if(xlines[i].xg && xlines[i].xg->info.act_tag == ref)
                if(!func(P_ToPtr(DMU_LINE, i), true, data, context, activator))
                    return false;
        }
    }
    return true;
}

/*
 * Returns the value requested by reftype from the line using data from
 * either line or context (will always be linetype_t).
 */
int XL_ValidateLineRef(line_t *line, int reftype, void *context, char *parmname)
{
    int answer = 0;
    side_t *side;

    switch(reftype)
    {
    case LDREF_ID:    // Line ID
        answer = P_ToIndex(DMU_LINE, line);
        XG_Dev("XL_ValidateLineRef: Using Line ID (%i) as %s", answer, parmname);
        break;

    case LDREF_SPECIAL:    // Line Special
        answer = P_XLine(line)->special;
        XG_Dev("XL_ValidateLineRef: Using Line Special (%i) as %s", answer, parmname);
        break;

    case LDREF_TAG:    // line Tag
        answer = P_XLine(line)->tag;
        XG_Dev("XL_ValidateLineRef: Using Line Tag (%i) as %s", answer, parmname);
        break;

    case LDREF_ACTTAG:    // line ActTag
        if(!P_XLine(line)->xg)
        {
            XG_Dev("XL_ValidateLineRef: REFERENCE NOT AN XG LINE");
            break;
        }

        if(!P_XLine(line)->xg->info.act_tag)
        {
            XG_Dev("XL_ValidateLineRef: REFERENCE DOESNT HAVE AN ACT TAG");
            break;
        }

        answer = P_XLine(line)->xg->info.act_tag;
        XG_Dev("XL_ValidateLineRef: Using Line ActTag (%i) as %s", answer, parmname);
        break;

    case LDREF_COUNT:    // line count
        if(!P_XLine(line)->xg)
        {
            XG_Dev("XL_ValidateLineRef: REFERENCE NOT AN XG LINE");
            break;
        }

        answer = P_XLine(line)->xg->info.act_count;
        XG_Dev("XL_ValidateLineRef: Using Line Count (%i) as %s", answer, parmname);
        break;

    case LDREF_ANGLE:    // line angle
        answer = (R_PointToAngle2(0, 0, P_GetFixedp(DMU_LINE, line, DMU_DX),
                                  P_GetFixedp(DMU_LINE, line, DMU_DY))
                 / (float) ANGLE_MAX *360);
        XG_Dev("XL_ValidateLineRef: Using Line Angle (%i) as %s", answer, parmname);
        break;

    case LDREF_LENGTH:    // line length
        // Answer should be in map units.
        answer = P_GetFixedp(DMU_LINE, line, DMU_LENGTH) >> FRACBITS;

        XG_Dev("XL_ValidateLineRef: Using Line Length (%i) as %s", answer, parmname);
        break;

    case LDREF_OFFSETX:    // x offset
        // Can this ever fail?
        side = P_GetPtrp(DMU_LINE, line, DMU_SIDE0);
        if(!side)
        {
            XG_Dev("XL_ValidateLineRef: REFERENCE MISSING FRONT SIDEDEF!");
            break;
        }

        answer = P_GetIntp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_X);

        XG_Dev("XL_ValidateLineRef: Using Line X Offset (%i) as %s", answer, parmname);
        break;

    case LDREF_OFFSETY:    // y offset
        // Can this ever fail?
        side = P_GetPtrp(DMU_LINE, line, DMU_SIDE0);
        if(!side)
        {
            XG_Dev("XL_ValidateLineRef: REFERENCE MISSING FRONT SIDEDEF!");
            break;
        }

        answer = P_GetIntp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_Y);

        XG_Dev("XL_ValidateLineRef: Using Line Y Offset (%i) as %s", answer, parmname);
        break;

    default:    // Could be explicit, return the actual int value
        answer = reftype;
        break;
    }

    return answer;
}

/*
 * Executes the lines' function as defined by its class type
 */
void XL_DoFunction(linetype_t * info, line_t *line, int sidenum,
                          mobj_t *act_thing, int evtype)
{
    xgclass_t *xgClass = xgClasses + info->line_class;

    XG_Dev("XL_DoFunction: Line %i, side %i, activator id %i, event %s",
            P_ToIndex(DMU_LINE, line),sidenum, act_thing ? act_thing->thinker.id : 0,
            EVTYPESTR(evtype));
    XG_Dev("  Executing class: %s (0x%X)...",GET_TXT(xgClass->className), info->line_class);

    // Does this class only work with certain events?
    if(xgClass->evtypeflags > 0)
    {
        if(!(xgClass->evtypeflags & evtype))
        {
            XG_Dev("  THIS CLASS DOES NOT SUPPORT %s EVENTS!", EVTYPESTR(evtype));
            return;
        }
    }

    // Does this class have an init function?
    if(xgClass->initFunc)
        xgClass->initFunc(line);

    // Does this class have a do function?
    if(xgClass->doFunc)
    {
        // Do we need to traverse?
        switch(xgClass->traverse)
        {
            case TRAV_NONE: // No need for traversal, call the doFunc
                xgClass->doFunc(line, true, (int) line, info, act_thing);
                break;
            case TRAV_LINES: // Traverse lines, executing doFunc for each
                XL_TraverseLines(line, info->iparm[xgClass->travref],
                                 info->iparm[xgClass->travdata], line,
                                 info, act_thing, xgClass->doFunc);
                break;
            case TRAV_PLANES: // Traverse planes, executing doFunc for each
            case TRAV_SECTORS:
                XL_TraversePlanes(line, info->iparm[xgClass->travref],
                                  info->iparm[xgClass->travdata], line,
                                  info, xgClass->traverse == TRAV_SECTORS? true : false,
                                  act_thing, xgClass->doFunc);
                break;
        }
    }
}

int C_DECL XLTrav_QuickActivate(line_t *line, boolean dummy, void *context,
                                void *context2, mobj_t *activator)
{
    xline_t *xline = &xlines[P_ToIndex(DMU_LINE, line)];

    if(!xline->xg)
        return true;
    xline->xg->active = (int) context;
    xline->xg->timer = XLTIMER_STOPPED;    // Stop timer.
    return true;
}

/*
 * Returns true if the line is active.
 */
int C_DECL XLTrav_CheckLine(line_t *line, boolean dummy, void *context,
                            void *context2, mobj_t *activator)
{
    xline_t *xline = &xlines[P_ToIndex(DMU_LINE, line)];

    if(!xline->xg)
        return false;    // Stop checking!
    return (xline->xg->active != 0) == ((int) context != 0);
}

/*
 * &param data  If true, the line will receive a chain event if not activate.
 *      If data=false, then ... if active.
 */
int C_DECL XLTrav_SmartActivate(line_t *line, boolean dummy, void *context, void *context2,
                         mobj_t *activator)
{
    xline_t *xline = &xlines[P_ToIndex(DMU_LINE, line)];

    if(!xline->xg)
        return false;

    if((int)context != xline->xg->active)
        XL_LineEvent(XLE_CHAIN, 0, line, 0, context2);
    return true;
}


//
// XG Line Type Classes which don't require traversal
//

int C_DECL XL_DoChainSequence(line_t *line, boolean dummy, void *context,
                              void *context2, mobj_t *activator)
{
    xline_t *xline = &xlines[P_ToIndex(DMU_LINE, line)];
    linetype_t *info = context2;

    if(!xline->xg)
        return false;

    xline->xg->chidx = 1;        // This is the first.
    // Start counting the first interval.
    xline->xg->chtimer = XG_RandomPercentFloat(info->fparm[1], info->fparm[0]);
    return true;
}

int C_DECL XL_DoDamage(line_t *line, boolean dummy, void *context,
                       void *context2, mobj_t *activator)
{
    int i;
    linetype_t *info = context2;

    if(!activator)
    {
/*        if(evtype == XLE_FUNC)
            XG_Dev("  Sector Functions don't have activators! Use a Sector Chain instead.");
        else*/
            XG_Dev("  No activator! Can't damage anything.");

        return false;
    }

    if(activator->health > info->iparm[2])
    {
        // Iparms define the min and max damage to inflict.
        // The real amount is random.
        i = XG_RandomInt(info->iparm[0], info->iparm[1]);
        if(i > 0)
            P_DamageMobj(activator, 0, 0, i);
        else if(i < 0)
        {
            activator->health -= i;
            // Don't go above a given level.
            if(activator->health > info->iparm[3])
                activator->health = info->iparm[3];
            if(activator->player)
            {
                // This is player.
                activator->player->health = activator->health;
                activator->player->update |= PSF_HEALTH;
            }
        }
    }
    return true;
}

int C_DECL XL_DoPower(line_t *line, boolean dummy, void *context,
                      void *context2, mobj_t *activator)
{
    player_t *player;
    linetype_t *info = context2;

    if(activator)
        player = activator->player;

    if(!player)  // Must be a player.
    {
/*        if(evtype == XLE_FUNC)
            XG_Dev("  Sector Functions don't have activators! Use a Sector Chain instead.");
        else*/
            XG_Dev("  Activator MUST be a player...");

        return false;
    }
    player->armorpoints += XG_RandomInt(info->iparm[0], info->iparm[1]);
    if(player->armorpoints < info->iparm[2])
        player->armorpoints = info->iparm[2];
    if(player->armorpoints > info->iparm[3])
        player->armorpoints = info->iparm[3];

    return true;
}

int C_DECL XL_DoKey(line_t *line, boolean dummy, void *context,
                    void *context2, mobj_t *activator)
{
    int i;
    player_t *player;
    linetype_t *info = context2;

    if(activator)
        player = activator->player;

    if(!player)  // Must be a player.
    {
/*        if(evtype == XLE_FUNC)
            XG_Dev("  Sector Functions don't have activators! Use a Sector Chain instead.");
        else*/
            XG_Dev("  Activator MUST be a player...");

        return false;
    }
    for(i = 0; i < NUMKEYS; i++)
    {
        if(info->iparm[0] & (1 << i))
            P_GiveKey(player, i);
        if(info->iparm[1] & (1 << i))
            player->keys[i] = false;
    }
    return true;
}

int C_DECL XL_DoExplode(line_t *line, boolean dummy, void *context,
                        void *context2, mobj_t *activator)
{
    if(!activator)
    {
/*        if(evtype == XLE_FUNC)
            XG_Dev("  Sector Functions don't have activators! Use a Sector Chain instead.");
        else*/
            XG_Dev("  No activator! Can't explode anything.");
        return false;
    }
    P_ExplodeMissile(activator);
    return true;
}

int C_DECL XL_DoCommand(line_t *line, boolean dummy, void *context,
                        void *context2, mobj_t *activator)
{
    linetype_t *info = context2;

    DD_Execute(info->sparm[0], true);
    return true;
}

//
// Following classes require traversal hence "Trav_"
//

int C_DECL XLTrav_ChangeLineType(line_t *line, boolean dummy, void *context,
                          void *context2, mobj_t *activator)
{
    linetype_t *info = context2;

    XL_SetLineType(line, info->iparm[2]);
    return true;    // Keep looking.
}

int C_DECL XLTrav_ChangeWallTexture(line_t *line, boolean dummy, void *context,
                             void *context2, mobj_t *activator)
{
    linetype_t *info = context2;
    side_t *side;
    blendmode_t blend = BM_NORMAL;
    byte rgba[4];
    int texture = 0;

    // i2: sidenum
    // i3: top texture (zero if no change)
    // i4: mid texture (zero if no change, -1 to remove)
    // i5: bottom texture (zero if no change)
    // i6: (true/false) set midtexture even if previously zero
    // i7: sdf_* flags
    // i8: mid texture blendmode
    // i9: top texture red
    // i10: top texture green
    // i11: top texture blue
    // i12: mid texture red
    // i13: mid texture green
    // i14: mid texture blue
    // i15: mid texture alpha
    // i16: bottom texture red
    // i17: bottom texture green
    // i18: bottom texture blue

    // Is there a sidedef?
    if(info->iparm[2])
    {
        if(P_GetPtrp(DMU_LINE, line, DMU_BACK_SECTOR) < 0)
            return true;

        side = P_GetPtrp(DMU_LINE, line, DMU_BACK_SECTOR);
    }
    else
    {
        if(P_GetPtrp(DMU_LINE, line, DMU_FRONT_SECTOR) < 0)
            return true;

        side = P_GetPtrp(DMU_LINE, line, DMU_FRONT_SECTOR);
    }

    XG_Dev("XLTrav_ChangeWallTexture: Line %i", P_ToIndex(DMU_LINE, line));

    rgba[0] = info->iparm[9];
    rgba[1] = info->iparm[10];
    rgba[2] = info->iparm[11];
    XL_ChangeTexture(line, info->iparm[2], LWS_UPPER, info->iparm[3],
                     blend, rgba, info->iparm[7]);

    rgba[0] = info->iparm[12];
    rgba[1] = info->iparm[13];
    rgba[2] = info->iparm[14];
    rgba[3] = info->iparm[15];

    if(info->iparm[4] && (P_GetIntp(DMU_SIDE, side, DMU_MIDDLE_TEXTURE) ||
                          info->iparm[6]))
    {
        if(!P_GetPtrp(DMU_LINE, line, DMU_BACK_SECTOR) && info->iparm[4] == -1)
            texture = 0;
        else
            texture = info->iparm[4];
    }

    XL_ChangeTexture(line, info->iparm[2], LWS_MID, texture,
                     info->iparm[8], rgba, info->iparm[7]);

    rgba[0] = info->iparm[16];
    rgba[1] = info->iparm[17];
    rgba[2] = info->iparm[18];
    XL_ChangeTexture(line, info->iparm[2], LWS_LOWER, info->iparm[5],
                     blend, rgba, info->iparm[7]);

    return true;
}

int C_DECL XLTrav_Activate(line_t *line, boolean dummy, void *context,
                    void *context2, mobj_t *activator)
{
    XL_LineEvent(XLE_CHAIN, 0, line, 0, activator);
    return true;    // Keep looking.
}

int C_DECL XLTrav_LineCount(line_t *line, boolean dummy, void *context, void *context2,
                     mobj_t *activator)
{
    xline_t *xline = &xlines[P_ToIndex(DMU_LINE, line)];
    linetype_t *info = context2;

    if(!xline->xg)
        return true;            // Must have extended type.
    if(info->iparm[2])
        xline->xg->info.act_count = info->iparm[3];
    else
        xline->xg->info.act_count += info->iparm[3];
    return true;
}

int C_DECL XLTrav_Music(line_t *line, boolean dummy, void *context, void *context2,
                 mobj_t *activator)
{
    int song = 0;
    int temp = 0;
    linetype_t *info = context2;

    if(info->iparm[2] == LREF_NONE)
    {    // the old format, use ip0 explicitly
        song = info->iparm[0];
    }
    else    // we might have a data reference to evaluate
    {
        if( info->iparm[2] == LREF_NONE)    // data (ip0) will be used to determine next level
        {
            song = info->iparm[0];

        }
        else
        { // evaluate ip0 for a data reference
            temp = XL_ValidateLineRef(line,info->iparm[0], context2, "Music ID");
            if(!temp)
                XG_Dev("XLTrav_Music: Reference data not valid. Song not changed");
            else
                song = temp;
        }
    }

    // FIXME: Add code to validate song id here

    if(song)
    {
        XG_Dev("XLTrav_Music: Play Music ID (%i)%s",song, info->iparm[1]? " looped.":".");
        S_StartMusicNum(song, info->iparm[1]);
    }
    return false;    // only do this once!
}

// maximum fixed_t units to move object to avoid hiccups
#define FUDGEFACTOR 10

int C_DECL XLTrav_LineTeleport(line_t *newline, boolean dummy, void *context, void *context2,
                        mobj_t *mobj)
{
    int fudge = FUDGEFACTOR;
    int side = 0, stepdown;
    unsigned    an;
    mobj_t    *flash;
    line_t *line = (line_t *) context;
    linetype_t *info = (linetype_t *) context2;
    vertex_t *newv1, *newv2;
    vertex_t *oldv1, *oldv2;
    sector_t *newfrontsector, *newbacksector;
    fixed_t newx, newy, newz, pos, s, c;
    fixed_t oldldx, oldldy, newldx, newldy;
    angle_t angle;

    // retrieve a few properties to make this look neater.
    oldv1 = P_GetPtrp(DMU_LINE, line, DMU_VERTEX1);
    oldv2 = P_GetPtrp(DMU_LINE, line, DMU_VERTEX2);
    oldldx = P_GetFixedp(DMU_LINE, line, DMU_DX);
    oldldy = P_GetFixedp(DMU_LINE, line, DMU_DY);

    newv1 = P_GetPtrp(DMU_LINE, newline, DMU_VERTEX1);
    newv2 = P_GetPtrp(DMU_LINE, newline, DMU_VERTEX2);
    newldx = P_GetFixedp(DMU_LINE, newline, DMU_DX);
    newldy = P_GetFixedp(DMU_LINE, newline, DMU_DY);
    newfrontsector = P_GetPtrp(DMU_LINE, newline, DMU_FRONT_SECTOR);
    newbacksector = P_GetPtrp(DMU_LINE, newline, DMU_BACK_SECTOR);

    // don't teleport things marked noteleport!
#ifdef __JHERETIC__
    if(mobj->flags2 & MF2_NOTELEPORT)
    {
        XG_Dev("XLTrav_LineTeleport: Activator can't be teleported (THING is unteleportable)");
        return false; // No point continuing...
    }
#endif

    // We shouldn't be trying to teleport to the same line
    if(newline == line)
    {
        XG_Dev("XLTrav_LineTeleport: Target == Origin. Continuing search...");
        return true; // Keep looking
    }

    // i2: 1 = Spawn Fog
    // i3: Sound = Sound to play
    // i4: 1 = reversed
    // i5: 1 = always telestomp

    XG_Dev("XLTrav_LineTeleport: %s, %s, %s",
           info->iparm[2]? "Spawn Flash":"No Flash", info->iparm[3]? "Play Sound":"Silent",
           info->iparm[4]? "Reversed":"Normal.");

    // Spawn flash at the old position?
    if(info->iparm[2])
    {
        flash = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_TFOG);

        // Play a sound?
        if(info->iparm[3])
            S_StartSound(info->iparm[3], flash);
    }

    // Get the thing's position along the source linedef
    if(abs(oldldx) > abs(oldldy))
        pos = FixedDiv(mobj->x - P_GetFixedp(DMU_VERTEX, oldv1, DMU_X), oldldx);
    else
        pos = FixedDiv(mobj->y - P_GetFixedp(DMU_VERTEX, oldv1, DMU_Y), oldldy);

    // Get the angle between the two linedefs, for rotating
    // orientation and momentum. Rotate 180 degrees, and flip
    // the position across the exit linedef, if reversed.
    angle = (info->iparm[4] ? pos = FRACUNIT-pos, 0 : ANG180) +
             R_PointToAngle2(0, 0, newldx, newldy) -
             R_PointToAngle2(0, 0, oldldx, oldldy);

    // Interpolate position across the exit linedef
    newx = P_GetFixedp(DMU_VERTEX, newv2, DMU_X) - FixedMul(pos, newldx);
    newy = P_GetFixedp(DMU_VERTEX, newv2, DMU_Y) - FixedMul(pos, newldy);

    // Sine, cosine of angle adjustment
    s = finesine[angle>>ANGLETOFINESHIFT];
    c = finecosine[angle>>ANGLETOFINESHIFT];

    // Whether walking towards first side of exit linedef steps down
    if(P_GetFixedp(DMU_SECTOR, newfrontsector, DMU_FLOOR_HEIGHT) <
       P_GetFixedp(DMU_SECTOR, newbacksector, DMU_FLOOR_HEIGHT))
        stepdown = true;
    else
        stepdown = false;

    // Height of thing above ground
    newz = mobj->z - mobj->floorz;

    // Side to exit the linedef on positionally.
    //
    // Notes:
    //
    // This flag concerns exit position, not momentum. Due to
    // roundoff error, the thing can land on either the left or
    // the right side of the exit linedef, and steps must be
    // taken to make sure it does not end up on the wrong side.
    //
    // Exit momentum is always towards side 1 in a reversed
    // teleporter, and always towards side 0 otherwise.
    //
    // Exiting positionally on side 1 is always safe, as far
    // as avoiding oscillations and stuck-in-wall problems,
    // but may not be optimum for non-reversed teleporters.
    //
    // Exiting on side 0 can cause oscillations if momentum
    // is towards side 1, as it is with reversed teleporters.
    //
    // Exiting on side 1 slightly improves player viewing
    // when going down a step on a non-reversed teleporter.

    if(!info->iparm[4] || (mobj->player && stepdown))
        side = 1;

    // Make sure we are on correct side of exit linedef.
    while(P_PointOnLineSide(newx, newy, newline) != side && --fudge>=0)
    {
        if (D_abs(newldx) > D_abs(newldy))
            newy -= newldx < 0 != side ? -1 : 1;
        else
            newx += newldy < 0 != side ? -1 : 1;
    }

    // Do the Teleport
    if(!P_TeleportMove (mobj, newx, newy, (info->iparm[5] > 0? true : false)))
    {
        XG_Dev("XLTrav_Teleport: Something went horribly wrong... aborting.");
        return false;
    }

    // Adjust z position to be same height above ground as before.
    // Ground level at the exit is measured as the higher of the
    // two floor heights at the exit linedef.
    if(stepdown)
        mobj->z = newz + P_GetFixedp(DMU_SECTOR, newfrontsector, DMU_FLOOR_HEIGHT);
    else
        mobj->z = newz + P_GetFixedp(DMU_SECTOR, newbacksector, DMU_FLOOR_HEIGHT);

    // Rotate mobj's orientation according to difference in linedef angles
    mobj->angle += angle;

    // Update momentum of mobj crossing teleporter linedef?
    newx = mobj->momx;
    newy = mobj->momy;

    // Rotate mobj's momentum to come out of exit just like it entered
    mobj->momx = FixedMul(newx, c) - FixedMul(newy, s);
    mobj->momy = FixedMul(newy, c) + FixedMul(newx, s);

    // Feet clipped?
#if __JHERETIC__
    if(mobj->flags2 & MF2_FOOTCLIP && P_GetThingFloorType(mobj) != FLOOR_SOLID)
        mobj->flags2 |= MF2_FEETARECLIPPED;
    else if(mobj->flags2 & MF2_FEETARECLIPPED)
        mobj->flags2 &= ~MF2_FEETARECLIPPED;
#endif

    // Spawn flash at the new position?
    if(info->iparm[2])
    {
        an = mobj->angle >> ANGLETOFINESHIFT;
        flash = P_SpawnMobj(mobj->x + 24 * finecosine[an],
                            mobj->y + 24 * finesine[an],
                            mobj->z, MT_TFOG);

        // Play a sound?
        if(info->iparm[3])
            S_StartSound(info->iparm[3], flash);
    }

    // Adjust the player's view, incase there has been a height change
    if(mobj->player)
    {
        mobj->dplayer->viewz = mobj->z + mobj->dplayer->viewheight;
        mobj->dplayer->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    }

    return false; // do this only once!
}


int XL_ValidateMap(int val, int type)
{
    int episode, level = val;

#ifdef __JDOOM__
    if (gamemode == commercial)
        episode = gameepisode;
    else
        episode = 0;
#elif __JHERETIC__
    episode = gameepisode;
#endif

    if(!G_ValidateMap(&episode, &level))
        XG_Dev("XLTrav_EndLevel: NOT A VALID MAP NUMBER %i (next level set to %i)",val,level);

    return level;
}

int C_DECL XLTrav_EndLevel(line_t *line, boolean dummy, void *context,
                           void *context2, mobj_t *activator)
{
    int map = 0;
    int temp = 0;
    linetype_t *info = context2;

    // Is this a secret exit?
    if(info->iparm[0] > 0)
    {
        G_SecretExitLevel();
        return false;
    }

    if(info->iparm[1] == LREF_NONE)    // (ip3) will be used to determine next level
    {
        map = XL_ValidateMap(info->iparm[3], 0);

    }
    else
    {    // we have a data reference to evaluate
        temp = XL_ValidateLineRef(line,info->iparm[3], context2, "Map Number");
        if(temp > 0)
            map = XL_ValidateMap(temp, info->iparm[3]);
        else
            XG_Dev("XLTrav_EndLevel: Reference data not valid. Next level as normal");
    }

    if(map)
    {
        XG_Dev("XLTrav_EndLevel: Next level set to %i",map);
        nextmap = map;
    }

    G_ExitLevel();
    return false;    // only do this once!
}

int C_DECL XLTrav_DisableLine(line_t *line, boolean dummy, void *context,
                              void *context2, mobj_t *activator)
{
    xline_t *origline;
    xline_t *xline = &xlines[P_ToIndex(DMU_LINE, line)];

    origline = &xlines[P_ToIndex(DMU_LINE, (line_t*) context)];

    if(!xline->xg)
        return true;
    xline->xg->disabled = origline->xg->active;
    return true;    // Keep looking.
}

int C_DECL XLTrav_EnableLine(line_t *line, boolean dummy, void *context,
                             void *context2, mobj_t *activator)
{
    xline_t *origline;
    xline_t *xline = &xlines[P_ToIndex(DMU_LINE, line)];

    origline = &xlines[P_ToIndex(DMU_LINE, (line_t*) context)];

    if(!xline->xg)
        return true;
    xline->xg->disabled = !origline->xg->active;
    return true;    // Keep looking.
}

/*
 * Checks if the given lines are active or inactive.
 * Returns true if all are in the specified state.
 */
boolean XL_CheckLineStatus(line_t *line, int reftype, int ref, int active,
                           mobj_t *activator)
{
    return XL_TraverseLines(line, reftype, ref, &active, 0, activator, XLTrav_CheckLine);
}

boolean XL_CheckMobjGone(int thingtype)
{
    thinker_t *th;
    mobj_t *mo;

    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        // Only find mobjs.
        if(th->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) th;
        if(mo->type == thingtype && mo->health > 0)
        {
            // Not dead.
            XG_Dev("XL_CheckMobjGone: Thing type %i: Found mo id=%i, "
                   "health=%i, pos=(%i,%i)", thingtype, mo->thinker.id,
                   mo->health, mo->x >> FRACBITS, mo->y >> FRACBITS);
            return false;
        }
    }

    XG_Dev("XL_CheckMobjGone: Thing type %i is gone", thingtype);
    return true;
}

boolean XL_SwitchSwap(side_t* side, int section)
{
    char   *name;
    char    buf[10];
    int     texid;
    boolean makeChange = false;

    if(!side)
        return false;

    // Which section of the wall are we checking?
    if(section == LWS_UPPER)
        name = R_TextureNameForNum(P_GetIntp(DMU_SIDE, side, DMU_TOP_TEXTURE));
    else if(section == LWS_MID)
        name = R_TextureNameForNum(P_GetIntp(DMU_SIDE, side, DMU_MIDDLE_TEXTURE));
    else if(section == LWS_LOWER)
        name = R_TextureNameForNum(P_GetIntp(DMU_SIDE, side, DMU_BOTTOM_TEXTURE));
    else
        return false;

    strncpy(buf, name, 8);
    buf[8] = 0;

    // Does this texture have another switch texture?
#ifdef __JHERETIC__
    // A kludge for Heretic.  Since it has some switch texture names
    // that don't follow the SW1/SW2 pattern, we'll do some special
    // checking.
    if(!stricmp(buf, "SW1ON"))
    {
        texid = R_TextureNumForName("SW1OFF");
        makeChange = true;
    }
    if(!stricmp(buf, "SW1OFF"))
    {
        texid = R_TextureNumForName("SW1ON");
        makeChange = true;
    }
    if(!stricmp(buf, "SW2ON"))
    {
        texid = R_TextureNumForName("SW2OFF");
        makeChange = true;
    }
    if(!stricmp(buf, "SW2OFF"))
    {
        texid = R_TextureNumForName("SW2ON");
        makeChange = true;
    }
#endif

    if(!strnicmp(buf, "SW1", 3))
    {
        buf[2] = '2';
        texid = R_TextureNumForName(buf);
        makeChange = true;
    }
    if(!strnicmp(buf, "SW2", 3))
    {
        buf[2] = '1';
        texid = R_TextureNumForName(buf);
        makeChange = true;
    }

    // Are we doing a switch swap?
    if(makeChange)
    {
        // Which section of the wall are we working on?
        // Make the change.
        if(section == LWS_UPPER)
            P_SetIntp(DMU_SIDE, side, DMU_TOP_TEXTURE, texid);
        else if(section == LWS_MID)
            P_SetIntp(DMU_SIDE, side, DMU_MIDDLE_TEXTURE, texid);
        else if(section == LWS_LOWER)
            P_SetIntp(DMU_SIDE, side, DMU_BOTTOM_TEXTURE, texid);
        else
            return false;

        // The change was successfull.
        return true;
    }
    else
        return false;
}

void XL_SwapSwitchTextures(line_t *line, int snum)
{
    side_t *side;

    if(snum)
        side = P_GetPtrp(DMU_LINE, line, DMU_SIDE1);
    else
        side = P_GetPtrp(DMU_LINE, line, DMU_SIDE0);

    if(!side)
        return;

    if(XL_SwitchSwap(side, LWS_UPPER) ||
       XL_SwitchSwap(side, LWS_MID) ||
       XL_SwitchSwap(side, LWS_LOWER) )
        XG_Dev("XL_SwapSwitchTextures: Line %i, side %i", P_ToIndex(DMU_LINE, line),
                P_ToIndex(DMU_SIDE, side));
}

/*
 * Changes texture of the given line.
 */
void XL_ChangeTexture(line_t *line, int sidenum, int section, int texture,
                      blendmode_t blendmode, byte rgba[4], int flags)
{
    int i;
    int currentFlags;
    side_t *side = P_ToPtr(DMU_SIDE, sidenum);

    if(!side)
        return;

    // Clamp the values
    for(i = 0; i < 4; i++)
    {
        if(rgba[i] > 255)
            rgba[i] = 255;

        if(rgba[i] < 0)
            rgba[i] = 0;
    }

    XG_Dev("XL_ChangeTexture: Line %i, side %i, section %i, texture %i",
           P_ToIndex(DMU_LINE, line), sidenum, section, texture);
    XG_Dev("  red %i, green %i, blue %i, alpha %i, blendmode %i",
           rgba[0], rgba[1], rgba[2], rgba[3], blendmode);

    // Which wall section are we working on?
    if(section == LWS_MID)
    {
        // Are we removing the middle texture?
        if(texture == -1)
            P_SetIntp(DMU_SIDE, side, DMU_MIDDLE_TEXTURE, 0);
        else if(texture)
            P_SetIntp(DMU_SIDE, side, DMU_MIDDLE_TEXTURE, texture);

        // Are we changing the blendmode?
        if(blendmode)
            P_SetIntp(DMU_SIDE, side, DMU_MIDDLE_BLENDMODE, blendmode);

        // Are we changing the surface color?
        for(i = 0; i < 4; i++)
            if(rgba[i])
                P_SetBytep(DMU_SIDE, side, TO_DMU_MIDDLE_COLOR(i), rgba[i]);
    }
    else if(section == LWS_UPPER)
    {
        if(texture)
            P_SetIntp(DMU_SIDE, side, DMU_TOP_TEXTURE, texture);

        for(i = 0; i < 3; i++)
            if(rgba[i])
                P_SetBytep(DMU_SIDE, side, TO_DMU_TOP_COLOR(i), rgba[i]);
    }
    else if(section == LWS_LOWER)
    {
        if(texture)
            P_SetIntp(DMU_SIDE, side, DMU_BOTTOM_TEXTURE, texture);

        for(i = 0; i < 3; i++)
            if(rgba[i])
                P_SetBytep(DMU_SIDE, side, TO_DMU_BOTTOM_COLOR(i), rgba[i]);
    }

    // Adjust the side's flags
    currentFlags = P_GetIntp(DMU_SIDE, side, DMU_FLAGS);
    currentFlags |= flags;

    P_SetIntp(DMU_SIDE, side, DMU_FLAGS, currentFlags);
}

void XL_Message(mobj_t *act, char *msg, boolean global)
{
    player_t *pl;
    int     i;

    if(!msg || !msg[0])
        return;

    if(global)
    {
        XG_Dev("XL_Message: GLOBAL '%s'", msg);
        // Send to all players in the game.
        for(i = 0; i < MAXPLAYERS; i++)
            if(players[i].plr->ingame)
                P_SetMessage(players + i, msg);
        return;
    }

    if(act->player)
    {
        pl = act->player;
    }
    else if(act->flags & MF_MISSILE && act->target && act->target->player)
    {
        // Originator of the missile.
        pl = act->target->player;
    }
    else
    {
        // We don't know whom to send the message.
        XG_Dev("XL_Message: '%s'", msg);
        XG_Dev("  NO DESTINATION, MESSAGE DISCARDED");
        return;
    }
    P_SetMessage(pl, msg);
}

/*
 * XL_ActivateLine
 */
void XL_ActivateLine(boolean activating, linetype_t * info, line_t *line,
                     int sidenum, mobj_t *data, int evtype)
{
    byte rgba[4] = { 0, 0, 0, 0 };
    int  lineid = P_ToIndex(DMU_LINE, line);
    xgline_t *xg;
    sector_t *frontsector;
    mobj_t *activator_thing = (mobj_t *) data;
    degenmobj_t *soundorg;

    xg = P_XLine(line)->xg;
    frontsector = P_GetPtrp(DMU_LINE, line, DMU_FRONT_SECTOR);

    XG_Dev("XL_ActivateLine: %s line %i, side %i, type %i",
           activating ? "Activating" : "Deactivating", lineid,
           sidenum, P_XLine(line)->special);

    if(xg->disabled)
    {
        XG_Dev("  LINE DISABLED, ABORTING");
        return;        // The line is disabled.
    }

    if((activating && xg->active) || (!activating && !xg->active))
    {
        XG_Dev("  Line is ALREADY %s, ABORTING",
               activating ? "ACTIVE" : "INACTIVE");
        return;    // Do nothing (can't activate if already active!).
    }

    // Activation should happen on the front side.
    if(frontsector)
        soundorg = P_GetPtrp(DMU_SECTOR, frontsector, DMU_SOUND_ORIGIN);

    // Let the line know who's activating it.
    xg->activator = data;

    // Process (de)activation chains. Chains always pass as an activation
    // method, but the other requirements of the chained type must be met.
    if(activating && info->act_chain)
    {
        XG_Dev("  Line has Act Chain (type %i) - It will be processed first...",info->act_chain);
        XL_LineEvent(XLE_CHAIN, info->act_chain, line, sidenum, data);
    }
    else if(!activating && info->deact_chain)
    {
        XG_Dev("  Line has Deact Chain (type %i) - It will be processed first...",info->deact_chain);
        XL_LineEvent(XLE_CHAIN, info->deact_chain, line, sidenum, data);
    }

    // Automatically swap any SW* textures.
    if(xg->active != activating)
        XL_SwapSwitchTextures(line, sidenum);

    // Change the state of the line.
    xg->active = activating;
    xg->timer = 0;        // Reset timer.

    // Activate lines with a matching tag with Group Activation.
    if((activating && info->flags2 & LTF2_GROUP_ACT) ||
       (!activating && info->flags2 & LTF2_GROUP_DEACT))
    {
        XL_TraverseLines(line, LREF_LINE_TAGGED, true, &activating, 0, activator_thing,
                         XLTrav_SmartActivate);
    }

    // For lines flagged Multiple, quick-(de)activate other lines that have
    // the same line tag.
    if(info->flags2 & LTF2_MULTIPLE)
    {
        XL_TraverseLines(line, LREF_LINE_TAGGED, true, &activating, 0, activator_thing,
                         XLTrav_QuickActivate);
    }

    // Should we apply the function of the line? Functions are defined by
    // the class of the line type.
    if(((activating && info->flags2 & LTF2_WHEN_ACTIVATED) ||
        (!activating && info->flags2 & LTF2_WHEN_DEACTIVATED)))
    {
        if(!(info->flags2 & LTF2_WHEN_LAST) || info->act_count == 1)
            XL_DoFunction(info, line, sidenum, activator_thing, evtype);
        else
            XG_Dev("  Line %i FUNCTION TEST FAILED", lineid);
    }
    else
    {
        if(activating)
            XG_Dev("  Line %i has no activation function", lineid);
        else
        {
            XG_Dev("  Line %i has no deactivation function", lineid);
        }
    }

    // Now do any secondary actions that should happen AFTER
    // the function of the line (regardless if one was applied or not)
    if(activating)
    {
        XL_Message(activator_thing, info->act_msg,
                   (info->flags2 & LTF2_GLOBAL_A_MSG) != 0);

        if(info->act_sound)
            S_StartSound(info->act_sound, (mobj_t *) soundorg);

        // Change the texture of the line if asked to.
        if(info->wallsection && info->act_tex)
            XL_ChangeTexture(line, sidenum, info->wallsection, info->act_tex, BM_NORMAL, rgba, 0); //FIXME

        // Change the class of the line if asked to
        if(info->act_linetype)
            XL_SetLineType(line, info->act_linetype);
    }
    else
    {
        XL_Message(activator_thing, info->deact_msg,
                   (info->flags2 & LTF2_GLOBAL_D_MSG) != 0);

        if(info->deact_sound)
            S_StartSound(info->deact_sound, (mobj_t *) soundorg);

        // Change the texture of the line if asked to.
        if(info->wallsection && info->deact_tex)
            XL_ChangeTexture(line, sidenum, info->wallsection,
                             info->deact_tex, BM_NORMAL, rgba, 0); //FIXME

        // Change the class of the line if asked to
        if(info->deact_linetype)
            XL_SetLineType(line, info->deact_linetype);
    }
}

/*
 * XL_CheckKeys
 */
boolean XL_CheckKeys(mobj_t *mo, int flags2)
{
    player_t *act = mo->player;

#ifdef __JDOOM__
    int     num = 6;
    char   *keystr[] = {
        "BLUE KEYCARD", "YELLOW KEYCARD", "RED KEYCARD",
        "BLUE SKULL KEY", "YELLOW SKULL KEY", "RED SKULL KEY"
    };
    int    *keys = (int *) act->keys;
    int     badsound = sfx_oof;
#elif defined __JHERETIC__
    int     num = 3;
    char   *keystr[] = { "YELLOW KEY", "GREEN KEY", "BLUE KEY" };
    boolean *keys = act->keys;
    int     badsound = sfx_plroof;
#elif defined __JSTRIFE__
//FIXME!!!
    int     num = 3;
    char   *keystr[] = { "YELLOW KEY", "GREEN KEY", "BLUE KEY" };
    int    *keys = (int *) act->keys;
    int     badsound = SFX_NONE;
#endif
    int     i;

    for(i = 0; i < num; i++)
    {
        if(flags2 & LTF2_KEY(i) && !keys[i])
        {
            // This key is missing! Show a message.
            sprintf(msgbuf, "YOU NEED A %s.", keystr[i]);
            XL_Message(mo, msgbuf, false);
            S_ConsoleSound(badsound, mo, act - players);
            return false;
        }
    }
    return true;
}

/*
 * Decides if the event leads to (de)activation.
 * Returns true if the event is processed.
 * Line must be extended.
 * Most conditions use AND (act method, game mode and difficult use OR).
 */
int XL_LineEvent(int evtype, int linetype, line_t *line, int sidenum,
                 void *data)
{
    int lineid = P_ToIndex(DMU_LINE, line);
    xgline_t *xg;
    linetype_t *info;
    boolean active;
    mobj_t *activator_thing = (mobj_t *) data;
    player_t *activator;
    int     i;
    int     flags;
    boolean anyTrigger = false;

    xg = P_XLine(line)->xg;
    info = &xg->info;
    active = xg->active;
    flags = P_GetIntp(DMU_LINE, line, DMU_FLAGS);

    // Clients rely on the server, they don't do XG themselves.
    if(IS_CLIENT)
        return false;

    if(activator_thing)
         activator = activator_thing->player;

#ifdef __JDOOM__
    // BOOM intergration
    if(flags & ML_ALLTRIGGER && !(info->flags2 & LTF2_OVERRIDE_ANY))
        anyTrigger = true;
#endif

    XG_Dev("XL_LineEvent: %s line %i, side %i (chained type %i)%s",
           EVTYPESTR(evtype), lineid, sidenum, linetype,
           anyTrigger? " ANY Trigger":"");

    if(xg->disabled)
    {
        XG_Dev("  LINE IS DISABLED, ABORTING EVENT");
        return false;        // The line is disabled.
    }

    // This is a chained event.
    if(linetype)
    {
        if(!XL_GetType(linetype))
            return false;
        info = &typebuffer;
    }

    // Process chained event first. It takes precedence.
    if(info->ev_chain)
    {
        if(XL_LineEvent(evtype, info->ev_chain, line, sidenum, data))
        {
            XG_Dev("  Event %s, line %i, side %i OVERRIDDEN BY EVENT CHAIN %i",
                   EVTYPESTR(evtype), lineid, sidenum, info->ev_chain);
            return true;
        }
    }

    // Check restrictions and conditions that will prevent processing
    // the event.
    if((active && info->act_type == LTACT_COUNTED_OFF) ||
       (!active && info->act_type == LTACT_COUNTED_ON))
    {
        // Can't be processed at this time.
        XG_Dev("  Line %i: Active=%i, type=%i ABORTING EVENT", lineid,
               active, info->act_type);
        return false;
    }

    // Check the type of the event vs. the requirements of the line.
    if(evtype == XLE_CHAIN || evtype == XLE_FUNC)
        goto type_passes;
    if(evtype == XLE_USE &&
       (((info->flags & LTF_PLAYER_USE_A && activator && !active) ||
        (info->flags & LTF_OTHER_USE_A && !activator && !active) ||
        (info->flags & LTF_PLAYER_USE_D && activator && active) ||
        (info->flags & LTF_OTHER_USE_D && !activator && active)) || anyTrigger))
        goto type_passes;
    if(evtype == XLE_SHOOT &&
       (((info->flags & LTF_PLAYER_SHOOT_A && activator && !active) ||
        (info->flags & LTF_OTHER_SHOOT_A && !activator && !active) ||
        (info->flags & LTF_PLAYER_SHOOT_D && activator && active) ||
        (info->flags & LTF_OTHER_SHOOT_D && !activator && active)) || anyTrigger))
        goto type_passes;
    if(evtype == XLE_CROSS &&
       (((info->flags & LTF_PLAYER_CROSS_A && activator && !active) ||
        (info->flags & LTF_MONSTER_CROSS_A &&
         activator_thing->flags & MF_COUNTKILL && !active) ||
        (info->flags & LTF_MISSILE_CROSS_A &&
         activator_thing->flags & MF_MISSILE && !active) ||
        (info->flags & LTF_ANY_CROSS_A && !active) ||
        (info->flags & LTF_PLAYER_CROSS_D && activator && active) ||
        (info->flags & LTF_MONSTER_CROSS_D &&
         activator_thing->flags & MF_COUNTKILL && active) ||
        (info->flags & LTF_MISSILE_CROSS_D &&
         activator_thing->flags & MF_MISSILE && active) ||
        (info->flags & LTF_ANY_CROSS_D && active)) || anyTrigger))
        goto type_passes;
    if(evtype == XLE_HIT &&
       (((info->flags & LTF_PLAYER_HIT_A && activator && !active) ||
        (info->flags & LTF_OTHER_HIT_A && !activator && !active) ||
        (info->flags & LTF_MONSTER_HIT_A &&
         activator_thing->flags & MF_COUNTKILL && !active) ||
        (info->flags & LTF_MISSILE_HIT_A && activator_thing->flags & MF_MISSILE
         && !active) || (info->flags & LTF_ANY_HIT_A && !active) ||
        (info->flags & LTF_PLAYER_HIT_D && activator && active) ||
        (info->flags & LTF_OTHER_HIT_D && !activator && active) ||
        (info->flags & LTF_MONSTER_HIT_D &&
         activator_thing->flags & MF_COUNTKILL && active) ||
        (info->flags & LTF_MISSILE_HIT_D && activator_thing->flags & MF_MISSILE
         && active) || (info->flags & LTF_ANY_HIT_D && active)) || anyTrigger))
        goto type_passes;
    if(evtype == XLE_TICKER &&
       ((info->flags & LTF_TICKER_A && !active) ||
        (info->flags & LTF_TICKER_D && active)))
        goto type_passes;

    // Type doesn't pass, sorry.
    XG_Dev("  Line %i: ACT REQUIREMENTS NOT FULFILLED, ABORTING EVENT", lineid);
    return false;

  type_passes:

    if(info->flags & LTF_NO_OTHER_USE_SECRET)
    {
        // Non-players can't use this line if line is flagged secret.
        if(evtype == XLE_USE && !activator && flags & ML_SECRET)
        {
            XG_Dev("  Line %i: ABORTING EVENT due to no_other_use_secret", lineid);
            return false;
        }
    }
    if(info->flags & LTF_MOBJ_GONE)
    {
        if(!XL_CheckMobjGone(info->aparm[9]))
        return false;
    }
    if(info->flags & LTF_ACTIVATOR_TYPE)
    {
        // Check the activator's type.
        if(!activator_thing || activator_thing->type != info->aparm[9])
        {
            XG_Dev("  Line %i: ABORTING EVENT due to activator type", lineid);
            return false;
        }
    }
    if((evtype == XLE_USE || evtype == XLE_SHOOT || evtype == XLE_CROSS) &&
       !(info->flags2 & LTF2_TWOSIDED))
    {
        // Only allow (de)activation from the front side.
        if(sidenum != 0)
        {
            XG_Dev("  Line %i: ABORTING EVENT due to line side test", lineid);
            return false;
        }
    }

    // Check counting.
    if(!info->act_count)
    {
        XG_Dev("  Line %i: ABORTING EVENT due to Count = 0", lineid);
        return false;
    }

    // More requirements.
    if(info->flags2 & LTF2_HEALTH_ABOVE &&
       activator_thing->health <= info->aparm[0])
        return false;
    if(info->flags2 & LTF2_HEALTH_BELOW &&
       activator_thing->health >= info->aparm[1])
        return false;
    if(info->flags2 & LTF2_POWER_ABOVE &&
       (!activator || activator->armorpoints <= info->aparm[2]))
        return false;
    if(info->flags2 & LTF2_POWER_BELOW &&
       (!activator || activator->armorpoints >= info->aparm[3]))
        return false;
    if(info->flags2 & LTF2_LINE_ACTIVE)
        if(!XL_CheckLineStatus(line, info->aparm[4], info->aparm[5], true,
                               activator_thing))
        {
            XG_Dev("  Line %i: ABORTING EVENT due to line_active test", lineid);
            return false;
        }
    if(info->flags2 & LTF2_LINE_INACTIVE)
        if(!XL_CheckLineStatus(line, info->aparm[6], info->aparm[7], false,
                               activator_thing))
        {
            XG_Dev("  Line %i: ABORTING EVENT due to line_inactive test", lineid);
            return false;
        }
    // Check game mode.
    if(IS_NETGAME)
    {
        if(!(info->flags2 & (LTF2_COOPERATIVE | LTF2_DEATHMATCH)))
        {
            XG_Dev("  Line %i: ABORTING EVENT due to netgame mode", lineid);
            return false;
        }
    }
    else
    {
        if(!(info->flags2 & LTF2_SINGLEPLAYER))
        {
            XG_Dev("  Line %i: ABORTING EVENT due to game mode (1p)", lineid);
            return false;
        }
    }
    // Check skill level.
    if(gameskill < 1)
        i = 1;
    else if(gameskill > 3)
        i = 4;
    else
        i = 1 << (gameskill - 1);
    i <<= LTF2_SKILL_SHIFT;
    if(!(info->flags2 & i))
    {
        XG_Dev("  Line %i: ABORTING EVENT due to skill level (%i)",
               lineid, gameskill);
        return false;
    }
    // Check activator color.
    if(info->flags2 & LTF2_COLOR)
    {
        if(!activator)
            return false;
        if(cfg.PlayerColor[activator - players] != info->aparm[8])
        {
            XG_Dev("  Line %i: ABORTING EVENT due to activator color (%i)",
                   lineid, cfg.PlayerColor[activator-players]);
            return false;
        }
    }
    // Keys require that the activator is a player.
    if(info->
       flags2 & (LTF2_KEY1 | LTF2_KEY2 | LTF2_KEY3 | LTF2_KEY4 | LTF2_KEY5 |
                 LTF2_KEY6))
    {
        // Check keys.
        if(!activator)
        {
            XG_Dev("  Line %i: ABORTING EVENT due to missing key "
                   "(no activator)", lineid);
            return false;
        }
        // Check that all the flagged keys are present.
        if(!XL_CheckKeys(activator_thing, info->flags2))
        {
            XG_Dev("  Line %i: ABORTING EVENT due to missing key", lineid);
            return false;        // Keys missing!
        }
    }

    // All tests passed, use this event.
    if(info->act_count > 0 && evtype != XLE_CHAIN && evtype != XLE_FUNC)
    {
        // Decrement counter.
        info->act_count--;

        XG_Dev("  Line %i: Decrementing counter, now %i", lineid,
               info->act_count);
    }
    XL_ActivateLine(!active, info, line, sidenum, activator_thing, evtype);
    return true;
}

/*
 * Returns true if the event was processed.
 */
int XL_CrossLine(line_t *line, int sidenum, mobj_t *thing)
{
    if(!P_XLine(line)->xg)
        return false;
    return XL_LineEvent(XLE_CROSS, 0, line, sidenum, thing);
}

/*
 * Returns true if the event was processed.
 */
int XL_UseLine(line_t *line, int sidenum, mobj_t *thing)
{
    if(!P_XLine(line)->xg)
        return false;
    return XL_LineEvent(XLE_USE, 0, line, sidenum, thing);
}

/*
 * Returns true if the event was processed.
 */
int XL_ShootLine(line_t *line, int sidenum, mobj_t *thing)
{
    if(!P_XLine(line)->xg)
        return false;
    return XL_LineEvent(XLE_SHOOT, 0, line, sidenum, thing);
}

int XL_HitLine(line_t *line, int sidenum, mobj_t *thing)
{
    if(!P_XLine(line)->xg)
        return false;
    return XL_LineEvent(XLE_HIT, 0, line, sidenum, thing);
}

void XL_DoChain(line_t *line, int chain, boolean activating, mobj_t *act_thing)
{
#ifdef TODO_MAP_UPDATE
    line_t  dummy;
    xgline_t dummyxg;

    XG_Dev("XL_DoChain: Line %i, chained type %i", line - lines, chain);
    XG_Dev("  (dummy line will show up as %i)", &dummy - lines);

    // We'll use a dummy line for the chain.
    memcpy(&dummy, line, sizeof(*line));
    memcpy(&dummyxg, line->xg, sizeof(*line->xg));
    dummy.sidenum[0] = -1;
    dummy.sidenum[1] = -1;
    dummy.xg = &dummyxg;
    dummyxg.active = !activating;

    XL_LineEvent(XLE_CHAIN, chain, &dummy, 0, act_thing);
#endif
}

void XL_ChainSequenceThink(line_t *line)
{
    int lineid = P_ToIndex(DMU_LINE, line);
    xgline_t *xg;
    linetype_t *info;

    xg = P_XLine(line)->xg;
    info = &xg->info;

    // Only process active chain sequences.
    if(info->line_class != LTC_CHAIN_SEQUENCE || !xg->active)
        return;

    xg->chtimer -= TIC2FLT(1);

    // idata = current pos
    // fdata = count down seconds

    // i1..i19: line types
    // f0: interval randomness (100 means real interval can be 0%..200%).
    // f1..f19: intervals (seconds)

    // If the counter goes to zero, it's time to execute the chain.
    if(xg->chtimer < 0)
    {
        XG_Dev("XL_ChainSequenceThink: Line %i, executing...", lineid);

        // Are there any more chains?
        if(xg->chidx < DDLT_MAX_PARAMS && info->iparm[xg->chidx])
        {
            // Only send activation events.
            XL_DoChain(line, info->iparm[xg->chidx], true, xg->activator);

            // Advance to the next one.
            xg->chidx++;

            // Are we out of chains?
            if((xg->chidx == DDLT_MAX_PARAMS || !info->iparm[xg->chidx]) &&
               info->iparm[0] & CHSF_LOOP)
            {
                // Loop back to beginning.
                xg->chidx = 1;
            }
            // If there are more chains, get the next interval.
            if(xg->chidx < DDLT_MAX_PARAMS && info->iparm[xg->chidx])
            {
                // Start counting it down.
                xg->chtimer =
                    XG_RandomPercentFloat(info->fparm[xg->chidx],
                                          info->fparm[0]);
            }
        }
        else if(info->iparm[0] & CHSF_DEACTIVATE_WHEN_DONE)
        {
        // The sequence has been completed.
            XL_ActivateLine(false, info, line, 0, xg->activator, XLE_CHAIN);
        }
    }
}

/*
 * Called once a tic for each XG line.
 */
void XL_Think(line_t *line)
{
    float   levtime = TIC2FLT(leveltime);
    xgline_t *xg = P_XLine(line)->xg;
    linetype_t *info = &xg->info;

    if(xg->disabled)
        return;        // Disabled, do nothing.

    // Increment time.
    if(xg->timer >= 0)
    {
        xg->timer++;
        xg->ticker_timer++;
    }

    // Activation by ticker.
    if((info->ticker_end <= 0 ||
        (levtime >= info->ticker_start && levtime <= info->ticker_end)) &&
       xg->ticker_timer > info->ticker_interval)
    {
        if(info->flags & LTF_TICKER)
        {
            xg->ticker_timer = 0;
            XL_LineEvent(XLE_TICKER, 0, line, 0, &dummything);
        }
        // How about some forced functions?
        if(((info->flags2 & LTF2_WHEN_ACTIVE && xg->active) ||
            (info->flags2 & LTF2_WHEN_INACTIVE && !xg->active)) &&
            (!(info->flags2 & LTF2_WHEN_LAST) || info->act_count == 1))
        {
            XL_DoFunction(info, line, 0, xg->activator, XLE_FORCED);
        }
    }

    XL_ChainSequenceThink(line);

    // Check for automatical (de)activation.
    if(((info->act_type == LTACT_COUNTED_OFF ||
         info->act_type == LTACT_FLIP_COUNTED_OFF) && xg->active) ||
       ((info->act_type == LTACT_COUNTED_ON ||
         info->act_type == LTACT_FLIP_COUNTED_ON) && !xg->active))
    {
        if(info->act_time >= 0 && xg->timer > FLT2TIC(info->act_time))
        {
            XG_Dev("XL_Think: Line %i, timed to go %s", P_ToIndex(DMU_LINE, line),
                    xg->active ? "INACTIVE" : "ACTIVE");

            // Swap line state without any checks.
            XL_ActivateLine(!xg->active, info, line, 0, &dummything, XLE_AUTO);
        }
    }

    if(info->texmove_speed)
    {
        // The texture should be moved. Calculate the offsets.
        int offset; // The current offset.
        side_t* side;

        angle_t ang =
            ((angle_t) (ANGLE_MAX * (info->texmove_angle / 360))) >>
            ANGLETOFINESHIFT;

        fixed_t spd = FRACUNIT * info->texmove_speed;
        fixed_t xoff = -FixedMul(finecosine[ang], spd);
        fixed_t yoff = FixedMul(finesine[ang], spd);

        // Apply to both sides of the line.
        // Front side
        side = P_GetPtrp(DMU_LINE, line, DMU_SIDE0);
        if(side)
        {
            offset = P_GetIntp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_X) + xoff;
            P_SetIntp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_X, offset);

            offset = P_GetIntp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_Y) + yoff;
            P_SetIntp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_Y, offset);
        }
        // back side
        side = P_GetPtrp(DMU_LINE, line, DMU_SIDE1);
        if(side)
        {
            offset = P_GetIntp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_X) + xoff;
            P_SetIntp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_X, offset);

            offset = P_GetIntp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_Y) + yoff;
            P_SetIntp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_Y, offset);
        }
    }
}

/*
 * Think for each extended line.
 */
 void XL_Ticker(void)
{
    int     i;

    for(i = 0; i < numlines; i++)
    {
        if(!xlines[i].xg)
            continue;        // Not an extended line.
        XL_Think(P_ToPtr(DMU_LINE, i));
    }
}

/*
 * During update, definitions are re-read, so the pointers need to be
 * updated. However, this is a bit messy operation, prone to errors.
 * Instead, we just disable XG.
 */
void XL_Update(void)
{
    int     i;

    // It's all PU_LEVEL memory, so we can just lose it.
    for(i = 0; i < numlines; i++)
        if(xlines[i].xg)
        {
            xlines[i].xg = NULL;
            xlines[i].special = 0;
        }
}
