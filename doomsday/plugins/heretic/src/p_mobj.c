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
 * p_mobj.c: Moving object handling. Spawn functions.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jheretic.h"

#include "dmu_lib.h"
#include "hu_stuff.h"
#include "g_common.h"
#include "p_map.h"
#include "p_terraintype.h"
#include "p_player.h"
#include "p_tick.h"

// MACROS ------------------------------------------------------------------

#define VANISHTICS              (2*TICSPERSEC)

#define MAX_BOB_OFFSET          (8)

#define NOMOMENTUM_THRESHOLD    (0.000001f)
#define STOPSPEED               (1.0f/1.6/10)
#define STANDSPEED              (1.0f/2)

#define ITEMQUEUESIZE           (128)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mobjtype_t puffType;
mobj_t *missileMobj;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static spawnspot_t itemRespawnQueue[ITEMQUEUESIZE];
static int itemRespawnTime[ITEMQUEUESIZE];
static int itemRespawnQueueHead, itemRespawnQueueTail;

// CODE --------------------------------------------------------------------

const terraintype_t* P_MobjGetFloorTerrainType(mobj_t* mo)
{
    sector_t*           sec = P_GetPtrp(mo->subsector, DMU_SECTOR);

    return P_GetPlaneMaterialType(sec, PLN_FLOOR);
}

/**
 * @return              @c true, if the mobj is still present.
 */
boolean P_MobjChangeState(mobj_t* mobj, statenum_t state)
{
    state_t*            st;

    if(state == S_NULL)
    {   // Remove mobj.
        mobj->state = (state_t *) S_NULL;
        P_MobjRemove(mobj, false);
        return false;
    }

    if(mobj->ddFlags & DDMF_REMOTE)
    {
        Con_Error("P_MobjChangeState: Can't set Remote state!\n");
    }

    st = &STATES[state];
    P_MobjSetState(mobj, state);

    mobj->turnTime = false; // $visangle-facetarget.
    if(st->action)
    {   // Call action function.
        st->action(mobj);
    }

    return true;
}

/**
 * Same as P_MobjChangeState, but does not call the state function.
 */
boolean P_SetMobjStateNF(mobj_t *mobj, statenum_t state)
{
    if(state == S_NULL)
    {   // Remove mobj.
        mobj->state = (state_t *) S_NULL;
        P_MobjRemove(mobj, false);
        return false;
    }

    mobj->turnTime = false; // $visangle-facetarget
    P_MobjSetState(mobj, state);

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

    if(mo->type == MT_WHIRLWIND)
    {
        if(++mo->special2 < 60)
        {
            return;
        }
    }

    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }

    if(mo->info->deathSound)
    {
        S_StartSound(mo->info->deathSound, mo);
    }
}

void P_FloorBounceMissile(mobj_t* mo)
{
    mo->mom[MZ] = -mo->mom[MZ];
    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));
}

void P_ThrustMobj(mobj_t* mo, angle_t angle, float move)
{
    uint                an = angle >> ANGLETOFINESHIFT;

    mo->mom[MX] += move * FIX2FLT(finecosine[an]);
    mo->mom[MY] += move * FIX2FLT(finesine[an]);
}

/**
 * @param delta         The amount 'source' needs to turn.
 *
 * @return              @c 1, = 'source' needs to turn clockwise, or
 *                      @c 0, = 'source' needs to turn counter clockwise.
 */
int P_FaceMobj(mobj_t* source, mobj_t* target, angle_t* delta)
{
    angle_t             diff, angle1, angle2;

    angle1 = source->angle;
    angle2 = R_PointToAngle2(source->pos[VX], source->pos[VY],
                             target->pos[VX], target->pos[VY]);
    if(angle2 > angle1)
    {
        diff = angle2 - angle1;
        if(diff > ANGLE_180)
        {
            *delta = ANGLE_MAX - diff;
            return 0;
        }
        else
        {
            *delta = diff;
            return 1;
        }
    }
    else
    {
        diff = angle1 - angle2;
        if(diff > ANGLE_180)
        {
            *delta = ANGLE_MAX - diff;
            return 1;
        }
        else
        {
            *delta = diff;
            return 0;
        }
    }
}

/**
 * The missile tracer field must be the target.
 *
 * @return              @c true, if target was tracked else @c false.
 */
boolean P_SeekerMissile(mobj_t* actor, angle_t thresh, angle_t turnMax)
{
    int                 dir;
    uint                an;
    float               dist;
    angle_t             delta;
    mobj_t*             target;

    target = actor->tracer;
    if(target == NULL)
        return false;

    if(!(target->flags & MF_SHOOTABLE))
    {   // Target died.
        actor->tracer = NULL;
        return false;
    }

    dir = P_FaceMobj(actor, target, &delta);
    if(delta > thresh)
    {
        delta >>= 1;
        if(delta > turnMax)
        {
            delta = turnMax;
        }
    }

    if(dir)
    {   // Turn clockwise.
        actor->angle += delta;
    }
    else
    {   // Turn counter clockwise.
        actor->angle -= delta;
    }

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = actor->info->speed * FIX2FLT(finecosine[an]);
    actor->mom[MY] = actor->info->speed * FIX2FLT(finesine[an]);

    if(actor->pos[VZ] + actor->height < target->pos[VZ] ||
       target->pos[VZ] + target->height < actor->pos[VZ])
    {   // Need to seek vertically.
        dist = P_ApproxDistance(target->pos[VX] - actor->pos[VX],
                                target->pos[VY] - actor->pos[VY]);
        dist /= actor->info->speed;
        if(dist < 1)
            dist = 1;

        actor->mom[MZ] = (target->pos[VZ] - actor->pos[VZ]) / dist;
    }

    return true;
}

/**
 * Wind pushes the mobj, if its sector special is a wind type.
 */
