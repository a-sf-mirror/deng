/*
 * Moving object handling. Spawn functions.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jHeretic/Doomdef.h"
#include "jHeretic/h_config.h"
#include "jHeretic/h_type.h"
#include "jHeretic/P_local.h"
#include "jHeretic/st_stuff.h"
#include "Common/hu_stuff.h"
#include "jHeretic/Soundst.h"
#include "jHeretic/h_stat.h"
#include "Common/g_common.h"

// MACROS ------------------------------------------------------------------

#define VANISHTICS  (2*TICSPERSEC)

#define STOPSPEED               0x1000
#define FRICTION_NORMAL         0xe800
#define FRICTION_LOW            0xf900
#define FRICTION_FLY            0xeb00

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    G_PlayerReborn(int player);
void    P_SpawnMapThing(thing_t * mthing);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t attackrange;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mobjtype_t PuffType;
mobj_t *MissileMobj;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Returns true if the mobj is still present.
 */
boolean P_SetMobjState(mobj_t *mobj, statenum_t state)
{
    state_t *st;

    if(state == S_NULL)
    {                           // Remove mobj
        mobj->state = (state_t *) S_NULL;
        P_RemoveMobj(mobj);
        return (false);
    }

    if(mobj->ddflags & DDMF_REMOTE)
    {
        Con_Error("P_SetMobjState: Can't set Remote state!\n");
    }

    st = &states[state];
    P_SetState(mobj, state);

    mobj->turntime = false;     // $visangle-facetarget
    if(st->action)
    {                           // Call action function
        st->action(mobj);
    }
    return (true);
}

/*
 * Same as P_SetMobjState, but does not call the state function.
 */
boolean P_SetMobjStateNF(mobj_t *mobj, statenum_t state)
{
    //state_t *st;

    if(state == S_NULL)
    {                           // Remove mobj
        mobj->state = (state_t *) S_NULL;
        P_RemoveMobj(mobj);
        return (false);
    }

    mobj->turntime = false;     // $visangle-facetarget
    P_SetState(mobj, state);
    return (true);
}

void P_ExplodeMissile(mobj_t *mo)
{
    if(IS_CLIENT)
    {
        // Clients won't explode missiles.
        P_SetMobjState(mo, S_NULL);
        return;
    }

    if(mo->type == MT_WHIRLWIND)
    {
        if(++mo->special2 < 60)
        {
            return;
        }
    }
    mo->momx = mo->momy = mo->momz = 0;
    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }
    if(mo->info->deathsound)
    {
        S_StartSound(mo->info->deathsound, mo);
    }
}

void P_FloorBounceMissile(mobj_t *mo)
{
    mo->momz = -mo->momz;
    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);
}

void P_ThrustMobj(mobj_t *mo, angle_t angle, fixed_t move)
{
    angle >>= ANGLETOFINESHIFT;
    mo->momx += FixedMul(move, finecosine[angle]);
    mo->momy += FixedMul(move, finesine[angle]);
}

/*
 * Returns 1 if 'source' needs to turn clockwise, or 0 if 'source' needs
 * to turn counter clockwise.  'delta' is set to the amount 'source'
 * needs to turn.
 */
int P_FaceMobj(mobj_t *source, mobj_t *target, angle_t *delta)
{
    angle_t diff;
    angle_t angle1;
    angle_t angle2;

    angle1 = source->angle;
    angle2 = R_PointToAngle2(source->pos[VX], source->pos[VY],
                             target->pos[VX], target->pos[VY]);
    if(angle2 > angle1)
    {
        diff = angle2 - angle1;
        if(diff > ANGLE_180)
        {
            *delta = ANGLE_MAX - diff;
            return (0);
        }
        else
        {
            *delta = diff;
            return (1);
        }
    }
    else
    {
        diff = angle1 - angle2;
        if(diff > ANGLE_180)
        {
            *delta = ANGLE_MAX - diff;
            return (1);
        }
        else
        {
            *delta = diff;
            return (0);
        }
    }
}

/*
 * The missile special1 field must be mobj_t *target.  Returns true if
 * target was tracked, false if not.
 */
