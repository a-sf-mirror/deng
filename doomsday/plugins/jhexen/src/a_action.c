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
 * a_action.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jhexen.h"

#include "gamemap.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

#define ORBITRES            256
#define FLOATBOBRES         64

#define TELEPORT_LIFE       1

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

static float* orbitTableX = NULL;
static float* orbitTableY = NULL;
static float* floatbobOffsetTable = NULL;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void X_CreateLUTs(void)
{
    uint i;

    orbitTableX = Z_Malloc(sizeof(float) * ORBITRES, PU_STATIC, 0);
    for(i = 0; i < ORBITRES; ++i)
        orbitTableX[i] = cos(((float) i) / 40.74f) * 15;

    orbitTableY = Z_Malloc(sizeof(float) * ORBITRES, PU_STATIC, 0);
    for(i = 0; i < ORBITRES; ++i)
        orbitTableY[i] = sin(((float) i) / 40.74f) * 15;

    floatbobOffsetTable = Z_Malloc(sizeof(float) * FLOATBOBRES, PU_STATIC, 0);
    for(i = 0; i < FLOATBOBRES; ++i)
        floatbobOffsetTable[i] = sin(((float) i) / 10.186f) * 8;
}

void X_DestroyLUTs(void)
{
    Z_Free(orbitTableX);
    Z_Free(orbitTableY);
    Z_Free(floatbobOffsetTable);
}

float P_FloatBobOffset(byte n)
{
    return floatbobOffsetTable[n < 0? 0 : n > FLOATBOBRES - 1? FLOATBOBRES - 1 : n];
}

void C_DECL A_PotteryExplode(mobj_t* actor)
{
    assert(actor);
    {
    GameMap* map = Thinker_Map((thinker_t*) actor);
    int i, maxBits = (P_Random() & 3) + 3;

    for(i = 0; i < maxBits; ++i)
    {
        mobj_t* mo;

        if((mo = GameMap_SpawnMobj3fv(map, MT_POTTERYBIT1, actor->pos, P_Random() << 24, 0)))
        {
            P_MobjChangeState(mo, P_GetState(mo->type, SN_SPAWN) + (P_Random() % 5));

            mo->mom[MZ] = FIX2FLT(((P_Random() & 7) + 5) * (3 * FRACUNIT / 4));
            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        }
    }

    S_StartSound(SFX_POTTERY_EXPLODE, actor);

    if(actor->args[0])
    {   // Spawn an item.
        if(!noMonstersParm ||
           !(MOBJINFO[P_MapScriptThingIdToMobjType(actor->args[0])].
             flags & MF_COUNTKILL))
        {   // Only spawn monsters if not -nomonsters.
            GameMap_SpawnMobj3fv(map, P_MapScriptThingIdToMobjType(actor->args[0]), actor->pos, actor->angle, 0);
        }
    }

    P_MobjRemove(actor, false);
    }
}

void C_DECL A_PotteryChooseBit(mobj_t* actor)
{
    assert(actor);
    P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH) + (P_Random() % 5) + 1);
    actor->tics = 256 + (P_Random() << 1);
}

void C_DECL A_PotteryCheck(mobj_t* actor)
{
    assert(actor);
    {

    if(!IS_NETGAME)
    {
        mobj_t* mo = players[CONSOLEPLAYER].plr->mo;

        if(P_CheckSight(actor, mo) &&
           (abs
            (R_PointToAngle2(mo->pos[VX], mo->pos[VY],
                             actor->pos[VX], actor->pos[VY]) -
             mo->angle) <= ANGLE_45))
        {   // Previous state (pottery bit waiting state).
            P_MobjChangeState(actor, actor->state - &STATES[0] - 1);
        }

        return;
    }

    {
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        mobj_t* mo;

        if(!players[i].plr->inGame)
            continue;

        mo = players[i].plr->mo;

        if(P_CheckSight(actor, mo) &&
           (abs
            (R_PointToAngle2(mo->pos[VX], mo->pos[VY],
                             actor->pos[VX], actor->pos[VY]) -
             mo->angle) <= ANGLE_45))
        {   // Previous state (pottery bit waiting state).
            P_MobjChangeState(actor, actor->state - &STATES[0] - 1);
            return;
        }
    }
    }
    }
}