void P_WindThrust(mobj_t *mo)
{
    static int          windTab[3] = { 2048 * 5, 2048 * 10, 2048 * 25 };

    sector_t           *sec = P_GetPtrp(mo->subsector, DMU_SECTOR);
    int                 special = P_ToXSector(sec)->special;

    switch(special)
    {
    case 40: // Wind_East
    case 41:
    case 42:
        P_ThrustMobj(mo, 0, FIX2FLT(windTab[special - 40]));
        break;

    case 43: // Wind_North
    case 44:
    case 45:
        P_ThrustMobj(mo, ANG90, FIX2FLT(windTab[special - 43]));
        break;

    case 46: // Wind_South
    case 47:
    case 48:
        P_ThrustMobj(mo, ANG270, FIX2FLT(windTab[special - 46]));
        break;

    case 49: // Wind_West
    case 50:
    case 51:
        P_ThrustMobj(mo, ANG180, FIX2FLT(windTab[special - 49]));
        break;

    default:
        break;
    }
}

float P_MobjGetFriction(mobj_t *mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) && !mo->onMobj)
    {
        return FRICTION_FLY;
    }
    else
    {
        sector_t           *sec = P_GetPtrp(mo->subsector, DMU_SECTOR);

        if(P_ToXSector(sec)->special == 15)
        {
            return FRICTION_LOW;
        }

        return XS_Friction(sec);
    }
}

void P_MobjMoveXY(mobj_t* mo)
{
    float               pos[2], mom[2];
    player_t*           player;
    boolean             largeNegative;

    // $democam: cameramen have their own movement code
    if(P_CameraXYMovement(mo))
        return;

    mom[MX] = MINMAX_OF(-MAXMOVE, mo->mom[MX], MAXMOVE);
    mom[MY] = MINMAX_OF(-MAXMOVE, mo->mom[MY], MAXMOVE);
    mo->mom[MX] = mom[MX];
    mo->mom[MY] = mom[MY];

    if(mom[MX] == 0 && mom[MY] == 0)
    {
        if(mo->flags & MF_SKULLFLY)
        {   // A flying mobj slammed into something.
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
            P_MobjChangeState(mo, P_GetState(mo->type, SN_SEE));
        }

        return;
    }

    if(mo->flags2 & MF2_WINDTHRUST)
        P_WindThrust(mo);

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
            pos[VX] = mo->pos[VX] + (mom[MX] /= 2);
            pos[VY] = mo->pos[VY] + (mom[MY] /= 2);
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

        // $dropoff_fix
        if(!P_TryMove(mo, pos[VX], pos[VY], true, false))
        {   // Blocked mom.
            if(mo->flags2 & MF2_SLIDE)
            {   // Try to slide along it.
                P_SlideMove(mo);
            }
            else if(mo->flags & MF_MISSILE)
            {   // Explode a missile
                sector_t*           backSec;

                //// kludge: Prevent missiles exploding against the sky.
                if(ceilingLine &&
                   (backSec = P_GetPtrp(ceilingLine, DMU_BACK_SECTOR)))
                {
                    if((P_GetIntp(P_GetPtrp(backSec, DMU_CEILING_MATERIAL),
                                    DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->pos[VZ] > P_GetFloatp(backSec, DMU_CEILING_HEIGHT))
                    {
                        if(mo->type == MT_BLOODYSKULL)
                        {
                            mo->mom[MX] = mo->mom[MY] = 0;
                            mo->mom[MZ] = -1;
                        }
                        else
                        {
                            P_MobjRemove(mo, false);
                        }

                        return;
                    }
                }

                if(floorLine &&
                   (backSec = P_GetPtrp(floorLine, DMU_BACK_SECTOR)))
                {
                    if((P_GetIntp(P_GetPtrp(backSec, DMU_FLOOR_MATERIAL),
                                    DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->pos[VZ] < P_GetFloatp(backSec, DMU_FLOOR_HEIGHT))
                    {
                        if(mo->type == MT_BLOODYSKULL)
                        {
                            mo->mom[MX] = mo->mom[MY] = 0;
                            mo->mom[MZ] = -1;
                        }
                        else
                        {
                            P_MobjRemove(mo, false);
                        }

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
        // Debug option for no sliding at all
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }

    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
    {   // No friction for missiles.
        return;
    }

    if(mo->pos[VZ] > mo->floorZ && !mo->onMobj && !(mo->flags2 & MF2_FLY))
    {   // No friction when falling.
        return;
    }

    if(cfg.slidingCorpses)
    {
        /**
         * $dropoff_fix:
         * Add objects falling off ledges. Does not apply to players!
         */

        if(((mo->flags & MF_CORPSE) || (mo->intFlags & MIF_FALLING)) &&
           !mo->player)
        {   // Do not stop sliding.
            // If halfway off a step with some momentum.
            if(mo->mom[MX] > 1.0f / 4 || mo->mom[MX] < -1.0f / 4 ||
               mo->mom[MY] > 1.0f / 4 || mo->mom[MY] < -1.0f / 4)
            {
                if(mo->floorZ !=
                   P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
                    return;
            }
        }
    }

    // Stop player walking animation.
    if(player && !player->plr->cmd.forwardMove && !player->plr->cmd.sideMove &&
       mo->mom[MX] > -STANDSPEED && mo->mom[MX] < STANDSPEED &&
       mo->mom[MY] > -STANDSPEED && mo->mom[MY] < STANDSPEED)
    {
        // If in a walking frame, stop moving.
        if((unsigned) ((player->plr->mo->state - STATES) - PCLASS_INFO(player->class_)->runState) < 4)
            P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class_)->normalState);
    }

    if((!player || (player->plr->cmd.forwardMove == 0 && player->plr->cmd.sideMove == 0)) &&
       mo->mom[MX] > -STOPSPEED && mo->mom[MX] < STOPSPEED &&
       mo->mom[MY] > -STOPSPEED && mo->mom[MY] < STOPSPEED)
    {
        mo->mom[MX] = 0;
        mo->mom[MY] = 0;
    }
    else
    {
        if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) &&
           !mo->onMobj)
        {
            mo->mom[MX] *= FRICTION_FLY;
            mo->mom[MY] *= FRICTION_FLY;
        }
        else
        {
#if __JHERETIC__
            if(P_ToXSector(P_GetPtrp(mo->subsector, DMU_SECTOR))->special == 15)
            {
                // Friction_Low
                mo->mom[MX] *= FRICTION_LOW;
                mo->mom[MY] *= FRICTION_LOW;
            }
            else
#endif
            {
                float       friction = P_MobjGetFriction(mo);

                mo->mom[MX] *= friction;
                mo->mom[MY] *= friction;
            }
        }
    }
}

void P_MobjMoveZ(mobj_t *mo)
{
    float               gravity;
    float               dist;
    float               delta;

    // $democam: cameramen get special z movement
    if(P_CameraZMovement(mo))
        return;

    gravity = XS_Gravity(P_GetPtrp(mo->subsector, DMU_SECTOR));

    // Check for smooth step up.
    if(mo->player && mo->pos[VZ] < mo->floorZ)
    {
        mo->dPlayer->viewHeight -= mo->floorZ - mo->pos[VZ];

        mo->dPlayer->viewHeightDelta =
            (cfg.plrViewHeight - mo->dPlayer->viewHeight) / 8;
    }

    // Adjust height.
    mo->pos[VZ] += mo->mom[MZ];
    if((mo->flags2 & MF2_FLY) &&
       mo->onMobj && mo->pos[VZ] > mo->onMobj->pos[VZ] + mo->onMobj->height)
        mo->onMobj = NULL; // We were on a mobj, we are NOT now.

    if((mo->flags & MF_FLOAT) && mo->target && !P_MobjIsCamera(mo->target))
    {
        // Float down towards target if too close.
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist = P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                    mo->pos[VY] - mo->target->pos[VY]);

            delta = (mo->target->pos[VZ] + mo->target->height /2) -
                    (mo->pos[VZ] + mo->height /2);

            if(dist < mo->radius + mo->target->radius &&
               fabs(delta) < mo->height + mo->target->height)
            {   // Don't go INTO the target.
                delta = 0;
            }

            if(delta < 0 && dist < -(delta * 3))
            {
                mo->pos[VZ] -= FLOATSPEED;
                P_MobjSetSRVOZ(mo, -FLOATSPEED);
            }
            else if(delta > 0 && dist < (delta * 3))
            {
                mo->pos[VZ] += FLOATSPEED;
                P_MobjSetSRVOZ(mo, FLOATSPEED);
            }
        }
    }

    // Do some fly-bobbing.
    if(mo->player && (mo->flags2 & MF2_FLY) && mo->pos[VZ] > mo->floorZ &&
       !mo->onMobj && (mapTime & 2))
    {
        mo->pos[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK]);
    }

    // Clip movement. Another thing?
    if(mo->onMobj && mo->pos[VZ] <= mo->onMobj->pos[VZ] + mo->onMobj->height)
    {
        if(mo->mom[MZ] < 0)
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down. Decrease viewheight for a moment after
                // hitting the ground (hard), and utter appropriate sound.
                mo->dPlayer->viewHeightDelta = mo->mom[MZ] / 8;

                if(mo->player->health > 0)
                    S_StartSound(SFX_PLROOF, mo);
            }
            mo->mom[MZ] = 0;
        }

        if(mo->mom[MZ] == 0)
            mo->pos[VZ] = mo->onMobj->pos[VZ] + mo->onMobj->height;

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }

    // The floor.
    if(mo->pos[VZ] <= mo->floorZ)
    {   // Hit the floor.
        boolean             movingDown;

        // Note (id):
        //  somebody left this after the setting mom[MZ] to 0,
        //  kinda useless there.
        //
        // cph - This was the a bug in the linuxdoom-1.10 source which
        //  caused it not to sync Doom 2 v1.9 demos. Someone
        //  added the above comment and moved up the following code. So
        //  demos would desync in close lost soul fights.
        // Note that this only applies to original Doom 1 or Doom2 demos - not
        //  Final Doom and Ultimate Doom.  So we test demo_compatibility *and*
        //  gamemission. (Note we assume that Doom1 is always Ult Doom, which
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
        int correct_lost_soul_bounce = false;

        if(correct_lost_soul_bounce && (mo->flags & MF_SKULLFLY))
        {
            // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(movingDown = (mo->mom[MZ] < 0))
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down. Decrease viewheight for a moment after
                // hitting the ground hard and utter appropriate sound.
                mo->player->plr->viewHeightDelta = mo->mom[MZ] / 8;
#if __JHERETIC__
                mo->player->jumpTics = 12; // Can't jump in a while.
#endif
                // Fix DOOM bug - dead players grunting when hitting the ground
                // (e.g., after an archvile attack)
                if(mo->player->health > 0)
                    S_StartSound(SFX_PLROOF, mo);
            }
        }

        mo->pos[VZ] = mo->floorZ;

        if(movingDown)
            P_HitFloor(mo);

        // cph 2001/05/26 -
        // See lost soul bouncing comment above. We need this here for bug
        // compatibility with original Doom2 v1.9 - if a soul is charging and
        // hit by a raising floor this incorrectly reverses its Y momentum.

        if(!correct_lost_soul_bounce && (mo->flags & MF_SKULLFLY))
            mo->mom[MZ] = -mo->mom[MZ];

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            if(mo->flags2 & MF2_FLOORBOUNCE)
            {
                P_FloorBounceMissile(mo);
                return;
            }
