/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/*
 * Handling interactions (i.e., collisions).
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "am_map.h"
#include "d_net.h"
#include "dmu_lib.h"
#include "p_player.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

#define BONUSADD 6

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int playerkeys;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     maxammo[NUM_AMMO_TYPES] = {
    100,                        // gold wand
    50,                         // crossbow
    200,                        // blaster
    200,                        // skull rod
    20,                         // phoenix rod
    150                         // mace
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int GetWeaponAmmo[NUM_WEAPON_TYPES] = {
    0,                          // staff
    25,                         // gold wand
    10,                         // crossbow
    30,                         // blaster
    50,                         // skull rod
    2,                          // phoenix rod
    50,                         // mace
    0                           // gauntlets
};

// CODE --------------------------------------------------------------------

/*
 * Returns true if the player accepted the ammo, false if it was
 * refused (player has maxammo[ammo]).
 */
boolean P_GiveAmmo(player_t *player, ammotype_t ammo, int num)
{
    if(ammo == AT_NOAMMO)
        return false;

    if(ammo < 0 || ammo > NUM_AMMO_TYPES)
        Con_Error("P_GiveAmmo: bad type %i", ammo);

    if(player->ammo[ammo] == player->maxammo[ammo])
        return false;

    if(gameskill == SM_BABY || gameskill == SM_NIGHTMARE)
    {   // extra ammo in baby mode and nightmare mode
        num += num >> 1;
    }

    // We are about to receive some more ammo. Does the player want to
    // change weapon automatically?
    P_MaybeChangeWeapon(player, WT_NOCHANGE, ammo, false);

    player->ammo[ammo] += num;
    player->update |= PSF_AMMO;

    if(player->ammo[ammo] > player->maxammo[ammo])
        player->ammo[ammo] = player->maxammo[ammo];

    // Maybe unhide the HUD?
    if(player == &players[consoleplayer])
        ST_HUDUnHide(HUE_ON_PICKUP_AMMO);

    return true;
}

/*
 * Returns true if the weapon or its ammo was accepted.
 */
boolean P_GiveWeapon(player_t *player, weapontype_t weapon)
{
    boolean gaveammo = false;
    boolean gaveweapon = false;
    int i;
    int lvl = (player->powers[PT_WEAPONLEVEL2]? 1 : 0);

    if(IS_NETGAME && !deathmatch)
    {
        // leave placed weapons forever on net games
        if(player->weaponowned[weapon])
            return false;

        player->bonuscount += BONUSADD;
        player->weaponowned[weapon] = true;
        player->update |= PSF_OWNED_WEAPONS;

        // Give some of each of the ammo types used by this weapon.
        for(i=0; i < NUM_AMMO_TYPES; ++i)
        {
            if(!weaponinfo[weapon][player->class].mode[lvl].ammotype[i])
                continue;   // Weapon does not take this type of ammo.

            if(P_GiveAmmo(player, i,GetWeaponAmmo[weapon]))
                gaveammo = true; // at least ONE type of ammo was given.
        }

        // Should we change weapon automatically?
        P_MaybeChangeWeapon(player, weapon, AT_NOAMMO, false);

        // Maybe unhide the HUD?
        if(player == &players[consoleplayer])
            ST_HUDUnHide(HUE_ON_PICKUP_WEAPON);

        S_ConsoleSound(sfx_wpnup, NULL, player - players);
        return false;
    }
    else
    {
        // Give some of each of the ammo types used by this weapon.
        for(i=0; i < NUM_AMMO_TYPES; ++i)
        {
            if(!weaponinfo[weapon][player->class].mode[lvl].ammotype[i])
                continue;   // Weapon does not take this type of ammo.

            if(P_GiveAmmo(player, i, GetWeaponAmmo[weapon]))
                gaveammo = true; // at least ONE type of ammo was given.
        }

        if(player->weaponowned[weapon])
            gaveweapon = false;
        else
        {
            gaveweapon = true;
            player->weaponowned[weapon] = true;
            player->update |= PSF_OWNED_WEAPONS;

            // Should we change weapon automatically?
            P_MaybeChangeWeapon(player, weapon, AT_NOAMMO, false);
        }

        // Maybe unhide the HUD?
        if(gaveweapon && player == &players[consoleplayer])
            ST_HUDUnHide(HUE_ON_PICKUP_WEAPON);

        return (gaveweapon || gaveammo);
    }
}

/*
 * Returns false if the body isn't needed at all.
 */