void C_DECL A_CorpseBloodDrip(mobj_t* actor)
{
    assert(actor);
    if(P_Random() > 128)
        return;
    GameMap_SpawnMobj3f(Thinker_Map((thinker_t*) actor), MT_CORPSEBLOODDRIP,
                        actor->pos[VX], actor->pos[VY], actor->pos[VZ] + actor->height / 2, actor->angle, 0);
}

void C_DECL A_CorpseExplode(mobj_t* actor)
{
    assert(actor);
    {
    GameMap* map = Thinker_Map((thinker_t*) actor);
    mobj_t* mo;
    int i;

    for(i = (P_Random() & 3) + 3; i; i--)
    {
        if((mo = GameMap_SpawnMobj3fv(map, MT_CORPSEBIT, actor->pos, P_Random() << 24, 0)))
        {
            P_MobjChangeState(mo, P_GetState(mo->type, SN_SPAWN) + (P_Random() % 3));
            mo->mom[MZ] = FIX2FLT((P_Random() & 7) + 5) * .75f;
            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        }
    }

    // Spawn a skull.
    if((mo = GameMap_SpawnMobj3fv(map, MT_CORPSEBIT, actor->pos, P_Random() << 24, 0)))
    {
        P_MobjChangeState(mo, S_CORPSEBIT_4);
        mo->mom[MZ] = FIX2FLT((fixed_t) (P_Random() & 7) + 5) * .75f;
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        S_StartSound(SFX_FIRED_DEATH, mo);
    }

    P_MobjRemove(actor, false);
    }
}

#ifdef MSVC
// I guess the compiler gets confused by the multitude of P_Random()s...
#  pragma optimize("g", off)
#endif

void C_DECL A_LeafSpawn(mobj_t* actor)
{
    assert(actor);
    {
    GameMap* map = Thinker_Map((thinker_t*) actor);
    mobj_t* mo;
    float pos[3];
    int i;

    for(i = (P_Random() & 3) + 1; i; i--)
    {
        pos[VX] = actor->pos[VX];
        pos[VY] = actor->pos[VY];
        pos[VZ] = actor->pos[VZ];

        pos[VX] += FIX2FLT((P_Random() - P_Random()) << 14);
        pos[VY] += FIX2FLT((P_Random() - P_Random()) << 14);
        pos[VZ] += FIX2FLT(P_Random() << 14);

        /**
         * @fixme We should not be using the original indices to determine
         * the mobjtype. Use a local table instead.
         */
        if((mo = GameMap_SpawnMobj3fv(map, MT_LEAF1 + (P_Random() & 1), pos, actor->angle, 0)))
        {
            P_ThrustMobj(mo, actor->angle, FIX2FLT(P_Random() << 9) + 3);
            mo->target = actor;
            mo->special1 = 0;
        }
    }
    }
}
#ifdef MSVC
#  pragma optimize("", on)
#endif

void C_DECL A_LeafThrust(mobj_t* actor)
{
    assert(actor);
    if(P_Random() > 96)
        return;
    actor->mom[MZ] += FIX2FLT(P_Random() << 9) + 1;
}

void C_DECL A_LeafCheck(mobj_t* actor)
{
    assert(actor);
    {
    int n;

    actor->special1++;
    if(actor->special1 >= 20)
    {
        P_MobjChangeState(actor, S_NULL);
        return;
    }

    if(P_Random() > 64)
    {
        if(actor->mom[MX] == 0 && actor->mom[MY] == 0)
        {
            P_ThrustMobj(actor, actor->target->angle, FIX2FLT(P_Random() << 9) + 1);
        }
        return;
    }

    P_MobjChangeState(actor, S_LEAF1_8);
    n = P_Random();
    actor->mom[MZ] = FIX2FLT(n << 9) + 1;
    P_ThrustMobj(actor, actor->target->angle, FIX2FLT(P_Random() << 9) + 2);
    actor->flags |= MF_MISSILE;
    }
}