#if __JHERETIC__
            else if(mo->type == MT_MNTRFX2)
            {   // Minotaur floor fire can go up steps
                return;
            }
            else
#endif
            {
                P_ExplodeMissile(mo);
                return;
            }
        }

        if(movingDown && mo->mom[MZ] < 0)
            mo->mom[MZ] = 0;

#if __JHERETIC__
        {
        statenum_t              state;

        if((state = P_GetState(mo->type, SN_CRASH)) != S_NULL &&
           (mo->flags & MF_CORPSE))
        {
            P_MobjChangeState(mo, state);
            return;
        }
        }
#endif
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -(gravity / 8) * 2;
        else
            mo->mom[MZ] -= gravity / 8;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -gravity * 2;
        else
            mo->mom[MZ] -= gravity;
    }

    if(mo->pos[VZ] + mo->height > mo->ceilingZ)
    {
        // hit the ceiling
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->pos[VZ] = mo->ceilingZ - mo->height;

        if(mo->flags & MF_SKULLFLY)
        {                       // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            if(P_GetIntp(P_GetPtrp(mo->subsector, DMU_CEILING_MATERIAL),
                           DMU_FLAGS) & MATF_SKYMASK)
            {
#if __JHERETIC__
                if(mo->type == MT_BLOODYSKULL)
                {
                    mo->mom[MX] = mo->mom[MY] = 0;
                    mo->mom[MZ] = -1;
                }
                else
#endif
                // Don't explode against sky.
                {
                    P_MobjRemove(mo, false);
                }
                return;
            }

            P_ExplodeMissile(mo);
            return;
        }
    }
}

