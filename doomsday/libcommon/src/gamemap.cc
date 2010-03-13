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

//#define __ACTION_LINK_H__

#include "common/GameMap"

/*
extern "C" {
#include "g_common.h"
}
*/

using namespace de;

namespace {
typedef struct spawnqueuenode_s {
    dint            startTime;
    dint            minTics; // Minimum number of tics before respawn.
    void          (*callback) (Object* object, void* context);
    void*           context;

    dfloat          pos[3];
    dangle          angle;
    mobjtype_t      type;
    dint            spawnFlags; // MSF_* flags

    struct spawnqueuenode_s* next;
} spawnqueuenode_t;

spawnqueuenode_t* allocateNode(void)
{
    return Z_Malloc(sizeof(spawnqueuenode_t), PU_STATIC, 0);
}

void freeNode(map_t* map, spawnqueuenode_t* node)
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

spawnqueuenode_t* dequeueSpawn(map_t* map)
{
    spawnqueuenode_t* n = map->_spawnQueueHead;

    if(map->_spawnQueueHead)
        map->_spawnQueueHead = map->_spawnQueueHead->next;

    return n;
}

void emptySpawnQueue(map_t* map)
{
    if(map->_spawnQueueHead)
    {
        spawnqueuenode_t* n;

        while((n = dequeueSpawn(map)))
            freeNode(map, n);
    }

    map->_spawnQueueHead = NULL;
}

void enqueueSpawn(map_t* map, int minTics, mobjtype_t type,
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

mobj_t* doDeferredSpawn(map_t* map)
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
void doDeferredSpawns(map_t* map)
{
    while(doDeferredSpawn(map));
}
}

GameMap::GameMap(const de::String& name, duint episode, duint map)
 : name(name), episode(episode), map(map)
{}

GameMap::~GameMap()
{
    if(_xLineDefs)
        Z_Free(_xLineDefs); _xLineDefs = NULL;

    if(_xSectors)
        Z_Free(_xSectors); _xSectors = NULL;

    if(_spechit)
        P_DestroyIterList(_spechit); _spechit = NULL;

    if(_linespecials)
        P_DestroyIterList(_linespecials); _linespecials = NULL;

    if(_rejectMatrix)
        Z_Free(_rejectMatrix); _rejectMatrix = NULL;

    destroyLineTagLists();
    destroySectorTagLists();
}

void GameMap::load()
{
    Map::load(name);
}

void GameMap::operator << (Reader& from)
{
    /*
    bool wasVoid = isVoid();
    
    Map::operator << (from);
    
    if(wasVoid)
    {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        G_InitNew(SM_MEDIUM, 1, 1);
#endif
    }
    */
}

void GameMap::runTick()
{
    actualTime++;

    // Pause if in menu and at least one tic has been run.
    if(!IS_NETGAME && (Hu_MenuIsActive() || Hu_IsMessageActive()) &&
       !Get(DD_PLAYBACK) && map->time > 1)
        return;

    runThinkers();

    updateSpecials();
    doDeferredSpawns(this);

#if __JDOOM64__
    thunder();
#endif
#if __JHERETIC__
    playAmbientSfx();
#endif
#if __JHEXEN__
    animateSurfaces();
#endif

    // For par times, among other things.
    time++;
}

#if __JHEXEN__
void GameMap::initPolyobjs()
{
    LOG_MESSAGE("GameMap::initPolyobjs: Initializing polyobjects.");

    for(duint i = 0; i < numPolyobjs(); ++i)
    {
        const mapspot_t* spot;
        Polyobj* po;
        duint j;
        
        po = polyobj(i | 0x80000000);

        // Init game-specific properties.
        po->specialData = NULL;

        // Find the mapspot associated with this polyobj.
        j = 0;
        spot = NULL;
        while(j < _numSpawnSpots && !spot)
        {
            if((_spawnSpots[j].doomEdNum == PO_SPAWN_DOOMEDNUM ||
                _spawnSpots[j].doomEdNum == PO_SPAWNCRUSH_DOOMEDNUM) &&
               _spawnSpots[j].angle == po->tag)
            {   // Polyobj mapspot.
                spot = &_spawnSpots[j];
            }
            else
            {
                j++;
            }
        }

        if(spot)
        {
            po->crush = (spot->doomEdNum == PO_SPAWNCRUSH_DOOMEDNUM? 1 : 0);
            po->translate(-po->origin + spot->pos);
        }
        else
        {
            LOG_WARNING("GameMap::initPolyobjs: Warning, missing mapspot for polyobj #")
                << i << ".";
        }
    }
}
#endif

void GameMap::deferSpawnThing(dint minTics, mobjtype_t type,
    const Vector3f& pos, dangle angle, sint spawnFlags,
    void (*callback) (Object* object, void* paramaters), void* paramaters)
{
    if(minTics > 0)
    {
        enqueueSpawn(this, minTics, type, pos.x, pos.y, pos.z, angle,
                     spawnFlags, callback, paramaters);
    }
    else // Spawn immediately.
    {
        Thing* thing;

        if((thing = spawnThing(type, pos, angle, spawnFlags)))
        {
            if(callback)
                callback(thing, paramaters);
        }
    }
}

void GameMap::purgeDeferredSpawns()
{
    emptySpawnQueue(this);
}

#if __JHERETIC__
void GameMap::addMaceSpot(dfloat x, dfloat y, dangle angle)
{
    mapspot_t* spot;

    _maceSpots = Z_Realloc(_maceSpots, sizeof(mapspot_t) * ++_maceSpotCount, PU_MAP);
    spot = &_maceSpots[_maceSpotCount-1];

    spot->pos = Vector2f(x, y);
    spot->angle = angle;
}

void GameMap::addBossSpot(dfloat x, dfloat y, dangle angle)
{
    mapspot_t* spot;

    _bossSpots = Z_Realloc(_bossSpots, sizeof(mapspot_t) * ++_bossSpotCount, PU_MAP);
    spot = &_bossSpots[_bossSpotCount-1];

    spot->pos = Vector2f(x, y);
    spot->angle = angle;
}
#endif