/**
 * Bridge variables
 *  Parent
 *      special1    true == removing from world
 *
 *  Child
 *      target      pointer to center mobj
 *      args[0]     angle of ball
 */

void C_DECL A_BridgeOrbit(mobj_t* actor)
{
    if(!actor)
        return;

    if(actor->target->special1)
    {
        P_MobjChangeState(actor, S_NULL);
    }
    actor->args[0] += 3;

    P_MobjUnsetPosition(actor);

    actor->pos[VX] = actor->target->pos[VX];
    actor->pos[VY] = actor->target->pos[VY];

    actor->pos[VX] += orbitTableX[actor->args[0]];
    actor->pos[VY] += orbitTableY[actor->args[0]];

    P_MobjSetPosition(actor);
}

void C_DECL A_BridgeInit(mobj_t* actor)
{
    assert(actor);
    {
    GameMap* map = Thinker_Map((thinker_t*) actor);
    byte startangle;
    mobj_t* ball1, *ball2, *ball3;

    startangle = P_Random();
    actor->special1 = 0;

    // Spawn triad into world.
    if((ball1 = GameMap_SpawnMobj3fv(map, MT_BRIDGEBALL, actor->pos, actor->angle, 0)))
    {
        ball1->args[0] = startangle;
        ball1->target = actor;
    }

    if((ball2 = GameMap_SpawnMobj3fv(map, MT_BRIDGEBALL, actor->pos, actor->angle, 0)))
    {
        ball2->args[0] = (startangle + 85) & 255;
        ball2->target = actor;
    }

    if((ball3 = GameMap_SpawnMobj3fv(map, MT_BRIDGEBALL, actor->pos, actor->angle, 0)))
    {
        ball3->args[0] = (startangle + 170) & 255;
        ball3->target = actor;
    }

    A_BridgeOrbit(ball1);
    A_BridgeOrbit(ball2);
    A_BridgeOrbit(ball3);
    }
}

void C_DECL A_BridgeRemove(mobj_t* actor)
{
    assert(actor);
    actor->special1 = true; // Removing the bridge.
    actor->flags &= ~MF_SOLID;
    P_MobjChangeState(actor, S_FREE_BRIDGE1);
}

void C_DECL A_HideThing(mobj_t* actor)
{
    assert(actor);
    actor->flags2 |= MF2_DONTDRAW;
}

void C_DECL A_UnHideThing(mobj_t* actor)
{
    assert(actor);
    actor->flags2 &= ~MF2_DONTDRAW;
}

void C_DECL A_SetShootable(mobj_t* actor)
{
    assert(actor);
    actor->flags2 &= ~MF2_NONSHOOTABLE;
    actor->flags |= MF_SHOOTABLE;
}

void C_DECL A_UnSetShootable(mobj_t* actor)
{
    assert(actor);
    actor->flags2 |= MF2_NONSHOOTABLE;
    actor->flags &= ~MF_SHOOTABLE;
}

void C_DECL A_SetAltShadow(mobj_t* actor)
{
    assert(actor);
    actor->flags &= ~MF_SHADOW;
    actor->flags |= MF_ALTSHADOW;
}

void C_DECL A_ContMobjSound(mobj_t* actor)
{
    assert(actor);
    {
    sfxenum_t soundId;

    switch(actor->type)
    {
    case MT_SERPENTFX:      soundId = SFX_SERPENTFX_CONTINUOUS; break;
    case MT_HAMMER_MISSILE: soundId = SFX_FIGHTER_HAMMER_CONTINUOUS; break;
    case MT_QUAKE_FOCUS:    soundId = SFX_EARTHQUAKE; break;
    default:                soundId = SFX_NONE; break;
    }

    S_StartSound(soundId, actor);
    }
}

