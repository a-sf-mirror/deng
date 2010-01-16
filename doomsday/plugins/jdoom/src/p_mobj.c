/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
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
 * p_mobj.c: Moving object handling. Spawn functions.
 */

#ifdef MSVC
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jdoom.h"

#include "gamemap.h"
#include "hu_stuff.h"
#include "g_common.h"
#include "p_map.h"
#include "p_terraintype.h"
#include "p_player.h"
#include "p_tick.h"
#include "dmu_lib.h"

// MACROS ------------------------------------------------------------------

#define VANISHTICS              (2*TICSPERSEC)

#define MAX_BOB_OFFSET          (8)

#define NOMOMENTUM_THRESHOLD    (0.000001f)
#define STOPSPEED               (1.0f/1.6/10)
#define DROPOFFMOMENTUM_THRESHOLD (1.0f / 4)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

const terraintype_t* P_MobjGetFloorTerrainType(mobj_t* mo)
{
    sector_t* sec = DMU_GetPtrp(mo->subsector, DMU_SECTOR);

    return P_GetPlaneMaterialType(sec, PLN_FLOOR);
}

/**
 * @return              @c true, if the mobj is still present.
 */
boolean P_MobjChangeState(mobj_t* mobj, statenum_t state)
{
    state_t* st;

    do
    {
        if(state == S_NULL)
        {
            mobj->state = (state_t *) S_NULL;
            P_MobjRemove(mobj, false);
            return false;
        }

        P_MobjSetState(mobj, state);
        st = &STATES[state];

        mobj->turnTime = false; // $visangle-facetarget

        // Modified handling.
        // Call action functions when the state is set.
        if(st->action)
            st->action(mobj);

        state = st->nextState;
    } while(!mobj->tics);

    return true;
}

void P_ExplodeMissile(mobj_t *mo)
{
    if(IS_CLIENT)
    {
        // Clients won't explode missiles.
        P_MobjChangeState(mo, S_NULL);
        return;
    }

    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));

    mo->tics -= P_Random() & 3;

    if(mo->tics < 1)
        mo->tics = 1;

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        // Remove the brightshadow flag.
        if(mo->flags & MF_BRIGHTSHADOW)
            mo->flags &= ~MF_BRIGHTSHADOW;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }

    if(mo->info->deathSound)
        S_StartSound(mo->info->deathSound, mo);
}

void P_FloorBounceMissile(mobj_t* mo)
{
    mo->mom[MZ] = -mo->mom[MZ];
    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));
}

/**
 * @return              The ground friction factor for the mobj.
 */
float P_MobjGetFriction(mobj_t* mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) && !mo->onMobj)
    {
        return FRICTION_FLY;
    }

    return XS_Friction(DMU_GetPtrp(mo->subsector, DMU_SECTOR));
}

static boolean isInWalkState(player_t* pl)
{
    return pl->plr->mo->state - STATES -
                PCLASS_INFO(pl->class)->runState < 4;
}

static float getFriction(mobj_t* mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) &&
       !mo->onMobj)
    {   // Airborne friction.
        return FRICTION_FLY;
    }

#if __JHERETIC__
    if(P_ToXSector(DMU_GetPtrp(mo->face, DMU_SECTOR))->special == 15)
    {   // Friction_Low
        return FRICTION_LOW;
    }
#endif

    return P_MobjGetFriction(mo);
}

