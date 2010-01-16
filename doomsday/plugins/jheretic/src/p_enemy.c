/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_enemy.c: Enemy thinking, AI.
 *
 * Action Pointer Functions that are associated with states/frames.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#include "jheretic.h"

#include "gamemap.h"
#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_floor.h"

// MACROS ------------------------------------------------------------------

#define MONS_LOOK_RANGE     (20*64)
#define MONS_LOOK_LIMIT     64

#define MNTR_CHARGE_SPEED   (13)

#define MAX_GEN_PODS        16

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

boolean P_TestMobjLocation(mobj_t *mobj);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Eight directional movement speeds.
#define MOVESPEED_DIAGONAL      (0.71716309f)
static const float dirSpeed[8][2] =
{
    {1, 0},
    {MOVESPEED_DIAGONAL, MOVESPEED_DIAGONAL},
    {0, 1},
    {-MOVESPEED_DIAGONAL, MOVESPEED_DIAGONAL},
    {-1, 0},
    {-MOVESPEED_DIAGONAL, -MOVESPEED_DIAGONAL},
    {0, -1},
    {MOVESPEED_DIAGONAL, -MOVESPEED_DIAGONAL}
};
#undef MOVESPEED_DIAGONAL

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * If a monster yells at a player, it will alert other monsters to the
 * player's whereabouts.
 */
void P_NoiseAlert(mobj_t* target, mobj_t* emitter)
{
    VALIDCOUNT++;
    P_RecursiveSound(target, DMU_GetPtrp(emitter->subsector, DMU_SECTOR), 0);
}

boolean P_CheckMeleeRange(mobj_t* actor)
{
    mobj_t* pl;
    float dist, range;

    if(!actor->target)
        return false;

    pl = actor->target;
    dist = P_ApproxDistance(pl->pos[VX] - actor->pos[VX],
                            pl->pos[VY] - actor->pos[VY]);
    if(!cfg.netNoMaxZMonsterMeleeAttack)
    {   // Account for Z height difference.
        if(pl->pos[VZ] > actor->pos[VZ] + actor->height ||
           pl->pos[VZ] + pl->height < actor->pos[VZ])
            return false;
    }

    range = MELEERANGE - 20 + pl->info->radius;
    if(dist >= range)
        return false;

    if(!P_CheckSight(actor, actor->target))
        return false;

    return true;
}

boolean P_CheckMissileRange(mobj_t *actor)
{
    float       dist;

    if(!P_CheckSight(actor, actor->target))
        return false;

    if(actor->flags & MF_JUSTHIT)
    {   // The target just hit the enemy, so fight back!
        actor->flags &= ~MF_JUSTHIT;
        return true;
    }

    if(actor->reactionTime)
        return false; // Don't attack yet

    dist =
        (P_ApproxDistance
         (actor->pos[VX] - actor->target->pos[VX],
          actor->pos[VY] - actor->target->pos[VY])) - 64;

    if(P_GetState(actor->type, SN_MELEE) == S_NULL)
        dist -= 128; // No melee attack, so fire more frequently.

    // Imp's fly attack from far away
    if(actor->type == MT_IMP)
        dist /= 2;

    if(dist > 200)
        dist = 200;

    if(P_Random() < dist)
        return false;

    return true;
}

/**
 * Move in the current direction.
 *
 * @return                  @c false, if the move is blocked.
 */
boolean P_Move(mobj_t* actor, boolean dropoff)
{
    gamemap_t* map = P_CurrentGameMap();
    float pos[2], step[2];
    linedef_t* ld;
    boolean good;

    if(actor->moveDir == DI_NODIR)
        return false;

    if((unsigned) actor->moveDir >= DI_NODIR)
        Con_Error("Weird actor->moveDir!");

    step[VX] = actor->info->speed * dirSpeed[actor->moveDir][VX];
    step[VY] = actor->info->speed * dirSpeed[actor->moveDir][VY];
    pos[VX] = actor->pos[VX] + step[VX];
    pos[VY] = actor->pos[VY] + step[VY];

    // killough $dropoff_fix.
    if(!P_TryMove(actor, pos[VX], pos[VY], dropoff, false))
    {
        // Open any specials.
        if((actor->flags & MF_FLOAT) && map->floatOk)
        {
            // Must adjust height.
            if(actor->pos[VZ] < map->tmFloorZ)
                actor->pos[VZ] += FLOATSPEED;
            else
                actor->pos[VZ] -= FLOATSPEED;

            actor->flags |= MF_INFLOAT;
            return true;
        }

        if(!P_IterListSize(GameMap_SpecHits(map)))
            return false;

        actor->moveDir = DI_NODIR;
        good = false;
        while((ld = P_PopIterList(GameMap_SpecHits(map))) != NULL)
        {
            /**
             * If the special is not a door that can be opened, return false.
             *
             * $unstuck: This is what caused monsters to get stuck in
             * doortracks, because it thought that the monster freed itself
             * by opening a door, even if it was moving towards the
             * doortrack, and not the door itself.
             *
             * If a line blocking the monster is activated, return true 90%
             * of the time. If a line blocking the monster is not activated,
             * but some other line is, return false 90% of the time.
             * A bit of randomness is needed to ensure it's free from
             * lockups, but for most cases, it returns the correct result.
             *
             * Do NOT simply return false 1/4th of the time (causes monsters
             * to back out when they shouldn't, and creates secondary
             * stickiness).
             */

            if(P_ActivateLine(ld, actor, 0, SPAC_USE))
                good |= ld == map->blockLine ? 1 : 2;
        }

        if(!good || cfg.monstersStuckInDoors)
            return good;
        else
            return (P_Random() >= 230) || (good & 1);
    }
    else
    {
        // "servo": movement smoothing
        P_MobjSetSRVO(actor, step[VX], step[VY]);

        actor->flags &= ~MF_INFLOAT;
    }

    // $dropoff_fix: fall more slowly, under gravity, if fellDown==true
    if(!(actor->flags & MF_FLOAT) && !map->fellDown)
    {
        if(actor->pos[VZ] > actor->floorZ)
            P_HitFloor(actor);

        actor->pos[VZ] = actor->floorZ;
    }

    return true;
}

/**
 * Attempts to move actor in its current (ob->moveangle) direction.
 * If blocked by either a wall or an actor returns FALSE.
 * If move is either clear of block only by a door, returns TRUE and sets.
 * If a door is in the way, an OpenDoor call is made to start it opening.
 */
boolean P_TryWalk(mobj_t *actor)
{
    // $dropoff_fix
    if(!P_Move(actor, false))
        return false;

    actor->moveCount = P_Random() & 15;
    return true;
}

static void newChaseDir(mobj_t *actor, float deltaX, float deltaY)
{
    dirtype_t xdir, ydir, tdir;
    dirtype_t olddir = actor->moveDir;
    dirtype_t turnaround = olddir;

    if(turnaround != DI_NODIR)  // find reverse direction
        turnaround ^= 4;

    xdir = (deltaX > 10 ? DI_EAST : deltaX < -10 ? DI_WEST : DI_NODIR);
    ydir = (deltaY < -10 ? DI_SOUTH : deltaY > 10 ? DI_NORTH : DI_NODIR);

    // Try direct route.
    if(xdir != DI_NODIR && ydir != DI_NODIR &&
       turnaround != (actor->moveDir =
                      deltaY < 0 ? deltaX >
                      0 ? DI_SOUTHEAST : DI_SOUTHWEST : deltaX >
                      0 ? DI_NORTHEAST : DI_NORTHWEST) && P_TryWalk(actor))
        return;

    // Try other directions.
    if(P_Random() > 200 || fabs(deltaY) > fabs(deltaX))
    {
        dirtype_t temp = xdir;

        xdir = ydir;
        ydir = temp;
    }

    if((xdir == turnaround ? xdir = DI_NODIR : xdir) != DI_NODIR &&
       (actor->moveDir = xdir, P_TryWalk(actor)))
        return; // Either moved forward or attacked.

    if((ydir == turnaround ? ydir = DI_NODIR : ydir) != DI_NODIR &&
       (actor->moveDir = ydir, P_TryWalk(actor)))
        return;

    // There is no direct path to the player, so pick another direction.
    if(olddir != DI_NODIR && (actor->moveDir = olddir, P_TryWalk(actor)))
        return;

    // Randomly determine direction of search.
    if(P_Random() & 1)
    {
        for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
            if(tdir != turnaround &&
               (actor->moveDir = tdir, P_TryWalk(actor)))
                return;
    }
    else
    {
        for(tdir = DI_SOUTHEAST; tdir != DI_EAST - 1; tdir--)
            if(tdir != turnaround &&
               (actor->moveDir = tdir, P_TryWalk(actor)))
                return;
    }

    if((actor->moveDir = turnaround) != DI_NODIR && !P_TryWalk(actor))
        actor->moveDir = DI_NODIR;
}

