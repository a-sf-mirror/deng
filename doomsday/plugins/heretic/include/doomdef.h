/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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
 * doomdef.h:
 *
 * Internally used data structures for virtually everything,
 * key definitions, lots of other stuff.
 */

#ifndef __DOOMDEF_H__
#define __DOOMDEF_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#ifdef WIN32
#pragma warning(disable:4244)
#endif

#include <stdio.h>
#include <string.h>

#include "doomsday.h"
#include "dd_api.h"
#include "version.h"

#define Set             DD_SetInteger
#define Get             DD_GetInteger

#define CONFIGFILE      GAMENAMETEXT".cfg"
#define DEFSFILE        GAMENAMETEXT"\\"GAMENAMETEXT".ded"
#define DATAPATH        "}data\\"GAMENAMETEXT"\\"
#define STARTUPWAD      "}data\\"GAMENAMETEXT"\\"GAMENAMETEXT".wad"
#define STARTUPPK3      "}data\\"GAMENAMETEXT"\\"GAMENAMETEXT".pk3"

// Verbose messages.
#define VERBOSE(code)   { if(verbose >= 1) { code; } }
#define VERBOSE2(code)  { if(verbose >= 2) { code; } }

extern game_import_t gi;
extern game_export_t gx;

//
// Global parameters/defines.
//

#define MOBJINFO            (*gi.mobjInfo)
#define STATES              (*gi.states)
#define VALIDCOUNT          (*gi.validCount)

// Game mode handling - identify IWAD version to handle IWAD dependend
// animations etc.
typedef enum {
    shareware, // shareware, E1, M9
    registered, // DOOM 1 registered, E3, M27
    extended, // episodes 4 and 5 present
    indetermined, // Well, no IWAD found.
    NUM_GAME_MODES
} gamemode_t;

// Game mode bits for the above.
#define GM_SHAREWARE        0x1 // shareware, E1, M9
#define GM_REGISTERED       0x2 // registered episodes
#define GM_EXTENDED         0x4 // episodes 4 and 5 present
#define GM_INDETERMINED     0x8 // Well, no IWAD found.

#define GM_ANY              (GM_SHAREWARE|GM_REGISTERED|GM_EXTENDED)
#define GM_NOTSHAREWARE     (GM_REGISTERED|GM_EXTENDED)

#define SCREENWIDTH         320
#define SCREENHEIGHT        200
#define SCREEN_MUL          1

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS          16

// Playsim, core timing rate in cycles per second.
#define TICRATE             35
#define TICSPERSEC          35

/**
 * Player Classes
 */
typedef enum {
    PCLASS_PLAYER,
    PCLASS_CHICKEN,
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
    fixed_t     forwardMove[2]; // walk, run
    fixed_t     sideMove[2]; // walk, run
    int         moveMul; // multiplier for above
    int         turnSpeed[3]; // [normal, speed, initial]
    int         jumpTics; // wait inbetween jumps
    int         failUseSound; // sound played when a use fails.
} classinfo_t;

extern classinfo_t classInfo[NUM_PLAYER_CLASSES];

/**
 * Game state (hi-level).
 *
 * The current state of the game: whether we are playing, gazing at the
 * intermission screen, the game final animation, or a demo.
 */
typedef enum {
    GS_MAP,
    GS_INTERMISSION,
    GS_FINALE,
    GS_DEMOSCREEN,
    GS_WAITING,
    GS_INFINE,
    NUM_GAME_STATES
} gamestate_t;

/**
 * Difficulty/skill settings/filters.
 */
typedef enum {
    SM_BABY,
    SM_EASY,
    SM_MEDIUM,
    SM_HARD,
    SM_NIGHTMARE,
    NUM_SKILL_MODES
} skillmode_t;

/**
 * Keys (as in, keys to lockables).
 */
typedef enum {
    KT_YELLOW,
    KT_GREEN,
    KT_BLUE,
    NUM_KEY_TYPES
} keytype_t;

/**
 * Weapon ids.
 *
 * The defined weapons, including a marker indicating user has not changed
 * weapon.
 */
typedef enum {
    WT_FIRST, // staff / beak
    WT_SECOND, // goldwand / beak
    WT_THIRD, // crossbow / beak
    WT_FOURTH, // blaster / beak
    WT_FIFTH, // skullrod / beak
    WT_SIXTH, // phoenixrod / beak
    WT_SEVENTH, // mace / beak
    WT_EIGHTH, // gauntlets / beak
    NUM_WEAPON_TYPES,

    WT_NOCHANGE // No pending weapon change.
} weapontype_t;

#define NUMWEAPLEVELS       2 // Number of weapon power levels.

/**
 * Ammunition types.
 */
