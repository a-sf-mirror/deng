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
 * doomdef.h: Internally used data structures for virtually everything,
 * key definitions, lots of other stuff.
 */

#ifndef __DOOMDEF_H__
#define __DOOMDEF_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#ifdef WIN32
#pragma warning(disable:4244)
#endif

#include <stdio.h>
#include <string.h>

#include "doomsday.h"
#include "dd_api.h"
#include "version.h"

#define Set                 DD_SetInteger
#define Get                 DD_GetInteger

#define CONFIGFILE          GAMENAMETEXT".cfg"
#define DEFSFILE            GAMENAMETEXT"\\"GAMENAMETEXT".ded"
#define DATAPATH            "}data\\"GAMENAMETEXT"\\"
#define STARTUPWAD          "}data\\"GAMENAMETEXT"\\"GAMENAMETEXT".wad"
#define STARTUPPK3          "}data\\"GAMENAMETEXT"\\"GAMENAMETEXT".pk3"

extern game_import_t gi;
extern game_export_t gx;

//
// Global parameters/defines.
//

#define MOBJINFO            (*gi.mobjInfo)
#define STATES              (*gi.states)
#define VALIDCOUNT          (*gi.validCount)

// Verbose messages.
#define VERBOSE(code)       { if(verbose >= 1) { code; } }
#define VERBOSE2(code)      { if(verbose >= 2) { code; } }

/**
 * Game mode handling - identify IWAD version to handle IWAD dependant
 * animations, game logic etc.
 * \note DOOM 2 german edition not detected.
 */
typedef enum {
    shareware, // DOOM 1 shareware, E1, M9
    registered, // DOOM 1 registered, E3, M27
    commercial, // DOOM 2 retail, E1 M34
    retail, // DOOM 1 retail, E4, M36
    indetermined, // Well, no IWAD found.
    NUM_GAME_MODES
} gamemode_t;

// Game mode bits for the above.
#define GM_SHAREWARE        0x1 // DOOM 1 shareware, E1, M9
#define GM_REGISTERED       0x2 // DOOM 1 registered, E3, M27
#define GM_COMMERCIAL       0x4 // DOOM 2 retail, E1 M34
// DOOM 2 german edition not handled.
#define GM_RETAIL           0x8 // DOOM 1 retail, E4, M36
#define GM_INDETERMINED     0x16 // Well, no IWAD found.

#define GM_ANY              (GM_SHAREWARE|GM_REGISTERED|GM_COMMERCIAL|GM_RETAIL)
#define GM_NOTSHAREWARE     (GM_REGISTERED|GM_COMMERCIAL|GM_RETAIL)

// Mission packs - might be useful for TC stuff?
typedef enum {
    GM_DOOM, // DOOM 1
    GM_DOOM2, // DOOM 2
    GM_TNT, // TNT mission pack
    GM_PLUT, // Plutonia pack
    GM_NONE,
    NUM_GAME_MISSIONS
} gamemission_t;

#define SCREENWIDTH         320
#define SCREENHEIGHT        200
#define SCREEN_MUL          1

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS          16

// State updates, number of tics / second.
#define TICRATE             35

/**
 * The current (high-level) state of the game: whether we are playing,
 * gazing at the intermission screen, the game final animation, or a demo.
 */
typedef enum gamestate_e {
    GS_MAP,
    GS_INTERMISSION,
    GS_FINALE,
    GS_DEMOSCREEN,
    GS_WAITING,
    GS_INFINE,
    NUM_GAME_STATES
} gamestate_t;

//
// Player Classes
//
typedef enum {
    PCLASS_PLAYER,
    NUM_PLAYER_CLASSES
} playerclass_t;

#define PCLASS_INFO(class)  (&classInfo[class])