/**
 * Monsters try to move away from tall dropoffs.
 *
 * In Doom, they were never allowed to hang over dropoffs, and would remain
 * stuck if involuntarily forced over one. This logic, combined with
 * p_map.c::P_TryMove(), allows monsters to free themselves without making
 * them tend to hang over dropoffs.
 */
static boolean PIT_AvoidDropoff(linedef_t* line, void* data)
{
    gamemap_t* map = P_CurrentGameMap();
    sector_t* backsector = DMU_GetPtrp(line, DMU_BACK_SECTOR);
    float* bbox = DMU_GetPtrp(line, DMU_BOUNDING_BOX);

    if(backsector &&
       map->tmBBox[BOXRIGHT]  > bbox[BOXLEFT] &&
       map->tmBBox[BOXLEFT]   < bbox[BOXRIGHT]  &&
       map->tmBBox[BOXTOP]    > bbox[BOXBOTTOM] && // Linedef must be contacted
       map->tmBBox[BOXBOTTOM] < bbox[BOXTOP]    &&
       DMU_BoxOnLineSide(map->tmBBox, line) == -1)
    {
        sector_t* frontsector = DMU_GetPtrp(line, DMU_FRONT_SECTOR);
        float front = DMU_GetFloatp(frontsector, DMU_FLOOR_HEIGHT);
        float back = DMU_GetFloatp(backsector, DMU_FLOOR_HEIGHT);
        float d1[2];
        angle_t angle;

        DMU_GetFloatpv(line, DMU_DXY, d1);

        // The monster must contact one of the two floors, and the other
        // must be a tall drop off (more than 24).
        if(back == map->floorZ && front < map->floorZ - 24)
        {
            angle = R_PointToAngle2(0, 0, d1[0], d1[1]); // front side drop off
        }
        else
        {
            if(front == map->floorZ && back < map->floorZ - 24)
                angle = R_PointToAngle2(d1[0], d1[1], 0, 0); // back side drop off
            else
                return true;
        }

        // Move away from drop off at a standard speed.
        // Multiple contacted linedefs are cumulative (e.g. hanging over corner)
        map->dropoffDelta[VX] -= FIX2FLT(finesine[angle >> ANGLETOFINESHIFT]) * 32;
        map->dropoffDelta[VY] += FIX2FLT(finecosine[angle >> ANGLETOFINESHIFT]) * 32;
    }
    return true;
}

/**
 * Driver for above
 */
static float P_AvoidDropoff(mobj_t* actor)
{
    gamemap_t* map = P_CurrentGameMap();

    map->floorZ = actor->pos[VZ]; // Remember floor height.
    map->dropoffDelta[VX] = map->dropoffDelta[VY] = 0;

    VALIDCOUNT++;

    // Check lines
    P_MobjLinesIterator(actor, PIT_AvoidDropoff, 0);

    // Non-zero if movement prescribed.
    return !(map->dropoffDelta[VX] == 0 || map->dropoffDelta[VY] == 0);
}

void P_NewChaseDir(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    float delta[2];

    if(!actor->target)
        Con_Error("P_NewChaseDir: called with no target");

    delta[VX] = actor->target->pos[VX] - actor->pos[VX];
    delta[VY] = actor->target->pos[VY] - actor->pos[VY];

    if(actor->floorZ - actor->dropOffZ > 24 &&
       actor->pos[VZ] <= actor->floorZ &&
       !(actor->flags & (MF_DROPOFF | MF_FLOAT)) &&
       !cfg.avoidDropoffs && P_AvoidDropoff(actor))
    {
        // Move away from dropoff.
        newChaseDir(actor, map->dropoffDelta[VX], map->dropoffDelta[VY]);

        // $dropoff_fix
        // If moving away from drop off, set movecount to 1 so that
        // small steps are taken to get monster away from drop off.

        actor->moveCount = 1;
        return;
    }

    newChaseDir(actor, delta[VX], delta[VY]);
    }
}

typedef struct {
    size_t              count;
    size_t              maxTries;
    mobj_t*             notThis;
    mobj_t*             foundMobj;
    float               origin[2];
    float               maxDistance;
    int                 minHealth;
    int                 compFlags;
    boolean             checkLOS;
    byte                randomSkip;
} findmobjparams_t;

static int findMobj(void* p, void* context)
{
    findmobjparams_t*   params = (findmobjparams_t*) context;
    mobj_t*             mo = (mobj_t*) p;

    // Flags requirement?
    if(params->compFlags > 0 && !(mo->flags & params->compFlags))
        return true; // Continue iteration.

    // Minimum health requirement?
    if(params->minHealth > 0 && mo->health < params->minHealth)
        return true; // Continue iteration.

    // Exclude this mobj?
    if(params->notThis && mo == params->notThis)
        return true; // Continue iteration.

    // Out of range?
    if(params->maxDistance > 0 &&
       P_ApproxDistance(params->origin[VX] - mo->pos[VX],
                        params->origin[VY] - mo->pos[VY]) >
       params->maxDistance)
        return true; // Continue iteration.

    // Randomly skip this?
    if(params->randomSkip && P_Random() < params->randomSkip)
        return true; // Continue iteration.

    if(params->maxTries > 0 && params->count++ > params->maxTries)
        return false; // Stop iteration.

    // Out of sight?
    if(params->checkLOS && params->notThis &&
       !P_CheckSight(params->notThis, mo))
        return true; // Continue iteration.

    // Found one!
    params->foundMobj = mo;
    return false; // Stop iteration.
}

boolean P_LookForMonsters(mobj_t* mo)
{
    findmobjparams_t    params;

    if(!P_CheckSight(players[0].plr->mo, mo))
        return false; // Player can't see the monster.

    params.count = 0;
    params.notThis = mo;
    params.origin[VX] = mo->pos[VX];
    params.origin[VY] = mo->pos[VY];
    params.foundMobj = NULL;
    params.maxDistance = MONS_LOOK_RANGE;
    params.maxTries = MONS_LOOK_LIMIT;
    params.minHealth = 1;
    params.compFlags = MF_COUNTKILL;
    params.checkLOS = true;
    params.randomSkip = 16;
    DD_IterateThinkers(P_MobjThinker, findMobj, &params);

    if(params.foundMobj)
    {
        mo->target = params.foundMobj;
        return true;
    }

    return false;
}

/**
 * If allaround is false, only look 180 degrees in front
 * returns true if a player is targeted
 */
boolean P_LookForPlayers(mobj_t* actor, boolean allaround)
{
    int c, stop;
    player_t* player;
    sector_t* sector;
    angle_t an;
    float dist;
    mobj_t* plrmo;
    int playerCount;

    // If in single player and player is dead, look for monsters.
    if(!IS_NETGAME && players[0].health <= 0)
        return P_LookForMonsters(actor);

    for(c = playerCount = 0; c < MAXPLAYERS; c++)
        if(players[c].plr->inGame)
            playerCount++;

    // Are there any players?
    if(!playerCount)
        return false;

    sector = DMU_GetPtrp(actor->subsector, DMU_SECTOR);
    c = 0;
    stop = (actor->lastLook - 1) & 3;
    for(;; actor->lastLook = (actor->lastLook + 1) & 3)
    {
        if(!players[actor->lastLook].plr->inGame)
            continue;

        if(c++ == 2 || actor->lastLook == stop)
            return false;  // Done looking

        player = &players[actor->lastLook];
        plrmo = player->plr->mo;

        // Dead?
        if(player->health <= 0)
            continue;

        // Out of sight?
        if(!P_CheckSight(actor, plrmo))
            continue;

        if(!allaround)
        {
            an = R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                                 plrmo->pos[VX], plrmo->pos[VY]) - actor->angle;
            if(an > ANG90 && an < ANG270)
            {
                dist =
                    P_ApproxDistance(plrmo->pos[VX] - actor->pos[VX],
                                     plrmo->pos[VY] - actor->pos[VY]);
                // if real close, react anyway
                if(dist > MELEERANGE)
                    continue;   // behind back
            }
        }

        // Is player invisible?
        if(plrmo->flags & MF_SHADOW)
        {
            if((P_ApproxDistance(plrmo->pos[VX] - actor->pos[VX],
                                 plrmo->pos[VY] - actor->pos[VY]) >
                2 * MELEERANGE) &&
               P_ApproxDistance(plrmo->mom[MX], plrmo->mom[MY]) < 5)
            {
                // Player is sneaking - can't detect.
                return false;
            }

            if(P_Random() < 225)
            {
                // Player isn't sneaking, but still didn't detect.
                return false;
            }
        }

        actor->target = plrmo;
        return true;
    }
}