void P_NightmareRespawn(mobj_t* mobj)
{
    mobj_t*             mo;

    // Something is occupying it's position?
    if(!P_CheckPosition2f(mobj, mobj->spawnSpot.pos[VX],
                          mobj->spawnSpot.pos[VY]))
        return; // No respwan.

    // Spawn a teleport fog at old spot because of removal of the body?
    mo = P_SpawnMobj3f(MT_TFOG, mobj->pos[VX], mobj->pos[VY], TELEFOGHEIGHT,
                       mobj->angle, MTF_Z_FLOOR);
    // Initiate teleport sound.
    S_StartSound(SFX_TELEPT, mo);

    // Spawn a teleport fog at the new spot.
    mo = P_SpawnMobj3f(MT_TFOG, mobj->spawnSpot.pos[VX],
                       mobj->spawnSpot.pos[VY], TELEFOGHEIGHT,
                       mobj->spawnSpot.angle, MTF_Z_FLOOR);

    S_StartSound(SFX_TELEPT, mo);

    // Inherit attributes from deceased one.
    mo = P_SpawnMobj3fv(mobj->type, mobj->spawnSpot.pos,
                        mobj->spawnSpot.angle, mobj->spawnSpot.flags);

    memcpy(&mo->spawnSpot, &mobj->spawnSpot, sizeof(mo->spawnSpot));

    mo->reactionTime = 18;

    // Remove the old monster.
    P_MobjRemove(mobj, true);
}

void P_MobjThinker(mobj_t *mobj)
{
    if(mobj->ddFlags & DDMF_REMOTE)
        return; // Remote mobjs are handled separately.

    if(mobj->type == MT_BLASTERFX1)
    {
        int                 i;
        float               frac[3];
        float               z;
        boolean             changexy;

        // Handle movement
        if(mobj->mom[MX] != 0 || mobj->mom[MY] != 0 || mobj->mom[MZ] != 0 ||
           (mobj->pos[VZ] != mobj->floorZ))
        {
            frac[MX] = mobj->mom[MX] / 8;
            frac[MY] = mobj->mom[MY] / 8;
            frac[MZ] = mobj->mom[MZ] / 8;

            changexy = (frac[MX] != 0 || frac[MY] != 0);
            for(i = 0; i < 8; ++i)
            {
                if(changexy)
                {
                    if(!P_TryMove(mobj, mobj->pos[VX] + frac[MX],
                                  mobj->pos[VY] + frac[MY], false, false))
                    {   // Blocked move.
                        P_ExplodeMissile(mobj);
                        return;
                    }
                }

                mobj->pos[VZ] += frac[MZ];
                if(mobj->pos[VZ] <= mobj->floorZ)
                {   // Hit the floor.
                    mobj->pos[VZ] = mobj->floorZ;
                    P_HitFloor(mobj);
                    P_ExplodeMissile(mobj);
                    return;
                }

                if(mobj->pos[VZ] + mobj->height > mobj->ceilingZ)
                {   // Hit the ceiling.
                    mobj->pos[VZ] = mobj->ceilingZ - mobj->height;
                    P_ExplodeMissile(mobj);
                    return;
                }

                if(changexy && (P_Random() < 64))
                {
                    z = mobj->pos[VZ] - 8;
                    if(z < mobj->floorZ)
                    {
                        z = mobj->floorZ;
                    }

                    P_SpawnMobj3f(MT_BLASTERSMOKE, mobj->pos[VX], mobj->pos[VY],
                                  z, P_Random() << 24, 0);
                }
            }
        }

        // Advance the state.
        if(mobj->tics != -1)
        {
            mobj->tics--;
            while(!mobj->tics)
            {
                if(!P_MobjChangeState(mobj, mobj->state->nextState))
                {   // Mobj was removed.
                    return;
                }
            }
        }

        return;
    }

#if __JDOOM__
    // Spectres get selector = 1.
    if(mobj->type == MT_SHADOWS)
        mobj->selector = (mobj->selector & ~DDMOBJ_SELECTOR_MASK) | 1;
#endif

    // The first three bits of the selector special byte contain a relative
    // health level.
    P_UpdateHealthBits(mobj);

    // Handle X and Y momentums.
    if(mobj->mom[MX] != 0 || mobj->mom[MY] != 0 ||
       (mobj->flags & MF_SKULLFLY))
    {
        P_MobjMoveXY(mobj);

        //// \fixme decent NOP/NULL/Nil function pointer please.
        if(mobj->thinker.function == NOPFUNC)
            return; // Mobj was removed.
    }

    if(mobj->flags2 & MF2_FLOATBOB)
    {   // Floating item bobbing motion.
        // Keep it on the floor.
        mobj->pos[VZ] = mobj->floorZ;

        // Negative floorclip raises the mobj off the floor.
        mobj->floorClip = -mobj->special1;
        if(mobj->floorClip < -MAX_BOB_OFFSET)
        {
            // We don't want it going through the floor.
            mobj->floorClip = -MAX_BOB_OFFSET;
        }
    }
    else if(mobj->pos[VZ] != mobj->floorZ || mobj->mom[MZ] != 0)
    {
        P_MobjMoveZ(mobj);
        if(mobj->thinker.function != P_MobjThinker)
            return; // mobj was removed
    }
    // Non-sentient objects at rest.
    else if(!(mobj->mom[MX] != 0 || mobj->mom[MY] != 0) && !sentient(mobj) &&
            !(mobj->player) && !((mobj->flags & MF_CORPSE) &&
            cfg.slidingCorpses))
    {
        /**
         * Objects fall off ledges if they are hanging off slightly push off
         * of ledge if hanging more than halfway off.
         */

        if(mobj->pos[VZ] > mobj->dropOffZ && // Only objects contacting dropoff
           !(mobj->flags & MF_NOGRAVITY) && cfg.fallOff)
        {
            P_ApplyTorque(mobj);
        }
        else
        {
            mobj->intFlags &= ~MIF_FALLING;
            mobj->gear = 0; // Reset torque.
        }
    }

    if(cfg.slidingCorpses)
    {
        if(((mobj->flags & MF_CORPSE) ? mobj->pos[VZ] > mobj->dropOffZ :
                                        mobj->pos[VZ] - mobj->dropOffZ > 24) && // Only objects contacting drop off.
           !(mobj->flags & MF_NOGRAVITY)) // Only objects which fall.
        {
            P_ApplyTorque(mobj); // Apply torque.
        }
        else
        {
            mobj->intFlags &= ~MIF_FALLING;
            mobj->gear = 0; // Reset torque.
        }
    }

    // $vanish: dead monsters disappear after some time.
    if(cfg.corpseTime && (mobj->flags & MF_CORPSE) && mobj->corpseTics != -1)
    {
        if(++mobj->corpseTics < cfg.corpseTime * TICSPERSEC)
        {
            mobj->translucency = 0; // Opaque.
        }
        else if(mobj->corpseTics < cfg.corpseTime * TICSPERSEC + VANISHTICS)
        {
            // Translucent during vanishing.
            mobj->translucency =
                ((mobj->corpseTics -
                  cfg.corpseTime * TICSPERSEC) * 255) / VANISHTICS;
        }
        else
        {
            // Too long; get rid of the corpse.
            mobj->corpseTics = -1;
            return;
        }
    }

    // Cycle through states, calling action functions at transitions.
    if(mobj->tics != -1)
    {
        mobj->tics--;

        P_MobjAngleSRVOTicker(mobj); // "angle-servo"; smooth actor turning.

        // You can cycle through multiple states in a tic.
        if(!mobj->tics)
        {
            P_MobjClearSRVO(mobj);
            if(!P_MobjChangeState(mobj, mobj->state->nextState))
                return; // Freed itself.
        }
    }
    else if(!IS_CLIENT)
    {
        // Check for nightmare respawn.
        if(!(mobj->flags & MF_COUNTKILL))
            return;

        if(!respawnMonsters)
            return;

        mobj->moveCount++;

        if(mobj->moveCount < 12 * 35)
            return;

        if(mapTime & 31)
            return;

        if(P_Random() > 4)
            return;

        P_NightmareRespawn(mobj);
    }
}