void C_DECL A_ESound(mobj_t* actor)
{
    assert(actor);
    {
    int soundId;
    if(actor->type ==  MT_SOUNDWIND)
        soundId = SFX_WIND;
    else
        soundId = SFX_NONE;
    S_StartSound(soundId, actor);
    }
}

/**
 * NOTE: See p_enemy for variable descriptions.
 */
void C_DECL A_Summon(mobj_t* actor)
{
    assert(actor);
    {
    GameMap* map = Thinker_Map((thinker_t*) actor);
    mobj_t* mo;

    if((mo = GameMap_SpawnMobj3fv(map, MT_MINOTAUR, actor->pos, actor->angle, 0)))
    {
        mobj_t* master;

        if(P_TestMobjLocation(mo) == false || !actor->tracer)
        {   // Didn't fit - change back to item.
            P_MobjChangeState(mo, S_NULL);

            if((mo = GameMap_SpawnMobj3fv(map, MT_SUMMONMAULATOR, actor->pos, actor->angle, 0)))
                mo->flags2 |= MF2_DROPPED;

            return;
        }

        // @fixme What is this really trying to achieve?
        memcpy((void*) mo->args, &map->time, sizeof(map->time));

        master = actor->tracer;
        if(master->flags & MF_CORPSE)
        {   // Master dead.
            mo->tracer = NULL; // No master.
        }
        else
        {
            mo->tracer = actor->tracer; // Pointer to master (mobj_t *)
            P_GivePower(master->player, PT_MINOTAUR);
        }

        // Make smoke puff.
        GameMap_SpawnMobj3fv(map, MT_MNTRSMOKE, actor->pos, P_Random() << 24, 0);
        S_StartSound(SFX_MAULATOR_ACTIVE, actor);
    }
    }
}

/**
 * Fog Variables:
 *
 *      args[0]     Speed (0..10) of fog
 *      args[1]     Angle of spread (0..128)
 *      args[2]     Frequency of spawn (1..10)
 *      args[3]     Lifetime countdown
 *      args[4]     Boolean: fog moving?
 *      special1        Internal:  Counter for spawn frequency
 *      special2        Internal:  Index into floatbob table
 */

void C_DECL A_FogSpawn(mobj_t* actor)
{
    assert(actor);
    {
    GameMap* map = Thinker_Map((thinker_t*) actor);
    mobj_t* mo;
    mobjtype_t type = 0;
    angle_t delta, angle;

    if(actor->special1-- > 0)
        return;

    actor->special1 = actor->args[2]; // Reset frequency count.

    switch(P_Random() % 3)
    {
    case 0: type = MT_FOGPATCHS; break;
    case 1: type = MT_FOGPATCHM; break;
    case 2: type = MT_FOGPATCHL; break;
    }

    delta = actor->args[1];
    if(delta == 0)
        delta = 1;

    angle = ((P_Random() % delta) - (delta / 2));
    angle <<= 24;

    if((mo = GameMap_SpawnMobj3fv(map, type, actor->pos, actor->angle + angle, 0)))
    {
        mo->target = actor;
        if(actor->args[0] < 1)
            actor->args[0] = 1;
        mo->args[0] = (P_Random() % (actor->args[0])) + 1; // Random speed.
        mo->args[3] = actor->args[3]; // Set lifetime.
        mo->args[4] = 1; // Set to moving.
        mo->special2 = P_Random() & 63;
    }
    }
}

void C_DECL A_FogMove(mobj_t* actor)
{
    assert(actor);
    {
    float speed = (float) actor->args[0];
    uint an, weaveindex;

    if(!(actor->args[4]))
        return;

    if(actor->args[3]-- <= 0)
    {
        P_SetMobjStateNF(actor, P_GetState(actor->type, SN_DEATH));
        return;
    }

    if((actor->args[3] % 4) == 0)
    {
        weaveindex = actor->special2;
        actor->pos[VZ] += P_FloatBobOffset(weaveindex) * 2;
        actor->special2 = (weaveindex + 1) & 63;
    }

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = speed * FIX2FLT(finecosine[an]);
    actor->mom[MY] = speed * FIX2FLT(finesine[an]);
    }
}