/**
 * Stay in state until a player is sighted.
 */
void C_DECL A_Look(mobj_t* actor)
{
    assert(actor);
    {
    mobj_t* targ;
    sector_t* sec;

    // Any shot will wake up
    actor->threshold = 0;
    sec = DMU_GetPtrp(actor->subsector, DMU_SECTOR);
    targ = P_ToXSector(sec)->soundTarget;
    if(targ && (targ->flags & MF_SHOOTABLE))
    {
        actor->target = targ;
        if(actor->flags & MF_AMBUSH)
        {
            if(P_CheckSight(actor, actor->target))
                goto seeyou;
        }
        else
            goto seeyou;
    }

    if(!P_LookForPlayers(actor, false))
        return;

    // go into chase state
  seeyou:
    if(actor->info->seeSound)
    {
        int     sound;

        sound = actor->info->seeSound;
        if(actor->flags2 & MF2_BOSS)
            S_StartSound(sound, NULL);  // Full volume
        else
            S_StartSound(sound, actor);
    }

    P_MobjChangeState(actor, P_GetState(actor->type, SN_SEE));
    }
}

/**
 * Actor has a melee attack, so it tries to close as fast as possible.
 */
void C_DECL A_Chase(mobj_t* actor)
{
    int                 delta;
    statenum_t          state;

    if(actor->reactionTime)
        actor->reactionTime--;

    // Modify target threshold.
    if(actor->threshold)
        actor->threshold--;

    if(gameSkill == SM_NIGHTMARE || cfg.fastMonsters)
    {
        // Monsters move faster in nightmare mode.
        actor->tics -= actor->tics / 2;
        if(actor->tics < 3)
            actor->tics = 3;
    }

    // Turn towards movement direction if not there yet.
    if(actor->moveDir < DI_NODIR)
    {
        actor->angle &= (7 << 29);

        delta = actor->angle - (actor->moveDir << 29);

        if(delta > 0)
            actor->angle -= ANG90 / 2;
        else if(delta < 0)
            actor->angle += ANG90 / 2;
    }

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {
        // Look for a new target.
        if(P_LookForPlayers(actor, true))
            return;  // Got a new target.

        P_MobjChangeState(actor, P_GetState(actor->type, SN_SPAWN));
        return;
    }

    // Don't attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;

        if(gameSkill != SM_NIGHTMARE)
            P_NewChaseDir(actor);
        return;
    }

    // Check for melee attack.
    if((state = P_GetState(actor->type, SN_MELEE)) != S_NULL &&
       P_CheckMeleeRange(actor))
    {
        if(actor->info->attackSound)
            S_StartSound(actor->info->attackSound, actor);

        P_MobjChangeState(actor, state);
        return;
    }

    // Check for missile attack.
    if((state = P_GetState(actor->type, SN_MISSILE)) != S_NULL)
    {
        if(!(gameSkill < SM_NIGHTMARE && actor->moveCount))
        {
            if(P_CheckMissileRange(actor))
            {
                P_MobjChangeState(actor, state);
                actor->flags |= MF_JUSTATTACKED;
                return;
            }
        }
    }

    // Possibly choose another target.
    if(IS_NETGAME && !actor->threshold &&
       !P_CheckSight(actor, actor->target))
    {
        if(P_LookForPlayers(actor, true))
            return; // Got a new target.
    }

    // Chase towards player.
    if(--actor->moveCount < 0 || !P_Move(actor, false))
    {
        P_NewChaseDir(actor);
    }

    // Make active sound.
    if(actor->info->activeSound && P_Random() < 3)
    {
        if(actor->type == MT_WIZARD && P_Random() < 128)
        {
            S_StartSound(actor->info->seeSound, actor);
        }
        else if(actor->type == MT_SORCERER2)
        {
            S_StartSound(actor->info->activeSound, NULL);
        }
        else
        {
            S_StartSound(actor->info->activeSound, actor);
        }
    }
}

void C_DECL A_FaceTarget(mobj_t *actor)
{
    if(!actor->target)
        return;

    actor->turnTime = true; // $visangle-facetarget
    actor->flags &= ~MF_AMBUSH;

    actor->angle =
        R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                        actor->target->pos[VX], actor->target->pos[VY]);

    // Is target a ghost?
    if(actor->target->flags & MF_SHADOW)
    {
        actor->angle += (P_Random() - P_Random()) << 21;
    }
}

void C_DECL A_Pain(mobj_t* actor)
{
    assert(actor);
    if(actor->info->painSound)
        S_StartSound(actor->info->painSound, actor);
}

void C_DECL A_DripBlood(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* mo;
    float pos[3];

    pos[VX] = actor->pos[VX] + FIX2FLT((P_Random() - P_Random()) << 11);
    pos[VY] = actor->pos[VY] + FIX2FLT((P_Random() - P_Random()) << 11);
    pos[VZ] = actor->pos[VZ];

    if((mo = GameMap_SpawnMobj3fv(map, MT_BLOOD, pos, P_Random() << 24, 0)))
    {
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);

        mo->flags2 |= MF2_LOGRAV;
    }
    }
}

void C_DECL A_KnightAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(3), false);
        S_StartSound(SFX_KGTAT2, actor);
        return;
    }

    // Throw axe.
    S_StartSound(actor->info->attackSound, actor);
    if(actor->type == MT_KNIGHTGHOST || P_Random() < 40)
    {
        // Red axe.
        P_SpawnMissile(MT_REDAXE, actor, actor->target, true);
        return;
    }

    // Green axe.
    P_SpawnMissile(MT_KNIGHTAXE, actor, actor->target, true);
}

void C_DECL A_ImpExplode(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* mo;

    if((mo = GameMap_SpawnMobj3fv(map, MT_IMPCHUNK1, actor->pos, P_Random() << 24, 0)))
    {
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MZ] = 9;
    }

    if((mo = GameMap_SpawnMobj3fv(map, MT_IMPCHUNK2, actor->pos, P_Random() << 24, 0)))
    {
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MZ] = 9;
    }

    if(actor->special1 == 666)
        P_MobjChangeState(actor, S_IMP_XCRASH1); // Extreme death crash.
    }
}

void C_DECL A_BeastPuff(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();

    if(P_Random() > 64)
    {
        float pos[3];

        pos[VX] = actor->pos[VX] + FIX2FLT((P_Random() - P_Random()) << 10);
        pos[VY] = actor->pos[VY] + FIX2FLT((P_Random() - P_Random()) << 10);
        pos[VZ] = actor->pos[VZ] + FIX2FLT((P_Random() - P_Random()) << 10);

        GameMap_SpawnMobj3fv(map, MT_PUFFY, pos, P_Random() << 24, 0);
    }
    }
}

void C_DECL A_ImpMeAttack(mobj_t* actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, 5 + (P_Random() & 7), false);
    }
}