/**
 * Spawns a mobj of "type" at the specified position.
 */
mobj_t* P_SpawnMobj3f(mobjtype_t type, float x, float y, float z,
                      angle_t angle, int spawnFlags)
{
    mobj_t*             mo;
    mobjinfo_t*         info = &MOBJINFO[type];
    float               space;
    int                 ddflags = 0;

#ifdef _DEBUG
    if(type < 0 || type >= Get(DD_NUMMOBJTYPES))
        Con_Error("P_SpawnMobj: Illegal mo type %i.\n", type);
#endif

    if(info->flags & MF_SOLID)
        ddflags |= DDMF_SOLID;
    if(info->flags & MF_NOBLOCKMAP)
        ddflags |= DDMF_NOBLOCKMAP;
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

    if(gameSkill != SM_NIGHTMARE)
        mo->reactionTime = info->reactionTime;

    mo->lastLook = P_Random() % MAXPLAYERS;

    // Must link before setting state (ID assigned for the mo).
    P_MobjSetState(mo, P_GetState(mo->type, SN_SPAWN));

    if(mo->type == MT_MACEFX1 || mo->type == MT_MACEFX2 ||
       mo->type == MT_MACEFX3)
        mo->special3 = 1000;

    // Set subsector and/or block links.
    P_MobjSetPosition(mo);

    mo->floorZ   = P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT);
    mo->dropOffZ = mo->floorZ;
    mo->ceilingZ = P_GetFloatp(mo->subsector, DMU_CEILING_HEIGHT);

    if((spawnFlags & MTF_Z_CEIL) || (info->flags & MF_SPAWNCEILING))
    {
        mo->pos[VZ] = mo->ceilingZ - mo->info->height - z;
    }
    else if((spawnFlags & MTF_Z_RANDOM) || (info->flags2 & MF2_SPAWNFLOAT))
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
    else if(spawnFlags & MTF_Z_FLOOR)
    {
        mo->pos[VZ] = mo->floorZ + z;
    }

    if(spawnFlags & MTF_AMBUSH)
        mo->flags |= MF_AMBUSH;

    mo->floorClip = 0;

    if((mo->flags2 & MF2_FLOORCLIP) &&
       mo->pos[VZ] == P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
    {
        const terraintype_t* tt = P_MobjGetFloorTerrainType(mo);

        if(tt->flags & TTF_FLOORCLIP)
        {
            mo->floorClip = 10;
        }
    }

    return mo;
}

mobj_t* P_SpawnMobj3fv(mobjtype_t type, float pos[3], angle_t angle,
                       int spawnFlags)
{
    return P_SpawnMobj3f(type, pos[VX], pos[VY], pos[VZ], angle, spawnFlags);
}

/**
 * Queue up a spawn from the specified spot.
 */
void P_RespawnEnqueue(spawnspot_t* spot)
{
    spawnspot_t*        spawnObj = &itemRespawnQueue[itemRespawnQueueHead];

    memcpy(spawnObj, spot, sizeof(*spawnObj));

    itemRespawnTime[itemRespawnQueueHead] = mapTime;
    itemRespawnQueueHead = (itemRespawnQueueHead + 1) & (ITEMQUEUESIZE - 1);

    // Lose one off the end?
    if(itemRespawnQueueHead == itemRespawnQueueTail)
        itemRespawnQueueTail = (itemRespawnQueueTail + 1) & (ITEMQUEUESIZE - 1);
}

void P_EmptyRespawnQueue(void)
{
    itemRespawnQueueHead = itemRespawnQueueTail = 0;
}

/**
 * Called when a player is spawned on the level. Most of the player
 * structure stays unchanged between levels.
 */
