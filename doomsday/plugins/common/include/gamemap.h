/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_GAMEMAP_H
#define LIBCOMMON_GAMEMAP_H

#include "p_iterlist.h"
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
#  include "p_xg.h"
#endif

/**
 * @defGroup mapSpotFlags Map Spot Flags
 * @todo Commonize these flags and introduce translations where needed.
 */
/*{*/
#define MSF_EASY            0x00000001 // Appears in easy skill modes.
#define MSF_MEDIUM          0x00000002 // Appears in medium skill modes.
#define MSF_HARD            0x00000004 // Appears in hard skill modes.

#if __JDOOM__ || __JDOOM64__
# define MSF_DEAF           0x00000008 // Thing is deaf.
#elif __JHERETIC__ || __JHEXEN__
# define MSF_AMBUSH         0x00000008 // Mobj will be deaf spawned deaf.
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
# define MSF_NOTSINGLE      0x00000010 // Appears in multiplayer game modes only.
#elif __JHEXEN__
# define MTF_DORMANT        0x00000010
#endif

#if __JDOOM__ || __JHERETIC__
# define MSF_NOTDM          0x00000020 // (BOOM) Can not be spawned in the Deathmatch gameMode.
# define MSF_NOTCOOP        0x00000040 // (BOOM) Can not be spawned in the Co-op gameMode.
# define MSF_FRIENDLY       0x00000080 // (BOOM) friendly monster.
#elif __JDOOM64__
# define MSF_DONTSPAWNATSTART 0x00000020 // Do not spawn this thing at map start.
# define MSF_SCRIPT_TOUCH   0x00000040 // Mobjs spawned from this spot will envoke a script when touched.
# define MSF_SCRIPT_DEATH   0x00000080 // Mobjs spawned from this spot will envoke a script on death.
# define MSF_SECRET         0x00000100 // A secret (bonus) item.
# define MSF_NOTARGET       0x00000200 // Mobjs spawned from this spot will not target their attacker when hurt.
# define MSF_NOTDM          0x00000400 // Can not be spawned in the Deathmatch gameMode.
# define MSF_NOTCOOP        0x00000800 // Can not be spawned in the Co-op gameMode.
#elif __JHEXEN__
# define MSF_FIGHTER        0x00000020
# define MSF_CLERIC         0x00000040
# define MSF_MAGE           0x00000080
# define MSF_NOTSINGLE      0x00000100
# define MSF_NOTCOOP        0x00000200
# define MSF_NOTDM          0x00000400
// The following are not currently implemented.
# define MSF_SHADOW         0x00000800 // (ZDOOM) Thing is 25% translucent.
# define MSF_INVISIBLE      0x00001000 // (ZDOOM) Makes the thing invisible.
# define MSF_FRIENDLY       0x00002000 // (ZDOOM) Friendly monster.
# define MSF_STILL          0x00004000 // (ZDOOM) Thing stands still (only useful for specific Strife monsters or friendlies).
#endif

// New flags:
#define MSF_Z_FLOOR         0x20000000 // Spawn relative to floor height.
#define MSF_Z_CEIL          0x40000000 // Spawn relative to ceiling height (minus thing height).
#define MSF_Z_RANDOM        0x80000000 // Random point between floor and ceiling.

// Unknown flag mask:
#if __JDOOM__
#define MASK_UNKNOWN_MSF_FLAGS (0xffffffff \
    ^ (MSF_EASY|MSF_MEDIUM|MSF_HARD|MSF_DEAF|MSF_NOTSINGLE|MSF_NOTDM|MSF_NOTCOOP|MSF_FRIENDLY))
#elif __JDOOM64__
#define MASK_UNKNOWN_MSF_FLAGS (0xffffffff \
    ^ (MSF_EASY|MSF_MEDIUM|MSF_HARD|MSF_DEAF|MSF_NOTSINGLE|MSF_DONTSPAWNATSTART|MSF_SCRIPT_TOUCH|MSF_SCRIPT_DEATH|MSF_SECRET|MSF_NOTARGET|MSF_NOTDM|MSF_NOTCOOP))