void C_DECL A_ImpMsAttack(mobj_t* actor)
{
    mobj_t*             dest;
    uint                an;
    int                 dist;

    if(!actor->target || P_Random() > 64)
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_SEE));
        return;
    }

    dest = actor->target;

    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor->info->attackSound, actor);

    A_FaceTarget(actor);
    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = 12 * FIX2FLT(finecosine[an]);
    actor->mom[MY] = 12 * FIX2FLT(finesine[an]);

    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);
    dist /= 12;
    if(dist < 1)
        dist = 1;

    actor->mom[MZ] = (dest->pos[VZ] + (dest->height /2) - actor->pos[VZ]) / dist;
}

/**
 * Fireball attack of the imp leader.
 */
void C_DECL A_ImpMsAttack2(mobj_t *actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, 5 + (P_Random() & 7), false);
        return;
    }

    P_SpawnMissile(MT_IMPBALL, actor, actor->target, true);
}

void C_DECL A_ImpDeath(mobj_t *actor)
{
    actor->flags &= ~MF_SOLID;
    actor->flags2 |= MF2_FLOORCLIP;

    if(actor->pos[VZ] <= actor->floorZ)
        P_MobjChangeState(actor, S_IMP_CRASH1);
}

void C_DECL A_ImpXDeath1(mobj_t *actor)
{
    actor->flags &= ~MF_SOLID;
    actor->flags |= MF_NOGRAVITY;
    actor->flags2 |= MF2_FLOORCLIP;

    actor->special1 = 666; // Flag the crash routine.
}

void C_DECL A_ImpXDeath2(mobj_t *actor)
{
    actor->flags &= ~MF_NOGRAVITY;

    if(actor->pos[VZ] <= actor->floorZ)
        P_MobjChangeState(actor, S_IMP_CRASH1);
}

/**
 * @return          @c true, if the chicken morphs.
 */
boolean P_UpdateChicken(mobj_t* actor, int tics)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* fog;
    float pos[3];
    mobjtype_t moType;
    mobj_t* mo;
    mobj_t oldChicken;

    actor->special1 -= tics;

    if(actor->special1 > 0)
        return false;

    moType = actor->special2;

    memcpy(pos, actor->pos, sizeof(pos));

    // @fixme Do this properly!
    memcpy(&oldChicken, actor, sizeof(oldChicken));

    if(!(mo = GameMap_SpawnMobj3fv(map, moType, pos, oldChicken.angle, 0)))
        return false;

    P_MobjChangeState(actor, S_FREETARGMOBJ);

    if(P_TestMobjLocation(mo) == false)
    {   // Didn't fit.
        P_MobjRemove(mo, true);

        if((mo = GameMap_SpawnMobj3fv(map, MT_CHICKEN, pos, oldChicken.angle, 0)))
        {
            mo->flags = oldChicken.flags;
            mo->health = oldChicken.health;
            mo->target = oldChicken.target;

            mo->special1 = 5 * TICSPERSEC;  // Next try in 5 seconds.
            mo->special2 = moType;
        }

        return false;
    }

    mo->target = oldChicken.target;

    if((fog = GameMap_SpawnMobj3f(map, MT_TFOG, pos[VX], pos[VY],
                            pos[VZ] + TELEFOGHEIGHT, mo->angle + ANG180, 0)))
        S_StartSound(SFX_TELEPT, fog);

    return true;
    }
}

void C_DECL A_ChicAttack(mobj_t *actor)
{
    if(P_UpdateChicken(actor, 18))
        return;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
        P_DamageMobj(actor->target, actor, actor, 1 + (P_Random() & 1), false);
}

void C_DECL A_ChicLook(mobj_t *actor)
{
    if(P_UpdateChicken(actor, 10))
        return;

    A_Look(actor);
}

void C_DECL A_ChicChase(mobj_t *actor)
{
    if(P_UpdateChicken(actor, 3))
        return;

    A_Chase(actor);
}

void C_DECL A_ChicPain(mobj_t *actor)
{
    if(P_UpdateChicken(actor, 10))
        return;

    S_StartSound(actor->info->painSound, actor);
}

void C_DECL A_Feathers(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    int i, count;
    mobj_t* mo;

    // In Pain?
    if(actor->health > 0)
        count = P_Random() < 32 ? 2 : 1;
    else // Death.
        count = 5 + (P_Random() & 3);

    for(i = 0; i < count; ++i)
    {
        if((mo = GameMap_SpawnMobj3f(map, MT_FEATHER,
                               actor->pos[VX], actor->pos[VY],
                               actor->pos[VZ] + 20, P_Random() << 24, 0)))
        {
            mo->target = actor;

            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MZ] = 1 + FIX2FLT(P_Random() << 9);

            P_MobjChangeState(mo, S_FEATHER1 + (P_Random() & 7));
        }
    }
    }
}

void C_DECL A_MummyAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(2), false);
        S_StartSound(SFX_MUMAT2, actor);
        return;
    }

    S_StartSound(SFX_MUMAT1, actor);
}

/**
 * Mummy leader missile attack.
 */
void C_DECL A_MummyAttack2(mobj_t *actor)
{
    mobj_t *mo;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(2), false);
        return;
    }

    mo = P_SpawnMissile(MT_MUMMYFX1, actor, actor->target, true);

    if(mo != NULL)
        mo->tracer = actor->target;
}

void C_DECL A_MummyFX1Seek(mobj_t* actor)
{
    P_SeekerMissile(actor, ANGLE_1 * 10, ANGLE_1 * 20);
}

void C_DECL A_MummySoul(mobj_t* mummy)
{
    assert(mummy);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* mo;

    if((mo = GameMap_SpawnMobj3f(map, MT_MUMMYSOUL,
                           mummy->pos[VX], mummy->pos[VY], mummy->pos[VZ] + 10,
                           mummy->angle, 0)))
    {
        mo->mom[MZ] = 1;
    }
    }
}

void C_DECL A_Sor1Pain(mobj_t* actor)
{
    actor->special1 = 20; // Number of steps to walk fast.
    A_Pain(actor);
}

void C_DECL A_Sor1Chase(mobj_t *actor)
{
    if(actor->special1)
    {
        actor->special1--;
        actor->tics -= 3;
    }

    A_Chase(actor);
}

/**
 * Sorcerer demon attack.
 */
void C_DECL A_Srcr1Attack(mobj_t *actor)
{
    mobj_t     *mo;
    angle_t     angle;

    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(8), false);
        return;
    }

    if(actor->health > (actor->info->spawnHealth / 3) * 2)
    {
        // Spit one fireball.
        P_SpawnMissile(MT_SRCRFX1, actor, actor->target, true);
    }
    else
    {
        // Spit three fireballs.
        mo = P_SpawnMissile(MT_SRCRFX1, actor, actor->target, true);
        if(mo)
        {
            angle = mo->angle;
            P_SpawnMissileAngle(MT_SRCRFX1, actor, angle - ANGLE_1 * 3, mo->mom[MZ]);
            P_SpawnMissileAngle(MT_SRCRFX1, actor, angle + ANGLE_1 * 3, mo->mom[MZ]);
        }

        if(actor->health < actor->info->spawnHealth / 3)
        {
            // Maybe attack again?
            if(actor->special1)
            {
                // Just attacked, so don't attack again/
                actor->special1 = 0;
            }
            else
            {
                // Set state to attack again/
                actor->special1 = 1;
                P_MobjChangeState(actor, S_SRCR1_ATK4);
            }
        }
    }
}

void C_DECL A_SorcererRise(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* mo;

    actor->flags &= ~MF_SOLID;
    if((mo = GameMap_SpawnMobj3fv(map, MT_SORCERER2, actor->pos, actor->angle, 0)))
    {
        P_MobjChangeState(mo, S_SOR2_RISE1);
        mo->target = actor->target;
    }
    }
}