void P_SpawnPlayer(spawnspot_t* spot, int plrnum)
{
    player_t*           p;
    float               pos[3];
    mobj_t*             mobj;
    int                 i, spawnFlags = 0;

    if(!players[plrnum].plr->inGame)
        return; // Not playing.

    p = &players[plrnum];

    Con_Printf("P_SpawnPlayer: spawning player %i, col=%i.\n", plrnum,
               cfg.playerColor[plrnum]);

    if(p->playerState == PST_REBORN)
        G_PlayerReborn(plrnum);

    if(spot)
    {
        pos[VX] = spot->pos[VX];
        pos[VY] = spot->pos[VY];
        pos[VZ] = spot->pos[VZ];
        spawnFlags = spot->flags;
    }
    else
    {
        pos[VX] = pos[VY] = pos[VZ] = 0;
        spawnFlags |= MTF_Z_FLOOR;
    }

    /* $unifiedangles */
    mobj = P_SpawnMobj3fv(MT_PLAYER, pos, (spot? spot->angle : 0), spawnFlags);

    // On clients all player mobjs are remote, even the CONSOLEPLAYER.
    if(IS_CLIENT)
    {
        mobj->flags &= ~MF_SOLID;
        mobj->ddFlags = DDMF_REMOTE | DDMF_DONTDRAW;
        // The real flags are received from the server later on.
    }

    // Set color translations for player sprites.
    i = cfg.playerColor[plrnum];
    if(i > 0)
        mobj->flags |= i << MF_TRANSSHIFT;
    p->plr->lookDir = 0; /* $unifiedangles */
    p->plr->lookDir = 0;
    p->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    p->jumpTics = 0;
    p->airCounter = 0;
    mobj->player = p;
    mobj->dPlayer = p->plr;
    mobj->health = p->health;
    p->plr->mo = mobj;
    p->playerState = PST_LIVE;
    p->refire = 0;
    p->damageCount = 0;
    p->bonusCount = 0;
    p->morphTics = 0;
    p->rain1 = NULL;
    p->rain2 = NULL;
    p->plr->extraLight = 0;
    p->plr->fixedColorMap = 0;

    if(!spot)
        p->plr->flags |= DDPF_CAMERA;

    if(p->plr->flags & DDPF_CAMERA)
    {
        p->plr->mo->pos[VZ] += (float) cfg.plrViewHeight;
        p->plr->viewHeight = 0;
    }
    else
        p->plr->viewHeight = (float) cfg.plrViewHeight;

    P_SetupPsprites(p); // Setup gun psprite.

    p->class_ = PCLASS_PLAYER;

    if(deathmatch)
    {   // Give all keys in death match mode.
        for(i = 0; i < NUM_KEY_TYPES; ++i)
        {
            p->keys[i] = true;
        }
    }

    // Wake up the status bar.
    ST_Start(p - players);

    // Wake up the heads up text.
    HU_Start(p - players);
}

void P_SpawnMapThing(spawnspot_t* th)
{
    int                 i, bit;
    mobj_t*             mobj;

    /*Con_Message("x = %g, y = %g, height = %g, angle = %i, type = %i, options = %i\n",
                  mthing->pos[VX], mthing->pos[VY], mthing->height, mthing->angle, mthing->type,
                  mthing->flags);*/

    // Count deathmatch start positions.
    if(th->type == 11)
    {
        if(deathmatchP < &deathmatchStarts[MAX_DM_STARTS])
        {
            memcpy(deathmatchP, th, sizeof(*th));
            deathmatchP++;
        }
        return;
    }

    // Check for players specially.
    if(th->type <= 4)
    {
        // Register this player start.
        P_RegisterPlayerStart(th);
        return;
    }

    // Ambient sound sequences.
    if(th->type >= 1200 && th->type < 1300)
    {
        P_AddAmbientSfx(th->type - 1200);
        return;
    }

    // Check for boss spots.
    if(th->type == 56)
    {
        P_AddBossSpot(th->pos[VX], th->pos[VY], th->angle);
        return;
    }

    if(!IS_NETGAME && (th->flags & MTF_NOTSINGLE))
        return;

    // Check for apropriate skill level.
    if(gameSkill == SM_BABY)
        bit = 1;
    else if(gameSkill == SM_NIGHTMARE)
        bit = 4;
    else
        bit = 1 << (gameSkill - 1);
    if(!(th->flags & bit))
        return;

    // Find which type to spawn.
    for(i = 0; i < Get(DD_NUMMOBJTYPES); ++i)
    {
        if(th->type == MOBJINFO[i].doomedNum)
            break;
    }

    // Clients only spawn local objects.
    if(IS_CLIENT)
    {
        if(!(MOBJINFO[i].flags & MF_LOCAL))
            return;
    }

    if(i == Get(DD_NUMMOBJTYPES))
    {
        Con_Error("P_SpawnMapThing: Unknown type %i at (%g, %g)", th->type,
                  th->pos[VX], th->pos[VY]);
    }

    // Don't spawn keys and players in deathmatch.
    if(deathmatch && (MOBJINFO[i].flags & MF_NOTDMATCH))
        return;

    // Don't spawn any monsters if -nomonsters.
    if(noMonstersParm && (MOBJINFO[i].flags & MF_COUNTKILL))
        return;

    switch(i)
    {
    case MT_WSKULLROD:
    case MT_WPHOENIXROD:
    case MT_AMSKRDWIMPY:
    case MT_AMSKRDHEFTY:
    case MT_AMPHRDWIMPY:
    case MT_AMPHRDHEFTY:
    case MT_AMMACEWIMPY:
    case MT_AMMACEHEFTY:
    case MT_ARTISUPERHEAL:
    case MT_ARTITELEPORT:
    case MT_ITEMSHIELD2:
        if(gameMode == shareware)
        {   // Don't place on map in shareware version.
            return;
        }
        break;

    case MT_WMACE:
        if(gameMode != shareware)
        {   // Put in the mace spot list.
            P_AddMaceSpot(th);
            return;
        }
        return;

    default:
        break;
    }

    mobj = P_SpawnMobj3fv(i, th->pos, th->angle, th->flags);

    if(mobj->tics > 0)
        mobj->tics = 1 + (P_Random() % mobj->tics);
    if(mobj->flags & MF_COUNTKILL)
        totalKills++;
    if(mobj->flags & MF_COUNTITEM)
        totalItems++;

    // Set the spawn info for this mobj.
    memcpy(&mobj->spawnSpot, th, sizeof(mobj->spawnSpot));
}

/**
 * Chooses the next spot to place the mace.
 */