#elif __JHERETIC__
#define MASK_UNKNOWN_MSF_FLAGS (0xffffffff \
    ^ (MSF_EASY|MSF_MEDIUM|MSF_HARD|MSF_AMBUSH|MSF_NOTSINGLE|MSF_NOTDM|MSF_NOTCOOP|MSF_FRIENDLY))
#elif __JHEXEN__
#define MASK_UNKNOWN_MSF_FLAGS (0xffffffff \
    ^ (MSF_EASY|MTF_NORMAL|MSF_HARD|MSF_AMBUSH|MTF_DORMANT|MSF_FIGHTER|MSF_CLERIC|MSF_MAGE|MSF_GSINGLE|MSF_GCOOP|MSF_GDEATHMATCH|MSF_SHADOW|MSF_INVISIBLE|MSF_FRIENDLY|MSF_STILL))
#endif
/*}*/

typedef struct {
#if __JHEXEN__
    short           tid;
#endif
    float           pos[3];
    angle_t         angle;
    int             doomEdNum;
    int             flags;
#if __JHEXEN__
    byte            special;
    byte            arg1;
    byte            arg2;
    byte            arg3;
    byte            arg4;
    byte            arg5;
#endif
} mapspot_t;

typedef struct {
    int             plrNum;
    byte            entryPoint;
    float           pos[3];
    angle_t         angle;
    int             spawnFlags; // @see mapSpotFlags
} playerstart_t;

typedef struct taglist_s {
    int             tag;
    iterlist_t*     list;
} taglist_t;

#define BODYQUEUESIZE           32

#if __JHEXEN__
#define CORPSEQUEUESIZE         64
#endif

#if __JHEXEN__
#define MAX_TID_COUNT           200
#endif

#if __JHEXEN__
#define STAIR_QUEUE_SIZE        32

typedef struct stairqueue_s {
    sector_t*       sector;
    int             type;
    float           height;
} stairqueue_t;

// Global vars for stair building, in a struct for neatness.
typedef struct stairdata_s {
    float           stepDelta;
    int             direction;
    float           speed;
    material_t*     material;
    int             startDelay;
    int             startDelayDelta;
    int             textureChange;
    float           startHeight;
} stairdata_t;
#endif

#if __JHERETIC__
#define MAX_AMBIENT_SFX     8
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
#define SP_floororigheight      planes[PLN_FLOOR].origHeight
#define SP_ceilorigheight       planes[PLN_CEILING].origHeight

// Stair build flags.
#define BL_BUILT            0x1
#define BL_WAS_BUILT        0x2
#define BL_SPREADED         0x4
#endif

typedef struct xsector_s {
    short           special;
    short           tag;  
    int             soundTraversed; // 0 = untraversed, 1,2 = sndlines -1   
    mobj_t*         soundTarget; // Thing that made a sound (or null)
    byte            seqType; // Stone, metal, heavy, etc...
    void*           specialData; // thinker_t for reversable actions.
    int             origID; // Original ID from the archived map format.

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    byte            blFlags; // Used during stair building.

    struct {
        float       origHeight;
    } planes[2];    // {floor, ceiling}

    float           origLight;
    float           origRGB[3];
    xgsector_t*     xg;
#endif
} xsector_t;

typedef struct xlinedef_s {
#if __JHEXEN__
    byte            special;
    byte            arg1;
    byte            arg2;
    byte            arg3;
    byte            arg4;
    byte            arg5;
#else
    short           special;
    short           tag;
#endif

    short           flags;
    // Has been rendered at least once and needs to appear in the map,
    // for each player.
    boolean         mapped[MAXPLAYERS];
    int             validCount;

    int             origID; // Original ID from the archived map format.

#if __JDOOM64__
    short           useOn;
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    xgline_t*       xg; // Extended generalized lines.
#endif
} xlinedef_t;

