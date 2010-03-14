/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * p_telept.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "gamemap.h"
#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_terraintype.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

mobj_t* P_SpawnTeleFog(map_t* map, float x, float y, angle_t angle)
{
    assert(map);
    return GameMap_SpawnMobj3f(map, MT_TFOG, x, y, TELEFOGHEIGHT, angle, MSF_Z_FLOOR);
}

boolean P_Teleport(mobj_t* thing, float x, float y, angle_t angle,
                   boolean spawnFog)
{
    assert(thing);
    {
    map_t* map = Thinker_Map((thinker_t*) thing);
    float oldpos[3], aboveFloor, fogDelta;
    player_t* player;
    uint an;
    angle_t oldAngle;
    mobj_t* fog;

    memcpy(oldpos, thing->pos, sizeof(oldpos));
    aboveFloor = thing->pos[VZ] - thing->floorZ;
    oldAngle = thing->angle;
    if(!P_TeleportMove(thing, x, y, false))
    {
        return false;
    }

    if(thing->player)
    {
        player = thing->player;
        if(player->powers[PT_FLIGHT] && aboveFloor > 0)
        {
            thing->pos[VZ] = thing->floorZ + aboveFloor;
            if(thing->pos[VZ] + thing->height > thing->ceilingZ)
            {
                thing->pos[VZ] = thing->ceilingZ - thing->height;
            }
            player->viewZ = thing->pos[VZ] + player->viewHeight;
        }
        else
        {
            thing->pos[VZ] = thing->floorZ;
            player->viewHeight = (float) cfg.plrViewHeight;
            player->viewHeightDelta = 0;
            player->viewZ = thing->pos[VZ] + player->viewHeight;
            //player->plr->clLookDir = 0; /* $unifiedangles */
            player->plr->lookDir = 0;
        }
        //player->plr->clAngle = angle; /* $unifiedangles */
        player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    }
    else if(thing->flags & MF_MISSILE)
    {
        thing->pos[VZ] = thing->floorZ + aboveFloor;
        if(thing->pos[VZ] + thing->height > thing->ceilingZ)
        {
            thing->pos[VZ] = thing->ceilingZ - thing->height;
        }
    }
    else
    {
        thing->pos[VZ] = thing->floorZ;
    }

    if(spawnFog)
    {
        // Spawn teleport fog at source and destination
        fogDelta = ((thing->flags & MF_MISSILE)? 0 : TELEFOGHEIGHT);
        if((fog = GameMap_SpawnMobj3f(map, MT_TFOG, oldpos[VX], oldpos[VY],
                                oldpos[VZ] + fogDelta, oldAngle + ANG180, 0)))
            S_StartSound(SFX_TELEPT, fog);

        an = angle >> ANGLETOFINESHIFT;

        if((fog = GameMap_SpawnMobj3f(map, MT_TFOG,
                                x + 20 * FIX2FLT(finecosine[an]),
                                y + 20 * FIX2FLT(finesine[an]),
                                thing->pos[VZ] + fogDelta, angle + ANG180, 0)))
            S_StartSound(SFX_TELEPT, fog);
    }

    if(thing->player && !thing->player->powers[PT_WEAPONLEVEL2])
    {   // Freeze player for about .5 sec.
        thing->reactionTime = 18;
    }

    thing->angle = angle;
    if(thing->flags2 & MF2_FLOORCLIP)
    {
        thing->floorClip = 0;

        if(thing->pos[VZ] ==
           DMU_GetFloatp(thing->subsector, DMU_FLOOR_HEIGHT))
        {
            const terraintype_t* tt = P_MobjGetFloorTerrainType(thing);

            if(tt->flags & TTF_FLOORCLIP)
            {
                thing->floorClip = 10;
            }
        }
    }

    if(thing->flags & MF_MISSILE)
    {
        an = angle >> ANGLETOFINESHIFT;
        thing->mom[MX] = thing->info->speed * FIX2FLT(finecosine[angle]);
        thing->mom[MY] = thing->info->speed * FIX2FLT(finesine[angle]);
    }
    else
    {
        thing->mom[MX] = thing->mom[MY] = thing->mom[MZ] = 0;
    }

    P_MobjClearSRVO(thing);
    return true;
    }
}

typedef struct {
    sector_t*           sec;
    mobjtype_t          type;
    mobj_t*             foundMobj;
} findmobjparams_t;

static int findMobj(void* p, void* context)
{
    findmobjparams_t* params = (findmobjparams_t*) context;
    mobj_t* mo = (mobj_t*) p;

    // Must be of the correct type?
    if(params->type >= 0 && params->type != mo->type)
        return true; // Continue iteration.

    // Must be in the specified sector?
    if(params->sec &&
       params->sec != DMU_GetPtrp(mo->subsector, DMU_SECTOR))
        return true; // Continue iteration.

    // Found it!
    params->foundMobj = mo;
    return false; // Stop iteration.
}

static mobj_t* getTeleportDestination(map_t* map, short tag)
{
    IterList* list;

    list = GameMap_SectorIterListForTag(map, tag, false);
    if(list)
    {
        sector_t* sec = NULL;
        findmobjparams_t params;

        params.type = MT_TELEPORTMAN;
        params.foundMobj = NULL;

        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            params.sec = sec;

            if(!Map_IterateThinkers(map, P_MobjThinker, findMobj, &params))
            {   // Found one!
                return params.foundMobj;
            }
        }
    }

    return NULL;
}

boolean EV_Teleport(linedef_t* line, int side, mobj_t* mo, boolean spawnFog)
{
    assert(line);
    assert(mo);
    {
    map_t* map = Thinker_Map((thinker_t*) mo);
    mobj_t* dest;

    if(mo->flags2 & MF2_NOTELEPORT)
        return false;

    // Don't teleport if hit back of line, so you can get out of teleporter.
    if(side == 1)
        return false;

    if((dest = getTeleportDestination(map, P_ToXLine(line)->tag)) != NULL)
    {
        return P_Teleport(mo, dest->pos[VX], dest->pos[VY], dest->angle, spawnFog);
    }

    return false;
    }
}

#if __JHERETIC__ || __JHEXEN__
void P_ArtiTele(player_t* player)
{
    assert(player);
    {
    map_t* map = Thinker_Map((thinker_t*) player->plr->mo);
    const playerstart_t* start;

    // Get a random deathmatch start.
    if((start = GameMap_PlayerStart(map, 0, deathmatch? -1 : 0, deathmatch)))
    {
        P_Teleport(player->plr->mo, start->pos[VX], start->pos[VY],
                   start->angle, true);

#if __JHEXEN__
        if(player->morphTics)
        {   // Teleporting away will undo any morph effects (pig)
            P_UndoPlayerMorph(player);
        }
        //S_StartSound(NULL, SFX_WPNUP); // Full volume laugh
#else
        /*S_StartSound(SFX_WPNUP, NULL); // Full volume laugh
           NetSv_Sound(NULL, SFX_WPNUP, player-players); */
        S_StartSound(SFX_WPNUP, NULL);
#endif
    }
    }
}
#endif
