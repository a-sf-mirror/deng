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
 * p_mobj.c:
 */

#ifdef MSVC
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include "jhexen.h"

#include "../../../engine/portable/include/m_bams.h" // for BANG2RAD

#include "dmu_lib.h"
#include "p_map.h"
#include "p_player.h"
#include "g_common.h"

// MACROS ------------------------------------------------------------------

#define MAX_TID_COUNT           200
#define MAX_BOB_OFFSET          8

#define BLAST_RADIUS_DIST       255
#define BLAST_SPEED             20
#define BLAST_FULLSTRENGTH      255
#define HEAL_RADIUS_DIST        255

#define NOMOMENTUM_THRESHOLD    (0.000001f)
#define STOPSPEED               (0.1f/1.6)

#define SMALLSPLASHCLIP         (12);

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    G_PlayerReborn(int player);
void    P_MarkAsLeaving(mobj_t *corpse);
mobj_t *P_CheckOnMobj(mobj_t *thing);
void    P_BounceWall(mobj_t *mo);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void PlayerLandedOnThing(mobj_t *mo, mobj_t *onmobj);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern mobj_t lavaInflictor;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mobjtype_t PuffType;
mobj_t *MissileMobj;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int TIDList[MAX_TID_COUNT + 1];  // +1 for termination marker
static mobj_t *TIDMobj[MAX_TID_COUNT];

// CODE --------------------------------------------------------------------

const terraintype_t* P_MobjGetFloorTerrainType(mobj_t* mo)
{
    sector_t*           sec = DMU_GetPtrp(mo->subsector, DMU_SECTOR);

    return P_GetPlaneMaterialType(sec, PLN_FLOOR);
}

/**
 * @return              @c true if the mobj is still present.
 */
boolean P_MobjChangeState(mobj_t *mobj, statenum_t state)
{
    state_t*            st;

    if(state == S_NULL)
    {   // Remove mobj.
        mobj->state = (state_t *) S_NULL;
        P_MobjRemove(mobj, false);
        return false;
    }
    st = &STATES[state];

    P_MobjSetState(mobj, state);
    mobj->turnTime = false; // $visangle-facetarget
    if(st->action)
    {   // Call action function.
        st->action(mobj);
    }

    // Return false if the action function removed the mobj.
    return mobj->thinker.function != NOPFUNC;
}

/**
 * Same as P_MobjChangeState, but does not call the state function.
 */
boolean P_SetMobjStateNF(mobj_t* mobj, statenum_t state)
{
    if(state == S_NULL)
    {   // Remove mobj
        mobj->state = (state_t *) S_NULL;
        P_MobjRemove(mobj, false);
        return false;
    }

    mobj->turnTime = false; // $visangle-facetarget
    P_MobjSetState(mobj, state);
    return true;
}

void P_ExplodeMissile(mobj_t* mo)
{
    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }

    switch(mo->type)
    {
    case MT_SORCBALL1:
    case MT_SORCBALL2:
    case MT_SORCBALL3:
        S_StartSound(SFX_SORCERER_BIGBALLEXPLODE, NULL);
        break;

    case MT_SORCFX1:
        S_StartSound(SFX_SORCERER_HEADSCREAM, NULL);
        break;

    default:
        if(mo->info->deathSound)
            S_StartSound(mo->info->deathSound, mo);
        break;
    }
}

void P_FloorBounceMissile(mobj_t* mo)
{
    boolean             shouldSplash = P_HitFloor(mo);

    if(shouldSplash)
    {
        switch(mo->type)
        {
        case MT_SORCFX1:
        case MT_SORCBALL1:
        case MT_SORCBALL2:
        case MT_SORCBALL3:
            break;

        default:
            P_MobjRemove(mo, false);
            return;
        }
    }

    switch(mo->type)
    {
    case MT_SORCFX1:
        mo->mom[MZ] = -mo->mom[MZ]; // No energy absorbed.
        break;

    case MT_SGSHARD1:
    case MT_SGSHARD2:
    case MT_SGSHARD3:
    case MT_SGSHARD4:
    case MT_SGSHARD5:
    case MT_SGSHARD6:
    case MT_SGSHARD7:
    case MT_SGSHARD8:
    case MT_SGSHARD9:
    case MT_SGSHARD0:
        mo->mom[MZ] *= -0.3f;
        if(fabs(mo->mom[MZ]) < 1.0f / 2)
        {
            P_MobjChangeState(mo, S_NULL);
            return;
        }
        break;

    default:
        mo->mom[MZ] *= -0.7f;
        break;
    }

    mo->mom[MX] = 2 * mo->mom[MX] / 3;
    mo->mom[MY] = 2 * mo->mom[MY] / 3;
    if(mo->info->seeSound)
    {
        switch(mo->type)
        {
        case MT_SORCBALL1:
        case MT_SORCBALL2:
        case MT_SORCBALL3:
            if(!mo->args[0])
                S_StartSound(mo->info->seeSound, mo);
            break;

        default:
            S_StartSound(mo->info->seeSound, mo);
            break;
        }

        S_StartSound(mo->info->seeSound, mo);
    }
}

void P_ThrustMobj(mobj_t *mo, angle_t angle, float move)
{
    uint            an = angle >> ANGLETOFINESHIFT;
    mo->mom[MX] += move * FIX2FLT(finecosine[an]);
    mo->mom[MY] += move * FIX2FLT(finesine[an]);
}

/**
 * @param delta         The amount 'source' needs to turn.
 *
 * @return              @c 1, if 'source' needs to turn clockwise, or
 *                      @c 0, if 'source' needs to turn counter clockwise.
 */
int P_FaceMobj(mobj_t *source, mobj_t *target, angle_t *delta)
{
    angle_t     diff, angle1, angle2;

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
 * The missile tracer field must be mobj_t *target.
 *
 * @return              @c true, if target was tracked.
 */
boolean P_SeekerMissile(mobj_t *actor, angle_t thresh, angle_t turnMax)
{
    int             dir;
    uint            an;
    float           dist;
    angle_t         delta;
    mobj_t         *target;

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

    if(dir) // Turn clockwise.
        actor->angle += delta;
    else // Turn counter clockwise.
        actor->angle -= delta;

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = actor->info->speed * FIX2FLT(finecosine[an]);
    actor->mom[MY] = actor->info->speed * FIX2FLT(finesine[an]);

    if(actor->pos[VZ]  + actor->height  < target->pos[VZ] ||
       target->pos[VZ] + target->height < actor->pos[VZ])
    {   // Need to seek vertically
        dist = P_ApproxDistance(target->pos[VX] - actor->pos[VX],
                                target->pos[VY] - actor->pos[VY]);
        dist /= actor->info->speed;
        if(dist < 1)
            dist = 1;

        actor->mom[MZ] =
            (target->pos[VZ] + (target->height /2) -
             (actor->pos[VZ] + (actor->height /2))) / dist;
    }

    return true;
}

float P_MobjGetFriction(mobj_t *mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) &&
       !(mo->flags2 & MF2_ONMOBJ))
    {
        return FRICTION_FLY;
    }
    else
    {
        const terraintype_t* tt = P_MobjGetFloorTerrainType(mo);

        if(tt->flags & TTF_FRICTION_LOW)
            return FRICTION_LOW;
    }

    return FRICTION_NORMAL;
}

