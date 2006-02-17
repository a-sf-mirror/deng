/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Internally used data structures for virtually everything,
 * key definitions, lots of other stuff.
 */

#ifndef __DOOMDEF__
#define __DOOMDEF__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#ifdef WIN32
#pragma warning(disable:4244 4761)
#endif

#include <stdio.h>
#include <string.h>

#include "../doomsday.h"
#include "../dd_api.h"
#include "g_dgl.h"
#include "version.h"
#include "p_ticcmd.h"

#define Set         DD_SetInteger
#define Get         DD_GetInteger

#define CONFIGFILE    GAMENAMETEXT".cfg"
#define DEFSFILE      GAMENAMETEXT"\\"GAMENAMETEXT".ded"
#define DATAPATH      "}Data\\"GAMENAMETEXT"\\"
#define STARTUPWAD    "}Data\\"GAMENAMETEXT"\\"GAMENAMETEXT".wad"

// Verbose messages.
#define VERBOSE(code)   { if(verbose >= 1) { code; } }
#define VERBOSE2(code)  { if(verbose >= 2) { code; } }

extern game_import_t gi;
extern game_export_t gx;

//
// Global parameters/defines.
//

#define mobjinfo    (*gi.mobjinfo)
#define states      (*gi.states)
#define validCount  (*gi.validcount)

// if rangecheck is undefined, most parameter validation debugging code
// will not be compiled
#define RANGECHECK

// Defines suck. C sucks.
// C++ might sucks for OOP, but it sure is a better C.
// So there.
#define SCREENWIDTH  320
//SCREEN_MUL*BASE_WIDTH //320
#define SCREENHEIGHT 200
#define SCREEN_MUL      1

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS      16

// State updates, number of tics / second.
#define TICRATE     35

// The current state of the game: whether we are
// playing, gazing at the intermission screen,
// the game final animation, or a demo.
typedef enum {
    GS_LEVEL,
    GS_INTERMISSION,
    GS_FINALE,
    GS_DEMOSCREEN,
    GS_WAITING,
    GS_INFINE
} gamestate_t;

//
// Difficulty/skill settings/filters.
//

// Skill flags.
#define MTF_EASY        1
#define MTF_NORMAL      2
#define MTF_HARD        4

// Deaf monsters/do not react to sound.
#define MTF_AMBUSH      8

typedef enum {
    sk_baby,
    sk_easy,
    sk_medium,
    sk_hard,
    sk_nightmare
} skill_t;

//
// Key cards.
//
typedef enum {
    key_yellow,
    key_green,
    key_blue,
    NUMKEYS
} keytype_t;

// The defined weapons,
//  including a marker indicating
//  user has not changed weapon.
typedef enum {
    wp_staff,
    wp_goldwand,
    wp_crossbow,
    wp_blaster,
    wp_skullrod,
    wp_phoenixrod,
    wp_mace,
    wp_gauntlets,
    wp_beak,

    NUMWEAPONS,

    // No pending weapon change.
    wp_nochange
} weapontype_t;

// Ammunition types defined.
typedef enum {
    am_goldwand,
    am_crossbow,
    am_blaster,
    am_skullrod,
    am_phoenixrod,
    am_mace,
    NUMAMMO,
    am_noammo                      // Unlimited for staff, gauntlets
} ammotype_t;

#define AMMO_GWND_WIMPY 10
#define AMMO_GWND_HEFTY 50
#define AMMO_CBOW_WIMPY 5
#define AMMO_CBOW_HEFTY 20
#define AMMO_BLSR_WIMPY 10
#define AMMO_BLSR_HEFTY 25
#define AMMO_SKRD_WIMPY 20
#define AMMO_SKRD_HEFTY 100
#define AMMO_PHRD_WIMPY 1
#define AMMO_PHRD_HEFTY 10
#define AMMO_MACE_WIMPY 20
#define AMMO_MACE_HEFTY 100

typedef enum {
    pw_None,
    pw_invulnerability,
    pw_invisibility,
    pw_allmap,
    pw_infrared,
    pw_weaponlevel2,
    pw_flight,
    pw_shield,
    pw_health2,
    NUMPOWERS
} powertype_t;

typedef enum {
    arti_none,
    arti_invulnerability,
    arti_invisibility,
    arti_health,
    arti_superhealth,
    arti_tomeofpower,
    arti_torch,
    arti_firebomb,
    arti_egg,
    arti_fly,
    arti_teleport,
    NUMARTIFACTS
} artitype_t;

#define INVULNTICS (30*35)
#define INVISTICS (60*35)
#define INFRATICS (120*35)
#define IRONTICS (60*35)
#define WPNLEV2TICS (40*35)
#define FLIGHTTICS (60*35)

#define CHICKENTICS (40*35)

#define MESSAGETICS (4*35)
#define BLINKTHRESHOLD (4*32)

enum { VX, VY, VZ };               // Vertex indices.

#define IS_SERVER       Get(DD_SERVER)
#define IS_CLIENT       Get(DD_CLIENT)
#define IS_NETGAME      Get(DD_NETGAME)
#define IS_DEDICATED    Get(DD_DEDICATED)