void P_DSparilTeleport(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();

    // No spots?
    if(map->bossSpotCount > 0)
    {
        const mapspot_t* dest;
        int i, tries;

        i = P_Random();
        tries = map->bossSpotCount;

        do
        {
            dest = &map->_bossSpots[++i % map->bossSpotCount];
            if(P_ApproxDistance(actor->pos[VX] - dest->pos[VX],
                                actor->pos[VY] - dest->pos[VY]) >= 128)
            {   // A suitable teleport destination is available.
                float prevpos[3];
                angle_t oldAngle;

                memcpy(prevpos, actor->pos, sizeof(prevpos));
                oldAngle = actor->angle;

                if(P_TeleportMove(actor, dest->pos[VX], dest->pos[VY], false))
                {
                    mobj_t* mo;

                    if((mo = GameMap_SpawnMobj3fv(map, MT_SOR2TELEFADE, prevpos, oldAngle + ANG180, 0)))
                        S_StartSound(SFX_TELEPT, mo);

                    P_MobjChangeState(actor, S_SOR2_TELE1);
                    actor->pos[VZ] = actor->floorZ;
                    actor->angle = dest->angle;
                    actor->mom[MX] = actor->mom[MY] = actor->mom[MZ] = 0;
                    S_StartSound(SFX_TELEPT, actor);
                }

                return;
            }
        } while(tries-- > 0); // Don't stay here forever.
    }
    }
}

void C_DECL A_Srcr2Decide(mobj_t* actor)
{
    static int chance[] = { 192, 120, 120, 120, 64, 64, 32, 16, 0 };
    gamemap_t* map = P_CurrentGameMap();

    // No spots?
    if(!map->bossSpotCount)
        return;

    if(P_Random() < chance[actor->health / (actor->info->spawnHealth / 8)])
    {
        P_DSparilTeleport(actor);
    }
}

void C_DECL A_Srcr2Attack(mobj_t *actor)
{
    int     chance;

    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, NULL);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(20), false);
        return;
    }

    chance = actor->health < actor->info->spawnHealth / 2 ? 96 : 48;
    if(P_Random() < chance)
    {
        // Wizard spawners.
        P_SpawnMissileAngle(MT_SOR2FX2, actor, actor->angle - ANG45, 1.0f/2);
        P_SpawnMissileAngle(MT_SOR2FX2, actor, actor->angle + ANG45, 1.0f/2);
    }
    else
    {
        // Blue bolt.
        P_SpawnMissile(MT_SOR2FX1, actor, actor->target, true);
    }
}

void C_DECL A_BlueSpark(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    int i;

    for(i = 0; i < 2; ++i)
    {
        mobj_t* mo;

        if((mo = GameMap_SpawnMobj3fv(map, MT_SOR2FXSPARK, actor->pos, P_Random() << 24, 0)))
        {
            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
            mo->mom[MZ] = 1 + FIX2FLT(P_Random() << 8);
        }
    }
    }
}

void C_DECL A_GenWizard(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* mo, *fog;

    if(!(mo = GameMap_SpawnMobj3f(map, MT_WIZARD, actor->pos[VX], actor->pos[VY],
                            actor->pos[VZ] - (MOBJINFO[MT_WIZARD].height / 2),
                            actor->angle, 0)))
        return;

    if(P_TestMobjLocation(mo) == false)
    {   // Didn't fit.
        P_MobjRemove(mo, true);
        return;
    }

    actor->mom[MX] = actor->mom[MY] = actor->mom[MZ] = 0;

    P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH));

    actor->flags &= ~MF_MISSILE;

    if((fog = GameMap_SpawnMobj3fv(map, MT_TFOG, actor->pos, actor->angle + ANG180, 0)))
        S_StartSound(SFX_TELEPT, fog);
    }
}

void C_DECL A_Sor2DthInit(mobj_t* actor)
{
    assert(actor);

    // Set the animation loop counter.
    actor->special1 = 7;

    // Kill monsters early.
    P_Massacre();
}

void C_DECL A_Sor2DthLoop(mobj_t* actor)
{
    assert(actor);

    if(--actor->special1)
    {   // Need to loop.
        P_MobjChangeState(actor, S_SOR2_DIE4);
    }
}

/**
 * D'Sparil Sound Routines.
 */
void C_DECL A_SorZap(mobj_t* actor)
{
    S_StartSound(SFX_SORZAP, NULL);
}

void C_DECL A_SorRise(mobj_t* actor)
{
    S_StartSound(SFX_SORRISE, NULL);
}

void C_DECL A_SorDSph(mobj_t* actor)
{
    S_StartSound(SFX_SORDSPH, NULL);
}

void C_DECL A_SorDExp(mobj_t* actor)
{
    S_StartSound(SFX_SORDEXP, NULL);
}

void C_DECL A_SorDBon(mobj_t* actor)
{
    S_StartSound(SFX_SORDBON, NULL);
}

void C_DECL A_SorSightSnd(mobj_t* actor)
{
    S_StartSound(SFX_SORSIT, NULL);
}

/**
 * Minotaur Melee attack.
 */
void C_DECL A_MinotaurAtk1(mobj_t* actor)
{
    player_t* player;

    if(!actor->target)
        return;

    S_StartSound(SFX_STFPOW, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(4), false);

        if((player = actor->target->player) != NULL)
        {
            // Squish the player.
            player->viewHeightDelta = -16;
        }
    }
}

/**
 * Minotaur : Choose a missile attack.
 */
void C_DECL A_MinotaurDecide(mobj_t* actor)
{
    uint                an;
    mobj_t*             target;
    float               dist;

    target = actor->target;
    if(!target)
        return;

    S_StartSound(SFX_MINSIT, actor);

    dist = P_ApproxDistance(actor->pos[VX] - target->pos[VX],
                            actor->pos[VY] - target->pos[VY]);

    if(target->pos[VZ] + target->height > actor->pos[VZ] &&
       target->pos[VZ] + target->height < actor->pos[VZ] + actor->height &&
       dist < 8 * 64 && dist > 1 * 64 && P_Random() < 150)
    {   // Charge attack.
        // Don't call the state function right away.
        P_SetMobjStateNF(actor, S_MNTR_ATK4_1);
        actor->flags |= MF_SKULLFLY;

        A_FaceTarget(actor);

        an = actor->angle >> ANGLETOFINESHIFT;
        actor->mom[MX] = MNTR_CHARGE_SPEED * FIX2FLT(finecosine[an]);
        actor->mom[MY] = MNTR_CHARGE_SPEED * FIX2FLT(finesine[an]);

        // Charge duration.
        actor->special1 = 35 / 2;
    }
    else if(target->pos[VZ] == target->floorZ && dist < 9 * 64 &&
            P_Random() < 220)
    {
        // Floor fire attack.
        P_MobjChangeState(actor, S_MNTR_ATK3_1);
        actor->special2 = 0;
    }
    else
    {
        // Swing attack.
        A_FaceTarget(actor);
        // NOTE: Don't need to call P_MobjChangeState because the current
        //       state falls through to the swing attack
    }
}

void C_DECL A_MinotaurCharge(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* puff;

    if(actor->special1)
    {
        if((puff = GameMap_SpawnMobj3fv(map, MT_PHOENIXPUFF, actor->pos, P_Random() << 24, 0)))
            puff->mom[MZ] = 2;
        actor->special1--;
        return;
    }

    actor->flags &= ~MF_SKULLFLY;
    P_MobjChangeState(actor, P_GetState(actor->type, SN_SEE));
    }
}

/**
 * Minotaur : Swing attack.
 */
void C_DECL A_MinotaurAtk2(mobj_t* actor)
{
    mobj_t*             mo;

    if(!actor->target)
        return;

    S_StartSound(SFX_MINAT2, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(5), false);
        return;
    }

    mo = P_SpawnMissile(MT_MNTRFX1, actor, actor->target, true);
    if(mo)
    {
        angle_t             angle = mo->angle;
        float               momZ = mo->mom[MZ];

        S_StartSound(SFX_MINAT2, mo);

        P_SpawnMissileAngle(MT_MNTRFX1, actor, angle - (ANG45 / 8), momZ);
        P_SpawnMissileAngle(MT_MNTRFX1, actor, angle + (ANG45 / 8), momZ);
        P_SpawnMissileAngle(MT_MNTRFX1, actor, angle - (ANG45 / 16), momZ);
        P_SpawnMissileAngle(MT_MNTRFX1, actor, angle + (ANG45 / 16), momZ);
    }
}

/**
 * Minotaur : Floor fire attack.
 */