typedef enum {
    AT_CRYSTAL,
    AT_ARROW,
    AT_ORB,
    AT_RUNE,
    AT_FIREORB,
    AT_MSPHERE,
    NUM_AMMO_TYPES,

    AT_NOAMMO // Takes no ammo, used for staff, gauntlets.
} ammotype_t;

#define AMMO_GWND_WIMPY     10
#define AMMO_GWND_HEFTY     50
#define AMMO_CBOW_WIMPY     5
#define AMMO_CBOW_HEFTY     20
#define AMMO_BLSR_WIMPY     10
#define AMMO_BLSR_HEFTY     25
#define AMMO_SKRD_WIMPY     20
#define AMMO_SKRD_HEFTY     100
#define AMMO_PHRD_WIMPY     1
#define AMMO_PHRD_HEFTY     10
#define AMMO_MACE_WIMPY     20
#define AMMO_MACE_HEFTY     100

/**
 * Powers, bestowable upon players only.
 */
typedef enum {
    PT_NONE,
    PT_INVULNERABILITY,
    PT_INVISIBILITY,
    PT_ALLMAP,
    PT_INFRARED,
    PT_WEAPONLEVEL2, // Temporarily boost all owned weapons to level 2.
    PT_FLIGHT,
    PT_SHIELD,
    PT_HEALTH2,
    NUM_POWER_TYPES
} powertype_t;

#define INVULNTICS          (30*TICRATE)
#define INVISTICS           (60*TICRATE)
#define INFRATICS           (120*TICRATE)
#define IRONTICS            (60*TICRATE)
#define WPNLEV2TICS         (40*TICRATE)
#define FLIGHTTICS          (60*TICRATE)
#define CHICKENTICS         (40*TICRATE)

/**
 * Inventory Item Types:
 */
typedef enum {
    IIT_NONE = 0,
    IIT_FIRST = 1,
    IIT_INVULNERABILITY = IIT_FIRST,
    IIT_INVISIBILITY,
    IIT_HEALTH,
    IIT_SUPERHEALTH,
    IIT_TOMBOFPOWER,
    IIT_TORCH,
    IIT_FIREBOMB,
    IIT_EGG,
    IIT_FLY,
    IIT_TELEPORT,
    NUM_INVENTORYITEM_TYPES
} inventoryitemtype_t;

#define MAXINVITEMCOUNT        16

#define BLINKTHRESHOLD      (4*TICRATE)

enum { VX, VY, VZ }; // Vertex indices.

enum { CR, CG, CB, CA }; // Color indices.

#define IS_SERVER           Get(DD_SERVER)
#define IS_CLIENT           Get(DD_CLIENT)
#define IS_NETGAME          Get(DD_NETGAME)
#define IS_DEDICATED        Get(DD_DEDICATED)

#define CVAR(typ, x)        (*(typ*)Con_GetVariable(x)->ptr)

#define SFXVOLUME       (Get(DD_SFX_VOLUME)/17)
#define MUSICVOLUME     (Get(DD_MUSIC_VOLUME)/17)

void        G_IdentifyVersion(void);
int         G_GetInteger(int id);
void       *G_GetVariable(int id);

// Most damage defined using HITDICE
#define HITDICE(a)          ((1+(P_Random()&7))*a)

#define SBARHEIGHT          (42) // status bar height at bottom of screen

#define TELEFOGHEIGHT       (32)

#define MAXEVENTS           (64)

extern fixed_t finesine[5 * FINEANGLES / 4];
extern fixed_t *finecosine;

//----------
//BASE LEVEL
//----------

void            D_DoomMain(void);
void            IncThermo(void);
void            InitThermo(int max);
void            tprintf(char *string, int initflag);

// called by IO functions when input is detected

void            NetUpdate(void);

// create any new ticcmds and broadcast to other players

void            D_QuitNetGame(void);

//-----
//PLAY
//-----

// called by C_Ticker
// can call G_PlayerExited
// carries out all thinking of monsters and players

void            P_SetupMap(int episode, int map, int playermask,
                             skillmode_t skill);
// called by W_Ticker
void            P_Init(void);

// called by startup code
void            R_InitTranslationTables(void);

//----
//MISC
//----
void            strcatQuoted(char *dest, char *src);

boolean         M_ValidEpisodeMap(int episode, int map);

// returns true if the episode/map combo is valid for the current
// game configuration

void            M_ForceUppercase(char *text);

int             M_DrawText(int x, int y, boolean direct, char *string);

//-----------------
// MENU (MN_menu.c)
//-----------------

void            MN_ActivateMenu(void);
void            MN_DeactivateMenu(void);

// Drawing text in the Current State.
void            MN_DrTextA_CS(char *text, int x, int y);
void            MN_DrTextAGreen_CS(char *text, int x, int y);
void            MN_DrTextB_CS(char *text, int x, int y);

#endif
