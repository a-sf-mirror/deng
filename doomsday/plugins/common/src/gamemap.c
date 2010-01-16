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

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "gamemap.h"
#include "p_mapsetup.h"
#include "am_map.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "p_start.h"
#include "p_user.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct spawnqueuenode_s {
    int             startTime;
    int             minTics; // Minimum number of tics before respawn.
    void          (*callback) (mobj_t* mo, void* context);
    void*           context;

    float           pos[3];
    angle_t         angle;
    mobjtype_t      type;
    int             spawnFlags; // MSF_* flags

    struct spawnqueuenode_s* next;
} spawnqueuenode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static spawnqueuenode_t* allocateNode(void)
{
    return Z_Malloc(sizeof(spawnqueuenode_t), PU_STATIC, 0);
}

static void freeNode(gamemap_t* map, spawnqueuenode_t* node)
{
    // Find this node in the spawn queue and unlink it if found.
    if(map->_spawnQueueHead)
    {
        if(map->_spawnQueueHead == node)
        {
            map->_spawnQueueHead = map->_spawnQueueHead->next;
        }
        else
        {
            spawnqueuenode_t* n;

            for(n = map->_spawnQueueHead; n->next; n = n->next)
            {
                if(n->next == node)
                    n->next = n->next->next;
            }
        }
    }

    Z_Free(node);
}

static spawnqueuenode_t* dequeueSpawn(gamemap_t* map)
{
    spawnqueuenode_t* n = map->_spawnQueueHead;

    if(map->_spawnQueueHead)
        map->_spawnQueueHead = map->_spawnQueueHead->next;

    return n;
}

static void emptySpawnQueue(gamemap_t* map)
{
    if(map->_spawnQueueHead)
    {
        spawnqueuenode_t* n;

        while((n = dequeueSpawn(map)))
            freeNode(map, n);
    }

    map->_spawnQueueHead = NULL;
}

static void enqueueSpawn(gamemap_t* map, int minTics, mobjtype_t type,
                         float x, float y, float z, angle_t angle, int spawnFlags,
                         void (*callback) (mobj_t* mo, void* context),
                         void* context)
{
    spawnqueuenode_t* n = allocateNode();

    n->type = type;
    n->pos[VX] = x;
    n->pos[VY] = y;
    n->pos[VZ] = z;
    n->angle = angle;
    n->spawnFlags = spawnFlags;

    n->startTime = map->time;
    n->minTics = minTics;

    n->callback = callback;
    n->context = context;

    if(map->_spawnQueueHead)
    {   // Find the correct insertion point.
        if(map->_spawnQueueHead->next)
        {
            spawnqueuenode_t* l = map->_spawnQueueHead;

            while(l->next &&
                  l->next->minTics - (map->time - l->next->startTime) <= minTics)
                l = l->next;

            n->next = (l->next? l->next : NULL);
            l->next = n;
        }
        else
        {   // After or before the head?
            if(map->_spawnQueueHead->minTics -
               (map->time - map->_spawnQueueHead->startTime) <= minTics)
            {
                n->next = NULL;
                map->_spawnQueueHead->next = n;
            }
            else
            {
                n->next = map->_spawnQueueHead;
                map->_spawnQueueHead = n;
            }
        }
    }
    else
    {
        n->next = NULL;
        map->_spawnQueueHead = n;
    }
}

static mobj_t* doDeferredSpawn(gamemap_t* map)
{
    mobj_t* mo = NULL;

    // Anything due to spawn?
    if(map->_spawnQueueHead &&
       map->time - map->_spawnQueueHead->startTime >= map->_spawnQueueHead->minTics)
    {
        spawnqueuenode_t* n = dequeueSpawn(map);

        // Spawn it.
        if((mo = GameMap_SpawnMobj3fv(map, n->type, n->pos, n->angle, n->spawnFlags)))
        {
            if(n->callback)
                n->callback(mo, n->context);
        }

        freeNode(map, n);
    }

    return mo;
}

/**
 * Called 35 times per second by P_DoTick.
 */
static void doDeferredSpawns(gamemap_t* map)
{
    while(doDeferredSpawn(map));
}

gamemap_t* P_CreateGameMap(const char mapID[9], int episodeNum, int mapNum)
{
    gamemap_t* map = Z_Calloc(sizeof(*map), PU_STATIC, 0);

    memcpy(map->mapID, mapID, sizeof(map->mapID));
    map->episodeNum = episodeNum;
    map->mapNum = mapNum;

    return map;
}

void P_DestroyGameMap(gamemap_t* map)
{
    assert(map);
    if(map->_spechit)
        P_DestroyIterList(map->_spechit);
    map->_spechit = NULL;

    if(map->_linespecials)
        P_DestroyIterList(map->_linespecials);
    map->_linespecials = NULL;

    if(map->_rejectMatrix)
        Z_Free(map->_rejectMatrix);
    map->_rejectMatrix = NULL;

    GameMap_DestroyLineTagLists(map);
    GameMap_DestroySectorTagLists(map);
    Z_Free(map);
}