boolean P_SeekerMissile(mobj_t *actor, angle_t thresh, angle_t turnMax)
{
    int     dir;
    int     dist;
    angle_t delta;
    angle_t angle;
    mobj_t *target;

    target = (mobj_t *) actor->special1;
    if(target == NULL)
    {
        return (false);
    }
    if(!(target->flags & MF_SHOOTABLE))
    {                           // Target died
        actor->special1 = 0;
        return (false);
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
    {                           // Turn clockwise
        actor->angle += delta;
    }
    else
    {                           // Turn counter clockwise
        actor->angle -= delta;
    }
    angle = actor->angle >> ANGLETOFINESHIFT;
    actor->momx = FixedMul(actor->info->speed, finecosine[angle]);
    actor->momy = FixedMul(actor->info->speed, finesine[angle]);
    if(actor->pos[VZ] + actor->height < target->pos[VZ] ||
       target->pos[VZ] + target->height < actor->pos[VZ])
    {                           // Need to seek vertically
        dist = P_ApproxDistance(target->pos[VX] - actor->pos[VX],
                                target->pos[VY] - actor->pos[VY]);
        dist = dist / actor->info->speed;
        if(dist < 1)
        {
            dist = 1;
        }
        actor->momz = (target->pos[VZ] - actor->pos[VZ]) / dist;
    }
    return (true);
}

/*
 * Wind pushes the mobj, if its sector special is a wind type.
 */
void P_WindThrust(mobj_t *mo)
{
    static int windTab[3] = { 2048 * 5, 2048 * 10, 2048 * 25 };

    int special = P_XSector(P_GetPtrp(mo->subsector,
                                      DMU_SECTOR))->special;
    switch (special)
    {
    case 40:
    case 41:
    case 42:                    // Wind_East
        P_ThrustMobj(mo, 0, windTab[special - 40]);
        break;
    case 43:
    case 44:
    case 45:                    // Wind_North
        P_ThrustMobj(mo, ANG90, windTab[special - 43]);
        break;
    case 46:
    case 47:
    case 48:                    // Wind_South
        P_ThrustMobj(mo, ANG270, windTab[special - 46]);
        break;
    case 49:
    case 50:
    case 51:                    // Wind_West
        P_ThrustMobj(mo, ANG180, windTab[special - 49]);
        break;
    }
}

fixed_t P_GetMobjFriction(mobj_t *mo)
{
    sector_t* sector = P_GetPtrp(mo->subsector,
                                DMU_SECTOR);

    if(mo->flags2 & MF2_FLY && !(mo->pos[VZ] <= mo->floorz) &&
       !(mo->flags2 & MF2_ONMOBJ))
    {
        return FRICTION_FLY;
    }
    else if(P_XSector(sector)->special == 15)
    {
        return FRICTION_LOW;
    }
    return XS_Friction(sector);
}

void P_XYMovement(mobj_t *mo)
{
    fixed_t ptryx, ptryy;
    player_t *player;
    fixed_t xmove, ymove;

    // $democam: cameramen have their own movement code
    if(P_CameraXYMovement(mo))
        return;

    if(!mo->momx && !mo->momy)
    {
        if(mo->flags & MF_SKULLFLY)
        {                       // A flying mobj slammed into something
            mo->flags &= ~MF_SKULLFLY;
            mo->momx = mo->momy = mo->momz = 0;
            P_SetMobjState(mo, mo->info->seestate);
        }
        return;
    }

    if(mo->flags2 & MF2_WINDTHRUST)
        P_WindThrust(mo);

    player = mo->player;

    if(mo->momx > MAXMOVE)
        mo->momx = MAXMOVE;
    else if(mo->momx < -MAXMOVE)
        mo->momx = -MAXMOVE;

    if(mo->momy > MAXMOVE)
        mo->momy = MAXMOVE;
    else if(mo->momy < -MAXMOVE)
        mo->momy = -MAXMOVE;

    xmove = mo->momx;
    ymove = mo->momy;
    do
    {
        if(xmove > MAXMOVE / 2 || ymove > MAXMOVE / 2)
        {
            ptryx = mo->pos[VX] + xmove / 2;
            ptryy = mo->pos[VY] + ymove / 2;
            xmove >>= 1;
            ymove >>= 1;
        }
        else
        {
            ptryx = mo->pos[VX] + xmove;
            ptryy = mo->pos[VY] + ymove;
            xmove = ymove = 0;
        }
        if(!P_TryMove(mo, ptryx, ptryy))
        {                       // Blocked move
            if(mo->flags2 & MF2_SLIDE)
            {                   // Try to slide along it
                P_SlideMove(mo);
            }
            else if(mo->flags & MF_MISSILE)
            {   // Explode a missile
                if(ceilingline)
                {
                    sector_t* backsector =
                        P_GetPtrp(ceilingline, DMU_BACK_SECTOR);

                    if(backsector && P_GetIntp(backsector,
                                               DMU_CEILING_TEXTURE) == skyflatnum)
                    {
                        // Hack to prevent missiles exploding against the sky
                        if(mo->type == MT_BLOODYSKULL)
                        {
                            mo->momx = mo->momy = 0;
                            mo->momz = -FRACUNIT;
                        }
                        else
                        {
                            P_RemoveMobj(mo);
                        }
                        return;
                    }
                }
                P_ExplodeMissile(mo);
            }
            else
            {
                mo->momx = mo->momy = 0;
            }
        }
    } while(xmove || ymove);

    // Friction

    if(player && player->cheats & CF_NOMOMENTUM)
    {                           // Debug option for no sliding at all
        mo->momx = mo->momy = 0;
        return;
    }
    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
    {                           // No friction for missiles
        return;
    }
    if(mo->pos[VZ] > mo->floorz && !(mo->flags2 & MF2_FLY) &&
       !(mo->flags2 & MF2_ONMOBJ))
    {                           // No friction when falling
        return;
    }
    if(mo->flags & MF_CORPSE)
    {
        // Don't stop sliding if halfway off a step with some momentum
        if(mo->momx > FRACUNIT / 4 || mo->momx < -FRACUNIT / 4 ||
           mo->momy > FRACUNIT / 4 || mo->momy < -FRACUNIT / 4)
        {
            if(mo->floorz != P_GetFixedp(mo->subsector,
                                         DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT))
            {
                return;
            }
        }
    }
    if(mo->momx > -STOPSPEED && mo->momx < STOPSPEED && mo->momy > -STOPSPEED
       && mo->momy < STOPSPEED && (!player ||
                                   (player->cmd.forwardMove == 0 &&
                                    player->cmd.sideMove == 0)))
    {                           // If in a walking frame, stop moving
        if(player)
        {
            if(player->chickenTics)
            {
                if((unsigned)
                   ((player->plr->mo->state - states) - S_CHICPLAY_RUN1) < 4)
                {
                    P_SetMobjState(player->plr->mo, S_CHICPLAY);
                }
            }
            else
            {
                if((unsigned) ((player->plr->mo->state - states) - S_PLAY_RUN1)
                   < 4)
                {
                    P_SetMobjState(player->plr->mo, S_PLAY);
                }
            }
        }
        mo->momx = 0;
        mo->momy = 0;
    }
    else
    {
        if(mo->flags2 & MF2_FLY && !(mo->pos[VZ] <= mo->floorz) &&
           !(mo->flags2 & MF2_ONMOBJ))
        {
            mo->momx = FixedMul(mo->momx, FRICTION_FLY);
            mo->momy = FixedMul(mo->momy, FRICTION_FLY);
        }
        else if(P_XSector(P_GetPtrp(mo->subsector,
                                    DMU_SECTOR))->special == 15)
        {
            // Friction_Low
            mo->momx = FixedMul(mo->momx, FRICTION_LOW);
            mo->momy = FixedMul(mo->momy, FRICTION_LOW);
        }
        else
        {
            fixed_t fric = P_GetMobjFriction(mo);

            mo->momx = FixedMul(mo->momx, fric);
            mo->momy = FixedMul(mo->momy, fric);
        }
    }
}

void P_ZMovement(mobj_t *mo)
{
    int     gravity = XS_Gravity(P_GetPtrp(mo->subsector, DMU_SECTOR));
    int     dist;
    int     delta;

    // $democam: cameramen get special z movement
    if(P_CameraZMovement(mo))
        return;

    // check for smooth step up
    if(mo->player && mo->pos[VZ] < mo->floorz)
    {
        mo->player->plr->viewheight -= mo->floorz - mo->pos[VZ];
        mo->player->plr->deltaviewheight =
            ((cfg.plrViewHeight << FRACBITS) - mo->player->plr->viewheight) >> 3;
    }

    // adjust height
    mo->pos[VZ] += mo->momz;

    if(!IS_CLIENT)
    {
        if(mo->flags & MF_FLOAT && mo->target)
        {
            // float down towards target if too close
            if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
            {
                dist =
                    P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                     mo->pos[VY] - mo->target->pos[VY]);
                delta = (mo->target->pos[VZ] + (mo->height >> 1)) - mo->pos[VZ];
                if(delta < 0 && dist < -(delta * 3))
                {
                    mo->pos[VZ] -= FLOATSPEED;
                    P_SetThingSRVOZ(mo, -FLOATSPEED);
                }
                else if(delta > 0 && dist < (delta * 3))
                {
                    mo->pos[VZ] += FLOATSPEED;
                    P_SetThingSRVOZ(mo, FLOATSPEED);
                }
            }
        }
        if(mo->player && mo->flags2 & MF2_FLY && !(mo->pos[VZ] <= mo->floorz) &&
           leveltime & 2)
        {
            mo->pos[VZ] += finesine[(FINEANGLES / 20 * leveltime >> 2) & FINEMASK];
        }
    }

    // clip movement
    if(mo->pos[VZ] <= mo->floorz)
    {                           // Hit the floor
        if(mo->flags & MF_MISSILE)
        {
            mo->pos[VZ] = mo->floorz;
            if(mo->flags2 & MF2_FLOORBOUNCE)
            {
                P_FloorBounceMissile(mo);
                return;
            }
            else if(mo->type == MT_MNTRFX2)
            {                   // Minotaur floor fire can go up steps
                return;
            }
            else
            {
                P_ExplodeMissile(mo);
                return;
            }
        }
        if(mo->pos[VZ] - mo->momz > mo->floorz)
        {                       // Spawn splashes, etc.
            P_HitFloor(mo);
        }
        mo->pos[VZ] = mo->floorz;
        if(!IS_CLIENT)
        {
            if(mo->momz < 0)
            {
                if(mo->player && mo->momz < -gravity * 8 && !(mo->flags2 & MF2_FLY))    // squat down
                {
                    mo->player->plr->deltaviewheight = mo->momz >> 3;
                    mo->player->jumptics = 12;  // can't jump in a while.
                    S_StartSound(sfx_plroof, mo);
                    // Don't lookspring with mouselook.
                    if(!cfg.usemlook)
                        mo->player->centering = true;
                }
                mo->momz = 0;
            }
            if(mo->flags & MF_SKULLFLY)
            {                   // The skull slammed into something
                mo->momz = -mo->momz;
            }
            if(mo->info->crashstate && (mo->flags & MF_CORPSE))
            {
                P_SetMobjState(mo, mo->info->crashstate);
                return;
            }
        }
        else
        {
            if(mo->momz < 0)
                mo->momz = 0;
        }
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(mo->momz == 0)
            mo->momz = -(gravity >> 3) * 2;
        else
            mo->momz -= gravity >> 3;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->momz == 0)
            mo->momz = -gravity * 2;
        else
            mo->momz -= gravity;
    }

    if(mo->pos[VZ] + mo->height > mo->ceilingz)
    {                           // hit the ceiling
        if(mo->momz > 0)
            mo->momz = 0;

        mo->pos[VZ] = mo->ceilingz - mo->height;
        if(mo->flags & MF_SKULLFLY)
        {                       // the skull slammed into something
            mo->momz = -mo->momz;
        }
        if(mo->flags & MF_MISSILE)
        {
            if(P_GetIntp(mo->subsector,
                         DMU_SECTOR_OF_SUBSECTOR | DMU_CEILING_TEXTURE) == skyflatnum)
            {
                if(mo->type == MT_BLOODYSKULL)
                {
                    mo->momx = mo->momy = 0;
                    mo->momz = -FRACUNIT;
                }
                else
                {
                    P_RemoveMobj(mo);
                }
                return;
            }
            P_ExplodeMissile(mo);
            return;
        }
    }
}

