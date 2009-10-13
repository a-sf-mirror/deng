/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 * p_telept.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include "jdoom64.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_terraintype.h"
#include "p_start.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

mobj_t* P_SpawnTeleFog(float x, float y, angle_t angle)
{
    return P_SpawnMobj3f(MT_TFOG, x, y, TELEFOGHEIGHT, angle, MSF_Z_FLOOR);
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

static mobj_t* getTeleportDestination(short tag)
{
    iterlist_t* list;

    list = P_GetSectorIterListForTag(tag, false);
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

            if(!DD_IterateThinkers(P_MobjThinker, findMobj, &params))
            {   // Found one.
                return params.foundMobj;
            }
        }
    }

    return NULL;
}

int EV_Teleport(linedef_t* line, int side, mobj_t* mo, boolean spawnFog)
{
    mobj_t* dest;

    if(mo->flags2 & MF2_NOTELEPORT)
        return 0;

    // Don't teleport if hit back of line, so you can get out of teleporter.
    if(side == 1)
        return 0;

    if((dest = getTeleportDestination(P_ToXLine(line)->tag)) != NULL)
    {   // A suitable destination has been found.
        mobj_t* fog;
        uint an;
        float oldPos[3];
        float aboveFloor;
        angle_t oldAngle;

        memcpy(oldPos, mo->pos, sizeof(mo->pos));
        oldAngle = mo->angle;
        aboveFloor = mo->pos[VZ] - mo->floorZ;

        if(!P_TeleportMove(mo, dest->pos[VX], dest->pos[VY], false))
            return 0;

        mo->pos[VZ] = mo->floorZ;

        if(spawnFog)
        {
            // Spawn teleport fog at source and destination.
            if((fog = P_SpawnMobj3fv(MT_TFOG, oldPos, oldAngle + ANG180, 0)))
                S_StartSound(SFX_TELEPT, fog);

            an = dest->angle >> ANGLETOFINESHIFT;
            if((fog = P_SpawnMobj3f(MT_TFOG,
                                    dest->pos[VX] + 20 * FIX2FLT(finecosine[an]),
                                    dest->pos[VY] + 20 * FIX2FLT(finesine[an]),
                                    mo->pos[VZ], dest->angle + ANG180, 0)))
            {
                // Emit sound, where?
                S_StartSound(SFX_TELEPT, fog);
            }
        }

        mo->angle = dest->angle;
        if(mo->flags2 & MF2_FLOORCLIP)
        {
            mo->floorClip = 0;

            if(mo->pos[VZ] == DMU_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
            {
                const terraintype_t* tt = P_MobjGetFloorTerrainType(mo);

                if(tt->flags & TTF_FLOORCLIP)
                {
                    mo->floorClip = 10;
                }
            }
        }

        mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

        // Don't move for a bit.
        if(mo->player)
        {
            mo->reactionTime = 18;
            if(mo->player->powers[PT_FLIGHT] && aboveFloor)
            {
                mo->pos[VZ] = mo->floorZ + aboveFloor;
                if(mo->pos[VZ] + mo->height > mo->ceilingZ)
                {
                    mo->pos[VZ] = mo->ceilingZ - mo->height;
                }
            }
            else
            {
                //mo->dPlayer->clLookDir = 0; /* $unifiedangles */
                mo->dPlayer->lookDir = 0;
            }
            mo->dPlayer->viewZ =
                mo->pos[VZ] + mo->dPlayer->viewHeight;

            //mo->dPlayer->clAngle = mo->angle; /* $unifiedangles */
            mo->dPlayer->flags |=
                DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
        }

        return 1;
    }

    return 0;
}

/**
 * d64tc
 * If the given doomed number is a type which fade spawns, this will return
 * the corresponding mobjtype. Else -1.
 *
 * DJS - Added in order to cleanup EV_FadeSpawn() somewhat.
 * \todo This is still far from ideal. An MF*_* flag would be better.
 */
static mobjtype_t isFadeSpawner(int doomEdNum)
{
    typedef struct fadespawner_s {
        int             doomEdNum;
        mobjtype_t      type;
    } fadespawner_t;

    static const fadespawner_t  spawners[] =
    {
        {7575, MT_SHOTGUN},
        {7576, MT_CHAINGUN},
        {7577, MT_SUPERSHOTGUN},
        {7578, MT_MISC27},
        {7579, MT_MISC28},
        {7580, MT_MISC25},
        {7581, MT_MISC11},
        {7582, MT_MISC10},
        {7583, MT_MISC0},
        {7584, MT_MISC1},
        {7585, MT_LASERGUN},
        {7586, MT_LPOWERUP1},
        {7587, MT_LPOWERUP2},
        {7588, MT_LPOWERUP3},
        {7589, MT_MEGA},
        {7590, MT_MISC12},
        {7591, MT_INS},
        {7592, MT_INV},
        {7593, MT_MISC13},
        {7594, MT_MISC2},
        {7595, MT_MISC3},
        {7596, MT_MISC15},
        {7597, MT_MISC16},
        {7598, MT_MISC14},
        {7599, MT_MISC22},
        {7600, MT_MISC23},
        {7601, MT_CLIP},
        {7602, MT_MISC17},
        {7603, MT_MISC18},
        {7604, MT_MISC19},
        {7605, MT_MISC20},
        {7606, MT_MISC21},
        {7607, MT_MISC24},
        {7608, MT_POSSESSED},
        {7609, MT_SHOTGUY},
        {7610, MT_TROOP},
        {7611, MT_NTROOP},
        {7612, MT_SERGEANT},
        {7613, MT_SHADOWS},
        {7615, MT_HEAD},
        {7617, MT_SKULL},
        {7618, MT_PAIN},
        {7619, MT_FATSO},
        {7620, MT_BABY},
        {7621, MT_CYBORG},
        {7622, MT_BITCH},
        {7623, MT_KNIGHT},
        {7624, MT_BRUISER},
        {7625, MT_MISC5},
        {7626, MT_MISC8},
        {7627, MT_MISC4},
        {7628, MT_MISC9},
        {7629, MT_MISC6},
        {7630, MT_MISC7},
        {-1, -1} // Terminator.
    };
    int                 i;

    for(i = 0; spawners[i].doomEdNum; ++i)
        if(spawners[i].doomEdNum == doomEdNum)
            return spawners[i].type;

    return -1;
}

typedef struct {
    sector_t*           sec;
    float               spawnHeight;
} fadespawnparams_t;

static int fadeSpawn(void* p, void* context)
{
    fadespawnparams_t*  params = (fadespawnparams_t*) context;
    mobj_t* origin = (mobj_t*) p;
    mobjtype_t spawntype;

    if(params->sec &&
       params->sec != DMU_GetPtrp(origin->subsector, DMU_SECTOR))
        return true; // Contiue iteration.

    // Only fade spawn origins of a certain type.
    spawntype = isFadeSpawner(origin->info->doomEdNum);
    if(spawntype != -1)
    {
        angle_t an;
        mobj_t* mo;
        float pos[3];

        an = origin->angle >> ANGLETOFINESHIFT;

        memcpy(pos, origin->pos, sizeof(pos));
        pos[VX] += 20 * FIX2FLT(finecosine[an]);
        pos[VY] += 20 * FIX2FLT(finesine[an]);
        pos[VZ] = params->spawnHeight;

        if((mo = P_SpawnMobj3fv(spawntype, pos, origin->angle, 0)))
        {
            mo->translucency = 255;
            mo->spawnFadeTics = 0;
            mo->intFlags |= MIF_FADE;

            // Emit sound, where?
            S_StartSound(SFX_ITMBK, mo);

            if(MOBJINFO[spawntype].flags & MF_COUNTKILL)
                totalKills++;
        }
    }

    return true; // Continue iteration.
}

/**
 * d64tc
 * kaiser - This sets a thing spawn depending on thing type placed in
 *       tagged sector.
 * \todo DJS - This is not a good design. There must be a better way
 *       to do this using a new thing flag (MF_NOTSPAWNONSTART?).
 */
int EV_FadeSpawn(linedef_t* li, mobj_t* mo)
{
    iterlist_t*         list;

    list = P_GetSectorIterListForTag(P_ToXLine(li)->tag, false);
    if(list)
    {
        sector_t*           sec;
        fadespawnparams_t   params;

        params.spawnHeight = mo->pos[VZ];

        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            params.sec = sec;
            DD_IterateThinkers(P_MobjThinker, fadeSpawn, &params);
        }
    }

    return 0;
}