void C_DECL A_PoisonBagInit(mobj_t* actor)
{
    assert(actor);
    {
    GameMap* map = Thinker_Map((thinker_t*) actor);
    mobj_t* mo;

    if((mo = GameMap_SpawnMobj3f(map, MT_POISONCLOUD, actor->pos[VX], actor->pos[VY],
                                 actor->pos[VZ] + 28, P_Random() << 24, 0)))
    {
        // Missile objects must move to impact other objects.
        mo->mom[MX] = FIX2FLT(1);
        mo->special1 = 24 + (P_Random() & 7);
        mo->special2 = 0;
        mo->target = actor->target;
        mo->radius = 20;
        mo->height = 30;
        mo->flags &= ~MF_NOCLIP;
    }
    }
}

void C_DECL A_PoisonBagCheck(mobj_t* actor)
{
    assert(actor);
    if(!--actor->special1)
    {
        P_MobjChangeState(actor, S_POISONCLOUD_X1);
    }
    else
    {
        return;
    }
}

void C_DECL A_PoisonBagDamage(mobj_t* actor)
{
    assert(actor);
    {
    int bobIndex;
    float z;

    A_Explode(actor);

    bobIndex = actor->special2;
    z = P_FloatBobOffset(bobIndex);
    actor->pos[VZ] += z / 16;
    actor->special2 = (bobIndex + 1) & 63;
    }
}

void C_DECL A_PoisonShroom(mobj_t* actor)
{
    assert(actor);
    {
    actor->tics = 128 + (P_Random() << 1);
    }
}

void C_DECL A_CheckThrowBomb(mobj_t* actor)
{
    assert(actor);
    if(fabs(actor->mom[MX]) < 1.5f && fabs(actor->mom[MY]) < 1.5f &&
       actor->mom[MZ] < 2 && actor->state == &STATES[S_THROWINGBOMB6])
    {
        P_MobjChangeState(actor, S_THROWINGBOMB7);
        actor->pos[VZ] = actor->floorZ;
        actor->mom[MZ] = 0;
        actor->flags2 &= ~MF2_FLOORBOUNCE;
        actor->flags &= ~MF_MISSILE;
        actor->flags |= MF_VIEWALIGN;
    }

    if(!--actor->health)
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH));
    }
}

/**
 * Quake variables
 *
 *      args[0]     Intensity on richter scale (2..9)
 *      args[1]     Duration in tics
 *      args[2]     Radius for damage
 *      args[3]     Radius for tremor
 *      args[4]     TID of map thing for focus of quake
 */

boolean A_LocalQuake(byte* args, mobj_t* actor)
{
    assert(args);
    {
    GameMap* map = Thinker_Map((thinker_t*) actor);
    mobj_t* focus, *target;
    int lastfound = 0;
    int success = false;

    actor = actor; // Shutup compiler warning.

    // Find all quake foci.
    do
    {
        if((target = P_FindMobjFromTID(map, args[4], &lastfound)))
        {
            if((focus = GameMap_SpawnMobj3fv(map, MT_QUAKE_FOCUS, target->pos, 0, 0)))
            {
                focus->args[0] = args[0];
                focus->args[1] = args[1] / 2; // Decremented every 2 tics.
                focus->args[2] = args[2];
                focus->args[3] = args[3];
                focus->args[4] = args[4];
                success = true;
            }
        }
    } while(target != NULL);

    return success;
    }
}