void P_NightmareRespawn(mobj_t *mobj)
{
    fixed_t pos[3];
    mobj_t *mo;
    thing_t *mthing;

    pos[VX] = mobj->spawnpoint.x << FRACBITS;
    pos[VY] = mobj->spawnpoint.y << FRACBITS;

    if(!P_CheckPosition(mobj, pos[VX], pos[VY]))
        return;                 // somthing is occupying it's position

    // spawn a teleport fog at old spot
    mo = P_SpawnMobj(mobj->pos[VX], mobj->pos[VY],
                     P_GetFixedp(mobj->subsector,
                                 DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT) + TELEFOGHEIGHT,
                     MT_TFOG);

    S_StartSound(sfx_telept, mo);

    // spawn a teleport fog at the new spot
    mo = P_SpawnMobj(pos[VX], pos[VY],
                     P_GetFixedp(R_PointInSubsector(pos[VX], pos[VY]),
                                 DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT) + TELEFOGHEIGHT,
                     MT_TFOG);

    S_StartSound(sfx_telept, mo);

    // spawn the new monster
    mthing = &mobj->spawnpoint;

    // spawn it
    if(mobj->info->flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else
        pos[VZ] = ONFLOORZ;

    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], mobj->type);
    mo->spawnpoint = mobj->spawnpoint;
    mo->angle = ANG45 * (mthing->angle / 45);

    if(mthing->options & MTF_AMBUSH)
        mo->flags |= MF_AMBUSH;

    mo->reactiontime = 18;

    // remove the old monster
    P_RemoveMobj(mobj);
}