void P_RepositionMace(mobj_t *mo)
{
    int                 spot;
    subsector_t*        ss;

    P_MobjUnsetPosition(mo);
    spot = P_Random() % maceSpotCount;
    mo->pos[VX] = maceSpots[spot].pos[VX];
    mo->pos[VY] = maceSpots[spot].pos[VY];
    ss = R_PointInSubsector(mo->pos[VX], mo->pos[VY]);

    mo->floorZ = P_GetFloatp(ss, DMU_CEILING_HEIGHT);
    mo->pos[VZ] = mo->floorZ;

    mo->ceilingZ = P_GetFloatp(ss, DMU_CEILING_HEIGHT);
    P_MobjSetPosition(mo);
}

void P_SpawnPuff(float x, float y, float z, angle_t angle)
{
    mobj_t*             puff;

    // Clients do not spawn puffs.
    if(IS_CLIENT)
        return;

    z += FIX2FLT((P_Random() - P_Random()) << 10);
    puff = P_SpawnMobj3f(puffType, x, y, z, angle, 0);
    if(puff->info->attackSound)
    {
        S_StartSound(puff->info->attackSound, puff);
    }

    switch(puffType)
    {
    case MT_BEAKPUFF:
    case MT_STAFFPUFF:
        puff->mom[MZ] = 1;
        break;

    case MT_GAUNTLETPUFF1:
    case MT_GAUNTLETPUFF2:
        puff->mom[MZ] = .8f;

    default:
        break;
    }
}

void P_SpawnBloodSplatter(float x, float y, float z, mobj_t *originator)
{
    mobj_t             *mo;

    mo = P_SpawnMobj3f(MT_BLOODSPLATTER, x, y, z, P_Random() << 24, 0);

    mo->target = originator;
    mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
    mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
    mo->mom[MZ] = 2;
}

void P_RipperBlood(mobj_t *mo)
{
    mobj_t             *th;
    float               pos[3];

    memcpy(pos, mo->pos, sizeof(pos));

    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VZ] += FIX2FLT((P_Random() - P_Random()) << 12);

    th = P_SpawnMobj3fv(MT_BLOOD, pos, P_Random() << 24, 0);

    th->flags |= MF_NOGRAVITY;
    th->mom[MX] = mo->mom[MX] / 2;
    th->mom[MY] = mo->mom[MY] / 2;
    th->tics += P_Random() & 3;
}

#define SMALLSPLASHCLIP 12;

/**
 * @return              @c true, if mobj contacted a non-solid floor.
 */
boolean P_HitFloor(mobj_t* thing)
{
    mobj_t*             mo;
    const terraintype_t* tt;

    if(thing->floorZ != P_GetFloatp(thing->subsector, DMU_FLOOR_HEIGHT))
    {
        // Don't splash if landing on the edge above water/lava/etc....
        return false;
    }

    // Things that don't splash go here.
    switch(thing->type)
    {
    case MT_LAVASMOKE:
    case MT_SPLASH:
    case MT_SLUDGECHUNK:
        return false;

    default:
        if(P_MobjIsCamera(thing))
            return false;
        break;
    }

    tt = P_MobjGetFloorTerrainType(thing);
    if(tt->flags & TTF_SPAWN_SPLASHES)
    {
        mo = P_SpawnMobj3f(MT_SPLASHBASE, thing->pos[VX], thing->pos[VY], 0,
                           thing->angle + ANG180, MTF_Z_FLOOR);
        if(mo)
            mo->floorClip += SMALLSPLASHCLIP;

        mo = P_SpawnMobj3f(MT_SPLASH, thing->pos[VX], thing->pos[VY], 0,
                           thing->angle, MTF_Z_FLOOR);
        if(mo)
        {
            mo->target = thing;
            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MZ] = 2 + FIX2FLT(P_Random() << 8);
        }

        S_StartSound(SFX_GLOOP, mo);
        return true;
    }
    else if(tt->flags & TTF_SPAWN_SMOKE)
    {
        mo = P_SpawnMobj3f(MT_LAVASPLASH, thing->pos[VX], thing->pos[VY], 0,
                           thing->angle + ANG180, MTF_Z_FLOOR);
        if(mo)
            mo->floorClip += SMALLSPLASHCLIP;

        mo = P_SpawnMobj3f(MT_LAVASMOKE, thing->pos[VX], thing->pos[VY], 0,
                           P_Random() << 24, MTF_Z_FLOOR);
        if(mo)
        {
            mo->mom[MZ] = 1 + FIX2FLT((P_Random() << 7));
        }
        S_StartSound(SFX_BURN, mo);
        return true;
    }
    else if(tt->flags & TTF_SPAWN_SLUDGE)
    {
       mo = P_SpawnMobj3f(MT_SLUDGESPLASH, thing->pos[VX], thing->pos[VY], 0,
                          thing->angle + ANG180, MTF_Z_FLOOR);
        if(mo)
            mo->floorClip += SMALLSPLASHCLIP;

        mo = P_SpawnMobj3f(MT_SLUDGECHUNK, thing->pos[VX], thing->pos[VY], 0,
                           P_Random() << 24, MTF_Z_FLOOR);
        if(mo)
        {
            mo->target = thing;
            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MZ] = 1 + FIX2FLT(P_Random() << 8);
        }
        return true;
    }

    return false;
}

/**
 * @return              @c true, if the missile is at a valid spawn point,
 *                      otherwise; explode it and return @false.
 */
