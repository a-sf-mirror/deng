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
// DESCRIPTION:  none
//  Implements special effects:
//  Texture animation, height or lighting changes
//   according to adjacent sectors, respective
//   utility functions, etc.
//
//-----------------------------------------------------------------------------

#ifndef __P_SPEC__
#define __P_SPEC__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// Many of the structs here will be directly read from old savegames,
// this'll keep things compatible.
#pragma pack(1)

//
// End-level timer (-TIMER option)
//
extern boolean  levelTimer;
extern int      levelTimeCount;

//      Define values for map objects
#define MO_TELEPORTMAN          14

// at game start
void            P_InitPicAnims(void);

// at map load
void            P_SpawnSpecials(void);

// every tic
void            P_UpdateSpecials(void);

// when needed
boolean         P_UseSpecialLine(mobj_t *thing, line_t *line, int side);

void            P_ShootSpecialLine(mobj_t *thing, line_t *line);

void            P_CrossSpecialLine(int linenum, int side, mobj_t *thing);

void            P_PlayerInSpecialSector(player_t *player);

fixed_t         P_FindLowestFloorSurrounding(sector_t *sec);
fixed_t         P_FindHighestFloorSurrounding(sector_t *sec);

fixed_t         P_FindNextHighestFloor(sector_t *sec, int currentheight);

fixed_t         P_FindLowestCeilingSurrounding(sector_t *sec);
fixed_t         P_FindHighestCeilingSurrounding(sector_t *sec);

int             P_FindSectorFromLineTag(line_t *line, int start);

int             P_FindMinSurroundingLight(sector_t *sector, int max);

sector_t       *getNextSector(line_t *line, sector_t *sec);

//
// SPECIAL
//
int             EV_DoDonut(line_t *line);

//
// P_LIGHTS
//
typedef struct {
    /* Do NOT change these members in any way */
    thinker_t       thinker;
    sector_t       *sector;
    int             count;
    int             maxlight;
    int             minlight;
    /* You can safely add new members after here */

} fireflicker_t;

// size of a fireflicker_t (num of bytes) for backward save game compatibility - DJS
#define SIZE_OF_FIREFLICKER (sizeof(thinker_t) + sizeof(sector_t*)     \
                             + (sizeof(int)*3))

typedef struct {
    /* Do NOT change these members in any way */
    thinker_t       thinker;
    sector_t       *sector;
    int             count;
    int             maxlight;
    int             minlight;
    int             maxtime;
    int             mintime;
    /* You can safely add new members after here */

} lightflash_t;

// size of a lightflash_t (num of bytes) for backward save game compatibility - DJS
#define SIZE_OF_FLASH (sizeof(thinker_t) + sizeof(sector_t*)     \
                       + (sizeof(int)*5))

typedef struct {
    /* Do NOT change these members in any way */
    thinker_t       thinker;
    sector_t       *sector;
    int             count;
    int             minlight;
    int             maxlight;
    int             darktime;
    int             brighttime;
    /* You can safely add new members after here */

} strobe_t;

// size of a strobe_t (num of bytes) for backward save game compatibility - DJS
#define SIZE_OF_STROBE (sizeof(thinker_t) + sizeof(sector_t*)     \
                        + (sizeof(int)*5))

typedef struct {
    /* Do NOT change these members in any way */
    thinker_t       thinker;
    sector_t       *sector;
    int             minlight;
    int             maxlight;
    int             direction;
    /* You can safely add new members after here */

} glow_t;

// size of a glow_t (num of bytes) for backward save game compatibility - DJS
#define SIZE_OF_GLOW (sizeof(thinker_t) + sizeof(sector_t*)     \
                      + (sizeof(int)*3))

#define GLOWSPEED           8
#define STROBEBRIGHT        5
#define FASTDARK            15
#define SLOWDARK            35

void T_FireFlicker(fireflicker_t * flick);
void P_SpawnFireFlicker(sector_t *sector);

void T_LightFlash(lightflash_t * flash);
void P_SpawnLightFlash(sector_t *sector);