/*
 * Thinker for the ultra-fast blaster PL2 ripper-spawning missile.
 */
void P_BlasterMobjThinker(mobj_t *mobj)
{
    int     i;
    fixed_t xfrac;
    fixed_t yfrac;
    fixed_t zfrac;
    fixed_t z;
    boolean changexy;

    // Handle movement
    if(mobj->momx || mobj->momy || (mobj->pos[VZ] != mobj->floorz) || mobj->momz)
    {
        xfrac = mobj->momx >> 3;
        yfrac = mobj->momy >> 3;
        zfrac = mobj->momz >> 3;
        changexy = xfrac || yfrac;
        for(i = 0; i < 8; i++)
        {
            if(changexy)
            {
                if(!P_TryMove(mobj, mobj->pos[VX] + xfrac, mobj->pos[VY] + yfrac))
                {               // Blocked move
                    P_ExplodeMissile(mobj);
                    return;
                }
            }
            mobj->pos[VZ] += zfrac;
            if(mobj->pos[VZ] <= mobj->floorz)
            {                   // Hit the floor
                mobj->pos[VZ] = mobj->floorz;
                P_HitFloor(mobj);
                P_ExplodeMissile(mobj);
                return;
            }
            if(mobj->pos[VZ] + mobj->height > mobj->ceilingz)
            {                   // Hit the ceiling
                mobj->pos[VZ] = mobj->ceilingz - mobj->height;
                P_ExplodeMissile(mobj);
                return;
            }
            if(changexy && (P_Random() < 64))
            {
                z = mobj->pos[VZ] - 8 * FRACUNIT;
                if(z < mobj->floorz)
                {
                    z = mobj->floorz;
                }
                P_SpawnMobj(mobj->pos[VX], mobj->pos[VY], z, MT_BLASTERSMOKE);
            }
        }
    }
    // Advance the state
    if(mobj->tics != -1)
    {
        mobj->tics--;
        while(!mobj->tics)
        {
            if(!P_SetMobjState(mobj, mobj->state->nextstate))
            {                   // mobj was removed
                return;
            }
        }
    }
}

void P_MobjThinker(mobj_t *mobj)
{
    mobj_t *onmo;

    if(mobj->ddflags & DDMF_REMOTE)
    {
        // Remote mobjs are handled separately.
        return;
    }

    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mobj);

    // Handle X and Y momentums
    if(mobj->momx || mobj->momy || (mobj->flags & MF_SKULLFLY))
    {
        P_XYMovement(mobj);
        if(mobj->thinker.function == (think_t) - 1)
        {                       // mobj was removed
            return;
        }
    }
    if(mobj->flags2 & MF2_FLOATBOB)
    {                           // Floating item bobbing motion
        //mobj->pos[VZ] = mobj->floorz+FloatBobOffsets[(mobj->health++)&63];

        // Keep it on the floor.
        mobj->pos[VZ] = mobj->floorz;

        // Negative floorclip raises the mobj off the floor.
        //mobj->floorclip = -mobj->special1;

        // Old floatbob used health as index, let's still increase it
        // as before (in case somebody wants to use it).
        mobj->health++;
    }
    else if((mobj->pos[VZ] != mobj->floorz) || mobj->momz)
    {
        // Handle Z momentum and gravity
        if(mobj->flags2 & MF2_PASSMOBJ)
        {
            if(!(onmo = P_CheckOnmobj(mobj)))
            {
                P_ZMovement(mobj);
            }
            else
            {
                if(mobj->player && mobj->momz < 0)
                {
                    mobj->flags2 |= MF2_ONMOBJ;
                    mobj->momz = 0;
                }
                if(mobj->player && (onmo->player || onmo->type == MT_POD))
                {
                    mobj->momx = onmo->momx;
                    mobj->momy = onmo->momy;
                    if(onmo->pos[VZ] < onmo->floorz)
                    {
                        mobj->pos[VZ] += onmo->floorz - onmo->pos[VZ];
                        if(onmo->player)
                        {
                            onmo->player->plr->viewheight -=
                                onmo->floorz - onmo->pos[VZ];
                            onmo->player->plr->deltaviewheight =
                                ((cfg.plrViewHeight << FRACBITS) -
                                 onmo->player->plr->viewheight) >> 3;
                        }
                        onmo->pos[VZ] = onmo->floorz;
                    }
                }
            }
        }
        else
        {
            P_ZMovement(mobj);
        }
        if(mobj->thinker.function == (think_t) - 1)
        {                       // mobj was removed
            return;
        }
    }

    // $vanish: dead monsters disappear after some time
    if(cfg.corpseTime && mobj->flags & MF_CORPSE)
    {
        if(++mobj->corpsetics < cfg.corpseTime * TICSPERSEC)
        {
            mobj->translucency = 0; // Opaque.
        }
        else if(mobj->corpsetics < cfg.corpseTime * TICSPERSEC + VANISHTICS)
        {
            // Translucent during vanishing.
            mobj->translucency =
                ((mobj->corpsetics -
                  cfg.corpseTime * TICSPERSEC) * 255) / VANISHTICS;
        }
        else
        {
            // Too long; get rid of the corpse.
            P_RemoveMobj(mobj);
            return;
        }
    }

    //
    // cycle through states, calling action functions at transitions
    //
    if(mobj->tics != -1)
    {
        //int oldsprite = mobj->sprite;

        P_SRVOAngleTicker(mobj);    // "angle-servo"; smooth actor turning

        mobj->tics--;
        // you can cycle through multiple states in a tic
        while(!mobj->tics)
        {
            P_ClearThingSRVO(mobj);

            if(!P_SetMobjState(mobj, mobj->state->nextstate))
            {                   // mobj was removed
                return;
            }

            if(mobj->thinker.function == (void (*)()) -1)
                return;         // The mobj was removed.

            // check nextframe and time to it
            /*if(mobj->state->nextstate != S_NULL)
               {
               state_t *st = &states[mobj->state->nextstate];
               if(oldsprite == st->sprite)
               {
               mobj->nextframe = st->frame;
               }
               else
               {
               mobj->nextframe = -1;
               }
               mobj->nexttime = mobj->tics;
               } */
        }
    }
    else if(!IS_CLIENT)
    {
        //mobj->nextframe = -1;

        // Check for monster respawn
        if(!(mobj->flags & MF_COUNTKILL))
        {
            return;
        }
        if(!respawnmonsters)
        {
            return;
        }
        mobj->movecount++;
        if(mobj->movecount < 12 * 35)
        {
            return;
        }
        if(leveltime & 31)
        {
            return;
        }
        if(P_Random() > 4)
        {
            return;
        }
        P_NightmareRespawn(mobj);
    }
}

mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
    mobj_t *mobj;

    //state_t *st;
    mobjinfo_t *info;
    fixed_t space;

#ifdef _DEBUG
    if(type < 0 || type >= Get(DD_NUMMOBJTYPES))
        Con_Error("P_SpawnMobj: Illegal mobj type %i.\n", type);
#endif

    mobj = Z_Malloc(sizeof(*mobj), PU_LEVEL, NULL);
    memset(mobj, 0, sizeof(*mobj));
    info = &mobjinfo[type];
    mobj->type = type;
    mobj->info = info;
    mobj->pos[VX] = x;
    mobj->pos[VY] = y;
    mobj->radius = info->radius;
    mobj->height = info->height;
    mobj->flags = info->flags;
    mobj->flags2 = info->flags2;

    // Let the engine know about solid objects.
    if(mobj->flags & MF_SOLID)
        mobj->ddflags |= DDMF_SOLID;
    if(mobj->flags2 & MF2_DONTDRAW)
        mobj->ddflags |= DDMF_DONTDRAW;

    mobj->damage = info->damage;

    mobj->health = info->spawnhealth
                   * (IS_NETGAME ? cfg.netMobHealthModifier : 1);

    if(gameskill != sk_nightmare)
    {
        mobj->reactiontime = info->reactiontime;
    }
    mobj->lastlook = P_Random() % MAXPLAYERS;

    // Must link before setting state.
    mobj->thinker.function = P_MobjThinker;
    P_AddThinker(&mobj->thinker);

    P_SetState(mobj, info->spawnstate);

    // Set subsector and/or block links.
    P_SetThingPosition(mobj);

    mobj->floorz = P_GetFixedp(mobj->subsector, DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT);
    mobj->ceilingz = P_GetFixedp(mobj->subsector, DMU_SECTOR_OF_SUBSECTOR | DMU_CEILING_HEIGHT);

    if(z == ONFLOORZ)
    {
        mobj->pos[VZ] = mobj->floorz;
    }
    else if(z == ONCEILINGZ)
    {
        mobj->pos[VZ] = mobj->ceilingz - mobj->info->height;
    }
    else if(z == FLOATRANDZ)
    {
        space = ((mobj->ceilingz) - (mobj->info->height)) - mobj->floorz;
        if(space > 48 * FRACUNIT)
        {
            space -= 40 * FRACUNIT;
            mobj->pos[VZ] =
                ((space * P_Random()) >> 8) + mobj->floorz + 40 * FRACUNIT;
        }
        else
        {
            mobj->pos[VZ] = mobj->floorz;
        }
    }
    else
    {
        mobj->pos[VZ] = z;
    }
    if(mobj->flags2 & MF2_FOOTCLIP && P_GetThingFloorType(mobj) != FLOOR_SOLID
       && mobj->floorz == P_GetFixedp(mobj->subsector, DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT))
    {
        mobj->flags2 |= MF2_FEETARECLIPPED;
    }
    else
    {
        mobj->flags2 &= ~MF2_FEETARECLIPPED;
    }
    return (mobj);
}

void P_RemoveMobj(mobj_t *mobj)
{
    // unlink from sector and block lists
    P_UnsetThingPosition(mobj);
    // stop any playing sound
    S_StopSound(0, mobj);
    // free block
    P_RemoveThinker((thinker_t *) mobj);
}

/*
 * Called when a player is spawned on the level
 * Most of the player structure stays unchanged between levels
 */