void P_MobjMoveXY(mobj_t* mo)
{
    assert(mo);
    {
    gamemap_t* map = P_CurrentGameMap();
    float pos[3], mom[3];
    player_t* player;
    boolean largeNegative;

    // $democam: cameramen have their own movement code.
    if(P_CameraXYMovement(mo))
        return;

    if(INRANGE_OF(mo->mom[MX], 0, NOMOMENTUM_THRESHOLD) &&
       INRANGE_OF(mo->mom[MY], 0, NOMOMENTUM_THRESHOLD))
    {
        if(mo->flags & MF_SKULLFLY)
        {   // The skull slammed into something.
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

            P_MobjChangeState(mo, P_GetState(mo->type, SN_SPAWN));
        }

        return;
    }

    mom[MX] = MINMAX_OF(-MAXMOVE, mo->mom[MX], MAXMOVE);
    mom[MY] = MINMAX_OF(-MAXMOVE, mo->mom[MY], MAXMOVE);
    mo->mom[MX] = mom[MX];
    mo->mom[MY] = mom[MY];

    player = mo->player;

    do
    {
        /**
         * DOOM.exe bug fix:
         * Large negative displacements were never considered. This explains
         * the tendency for Mancubus fireballs to pass through walls.
         */

        largeNegative = false;
        if(!cfg.moveBlock &&
           (mom[MX] < -MAXMOVE / 2 || mom[MY] < -MAXMOVE / 2))
        {
            // Make an exception for "north-only wallrunning".
            if(!(cfg.wallRunNorthOnly && mo->wallRun))
                largeNegative = true;
        }

        if(largeNegative || mom[MX] > MAXMOVE / 2 || mom[MY] > MAXMOVE / 2)
        {
            pos[VX] = mo->pos[VX] + mom[MX] / 2;
            pos[VY] = mo->pos[VY] + mom[MY] / 2;
            mom[VX] /= 2;
            mom[VY] /= 2;
        }
        else
        {
            pos[VX] = mo->pos[VX] + mom[MX];
            pos[VY] = mo->pos[VY] + mom[MY];
            mom[MX] = mom[MY] = 0;
        }

        // If mobj was wallrunning - stop.
        if(mo->wallRun)
            mo->wallRun = false;

        // $dropoff_fix.
        if(!P_TryMove(mo, pos[VX], pos[VY], true, false))
        {   // Blocked move.
            if(mo->flags2 & MF2_SLIDE)
            {   // Try to slide along it.
                P_SlideMove(mo);
            }
            else if(mo->flags & MF_MISSILE)
            {
                sector_t* backSec;

                //// kludge: Prevent missiles exploding against the sky.
                if(map->ceilingLine &&
                   (backSec = DMU_GetPtrp(map->ceilingLine, DMU_BACK_SECTOR)))
                {
                    material_t* mat = DMU_GetPtrp(backSec, DMU_CEILING_MATERIAL);

                    if((DMU_GetIntp(mat, DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->pos[VZ] > DMU_GetFloatp(backSec, DMU_CEILING_HEIGHT))
                    {
                        P_MobjRemove(mo, false);
                        return;
                    }
                }

                if(map->floorLine &&
                   (backSec = DMU_GetPtrp(map->floorLine, DMU_BACK_SECTOR)))
                {
                    material_t* mat = DMU_GetPtrp(backSec, DMU_FLOOR_MATERIAL);

                    if((DMU_GetIntp(mat, DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->pos[VZ] < DMU_GetFloatp(backSec, DMU_FLOOR_HEIGHT))
                    {
                        P_MobjRemove(mo, false);
                        return;
                    }
                }
                //// kludge end.

                P_ExplodeMissile(mo);
            }
            else
            {
                mo->mom[MX] = mo->mom[MY] = 0;
            }
        }
    } while(!INRANGE_OF(mom[MX], 0, NOMOMENTUM_THRESHOLD) ||
            !INRANGE_OF(mom[MY], 0, NOMOMENTUM_THRESHOLD));

    // Slow down.
    if(player && (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
    {
        // Debug option for no sliding at all.
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }

    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
        return; // No friction for missiles ever.

    if(mo->pos[VZ] > mo->floorZ && !mo->onMobj && !(mo->flags2 & MF2_FLY))
        return; // No friction when falling.

    if(cfg.slidingCorpses)
    {
        // $dropoff_fix: Add objects falling off ledges, does not apply to
        // players!
        if(((mo->flags & MF_CORPSE) || (mo->intFlags & MIF_FALLING)) &&
           !mo->player)
        {
            // Do not stop sliding if halfway off a step with some momentum.
            if(!INRANGE_OF(mo->mom[MX], 0, DROPOFFMOMENTUM_THRESHOLD) ||
               !INRANGE_OF(mo->mom[MY], 0, DROPOFFMOMENTUM_THRESHOLD))
            {
                if(mo->floorZ != DMU_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
                    return;
            }
        }
    }

    // Stop player walking animation.
    if((!player || !(player->plr->cmd.forwardMove | player->plr->cmd.sideMove) ||
         player->plr->mo != mo /* $voodoodolls: Stop also. */) &&
       INRANGE_OF(mo->mom[MX], 0, STOPSPEED) &&
       INRANGE_OF(mo->mom[MY], 0, STOPSPEED))
    {
        // If in a walking frame, stop moving.
        if(player && isInWalkState(player) && player->plr->mo == mo)
            P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->normalState);

        mo->mom[MX] = mo->mom[MY] = 0;

        // $voodoodolls: Stop view bobbing if this isn't a voodoo doll.
        if(player && player->plr->mo == mo)
            player->bob = 0;
    }
    else
    {
        float       friction = getFriction(mo);

        mo->mom[MX] *= friction;
        mo->mom[MY] *= friction;
    }
    }
}

/*
static boolean PIT_Splash(sector_t *sector, void *data)
{
    mobj_t             *mo = data;
    float               floorHeight;

    floorHeight = DMU_GetFloatp(sector, DMU_FLOOR_HEIGHT);

    // Is the mobj touching the floor of this sector?
    if(mo->pos[VZ] < floorHeight &&
       mo->pos[VZ] + mo->height / 2 > floorHeight)
    {
        //// \todo Play a sound, spawn a generator, etc.
    }

    // Continue checking.
    return true;
}
*/

void P_HitFloor(mobj_t* mo)
{
    //P_MobjSectorsIterator(mo, PIT_Splash, mo);
}

void P_MobjMoveZ(mobj_t* mo)
{
    gamemap_t* map = P_CurrentGameMap();
    float gravity, targetZ, floorZ, ceilingZ;

    // $democam: cameramen get special z movement.
    if(P_CameraZMovement(mo))
        return;

    targetZ = mo->pos[VZ] + mo->mom[MZ];
    floorZ = (mo->onMobj? mo->onMobj->pos[VZ] + mo->onMobj->height : mo->floorZ);
    ceilingZ = mo->ceilingZ;
    gravity = XS_Gravity(DMU_GetPtrp(mo->subsector, DMU_SECTOR));

    if((mo->flags2 & MF2_FLY) && mo->player &&
       mo->onMobj && mo->pos[VZ] > mo->onMobj->pos[VZ] + mo->onMobj->height)
        mo->onMobj = NULL; // We were on a mobj, we are NOT now.

    if(!((mo->flags ^ MF_FLOAT) & (MF_FLOAT | MF_SKULLFLY | MF_INFLOAT)) &&
       mo->target && !P_MobjIsCamera(mo->target))
    {
        float dist, delta;

        // Float down towards target if too close.
        dist = P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                mo->pos[VY] - mo->target->pos[VY]);

        delta = (mo->target->pos[VZ] + mo->target->height / 2) -
                (mo->pos[VZ] + mo->height / 2);

        // Don't go INTO the target.
        if(!(dist < mo->radius + mo->target->radius &&
             fabs(delta) < mo->height + mo->target->height))
        {
            if(delta < 0 && dist < -(delta * 3))
            {
                targetZ -= FLOATSPEED;
                P_MobjSetSRVOZ(mo, -FLOATSPEED);
            }
            else if(delta > 0 && dist < (delta * 3))
            {
                targetZ += FLOATSPEED;
                P_MobjSetSRVOZ(mo, FLOATSPEED);
            }
        }
    }

    // Do some fly-bobbing.
    if(mo->player && (mo->flags2 & MF2_FLY) && mo->pos[VZ] > floorZ &&
       (map->time & 2))
    {
        targetZ += FIX2FLT(finesine[(FINEANGLES / 20 * map->time >> 2) & FINEMASK]);
    }

    if(targetZ < floorZ)
    {   // Hit the floor (or another mobj).
        boolean movingDown;
        // Note (id):
        //  somebody left this after the setting momz to 0,
        //  kinda useless there.
        //
        // cph - This was the a bug in the linuxdoom-1.10 source which
        //  caused it not to sync Doom 2 v1.9 demos. Someone
        //  added the above comment and moved up the following code. So
        //  demos would desync in close lost soul fights.
        // Note that this only applies to original Doom 1 or Doom2 demos - not
        //  Final Doom and Ultimate Doom.  So we test demo_compatibility *and*
        //  gameMission. (Note we assume that Doom1 is always Ult Doom, which
        //  seems to hold for most published demos.)
        //
        //  fraggle - cph got the logic here slightly wrong.  There are three
        //  versions of Doom 1.9:
        //
        //  * The version used in registered doom 1.9 + doom2 - no bounce
        //  * The version used in ultimate doom - has bounce
        //  * The version used in final doom - has bounce
        //
        // So we need to check that this is either retail or commercial
        // (but not doom2)
        int correctLostSoulBounce =
            (gameMode == retail || gameMode == commercial) &&
                    gameMission != GM_DOOM2;

        if(correctLostSoulBounce && (mo->flags & MF_SKULLFLY))
        {
            // The skull slammed into something.
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(movingDown = (mo->mom[MZ] < 0))
        {
            if(mo->player && mo->player->plr->mo == mo &&
               mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment after hitting the ground
                // (hard), and utter appropriate sound.
                mo->player->viewHeightDelta = mo->mom[MZ] / 8;
                mo->player->jumpTics = 10;

                /**
                 * DOOM bug:
                 * Dead players would grunt when hitting the ground (e.g.,
                 * after an archvile attack).
                 */
                if(mo->player->health > 0)
                    S_StartSound(SFX_OOF, mo);
            }
        }

        targetZ = floorZ;

        if(movingDown && !mo->onMobj)
            P_HitFloor(mo);

        /**
         * See lost soul bouncing comment above. We need this here for bug
         * compatibility with original Doom2 v1.9 - if a soul is charging
         * and hit by a raising floor this would incorrectly reverse it's
         * Y momentum.
         */
        if(!correctLostSoulBounce && (mo->flags & MF_SKULLFLY))
            mo->mom[MZ] = -mo->mom[MZ];

        if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
        {
            mo->pos[VZ] = targetZ;

            if((mo->flags2 & MF2_FLOORBOUNCE) && !mo->onMobj)
            {
                P_FloorBounceMissile(mo);
                return;
            }
            else
            {
                P_ExplodeMissile(mo);
                return;
            }
        }

        if(movingDown && mo->mom[MZ] < 0)
            mo->mom[MZ] = 0;

        // $voodoodolls: Check for smooth step up unless a voodoo doll.
        if(mo->player && mo->player->plr->mo == mo &&
           mo->pos[VZ] < mo->floorZ)
        {
            mo->player->viewHeight -= (mo->floorZ - mo->pos[VZ]);
            mo->player->viewHeightDelta =
                (cfg.plrViewHeight - mo->player->viewHeight) / 8;
        }

        mo->pos[VZ] = floorZ;

        if(mo->flags & MF_SKULLFLY)
        {   // The skull slammed into something.
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
        {
            material_t* mat = DMU_GetPtrp(mo->subsector, DMU_FLOOR_MATERIAL);

            // Don't explode against sky.
            if(DMU_GetIntp(mat, DMU_FLAGS) & MATF_SKYMASK)
            {
                P_MobjRemove(mo, false);
            }
            else
            {
                P_ExplodeMissile(mo);
            }
        }
    }
    else
    {
        if(targetZ + mo->height > ceilingZ)
        {   // Hit the ceiling.
            if(mo->mom[MZ] > 0)
                mo->mom[MZ] = 0;

            mo->pos[VZ] = mo->ceilingZ - mo->height;

            if(mo->flags & MF_SKULLFLY)
            {   // The skull slammed into something.
                mo->mom[MZ] = -mo->mom[MZ];
            }

            if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
            {
                material_t* mat = DMU_GetPtrp(mo->subsector, DMU_CEILING_MATERIAL);

                // Don't explode against sky.
                if(DMU_GetIntp(mat, DMU_FLAGS) & MATF_SKYMASK)
                {
                    P_MobjRemove(mo, false);
                }
                else
                {
                    P_ExplodeMissile(mo);
                }
            }
        }
        else
        {   // In "free space".
            // Update gravity's effect on momentum.
            if(mo->flags2 & MF2_LOGRAV)
            {
                if(mo->mom[MZ] == 0)
                    mo->mom[MZ] = -(gravity / 8) * 2;
                else
                    mo->mom[MZ] -= (gravity / 8);
            }
            else if(!(mo->flags & MF_NOGRAVITY))
            {
                if(mo->mom[MZ] == 0)
                    mo->mom[MZ] = -gravity * 2;
                else
                    mo->mom[MZ] -= gravity;
            }

            mo->pos[VZ] = targetZ;
        }
    }
}

void P_NightmareRespawn(mobj_t* mobj)
{
    assert(mobj);
    {
    gamemap_t* map = P_CurrentGameMap();
    mobj_t* mo;

    // Something is occupying it's position?
    if(!P_CheckPosition2f(mobj, mobj->spawnSpot.pos[VX],
                          mobj->spawnSpot.pos[VY]))
        return; // No respwan.

    if((mo = GameMap_SpawnMobj3fv(map, mobj->type, mobj->spawnSpot.pos,
                            mobj->spawnSpot.angle, mobj->spawnSpot.flags)))
    {
        mo->reactionTime = 18;

        // Spawn a teleport fog at old spot.
        if((mo = GameMap_SpawnMobj3f(map, MT_TFOG, mobj->pos[VX], mobj->pos[VY], 0,
                           mobj->angle, MSF_Z_FLOOR)))
            S_StartSound(SFX_TELEPT, mo);

        // Spawn a teleport fog at the new spot.
        if((mo = GameMap_SpawnMobj3fv(map, MT_TFOG, mobj->spawnSpot.pos,
                                mobj->spawnSpot.angle,
                                mobj->spawnSpot.flags)))
            S_StartSound(SFX_TELEPT, mo);
    }

    // Remove the old monster.
    P_MobjRemove(mobj, true);
    }
}

void P_MobjThinker(mobj_t* mo)
{
    float floorZ;

    if(!mo)
        return; // Wha?

    if(mo->ddFlags & DDMF_REMOTE)
        return; // Remote mobjs are handled separately.

    // Spectres get selector = 1.
    if(mo->type == MT_SHADOWS)
        mo->selector = (mo->selector & ~DDMOBJ_SELECTOR_MASK) | 1;

    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mo);

#if __JHERETIC__
    // Lightsources must stay where they're hooked.
    if(mo->type == MT_LIGHTSOURCE)
    {
        if(mo->moveDir > 0)
            mo->pos[VZ] =
                DMU_GetFloatp(mo->face, DMU_FLOOR_HEIGHT);
        else
            mo->pos[VZ] =
                DMU_GetFloatp(mo->face, DMU_CEILING_HEIGHT);

        mo->pos[VZ] += FIX2FLT(mo->moveDir);
        return;
    }
#endif

    // Handle X and Y momentums.
    if(!INRANGE_OF(mo->mom[MX], 0, NOMOMENTUM_THRESHOLD) ||
       !INRANGE_OF(mo->mom[MY], 0, NOMOMENTUM_THRESHOLD) ||
       (mo->flags & MF_SKULLFLY))
    {
        P_MobjMoveXY(mo);

        //// \fixme decent NOP/NULL/Nil function pointer please.
        if(mo->thinker.function == NOPFUNC)
            return; // Mobj was removed.
    }

    floorZ = (mo->onMobj? mo->onMobj->pos[VZ] + mo->onMobj->height : mo->floorZ);

    if(mo->flags2 & MF2_FLOATBOB)
    {   // Floating item bobbing motion.
        // Keep it on the floor.
        mo->pos[VZ] = floorZ;
#if __JHERETIC__
        // Negative floorclip raises the mo off the floor.
        mo->floorClip = -mo->special1;
#elif __JDOOM__
        mo->floorClip = 0;
#endif
        if(mo->floorClip < -MAX_BOB_OFFSET)
        {
            // We don't want it going through the floor.
            mo->floorClip = -MAX_BOB_OFFSET;
        }
    }
    else if(mo->pos[VZ] != floorZ ||
            !INRANGE_OF(mo->mom[MZ], 0, NOMOMENTUM_THRESHOLD))
    {
        P_MobjMoveZ(mo);

        //// \fixme decent NOP/NULL/Nil function pointer please.
        if(mo->thinker.function == NOPFUNC)
            return; // Mobj was removed.
    }
    // Non-sentient objects at rest.
    else if(!sentient(mo) && !mo->player &&
            !(INRANGE_OF(mo->mom[MX], 0, NOMOMENTUM_THRESHOLD) &&
              INRANGE_OF(mo->mom[MY], 0, NOMOMENTUM_THRESHOLD)))
    {
        // Objects fall off ledges if they are hanging off. Slightly push
        // off of ledge if hanging more than halfway off.

        if(mo->pos[VZ] > mo->dropOffZ && // Only objects contacting dropoff.
           !(mo->flags & MF_NOGRAVITY) &&
           !(mo->flags2 & MF2_FLOATBOB) && cfg.fallOff)
        {
            P_ApplyTorque(mo);
        }
        else
        {
            mo->intFlags &= ~MIF_FALLING;
            mo->gear = 0; // Reset torque.
        }
    }

    if(cfg.slidingCorpses)
    {
        if(((mo->flags & MF_CORPSE)? mo->pos[VZ] > mo->dropOffZ :
                                       mo->pos[VZ] - mo->dropOffZ > 24) && // Only objects contacting drop off
           !(mo->flags & MF_NOGRAVITY)) // Only objects which fall.
        {
            P_ApplyTorque(mo); // Apply torque.
        }
        else
        {
            mo->intFlags &= ~MIF_FALLING;
            mo->gear = 0; // Reset torque.
        }
    }

    // $vanish: dead monsters disappear after some time.
    if(cfg.corpseTime && (mo->flags & MF_CORPSE) && mo->corpseTics != -1)
    {
        if(++mo->corpseTics < cfg.corpseTime * TICSPERSEC)
        {
            mo->translucency = 0; // Opaque.
        }
        else if(mo->corpseTics < cfg.corpseTime * TICSPERSEC + VANISHTICS)
        {
            // Translucent during vanishing.
            mo->translucency =
                ((mo->corpseTics -
                  cfg.corpseTime * TICSPERSEC) * 255) / VANISHTICS;
        }
        else
        {
            // Too long; get rid of the corpse.
            mo->corpseTics = -1;
            return;
        }
    }

    // Cycle through states, calling action functions at transitions.
    if(mo->tics != -1)
    {
        mo->tics--;

        P_MobjAngleSRVOTicker(mo); // "angle-servo"; smooth actor turning.

        // You can cycle through multiple states in a tic.
        if(!mo->tics)
        {
            P_MobjClearSRVO(mo);
            P_MobjChangeState(mo, mo->state->nextState);
        }
    }
    else if(!IS_CLIENT)
    {
        gamemap_t* map = P_CurrentGameMap();

        // Check for nightmare respawn.
        if(!(mo->flags & MF_COUNTKILL))
            return;

        if(!respawnMonsters)
            return;

        mo->moveCount++;

        if(mo->moveCount >= 12 * 35 && !(map->time & 31) &&
           P_Random() <= 4)
        {
            P_NightmareRespawn(mo);
        }
    }
}

/**
 * Spawns a mobj of "type" at the specified position.
 */
mobj_t* GameMap_SpawnMobj3f(gamemap_t* map, mobjtype_t type, float x, float y,
                      float z, angle_t angle, int spawnFlags)
{
    assert(map);
    {
    mobj_t* mo;
    mobjinfo_t* info;
    float space;
    int ddflags = 0;

    info = &MOBJINFO[type];

    // Clients only spawn local objects.
    if(!(info->flags & MF_LOCAL) && IS_CLIENT)
        return NULL;

    // Not for deathmatch?
    if(deathmatch && (info->flags & MF_NOTDMATCH))
        return NULL;

    // Check for specific disabled objects.
    if(IS_NETGAME)
    {
        // Cooperative weapons?
        if(cfg.noCoopWeapons && !deathmatch && type >= MT_CLIP &&
           type <= MT_SUPERSHOTGUN)
            return NULL;

        // Don't spawn any special objects in coop?
        if(cfg.noCoopAnything && !deathmatch)
            return NULL;

        // BFG disabled in netgames?
        if(cfg.noNetBFG && type == MT_MISC25)
            return NULL;
    }

    switch(type)
    {
    case MT_BABY: // 68, Arachnotron
    case MT_VILE: // 64, Archvile
    case MT_BOSSBRAIN: // 88, Boss Brain
    case MT_BOSSSPIT: // 89, Boss Shooter
    case MT_KNIGHT: // 69, Hell Knight
    case MT_FATSO: // 67, Mancubus
    case MT_PAIN: // 71, Pain Elemental
    case MT_MEGA: // 74, MegaSphere
    case MT_CHAINGUY: // 65, Former Human Commando
    case MT_UNDEAD: // 66, Revenant
    case MT_WOLFSS: // 84, Wolf SS
        if(gameMode != commercial)
            return NULL;
        break;

    default:
        break;
    }

    // Don't spawn any monsters if -noMonstersParm.
    if(noMonstersParm && ((info->flags & MF_COUNTKILL) || type == MT_SKULL))
        return NULL;

    if(info->flags & MF_SOLID)
        ddflags |= DDMF_SOLID;
    if(info->flags2 & MF2_DONTDRAW)
        ddflags |= DDMF_DONTDRAW;

    mo = P_MobjCreate(P_MobjThinker, x, y, z, angle, info->radius,
                      info->height, ddflags);
    mo->type = type;
    mo->info = info;
    mo->flags = info->flags;
    mo->flags2 = info->flags2;
    mo->flags3 = info->flags3;
    mo->damage = info->damage;
    mo->health =
        info->spawnHealth * (IS_NETGAME ? cfg.netMobHealthModifier : 1);
    mo->moveDir = DI_NODIR;

    // Let the engine know about solid objects.
    P_SetDoomsdayFlags(mo);

    if(gameSkill != SM_NIGHTMARE)
        mo->reactionTime = info->reactionTime;

    mo->lastLook = P_Random() % MAXPLAYERS;

    // Do not set the state with P_MobjChangeState, because action routines
    // can not be called yet.

    // Must link before setting state (ID assigned for the mo).
    P_MobjSetState(mo, P_GetState(mo->type, SN_SPAWN));
    P_MobjSetPosition(mo);

    mo->floorZ = DMU_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT);
    mo->dropOffZ = mo->floorZ;
    mo->ceilingZ = DMU_GetFloatp(mo->subsector, DMU_CEILING_HEIGHT);

    if((spawnFlags & MSF_Z_CEIL) || (info->flags & MF_SPAWNCEILING))
    {
        mo->pos[VZ] = mo->ceilingZ - mo->info->height - z;
    }
    else if((spawnFlags & MSF_Z_RANDOM) || (info->flags2 & MF2_SPAWNFLOAT))
    {
        space = mo->ceilingZ - mo->info->height - mo->floorZ;
        if(space > 48)
        {
            space -= 40;
            mo->pos[VZ] = ((space * P_Random()) / 256) + mo->floorZ + 40;
        }
        else
        {
            mo->pos[VZ] = mo->floorZ;
        }
    }
    else if(spawnFlags & MSF_Z_FLOOR)
    {
        mo->pos[VZ] = mo->floorZ + z;
    }

    if(spawnFlags & MSF_DEAF)
        mo->flags |= MF_AMBUSH;

    mo->floorClip = 0;
    if((mo->flags2 & MF2_FLOORCLIP) &&
       mo->pos[VZ] == DMU_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
    {
        const terraintype_t* tt = P_MobjGetFloorTerrainType(mo);

        if(tt->flags & TTF_FLOORCLIP)
        {
            mo->floorClip = 10;
        }
    }

    // Copy spawn attributes to the new mobj.
    mo->spawnSpot.pos[VX] = x;
    mo->spawnSpot.pos[VY] = y;
    mo->spawnSpot.pos[VZ] = z;
    mo->spawnSpot.angle = angle;
    mo->spawnSpot.flags = spawnFlags;

    return mo;
    }
}

mobj_t* GameMap_SpawnMobj3fv(gamemap_t* map, mobjtype_t type, const float pos[3],
                       angle_t angle, int spawnFlags)
{
    assert(map);
    return GameMap_SpawnMobj3f(map, type, pos[VX], pos[VY], pos[VZ], angle, spawnFlags);
}

mobj_t* P_SpawnCustomPuff(gamemap_t* map, mobjtype_t type, float x,
                          float y, float z, angle_t angle)
{
    assert(map);
    {
    mobj_t* mo;

    // Clients do not spawn puffs.
    if(IS_CLIENT)
        return NULL;

    z += FIX2FLT((P_Random() - P_Random()) << 10);

    if((mo = GameMap_SpawnMobj3f(map, type, x, y, z, angle, 0)))
    {
        mo->mom[MZ] = FIX2FLT(FRACUNIT);
        mo->tics -= P_Random() & 3;

        // Make it last at least one tic.
        if(mo->tics < 1)
            mo->tics = 1;
    }

    return mo;
    }
}

void P_SpawnPuff(gamemap_t* map, float x, float y, float z, angle_t angle)
{
    assert(map);
    {
    mobj_t* th;

    if((th = P_SpawnCustomPuff(map, MT_PUFF, x, y, z, angle)))
    {
        // Don't make punches spark on the wall.
        if(th && map->attackRange == MELEERANGE)
            P_MobjChangeState(th, S_PUFF3);
    }
    }
}

void P_SpawnBlood(gamemap_t* map, float x, float y, float z, int damage, angle_t angle)
{
    assert(map);
    {
    mobj_t* th;

    z += FIX2FLT((P_Random() - P_Random()) << 10);
    if((th = GameMap_SpawnMobj3f(map, MT_BLOOD, x, y, z, angle, 0)))
    {
        th->mom[MZ] = 2;
        th->tics -= P_Random() & 3;

        if(th->tics < 1)
            th->tics = 1;

        if(damage <= 12 && damage >= 9)
            P_MobjChangeState(th, S_BLOOD2);
        else if(damage < 9)
            P_MobjChangeState(th, S_BLOOD3);
    }
    }
}

/**
 * Moves the missile forward a bit and possibly explodes it right there.
 *
 * @param th            The missile to be checked.
 *
 * @return              @c true, if the missile is at a valid location else
 *                      @c false
 */
boolean P_CheckMissileSpawn(mobj_t* th)
{
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    // Move forward slightly so an angle can be computed if it explodes
    // immediately.
    th->pos[VX] += th->mom[MX] / 2;
    th->pos[VY] += th->mom[MY] / 2;
    th->pos[VZ] += th->mom[MZ] / 2;

    if(!P_TryMove(th, th->pos[VX], th->pos[VY], false, false))
    {
        P_ExplodeMissile(th);
        return false;
    }

    return true;
}

/**
 * Tries to aim at a nearby monster if source is a player. Else aim is
 * taken at dest.
 *
 * @param source        The mobj doing the shooting.
 * @param dest          The mobj being shot at. Can be @c NULL if source
 *                      is a player.
 * @param type          The type of mobj to be shot.
 *
 * @return              Pointer to the newly spawned missile.
 */
mobj_t* P_SpawnMissile(mobjtype_t type, mobj_t* source, mobj_t* dest)
{
    assert(source);
    {
    gamemap_t* map = P_CurrentGameMap();
    float pos[3];
    mobj_t* th = 0;
    unsigned int an;
    angle_t angle = 0;
    float dist = 0;
    float slope = 0;
    float spawnZOff = 0;

    memcpy(pos, source->pos, sizeof(pos));

    if(source->player)
    {
        // See which target is to be aimed at.
        angle = source->angle;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!cfg.noAutoAim)
            if(!map->lineTarget)
            {
                angle += 1 << 26;
                slope = P_AimLineAttack(source, angle, 16 * 64);

                if(!map->lineTarget)
                {
                    angle -= 2 << 26;
                    slope = P_AimLineAttack(source, angle, 16 * 64);
                }

                if(!map->lineTarget)
                {
                    angle = source->angle;
                    slope = tan(LOOKDIR2RAD(source->dPlayer->lookDir)) / 1.2f;
                }
            }

        if(!P_MobjIsCamera(source->player->plr->mo))
            spawnZOff = cfg.plrViewHeight - 9 +
                source->player->plr->lookDir / 173;
    }
    else
    {
        // Type specific offset to spawn height z.
        switch(type)
        {
        case MT_TRACER: // Revenant Tracer Missile.
            spawnZOff = 16 + 32;
            break;

        default:
            spawnZOff = 32;
            break;
        }
    }

    pos[VZ] += spawnZOff;
    pos[VZ] -= source->floorClip;

    if(!source->player)
    {
        angle = R_PointToAngle2(pos[VX], pos[VY], dest->pos[VX], dest->pos[VY]);

        // Fuzzy player.
        if(dest->flags & MF_SHADOW)
            angle += (P_Random() - P_Random()) << 20;
    }

    if(!(th = GameMap_SpawnMobj3fv(map, type, pos, angle, 0)))
        return NULL;

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Where it came from.
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    if(source->player)
    {   // Allow free-aim with the BFG in deathmatch?
        if(deathmatch && cfg.netBFGFreeLook == 0 && type == MT_BFG)
            th->mom[MZ] = 0;
        else
            th->mom[MZ] = th->info->speed * slope;
    }
    else
    {
        dist = P_ApproxDistance(dest->pos[VX] - pos[VX],
                                dest->pos[VY] - pos[VY]);
        dist /= th->info->speed;
        if(dist < 1)
            dist = 1;
        th->mom[MZ] = (dest->pos[VZ] - source->pos[VZ]) / dist;
    }

    // Make sure the speed is right (in 3D).
    dist = P_ApproxDistance(P_ApproxDistance(th->mom[MX], th->mom[MY]),
                            th->mom[MZ]);
    if(dist < 1)
        dist = 1;
    dist = th->info->speed / dist;

    th->mom[MX] *= dist;
    th->mom[MY] *= dist;
    th->mom[MZ] *= dist;

    if(P_CheckMissileSpawn(th))
        return th;

    return NULL;
    }
}