void T_StrobeFlash(strobe_t * flash);
void P_SpawnStrobeFlash(sector_t *sector, int fastOrSlow, int inSync);

void EV_StartLightStrobing(line_t *line);
void EV_TurnTagLightsOff(line_t *line);

void EV_LightTurnOn(line_t *line, int bright);

void T_Glow(glow_t * g);
void P_SpawnGlowingLight(sector_t *sector);

//
// P_SWITCH
//
typedef struct {
    char            name1[9];
    char            name2[9];
    short           episode;

} switchlist_t;

typedef enum {
    top,
    middle,
    bottom
} bwhere_e;

typedef struct {
    line_t         *line;
    bwhere_e        where;
    int             btexture;
    int             btimer;
    mobj_t         *soundorg;

} button_t;

 // max # of wall switches in a level
#define MAXSWITCHES     50

 // 4 players, 4 buttons each at once, max.
#define MAXBUTTONS      16

 // 1 second, in ticks.
#define BUTTONTIME      35

extern button_t buttonlist[MAXBUTTONS];

void            P_ChangeSwitchTexture(line_t *line, int useAgain);

void            P_InitSwitchList(void);

//
// P_PLATS
//
typedef enum {
    up,
    down,
    waiting,
    in_stasis
} plat_e;

typedef enum {
    perpetualRaise,
    downWaitUpStay,
    raiseAndChange,
    raiseToNearestAndChange,
    blazeDWUS
} plattype_e;

typedef struct {
    /* Do NOT change these members in any way */
    thinker_t       thinker;
    sector_t       *sector;
    fixed_t         speed;
    fixed_t         low;
    fixed_t         high;
    int             wait;
    int             count;
    plat_e          status;
    plat_e          oldstatus;
    boolean         crush;
    int             tag;
    plattype_e      type;
    /* You can safely add new members after here */

    struct platlist *list;   // killough
} plat_t;

// size of a plat (num of bytes) for backward save game compatibility - DJS
#define SIZE_OF_PLAT (sizeof(thinker_t) + sizeof(sector_t*)     \
                      + (sizeof(fixed_t)*3) + (sizeof(int)*3)   \
                      + (sizeof(plat_e)*2) + sizeof(boolean)    \
                      + sizeof(plattype_e))

// New limit-free plat structure -- killough

typedef struct platlist {
  plat_t *plat;
  struct platlist *next,**prev;
} platlist_t;

#define PLATWAIT        3
#define PLATSPEED       FRACUNIT

extern platlist_t *activeplats;

void            T_PlatRaise(plat_t * plat);

int             EV_DoPlat(line_t *line, plattype_e type, int amount);
int             EV_StopPlat(line_t* line);

void            P_AddActivePlat(plat_t * plat);
void            P_RemoveActivePlat(plat_t * plat);
void            P_RemoveAllActivePlats( void );    // killough
void            P_ActivateInStasis(int tag);

//
// P_DOORS
//
typedef enum {
    normal,
    close30ThenOpen,
    close,
    open,
    raiseIn5Mins,
    blazeRaise,
    blazeOpen,
    blazeClose
} vldoor_e;

typedef struct {
    /* Do NOT change these members in any way */
    thinker_t       thinker;
    vldoor_e        type;
    sector_t       *sector;
    fixed_t         topheight;
    fixed_t         speed;

    // 1 = up, 0 = waiting at top, -1 = down
    int             direction;

    // tics to wait at the top
    int             topwait;
    // (keep in case a door going down is reset)
    // when it reaches 0, start going down
    int             topcountdown;
    /* You can safely add new members after here */

} vldoor_t;

// size of a vldoor_t (num of bytes) for backward save game compatibility - DJS
#define SIZE_OF_DOOR (sizeof(thinker_t) + sizeof(sector_t*)     \
                      + sizeof(vldoor_e) + (sizeof(fixed_t)*2)  \
                      + (sizeof(int)*3))

#define VDOORSPEED      FRACUNIT*2
#define VDOORWAIT       150

void            EV_VerticalDoor(line_t *line, mobj_t *thing);

int             EV_DoDoor(line_t *line, vldoor_e type);

