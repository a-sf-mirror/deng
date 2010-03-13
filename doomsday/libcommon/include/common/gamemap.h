/*
 * The Doomsday Engine Project
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBCOMMON_GAMEMAP_H
#define LIBCOMMON_GAMEMAP_H

#include "common.h"

#include <de/Map>

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
    dshort          tid;
#endif
    dfloat          pos[3];
    dangle          angle;
    dint            doomEdNum;
    dint            flags;
#if __JHEXEN__
    dbyte           special;
    dbyte           arg1;
    dbyte           arg2;
    dbyte           arg3;
    dbyte           arg4;
    dbyte           arg5;
#endif
} mapspot_t;

typedef struct {
    dint            plrNum;
    dbyte           entryPoint;
    dfloat          pos[3];
    dangle          angle;
    dint            spawnFlags; // @see mapSpotFlags
} playerstart_t;

typedef struct taglist_s {
    dint            tag;
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
    Sector*         sector;
    dint            type;
    dfloat          height;
} stairqueue_t;

// Global vars for stair building, in a struct for neatness.
typedef struct stairdata_s {
    dfloat          stepDelta;
    dint            direction;
    dfloat          speed;
    Material*       material;
    dint            startDelay;
    dint            startDelayDelta;
    dint            textureChange;
    dfloat          startHeight;
} stairdata_t;
#endif

#if __JHERETIC__
#define MAX_AMBIENT_SFX     8
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
#define SP_floororigheight      planes[0].origHeight
#define SP_ceilorigheight       planes[1].origHeight

// Stair build flags.
#define BL_BUILT            0x1
#define BL_WAS_BUILT        0x2
#define BL_SPREADED         0x4
#endif

typedef struct xsector_s {
    dshort special;
    dshort tag;  

    /// 0 = untraversed, 1,2 = sndlines -1
    dint soundTraversed;

    /// Thing that made a sound (can be NULL).
    Thing* soundTarget;

    /// Stone, metal, heavy, etc...
    dbyte seqType;

    /// Thinker function for reversable actions.
    void* specialData;

    /// Original ID from the archived map format.
    dint origID;

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    /// Used during stair building.
    dbyte blFlags;

    struct {
        dfloat origHeight;
    } planes[2]; // {floor, ceiling}

    dfloat origLight;
    dfloat origRGB[3];

    /// Extended generalized sectors.
    xgsector_t* xg;
#endif
} xsector_t;

typedef struct xlinedef_s {
#if __JHEXEN__
    dbyte special;
    dbyte arg1;
    dbyte arg2;
    dbyte arg3;
    dbyte arg4;
    dbyte arg5;
#else
    dshort special;
    dshort tag;
#endif

    short flags;

    /// Has been rendered at least once and needs to appear in the map, for each player.
    bool mapped[MAXPLAYERS];

    dint validCount;

    /// Original ID from the archived map format.
    dint origID;

#if __JDOOM64__
    dshort useOn;
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    /// Extended generalized lines.
    xgline_t* xg;
#endif
} xlinedef_t;

/**
 * Base class for the game plugins' Map.
 */
class LIBCOMMON_API GameMap : public de::Map
{
public:
    /// Unique map name (e.g., MAP01, E1M1).
    de::String name;

    /// Logical episode number.
    duint episode;

    /// Logical map number.
    duint map;

    /// @c true = we are in the process of setting up this map.
    bool inSetup;

    dint time;
    dint actualTime;

    /// Game tic at map start, for time calculation.
    dint startTic;

    dint _numPlayerStarts;
    playerstart_t* _playerStarts;

    dint _numPlayerDMStarts;
    playerstart_t* _deathmatchStarts;

    duint _numSpawnSpots;
    mapspot_t* _spawnSpots;

#if __JHERETIC__
    dint _maceSpotCount;
    mapspot_t* _maceSpots;

    dint _bossSpotCount;
    mapspot_t* _bossSpots;
#endif

#if __JHERETIC__
    dint const * ambientSfx[MAX_AMBIENT_SFX];
    dint ambientSfxCount;
    struct {
        const dint* ptr;
        dint tics;
        dint volume;
    } ambSfx;
#endif

#if __JHERETIC__ || __JHEXEN__
    Thing lavaInflictor;
#endif

#if __JHEXEN__
    dint _TIDList[MAX_TID_COUNT + 1]; // +1 for termination marker
    Thing* _TIDMobj[MAX_TID_COUNT];
#endif

    xsector_t* _xSectors;
    xlinedef_t* _xLineDefs;

    /// For fast sight rejection.
    dbyte* _rejectMatrix;

    /// For crossed line specials.
    iterlist_t* _spechit;

    /// For surfaces that tick eg wall scrollers.
    iterlist_t* _linespecials;

    taglist_t* _lineTagLists;
    dint numLineTagLists;

    taglist_t* _sectorTagLists;
    dint numSectorTagLists;

    Thing* bodyQueue[BODYQUEUESIZE];
    dint bodyQueueSlot;

#if __JHEXEN__
    Thing* corpseQueue[CORPSEQUEUESIZE];
    dint corpseQueueSlot;
#endif

    struct spawnqueuenode_s* _spawnQueueHead;

#if __JDOOM__ || __JDOOM64__
    bool bossKilled;
#endif

    /// For intermission.
    dint totalKills;
    dint totalItems;
    dint totalSecret;

#if __JDOOM__
    Thing* corpseHit;
    Thing* vileObj;
    Vector3f vileTry;

    /// Global state of boss brain.
    struct {
        Thing** targets;
        dint numTargets;
        dint numTargetsAlloc;
        dint easy;
        dint targetOn;
    } brain;
#endif

public:
    GameMap(const de::String& name, duint episode, duint map);
    ~GameMap();
    