void P_SpawnPlayer(thing_t * mthing, int plrnum)
{
    player_t *p;
    fixed_t pos[3];
    mobj_t *mobj;
    int     i;
    extern int playerkeys;

    if(!players[plrnum].plr->ingame)
        return;                 // not playing

    p = &players[plrnum];

    Con_Printf("P_SpawnPlayer: spawning player %i, col=%i.\n", plrnum,
               cfg.PlayerColor[plrnum]);

    if(p->playerstate == PST_REBORN)
        G_PlayerReborn(plrnum);

    pos[VX] = mthing->x << FRACBITS;
    pos[VY] = mthing->y << FRACBITS;

    pos[VZ] = ONFLOORZ;
    mobj = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PLAYER);

    // On clients all player mobjs are remote, even the consoleplayer.
    if(IS_CLIENT)
    {
        mobj->flags &= ~MF_SOLID;
        mobj->ddflags = DDMF_REMOTE | DDMF_DONTDRAW;
        // The real flags are received from the server later on.
    }

    // set color translations for player sprites
    i = cfg.PlayerColor[plrnum];
    if(i > 0)
        mobj->flags |= i << MF_TRANSSHIFT;
    p->plr->clAngle = mobj->angle = ANG45 * (mthing->angle / 45);
    p->plr->clLookDir = 0;
    p->plr->lookdir = 0;
    p->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    mobj->player = p;
    mobj->dplayer = p->plr;
    mobj->health = p->health;
    p->plr->mo = mobj;
    p->playerstate = PST_LIVE;
    p->refire = 0;
    p->message = NULL;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->chickenTics = 0;
    p->rain1 = NULL;
    p->rain2 = NULL;
    p->plr->extralight = 0;
    p->plr->fixedcolormap = 0;
    p->plr->viewheight = (cfg.plrViewHeight << FRACBITS);
    p->plr->viewz = mobj->pos[VZ] + p->plr->viewheight;
    P_SetupPsprites(p);         // setup gun psprite
    if(deathmatch)
    {                           // Give all keys in death match mode
        for(i = 0; i < NUMKEYS; i++)
        {
            p->keys[i] = true;
            if(p == &players[consoleplayer])
            {
                playerkeys = 7;
                GL_Update(DDUF_STATBAR);
            }
        }
    }
    else if(p == &players[consoleplayer])
    {
        playerkeys = 0;
        GL_Update(DDUF_STATBAR);
    }

    if(plrnum == consoleplayer)
    {
        // wake up the status bar
        ST_Start();

        // wake up the heads up text
        HU_Start();
    }
}

void P_SpawnMapThing(thing_t * mthing)
{
    int     i;
    int     bit;
    mobj_t *mobj;
    fixed_t pos[3];

    Con_Message("x = %i, y = %i, height = %i, angle = %i, type = %i, options = %i\n",
                mthing->x, mthing->y, mthing->height, mthing->angle, mthing->type, mthing->options);

    // count deathmatch start positions
    if(mthing->type == 11)
    {
        if(deathmatch_p < &deathmatchstarts[16])
        {
            memcpy(deathmatch_p, mthing, sizeof(*mthing));
            deathmatch_p++;
        }
        return;
    }

    // check for players specially
    if(mthing->type <= 4)
    {
        // Register this player start.
        P_RegisterPlayerStart(mthing);
        return;
    }

    // Ambient sound sequences
    if(mthing->type >= 1200 && mthing->type < 1300)
    {
        P_AddAmbientSfx(mthing->type - 1200);
        return;
    }

    // Check for boss spots
    if(mthing->type == 56)      // Monster_BossSpot
    {
        P_AddBossSpot(mthing->x, mthing->y,
                      ANG45 * (mthing->angle / 45));
        return;
    }

    // check for apropriate skill level
    if(!IS_NETGAME && (mthing->options & 16))
        return;

    if(gameskill == sk_baby)
        bit = 1;
    else if(gameskill == sk_nightmare)
        bit = 4;
    else
        bit = 1 << (gameskill - 1);
    if(!(mthing->options & bit))
        return;

    // find which type to spawn
    for(i = 0; i < Get(DD_NUMMOBJTYPES); i++)
        if(mthing->type == mobjinfo[i].doomednum)
            break;

    // Clients only spawn local objects.
    if(IS_CLIENT)
    {
        if(!(mobjinfo[i].flags & MF_LOCAL))
            return;
    }

    if(i == Get(DD_NUMMOBJTYPES))
    {
        Con_Error("P_SpawnMapThing: Unknown type %i at (%i, %i)", mthing->type,
                  mthing->x, mthing->y);
    }

    // don't spawn keys and players in deathmatch
    if(deathmatch && mobjinfo[i].flags & MF_NOTDMATCH)
        return;

    // don't spawn any monsters if -nomonsters
    if(nomonsters && (mobjinfo[i].flags & MF_COUNTKILL))
        return;

    // spawn it
    switch (i)
    {                           // Special stuff
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
        if(shareware)
        {                       // Don't place on map in shareware version
            return;
        }
        break;
    case MT_WMACE:
        if(!shareware)
        {                       // Put in the mace spot list
            P_AddMaceSpot(mthing);
            return;
        }
        return;
    default:
        break;
    }
    pos[VX] = mthing->x << FRACBITS;
    pos[VY] = mthing->y << FRACBITS;
    if(mobjinfo[i].flags & MF_SPAWNCEILING)
    {
        pos[VZ] = ONCEILINGZ;
    }
    else if(mobjinfo[i].flags2 & MF2_SPAWNFLOAT)
    {
        pos[VZ] = FLOATRANDZ;
    }
    else
    {
        pos[VZ] = ONFLOORZ;
    }

    mobj = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], i);
    if(mobj->flags2 & MF2_FLOATBOB)
    {                           // Seed random starting index for bobbing motion
        mobj->health = P_Random();
    }
    if(mobj->tics > 0)
    {
        mobj->tics = 1 + (P_Random() % mobj->tics);
    }
    if(mobj->flags & MF_COUNTKILL)
    {
        totalkills++;
        mobj->spawnpoint = *mthing;
    }
    if(mobj->flags & MF_COUNTITEM)
    {
        totalitems++;
    }
    mobj->angle = ANG45 * (mthing->angle / 45);
    mobj->visangle = mobj->angle >> 16; // "angle-servo"; smooth actor turning
    if(mthing->options & MTF_AMBUSH)
    {
        mobj->flags |= MF_AMBUSH;
    }
}

void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z)
{
    mobj_t *puff;

    // Clients do not spawn puffs.
    if(IS_CLIENT)
        return;

    z += ((P_Random() - P_Random()) << 10);
    puff = P_SpawnMobj(x, y, z, PuffType);
    if(puff->info->attacksound)
    {
        S_StartSound(puff->info->attacksound, puff);
    }
    switch (PuffType)
    {
    case MT_BEAKPUFF:
    case MT_STAFFPUFF:
        puff->momz = FRACUNIT;
        break;
    case MT_GAUNTLETPUFF1:
    case MT_GAUNTLETPUFF2:
        puff->momz = .8 * FRACUNIT;
    default:
        break;
    }
}