void C_DECL A_MinotaurAtk3(mobj_t* actor)
{

    player_t*           player;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(5), false);

        if((player = actor->target->player) != NULL)
        {
            // Squish the player.
            player->viewHeightDelta = -16;
        }
    }
    else
    {
        mobj_t*             mo;
        boolean             fixFloorFire = (!cfg.fixFloorFire &&
            actor->floorClip > 0);

        /**
         * Original Heretic bug:
         * When an attempt is made to spawn MT_MNTRFX2 (the Maulotaur's
         * ground flame) the z coordinate is set to ONFLOORZ but if the
         * Maulotaur's feet are currently clipped (i.e., it is in a sector
         * whose terrain info is set to clip) then FOOTCLIPSIZE is
         * subtracted from the z coordinate. So when P_SpawnMobj is called,
         * z != ONFLOORZ, so rather than being set to the height of the
         * floor it is left at 2146838915 (float: 32758.162).
         *
         * This in turn means that when P_TryMove is called (via
         * P_CheckMissileSpawn), the test which is there to check whether a
         * missile hits an upper sidedef section will return true
         * (ceilingheight - thingz > thingheight).
         *
         * This results in P_ExplodeMissile being called instantly.
         *
         * jHeretic fixes this bug, however we maintain original behaviour
         * using the following method:
         *
         * 1) Do not call P_CheckMissileSpawn from P_SpawnMissile.
         * 2) Use special-case logic here which behaves similarly.
         */

        if((mo = P_SpawnMissile(MT_MNTRFX2, actor, actor->target,
                                (fixFloorFire? false : true))))
        {
            if(fixFloorFire)
            {
                mo->pos[VX] += mo->mom[MX] / 2;
                mo->pos[VY] += mo->mom[MY] / 2;
                mo->pos[VZ] += mo->mom[MZ] / 2;

                P_ExplodeMissile(mo);
            }
            else
            {
                S_StartSound(SFX_MINAT1, mo);
            }
        }
    }

    if(P_Random() < 192 && actor->special2 == 0)
    {
        P_MobjChangeState(actor, S_MNTR_ATK3_4);
        actor->special2 = 1;
    }
}

void C_DECL A_MntrFloorFire(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* mo;
    float pos[3];
    angle_t angle;

    // Make sure we are on the floor.
    actor->pos[VZ] = actor->floorZ;

    pos[VX] = actor->pos[VX];
    pos[VY] = actor->pos[VY];
    pos[VZ] = 0;

    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 10);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 10);

    angle = R_PointToAngle2(actor->pos[VX], actor->pos[VY], pos[VX], pos[VY]);

    if((mo = GameMap_SpawnMobj3fv(map, MT_MNTRFX3, pos, angle, MSF_Z_FLOOR)))
    {
        mo->target = actor->target;
        mo->mom[MX] = FIX2FLT(1); // Force block checking.

        P_CheckMissileSpawn(mo);
    }
    }
}

void C_DECL A_BeastAttack(mobj_t* actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(3), false);
        return;
    }

    P_SpawnMissile(MT_BEASTBALL, actor, actor->target, true);
}

void C_DECL A_HeadAttack(mobj_t* actor)
{
    assert(actor);
    {
    static const int atkResolve1[] = { 50, 150 };
    static const int atkResolve2[] = { 150, 200 };

    gamemap_t* map = P_CurrentGameMap();
    mobj_t* fire, *baseFire, *mo, *target;
    int randAttack;
    float dist;
    int i;

    // Ice ball     (close 20% : far 60%)
    // Fire column  (close 40% : far 20%)
    // Whirlwind    (close 40% : far 20%)
    // Distance threshold = 8 cells

    target = actor->target;
    if(target == NULL)
        return;

    A_FaceTarget(actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(target, actor, actor, HITDICE(6), false);
        return;
    }

    dist = P_ApproxDistance(actor->pos[VX] - target->pos[VX],
                            actor->pos[VY] - target->pos[VY]) > 8 * 64;

    randAttack = P_Random();
    if(randAttack < atkResolve1[(FLT2FIX(dist) != 0)? 1 : 0])
    {
        // Ice ball
        P_SpawnMissile(MT_HEADFX1, actor, target, true);
        S_StartSound(SFX_HEDAT2, actor);
    }
    else if(randAttack < atkResolve2[(FLT2FIX(dist) != 0)? 1 : 0])
    {
        // Fire column
        baseFire = P_SpawnMissile(MT_HEADFX3, actor, target, true);
        if(baseFire != NULL)
        {
            P_MobjChangeState(baseFire, S_HEADFX3_4);  // Don't grow
            for(i = 0; i < 5; ++i)
            {
                if((fire = GameMap_SpawnMobj3fv(map, MT_HEADFX3, baseFire->pos, baseFire->angle, 0)))
                {
                    if(i == 0)
                        S_StartSound(SFX_HEDAT1, actor);

                    fire->target = baseFire->target;
                    fire->mom[MX] = baseFire->mom[MX];
                    fire->mom[MY] = baseFire->mom[MY];
                    fire->mom[MZ] = baseFire->mom[MZ];
                    fire->damage = 0;
                    fire->special3 = (i + 1) * 2;

                    P_CheckMissileSpawn(fire);
                }
            }
        }
    }
    else
    {
        // Whirlwind.
        if((mo = P_SpawnMissile(MT_WHIRLWIND, actor, target, true)))
        {
            mo->pos[VZ] -= 32;
            mo->tracer = target;
            mo->special1 = 60;
            mo->special2 = 50; // Timer for active sound.
            mo->special3 = 20 * TICSPERSEC; // Duration.

            S_StartSound(SFX_HEDAT3, actor);
        }
    }
    }
}

void C_DECL A_WhirlwindSeek(mobj_t *actor)
{
    actor->special3 -= 3;
    if(actor->special3 < 0)
    {
        actor->mom[MX] = actor->mom[MY] = actor->mom[MZ] = 0;
        P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH));
        actor->flags &= ~MF_MISSILE;
        return;
    }

    if((actor->special2 -= 3) < 0)
    {
        actor->special2 = 58 + (P_Random() & 31);
        S_StartSound(SFX_HEDAT3, actor);
    }

    if(actor->tracer && actor->tracer->flags & MF_SHADOW)
        return;

    P_SeekerMissile(actor, ANGLE_1 * 10, ANGLE_1 * 30);
}

void C_DECL A_HeadIceImpact(mobj_t* ice)
{
    assert(ice);
    {
    gamemap_t* map = P_CurrentGameMap();
    int i;

    for(i = 0; i < 8; ++i)
    {
        mobj_t* shard;
        angle_t angle = i * ANG45;

        if((shard = GameMap_SpawnMobj3fv(map, MT_HEADFX2, ice->pos, angle, 0)))
        {
            unsigned int an = angle >> ANGLETOFINESHIFT;

            shard->target = ice->target;
            shard->mom[MX] = shard->info->speed * FIX2FLT(finecosine[an]);
            shard->mom[MY] = shard->info->speed * FIX2FLT(finesine[an]);
            shard->mom[MZ] = -.6f;

            P_CheckMissileSpawn(shard);
        }
    }
    }
}

void C_DECL A_HeadFireGrow(mobj_t *fire)
{
    fire->special3--;
    fire->pos[VZ] += 9;

    if(fire->special3 == 0)
    {
        fire->damage = fire->info->damage;
        P_MobjChangeState(fire, S_HEADFX3_4);
    }
}

void C_DECL A_SnakeAttack(mobj_t *actor)
{
    if(!actor->target)
    {
        P_MobjChangeState(actor, S_SNAKE_WALK1);
        return;
    }

    S_StartSound(actor->info->attackSound, actor);
    A_FaceTarget(actor);
    P_SpawnMissile(MT_SNAKEPRO_A, actor, actor->target, true);
}

void C_DECL A_SnakeAttack2(mobj_t *actor)
{
    if(!actor->target)
    {
        P_MobjChangeState(actor, S_SNAKE_WALK1);
        return;
    }

    S_StartSound(actor->info->attackSound, actor);
    A_FaceTarget(actor);
    P_SpawnMissile(MT_SNAKEPRO_B, actor, actor->target, true);
}

void C_DECL A_ClinkAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 7) + 3);
        P_DamageMobj(actor->target, actor, actor, damage, false);
    }
}

void C_DECL A_GhostOff(mobj_t *actor)
{
    actor->flags &= ~MF_SHADOW;
}