typedef struct classinfo_s{
    char*       niceName;
    boolean     userSelectable;
    int         normalState;
    int         runState;
    int         attackState;
    int         attackEndState;
    int         maxArmor;
    fixed_t     maxMove;
    fixed_t     forwardMove[2]; // [walk, run].
    fixed_t     sideMove[2]; // [walk, run].
    int         moveMul; // Multiplier for above.
    int         turnSpeed[3]; // [normal, speed, initial]
    int         jumpTics; // Wait in between jumps.
    int         failUseSound; // Sound played when a use fails.
} classinfo_t;

extern classinfo_t classInfo[NUM_PLAYER_CLASSES];

typedef enum {
    SM_NOITEMS = -1, // skill mode 0
    SM_BABY = 0,
    SM_EASY,
    SM_MEDIUM,
    SM_HARD,
    SM_NIGHTMARE,
    NUM_SKILL_MODES
} skillmode_t;

//
// Key cards.
//
typedef enum {
    KT_BLUECARD,
    KT_YELLOWCARD,
    KT_REDCARD,
    KT_BLUESKULL,
    KT_YELLOWSKULL,
    KT_REDSKULL,
    NUM_KEY_TYPES
} keytype_t;

// The defined weapons,
//  including a marker indicating
//  user has not changed weapon.
typedef enum {
    WT_FIRST, // fist
    WT_SECOND, // pistol
    WT_THIRD, // shotgun
    WT_FOURTH, // chaingun
    WT_FIFTH, // missile launcher
    WT_SIXTH, // plasma rifle
    WT_SEVENTH, // bfg
    WT_EIGHTH, // chainsaw
    WT_NINETH, // supershotgun
    NUM_WEAPON_TYPES,

    // No pending weapon change.
    WT_NOCHANGE
} weapontype_t;

#define NUMWEAPLEVELS       2 // DOOM weapons have 1 power level.

// Ammunition types defined.
typedef enum {
    AT_CLIP, // Pistol / chaingun ammo.
    AT_SHELL, // Shotgun / double barreled shotgun.
    AT_CELL, // Plasma rifle, BFG.
    AT_MISSILE, // Missile launcher.
    NUM_AMMO_TYPES,
    AT_NOAMMO // Unlimited for chainsaw / fist.
} ammotype_t;

// Power ups.
typedef enum {
    PT_INVULNERABILITY,
    PT_STRENGTH,
    PT_INVISIBILITY,
    PT_IRONFEET,
    PT_ALLMAP,
    PT_INFRARED,
    PT_FLIGHT,
    NUM_POWER_TYPES
} powertype_t;

/**
 * Power up durations, how many seconds till expiration, assuming TICRATE
 * is 35 ticks/second.
 */
typedef enum {
    INVULNTICS = (30 * TICRATE),
    INVISTICS = (60 * TICRATE),
    INFRATICS = (120 * TICRATE),
    IRONTICS = (60 * TICRATE)
} powerduration_t;

enum { VX, VY, VZ }; // Vertex indices.

enum { CR, CG, CB, CA }; // Color indices.

#define IS_SERVER           (Get(DD_SERVER))
#define IS_CLIENT           (Get(DD_CLIENT))
#define IS_NETGAME          (Get(DD_NETGAME))
#define IS_DEDICATED        (Get(DD_DEDICATED))

#define CVAR(typ, x)        (*((typ) *) Con_GetVariable(x)->ptr)

#define SFXVOLUME           (Get(DD_SFX_VOLUME) / 17)
#define MUSICVOLUME         (Get(DD_MUSIC_VOLUME) / 17)

#define VIEWWINDOWX         (Get(DD_VIEWWINDOW_X))
#define VIEWWINDOWY         (Get(DD_VIEWWINDOW_Y))

// Player taking events, and displaying.
#define CONSOLEPLAYER       (Get(DD_CONSOLEPLAYER))
#define DISPLAYPLAYER       (Get(DD_DISPLAYPLAYER))

#define GAMETIC             (*((timespan_t*) DD_GetVariable(DD_GAMETIC)))
#endif
