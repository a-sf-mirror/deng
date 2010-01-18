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
 * p_things.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "gamemap.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean activateMobj(mobj_t* mo);
static boolean deactivateMobj(mobj_t* mo);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

static const mobjtype_t mapScriptThingIdToMobjTypeXlat[] = {
    MT_MAPSPOT,
    MT_CENTAUR,
    MT_CENTAURLEADER,
    MT_DEMON,
    MT_ETTIN,
    MT_FIREDEMON,
    MT_SERPENT,
    MT_SERPENTLEADER,
    MT_WRAITH,
    MT_WRAITHB,
    MT_FIREBALL1,
    MT_MANA1,
    MT_MANA2,
    MT_SPEEDBOOTS,
    MT_ARTIEGG,
    MT_ARTIFLY,
    MT_SUMMONMAULATOR,
    MT_TELEPORTOTHER,
    MT_ARTITELEPORT,
    MT_BISHOP,
    MT_ICEGUY,
    MT_BRIDGE,
    MT_BOOSTARMOR,
    MT_HEALINGBOTTLE,
    MT_HEALTHFLASK,
    MT_ARTISUPERHEAL,
    MT_BOOSTMANA,
    MT_FW_AXE,
    MT_FW_HAMMER,
    MT_FW_SWORD1,
    MT_FW_SWORD2,
    MT_FW_SWORD3,
    MT_CW_SERPSTAFF,
    MT_CW_HOLY1,
    MT_CW_HOLY2,
    MT_CW_HOLY3,
    MT_MW_CONE,
    MT_MW_STAFF1,
    MT_MW_STAFF2,
    MT_MW_STAFF3,
    MT_EGGFX,
    MT_ROCK1,
    MT_ROCK2,
    MT_ROCK3,
    MT_DIRT1,
    MT_DIRT2,
    MT_DIRT3,
    MT_DIRT4,
    MT_DIRT5,
    MT_DIRT6,
    MT_ARROW,
    MT_DART,
    MT_POISONDART,
    MT_RIPPERBALL,
    MT_SGSHARD1,
    MT_SGSHARD2,
    MT_SGSHARD3,
    MT_SGSHARD4,
    MT_SGSHARD5,
    MT_SGSHARD6,
    MT_SGSHARD7,
    MT_SGSHARD8,
    MT_SGSHARD9,
    MT_SGSHARD0,
    MT_PROJECTILE_BLADE,
    MT_ICESHARD,
    MT_FLAME_SMALL,
    MT_FLAME_LARGE,
    MT_ARMOR_1,
    MT_ARMOR_2,
    MT_ARMOR_3,
    MT_ARMOR_4,
    MT_ARTIPOISONBAG,
    MT_ARTITORCH,
    MT_BLASTRADIUS,
    MT_MANA3,
    MT_ARTIPUZZSKULL,
    MT_ARTIPUZZGEMBIG,
    MT_ARTIPUZZGEMRED,
    MT_ARTIPUZZGEMGREEN1,
    MT_ARTIPUZZGEMGREEN2,
    MT_ARTIPUZZGEMBLUE1,
    MT_ARTIPUZZGEMBLUE2,
    MT_ARTIPUZZBOOK1,
    MT_ARTIPUZZBOOK2,
    MT_KEY1,
    MT_KEY2,
    MT_KEY3,
    MT_KEY4,
    MT_KEY5,
    MT_KEY6,
    MT_KEY7,
    MT_KEY8,
    MT_KEY9,
    MT_KEYA,
    MT_WATER_DRIP,
    MT_FLAME_SMALL_TEMP,
    MT_FLAME_SMALL,
    MT_FLAME_LARGE_TEMP,
    MT_FLAME_LARGE,
    MT_DEMON_MASH,
    MT_DEMON2_MASH,
    MT_ETTIN_MASH,
    MT_CENTAUR_MASH,
    MT_THRUSTFLOOR_UP,
    MT_THRUSTFLOOR_DOWN,
    MT_WRAITHFX4,
    MT_WRAITHFX5,
    MT_WRAITHFX2
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

mobjtype_t P_MapScriptThingIdToMobjType(int thingId)
{
    assert(thingId >= 0 && thingId < sizeof(mapScriptThingIdToMobjTypeXlat) / sizeof(mapScriptThingIdToMobjTypeXlat[0]));
    return mapScriptThingIdToMobjTypeXlat[thingId];
}

boolean EV_ThingProjectile(map_t* map, byte* args, boolean gravity)
{
    assert(map);
    assert(args);
    {
    uint an;
    int tid, searcher;
    angle_t angle;
    float speed, vspeed;
    mobjtype_t moType;
    mobj_t* mobj, *newMobj;
    boolean success;

    success = false;
    searcher = -1;
    tid = args[0];
    moType = mapScriptThingIdToMobjTypeXlat[args[1]];
    if(noMonstersParm && (MOBJINFO[moType].flags & MF_COUNTKILL))
    {   // Don't spawn monsters if -nomonsters
        return false;
    }

    angle = (int) args[2] << 24;
    an = angle >> ANGLETOFINESHIFT;
    speed = FIX2FLT((int) args[3] << 13);
    vspeed = FIX2FLT((int) args[4] << 13);
    while((mobj = P_FindMobjFromTID(map, tid, &searcher)) != NULL)
    {
        if((newMobj = GameMap_SpawnMobj3fv(map, moType, mobj->pos, angle, 0)))
        {
            if(newMobj->info->seeSound)
                S_StartSound(newMobj->info->seeSound, newMobj);

            newMobj->target = mobj; // Originator
            newMobj->mom[MX] = speed * FIX2FLT(finecosine[an]);
            newMobj->mom[MY] = speed * FIX2FLT(finesine[an]);
            newMobj->mom[MZ] = vspeed;
            newMobj->flags2 |= MF2_DROPPED; // Don't respawn
            if(gravity == true)
            {
                newMobj->flags &= ~MF_NOGRAVITY;
                newMobj->flags2 |= MF2_LOGRAV;
            }

            if(P_CheckMissileSpawn(newMobj) == true)
            {
                success = true;
            }
        }
    }

    return success;
    }
}

boolean EV_ThingSpawn(map_t* map, byte* args, boolean fog)
{
    assert(map);
    assert(args);
    {
    int tid, searcher;
    angle_t angle;
    mobj_t* mobj, *newMobj, *fogMobj;
    mobjtype_t moType;
    boolean success;
    float z;

    success = false;
    searcher = -1;
    tid = args[0];
    moType = mapScriptThingIdToMobjTypeXlat[args[1]];
    if(noMonstersParm && (MOBJINFO[moType].flags & MF_COUNTKILL))
    {   // Don't spawn monsters if -nomonsters
        return false;
    }

    angle = (int) args[2] << 24;
    while((mobj = P_FindMobjFromTID(map, tid, &searcher)) != NULL)
    {
        z = mobj->pos[VZ];

        if((newMobj = GameMap_SpawnMobj3fv(map, moType, mobj->pos, angle, 0)))
        {
            if(P_TestMobjLocation(newMobj) == false)
            {   // Didn't fit
                P_MobjRemove(newMobj, true);
            }
            else
            {
                if(fog)
                {
                    if((fogMobj = GameMap_SpawnMobj3f(map, MT_TFOG,
                                                mobj->pos[VX], mobj->pos[VY],
                                                mobj->pos[VZ] + TELEFOGHEIGHT,
                                                angle + ANG180, 0)))
                        S_StartSound(SFX_TELEPORT, fogMobj);
                }

                newMobj->flags2 |= MF2_DROPPED; // Don't respawn
                if(newMobj->flags2 & MF2_FLOATBOB)
                {
                    newMobj->special1 =
                        FLT2FIX(newMobj->pos[VZ] - newMobj->floorZ);
                }

                success = true;
            }
        }
    }

    return success;
    }
}

boolean EV_ThingActivate(map_t* map, int tid)
{
    assert(map);
    {
    mobj_t* mobj;
    int searcher;
    boolean success;

    success = false;
    searcher = -1;
    while((mobj = P_FindMobjFromTID(map, tid, &searcher)) != NULL)
    {
        if(activateMobj(mobj) == true)
        {
            success = true;
        }
    }
    return success;
    }
}

boolean EV_ThingDeactivate(map_t* map, int tid)
{
    assert(map);
    {
    mobj_t* mobj;
    int searcher;
    boolean success;

    success = false;
    searcher = -1;
    while((mobj = P_FindMobjFromTID(map, tid, &searcher)) != NULL)
    {
        if(deactivateMobj(mobj) == true)
        {
            success = true;
        }
    }
    return success;
    }
}

boolean EV_ThingRemove(map_t* map, int tid)
{
    assert(map);
    {
    mobj_t* mobj;
    int searcher;
    boolean success;

    success = false;
    searcher = -1;
    while((mobj = P_FindMobjFromTID(map, tid, &searcher)) != NULL)
    {
        if(mobj->type == MT_BRIDGE)
        {
            A_BridgeRemove(mobj);
            return true;
        }
        P_MobjRemove(mobj, false);
        success = true;
    }
    return success;
    }
}

boolean EV_ThingDestroy(map_t* map, int tid)
{
    assert(map);
    {
    mobj_t* mobj;
    int searcher;
    boolean success;

    success = false;
    searcher = -1;
    while((mobj = P_FindMobjFromTID(map, tid, &searcher)) != NULL)
    {
        if(mobj->flags & MF_SHOOTABLE)
        {
            P_DamageMobj(mobj, NULL, NULL, 10000, false);
            success = true;
        }
    }
    return success;
    }
}

static boolean activateMobj(mobj_t* mo)
{
    if(mo->flags & MF_COUNTKILL)
    {                           // Monster
        if(mo->flags2 & MF2_DORMANT)
        {
            mo->flags2 &= ~MF2_DORMANT;
            mo->tics = 1;
            return true;
        }
        return false;
    }

    switch(mo->type)
    {
    case MT_ZTWINEDTORCH:
    case MT_ZTWINEDTORCH_UNLIT:
        P_MobjChangeState(mo, S_ZTWINEDTORCH_1);
        S_StartSound(SFX_IGNITE, mo);
        break;

    case MT_ZWALLTORCH:
    case MT_ZWALLTORCH_UNLIT:
        P_MobjChangeState(mo, S_ZWALLTORCH1);
        S_StartSound(SFX_IGNITE, mo);
        break;

    case MT_ZGEMPEDESTAL:
        P_MobjChangeState(mo, S_ZGEMPEDESTAL2);
        break;

    case MT_ZWINGEDSTATUENOSKULL:
        P_MobjChangeState(mo, S_ZWINGEDSTATUENOSKULL2);
        break;

    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
        if(mo->args[0] == 0)
        {
            S_StartSound(SFX_THRUSTSPIKE_LOWER, mo);
            mo->flags2 &= ~MF2_DONTDRAW;
            if(mo->args[1])
                P_MobjChangeState(mo, S_BTHRUSTRAISE1);
            else
                P_MobjChangeState(mo, S_THRUSTRAISE1);
        }
        break;

    case MT_ZFIREBULL:
    case MT_ZFIREBULL_UNLIT:
        P_MobjChangeState(mo, S_ZFIREBULL_BIRTH);
        S_StartSound(SFX_IGNITE, mo);
        break;

    case MT_ZBELL:
        if(mo->health > 0)
        {
            P_DamageMobj(mo, NULL, NULL, 10, false); // 'ring' the bell
        }
        break;

    case MT_ZCAULDRON:
    case MT_ZCAULDRON_UNLIT:
        P_MobjChangeState(mo, S_ZCAULDRON1);
        S_StartSound(SFX_IGNITE, mo);
        break;

    case MT_FLAME_SMALL:
        S_StartSound(SFX_IGNITE, mo);
        P_MobjChangeState(mo, S_FLAME_SMALL1);
        break;

    case MT_FLAME_LARGE:
        S_StartSound(SFX_IGNITE, mo);
        P_MobjChangeState(mo, S_FLAME_LARGE1);
        break;

    case MT_BAT_SPAWNER:
        P_MobjChangeState(mo, S_SPAWNBATS1);
        break;

    default:
        return false;
        break;
    }
    return true;
}

static boolean deactivateMobj(mobj_t* mo)
{
    if(mo->flags & MF_COUNTKILL)
    {                           // Monster
        if(!(mo->flags2 & MF2_DORMANT))
        {
            mo->flags2 |= MF2_DORMANT;
            mo->tics = -1;
            return true;
        }
        return false;
    }

    switch(mo->type)
    {
    case MT_ZTWINEDTORCH:
    case MT_ZTWINEDTORCH_UNLIT:
        P_MobjChangeState(mo, S_ZTWINEDTORCH_UNLIT);
        break;

    case MT_ZWALLTORCH:
    case MT_ZWALLTORCH_UNLIT:
        P_MobjChangeState(mo, S_ZWALLTORCH_U);
        break;

    case MT_THRUSTFLOOR_UP:
    case MT_THRUSTFLOOR_DOWN:
        if(mo->args[0] == 1)
        {
            S_StartSound(SFX_THRUSTSPIKE_RAISE, mo);
            if(mo->args[1])
                P_MobjChangeState(mo, S_BTHRUSTLOWER);
            else
                P_MobjChangeState(mo, S_THRUSTLOWER);
        }
        break;

    case MT_ZFIREBULL:
    case MT_ZFIREBULL_UNLIT:
        P_MobjChangeState(mo, S_ZFIREBULL_DEATH);
        break;

    case MT_ZCAULDRON:
    case MT_ZCAULDRON_UNLIT:
        P_MobjChangeState(mo, S_ZCAULDRON_U);
        break;

    case MT_FLAME_SMALL:
        P_MobjChangeState(mo, S_FLAME_SDORM1);
        break;

    case MT_FLAME_LARGE:
        P_MobjChangeState(mo, S_FLAME_LDORM1);
        break;

    case MT_BAT_SPAWNER:
        P_MobjChangeState(mo, S_SPAWNBATS_OFF);
        break;

    default:
        return false;
        break;
    }
    return true;
}