boolean P_CheckMissileSpawn(mobj_t *missile)
{
    // Move a little forward so an angle can be computed if it immediately
    // explodes
    if(missile->type == MT_BLASTERFX1)
    {   // Ultra-fast ripper spawning missile.
        missile->pos[VX] += missile->mom[MX] / 8;
        missile->pos[VY] += missile->mom[MY] / 8;
        missile->pos[VZ] += missile->mom[MZ] / 8;
    }
    else
    {
        missile->pos[VX] += missile->mom[MX] / 2;
        missile->pos[VY] += missile->mom[MY] / 2;
        missile->pos[VZ] += missile->mom[MZ] / 2;
    }

    if(!P_TryMove(missile, missile->pos[VX], missile->pos[VY], false, false))
    {
        P_ExplodeMissile(missile);
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
 * @param checkSpawn    @c true call P_CheckMissileSpawn.
 *
 * @return              Pointer to the newly spawned missile.
 */
mobj_t* P_SpawnMissile(mobjtype_t type, mobj_t* source, mobj_t* dest,
                       boolean checkSpawn)
{
    float               pos[3];
    mobj_t*             th = 0;
    unsigned int        an = 0;
    angle_t             angle = 0;
    float               dist = 0;
    float               slope = 0;
    float               spawnZOff = 0;
    int                 spawnFlags = 0;

    memcpy(pos, source->pos, sizeof(pos));

    if(source->player)
    {
        // see which target is to be aimed at
        angle = source->angle;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!cfg.noAutoAim)
            if(!lineTarget)
            {
                angle += 1 << 26;
                slope = P_AimLineAttack(source, angle, 16 * 64);
                if(!lineTarget)
                {
                    angle -= 2 << 26;
                    slope = P_AimLineAttack(source, angle, 16 * 64);
                }

                if(!lineTarget)
                {
                    angle = source->angle;
                    slope =
                        tan(LOOKDIR2RAD(source->dPlayer->lookDir)) / 1.2f;
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
        case MT_MNTRFX1: // Minotaur swing attack missile.
            spawnZOff = 40;
            break;

        case MT_SRCRFX1: // Sorcerer Demon fireball.
            spawnZOff = 48;
            break;

        case MT_KNIGHTAXE: // Knight normal axe.
        case MT_REDAXE: // Knight red power axe.
            spawnZOff = 36;
            break;

        case MT_MNTRFX2:
            spawnZOff = 0;
            break;

        default:
            spawnZOff = 32;
            break;
        }
    }

    if(type == MT_MNTRFX2) // always exactly on the floor.
    {
        spawnFlags |= MTF_Z_FLOOR;
    }
    else
    {
        pos[VZ] += spawnZOff;
        pos[VZ] -= source->floorClip;
    }

    if(!source->player)
    {
        angle = R_PointToAngle2(pos[VX], pos[VY], dest->pos[VX], dest->pos[VY]);
        // Fuzzy player.
        if(dest->flags & MF_SHADOW)
            angle += (P_Random() - P_Random()) << 21; // note << 20 in jDoom
    }

    th = P_SpawnMobj3fv(type, pos, angle, spawnFlags);

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Where it came from.
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    if(source->player)
    {
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
    if(!dist)
        dist = 1;
    dist = th->info->speed / dist;

    th->mom[MX] *= dist;
    th->mom[MY] *= dist;
    th->mom[MZ] *= dist;

#if __JHERETIC__
    //// \kludge Set this global ptr as we need access to the mobj even if it
    //// explodes instantly in order to assign values to it.
    //// This is a bit of a kludge really...
    missileMobj = th;
#endif

    if(checkSpawn)
        return (P_CheckMissileSpawn(th)? th : NULL);

    return th;
}

/**
 * Tries to aim at a nearby monster if 'source' is a player. Else aim is
 * the angle of the source mobj. Z angle is specified with momZ.
 *
 * @param type          The type of mobj to be shot.
 * @param source        The mobj doing the shooting.
 * @param angle         The X/Y angle to shoot the missile in.
 * @param momZ          The Z momentum of the missile to be spawned.
 *
 * @return              Pointer to the newly spawned missile.
 */
mobj_t *P_SpawnMissileAngle(mobjtype_t type, mobj_t *source, angle_t mangle,
                            float momZ)
{
    float               pos[3];
    mobj_t             *th = 0;
    unsigned int        an = 0;
    angle_t             angle = 0;
    float               dist = 0;
    float               slope = 0;
    float               spawnZOff = 0;
    int                 spawnFlags = 0;

    memcpy(pos, source->pos, sizeof(pos));

    angle = mangle;
    if(source->player)
    {
        // Try to find a target.
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!cfg.noAutoAim)
            if(!lineTarget)
            {
                angle += 1 << 26;
                slope = P_AimLineAttack(source, angle, 16 * 64);
                if(!lineTarget)
                {
                    angle -= 2 << 26;
                    slope = P_AimLineAttack(source, angle, 16 * 64);
                }

                if(!lineTarget)
                {
                    angle = mangle;
                    slope =
                        tan(LOOKDIR2RAD(source->dPlayer->lookDir)) / 1.2f;
                }
            }

        if(!(source->player->plr->flags & DDPF_CAMERA))
            spawnZOff = cfg.plrViewHeight - 9 +
                        (source->player->plr->lookDir) / 173;
    }
    else
    {
        // Type specific offset to spawn height z.
        switch(type)
        {
        case MT_MNTRFX1: // Minotaur swing attack missile.
            spawnZOff = 40;
            break;

        case MT_SRCRFX1: // Sorcerer Demon fireball.
            spawnZOff = 48;
            break;

        case MT_KNIGHTAXE: // Knight normal axe.
        case MT_REDAXE: // Knight red power axe.
            spawnZOff = 36;
            break;

        default:
            spawnZOff = 32;
            break;
        }
    }

    if(type == MT_MNTRFX2) // Always exactly on the floor.
    {
        spawnFlags |= MTF_Z_FLOOR;
    }
    else
    {
        pos[VZ] += spawnZOff;
        pos[VZ] -= source->floorClip;
    }

    th = P_SpawnMobj3fv(type, pos, angle, spawnFlags);

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Where it came from.
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    if(source->player && momZ == -12345)
    {
        th->mom[MZ] = th->info->speed * slope;

        // Make sure the speed is right (in 3D).
        dist = P_ApproxDistance(P_ApproxDistance(th->mom[MX], th->mom[MY]),
                                th->mom[MZ]);
        if(dist < 1)
            dist = 1;
        dist = th->info->speed / dist;

        th->mom[MX] *= dist;
        th->mom[MY] *= dist;
        th->mom[MZ] *= dist;
    }
    else
    {
        th->mom[MZ] = momZ;
    }

#if __JHERETIC__
    //// \kludge Set this global ptr as we need access to the mobj even if it
    //// explodes instantly in order to assign values to it.
    //// This is a bit of a kludge really...
    missileMobj = th;
#endif

    if(P_CheckMissileSpawn(th))
        return th;

    return NULL;
}

void C_DECL A_ContMobjSound(mobj_t *actor)
{
    switch(actor->type)
    {
    case MT_KNIGHTAXE:
        S_StartSound(SFX_KGTATK, actor);
        break;

    case MT_MUMMYFX1:
        S_StartSound(SFX_MUMHED, actor);
        break;

    default:
        break;
    }
}