void GameMap_RunTick(gamemap_t* map)
{
    assert(map);

    map->actualTime++;

    // Pause if in menu and at least one tic has been run.
    if(!IS_NETGAME && (Hu_MenuIsActive() || Hu_IsMessageActive()) &&
       !Get(DD_PLAYBACK) && map->time > 1)
        return;

    DD_RunThinkers();

    GameMap_UpdateSpecials(map);
    doDeferredSpawns(map);

#if __JDOOM64__
    GameMap_Thunder(map);
#endif
#if __JHERETIC__
    GameMap_PlayAmbientSfx(map);
#endif
#if __JHEXEN__
    GameMap_AnimateSurfaces(map);
#endif

    // For par times, among other things.
    map->time++;
}

#if __JHEXEN__
/**
 * Initialize all polyobjects in the current map.
 */
void GameMap_InitPolyobjs(gamemap_t* map)
{
    assert(map);
    {
    uint i;

    Con_Message("GameMap_InitPolyobjs: Initializing polyobjects.\n");

    for(i = 0; i < numpolyobjs; ++i)
    {
        const mapspot_t* spot;
        polyobj_t* po;
        uint j;
        
        po = P_GetPolyobj(i | 0x80000000);

        // Init game-specific properties.
        po->specialData = NULL;

        // Find the mapspot associated with this polyobj.
        j = 0;
        spot = NULL;
        while(j < map->numSpawnSpots && !spot)
        {
            if((map->_spawnSpots[j].doomEdNum == PO_SPAWN_DOOMEDNUM ||
                map->_spawnSpots[j].doomEdNum == PO_SPAWNCRUSH_DOOMEDNUM) &&
               map->_spawnSpots[j].angle == po->tag)
            {   // Polyobj mapspot.
                spot = &map->_spawnSpots[j];
            }
            else
            {
                j++;
            }
        }

        if(spot)
        {
            po->crush = (spot->doomEdNum == PO_SPAWNCRUSH_DOOMEDNUM? 1 : 0);
            P_PolyobjMove(po, -po->pos[VX] + spot->pos[VX],
                              -po->pos[VY] + spot->pos[VY]);
        }
        else
        {
            Con_Message("GameMap_InitPolyobjs: Warning, missing mapspot for poly %i.", i);
        }
    }
    }
}
#endif

/**
 * Deferred mobj spawning until at least @minTics have passed.
 * Spawn behavior is otherwise exactly the same as an immediate spawn, via   * P_SpawnMobj*
 */
void GameMap_DeferSpawnMobj3f(gamemap_t* map, int minTics, mobjtype_t type,
    float x, float y, float z, angle_t angle, int spawnFlags,
    void (*callback) (mobj_t* mo, void* context), void* context)
{
    assert(map);

    if(minTics > 0)
    {
        enqueueSpawn(map, minTics, type, x, y, z, angle, spawnFlags, callback,
                     context);
    }
    else // Spawn immediately.
    {
        mobj_t* mo;

        if((mo = GameMap_SpawnMobj3f(map, type, x, y, z, angle, spawnFlags)))
        {
            if(callback)
                callback(mo, context);
        }
    }
}

void GameMap_DeferSpawnMobj3fv(gamemap_t* map, int minTics, mobjtype_t type,
    const float pos[3], angle_t angle, int spawnFlags,
    void (*callback) (mobj_t* mo, void* context), void* context)
{
    assert(map);

    if(minTics > 0)
    {
        enqueueSpawn(map, minTics, type, pos[VX], pos[VY], pos[VZ], angle,
                     spawnFlags, callback, context);
    }
    else // Spawn immediately.
    {
        mobj_t* mo;

        if((mo = GameMap_SpawnMobj3fv(map, type, pos, angle, spawnFlags)))
        {
            if(callback)
                callback(mo, context);
        }
    }
}

void GameMap_PurgeDeferredSpawns(gamemap_t* map)
{
    assert(map);
    emptySpawnQueue(map);
}

#if __JHERETIC__
void GameMap_AddMaceSpot(gamemap_t* map, float x, float y, angle_t angle)
{
    assert(map);
    {
    mapspot_t* spot;

    map->_maceSpots = Z_Realloc(map->_maceSpots, sizeof(mapspot_t) * ++map->maceSpotCount, PU_MAP);
    spot = &map->_maceSpots[map->maceSpotCount-1];

    spot->pos[VX] = x;
    spot->pos[VY] = y;
    spot->angle = angle;
    }
}

void GameMap_AddBossSpot(gamemap_t* map, float x, float y, angle_t angle)
{
    assert(map);
    {
    mapspot_t* spot;

    map->_bossSpots = Z_Realloc(map->_bossSpots, sizeof(mapspot_t) * ++map->bossSpotCount, PU_MAP);
    spot = &map->_bossSpots[map->bossSpotCount-1];

    spot->pos[VX] = x;
    spot->pos[VY] = y;
    spot->angle = angle;
    }
}
#endif