void C_DECL A_WizAtk1(mobj_t *actor)
{
    A_FaceTarget(actor);
    actor->flags &= ~MF_SHADOW;
}

void C_DECL A_WizAtk2(mobj_t *actor)
{
    A_FaceTarget(actor);
    actor->flags |= MF_SHADOW;
}

void C_DECL A_WizAtk3(mobj_t *actor)
{
    mobj_t     *mo;
    angle_t     angle;
    float       momZ;

    actor->flags &= ~MF_SHADOW;

    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(4), false);
        return;
    }

    mo = P_SpawnMissile(MT_WIZFX1, actor, actor->target, true);
    if(mo)
    {
        momZ = mo->mom[MZ];
        angle = mo->angle;

        P_SpawnMissileAngle(MT_WIZFX1, actor, angle - (ANG45 / 8), momZ);
        P_SpawnMissileAngle(MT_WIZFX1, actor, angle + (ANG45 / 8), momZ);
    }
}

void C_DECL A_Scream(mobj_t *actor)
{
    switch(actor->type)
    {
    case MT_CHICPLAYER:
    case MT_SORCERER1:
    case MT_MINOTAUR:
        // Make boss death sounds full volume
        S_StartSound(actor->info->deathSound, NULL);
        break;

    case MT_PLAYER:
        // Handle the different player death screams
        if(actor->special1 < 10)
        {
            // Wimpy death sound.
            S_StartSound(SFX_PLRWDTH, actor);
        }
        else if(actor->health > -50)
        {
            // Normal death sound.
            S_StartSound(actor->info->deathSound, actor);
        }
        else if(actor->health > -100)
        {
            // Crazy death sound.
            S_StartSound(SFX_PLRCDTH, actor);
        }
        else
        {
            // Extreme death sound.
            S_StartSound(SFX_GIBDTH, actor);
        }
        break;

    default:
        S_StartSound(actor->info->deathSound, actor);
        break;
    }
}

mobj_t* P_DropItem(mobjtype_t type, mobj_t* source, int special, int chance)
{
    assert(source);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* mo;

    if(P_Random() > chance)
        return NULL;

    if((mo = GameMap_SpawnMobj3f(map, type, source->pos[VX], source->pos[VY],
                                 source->pos[VZ] + source->height / 2, source->angle, 0)))
    {
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
        if(!(mo->info->flags2 & MF2_FLOATBOB))
            mo->mom[MZ] = 5 + FIX2FLT(P_Random() << 10);

        mo->flags |= MF_DROPPED;
        mo->health = special;
    }

    return mo;
    }
}

void C_DECL A_NoBlocking(mobj_t *actor)
{
    actor->flags &= ~MF_SOLID;

    // Check for monsters dropping things.
    switch(actor->type)
    {
    case MT_MUMMY:
    case MT_MUMMYLEADER:
    case MT_MUMMYGHOST:
    case MT_MUMMYLEADERGHOST:
        P_DropItem(MT_AMGWNDWIMPY, actor, 3, 84);
        break;

    case MT_KNIGHT:
    case MT_KNIGHTGHOST:
        P_DropItem(MT_AMCBOWWIMPY, actor, 5, 84);
        break;

    case MT_WIZARD:
        P_DropItem(MT_AMBLSRWIMPY, actor, 10, 84);
        P_DropItem(MT_ARTITOMEOFPOWER, actor, 0, 4);
        break;

    case MT_HEAD:
        P_DropItem(MT_AMBLSRWIMPY, actor, 10, 84);
        P_DropItem(MT_ARTIEGG, actor, 0, 51);
        break;

    case MT_BEAST:
        P_DropItem(MT_AMCBOWWIMPY, actor, 10, 84);
        break;

    case MT_CLINK:
        P_DropItem(MT_AMSKRDWIMPY, actor, 20, 84);
        break;

    case MT_SNAKE:
        P_DropItem(MT_AMPHRDWIMPY, actor, 5, 84);
        break;

    case MT_MINOTAUR:
        P_DropItem(MT_ARTISUPERHEAL, actor, 0, 51);
        P_DropItem(MT_AMPHRDWIMPY, actor, 10, 84);
        break;

    default:
        break;
    }
}

void C_DECL A_Explode(mobj_t* actor)
{
    int                 damage;

    damage = 128;
    switch(actor->type)
    {
    case MT_FIREBOMB:
        // Time Bombs.
        actor->pos[VZ] += 32;
        actor->flags &= ~MF_SHADOW;
        actor->flags |= MF_BRIGHTSHADOW | MF_VIEWALIGN;
        break;

    case MT_MNTRFX2:
        // Minotaur floor fire
        damage = 24;
        break;

    case MT_SOR2FX1:
        // D'Sparil missile
        damage = 80 + (P_Random() & 31);
        break;

    default:
        break;
    }

    P_RadiusAttack(actor, actor->target, damage, damage - 1);
    P_HitFloor(actor);
}

void C_DECL A_PodPain(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    int i, count, chance;

    chance = P_Random();
    if(chance < 128)
        return;

    if(chance > 240)
        count = 2;
    else
        count = 1;

    for(i = 0; i < count; ++i)
    {
        mobj_t* goo;

        if((goo = GameMap_SpawnMobj3f(map, MT_PODGOO, actor->pos[VX], actor->pos[VY],
                                      actor->pos[VZ] + 48, actor->angle, 0)))
        {
            goo->target = actor;
            goo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
            goo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
            goo->mom[MZ] = 1.0f / 2 + FIX2FLT((P_Random() << 9));
        }
    }
    }
}

void C_DECL A_RemovePod(mobj_t* actor)
{
    assert(actor);
    {
    mobj_t* mo;

    if((mo = actor->generator))
    {
        if(mo->special1 > 0)
            mo->special1--;
    }
    }
}

void C_DECL A_MakePod(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* mo;

    // Too many generated pods?
    if(actor->special1 == MAX_GEN_PODS)
        return;

    if(!(mo = GameMap_SpawnMobj3f(map, MT_POD, actor->pos[VX], actor->pos[VY], 0, actor->angle, MSF_Z_FLOOR)))
        return;

    if(P_CheckPosition2f(mo, mo->pos[VX], mo->pos[VY]) == false)
    {
        // Didn't fit.
        P_MobjRemove(mo, true);
        return;
    }

    P_MobjChangeState(mo, S_POD_GROW1);
    P_ThrustMobj(mo, P_Random() << 24, 4.5f);

    S_StartSound(SFX_NEWPOD, mo);

    // Increment generated pod count.
    actor->special1++;

    // Link the generator to the pod.
    mo->generator = actor;
    return;
    }
}

static int massacreMobj(void* p, void* context)
{
    int* count = (int*) context;
    mobj_t* mo = (mobj_t*) p;

    if(!mo->player && sentient(mo) && (mo->flags & MF_SHOOTABLE))
    {
        P_DamageMobj(mo, NULL, NULL, 10000, false);
        (*count)++;
    }

    return true; // Continue iteration.
}

/**
 * Kills all monsters.
 */
int P_Massacre(void)
{
    int                 count = 0;

    // Only massacre when actually in a level.
    if(G_GetGameState() == GS_MAP)
    {
        DD_IterateThinkers(P_MobjThinker, massacreMobj, &count);
    }

    return count;
}

typedef struct {
    mobjtype_t          type;
    size_t              count;
} countmobjoftypeparams_t;

static int countMobjOfType(void* p, void* context)
{
    countmobjoftypeparams_t* params = (countmobjoftypeparams_t*) context;
    mobj_t*             mo = (mobj_t*) p;

    if(params->type == mo->type && mo->health > 0)
        params->count++;

    return true; // Continue iteration.
}

/**
 * Trigger special effects if all bosses are dead.
 */
void C_DECL A_BossDeath(mobj_t* actor)
{
    static mobjtype_t   bossType[6] = {
        MT_HEAD,
        MT_MINOTAUR,
        MT_SORCERER2,
        MT_HEAD,
        MT_MINOTAUR,
        -1
    };

    linedef_t*          dummyLine;
    countmobjoftypeparams_t params;

    // Not a boss level?
    if(gameMap != 8)
        return;

    // Not considered a boss in this episode?
    if(actor->type != bossType[gameEpisode - 1])
        return;

    // Scan the remaining thinkers to see if all bosses are dead.
    params.type = actor->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(params.count)
    {   // Other boss not dead.
        return;
    }

    // Kill any remaining monsters.
    if(gameEpisode > 1)
        P_Massacre();

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 666;
    EV_DoFloor(dummyLine, FT_LOWER);
    P_FreeDummyLine(dummyLine);
}