// Bitwise ops (for changeMobjFlags)
typedef enum {
    BW_CLEAR,
    BW_SET,
    BW_XOR
} bitwiseop_t;

typedef struct {
    sector_t*           sec;
    boolean             notPlayers;
    int                 flags;
    bitwiseop_t         op;
} pit_changemobjflagsparams_t;

int PIT_ChangeMobjFlags(void* p, void* context)
{
    pit_changemobjflagsparams_t* params =
        (pit_changemobjflagsparams_t*) context;
    mobj_t* mo = (mobj_t*) p;

    if(params->sec &&
       params->sec != DMU_GetPtrp(mo->subsector, DMU_SECTOR))
        return true; // Continue iteration.

    if(params->notPlayers && mo->player)
        return true; // Continue iteration.

    switch(params->op)
    {
    case BW_CLEAR:
        mo->flags &= ~params->flags;
        break;

    case BW_SET:
        mo->flags |= params->flags;
        break;

    case BW_XOR:
        mo->flags ^= params->flags;
        break;

    default:
        Con_Error("PIT_ChangeMobjFlags: Unknown flag bit op %i\n", params->op);
        break;
    }

    return true; // Continue iteration.
}

/**
 * d64tc
 * kaiser - removes things in tagged sector!
 * DJS - actually, no it doesn't at least not directly.
 *
 * \fixme: It appears the MF_TELEPORT flag has been hijacked.
 */
int EV_FadeAway(linedef_t* line, mobj_t* thing)
{
    sector_t*           sec = NULL;
    iterlist_t*         list;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(list)
    {
        pit_changemobjflagsparams_t params;

        params.flags = MF_TELEPORT;
        params.op = BW_SET;
        params.notPlayers = true;

        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            params.sec = sec;
            DD_IterateThinkers(P_MobjThinker, PIT_ChangeMobjFlags, &params);
        }
    }

    return 0;
}