void P_MobjMoveXY(mobj_t* mo)
{
    static const int    windTab[3] = { 2048 * 5, 2048 * 10, 2048 * 25 };

    float               posTry[2];
    player_t*           player;
    float               move[2];
    int                 special;
    angle_t             angle;

    // $democam: cameramen have their own movement code
    if(P_CameraXYMovement(mo))
        return;

    if(mo->mom[MX] == 0 && mo->mom[MY] == 0)
    {
        if(mo->flags & MF_SKULLFLY)
        {   // A flying mobj slammed into something
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
            P_MobjChangeState(mo, P_GetState(mo->type, SN_SEE));
        }
        return;
    }

    special = P_ToXSectorOfSubsector(mo->subsector)->special;
    if(mo->flags2 & MF2_WINDTHRUST)
    {
        switch(special)
        {
        case 40:
        case 41:
        case 42: // Wind_East
            P_ThrustMobj(mo, 0, FIX2FLT(windTab[special - 40]));
            break;

        case 43:
        case 44:
        case 45: // Wind_North
            P_ThrustMobj(mo, ANG90, FIX2FLT(windTab[special - 43]));
            break;

        case 46:
        case 47:
        case 48: // Wind_South
            P_ThrustMobj(mo, ANG270, FIX2FLT(windTab[special - 46]));
            break;

        case 49:
        case 50:
        case 51: // Wind_West
            P_ThrustMobj(mo, ANG180, FIX2FLT(windTab[special - 49]));
            break;
        }
    }
    player = mo->player;

    if(mo->mom[MX] > MAXMOVE)
        mo->mom[MX] = MAXMOVE;
    else if(mo->mom[MX] < -MAXMOVE)
        mo->mom[MX] = -MAXMOVE;

    if(mo->mom[MY] > MAXMOVE)
        mo->mom[MY] = MAXMOVE;
    else if(mo->mom[MY] < -MAXMOVE)
        mo->mom[MY] = -MAXMOVE;

    move[VX] = mo->mom[MX];
    move[VY] = mo->mom[MY];
    do
    {
        if(move[VX] > MAXMOVE / 2 || move[VY] > MAXMOVE / 2)
        {
            posTry[VX] = mo->pos[VX] + move[VX] / 2;
            posTry[VY] = mo->pos[VY] + move[VY] / 2;
            move[VX] /= 2;
            move[VY] /= 2;
        }
        else
        {
            posTry[VX] = mo->pos[VX] + move[VX];
            posTry[VY] = mo->pos[VY] + move[VY];
            move[VX] = move[VY] = 0;
        }

        if(!P_TryMove(mo, posTry[VX], posTry[VY]))
        {   // Blocked move.
            if(mo->flags2 & MF2_SLIDE)
            {   // Try to slide along it.
                if(blockingMobj == NULL)
                {   // Slide against wall.
                    P_SlideMove(mo);
                }
                else
                {   // Slide against mobj.
                    if(P_TryMove(mo, mo->pos[VX], posTry[VY]))
                    {
                        mo->mom[MX] = 0;
                    }
                    else if(P_TryMove(mo, posTry[VX], mo->pos[VY]))
                    {
                        mo->mom[MY] = 0;
                    }
                    else
                    {
                        mo->mom[MX] = mo->mom[MY] = 0;
                    }
                }
            }
            else if(mo->flags & MF_MISSILE)
            {
                sector_t*           backSec;

                if(mo->flags2 & MF2_FLOORBOUNCE)
                {
                    if(blockingMobj)
                    {
                        if((blockingMobj->flags2 & MF2_REFLECTIVE) ||
                           ((!blockingMobj->player) &&
                            (!(blockingMobj->flags & MF_COUNTKILL))))
                        {
                            float       speed;

                            angle =
                                R_PointToAngle2(blockingMobj->pos[VX],
                                                blockingMobj->pos[VY],
                                                mo->pos[VX], mo->pos[VY]) +
                                ANGLE_1 * ((P_Random() % 16) - 8);

                            speed = P_ApproxDistance(mo->mom[MX], mo->mom[MY]);
                            speed *= 0.75f;

                            mo->angle = angle;
                            angle >>= ANGLETOFINESHIFT;
                            mo->mom[MX] = speed * FIX2FLT(finecosine[angle]);
                            mo->mom[MY] = speed * FIX2FLT(finesine[angle]);
                            if(mo->info->seeSound)
                                S_StartSound(mo->info->seeSound, mo);

                            return;
                        }
                        else
                        {   // Struck a player/creature
                            P_ExplodeMissile(mo);
                        }
                    }
                    else
                    {   // Struck a wall
                        P_BounceWall(mo);
                        switch(mo->type)
                        {
                        case MT_SORCBALL1:
                        case MT_SORCBALL2:
                        case MT_SORCBALL3:
                        case MT_SORCFX1:
                            break;

                        default:
                            if(mo->info->seeSound)
                                S_StartSound(mo->info->seeSound, mo);
                            break;
                        }

                        return;
                    }
                }

                if(blockingMobj && (blockingMobj->flags2 & MF2_REFLECTIVE))
                {
                    angle =
                        R_PointToAngle2(blockingMobj->pos[VX],
                                        blockingMobj->pos[VY],
                                        mo->pos[VX],
                                        mo->pos[VY]);

                    // Change angle for delflection/reflection
                    switch(blockingMobj->type)
                    {
                    case MT_CENTAUR:
                    case MT_CENTAURLEADER:
                        if(abs(angle - blockingMobj->angle) >> 24 > 45)
                            goto explode;
                        if(mo->type == MT_HOLY_FX)
                            goto explode;
                        // Drop through to sorcerer full reflection
                    case MT_SORCBOSS:
                        // Deflection
                        if(P_Random() < 128)
                            angle += ANGLE_45;
                        else
                            angle -= ANGLE_45;
                        break;

                    default:
                        // Reflection
                        angle += ANGLE_1 * ((P_Random() % 16) - 8);
                        break;
                    }

                    // Reflect the missile along angle
                    mo->angle = angle;
                    angle >>= ANGLETOFINESHIFT;

                    mo->mom[MX] =
                        (mo->info->speed / 2) * FIX2FLT(finecosine[angle]);
                    mo->mom[MY] =
                        (mo->info->speed / 2) * FIX2FLT(finesine[angle]);

                    if(mo->flags2 & MF2_SEEKERMISSILE)
                    {
                        mo->tracer = mo->target;
                    }
                    mo->target = blockingMobj;

                    return;
                }

explode:
                // Explode a missile

                //// kludge: Prevent missiles exploding against the sky.
                if(ceilingLine &&
                   (backSec = DMU_GetPtrp(ceilingLine, DMU_BACK_SECTOR)))
                {
                    if((DMU_GetIntp(DMU_GetPtrp(backSec, DMU_CEILING_MATERIAL),
                                    DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->pos[VZ] > DMU_GetFloatp(backSec, DMU_CEILING_HEIGHT))
                    {
                        if(mo->type == MT_BLOODYSKULL)
                        {
                            mo->mom[MX] = mo->mom[MY] = 0;
                            mo->mom[MZ] = -1;
                        }
                        else if(mo->type == MT_HOLY_FX)
                        {
                            P_ExplodeMissile(mo);
                        }
                        else
                        {
                            P_MobjRemove(mo, false);
                        }

                        return;
                    }
                }

                if(floorLine &&
                   (backSec = DMU_GetPtrp(floorLine, DMU_BACK_SECTOR)))
                {
                    if((DMU_GetIntp(DMU_GetPtrp(backSec, DMU_FLOOR_MATERIAL),
                                    DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->pos[VZ] < DMU_GetFloatp(backSec, DMU_FLOOR_HEIGHT))
                    {
                        if(mo->type == MT_BLOODYSKULL)
                        {
                            mo->mom[MX] = mo->mom[MY] = 0;
                            mo->mom[MZ] = -1;
                        }
                        else if(mo->type == MT_HOLY_FX)
                        {
                            P_ExplodeMissile(mo);
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
    } while(!INRANGE_OF(move[MX], 0, NOMOMENTUM_THRESHOLD) ||
            !INRANGE_OF(move[MY], 0, NOMOMENTUM_THRESHOLD));

    // Friction
    if(player && (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
    {   // Debug option for no sliding at all
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }
    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
        return; // No friction for missiles

    if(mo->pos[VZ] > mo->floorZ && !(mo->flags2 & MF2_FLY) &&
       !(mo->flags2 & MF2_ONMOBJ))
    {    // No friction when falling
        if(mo->type != MT_BLASTEFFECT)
            return;
    }

    if(mo->flags & MF_CORPSE)
    {   // Don't stop sliding if halfway off a step with some momentum
        if(mo->mom[MX] > 1.0f / 4 || mo->mom[MX] < -1.0f / 4 ||
           mo->mom[MY] > 1.0f / 4 || mo->mom[MY] < -1.0f / 4)
        {
            if(mo->floorZ != DMU_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
            {
                return;
            }
        }
    }

    if(mo->mom[MX] > -STOPSPEED && mo->mom[MX] < STOPSPEED &&
       mo->mom[MY] > -STOPSPEED && mo->mom[MY] < STOPSPEED &&
       (!player || (player->plr->cmd.forwardMove == 0 &&
                    player->plr->cmd.sideMove == 0)))
    {   // If in a walking frame, stop moving
        if(player)
        {
            if((unsigned)
               ((player->plr->mo->state - STATES) - PCLASS_INFO(player->class)->runState) <
               4)
            {
                P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->normalState);
            }
        }
        mo->mom[MX] = 0;
        mo->mom[MY] = 0;
    }
    else
    {
        float       friction = P_MobjGetFriction(mo);

        mo->mom[MX] *= friction;
        mo->mom[MY] *= friction;
    }
}

/**
 * \todo Move this to p_inter ***
 */
void P_MonsterFallingDamage(mobj_t *mo)
{
    int         damage;
    float       mom;

    mom = (int) fabs(mo->mom[MZ]);
    if(mom > 35)
    {    // automatic death
        damage = 10000;
    }
    else
    {
        damage = (int) ((mom - 23) * 6);
    }
    damage = 10000; // always kill 'em.

    P_DamageMobj(mo, NULL, NULL, damage, false);
}

void P_MobjMoveZ(mobj_t *mo)
{
    float           gravity, dist, delta;

    // $democam: cameramen get special z movement
    if(P_CameraZMovement(mo))
        return;

    gravity = P_GetGravity();

    // Check for smooth step up.
    if(mo->player && mo->pos[VZ] < mo->floorZ)
    {
        mo->player->plr->viewHeight -= mo->floorZ - mo->pos[VZ];
        mo->player->plr->viewHeightDelta =
            (cfg.plrViewHeight - mo->player->plr->viewHeight) / 8;
    }

    // Adjust height.
    mo->pos[VZ] += mo->mom[MZ];
    if((mo->flags & MF_FLOAT) && mo->target && !P_MobjIsCamera(mo->target))
    {
        // Float down towards target if too close.
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist =
                P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                 mo->pos[VY] - mo->target->pos[VY]);
            delta = (mo->target->pos[VZ] + (mo->height /2)) - mo->pos[VZ];
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

    if(mo->player && (mo->flags2 & MF2_FLY) &&
       !(mo->pos[VZ] <= mo->floorZ) && (mapTime & 2))
    {
        mo->pos[VZ] +=
            FIX2FLT(finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK]);
    }

    // Clip movement.
    if(mo->pos[VZ] <= mo->floorZ)
    {   // Hit the floor
        statenum_t              state;

        if(mo->flags & MF_MISSILE)
        {
            mo->pos[VZ] = mo->floorZ;
            if(mo->flags2 & MF2_FLOORBOUNCE)
            {
                P_FloorBounceMissile(mo);
                return;
            }
            else if(mo->type == MT_HOLY_FX)
            {   // The spirit struck the ground.
                mo->mom[MZ] = 0;
                P_HitFloor(mo);
                return;
            }
            else if(mo->type == MT_MNTRFX2 || mo->type == MT_LIGHTNING_FLOOR)
            {   // Minotaur floor fire can go up steps.
                return;
            }
            else
            {
                P_HitFloor(mo);
                P_ExplodeMissile(mo);
                return;
            }
        }

        if(mo->flags & MF_COUNTKILL)
        {
            // Blasted mobj falling.
            if(mo->mom[MZ] < -23)
            {
                P_MonsterFallingDamage(mo);
            }
        }

        if(mo->pos[VZ] - mo->mom[MZ] > mo->floorZ)
        {   // Spawn splashes, etc.
            P_HitFloor(mo);
        }

        mo->pos[VZ] = mo->floorZ;
        if(mo->mom[MZ] < 0)
        {
            if((mo->flags2 & MF2_ICEDAMAGE) && mo->mom[MZ] < -gravity * 8)
            {
                mo->tics = 1;
                mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
                return;
            }

            if(mo->player)
            {
                mo->player->jumpTics = 7; // delay any jumping for a short time
                if(mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
                {   // Squat down.
                    mo->player->plr->viewHeightDelta = mo->mom[MZ] / 8;
                    if(mo->mom[MZ] < -23)
                    {
                        P_FallingDamage(mo->player);
                        P_NoiseAlert(mo, mo);
                    }
                    else if(mo->mom[MZ] < -gravity * 12 && !mo->player->morphTics)
                    {
                        S_StartSound(SFX_PLAYER_LAND, mo);

                        // Fix DOOM bug - dead players grunting when hitting the ground
                        // (e.g., after an archvile attack)
                        if(mo->player->health > 0)
                            switch(mo->player->class)
                            {
                            case PCLASS_FIGHTER:
                                S_StartSound(SFX_PLAYER_FIGHTER_GRUNT, mo);
                                break;

                            case PCLASS_CLERIC:
                                S_StartSound(SFX_PLAYER_CLERIC_GRUNT, mo);
                                break;

                            case PCLASS_MAGE:
                                S_StartSound(SFX_PLAYER_MAGE_GRUNT, mo);
                                break;

                            default:
                                break;
                            }
                    }
                    else if(!mo->player->morphTics)
                    {
                        const terraintype_t* tt =
                            P_MobjGetFloorTerrainType(mo);

                        if(!(tt->flags & TTF_NONSOLID))
                            S_StartSound(SFX_PLAYER_LAND, mo);
                    }

                    if(!cfg.useMLook && cfg.lookSpring)
                        mo->player->centering = true;
                }
            }
            else if(mo->type >= MT_POTTERY1 && mo->type <= MT_POTTERY3)
            {
                P_DamageMobj(mo, NULL, NULL, 25, false);
            }
            else if(mo->flags & MF_COUNTKILL)
            {
                if(mo->mom[MZ] < -23)
                {
                    // Doesn't get here
                }
            }
            mo->mom[MZ] = 0;
        }

        if(mo->flags & MF_SKULLFLY)
        {   // The skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if((state = P_GetState(mo->type, SN_CRASH)) != S_NULL &&
           (mo->flags & MF_CORPSE) && !(mo->flags2 & MF2_ICEDAMAGE))
        {
            P_MobjChangeState(mo, state);
            return;
        }
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
    {   // hit the ceiling
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->pos[VZ] = mo->ceilingZ - mo->height;
        if(mo->flags2 & MF2_FLOORBOUNCE)
        {
            // Maybe reverse momentum here for ceiling bounce
            // Currently won't happen
            if(mo->info->seeSound)
            {
                S_StartSound(mo->info->seeSound, mo);
            }
            return;
        }

        if(mo->flags & MF_SKULLFLY)
        {   // the skull slammed into something
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(mo->flags & MF_MISSILE)
        {
            if(mo->type == MT_LIGHTNING_CEILING)
                return;

            if(DMU_GetIntp(DMU_GetPtrp(mo->subsector, DMU_CEILING_MATERIAL),
                           DMU_FLAGS) & MATF_SKYMASK)
            {
                if(mo->type == MT_BLOODYSKULL)
                {
                    mo->mom[MX] = mo->mom[MY] = 0;
                    mo->mom[MZ] = -1;
                }
                else if(mo->type == MT_HOLY_FX)
                {
                    P_ExplodeMissile(mo);
                }
                else
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

static void PlayerLandedOnThing(mobj_t *mo, mobj_t *onmobj)
{
    mo->player->plr->viewHeightDelta = mo->mom[MZ] / 8;
    if(mo->mom[MZ] < -23)
    {
        P_FallingDamage(mo->player);
        P_NoiseAlert(mo, mo);
    }
    else if(mo->mom[MZ] < -P_GetGravity() * 12 && !mo->player->morphTics)
    {
        S_StartSound(SFX_PLAYER_LAND, mo);
        switch(mo->player->class)
        {
        case PCLASS_FIGHTER:
            S_StartSound(SFX_PLAYER_FIGHTER_GRUNT, mo);
            break;

        case PCLASS_CLERIC:
            S_StartSound(SFX_PLAYER_CLERIC_GRUNT, mo);
            break;

        case PCLASS_MAGE:
            S_StartSound(SFX_PLAYER_MAGE_GRUNT, mo);
            break;

        default:
            break;
        }
    }
    else if(!mo->player->morphTics)
    {
        S_StartSound(SFX_PLAYER_LAND, mo);
    }

    // Lookspring is stupid when mouselook is on (and not in demo).
    if(!cfg.useMLook && cfg.lookSpring) // || demorecording || demoplayback)
        mo->player->centering = true;
}

void P_MobjThinker(mobj_t* mobj)
{
    mobj_t*             onmo = NULL;

    if(mobj->ddFlags & DDMF_REMOTE) // Remote mobjs are handled separately.
        return;

    if(mobj->type == MT_MWAND_MISSILE || mobj->type == MT_CFLAME_MISSILE)
    {
        int                 i;
        float               z, frac[3];
        boolean             changexy;

        // Handle movement.
        if(mobj->mom[MX] != 0 || mobj->mom[MY] != 0 || mobj->mom[MZ] != 0 ||
           mobj->pos[VZ] != mobj->floorZ)
        {
            frac[VX] = mobj->mom[MX] / 8;
            frac[VY] = mobj->mom[MY] / 8;
            frac[VZ] = mobj->mom[MZ] / 8;
            changexy = (frac[VX] != 0 || frac[VY] != 0);

            for(i = 0; i < 8; ++i)
            {
                if(changexy)
                {
                    if(!P_TryMove(mobj, mobj->pos[VX] + frac[VX],
                                        mobj->pos[VY] + frac[VY]))
                    {   // Blocked move.
                        P_ExplodeMissile(mobj);
                        return;
                    }
                }

                mobj->pos[VZ] += frac[VZ];
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

                if(changexy)
                {
                    if(mobj->type == MT_MWAND_MISSILE && (P_Random() < 128))
                    {
                        z = mobj->pos[VZ] - 8;
                        if(z < mobj->floorZ)
                        {
                            z = mobj->floorZ;
                        }
                        P_SpawnMobj3f(MT_MWANDSMOKE, mobj->pos[VX],
                                      mobj->pos[VY], z, P_Random() << 24, 0);
                    }
                    else if(!--mobj->special1)
                    {
                        mobj->special1 = 4;
                        z = mobj->pos[VZ] - 12;
                        if(z < mobj->floorZ)
                        {
                            z = mobj->floorZ;
                        }

                        P_SpawnMobj3f(MT_CFLAMEFLOOR, mobj->pos[VX],
                                      mobj->pos[VY], z, mobj->angle, 0);
                    }
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
                    return; // Mobj was removed.
            }
        }

        return;
    }

    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mobj);

    // Handle X and Y momentums
    blockingMobj = NULL;
    if(mobj->mom[MX] != 0 || mobj->mom[MY] != 0 ||
       (mobj->flags & MF_SKULLFLY))
    {
        P_MobjMoveXY(mobj);
        if(mobj->thinker.function == NOPFUNC)
        {   // Mobj was removed.
            return;
        }
    }
    else if(mobj->flags2 & MF2_BLASTED)
    {   // Reset to not blasted when momentums are gone.
        ResetBlasted(mobj);
    }

    if(mobj->flags2 & MF2_FLOATBOB)
    {
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
    else if(mobj->pos[VZ] != mobj->floorZ || mobj->mom[MZ] != 0 || blockingMobj)
    {   // Handle Z momentum and gravity
        if(mobj->flags2 & MF2_PASSMOBJ)
        {
            onmo = P_CheckOnMobj(mobj);
            if(!onmo)
            {
                P_MobjMoveZ(mobj);
                if(mobj->player && mobj->flags & MF2_ONMOBJ)
                {
                    mobj->flags2 &= ~MF2_ONMOBJ;
                }
            }
            else
            {
                if(mobj->player)
                {
                    if(mobj->mom[MZ] < -P_GetGravity() * 8 && !(mobj->flags2 & MF2_FLY))
                    {
                        PlayerLandedOnThing(mobj, onmo);
                    }

                    if(onmo->pos[VZ] + onmo->height - mobj->pos[VZ] <= 24)
                    {
                        mobj->player->plr->viewHeight -=
                            onmo->pos[VZ] + onmo->height - mobj->pos[VZ];
                        mobj->player->plr->viewHeightDelta =
                            (cfg.plrViewHeight - mobj->player->plr->viewHeight) / 8;
                        mobj->pos[VZ] = onmo->pos[VZ] + onmo->height;
                        mobj->flags2 |= MF2_ONMOBJ;
                        mobj->mom[MZ] = 0;
                    }
                    else
                    {   // hit the bottom of the blocking mobj
                        mobj->mom[MZ] = 0;
                    }
                }

                /* Landing on another player, and mimicking his movements
                if(mobj->player && onmo->player)
                {
                    mobj->mom[MX] = onmo->mom[MX];
                    mobj->mom[MY] = onmo->mom[MY];
                    if(onmo->z < onmo->floorZ)
                    {
                        mobj->z += onmo->floorZ-onmo->z;
                        if(onmo->player)
                        {
                            onmo->player->plr->viewHeight -= onmo->floorZ-onmo->z;
                            onmo->player->plr->viewHeightDelta =
                                 (VIEWHEIGHT- onmo->player->plr->viewHeight) / 8;
                        }
                        onmo->z = onmo->floorZ;
                    }
                }
                */
            }
        }
        else
        {
            P_MobjMoveZ(mobj);
        }

        if(mobj->thinker.function == NOPFUNC)
        {   // mobj was removed
            return;
        }
    }

    // Cycle through states, calling action functions at transitions.
    if(mobj->tics != -1)
    {
        mobj->tics--;
        P_MobjAngleSRVOTicker(mobj);
        // You can cycle through multiple states in a tic.
        while(!mobj->tics)
        {
            P_MobjClearSRVO(mobj);
            if(!P_MobjChangeState(mobj, mobj->state->nextState))
            {   // Mobj was removed.
                return;
            }
        }
    }

    // Ice corpses aren't going anywhere.
    if(mobj->flags & MF_ICECORPSE)
        P_MobjSetSRVO(mobj, 0, 0);
}

mobj_t* P_SpawnMobj3f(mobjtype_t type, float x, float y, float z,
                      angle_t angle, int spawnFlags)
{
    mobj_t*             mo;
    mobjinfo_t*         info;
    float               space;
    int                 ddflags = 0;

    if(type == MT_ZLYNCHED_NOHEART)
    {
        type = MT_BLOODPOOL;
        angle = 0;
        spawnFlags |= MSF_Z_FLOOR;
    }

    if(type < MT_FIRST || type >= Get(DD_NUMMOBJTYPES))
    {
#ifdef _DEBUG
        Con_Error("P_SpawnMobj: Illegal mo type %i.\n", type);
#endif
        return NULL;
    }

    info = &MOBJINFO[type];

    // Clients only spawn local objects.
    if(!(info->flags & MF_LOCAL) && IS_CLIENT)
        return NULL;

    // Not for deathmatch?
    if(deathmatch && (info->flags & MF_NOTDMATCH))
        return NULL;

    // Don't spawn any monsters if -noMonstersParm.
    if(noMonstersParm && (info->flags & MF_COUNTKILL))
        return NULL;

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
    // This doesn't appear to actually be used see P_DamageMobj in P_inter.c
    mo->damage = info->damage;
    mo->health = info->spawnHealth *
        (IS_NETGAME ? cfg.netMobHealthModifier : 1);
    mo->moveDir = DI_NODIR;

    if(gameSkill != SM_NIGHTMARE)
    {
        mo->reactionTime = info->reactionTime;
    }
    mo->lastLook = P_Random() % MAXPLAYERS;

    // Must link before setting state.
    P_MobjSetState(mo, P_GetState(mo->type, SN_SPAWN));

    // Set subsector and/or block links.
    P_MobjSetPosition(mo);

    mo->floorZ = DMU_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT);
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

    if(spawnFlags & MSF_AMBUSH)
    {
        mo->flags |= MF_AMBUSH;
    }

    mo->floorClip = 0;

    if((mo->flags2 & MF2_FLOORCLIP) &&
        mo->pos[VZ] == DMU_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
    {
        const terraintype_t* tt = P_MobjGetFloorTerrainType(mo);

        if(tt->flags & TTF_FLOORCLIP)
            mo->floorClip = 10;
    }

    if(spawnFlags & MTF_DORMANT)
    {
        mo->flags2 |= MF2_DORMANT;
        if(mo->type == MT_ICEGUY)
            P_MobjChangeState(mo, S_ICEGUY_DORMANT);
        mo->tics = -1;
    }

    return mo;
}

mobj_t* P_SpawnMobj3fv(mobjtype_t type, const float pos[3], angle_t angle,
                       int spawnFlags)
{
    return P_SpawnMobj3f(type, pos[VX], pos[VY], pos[VZ], angle,
                         spawnFlags);
}

static boolean addToTIDList(thinker_t* th, void* context)
{
    size_t*             count = (size_t*) context;
    mobj_t*             mo = (mobj_t *) th;

    if(mo->tid != 0)
    {
        // Add to list.
        if(*count == MAX_TID_COUNT)
        {
            Con_Error("P_CreateTIDList: MAX_TID_COUNT (%d) exceeded.",
                      MAX_TID_COUNT);
        }

        TIDList[*count] = mo->tid;
        TIDMobj[(*count)++] = mo;
    }

    return true; // Continue iteration.
}

void P_CreateTIDList(void)
{
    size_t              count = 0;

    DD_IterateThinkers(P_MobjThinker, addToTIDList, &count);

    // Add termination marker
    TIDList[count] = 0;
}

void P_MobjInsertIntoTIDList(mobj_t* mobj, int tid)
{
    int                 i, index;

    index = -1;
    for(i = 0; TIDList[i] != 0; ++i)
    {
        if(TIDList[i] == -1)
        {   // Found empty slot
            index = i;
            break;
        }
    }

    if(index == -1)
    {   // Append required
        if(i == MAX_TID_COUNT)
        {
            Con_Error("P_MobjInsertIntoTIDList: MAX_TID_COUNT (%d)"
                      "exceeded.", MAX_TID_COUNT);
        }
        index = i;
        TIDList[index + 1] = 0;
    }

    mobj->tid = tid;
    TIDList[index] = tid;
    TIDMobj[index] = mobj;
}

void P_MobjRemoveFromTIDList(mobj_t* mobj)
{
    int                 i;

    if(!mobj->tid)
        return;

    for(i = 0; TIDList[i] != 0; ++i)
    {
        if(TIDMobj[i] == mobj)
        {
            TIDList[i] = -1;
            TIDMobj[i] = NULL;
            mobj->tid = 0;
            return;
        }
    }

    mobj->tid = 0;
}

mobj_t* P_FindMobjFromTID(int tid, int* searchPosition)
{
    int                 i;

    for(i = *searchPosition + 1; TIDList[i] != 0; ++i)
    {
        if(TIDList[i] == tid)
        {
            *searchPosition = i;
            return TIDMobj[i];
        }
    }

    *searchPosition = -1;
    return NULL;
}

void P_SpawnPuff(float x, float y, float z, angle_t angle)
{
    mobj_t*             puff;

    z += FIX2FLT((P_Random() - P_Random()) << 10);
    if((puff = P_SpawnMobj3f(PuffType, x, y, z, angle, 0)))
    {
        if(lineTarget && puff->info->seeSound)
        {   // Hit thing sound.
            S_StartSound(puff->info->seeSound, puff);
        }
        else if(puff->info->attackSound)
        {
            S_StartSound(puff->info->attackSound, puff);
        }

        switch(PuffType)
        {
        case MT_PUNCHPUFF:
            puff->mom[MZ] = 1;
            break;

        case MT_HAMMERPUFF:
            puff->mom[MZ] = .8f;
            break;

        default:
            break;
        }
    }

    puffSpawned = puff;
}

void P_SpawnBloodSplatter(float x, float y, float z, mobj_t* originator)
{
    mobj_t*             mo;

    if((mo = P_SpawnMobj3f(MT_BLOODSPLATTER, x, y, z, P_Random() << 24, 0)))
    {
        mo->target = originator;
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MZ] = 3;
    }
}

void P_SpawnBloodSplatter2(float x, float y, float z, mobj_t* originator)
{
    mobj_t*             mo;

    if((mo = P_SpawnMobj3f(MT_AXEBLOOD,
                           x + FIX2FLT((P_Random() - 128) << 11),
                           y + FIX2FLT((P_Random() - 128) << 11),
                           z, P_Random() << 24, 0)))
    {
        mo->target = originator;
    }
}

boolean P_HitFloor(mobj_t *thing)
{
    mobj_t*             mo;
    int                 smallsplash = false;
    const terraintype_t* tt;

    if(thing->floorZ != DMU_GetFloatp(thing->subsector, DMU_FLOOR_HEIGHT))
    {   // Don't splash if landing on the edge above water/lava/etc....
        return false;
    }

    // Things that don't splash go here
    switch(thing->type)
    {
    case MT_LEAF1:
    case MT_LEAF2:
    case MT_SPLASH:
    case MT_SLUDGECHUNK:
        return false;

    default:
        if(P_MobjIsCamera(thing))
            return false;
        break;
    }

    // Small splash for small masses.
    if(thing->info->mass < 10)
        smallsplash = true;

    tt = P_MobjGetFloorTerrainType(thing);
    if(tt->flags & TTF_SPAWN_SPLASHES)
    {
        if(smallsplash)
        {
            if((mo = P_SpawnMobj3f(MT_SPLASHBASE, thing->pos[VX],
                                   thing->pos[VY], 0, thing->angle + ANG180,
                                   MSF_Z_FLOOR)))
            {

                mo->floorClip += SMALLSPLASHCLIP;
                S_StartSound(SFX_AMBIENT10, mo); // small drip
            }
        }
        else
        {
            if((mo = P_SpawnMobj3f(MT_SPLASH, thing->pos[VX], thing->pos[VY], 0,
                                  P_Random() << 24, MSF_Z_FLOOR)))
            {
                mo->target = thing;
                mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
                mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
                mo->mom[MZ] = 2 + FIX2FLT(P_Random() << 8);

                mo = P_SpawnMobj3f(MT_SPLASHBASE, thing->pos[VX], thing->pos[VY],
                                   0, thing->angle + ANG180, MSF_Z_FLOOR);
                S_StartSound(SFX_WATER_SPLASH, mo);
            }

            if(thing->player)
                P_NoiseAlert(thing, thing);
        }

        return true;
    }
    else if(tt->flags & TTF_SPAWN_SMOKE)
    {
        if(smallsplash)
        {
            if((mo = P_SpawnMobj3f(MT_LAVASPLASH, thing->pos[VX], thing->pos[VY],
                                   0, P_Random() << 24, MSF_Z_FLOOR)))
                mo->floorClip += SMALLSPLASHCLIP;
        }
        else
        {
            if((mo = P_SpawnMobj3f(MT_LAVASMOKE, thing->pos[VX], thing->pos[VY],
                                   0, P_Random() << 24, MSF_Z_FLOOR)))
            {
                mo->mom[MZ] = 1 + FIX2FLT(P_Random() << 7);
                mo = P_SpawnMobj3f(MT_LAVASPLASH, thing->pos[VX], thing->pos[VY],
                                   0, P_Random() << 24, MSF_Z_FLOOR);
            }

            if(thing->player)
                P_NoiseAlert(thing, thing);
        }

        S_StartSound(SFX_LAVA_SIZZLE, mo);
        if(thing->player && mapTime & 31)
        {
            P_DamageMobj(thing, &lavaInflictor, NULL, 5, false);
        }
        return true;
    }
    else if(tt->flags & TTF_SPAWN_SLUDGE)
    {
        mo = NULL;

        if(smallsplash)
        {
            if((mo = P_SpawnMobj3f(MT_SLUDGESPLASH, thing->pos[VX],
                                   thing->pos[VY], 0, P_Random() << 24,
                                   MSF_Z_FLOOR)))
            {
                mo->floorClip += SMALLSPLASHCLIP;
            }
        }
        else
        {
            if((mo = P_SpawnMobj3f(MT_SLUDGECHUNK, thing->pos[VX],
                                   thing->pos[VY], 0, P_Random() << 24,
                                   MSF_Z_FLOOR)))
            {
                mo->target = thing;
                mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
                mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
                mo->mom[MZ] = 1 + FIX2FLT(P_Random() << 8);
            }

            mo = P_SpawnMobj3f(MT_SLUDGESPLASH, thing->pos[VX],
                               thing->pos[VY], 0, P_Random() << 24,
                               MSF_Z_FLOOR);

            if(thing->player)
                P_NoiseAlert(thing, thing);
        }

        S_StartSound(SFX_SLUDGE_GLOOP, mo);
        return true;
    }

    return false;
}

void ResetBlasted(mobj_t *mo)
{
    mo->flags2 &= ~MF2_BLASTED;
    if(!(mo->flags & MF_ICECORPSE))
    {
        mo->flags2 &= ~MF2_SLIDE;
    }
}

void P_BlastMobj(mobj_t *source, mobj_t *victim, float strength)
{
    uint            an;
    angle_t         angle;
    mobj_t         *mo;
    float           pos[3];

    angle = R_PointToAngle2(source->pos[VX], source->pos[VY],
                            victim->pos[VX], victim->pos[VY]);
    an = angle >> ANGLETOFINESHIFT;
    if(strength < BLAST_FULLSTRENGTH)
    {
        victim->mom[MX] = strength * FIX2FLT(finecosine[an]);
        victim->mom[MY] = strength * FIX2FLT(finesine[an]);
        if(victim->player)
        {
            // Players handled automatically.
        }
        else
        {
            victim->flags2 |= MF2_SLIDE;
            victim->flags2 |= MF2_BLASTED;
        }
    }
    else // Full strength.
    {
        if(victim->flags & MF_MISSILE)
        {
            switch(victim->type)
            {
            case MT_SORCBALL1: // Don't blast sorcerer balls.
            case MT_SORCBALL2:
            case MT_SORCBALL3:
                return;
                break;

            case MT_MSTAFF_FX2: // Reflect to originator.
                victim->tracer = victim->target;
                victim->target = source;
                break;

            default:
                break;
            }
        }

        if(victim->type == MT_HOLY_FX)
        {
            if(victim->tracer == source)
            {
                victim->tracer = victim->target;
                victim->target = source;
            }
        }
        victim->mom[MX] = FIX2FLT(BLAST_SPEED) * FIX2FLT(finecosine[an]);
        victim->mom[MY] = FIX2FLT(BLAST_SPEED) * FIX2FLT(finesine[an]);

        // Spawn blast puff.
        angle = R_PointToAngle2(victim->pos[VX], victim->pos[VY],
                              source->pos[VX], source->pos[VY]);
        an = angle >> ANGLETOFINESHIFT;

        pos[VX] = victim->pos[VX];
        pos[VY] = victim->pos[VY];
        pos[VZ] = victim->pos[VZ];

        pos[VX] += victim->radius + 1 * FIX2FLT(finecosine[an]);
        pos[VY] += victim->radius + 1 * FIX2FLT(finesine[an]);
        pos[VZ] -= victim->floorClip + victim->height / 2;

        if((mo = P_SpawnMobj3fv(MT_BLASTEFFECT, pos, angle, 0)))
        {
            mo->mom[MX] = victim->mom[MX];
            mo->mom[MY] = victim->mom[MY];
        }

        if((victim->flags & MF_MISSILE))
        {
            victim->mom[MZ] = 8;
            if(mo)
                mo->mom[MZ] = victim->mom[MZ];
        }
        else
        {
            victim->mom[MZ] = 1000 / victim->info->mass;
        }

        if(victim->player)
        {
            // Players handled automatically
        }
        else
        {
            victim->flags2 |= MF2_SLIDE;
            victim->flags2 |= MF2_BLASTED;
        }
    }
}

typedef struct {
    float               maxDistance;
    mobj_t*             source;
} radiusblastparams_t;

static boolean radiusBlast(thinker_t* th, void* context)
{
    radiusblastparams_t* params = (radiusblastparams_t*) context;
    mobj_t*             mo = (mobj_t *) th;
    float               dist;

    if(mo == params->source || (mo->flags2 & MF2_BOSS))
        return true; // Continue iteration.

    if(mo->type == MT_POISONCLOUD || // poison cloud.
       mo->type == MT_HOLY_FX || // holy fx.
       (mo->flags & MF_ICECORPSE)) // frozen corpse.
    {
        // Let these special cases go.
    }
    else if((mo->flags & MF_COUNTKILL) && mo->health <= 0)
    {
        return true; // Continue iteration.
    }
    else if(!(mo->flags & MF_COUNTKILL) && !mo->player &&
            !(mo->flags & MF_MISSILE))
    {   // Must be monster, player, or missile.
        return true; // Continue iteration.
    }

    // Is this mobj dormant?
    if(mo->flags2 & MF2_DORMANT)
        return true; // Continue iteration.

    // Is this an underground Wraith?
    if(mo->type == MT_WRAITHB && (mo->flags2 & MF2_DONTDRAW))
        return true; // Continue iteration.

    if(mo->type == MT_SPLASHBASE || mo->type == MT_SPLASH)
        return true; // Continue iteration.

    if(mo->type == MT_SERPENT || mo->type == MT_SERPENTLEADER)
        return true; // Continue iteration.

    // Within range?
    dist = P_ApproxDistance(params->source->pos[VX] - mo->pos[VX],
                            params->source->pos[VY] - mo->pos[VY]);
    if(dist <= params->maxDistance)
    {
        P_BlastMobj(params->source, mo, BLAST_FULLSTRENGTH);
    }

    return true; // Continue iteration.
}

/**
 * Blast all mobjs away.
 */
void P_BlastRadius(player_t* pl)
{
    mobj_t*             pmo = pl->plr->mo;
    radiusblastparams_t params;

    S_StartSound(SFX_INVITEM_BLAST, pmo);
    P_NoiseAlert(pmo, pmo);

    params.source = pmo;
    params.maxDistance = BLAST_RADIUS_DIST;
    DD_IterateThinkers(P_MobjThinker, radiusBlast, &params);
}

typedef struct {
    float               origin[2];
    float               maxDistance;
    boolean             effective;
} radiusgiveparams_t;

static boolean radiusGiveArmor(thinker_t* th, void* context)
{
    radiusgiveparams_t* params = (radiusgiveparams_t*) context;
    mobj_t*             mo = (mobj_t *) th;
    float               dist;

    if(!mo->player || mo->health <= 0)
        return true; // Continue iteration.

    // Within range?
    dist = P_ApproxDistance(params->origin[VX] - mo->pos[VX],
                            params->origin[VY] - mo->pos[VY]);
    if(dist <= params->maxDistance)
    {
        if((P_GiveArmor2(mo->player, ARMOR_ARMOR, 1)) ||
           (P_GiveArmor2(mo->player, ARMOR_SHIELD, 1)) ||
           (P_GiveArmor2(mo->player, ARMOR_HELMET, 1)) ||
           (P_GiveArmor2(mo->player, ARMOR_AMULET, 1)))
        {
            params->effective = true;
            S_StartSound(SFX_MYSTICINCANT, mo);
        }
    }

    return true; // Continue iteration.
}

static boolean radiusGiveBody(thinker_t* th, void* context)
{
    radiusgiveparams_t* params = (radiusgiveparams_t*) context;
    mobj_t*             mo = (mobj_t *) th;
    float               dist;

    if(!mo->player || mo->health <= 0)
        return true; // Continue iteration.

    // Within range?
    dist = P_ApproxDistance(params->origin[VX] - mo->pos[VX],
                            params->origin[VY] - mo->pos[VY]);
    if(dist <= params->maxDistance)
    {
        int                 amount = 50 + (P_Random() % 50);

        if(P_GiveBody(mo->player, amount))
        {
            params->effective = true;
            S_StartSound(SFX_MYSTICINCANT, mo);
        }
    }

    return true; // Continue iteration.
}

static boolean radiusGiveMana(thinker_t* th, void* context)
{
    radiusgiveparams_t* params = (radiusgiveparams_t*) context;
    mobj_t*             mo = (mobj_t *) th;
    float               dist;

    if(!mo->player || mo->health <= 0)
        return true; // Continue iteration.

    // Within range?
    dist = P_ApproxDistance(params->origin[VX] - mo->pos[VX],
                            params->origin[VY] - mo->pos[VY]);
    if(dist <= params->maxDistance)
    {
        int                 amount = 50 + (P_Random() % 50);

        if((P_GiveMana(mo->player, AT_BLUEMANA, amount)) ||
           (P_GiveMana(mo->player, AT_GREENMANA, amount)))
        {
            params->effective = true;
            S_StartSound(SFX_MYSTICINCANT, mo);
        }
    }

    return true; // Continue iteration.
}

/**
 * Do class specific effect for everyone in radius
 */
boolean P_HealRadius(player_t* player)
{
    mobj_t*             pmo = player->plr->mo;
    radiusgiveparams_t  params;

    params.effective = false;
    params.origin[VX] = pmo->pos[VX];
    params.origin[VY] = pmo->pos[VY];
    params.maxDistance = HEAL_RADIUS_DIST;

    switch(player->class)
    {
    case PCLASS_FIGHTER:
        DD_IterateThinkers(P_MobjThinker, radiusGiveArmor, &params);
        break;

    case PCLASS_CLERIC:
        DD_IterateThinkers(P_MobjThinker, radiusGiveBody, &params);
        break;

    case PCLASS_MAGE:
        DD_IterateThinkers(P_MobjThinker, radiusGiveMana, &params);
        break;

    default:
        break;
    }

    return params.effective;
}

/**
 * @return              @c true, if the missile is at a valid spawn point,
 *                      otherwise explodes it and return @c false.
 */
boolean P_CheckMissileSpawn(mobj_t *missile)
{
    // Move a little forward so an angle can be computed if it
    // immediately explodes
    missile->pos[VX] += missile->mom[MX] / 2;
    missile->pos[VY] += missile->mom[MY] / 2;
    missile->pos[VZ] += missile->mom[MZ] / 2;

    if(!P_TryMove(missile, missile->pos[VX], missile->pos[VY]))
    {
        P_ExplodeMissile(missile);
        return false;
    }

    return true;
}

/**
 * @return             @c NULL, if the missile exploded immediately,
 *                     otherwise returns a mobj_t pointer to the spawned
 *                     missile.
 */
mobj_t *P_SpawnMissile(mobjtype_t type, mobj_t *source, mobj_t *dest)
{
    uint            an;
    float           z;
    mobj_t         *th;
    angle_t         angle;
    float           aim, dist, origdist;

    switch(type)
    {
    case MT_MNTRFX1: // Minotaur swing attack missile
        z = source->pos[VZ] + 40;
        break;

    case MT_MNTRFX2: // Minotaur floor fire missile
        z = source->floorZ;
        break;

    case MT_CENTAUR_FX:
        z = source->pos[VZ] + 45;
        break;

    case MT_ICEGUY_FX:
        z = source->pos[VZ] + 40;
        break;

    case MT_HOLY_MISSILE:
        z = source->pos[VZ] + 40;
        break;

    default:
        z = source->pos[VZ] + 32;
        break;
    }
    z -= source->floorClip;

    angle = R_PointToAngle2(source->pos[VX], source->pos[VY],
                            dest->pos[VX], dest->pos[VY]);
    if(dest->flags & MF_SHADOW)
    {   // Invisible target
        angle += (P_Random() - P_Random()) << 21;
    }

    if(!(th = P_SpawnMobj3f(type, source->pos[VX], source->pos[VY], z, angle, 0)))
        return NULL;

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Originator
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    origdist = P_ApproxDistance(dest->pos[VX] - source->pos[VX],
                                dest->pos[VY] - source->pos[VY]);
    dist = origdist / th->info->speed;
    if(dist < 1)
        dist = 1;
    th->mom[MZ] = (dest->pos[VZ] - source->pos[VZ]) / dist;

    // Use a more three-dimensional method.
    aim =
        BANG2RAD(bamsAtan2
                 ((int) (dest->pos[VZ] - source->pos[VZ]), (int) origdist));

    th->mom[MX] *= cos(aim);
    th->mom[MY] *= cos(aim);
    th->mom[MZ] = sin(aim) * th->info->speed;

    if(P_CheckMissileSpawn(th))
        return th;

    return NULL;
}

/**
 * @return              @c NULL, if the missile exploded immediately,
 *                      otherwise returns a mobj_t pointer to the spawned
 *                      missile.
 */
mobj_t *P_SpawnMissileXYZ(mobjtype_t type, float x, float y, float z,
                          mobj_t *source, mobj_t *dest)
{
    uint            an;
    mobj_t         *th;
    angle_t         angle;
    float           dist;

    z -= source->floorClip;

    angle = R_PointToAngle2(source->pos[VX], source->pos[VY],
                            dest->pos[VX], dest->pos[VY]);
    if(dest->flags & MF_SHADOW)
    {   // Invisible target
        angle += (P_Random() - P_Random()) << 21;
    }

    if(!(th = P_SpawnMobj3f(type, x, y, z, angle, 0)))
        return NULL;

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Originator
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);
    dist = P_ApproxDistance(dest->pos[VX] - source->pos[VX],
                            dest->pos[VY] - source->pos[VY]);
    dist /= th->info->speed;
    if(dist < 1)
        dist = 1;
    th->mom[MZ] = (dest->pos[VZ] - source->pos[VZ]) / dist;

    if(P_CheckMissileSpawn(th))
        return th;

    return NULL;
}

/**
 * @return              @c NULL, if the missile exploded immediately,
 *                      otherwise returns a mobj_t pointer to the spawned
 *                      missile.
 */
mobj_t* P_SpawnMissileAngle(mobjtype_t type, mobj_t* source, angle_t angle,
                            float momz)
{
    unsigned int        an;
    float               pos[3], spawnZOff;
    mobj_t*             mo;

    memcpy(pos, source->pos, sizeof(pos));

    switch(type)
    {
    case MT_MNTRFX1: // Minotaur swing attack missile
        spawnZOff = 40;
        break;

    case MT_ICEGUY_FX2: // Secondary Projectiles of the Ice Guy
        spawnZOff = 3;
        break;

    case MT_MSTAFF_FX2:
        spawnZOff = 40;
        break;

    default:
        if(source->player)
        {
            if(!P_MobjIsCamera(source->player->plr->mo))
                spawnZOff = cfg.plrViewHeight - 9 +
                    source->player->plr->lookDir / 173;
        }
        else
        {
            spawnZOff = 32;
        }
        break;
    }

    if(type == MT_MNTRFX2) // Minotaur floor fire missile
    {
        mo = P_SpawnMobj3f(type, pos[VX], pos[VY], 0, angle, MSF_Z_FLOOR);
    }
    else
    {
        pos[VZ] += spawnZOff;
        pos[VZ] -= source->floorClip;
        mo = P_SpawnMobj3fv(type, pos, angle, 0);
    }

    if(mo)
    {
        if(mo->info->seeSound)
            S_StartSound(mo->info->seeSound, mo);

        mo->target = source; // Originator
        an = angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
        mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);
        mo->mom[MZ] = momz;

        return (P_CheckMissileSpawn(mo) ? mo : NULL);
    }

    return NULL;
}

/**
 * @return              @c NULL, if the missile exploded immediately,
 *                      otherwise returns a mobj_t pointer to the spawned
 *                      missile.
 */
mobj_t *P_SpawnMissileAngleSpeed(mobjtype_t type, mobj_t *source,
                                 angle_t angle, float momz, float speed)
{
    unsigned int        an;
    float               z;
    mobj_t*             mo;

    z = source->pos[VZ];
    z -= source->floorClip;
    mo = P_SpawnMobj3f(type, source->pos[VX], source->pos[VY], z, angle, 0);

    if(mo)
    {
        mo->target = source; // Originator
        an = angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = speed * FIX2FLT(finecosine[an]);
        mo->mom[MY] = speed * FIX2FLT(finesine[an]);
        mo->mom[MZ] = momz;

        return (P_CheckMissileSpawn(mo) ? mo : NULL);
    }

    return NULL;
}

/**
 * Tries to aim at a nearby monster.
 */
mobj_t *P_SpawnPlayerMissile(mobjtype_t type, mobj_t *source)
{
    uint            an;
    angle_t         angle;
    float           pos[3], slope;
    float           fangle = LOOKDIR2RAD(source->player->plr->lookDir);
    float           movfac = 1;
    boolean         dontAim = cfg.noAutoAim;
    int             spawnFlags = 0;

    // Try to find a target
    angle = source->angle;
    slope = P_AimLineAttack(source, angle, 16 * 64);
    if(!lineTarget || dontAim)
    {
        angle += 1 << 26;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!lineTarget)
        {
            angle -= 2 << 26;
            slope = P_AimLineAttack(source, angle, 16 * 64);
        }

        if(!lineTarget || dontAim)
        {
            angle = source->angle;

            slope = sin(fangle) / 1.2;
            movfac = cos(fangle);
        }
    }

    memcpy(pos, source->pos, sizeof(pos));

    if(type == MT_LIGHTNING_FLOOR)
    {
        pos[VZ] = 0;
        slope = 0;
        spawnFlags |= MSF_Z_FLOOR;
    }
    else if(type == MT_LIGHTNING_CEILING)
    {
        pos[VZ] = 0;
        slope = 0;
        spawnFlags |= MSF_Z_CEIL;
    }
    else
    {
        if(!P_MobjIsCamera(source->player->plr->mo))
            pos[VZ] += cfg.plrViewHeight - 9 +
                (source->player->plr->lookDir / 173);
        pos[VZ] -= source->floorClip;
    }

    if(!(MissileMobj = P_SpawnMobj3fv(type, pos, angle, spawnFlags)))
        return NULL;

    MissileMobj->target = source;
    an = angle >> ANGLETOFINESHIFT;
    MissileMobj->mom[MX] =
        movfac * MissileMobj->info->speed * FIX2FLT(finecosine[an]);
    MissileMobj->mom[MY] =
        movfac * MissileMobj->info->speed * FIX2FLT(finesine[an]);
    MissileMobj->mom[MZ] = MissileMobj->info->speed * slope;

    if(MissileMobj->type == MT_MWAND_MISSILE ||
       MissileMobj->type == MT_CFLAME_MISSILE)
    {   // Ultra-fast ripper spawning missile
        MissileMobj->pos[VX] += MissileMobj->mom[MX] / 8;
        MissileMobj->pos[VY] += MissileMobj->mom[MY] / 8;
        MissileMobj->pos[VZ] += MissileMobj->mom[MZ] / 8;
    }
    else
    {   // Normal missile
        MissileMobj->pos[VX] += MissileMobj->mom[MX] / 2;
        MissileMobj->pos[VY] += MissileMobj->mom[MY] / 2;
        MissileMobj->pos[VZ] += MissileMobj->mom[MZ] / 2;
    }

    if(!P_TryMove(MissileMobj, MissileMobj->pos[VX], MissileMobj->pos[VY]))
    {   // Exploded immediately
        P_ExplodeMissile(MissileMobj);
        return NULL;
    }

    return MissileMobj;
}

mobj_t *P_SPMAngle(mobjtype_t type, mobj_t *source, angle_t origAngle)
{
    uint            an;
    angle_t         angle;
    mobj_t         *th;
    float           pos[3], slope;
    float           fangle = LOOKDIR2RAD(source->player->plr->lookDir);
    float           movfac = 1;
    boolean         dontAim = cfg.noAutoAim;

    // See which target is to be aimed at.
    angle = origAngle;
    slope = P_AimLineAttack(source, angle, 16 * 64);
    if(!lineTarget || dontAim)
    {
        angle += 1 << 26;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!lineTarget)
        {
            angle -= 2 << 26;
            slope = P_AimLineAttack(source, angle, 16 * 64);
        }

        if(!lineTarget || dontAim)
        {
            angle = origAngle;

            slope = sin(fangle) / 1.2;
            movfac = cos(fangle);
        }
    }

    memcpy(pos, source->pos, sizeof(pos));
    if(!P_MobjIsCamera(source->player->plr->mo))
        pos[VZ] += cfg.plrViewHeight - 9 +
            (source->player->plr->lookDir / 173);
    pos[VZ] -= source->floorClip;

    if((th = P_SpawnMobj3fv(type, pos, angle, 0)))
    {
        th->target = source;
        an = angle >> ANGLETOFINESHIFT;
        th->mom[MX] = movfac * th->info->speed * FIX2FLT(finecosine[an]);
        th->mom[MY] = movfac * th->info->speed * FIX2FLT(finesine[an]);
        th->mom[MZ] = th->info->speed * slope;

        if(P_CheckMissileSpawn(th))
            return th;
    }

    return NULL;
}

mobj_t* P_SPMAngleXYZ(mobjtype_t type, float x, float y, float z,
                      mobj_t* source, angle_t origAngle)
{
    uint            an;
    mobj_t*         th;
    angle_t         angle;
    float           slope, movfac = 1;
    float           fangle = LOOKDIR2RAD(source->player->plr->lookDir);
    boolean         dontAim = cfg.noAutoAim;

    // See which target is to be aimed at.
    angle = origAngle;
    slope = P_AimLineAttack(source, angle, 16 * 64);
    if(!lineTarget || dontAim)
    {
        angle += 1 << 26;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!lineTarget)
        {
            angle -= 2 << 26;
            slope = P_AimLineAttack(source, angle, 16 * 64);
        }

        if(!lineTarget || dontAim)
        {
            angle = origAngle;
            slope = sin(fangle) / 1.2;
            movfac = cos(fangle);
        }
    }

    if(!P_MobjIsCamera(source->player->plr->mo))
        z += cfg.plrViewHeight - 9 + (source->player->plr->lookDir / 173);
    z -= source->floorClip;

    if((th = P_SpawnMobj3f(type, x, y, z, angle, 0)))
    {
        th->target = source;
        an = angle >> ANGLETOFINESHIFT;
        th->mom[MX] = movfac * th->info->speed * FIX2FLT(finecosine[an]);
        th->mom[MY] = movfac * th->info->speed * FIX2FLT(finesine[an]);
        th->mom[MZ] = th->info->speed * slope;

        if(P_CheckMissileSpawn(th))
            return th;
    }

    return NULL;
}

mobj_t* P_SpawnKoraxMissile(mobjtype_t type, float x, float y, float z,
                            mobj_t* source, mobj_t* dest)
{
    uint            an;
    mobj_t         *th;
    angle_t         angle;
    float           dist;

    z -= source->floorClip;

    angle = R_PointToAngle2(x, y, dest->pos[VX], dest->pos[VY]);
    if(dest->flags & MF_SHADOW)
    {   // Invisible target
        angle += (P_Random() - P_Random()) << 21;
    }

    if((th = P_SpawnMobj3f(type, x, y, z, angle, 0)))
    {
        if(th->info->seeSound)
            S_StartSound(th->info->seeSound, th);

        th->target = source; // Originator
        an = angle >> ANGLETOFINESHIFT;
        th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
        th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

        dist = P_ApproxDistance(dest->pos[VX] - x, dest->pos[VY] - y);
        dist /= th->info->speed;
        if(dist < 1)
            dist = 1;
        th->mom[MZ] = (dest->pos[VZ] - z + 30) / dist;

        if(P_CheckMissileSpawn(th))
            return th;
    }

    return NULL;
}