boolean P_GiveBody(player_t *player, int num)
{
    int     max;

    max = MAXHEALTH;
    if(player->morphTics)
    {
        max = MAXCHICKENHEALTH;
    }
    if(player->health >= max)
    {
        return (false);
    }
    player->health += num;
    if(player->health > max)
    {
        player->health = max;
    }
    player->update |= PSF_HEALTH;
    player->plr->mo->health = player->health;

    // Maybe unhide the HUD?
    if(player == &players[consoleplayer])
        ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);

    return true;
}

/*
 * Returns false if the armor is worse than the current armor.
 */
boolean P_GiveArmor(player_t *player, int armortype)
{
    int     hits;

    hits = armortype * 100;
    if(player->armorpoints >= hits)
        return false;

    player->armortype = armortype;
    player->armorpoints = hits;
    player->update |= PSF_ARMOR_TYPE | PSF_ARMOR_POINTS;

    // Maybe unhide the HUD?
    if(player == &players[consoleplayer])
        ST_HUDUnHide(HUE_ON_PICKUP_ARMOR);

    return true;
}

void P_GiveKey(player_t *player, keytype_t key)
{
    if(player->keys[key])
        return;

    if(player == &players[consoleplayer])
    {
        playerkeys |= 1 << key;
    }

    player->bonuscount = BONUSADD;
    player->keys[key] = true;
    player->update |= PSF_KEYS;

    // Maybe unhide the HUD?
    if(player == &players[consoleplayer])
        ST_HUDUnHide(HUE_ON_PICKUP_KEY);
}

/*
 * Returns true if power accepted.
 */
boolean P_GivePower(player_t *player, powertype_t power)
{
    mobj_t     *plrmo = player->plr->mo;
    boolean     retval = false;

    player->update |= PSF_POWERS;
    switch(power)
    {
    case PT_INVULNERABILITY:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = INVULNTICS;
            retval = true;
        }
        break;

    case PT_WEAPONLEVEL2:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = WPNLEV2TICS;
            retval = true;
        }
        break;

    case PT_INVISIBILITY:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = INVISTICS;
            plrmo->flags |= MF_SHADOW;
            retval = true;
        }
        break;

    case PT_FLIGHT:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = FLIGHTTICS;
            plrmo->flags2 |= MF2_FLY;
            plrmo->flags |= MF_NOGRAVITY;
            if(plrmo->pos[VZ] <= FLT2FIX(plrmo->floorz))
            {
                player->flyheight = 10; // thrust the player in the air a bit
                player->plr->flags |= DDPF_FIXMOM;
            }
            retval = true;
        }
        break;

    case PT_INFRARED:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = INFRATICS;
            retval = true;
        }
        break;

    default:
        if(!player->powers[power])
        {
            player->powers[power] = 1;
            retval = true;
        }
        break;
    }

    if(retval)
    {
        // Maybe unhide the HUD?
        if(player == &players[consoleplayer])
            ST_HUDUnHide(HUE_ON_PICKUP_POWER);
    }
    return retval;
}

/*
 * Removes the MF_SPECIAL flag, and initiates the artifact pickup
 * animation.
 */
void P_SetDormantArtifact(mobj_t *arti)
{
    arti->flags &= ~MF_SPECIAL;
    if(deathmatch && (arti->type != MT_ARTIINVULNERABILITY) &&
       (arti->type != MT_ARTIINVISIBILITY))
    {
        P_SetMobjState(arti, S_DORMANTARTI1);
    }
    else
    {                           // Don't respawn
        P_SetMobjState(arti, S_DEADARTI1);
    }
    S_StartSound(sfx_artiup, arti);
}

void C_DECL A_RestoreArtifact(mobj_t *arti)
{
    arti->flags |= MF_SPECIAL;
    P_SetMobjState(arti, arti->info->spawnstate);
    S_StartSound(sfx_respawn, arti);
}

void P_HideSpecialThing(mobj_t *thing)
{
    thing->flags &= ~MF_SPECIAL;
    thing->flags2 |= MF2_DONTDRAW;
    P_SetMobjState(thing, S_HIDESPECIAL1);
}

/*
 * Make a special thing visible again.
 */
void C_DECL A_RestoreSpecialThing1(mobj_t *thing)
{
    if(thing->type == MT_WMACE)
    {                           // Do random mace placement
        P_RepositionMace(thing);
    }
    thing->flags2 &= ~MF2_DONTDRAW;
    S_StartSound(sfx_respawn, thing);
}

void C_DECL A_RestoreSpecialThing2(mobj_t *thing)
{
    thing->flags |= MF_SPECIAL;
    P_SetMobjState(thing, thing->info->spawnstate);
}