    void load(skillmode_t skill);
    
    void operator << (de::Reader& from);

    /**
     * Execute one game tic.
     */
    void runTick();

#if __JHEXEN__
    /**
     * Initialize all polyobjs.
     */
    void initPolyobjs()
#endif

#if __JHERETIC__
    void addMaceSpot(dfloat x, dfloat y, dangle angle);

    void addBossSpot(dfloat x, dfloat y, dangle angle);
#endif

    void purgeDeferredSpawns();

    /**
     * Deferred Thing spawning until at least @minTics have passed.
     * Spawn behavior is otherwise exactly the same as an immediate spawn.
     */
    void deferSpawnThing(dint minTics, mobjtype_t type,
        const Vector3f& pos, dangle angle, sint spawnFlags,
        void (*callback) (Thing* thing, void* paramaters) = 0, void* paramaters = 0);

    void deferSpawnThing(dint minTics, mobjtype_t type,
        dfloat x, dfloat y, dfloat z, dangle angle, dint spawnFlags,
        void (*callback) (Thing* thing, void* paramaters) = 0, void* paramaters = 0) {
            deferSpawnThing(minTics, type, Vector3f(x, y, z), angle, spawnFlags, callback, paramaters);
    }

    void precache();

    void spawnSpecials();
    void spawnPlayers();

    void spawnPlayer(dint plrNum, playerclass_t pClass, dfloat x, dfloat y, dfloat z,
        dangle angle, dint spawnFlags, bool makeCamera);

    void spawnPlayerDM(dint playernum);

    Thing* spawnThing(mobjtype_t type, dfloat x, dfloat y, dfloat z, dangle angle, dint spawnFlags);
    Thing* spawnThing(mobjtype_t type, const dfloat pos[3], dangle angle, dint spawnFlags);

    void updateSpecials();

    void addPlayerStart(dint defaultPlrNum, duint entryPoint, bool deathmatch, dfloat x,
        dfloat y, dfloat z, dangle angle, dint spawnFlags);

    duint numPlayerStarts(bool deathmatch);
    const playerstart_t* playerStart(duint entryPoint, dint pnum, bool deathmatch);

    void clearPlayerStarts();
    void dealPlayerStarts(duint entryPoint);

#if __JHERETIC__
    void addMaceSpot(dfloat x, dfloat y, dangle angle);
    void addBossSpot(dfloat x, dfloat y, dangle angle);
#endif

#if __JDOOM64__
    void thunder();
#endif

#if __JHEXEN__
    void animateSurfaces();
#endif

    void destroyLineTagLists();
    iterlist_t* iterListForTag(dint tag, bool createNewList);

    void destroySectorTagLists();
    iterlist_t* sectorIterListForTag(dint tag, bool createNewList);

    xlinedef_t* XLineDef(duint idx);
    xsector_t* XSector(duint idx);

    dfloat gravity();

    // @todo Should be private to GameMap.
    iterlist_t* specHits();

private:
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
    dfloat dropoffDelta[2], floorZ;
#endif

#if __JHEXEN__
    stairdata_t stairData;
    stairqueue_t stairQueue[STAIR_QUEUE_SIZE];
    dint stairQueueHead;
    dint stairQueueTail;
#endif

    dfloat tmBBox[4];
    Thing* tmThing;

    /// If @c true, move would be ok if within "tmFloorZ - tmCeilingZ".
    bool floatOk;

    dfloat tmFloorZ;
    dfloat tmCeilingZ;
#if __JHEXEN__
    Material* tmFloorMaterial;
#endif

    bool fellDown; // $dropoff_fix

    /**
     * The following are used to keep track of the linedefs that clip the open
     * height range e.g. PIT_CheckLine. They in turn are used with the &unstuck
     * logic and to prevent missiles from exploding against sky hack walls.
     */
    LineDef* ceilingLine;
    LineDef* floorLine;

    /// Who got hit (or NULL).
    Thing* lineTarget;

    /// $unstuck: blocking linedef.
    LineDef* blockLine;

    dfloat attackRange;

#if __JHEXEN__
    Thing* puffSpawned;
    Thing* blockingThing;
#endif
#if __JHERETIC__ || __JHEXEN__
    mobjtype_t puffType;
    Thing* missileThing;
#endif

    Vector3f tm;
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    dfloat tmHeight;
    LineDef* tmHitLine;
#endif
    dfloat tmDropoffZ;
    dfloat bestSlideFrac, secondSlideFrac;
    LineDef* bestSlideLine, *secondSlideLine;

    Thing* slideThing;

    Vector3f tmMove;
    Thing* shootThing;

    /// Height if not aiming up or down.
    sfloat shootZ;

    dint lineAttackDamage;
    dfloat aimSlope;

    /// Slopes to top and bottom of target
    dfloat topSlope, bottomSlope;

    Thing* useThing;

    Thing* bombSource, *bombSpot;
    dint bombDamage;
    dint bombDistance;

    bool crushChange;
    bool noFit;

    /// Start position for trajectory line checks.
    Vector3f startPos;

    /// End position for trajectory checks.
    Vector3f endPos;

#if __JHEXEN__
    Thing* tsThing;
    bool damageSource;

    /// Generic global onThing...used for landing on pods/players.
    Thing* onThing;

    Thing* puzzleItemUser;
    dint puzzleItemType;
    bool puzzleActivated;
#endif

#if !__JHEXEN__
    /// $unstuck: used to check unsticking.
    dint tmUnstuck;
#endif
};

#endif /* LIBCOMMON_GAMEMAP_H */