int             EV_DoLockedDoor(line_t *line, vldoor_e type, mobj_t *thing);

void            T_VerticalDoor(vldoor_t * door);
void            P_SpawnDoorCloseIn30(sector_t *sec);

void            P_SpawnDoorRaiseIn5Mins(sector_t *sec, int secnum);

//
// P_CEILNG
//
typedef enum {
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise,
    silentCrushAndRaise
} ceiling_e;

typedef struct {
    /* Do NOT change these members in any way */
    thinker_t       thinker;
    ceiling_e       type;
    sector_t       *sector;
    fixed_t         bottomheight;
    fixed_t         topheight;
    fixed_t         speed;
    boolean         crush;

    // 1 = up, 0 = waiting, -1 = down
    int             direction;

    // ID
    int             tag;
    int             olddirection;
    /* You can safely add new members after here */

    struct ceilinglist *list;   // jff 2/22/98 copied from killough's plats
} ceiling_t;

// size of a ceiling (num of bytes) for backward save game compatibility - DJS
#define SIZE_OF_CEILING (sizeof(thinker_t) + sizeof(ceiling_e)      \
                         + (sizeof(fixed_t)*3) + (sizeof(int)*3)    \
                         + sizeof(sector_t*) + sizeof(boolean))

typedef struct ceilinglist {
    ceiling_t *ceiling;
    struct ceilinglist *next,**prev;
} ceilinglist_t;

#define CEILSPEED       FRACUNIT
#define CEILWAIT        150

extern ceilinglist_t *activeceilings;

int             EV_DoCeiling(line_t *line, ceiling_e type);

void            T_MoveCeiling(ceiling_t * ceiling);
void            P_AddActiveCeiling(ceiling_t * c);
void            P_RemoveActiveCeiling(ceiling_t * c);
void            P_RemoveAllActiveCeilings(void);
int             EV_CeilingCrushStop(line_t *line);
int             P_ActivateInStasisCeiling(line_t *line);

//
// P_FLOOR
//
typedef enum {
    // lower floor to highest surrounding floor
    lowerFloor,

    // lower floor to lowest surrounding floor
    lowerFloorToLowest,

    // lower floor to highest surrounding floor VERY FAST
    turboLower,

    // raise floor to lowest surrounding CEILING
    raiseFloor,

    // raise floor to next highest surrounding floor
    raiseFloorToNearest,

    // raise floor to shortest height texture around it
    raiseToTexture,

    // lower floor to lowest surrounding floor
    //  and change floorpic
    lowerAndChange,

    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,

    // raise to next highest floor, turbo-speed
    raiseFloorTurbo,
    donutRaise,
    raiseFloor512
} floor_e;

typedef enum {
    build8,                        // slowly build by 8
    turbo16                        // quickly build by 16
} stair_e;

typedef struct {
    /* Do NOT change these members in any way */
    thinker_t       thinker;
    floor_e         type;
    boolean         crush;
    sector_t       *sector;
    int             direction;
    int             newspecial;
    short           texture;
    fixed_t         floordestheight;
    fixed_t         speed;
    /* You can safely add new members after here */

} floormove_t;

// size of a floor_t (num of bytes) for backward save game compatibility - DJS
#define SIZE_OF_FLOOR (sizeof(thinker_t) + sizeof(sector_t*)     \
                       + sizeof(floor_e) + sizeof(boolean)       \
                       + (sizeof(int)*2) + sizeof(short)         \
                       + (sizeof(fixed_t)*2))

#define FLOORSPEED      FRACUNIT

typedef enum {
    ok,
    crushed,
    pastdest
} result_e;

result_e        T_MovePlane(sector_t *sector, fixed_t speed, fixed_t dest,
                            boolean crush, int floorOrCeiling, int direction);

int             EV_BuildStairs(line_t *line, stair_e type);

int             EV_DoFloor(line_t *line, floor_e floortype);

void            T_MoveFloor(floormove_t * floor);

//
// P_TELEPT
//
int             EV_Teleport(line_t *line, int side, mobj_t *thing);

#pragma pack()

#endif
