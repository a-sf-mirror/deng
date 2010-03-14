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

#include "common/IterList"

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

class MapSpot
{
public:
#if __JHEXEN__
    de::dshort          tid;
#endif
    de::dfloat          pos[3];
    de::dangle          angle;
    de::dint            doomEdNum;
    de::dint            flags;
#if __JHEXEN__
    de::dbyte           special;
    de::dbyte           arg1;
    de::dbyte           arg2;
    de::dbyte           arg3;
    de::dbyte           arg4;
    de::dbyte           arg5;
#endif
};

typedef struct {
    de::dint            plrNum;
    de::dbyte           entryPoint;
    de::dfloat          pos[3];
    de::dangle          angle;
    de::dint            spawnFlags; // @see mapSpotFlags
} playerstart_t;

typedef struct taglist_s {
    de::dint            tag;
    IterList*           list;
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
    de::Sector*         sector;
    de::dint            type;
    de::dfloat          height;
} stairqueue_t;

// Global vars for stair building, in a struct for neatness.
typedef struct stairdata_s {
    de::dfloat          stepDelta;
    de::dint            direction;
    de::dfloat          speed;
    de::Material*       material;
    de::dint            startDelay;
    de::dint            startDelayDelta;
    de::dint            textureChange;
    de::dfloat          startHeight;
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

class XSector
{
public:
    de::dshort special;
    de::dshort tag;  

    /// 0 = untraversed, 1,2 = sndlines -1
    de::dint soundTraversed;

    /// Thing that made a sound (can be NULL).
    de::Thing* soundTarget;

    /// Stone, metal, heavy, etc...
    de::dbyte seqType;

    /// Thinker function for reversable actions.
    void* specialData;

    /// Original ID from the archived map format.
    de::dint origID;

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    /// Used during stair building.
    de::dbyte blFlags;

    struct {
        de::dfloat origHeight;
    } planes[2]; // {floor, ceiling}

    de::dfloat origLight;
    de::dfloat origRGB[3];

    /// Extended generalized sectors.
    xgsector_t* xg;
#endif
};

class XLineDef
{
public:
#if __JHEXEN__
    de::dbyte special;
    de::dbyte arg1;
    de::dbyte arg2;
    de::dbyte arg3;
    de::dbyte arg4;
    de::dbyte arg5;
#else
    de::dshort special;
    de::dshort tag;
#endif

    de::dshort flags;

    /// Has been rendered at least once and needs to appear in the map, for each player.
    bool mapped[MAXPLAYERS];

    de::dint validCount;

    /// Original ID from the archived map format.
    de::dint origID;

#if __JDOOM64__
    de::dshort useOn;
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    /// Extended generalized lines.
    xgline_t* xg;
#endif
};

/**
 * Base class for the game plugins' Map.
 */
class LIBCOMMON_API GameMap : public de::Map
{
public:
    /// Unique map name (e.g., MAP01, E1M1).
    de::String name;

    /// Logical episode number.
    de::duint episode;

    /// Logical map number.
    de::duint map;

    /// @c true = we are in the process of setting up this map.
    bool inSetup;

    de::dint time;
    de::dint actualTime;

    /// Game tic at map start, for time calculation.
    de::dint startTic;

    de::dint _numPlayerStarts;
    playerstart_t* _playerStarts;

    de::dint _numPlayerDMStarts;
    playerstart_t* _deathmatchStarts;

    de::duint _numSpawnSpots;
    MapSpot* _spawnSpots;

#if __JHERETIC__
    de::dint _maceSpotCount;
    MapSpot* _maceSpots;

    de::dint _bossSpotCount;
    MapSpot* _bossSpots;
#endif

#if __JHERETIC__
    de::dint const * ambientSfx[MAX_AMBIENT_SFX];
    de::dint ambientSfxCount;
    struct {
        const de::dint* ptr;
        de::dint tics;
        de::dint volume;
    } ambSfx;
#endif

#if __JHERETIC__ || __JHEXEN__
    de::Thing lavaInflictor;
#endif

#if __JHEXEN__
    de::dint _TIDList[MAX_TID_COUNT + 1]; // +1 for termination marker
    de::Thing* _TIDMobj[MAX_TID_COUNT];
#endif

    XSector* _xSectors;
    XLineDef* _xLineDefs;

    /// For fast sight rejection.
    de::dbyte* _rejectMatrix;

    /// For crossed line specials.
    IterList* _spechit;

    /// For surfaces that tick eg wall scrollers.
    IterList* _linespecials;

    taglist_t* _lineTagLists;
    de::dint numLineTagLists;

    taglist_t* _sectorTagLists;
    de::dint numSectorTagLists;

    de::Thing* bodyQueue[BODYQUEUESIZE];
    de::dint bodyQueueSlot;

#if __JHEXEN__
    de::Thing* corpseQueue[CORPSEQUEUESIZE];
    de::dint corpseQueueSlot;
#endif

    struct spawnqueuenode_s* _spawnQueueHead;

#if __JDOOM__ || __JDOOM64__
    bool bossKilled;
#endif

    /// For intermission.
    de::dint totalKills;
    de::dint totalItems;
    de::dint totalSecret;

#if __JDOOM__
    de::Thing* corpseHit;
    de::Thing* vileObj;
    de::Vector3f vileTry;

    /// Global state of boss brain.
    struct {
        de::Thing** targets;
        de::dint numTargets;
        de::dint numTargetsAlloc;
        de::dint easy;
        de::dint targetOn;
    } brain;
#endif

public:
    GameMap(const de::String& name, de::duint episode, de::duint map);
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
    void addMaceSpot(de::dfloat x, de::dfloat y, de::dangle angle);

    void addBossSpot(de::dfloat x, de::dfloat y, de::dangle angle);
#endif

    void purgeDeferredSpawns();

    /**
     * Deferred de::Thing spawning until at least @minTics have passed.
     * Spawn behavior is otherwise exactly the same as an immediate spawn.
     */
    void deferSpawnThing(de::dint minTics, mobjtype_t type,
        const de::Vector3f& pos, de::dangle angle, sint spawnFlags,
        void (*callback) (de::Thing* thing, void* paramaters) = 0, void* paramaters = 0);

    void deferSpawnThing(de::dint minTics, mobjtype_t type,
        de::dfloat x, de::dfloat y, de::dfloat z, de::dangle angle, de::dint spawnFlags,
        void (*callback) (de::Thing* thing, void* paramaters) = 0, void* paramaters = 0) {
            deferSpawnThing(minTics, type, de::Vector3f(x, y, z), angle, spawnFlags, callback, paramaters);
    }

    void precache();

    void spawnSpecials();
    void spawnPlayers();

    void spawnPlayer(de::dint plrNum, playerclass_t pClass, de::dfloat x, de::dfloat y, de::dfloat z,
        de::dangle angle, de::dint spawnFlags, bool makeCamera);

    void spawnPlayerDM(de::dint playernum);

    de::Thing* spawnThing(mobjtype_t type, de::dfloat x, de::dfloat y, de::dfloat z, de::dangle angle, de::dint spawnFlags);
    de::Thing* spawnThing(mobjtype_t type, const de::Vector3f& pos, de::dangle angle, de::dint spawnFlags);

    void updateSpecials();

    void addPlayerStart(de::dint defaultPlrNum, de::duint entryPoint, bool deathmatch, de::dfloat x,
        de::dfloat y, de::dfloat z, de::dangle angle, de::dint spawnFlags);

    de::duint numPlayerStarts(bool deathmatch);
    const playerstart_t* playerStart(de::duint entryPoint, de::dint pnum, bool deathmatch);

    void clearPlayerStarts();
    void dealPlayerStarts(de::duint entryPoint);

#if __JHERETIC__
    void addMaceSpot(de::dfloat x, de::dfloat y, de::dangle angle);
    void addBossSpot(de::dfloat x, de::dfloat y, de::dangle angle);
#endif

#if __JDOOM64__
    void thunder();
#endif

#if __JHEXEN__
    void animateSurfaces();
#endif

    void destroyLineTagLists();
    IterList* iterListForTag(de::dint tag, bool createNewList);

    void destroySectorTagLists();
    IterList* sectorIterListForTag(de::dint tag, bool createNewList);

    XLineDef* XLineDef(de::duint idx);
    XSector* XSector(de::duint idx);

    de::dfloat gravity();

    // @todo Should be private to GameMap.
    IterList* specHits();

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
    de::dfloat dropoffDelta[2], floorZ;
#endif

#if __JHEXEN__
    stairdata_t stairData;
    stairqueue_t stairQueue[STAIR_QUEUE_SIZE];
    de::dint stairQueueHead;
    de::dint stairQueueTail;
#endif

    de::dfloat tmBBox[4];
    de::Thing* tmThing;

    /// If @c true, move would be ok if within "tmFloorZ - tmCeilingZ".
    bool floatOk;

    de::dfloat tmFloorZ;
    de::dfloat tmCeilingZ;
#if __JHEXEN__
    de::Material* tmFloorMaterial;
#endif

    bool fellDown; // $dropoff_fix

    /**
     * The following are used to keep track of the linedefs that clip the open
     * height range e.g. PIT_CheckLine. They in turn are used with the &unstuck
     * logic and to prevent missiles from exploding against sky hack walls.
     */
    de::LineDef* ceilingLine;
    de::LineDef* floorLine;

    /// Who got hit (or NULL).
    de::Thing* lineTarget;

    /// $unstuck: blocking linedef.
    de::LineDef* blockLine;

    de::dfloat attackRange;

#if __JHEXEN__
    de::Thing* puffSpawned;
    de::Thing* blockingThing;
#endif
#if __JHERETIC__ || __JHEXEN__
    mobjtype_t puffType;
    de::Thing* missileThing;
#endif

    de::Vector3f tm;
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    de::dfloat tmHeight;
    de::LineDef* tmHitLine;
#endif
    de::dfloat tmDropoffZ;
    de::dfloat bestSlideFrac, secondSlideFrac;
    de::LineDef* bestSlideLine, *secondSlideLine;

    de::Thing* slideThing;

    de::Vector3f tmMove;
    de::Thing* shootThing;

    /// Height if not aiming up or down.
    de::dfloat shootZ;

    de::dint lineAttackDamage;
    de::dfloat aimSlope;

    /// Slopes to top and bottom of target
    de::dfloat topSlope, bottomSlope;

    de::Thing* useThing;

    de::Thing* bombSource, *bombSpot;
    de::dint bombDamage;
    de::dint bombDistance;

    bool crushChange;
    bool noFit;

    /// Start position for trajectory line checks.
    de::Vector3f startPos;

    /// End position for trajectory checks.
    de::Vector3f endPos;

#if __JHEXEN__
    de::Thing* tsThing;
    bool damageSource;

    /// Generic global onThing...used for landing on pods/players.
    de::Thing* onThing;

    de::Thing* puzzleItemUser;
    de::dint puzzleItemType;
    bool puzzleActivated;
#endif

#if !__JHEXEN__
    /// $unstuck: used to check unsticking.
    de::dint tmUnstuck;
#endif
};

#endif /* LIBCOMMON_GAMEMAP_H */