void C_DECL A_Quake(mobj_t* actor)
{
    assert(actor);
    {
    angle_t angle;
    player_t* player;
    mobj_t* victim;
    int shakeStrength = actor->args[0];
    int playnum;
    float dist;

    if(actor->args[1]-- > 0)
    {
        for(playnum = 0; playnum < MAXPLAYERS; ++playnum)
        {
            player = &players[playnum];
            if(!players[playnum].plr->inGame)
                continue;

            victim = player->plr->mo;
            dist =
                P_ApproxDistance(actor->pos[VX] - victim->pos[VX],
                                 actor->pos[VY] - victim->pos[VY]);

            dist = FIX2FLT(FLT2FIX(dist) >> (FRACBITS + 6));

            // Tested in tile units (64 pixels).
            if(dist < FIX2FLT(actor->args[3])) // In tremor radius.
            {
                players[playnum].viewShake = shakeStrength;
                players[playnum].update |= PSF_LOCAL_QUAKE;
            }

            // Check if in damage radius.
            if(dist < FIX2FLT(actor->args[2]) &&
               victim->pos[VZ] <= victim->floorZ)
            {
                if(P_Random() < 50)
                {
                    P_DamageMobj(victim, NULL, NULL, HITDICE(1), false);
                }

                // Thrust player around.
                angle = victim->angle + ANGLE_1 * P_Random();
                P_ThrustMobj(victim, angle, FIX2FLT(shakeStrength << (FRACBITS - 1)));
            }
        }
    }
    else
    {
        for(playnum = 0; playnum < MAXPLAYERS; playnum++)
        {
            players[playnum].viewShake = 0;
            players[playnum].update |= PSF_LOCAL_QUAKE;
        }
        P_MobjChangeState(actor, S_NULL);
    }
    }
}

static void telospawn(mobjtype_t type, mobj_t* actor)
{
    GameMap* map = Thinker_Map((thinker_t*) actor);
    mobj_t* mo;

    if((mo = GameMap_SpawnMobj3fv(map, MT_TELOTHER_FX2, actor->pos, actor->angle, 0)))
    {
        mo->special1 = TELEPORT_LIFE; // Lifetime countdown.
        mo->target = actor->target;
        mo->mom[MX] = actor->mom[MX] / 2;
        mo->mom[MY] = actor->mom[MY] / 2;
        mo->mom[MZ] = actor->mom[MZ] / 2;
    }
}

void C_DECL A_TeloSpawnA(mobj_t* mo)
{
    assert(mo);
    telospawn(MT_TELOTHER_FX2, mo);
}

void C_DECL A_TeloSpawnB(mobj_t* mo)
{
    assert(mo);
    telospawn(MT_TELOTHER_FX3, mo);
}

void C_DECL A_TeloSpawnC(mobj_t* mo)
{
    assert(mo);
    telospawn(MT_TELOTHER_FX4, mo);
}

void C_DECL A_TeloSpawnD(mobj_t* mo)
{
    assert(mo);
    telospawn(MT_TELOTHER_FX5, mo);
}

void C_DECL A_CheckTeleRing(mobj_t* actor)
{
    assert(actor);
    if(actor->special1-- <= 0)
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH));
    }
}

void P_SpawnDirt(mobj_t* actor, float radius)
{
    assert(actor);
    {
    GameMap* map = Thinker_Map((thinker_t*) actor);
    float pos[3];
    int mobjType = 0;
    mobj_t* mo;
    uint an;

    an = P_Random() << 5;

    pos[VX] = actor->pos[VX];
    pos[VY] = actor->pos[VY];
    pos[VZ] = actor->pos[VZ];

    pos[VX] += radius * FIX2FLT(finecosine[an]);
    pos[VY] += radius * FIX2FLT(finesine[an]);
    pos[VZ] += FLT2FIX(P_Random() << 9) + 1;

    switch(P_Random() % 6)
    {
    case 0: mobjType = MT_DIRT1; break;
    case 1: mobjType = MT_DIRT2; break;
    case 2: mobjType = MT_DIRT3; break;
    case 3: mobjType = MT_DIRT4; break;
    case 4: mobjType = MT_DIRT5; break;
    case 5: mobjType = MT_DIRT6; break;
    }

    if((mo = GameMap_SpawnMobj3fv(map, mobjType, pos, 0, 0)))
    {
        mo->mom[MZ] = FIX2FLT(P_Random() << 10);
    }
    }
}

/**
 * Thrust Spike Variables
 *      tracer          pointer to dirt clump mobj
 *      special2        speed of raise
 *      args[0]     0 = lowered,  1 = raised
 *      args[1]     0 = normal,   1 = bloody
 */