void P_BloodSplatter(fixed_t x, fixed_t y, fixed_t z, mobj_t *originator)
{
    mobj_t *mo;

    mo = P_SpawnMobj(x, y, z, MT_BLOODSPLATTER);
    mo->target = originator;
    mo->momx = (P_Random() - P_Random()) << 9;
    mo->momy = (P_Random() - P_Random()) << 9;
    mo->momz = FRACUNIT * 2;
}

void P_RipperBlood(mobj_t *mo)
{
    mobj_t *th;
    fixed_t pos[3];

    memcpy(pos, mo->pos, sizeof(pos));
    pos[VX] += ((P_Random() - P_Random()) << 12);
    pos[VY] += ((P_Random() - P_Random()) << 12);
    pos[VZ] += ((P_Random() - P_Random()) << 12);
    th = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_BLOOD);
    th->flags |= MF_NOGRAVITY;
    th->momx = mo->momx >> 1;
    th->momy = mo->momy >> 1;
    th->tics += P_Random() & 3;
}

int P_GetThingFloorType(mobj_t *thing)
{
    return (TerrainTypes[P_GetIntp(thing->subsector,
                                   DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_TEXTURE)]);
}

int P_HitFloor(mobj_t *thing)
{
    mobj_t *mo;

    if(thing->floorz != P_GetFixedp(thing->subsector,
                                    DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT))
    {
        // don't splash if landing on the edge above water/lava/etc....
        return (FLOOR_SOLID);
    }

    switch (P_GetThingFloorType(thing))
    {
    case FLOOR_WATER:
        P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SPLASHBASE);
        mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SPLASH);
        mo->target = thing;
        mo->momx = (P_Random() - P_Random()) << 8;
        mo->momy = (P_Random() - P_Random()) << 8;
        mo->momz = 2 * FRACUNIT + (P_Random() << 8);
        S_StartSound(sfx_gloop, mo);
        return (FLOOR_WATER);

    case FLOOR_LAVA:
        P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_LAVASPLASH);
        mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_LAVASMOKE);
        mo->momz = FRACUNIT + (P_Random() << 7);
        S_StartSound(sfx_burn, mo);
        return (FLOOR_LAVA);

    case FLOOR_SLUDGE:
        P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SLUDGESPLASH);
        mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], ONFLOORZ, MT_SLUDGECHUNK);
        mo->target = thing;
        mo->momx = (P_Random() - P_Random()) << 8;
        mo->momy = (P_Random() - P_Random()) << 8;
        mo->momz = FRACUNIT + (P_Random() << 8);
        return (FLOOR_SLUDGE);
    }
    return (FLOOR_SOLID);
}

/*
 * Returns true if the missile is at a valid spawn point, otherwise
 * explodes it and returns false.
 */
boolean P_CheckMissileSpawn(mobj_t *missile)
{
    //missile->tics -= P_Random()&3;

    // move a little forward so an angle can be computed if it
    // immediately explodes
    missile->pos[VX] += (missile->momx >> 1);
    missile->pos[VY] += (missile->momy >> 1);
    missile->pos[VZ] += (missile->momz >> 1);
    if(!P_TryMove(missile, missile->pos[VX], missile->pos[VY]))
    {
        P_ExplodeMissile(missile);
        return (false);
    }
    return (true);
}

/*
 * Returns NULL if the missile exploded immediately, otherwise returns
 * a mobj_t pointer to the missile.
 */
mobj_t *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type)
{
    fixed_t z;
    mobj_t *th;
    angle_t an;
    int     dist;

    switch (type)
    {
    case MT_MNTRFX1:            // Minotaur swing attack missile
        z = source->pos[VZ] + 40 * FRACUNIT;
        break;
    case MT_MNTRFX2:            // Minotaur floor fire missile
        z = ONFLOORZ;
        break;
    case MT_SRCRFX1:            // Sorcerer Demon fireball
        z = source->pos[VZ] + 48 * FRACUNIT;
        break;
    case MT_KNIGHTAXE:          // Knight normal axe
    case MT_REDAXE:         // Knight red power axe
        z = source->pos[VZ] + 36 * FRACUNIT;
        break;
    default:
        z = source->pos[VZ] + 32 * FRACUNIT;
        break;
    }

    if(source->flags2 & MF2_FEETARECLIPPED)
    {
        z -= FOOTCLIPSIZE;
    }

    th = P_SpawnMobj(source->pos[VX], source->pos[VY], z, type);
    if(th->info->seesound)
    {
        S_StartSound(th->info->seesound, th);
    }

    th->target = source;        // Originator
    an = R_PointToAngle2(source->pos[VX], source->pos[VY],
                         dest->pos[VX], dest->pos[VY]);
    if(dest->flags & MF_SHADOW)
    {                           // Invisible target
        an += (P_Random() - P_Random()) << 21;
    }

    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->momx = FixedMul(th->info->speed, finecosine[an]);
    th->momy = FixedMul(th->info->speed, finesine[an]);
    dist = P_ApproxDistance(dest->pos[VX] - source->pos[VX],
                            dest->pos[VY] - source->pos[VY]);
    dist = dist / th->info->speed;
    if(dist < 1)
        dist = 1;

    th->momz = (dest->pos[VZ] - source->pos[VZ]) / dist;
    return (P_CheckMissileSpawn(th) ? th : NULL);
}

/*
 * Returns NULL if the missile exploded immediately, otherwise returns
 * a mobj_t pointer to the missile.
 */