void            H_IdentifyVersion(void);
char           *G_Get(int id);

void            R_SetViewSize(int blocks, int detail);














#define NUMARTIFCTS 28
#define MAXPLAYERS  16
#define TICSPERSEC  35

// Most damage defined using HITDICE
#define HITDICE(a) ((1+(P_Random()&7))*a)


#define SBARHEIGHT  42             // status bar height at bottom of screen



/*
   ===============================================================================

   GLOBAL VARIABLES

   ===============================================================================
 */

#define TELEFOGHEIGHT (32*FRACUNIT)

#define MAXEVENTS 64

extern fixed_t  finesine[5 * FINEANGLES / 4];
extern fixed_t *finecosine;


extern boolean  shareware;         // true if main WAD is the shareware version
extern boolean  ExtendedWAD;       // true if main WAD is the extended version


extern boolean  cdrom;             // true if cd-rom mode active ("-cdrom")


extern boolean  DebugSound;        // debug flag for displaying sound info

extern int      skytexture;


extern int      viewwidth;
extern int      scaledviewwidth;

/*
   ===============================================================================

   GLOBAL FUNCTIONS

   ===============================================================================
 */

//----------
//BASE LEVEL
//----------

void            D_DoomMain(void);
void            IncThermo(void);
void            InitThermo(int max);
void            tprintf(char *string, int initflag);

// not a globally visible function, just included for source reference
// calls all startup code
// parses command line options
// if not overrided, calls N_AdvanceDemo

void            D_DoomLoop(void);

// not a globally visible function, just included for source reference
// called by D_DoomMain, never exits
// manages timing and IO
// calls all ?_Responder, ?_Ticker, and ?_Drawer functions
// calls I_GetTime, I_StartFrame, and I_StartTic

void            D_PostEvent(event_t *ev);

// called by IO functions when input is detected

void            NetUpdate(void);

// create any new ticcmds and broadcast to other players

void            D_QuitNetGame(void);

// broadcasts special packets to other players to notify of game exit

void            TryRunTics(void);


//---------
//SYSTEM IO
//---------

void            I_StartFrame(void);

// called by D_DoomLoop
// called before processing any tics in a frame (just after displaying a frame)
// time consuming syncronous operations are performed here (joystick reading)
// can call D_PostEvent

void            I_StartTic(void);

// called by D_DoomLoop
// called before processing each tic in a frame
// quick syncronous operations are performed here
// can call D_PostEvent

// asyncronous interrupt functions should maintain private ques that are
// read by the syncronous functions to be converted into events

void            I_Init(void);

// called by D_DoomMain
// determines the hardware configuration and sets up the video mode

void            I_BeginRead(void);
void            I_EndRead(void);

//-----
//PLAY
//-----

void            P_Ticker(void);

// called by C_Ticker
// can call G_PlayerExited
// carries out all thinking of monsters and players

void            P_SetupLevel(int episode, int map, int playermask,
                             skill_t skill);
// called by W_Ticker

void            P_Init(void);

// called by startup code

void            R_InitTranslationTables(void);

// called by startup code

// called by M_Responder

//----
//MISC
//----
#define myargc  Argc()

void            strcatQuoted(char *dest, char *src);

boolean         M_ValidEpisodeMap(int episode, int map);

// returns true if the episode/map combo is valid for the current
// game configuration

void            M_ForceUppercase(char *text);

// Changes a string to uppercase

//int M_Random (void);
// returns a number from 0 to 255
int             P_Random(void);

// as M_Random, but used only by the play simulation

void            M_ClearRandom(void);

// fix randoms for demos

void            M_ClearBox(fixed_t *box);
void            M_AddToBox(fixed_t *box, fixed_t x, fixed_t y);

// bounding box functions

int             M_DrawText(int x, int y, boolean direct, char *string);

//----------------------
// Interlude (IN_lude.c)
//----------------------

extern boolean  intermission;

void            IN_Start(void);
void            IN_Stop(void);
void            IN_Ticker(void);
void            IN_Drawer(void);

extern boolean  chatmodeon;
extern boolean  ultimatemsg;

//----------------------
// STATUS BAR (SB_bar.c)
//----------------------

void            ST_Init(void);
void            ST_Ticker(void);
void            ST_Drawer(int fullscreenmode, boolean refresh);


//-----------------
// MENU (MN_menu.c)
//-----------------

void            MN_Init(void);
void            MN_ActivateMenu(void);
void            MN_DeactivateMenu(void);
boolean         M_Responder(event_t *event);
void            MN_Ticker(void);
void            M_Drawer(void);
void            MN_DrTextA(char *text, int x, int y);
int             MN_TextAWidth(char *text);
void            MN_DrTextB(char *text, int x, int y);
int             MN_TextBWidth(char *text);
void            MN_TextFilter(char *text);
int             MN_FilterChar(int ch);

// Drawing text in the Current State.
void            MN_DrTextA_CS(char *text, int x, int y);
void            MN_DrTextAGreen_CS(char *text, int x, int y);
void            MN_DrTextB_CS(char *text, int x, int y);

#endif