typedef struct gamemap_s {
    char            mapID[9];
    int             episodeNum, mapNum;
    boolean         inSetup; // @c true = we are in the process of setting up this map.

    int             time;
    int             actualTime;
    int             startTic; // Game tic at map start, for time calculation.

    int             numPlayerStarts;
    playerstart_t*  _playerStarts;
    int             numPlayerDMStarts;
    playerstart_t*  _deathmatchStarts;
    uint            numSpawnSpots;
    mapspot_t*      _spawnSpots;
#if __JHERETIC__
    int             maceSpotCount;
    mapspot_t*      _maceSpots;
    int             bossSpotCount;
    mapspot_t*      _bossSpots;

    int const *     ambientSfx[MAX_AMBIENT_SFX];
    int             ambientSfxCount;
    struct {
        const int*      ptr;
        int             tics;
        int             volume;
    } ambSfx;
#endif

#if __JHERETIC__ || __JHEXEN__
    mobj_t          lavaInflictor;
#endif

#if __JHEXEN__
    int             _TIDList[MAX_TID_COUNT + 1]; // +1 for termination marker
    mobj_t*         _TIDMobj[MAX_TID_COUNT];
#endif

    xsector_t*      _xSectors;
    xlinedef_t*     _xLineDefs;
    byte*           _rejectMatrix; // For fast sight rejection.

    iterlist_t*     _spechit; // for crossed line specials.
    iterlist_t*     _linespecials; // for surfaces that tick eg wall scrollers.

    taglist_t*      _lineTagLists;
    int             numLineTagLists;

    taglist_t*      _sectorTagLists;
    int             numSectorTagLists;

    mobj_t*         bodyQueue[BODYQUEUESIZE];
    int             bodyQueueSlot;

#if __JHEXEN__
    mobj_t*         corpseQueue[CORPSEQUEUESIZE];
    int             corpseQueueSlot;
#endif

    struct spawnqueuenode_s* _spawnQueueHead;

#if __JDOOM__ || __JDOOM64__
    boolean         bossKilled;
#endif

    // For intermission.
    int             totalKills;
    int             totalItems;
    int             totalSecret;

#if __JDOOM__
    mobj_t*         corpseHit;
    mobj_t*         vileObj;
    float           vileTry[3];

    struct {
        mobj_t**        targets;
        int             numTargets;
        int             numTargetsAlloc;
        int             easy;
        int             targetOn;
    } brain; // Global state of boss brain.
#endif

    /**
     * Vars and structures bellow this point were formerly global. Their
     * purpose varies but all are used by somewhat poorly implemented
     * multi-part algorithms defining input, state and output. As such,
     * they were moved into this class to prevent possible access and/or
     * race issues should we wish to parallelize map AI/thinking.
     *
     * @todo Redesign the associated algorithms which reference these so
     * their internal state is maintained "cleanly".
     */
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    float           dropoffDelta[2], floorZ;
#endif

#if __JHEXEN__
    stairdata_t     stairData;
    stairqueue_t    stairQueue[STAIR_QUEUE_SIZE];
    int             stairQueueHead;
    int             stairQueueTail;
#endif

    float           tmBBox[4];
    mobj_t*         tmThing;

    // If "floatOk" true, move would be ok if within "tmFloorZ - tmCeilingZ".
    boolean         floatOk;

    float           tmFloorZ;
    float           tmCeilingZ;
#if __JHEXEN__
    material_t*     tmFloorMaterial;
#endif

    boolean         fellDown; // $dropoff_fix

    // The following is used to keep track of the linedefs that clip the open
    // height range e.g. PIT_CheckLine. They in turn are used with the &unstuck
    // logic and to prevent missiles from exploding against sky hack walls.
    linedef_t*      ceilingLine;
    linedef_t*      floorLine;

    mobj_t*         lineTarget; // Who got hit (or NULL).
    linedef_t*      blockLine; // $unstuck: blocking linedef

    float           attackRange;

#if __JHEXEN__
    mobj_t*         puffSpawned;
    mobj_t*         blockingMobj;
#endif
#if __JHERETIC__ || __JHEXEN__
    mobjtype_t      puffType;
    mobj_t*         missileMobj;
#endif

    float           tm[3];
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    float           tmHeight;
    linedef_t*      tmHitLine;
#endif
    float           tmDropoffZ;
    float           bestSlideFrac, secondSlideFrac;
    linedef_t*      bestSlideLine, *secondSlideLine;

    mobj_t*         slideMo;

    float           tmMove[3];
    mobj_t*         shootThing;

    // Height if not aiming up or down.
    float           shootZ;

    int             lineAttackDamage;
    float           aimSlope;

    // slopes to top and bottom of target
    float           topSlope, bottomSlope;

    mobj_t*         useThing;

    mobj_t*         bombSource, *bombSpot;
    int             bombDamage;
    int             bombDistance;

    boolean         crushChange;
    boolean         noFit;

    float           startPos[3]; // start position for trajectory line checks
    float           endPos[3]; // end position for trajectory checks

#if __JHEXEN__
    mobj_t*         tsThing;
    boolean         damageSource;
    mobj_t*         onMobj; // generic global onMobj...used for landing on pods/players

    mobj_t*         puzzleItemUser;
    int             puzzleItemType;
    boolean         puzzleActivated;
#endif

#if !__JHEXEN__
    int             tmUnstuck; // $unstuck: used to check unsticking
#endif
} gamemap_t;