mobj_t *P_SpawnMissileAngle(mobj_t *source, mobjtype_t type, angle_t angle,
                            fixed_t momz)
{
    fixed_t z;
    mobj_t *mo;

    switch (type)
    {
    case MT_MNTRFX1:            // Minotaur swing attack missile
        z = source->pos[VZ] + 40 * FRACUNIT;
        break;
    case MT_MNTRFX2:            // Minotaur floor fire missile
        z = ONFLOORZ;
        break;
    case MT_SRCRFX1:            // Sorcerer Demon fireball
        z = source->pos[VZ] + 48 * FRACUNIT;
        break;
    default:
        z = source->pos[VZ] + 32 * FRACUNIT;
        break;
    }

    if(source->flags2 & MF2_FEETARECLIPPED)
        z -= FOOTCLIPSIZE;

    mo = P_SpawnMobj(source->pos[VX], source->pos[VY], z, type);
    if(mo->info->seesound)
        S_StartSound(mo->info->seesound, mo);

    mo->target = source;        // Originator
    mo->angle = angle;
    angle >>= ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[angle]);
    mo->momy = FixedMul(mo->info->speed, finesine[angle]);
    mo->momz = momz;
    return (P_CheckMissileSpawn(mo) ? mo : NULL);
}

/*
 * Tries to aim at a nearby monster
 */
mobj_t *P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type)
{
    angle_t an;
    fixed_t pos[3], slope;
    float   fangle = LOOKDIR2RAD(source->player->plr->lookdir), movfactor = 1;
    boolean dontAim =
        cfg.noAutoAim /*&& !demorecording && !demoplayback && !IS_NETGAME */ ;

    // Try to find a target
    an = source->angle;
    slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
    if(!linetarget || dontAim)
    {
        an += 1 << 26;
        slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        if(!linetarget)
        {
            an -= 2 << 26;
            slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        }
        if(!linetarget || dontAim)
        {
            an = source->angle;
            /*if(demoplayback || demorecording)
               slope = (((int)source->player->plr->lookdir)<<FRACBITS)/173;
               else
               { */
            slope = FRACUNIT * sin(fangle) / 1.2;
            movfactor = cos(fangle);
            //  }
        }
    }

    memcpy(pos, source->pos, sizeof(pos));
    pos[VZ] += (cfg.plrViewHeight - 9) * FRACUNIT +
               (((int) source->player->plr->lookdir) << FRACBITS) / 173;

    if(source->flags2 & MF2_FEETARECLIPPED)
        pos[VZ] -= FOOTCLIPSIZE;

    MissileMobj = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], type);

    if(MissileMobj->info->seesound)
        S_StartSound(MissileMobj->info->seesound, MissileMobj);

    MissileMobj->target = source;
    MissileMobj->angle = an;
    MissileMobj->momx =
        movfactor * FixedMul(MissileMobj->info->speed,
                             finecosine[an >> ANGLETOFINESHIFT]);
    MissileMobj->momy =
        movfactor * FixedMul(MissileMobj->info->speed,
                             finesine[an >> ANGLETOFINESHIFT]);
    MissileMobj->momz = FixedMul(MissileMobj->info->speed, slope);
    if(MissileMobj->type == MT_BLASTERFX1)
    {                           // Ultra-fast ripper spawning missile
        MissileMobj->pos[VX] += (MissileMobj->momx >> 3);
        MissileMobj->pos[VY] += (MissileMobj->momy >> 3);
        MissileMobj->pos[VZ] += (MissileMobj->momz >> 3);
    }
    else
    {                           // Normal missile
        MissileMobj->pos[VX] += (MissileMobj->momx >> 1);
        MissileMobj->pos[VY] += (MissileMobj->momy >> 1);
        MissileMobj->pos[VZ] += (MissileMobj->momz >> 1);
    }

    if(!P_TryMove(MissileMobj, MissileMobj->pos[VX], MissileMobj->pos[VY]))
    {                           // Exploded immediately
        P_ExplodeMissile(MissileMobj);
        return (NULL);
    }
    return (MissileMobj);
}

mobj_t *P_SPMAngle(mobj_t *source, mobjtype_t type, angle_t angle)
{
    mobj_t *th;
    angle_t an;
    fixed_t pos[3], slope;
    float   fangle = LOOKDIR2RAD(source->player->plr->lookdir), movfactor = 1;
    boolean dontAim = cfg.noAutoAim;

    // see which target is to be aimed at
    an = angle;
    slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
    if(!linetarget || dontAim)
    {
        an += 1 << 26;
        slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        if(!linetarget)
        {
            an -= 2 << 26;
            slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        }
        if(!linetarget || dontAim)
        {
            an = angle;

            slope = FRACUNIT * sin(fangle) / 1.2;
            movfactor = cos(fangle);
        }
    }

    memcpy(pos, source->pos, sizeof(pos));
    pos[VZ] += (cfg.plrViewHeight - 9) * FRACUNIT +
               (((int) source->player->plr->lookdir) << FRACBITS) / 173;

    if(source->flags2 & MF2_FEETARECLIPPED)
        pos[VZ] -= FOOTCLIPSIZE;

    th = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], type);

    if(th->info->seesound)
        S_StartSound(th->info->seesound, th);

    th->target = source;
    th->angle = an;
    th->momx =
        movfactor * FixedMul(th->info->speed,
                             finecosine[an >> ANGLETOFINESHIFT]);
    th->momy =
        movfactor * FixedMul(th->info->speed,
                             finesine[an >> ANGLETOFINESHIFT]);
    th->momz = FixedMul(th->info->speed, slope);
    return (P_CheckMissileSpawn(th) ? th : NULL);
}

void C_DECL A_ContMobjSound(mobj_t *actor)
{
    switch (actor->type)
    {
    case MT_KNIGHTAXE:
        S_StartSound(sfx_kgtatk, actor);
        break;
    case MT_MUMMYFX1:
        S_StartSound(sfx_mumhed, actor);
        break;
    default:
        break;
    }
}