void P_TouchSpecialThing(mobj_t *special, mobj_t *toucher)
{
    int     i;
    player_t *player;
    fixed_t delta;
    int     sound;
    boolean respawn;

    delta = special->pos[VZ] - toucher->pos[VZ];
    if(delta > FLT2FIX(toucher->height) || delta < -32 * FRACUNIT)
    {                           // Out of reach
        return;
    }
    if(toucher->health <= 0)
    {                           // Toucher is dead
        return;
    }
    sound = sfx_itemup;
    player = toucher->player;
    if(player == NULL)
        return;
    respawn = true;
    switch (special->sprite)
    {
        // Items
    case SPR_PTN1:              // Item_HealingPotion
        if(!P_GiveBody(player, 10))
        {
            return;
        }
        P_SetMessage(player, TXT_ITEMHEALTH, false);
        break;
    case SPR_SHLD:              // Item_Shield1
        if(!P_GiveArmor(player, 1))
        {
            return;
        }
        P_SetMessage(player, TXT_ITEMSHIELD1, false);
        break;
    case SPR_SHD2:              // Item_Shield2
        if(!P_GiveArmor(player, 2))
        {
            return;
        }
        P_SetMessage(player, TXT_ITEMSHIELD2, false);
        break;
    case SPR_BAGH:              // Item_BagOfHolding
        if(!player->backpack)
        {
            for(i = 0; i < NUM_AMMO_TYPES; i++)
            {
                player->maxammo[i] *= 2;
            }
            player->backpack = true;
        }
        P_GiveAmmo(player, AT_CRYSTAL, AMMO_GWND_WIMPY);
        P_GiveAmmo(player, AT_ORB, AMMO_BLSR_WIMPY);
        P_GiveAmmo(player, AT_ARROW, AMMO_CBOW_WIMPY);
        P_GiveAmmo(player, AT_RUNE, AMMO_SKRD_WIMPY);
        P_GiveAmmo(player, AT_FIREORB, AMMO_PHRD_WIMPY);
        P_SetMessage(player, TXT_ITEMBAGOFHOLDING, false);
        break;
    case SPR_SPMP:              // Item_SuperMap
        if(!P_GivePower(player, PT_ALLMAP))
        {
            return;
        }
        P_SetMessage(player, TXT_ITEMSUPERMAP, false);
        break;

        // Keys
    case SPR_BKYY:              // Key_Blue
        if(!player->keys[KT_BLUE])
        {
            P_SetMessage(player, TXT_GOTBLUEKEY, false);
        }
        P_GiveKey(player, KT_BLUE);
        sound = sfx_keyup;
        if(!IS_NETGAME)
        {
            break;
        }
        return;
    case SPR_CKYY:              // Key_Yellow
        if(!player->keys[KT_YELLOW])
        {
            P_SetMessage(player, TXT_GOTYELLOWKEY, false);
        }
        sound = sfx_keyup;
        P_GiveKey(player, KT_YELLOW);
        if(!IS_NETGAME)
        {
            break;
        }
        return;
    case SPR_AKYY:              // Key_Green
        if(!player->keys[KT_GREEN])
        {
            P_SetMessage(player, TXT_GOTGREENKEY, false);
        }
        sound = sfx_keyup;
        P_GiveKey(player, KT_GREEN);
        if(!IS_NETGAME)
        {
            break;
        }
        return;

        // Artifacts
    case SPR_PTN2:              // Arti_HealingPotion
        if(P_GiveArtifact(player, arti_health, special))
        {
            P_SetMessage(player, TXT_ARTIHEALTH, false);
            P_SetDormantArtifact(special);
        }
        return;
    case SPR_SOAR:              // Arti_Fly
        if(P_GiveArtifact(player, arti_fly, special))
        {
            P_SetMessage(player, TXT_ARTIFLY, false);
            P_SetDormantArtifact(special);
        }
        return;
    case SPR_INVU:              // Arti_Invulnerability
        if(P_GiveArtifact(player, arti_invulnerability, special))
        {
            P_SetMessage(player, TXT_ARTIINVULNERABILITY, false);
            P_SetDormantArtifact(special);
        }
        return;
    case SPR_PWBK:              // Arti_TomeOfPower
        if(P_GiveArtifact(player, arti_tomeofpower, special))
        {
            P_SetMessage(player, TXT_ARTITOMEOFPOWER, false);
            P_SetDormantArtifact(special);
        }
        return;
    case SPR_INVS:              // Arti_Invisibility
        if(P_GiveArtifact(player, arti_invisibility, special))
        {
            P_SetMessage(player, TXT_ARTIINVISIBILITY, false);
            P_SetDormantArtifact(special);
        }
        return;
    case SPR_EGGC:              // Arti_Egg
        if(P_GiveArtifact(player, arti_egg, special))
        {
            P_SetMessage(player, TXT_ARTIEGG, false);
            P_SetDormantArtifact(special);
        }
        return;
    case SPR_SPHL:              // Arti_SuperHealth
        if(P_GiveArtifact(player, arti_superhealth, special))
        {
            P_SetMessage(player, TXT_ARTISUPERHEALTH, false);
            P_SetDormantArtifact(special);
        }
        return;
    case SPR_TRCH:              // Arti_Torch
        if(P_GiveArtifact(player, arti_torch, special))
        {
            P_SetMessage(player, TXT_ARTITORCH, false);
            P_SetDormantArtifact(special);
        }
        return;
    case SPR_FBMB:              // Arti_FireBomb
        if(P_GiveArtifact(player, arti_firebomb, special))
        {
            P_SetMessage(player, TXT_ARTIFIREBOMB, false);
            P_SetDormantArtifact(special);
        }
        return;
    case SPR_ATLP:              // Arti_Teleport
        if(P_GiveArtifact(player, arti_teleport, special))
        {
            P_SetMessage(player, TXT_ARTITELEPORT, false);
            P_SetDormantArtifact(special);
        }
        return;

        // Ammo
    case SPR_AMG1:              // Ammo_GoldWandWimpy
        if(!P_GiveAmmo(player, AT_CRYSTAL, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOGOLDWAND1, false);
        break;
    case SPR_AMG2:              // Ammo_GoldWandHefty
        if(!P_GiveAmmo(player, AT_CRYSTAL, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOGOLDWAND2, false);
        break;
    case SPR_AMM1:              // Ammo_MaceWimpy
        if(!P_GiveAmmo(player, AT_MSPHERE, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOMACE1, false);
        break;
    case SPR_AMM2:              // Ammo_MaceHefty
        if(!P_GiveAmmo(player, AT_MSPHERE, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOMACE2, false);
        break;
    case SPR_AMC1:              // Ammo_CrossbowWimpy
        if(!P_GiveAmmo(player, AT_ARROW, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOCROSSBOW1, false);
        break;
    case SPR_AMC2:              // Ammo_CrossbowHefty
        if(!P_GiveAmmo(player, AT_ARROW, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOCROSSBOW2, false);
        break;
    case SPR_AMB1:              // Ammo_BlasterWimpy
        if(!P_GiveAmmo(player, AT_ORB, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOBLASTER1, false);
        break;
    case SPR_AMB2:              // Ammo_BlasterHefty
        if(!P_GiveAmmo(player, AT_ORB, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOBLASTER2, false);
        break;
    case SPR_AMS1:              // Ammo_SkullRodWimpy
        if(!P_GiveAmmo(player, AT_RUNE, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOSKULLROD1, false);
        break;
    case SPR_AMS2:              // Ammo_SkullRodHefty
        if(!P_GiveAmmo(player, AT_RUNE, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOSKULLROD2, false);
        break;
    case SPR_AMP1:              // Ammo_PhoenixRodWimpy
        if(!P_GiveAmmo(player, AT_FIREORB, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOPHOENIXROD1, false);
        break;
    case SPR_AMP2:              // Ammo_PhoenixRodHefty
        if(!P_GiveAmmo(player, AT_FIREORB, special->health))
        {
            return;
        }
        P_SetMessage(player, TXT_AMMOPHOENIXROD2, false);
        break;

        // Weapons
    case SPR_WMCE:              // Weapon_Mace
        if(!P_GiveWeapon(player, WT_SEVENTH))
        {
            return;
        }
        P_SetMessage(player, TXT_WPNMACE, false);
        sound = sfx_wpnup;
        break;
    case SPR_WBOW:              // Weapon_Crossbow
        if(!P_GiveWeapon(player, WT_THIRD))
        {
            return;
        }
        P_SetMessage(player, TXT_WPNCROSSBOW, false);
        sound = sfx_wpnup;
        break;
    case SPR_WBLS:              // Weapon_Blaster
        if(!P_GiveWeapon(player, WT_FOURTH))
        {
            return;
        }
        P_SetMessage(player, TXT_WPNBLASTER, false);
        sound = sfx_wpnup;
        break;
    case SPR_WSKL:              // Weapon_SkullRod
        if(!P_GiveWeapon(player, WT_FIFTH))
        {
            return;
        }
        P_SetMessage(player, TXT_WPNSKULLROD, false);
        sound = sfx_wpnup;
        break;
    case SPR_WPHX:              // Weapon_PhoenixRod
        if(!P_GiveWeapon(player, WT_SIXTH))
        {
            return;
        }
        P_SetMessage(player, TXT_WPNPHOENIXROD, false);
        sound = sfx_wpnup;
        break;
    case SPR_WGNT:              // Weapon_Gauntlets
        if(!P_GiveWeapon(player, WT_EIGHTH))
        {
            return;
        }
        P_SetMessage(player, TXT_WPNGAUNTLETS, false);
        sound = sfx_wpnup;
        break;
    default:
        Con_Error("P_SpecialThing: Unknown gettable thing");
    }
    if(special->flags & MF_COUNTITEM)
    {
        player->itemcount++;
    }
    if(deathmatch && respawn && !(special->flags & MF_DROPPED))
    {
        P_HideSpecialThing(special);
    }
    else
    {
        P_RemoveMobj(special);
    }

    player->bonuscount += BONUSADD;

    if(player == &players[consoleplayer])
        ST_doPaletteStuff();

    S_ConsoleSound(sound, NULL, player - players);
}

void P_KillMobj(mobj_t *source, mobj_t *target)
{
    if(!target) // nothing to kill
        return;

    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY | MF_NOGRAVITY);
    target->flags |= MF_CORPSE | MF_DROPOFF;
    target->flags2 &= ~MF2_PASSMOBJ;
    target->corpsetics = 0;
    target->height /= 2*2;
    if(source && source->player)
    {
        if(target->flags & MF_COUNTKILL)
        {                       // Count for intermission
            source->player->killcount++;
        }
        if(target->player)
        {
            // Frag stuff
            source->player->update |= PSF_FRAGS;
            if(target == source)
            {                   // Self-frag
                target->player->frags[target->player - players]--;
                NetSv_FragsForAll(target->player);
            }
            else
            {
                source->player->frags[target->player - players]++;
                NetSv_FragsForAll(source->player);

                if(source->player->morphTics)
                {               // Make a super chicken
                    P_GivePower(source->player, PT_WEAPONLEVEL2);
                }
            }
        }
    }
    else if(!IS_NETGAME && (target->flags & MF_COUNTKILL))
    {                           // Count all monster deaths
        players[0].killcount++;
    }
    if(target->player)
    {
        if(!source)
        {                       // Self-frag
            target->player->frags[target->player - players]--;
            NetSv_FragsForAll(target->player);
        }
        target->flags &= ~MF_SOLID;
        target->flags2 &= ~MF2_FLY;
        target->player->powers[PT_FLIGHT] = 0;
        target->player->powers[PT_WEAPONLEVEL2] = 0;
        target->player->playerstate = PST_DEAD;
        target->player->plr->flags |= DDPF_DEAD;
        target->player->update |= PSF_STATE;
        P_DropWeapon(target->player);

        if(target->flags2 & MF2_FIREDAMAGE)
        {                       // Player flame death
            P_SetMobjState(target, S_PLAY_FDTH1);
            return;
        }

        // Don't die in auto map.
        AM_Stop(target->player - players);
    }

    if(target->health < -(target->info->spawnhealth >> 1) &&
       target->info->xdeathstate)
    {                           // Extreme death
        P_SetMobjState(target, target->info->xdeathstate);
    }
    else
    {                           // Normal death
        P_SetMobjState(target, target->info->deathstate);
    }
    target->tics -= P_Random() & 3;

}

void P_MinotaurSlam(mobj_t *source, mobj_t *target)
{
    angle_t angle;
    fixed_t thrust;

    angle = R_PointToAngle2(source->pos[VX], source->pos[VY],
                            target->pos[VX], target->pos[VY]);
    angle >>= ANGLETOFINESHIFT;
    thrust = 16 * FRACUNIT + (P_Random() << 10);
    target->mom[MX] += FixedMul(thrust, finecosine[angle]);
    target->mom[MY] += FixedMul(thrust, finesine[angle]);
    P_DamageMobj(target, NULL, NULL, HITDICE(6));
    if(target->player)
    {
        target->reactiontime = 14 + (P_Random() & 7);
    }
}

void P_TouchWhirlwind(mobj_t *target)
{
    int     randVal;

    target->angle += (P_Random() - P_Random()) << 20;
    target->mom[MX] += (P_Random() - P_Random()) << 10;
    target->mom[MY] += (P_Random() - P_Random()) << 10;
    if(leveltime & 16 && !(target->flags2 & MF2_BOSS))
    {
        randVal = P_Random();
        if(randVal > 160)
        {
            randVal = 160;
        }
        target->mom[MZ] += randVal << 10;
        if(target->mom[MZ] > 12 * FRACUNIT)
        {
            target->mom[MZ] = 12 * FRACUNIT;
        }
    }
    if(!(leveltime & 7))
    {
        P_DamageMobj(target, NULL, NULL, 3);
    }
}

/*
 * Returns true if the player gets turned into a chicken.
 */
boolean P_MorphPlayer(player_t *player)
{
    mobj_t *pmo;
    mobj_t *fog;
    mobj_t *chicken;
    fixed_t pos[3];
    angle_t angle;
    int     oldFlags2;

    if(player->morphTics)
    {
        if((player->morphTics < CHICKENTICS - TICSPERSEC) &&
           !player->powers[PT_WEAPONLEVEL2])
        {                       // Make a super chicken
            P_GivePower(player, PT_WEAPONLEVEL2);
        }
        return false;
    }

    if(player->powers[PT_INVULNERABILITY])
    {                           // Immune when invulnerable
        return false;
    }

    pmo = player->plr->mo;
    memcpy(pos, pmo->pos, sizeof(pos));
    angle = pmo->angle;
    oldFlags2 = pmo->flags2;
    P_SetMobjState(pmo, S_FREETARGMOBJ);

    fog = P_SpawnMobj(pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT, MT_TFOG);
    S_StartSound(sfx_telept, fog);

    chicken = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_CHICPLAYER);
    chicken->special1 = player->readyweapon;
    chicken->angle = angle;
    chicken->player = player;
    chicken->dplayer = player->plr;

    player->health = chicken->health = MAXCHICKENHEALTH;
    player->plr->mo = chicken;
    player->armorpoints = player->armortype = 0;
    player->powers[PT_INVISIBILITY] = 0;
    player->powers[PT_WEAPONLEVEL2] = 0;

    if(oldFlags2 & MF2_FLY)
        chicken->flags2 |= MF2_FLY;

    player->morphTics = CHICKENTICS;
    player->plr->flags |= DDPF_FIXPOS | DDPF_FIXMOM;
    player->update |=
        PSF_MORPH_TIME | PSF_HEALTH | PSF_POWERS | PSF_ARMOR_POINTS;

    P_ActivateMorphWeapon(player);
    return true;
}

boolean P_MorphMonster(mobj_t *actor)
{
    mobj_t *fog;
    mobj_t *chicken;
    mobj_t *target;
    mobjtype_t moType;
    fixed_t pos[3];
    angle_t angle;
    int     ghost;

    if(actor->player)
        return false;

    moType = actor->type;
    switch (moType)
    {
    case MT_POD:
    case MT_CHICKEN:
    case MT_HEAD:
    case MT_MINOTAUR:
    case MT_SORCERER1:
    case MT_SORCERER2:
        return false;

    default:
        break;
    }

    memcpy(pos, actor->pos, sizeof(pos));
    angle = actor->angle;
    ghost = actor->flags & MF_SHADOW;
    target = actor->target;
    P_SetMobjState(actor, S_FREETARGMOBJ);

    fog = P_SpawnMobj(pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT, MT_TFOG);
    S_StartSound(sfx_telept, fog);

    chicken = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_CHICKEN);
    chicken->special2 = moType;
    chicken->special1 = CHICKENTICS + P_Random();
    chicken->flags |= ghost;
    chicken->target = target;
    chicken->angle = angle;
    return true;
}

boolean P_AutoUseChaosDevice(player_t *player)
{
    int     i;

    for(i = 0; i < player->inventorySlotNum; i++)
    {
        if(player->inventory[i].type == arti_teleport)
        {
            P_InventoryUseArtifact(player, arti_teleport);
            player->health = player->plr->mo->health =
                (player->health + 1) / 2;
            return (true);
        }
    }
    return (false);
}

void P_AutoUseHealth(player_t *player, int saveHealth)
{
    int     i;
    int     count;
    int     normalCount = 0;
    int     normalSlot = 0;
    int     superCount = 0;
    int     superSlot = 0;

    for(i = 0; i < player->inventorySlotNum; i++)
    {
        if(player->inventory[i].type == arti_health)
        {
            normalSlot = i;
            normalCount = player->inventory[i].count;
        }
        else if(player->inventory[i].type == arti_superhealth)
        {
            superSlot = i;
            superCount = player->inventory[i].count;
        }
    }

    if((gameskill == SM_BABY) && (normalCount * 25 >= saveHealth))
    {
        // Use quartz flasks
        count = (saveHealth + 24) / 25;
        for(i = 0; i < count; i++)
        {
            player->health += 25;
            P_InventoryRemoveArtifact(player, normalSlot);
        }
    }
    else if(superCount * 100 >= saveHealth)
    {
        // Use mystic urns
        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; i++)
        {
            player->health += 100;
            P_InventoryRemoveArtifact(player, superSlot);
        }
    }
    else if((gameskill == SM_BABY) &&
            (superCount * 100 + normalCount * 25 >= saveHealth))
    {
        // Use mystic urns and quartz flasks
        count = (saveHealth + 24) / 25;
        saveHealth -= count * 25;
        for(i = 0; i < count; i++)
        {
            player->health += 25;
            P_InventoryRemoveArtifact(player, normalSlot);
        }
        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; i++)
        {
            player->health += 100;
            P_InventoryRemoveArtifact(player, normalSlot);
        }
    }
    player->plr->mo->health = player->health;
}

void P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                  int damage)
{
    P_DamageMobj2(target, inflictor, source, damage, false);
}

/*
 * Damages both enemies and players
 * inflictor is the thing that caused the damage
 *        creature or missile, can be NULL (slime, etc)
 * source is the thing to target after taking damage
 *        creature or NULL
 * Source and inflictor are the same for melee attacks
 * source can be null for barrel explosions and other environmental stuff
 */
void P_DamageMobj2(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                  int damage, boolean stomping)
{
    unsigned ang;
    int     saved;
    player_t *player;
    fixed_t thrust;
    int     temp;

    // Clients are not able to damage anything.
    if(IS_CLIENT)
        return;

    if(!(target->flags & MF_SHOOTABLE)) // Shouldn't happen
        return;

    if(target->health <= 0)
        return;

    if(target->flags & MF_SKULLFLY)
    {
        if(target->type == MT_MINOTAUR)
        {                       // Minotaur is invulnerable during charge attack
            return;
        }
        target->mom[MX] = target->mom[MY] = target->mom[MZ] = 0;
    }

    player = target->player;

    // In trainer mode? then take half damage
    if(player && gameskill == SM_BABY)
        damage >>= 1;

    // Special damage types
    if(inflictor)
    {
        switch (inflictor->type)
        {
        case MT_EGGFX:
            if(player)
            {
                P_MorphPlayer(player);
            }
            else
            {
                P_MorphMonster(target);
            }
            return;             // Always return

        case MT_WHIRLWIND:
            P_TouchWhirlwind(target);
            return;

        case MT_MINOTAUR:
            if(inflictor->flags & MF_SKULLFLY)
            {
                // Slam only when in charge mode
                P_MinotaurSlam(inflictor, target);
                return;
            }
            break;

        case MT_MACEFX4:
            // Death ball
            if((target->flags2 & MF2_BOSS) || target->type == MT_HEAD)
            {
                // Don't allow cheap boss kills
                break;
            }
            else if(target->player)
            {
                // Player specific checks

                // Is player invulnerable?
                if(target->player->powers[PT_INVULNERABILITY])
                    break;

                // Does the player have a Chaos Device he can use
                // to get him out of trouble?
                if(P_AutoUseChaosDevice(target->player))
                    return; // He's lucky... this time
            }

            // Something's gonna die
            damage = 10000;
            break;

        case MT_PHOENIXFX2:
            // Flame thrower
            if(target->player && P_Random() < 128)
            {
                // Freeze player for a bit
                target->reactiontime += 4;
            }
            break;

        case MT_RAINPLR1:
        case MT_RAINPLR2:
        case MT_RAINPLR3:
        case MT_RAINPLR4:
            // Rain missiles
            if(target->flags2 & MF2_BOSS)
            {
                // Decrease damage for bosses
                damage = (P_Random() & 7) + 1;
            }
            break;

        case MT_HORNRODFX2:
        case MT_PHOENIXFX1:
            if(target->type == MT_SORCERER2 && P_Random() < 96)
            {
                // D'Sparil teleports away
                P_DSparilTeleport(target);
                return;
            }
            break;

        case MT_BLASTERFX1:
        case MT_RIPPER:
            if(target->type == MT_HEAD)
            {
                // Less damage to Ironlich bosses
                damage = P_Random() & 1;
                if(!damage)
                    return;
            }
            break;

        default:
            break;
        }
    }

    // Push the target unless source is using the gauntlets
    if(inflictor && !(target->flags & MF_NOCLIP) &&
       (!source || !source->player ||
        source->player->readyweapon != WT_EIGHTH) &&
       !(inflictor->flags2 & MF2_NODMGTHRUST))
    {
        ang = R_PointToAngle2(inflictor->pos[VX], inflictor->pos[VY],
                              target->pos[VX], target->pos[VY]);

        thrust = damage * (FRACUNIT >> 3) * 150 / target->info->mass;

        // make fall forwards sometimes
        if((damage < 40) && (damage > target->health) &&
           (target->pos[VZ] - inflictor->pos[VZ] > 64 * FRACUNIT) && (P_Random() & 1))
        {
            ang += ANG180;
            thrust *= 4;
        }

        ang >>= ANGLETOFINESHIFT;

        if(source && source->player && (source == inflictor) &&
           source->player->powers[PT_WEAPONLEVEL2] &&
           source->player->readyweapon == WT_FIRST)
        {
            // Staff power level 2
            target->mom[MX] += FixedMul(10 * FRACUNIT, finecosine[ang]);
            target->mom[MY] += FixedMul(10 * FRACUNIT, finesine[ang]);
            if(!(target->flags & MF_NOGRAVITY))
            {
                target->mom[MZ] += 5 * FRACUNIT;
            }
        }
        else
        {
            target->mom[MX] += FixedMul(thrust, finecosine[ang]);
            target->mom[MY] += FixedMul(thrust, finesine[ang]);
        }

        if(target->dplayer)
        {
            // Only fix momentum. Otherwise clients will find it difficult
            // to escape from the damage inflictor.
            target->dplayer->flags |= DDPF_FIXMOM;
        }

        // killough $dropoff_fix: thrust objects hanging off ledges
        if(target->intflags & MIF_FALLING && target->gear >= MAXGEAR)
            target->gear = 0;
    }

    // player specific
    if(player)
    {
        if(damage < 1000 &&
           ((P_GetPlayerCheats(player) & CF_GODMODE) ||
            player->powers[PT_INVULNERABILITY]))
        {
            return;
        }

        if(player->armortype)
        {
            if(player->armortype == 1)
            {
                saved = damage >> 1;
            }
            else
            {
                saved = (damage >> 1) + (damage >> 2);
            }
            if(player->armorpoints <= saved)
            {
                // armor is used up
                saved = player->armorpoints;
                player->armortype = 0;
            }
            player->armorpoints -= saved;
            damage -= saved;
            player->update |= PSF_ARMOR_POINTS;
        }

        if(damage >= player->health && ((gameskill == SM_BABY) || deathmatch)
           && !player->morphTics)
        {                       // Try to use some inventory health
            P_AutoUseHealth(player, damage - player->health + 1);
        }

        player->health -= damage;   // mirror mobj health here for Dave
        if(player->health < 0)
        {
            player->health = 0;
        }

        player->update |= PSF_HEALTH;
        player->attacker = source;

        player->damagecount += damage;  // add damage after armor / invuln
        if(player->damagecount > 100)
        {
            player->damagecount = 100;  // teleport stomp does 10k points...
        }
        temp = damage < 100 ? damage : 100;

        // Maybe unhide the HUD?
        if(player == &players[consoleplayer]);
            ST_HUDUnHide(HUE_ON_DAMAGE);
    }

    // How about some particles, yes?
    // Only works when both target and inflictor are real mobjs.
    P_SpawnDamageParticleGen(target, inflictor, damage);

    // do the damage
    target->health -= damage;
    if(target->health <= 0)
    {
        // Death
        target->special1 = damage;
        if(target->type == MT_POD && source && source->type != MT_POD)
        {
            // Make sure players get frags for chain-reaction kills
            target->target = source;
        }

        if(player && inflictor && !player->morphTics)
        {
            // Check for flame death
            if((inflictor->flags2 & MF2_FIREDAMAGE) ||
               ((inflictor->type == MT_PHOENIXFX1) && (target->health > -50) &&
                (damage > 25)))
            {
                target->flags2 |= MF2_FIREDAMAGE;
            }
        }

        P_KillMobj(source, target);
        return;
    }

    if((P_Random() < target->info->painchance) &&
       !(target->flags & MF_SKULLFLY))
    {
        // fight back!
        target->flags |= MF_JUSTHIT;
        P_SetMobjState(target, target->info->painstate);
    }

    // we're awake now...
    target->reactiontime = 0;
    if(!target->threshold && source && !(source->flags3 & MF3_NOINFIGHT) &&
       !(target->type == MT_SORCERER2 && source->type == MT_WIZARD))
    {
        // Target actor is not intent on another actor,
        // so make him chase after source
        target->target = source;
        target->threshold = BASETHRESHOLD;
        if(target->state == &states[target->info->spawnstate] &&
           target->info->seestate != S_NULL)
        {
            P_SetMobjState(target, target->info->seestate);
        }
    }
}