void C_DECL A_ThrustInitUp(mobj_t* actor)
{
    assert(actor);
    actor->special2 = 5;
    actor->args[0] = 1;
    actor->floorClip = 0;
    actor->flags = MF_SOLID;
    actor->flags2 = MF2_NOTELEPORT | MF2_FLOORCLIP;
    actor->tracer = NULL;
}

void C_DECL A_ThrustInitDn(mobj_t* actor)
{
    assert(actor);
    {
    GameMap* map = Thinker_Map((thinker_t*) actor);
    mobj_t* mo;

    actor->special2 = 5;
    actor->args[0] = 0;
    actor->floorClip = actor->info->height;
    actor->flags = 0;
    actor->flags2 = MF2_NOTELEPORT | MF2_FLOORCLIP | MF2_DONTDRAW;
    if((mo = GameMap_SpawnMobj3fv(map, MT_DIRTCLUMP, actor->pos, 0, 0)))
        actor->tracer = mo;
    }
}

void C_DECL A_ThrustRaise(mobj_t* actor)
{
    assert(actor);
    if(A_RaiseMobj(actor))
    {   // Reached it's target height.
        actor->args[0] = 1;
        if(actor->args[1])
            P_SetMobjStateNF(actor, S_BTHRUSTINIT2_1);
        else
            P_SetMobjStateNF(actor, S_THRUSTINIT2_1);
    }

    // Lose the dirt clump.
    if(actor->floorClip < actor->height && actor->tracer)
    {
        P_MobjRemove(actor->tracer, false);
        actor->tracer = NULL;
    }

    // Spawn some dirt.
    if(P_Random() < 40)
        P_SpawnDirt(actor, actor->radius);
    actor->special2++; // Increase raise speed.
}

void C_DECL A_ThrustLower(mobj_t* actor)
{
    assert(actor);
    if(A_SinkMobj(actor))
    {
        actor->args[0] = 0;
        if(actor->args[1])
            P_SetMobjStateNF(actor, S_BTHRUSTINIT1_1);
        else
            P_SetMobjStateNF(actor, S_THRUSTINIT1_1);
    }
}

void C_DECL A_ThrustBlock(mobj_t* actor)
{
    assert(actor);
    actor->flags |= MF_SOLID;
}

/**
 * Impale all shootables in radius.
 */
void C_DECL A_ThrustImpale(mobj_t *actor)
{
    assert(actor);
    PIT_ThrustSpike(actor);
}

#if MSVC
#  pragma optimize("g",off)
#endif
void C_DECL A_SoAExplode(mobj_t* actor)
{
    assert(actor);
    {
    GameMap* map = Thinker_Map((thinker_t*) actor);
    int i;

    for(i = 0; i < 10; ++i)
    {
        mobj_t* mo;
        float pos[3];

        pos[VX] = actor->pos[VX];
        pos[VY] = actor->pos[VY];
        pos[VZ] = actor->pos[VZ];

        pos[VX] += FIX2FLT((P_Random() - 128) << 12);
        pos[VY] += FIX2FLT((P_Random() - 128) << 12);
        pos[VZ] += FIX2FLT(P_Random() * FLT2FIX(actor->height) / 256);

        if((mo = GameMap_SpawnMobj3fv(map, MT_ZARMORCHUNK, pos, P_Random() << 24, 0)))
        {
            P_MobjChangeState(mo, P_GetState(mo->type, SN_SPAWN) + i);

            mo->mom[MZ] = ((P_Random() & 7) + 5);
            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        }
    }

    if(actor->args[0])
    {   // Spawn an item.
        if(!noMonstersParm ||
           !(MOBJINFO[P_MapScriptThingIdToMobjType(actor->args[0])].
             flags & MF_COUNTKILL))
        {   // Only spawn monsters if not -nomonsters.
            GameMap_SpawnMobj3fv(map, P_MapScriptThingIdToMobjType(actor->args[0]), actor->pos, actor->angle, 0);
        }
    }

    S_StartSound(SFX_SUITOFARMOR_BREAK, actor);
    P_MobjRemove(actor, false);
    }
}
#if MSVC
#  pragma optimize("",on)
#endif