gamemap_t*          P_CreateGameMap(const char mapID[9], int episodeNum, int mapNum);
void                P_DestroyGameMap(gamemap_t* map);

boolean             GameMap_Load(gamemap_t* map, skillmode_t skill);
void                GameMap_Precache(gamemap_t* map);

#if __JHEXEN__
void                GameMap_InitPolyobjs(gamemap_t* map);
#endif
void                GameMap_SpawnSpecials(gamemap_t* map);
void                GameMap_SpawnPlayers(gamemap_t* map);
void                GameMap_SpawnPlayer(gamemap_t* map, int plrNum, playerclass_t pClass, float x,
                                        float y, float z, angle_t angle,
                                        int spawnFlags, boolean makeCamera);
void                GameMap_SpawnPlayerDM(gamemap_t* map, int playernum);

mobj_t*             GameMap_SpawnMobj3f(gamemap_t* map, mobjtype_t type, float x, float y, float z, angle_t angle, int spawnFlags);
mobj_t*             GameMap_SpawnMobj3fv(gamemap_t* map, mobjtype_t type, const float pos[3], angle_t angle, int spawnFlags);
void                GameMap_DeferSpawnMobj3f(gamemap_t* map, int minTics, mobjtype_t type, float x, float y, float z, angle_t angle, int spawnFlags,
                                             void (*callback) (mobj_t* mo, void* context), void* context);
void                GameMap_DeferSpawnMobj3fv(gamemap_t* map, int minTics, mobjtype_t type, const float pos[3], angle_t angle, int spawnFlags,
                                              void (*callback) (mobj_t* mo, void* context), void* context);
void                GameMap_PurgeDeferredSpawns(gamemap_t* map);

void                GameMap_RunTick(gamemap_t* map);
void                GameMap_UpdateSpecials(gamemap_t* map);

void                GameMap_AddPlayerStart(gamemap_t* map, int defaultPlrNum, byte entryPoint,
                                           boolean deathmatch, float x, float y,
                                           float z, angle_t angle, int spawnFlags);
uint                GameMap_NumPlayerStarts(gamemap_t* map, boolean deathmatch);
const playerstart_t* GameMap_PlayerStart(gamemap_t* map, byte entryPoint, int pnum, boolean deathmatch);

void                GameMap_ClearPlayerStarts(gamemap_t* map);
void                GameMap_DealPlayerStarts(gamemap_t* map, byte entryPoint);

#if __JHERETIC__
void                GameMap_AddMaceSpot(gamemap_t* map, float x, float y, angle_t angle);
void                GameMap_AddBossSpot(gamemap_t* map, float x, float y, angle_t angle);
#endif

#if __JDOOM64__
void                GameMap_Thunder(gamemap_t* map);
#endif

#if __JHEXEN__
void                GameMap_AnimateSurfaces(gamemap_t* map);
#endif

void                GameMap_DestroyLineTagLists(gamemap_t* map);
iterlist_t*         GameMap_IterListForTag(gamemap_t* map, int tag, boolean createNewList);

void                GameMap_DestroySectorTagLists(gamemap_t* map);
iterlist_t*         GameMap_SectorIterListForTag(gamemap_t* map, int tag, boolean createNewList);

xlinedef_t*         GameMap_XLineDef(gamemap_t* map, uint idx);
xsector_t*          GameMap_XSector(gamemap_t* map, uint idx);

// @todo Should be private to GameMap.
iterlist_t*         GameMap_SpecHits(gamemap_t* map);

#endif /* LIBCOMMON_GAMEMAP_H */