void C_DECL A_ESound(mobj_t *mo)
{
    int     sound;

    switch(mo->type)
    {
    case MT_SOUNDWATERFALL:
        sound = SFX_WATERFL;
        break;

    case MT_SOUNDWIND:
        sound = SFX_WIND;
        break;

    default:
        return;
    }

    S_StartSound(sound, mo);
}

void C_DECL A_SpawnTeleGlitter(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    float pos[3];
    mobj_t* mo;

    pos[VX] = actor->pos[VX] + ((P_Random() & 31) - 16);
    pos[VY] = actor->pos[VY] + ((P_Random() & 31) - 16);
    pos[VZ] = 0;

    if((mo = GameMap_SpawnMobj3fv(map, MT_TELEGLITTER, pos, P_Random() << 24, MSF_Z_FLOOR)))
    {
        mo->mom[MZ] = 1.0f / 4;
        mo->special3 = 1000;
    }
    }
}

void C_DECL A_SpawnTeleGlitter2(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    float pos[3];
    mobj_t* mo;

    pos[VX] = actor->pos[VX] + ((P_Random() & 31) - 16);
    pos[VY] = actor->pos[VY] + ((P_Random() & 31) - 16);
    pos[VZ] = 0;

    if((mo = GameMap_SpawnMobj3fv(map, MT_TELEGLITTER2, pos, P_Random() << 24, MSF_Z_FLOOR)))
    {
        mo->mom[MZ] = 1.0f / 4;
        mo->special3 = 1000;
    }
    }
}

void C_DECL A_AccTeleGlitter(mobj_t* actor)
{
    assert(actor);
    if(++actor->special3 > 35)
        actor->mom[MZ] += actor->mom[MZ] / 2;
}

void C_DECL A_InitKeyGizmo(mobj_t* gizmo)
{
    assert(gizmo);
    {
    gamemap_t* map = P_CurrentGameMap();
    statenum_t state;
    mobj_t* mo;

    switch(gizmo->type)
    {
    case MT_KEYGIZMOBLUE:
        state = S_KGZ_BLUEFLOAT1;
        break;

    case MT_KEYGIZMOGREEN:
        state = S_KGZ_GREENFLOAT1;
        break;

    case MT_KEYGIZMOYELLOW:
        state = S_KGZ_YELLOWFLOAT1;
        break;

    default:
        return;
    }

    if((mo = GameMap_SpawnMobj3f(map, MT_KEYGIZMOFLOAT,
                           gizmo->pos[VX], gizmo->pos[VY], gizmo->pos[VZ] + 60,
                           gizmo->angle, 0)))
    {
        P_MobjChangeState(mo, state);
    }
    }
}

void C_DECL A_VolcanoSet(mobj_t* volcano)
{
    assert(volcano);
    volcano->tics = 105 + (P_Random() & 127);
}

void C_DECL A_VolcanoBlast(mobj_t* volcano)
{
    assert(volcano);
    {
    gamemap_t* map = P_CurrentGameMap();
    int i, count;

    count = 1 + (P_Random() % 3);
    for(i = 0; i < count; ++i)
    {
        mobj_t* blast;
        unsigned int an;

        if((blast = GameMap_SpawnMobj3f(map, MT_VOLCANOBLAST,
                                  volcano->pos[VX], volcano->pos[VY],
                                  volcano->pos[VZ] + 44, P_Random() << 24, 0)))
        {
            blast->target = volcano;

            an = blast->angle >> ANGLETOFINESHIFT;
            blast->mom[MX] = 1 * FIX2FLT(finecosine[an]);
            blast->mom[MY] = 1 * FIX2FLT(finesine[an]);
            blast->mom[MZ] = 2.5f + FIX2FLT(P_Random() << 10);

            S_StartSound(SFX_VOLSHT, blast);
            P_CheckMissileSpawn(blast);
        }
    }
    }
}

void C_DECL A_VolcBallImpact(mobj_t* ball)
{
    assert(ball);
    {
    gamemap_t* map = P_CurrentGameMap();
    int i;

    if(ball->pos[VZ] <= ball->floorZ)
    {
        ball->flags |= MF_NOGRAVITY;
        ball->flags2 &= ~MF2_LOGRAV;
        ball->pos[VZ] += 28;
    }

    P_RadiusAttack(ball, ball->target, 25, 24);
    for(i = 0; i < 4; ++i)
    {
        mobj_t* tiny;

        if((tiny = GameMap_SpawnMobj3fv(map, MT_VOLCANOTBLAST, ball->pos, i * ANG90, 0)))
        {
            unsigned int an = tiny->angle >> ANGLETOFINESHIFT;

            tiny->target = ball;
            tiny->mom[MX] = .7f * FIX2FLT(finecosine[an]);
            tiny->mom[MY] = .7f * FIX2FLT(finesine[an]);
            tiny->mom[MZ] = 1 + FIX2FLT(P_Random() << 9);

            P_CheckMissileSpawn(tiny);
        }
    }
    }
}

void C_DECL A_SkullPop(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* mo;

    if((mo = GameMap_SpawnMobj3f(map, MT_BLOODYSKULL, actor->pos[VX], actor->pos[VY],
                           actor->pos[VZ] + 48, actor->angle, 0)))
    {
        player_t* player;

        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
        mo->mom[MZ] = 2 + FIX2FLT(P_Random() << 6);

        // Attach player mobj to bloody skull.
        player = actor->player;
        actor->player = NULL;
        actor->dPlayer = NULL;
        actor->flags &= ~MF_SOLID;

        mo->player = player;
        mo->dPlayer = player->plr;
        mo->health = actor->health;

        player->plr->mo = mo;
        player->plr->lookDir = 0;
        player->damageCount = 32;
    }
    }
}

void C_DECL A_CheckSkullFloor(mobj_t* actor)
{
    assert(actor);
    if(actor->pos[VZ] <= actor->floorZ)
        P_MobjChangeState(actor, S_BLOODYSKULLX1);
}

void C_DECL A_CheckSkullDone(mobj_t* actor)
{
    assert(actor);
    if(actor->special2 == 666)
        P_MobjChangeState(actor, S_BLOODYSKULLX2);
}

void C_DECL A_CheckBurnGone(mobj_t* actor)
{
    assert(actor);
    if(actor->special2 == 666)
        P_MobjChangeState(actor, S_PLAY_FDTH20);
}

void C_DECL A_FreeTargMobj(mobj_t* mo)
{
    assert(mo);
    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    mo->pos[VZ] = mo->ceilingZ + 4;

    mo->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY | MF_SOLID);
    mo->flags |= MF_CORPSE | MF_DROPOFF | MF_NOGRAVITY;
    mo->flags2 &= ~(MF2_PASSMOBJ | MF2_LOGRAV);

    mo->player = NULL;
    mo->dPlayer = NULL;
}

void C_DECL A_AddPlayerCorpse(mobj_t* actor)
{
    assert(actor);
    {
    gamemap_t* map = P_CurrentGameMap();

    // Too many player corpses?
    if(map->bodyQueueSlot >= BODYQUEUESIZE)
    {
        // Remove an old one.
        P_MobjRemove(map->bodyQueue[map->bodyQueueSlot % BODYQUEUESIZE], true);
    }

    map->bodyQueue[map->bodyQueueSlot % BODYQUEUESIZE] = actor;
    map->bodyQueueSlot++;
    }
}

void C_DECL A_FlameSnd(mobj_t *actor)
{
    S_StartSound(SFX_HEDAT1, actor); // Burn sound.
}

void C_DECL A_HideThing(mobj_t *actor)
{
    //P_MobjUnsetPosition(actor);
    actor->flags2 |= MF2_DONTDRAW;
}

void C_DECL A_UnHideThing(mobj_t *actor)
{
    //P_MobjSetPosition(actor);
    actor->flags2 &= ~MF2_DONTDRAW;
}