void C_DECL A_BellReset1(mobj_t* actor)
{
    assert(actor);
    actor->flags |= MF_NOGRAVITY;
    actor->height *= 2*2;
}

void C_DECL A_BellReset2(mobj_t* actor)
{
    assert(actor);
    actor->flags |= MF_SHOOTABLE;
    actor->flags &= ~MF_CORPSE;
    actor->health = 5;
}

void C_DECL A_FlameCheck(mobj_t* actor)
{
    assert(actor);
    if(!actor->args[0]--) // Called every 8 tics.
    {
        P_MobjChangeState(actor, S_NULL);
    }
}

/**
 * Bat Spawn Variables
 *  special1    frequency counter
 *  special2
 *  args[0]     frequency of spawn (1=fastest, 10=slowest)
 *  args[1]     spread angle (0..255)
 *  args[2]
 *  args[3]     duration of bats (in octics)
 *  args[4]     turn amount per move (in degrees)
 *
 * Bat Variables
 *  special2    lifetime counter
 *  args[4]     turn amount per move (in degrees)
 */

void C_DECL A_BatSpawnInit(mobj_t* actor)
{
    assert(actor);
    actor->special1 = 0; // Frequency count.
}

void C_DECL A_BatSpawn(mobj_t* actor)
{
    assert(actor);
    {
    mobj_t* mo;
    int delta;
    angle_t angle;

    // Countdown until next spawn
    if(actor->special1-- > 0)
        return;

    actor->special1 = actor->args[0]; // Reset frequency count.

    delta = actor->args[1];
    if(delta == 0)
        delta = 1;
    angle = actor->angle + (((P_Random() % delta) - (delta >> 1)) << 24);

    mo = P_SpawnMissileAngle(MT_BAT, actor, angle, 0);
    if(mo)
    {
        mo->args[0] = P_Random() & 63; // floatbob index
        mo->args[4] = actor->args[4]; // turn degrees
        mo->special2 = actor->args[3] << 3; // Set lifetime
        mo->target = actor;
    }
    }
}

void C_DECL A_BatMove(mobj_t* actor)
{
    assert(actor);
    {
    angle_t angle;
    uint an;
    float speed;

    if(actor->special2 < 0)
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH));
    }
    actor->special2 -= 2;       // Called every 2 tics

    if(P_Random() < 128)
    {
        angle = actor->angle + ANGLE_1 * actor->args[4];
    }
    else
    {
        angle = actor->angle - ANGLE_1 * actor->args[4];
    }

    // Adjust momentum vector to new direction
    an = angle >> ANGLETOFINESHIFT;
    speed = actor->info->speed * FIX2FLT(P_Random() << 10);
    actor->mom[MX] = speed * FIX2FLT(finecosine[an]);
    actor->mom[MY] = speed * FIX2FLT(finesine[an]);

    if(P_Random() < 15)
        S_StartSound(SFX_BAT_SCREAM, actor);

    // Handle Z movement
    actor->pos[VZ] =
        actor->target->pos[VZ] + 2 * P_FloatBobOffset((int) actor->args[0]);
    actor->args[0] = (actor->args[0] + 3) & 63;
    }
}

void C_DECL A_TreeDeath(mobj_t* actor)
{
    assert(actor);
    if(!(actor->flags2 & MF2_FIREDAMAGE))
    {
        actor->height *= 2*2;
        actor->flags |= MF_SHOOTABLE;
        actor->flags &= ~(MF_CORPSE + MF_DROPOFF);
        actor->health = 35;
        return;
    }

    P_MobjChangeState(actor, P_GetState(actor->type, SN_MELEE));
}

void C_DECL A_NoGravity(mobj_t* actor)
{
    assert(actor);
    actor->flags |= MF_NOGRAVITY;
}
