/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_inter.c: Handling mobj vs mobj interactions (i.e., collisions).
 */

#ifdef MSVC
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "gamemap.h"
#include "am_map.h"
#include "d_net.h"
#include "dmu_lib.h"
#include "p_player.h"
#include "p_user.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#define BONUSADD            (6)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// A weapon is found with two clip loads, a big item has five clip loads.
int maxAmmo[NUM_AMMO_TYPES] = {200, 50, 300, 50};
int clipAmmo[NUM_AMMO_TYPES] = {10, 4, 20, 1};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * @param num           Number of clip loads, not the individual count.
 *
 * @return              @c false, if the ammo can't be picked up at all.
 */
boolean P_GiveAmmo(player_t *player, ammotype_t ammo, int num)
{
    if(ammo == AT_NOAMMO)
        return false;

    if(ammo < 0 || ammo > NUM_AMMO_TYPES)
        Con_Error("P_GiveAmmo: bad type %i", ammo);

    if(!(player->ammo[ammo].owned < player->ammo[ammo].max))
        return false;

    if(num)
        num *= clipAmmo[ammo];
    else
        num = clipAmmo[ammo] / 2;

    if(gameSkill == SM_BABY || gameSkill == SM_NIGHTMARE)
    {
        // Give double ammo in trainer mode (you'll need it in nightmare!).
        num *= 2;
    }

    // We are about to receive some more ammo. Does the player want to
    // change weapon automatically?
    P_MaybeChangeWeapon(player, WT_NOCHANGE, ammo, false);

    player->ammo[ammo].owned = MIN_OF(player->ammo[ammo].max,
        player->ammo[ammo].owned + num);
    player->update |= PSF_AMMO;

    // Maybe unhide the HUD?
    ST_HUDUnHide(player - players, HUE_ON_PICKUP_AMMO);

    return true;
}

/**
 * The weapon name may have a MF_DROPPED flag ored in.
 */
boolean P_GiveWeapon(player_t* player, weapontype_t weapon, boolean dropped)
{
    int                 numClips;
    ammotype_t          i;
    boolean             gaveAmmo = false, gaveWeapon = false;

    if(IS_NETGAME && (deathmatch != 2) && !dropped)
    {
        // Leave placed weapons forever on net games.
        if(player->weapons[weapon].owned)
            return false;

        player->bonusCount += BONUSADD;
        player->weapons[weapon].owned = true;
        player->update |= PSF_OWNED_WEAPONS;

        // Give some of each of the ammo types used by this weapon.
        for(i=0; i < NUM_AMMO_TYPES; ++i)
        {
            if(!weaponInfo[weapon][player->class].mode[0].ammoType[i])
                continue; // Weapon does not take this type of ammo.

            if(deathmatch)
                numClips = 5;
            else
                numClips = 2;

            if(P_GiveAmmo(player, i, numClips))
                gaveAmmo = true; // At least ONE type of ammo was given.
        }

        // Should we change weapon automatically?
        P_MaybeChangeWeapon(player, weapon, AT_NOAMMO, deathmatch == 1);

        // Maybe unhide the HUD?
        ST_HUDUnHide(player - players, HUE_ON_PICKUP_WEAPON);

        S_ConsoleSound(SFX_WPNUP, NULL, player - players);
        return false;
    }
    else
    {
        // Give some of each of the ammo types used by this weapon.
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            if(!weaponInfo[weapon][player->class].mode[0].ammoType[i])
                continue;  // Weapon does not take this type of ammo.

            // Give one clip with a dropped weapon, two clips if found.
            if(dropped)
                numClips = 1;
            else
                numClips = 2;

            if(P_GiveAmmo(player, i, numClips))
                gaveAmmo = true; // At least ONE type of ammo was given.
        }

        if(player->weapons[weapon].owned)
        {
            gaveWeapon = false;
        }
        else
        {
            gaveWeapon = true;
            player->weapons[weapon].owned = true;
            player->update |= PSF_OWNED_WEAPONS;

            // Should we change weapon automatically?
            P_MaybeChangeWeapon(player, weapon, AT_NOAMMO, false);
        }

        // Maybe unhide the HUD?
        if(gaveWeapon)
            ST_HUDUnHide(player - players, HUE_ON_PICKUP_WEAPON);

        return (gaveWeapon || gaveAmmo);
    }
}

/**
 * @return              @c false, if the body isn't needed at all.
 */
boolean P_GiveBody(player_t *player, int num)
{
    if(player->health >= maxHealth)
        return false;

    player->health += num;
    if(player->health > maxHealth)
        player->health = maxHealth;

    player->plr->mo->health = player->health;
    player->update |= PSF_HEALTH;

    // Maybe unhide the HUD?
    ST_HUDUnHide(player - players, HUE_ON_PICKUP_HEALTH);

    return true;
}

/**
 * @return              @c true, iff the armor was given.
 */
boolean P_GiveArmor(player_t* plr, int type, int points)
{
    if(plr->armorPoints >= points)
        return false; // Don't pick up.

    P_PlayerSetArmorType(plr, type);
    P_PlayerGiveArmorBonus(plr, points - plr->armorPoints);

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_ARMOR);

    return true;
}

void P_GiveKey(player_t* plr, keytype_t card)
{
    if(plr->keys[card])
        return;

    plr->bonusCount = BONUSADD;
    plr->keys[card] = 1;
    plr->update |= PSF_KEYS;

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_KEY);
}

void P_GiveBackpack(player_t* plr)
{
    int                 i;

    if(!plr->backpack)
    {
        plr->update |= PSF_MAX_AMMO;
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            plr->ammo[i].max *= 2;
        }

        plr->backpack = true;
    }

    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        P_GiveAmmo(plr, i, 1);
    }

    P_SetMessage(plr, GOTBACKPACK, false);
}

boolean P_GivePower(player_t* player, int power)
{
    player->update |= PSF_POWERS;

    switch(power)
    {
    case PT_INVULNERABILITY:
        player->powers[power] = INVULNTICS;
        break;

    case PT_INVISIBILITY:
        player->powers[power] = INVISTICS;
        player->plr->mo->flags |= MF_SHADOW;;
        break;

    case PT_FLIGHT:
        player->powers[power] = 1;
        player->plr->mo->flags2 |= MF2_FLY;
        player->plr->mo->flags |= MF_NOGRAVITY;
        if(player->plr->mo->pos[VZ] <= player->plr->mo->floorZ)
        {
            player->flyHeight = 10; // Thrust the player in the air a bit.
            player->plr->mo->flags |= DDPF_FIXMOM;
        }
        break;

    case PT_INFRARED:
        player->powers[power] = INFRATICS;
        break;

    case PT_IRONFEET:
        player->powers[power] = IRONTICS;
        break;

    case PT_STRENGTH:
        P_GiveBody(player, maxHealth);
        player->powers[power] = 1;
        break;

    default:
        if(player->powers[power])
            return false; // Already got it.

        player->powers[power] = 1;
        break;
    }

    if(power == PT_ALLMAP)
        AM_RevealMap(AM_MapForPlayer(player - players), true);

    // Maybe unhide the HUD?
    ST_HUDUnHide(player - players, HUE_ON_PICKUP_POWER);

    return true;
}

boolean P_TakePower(player_t* player, int power)
{
    mobj_t*             plrmo = player->plr->mo;

    player->update |= PSF_POWERS;
    if(player->powers[PT_FLIGHT])
    {
        if(plrmo->pos[VZ] != plrmo->floorZ && cfg.lookSpring)
        {
            player->centering = true;
        }

        plrmo->flags2 &= ~MF2_FLY;
        plrmo->flags &= ~MF_NOGRAVITY;
        player->powers[power] = 0;
        return true;
    }

    if(!player->powers[power])
        return false; // Dont got it.

    if(power == PT_ALLMAP)
        AM_RevealMap(AM_MapForPlayer(player - players), false);

    player->powers[power] = 0;
    return true;
}

typedef enum {
    IT_NONE = 0,
    IT_HEALTH_PACK,
    IT_HEALTH_KIT,
    IT_HEALTH_BONUS,
    IT_HEALTH_SOULSPHERE,
    IT_ARMOR_GREEN,
    IT_ARMOR_BLUE,
    IT_ARMOR_BONUS,
    IT_WEAPON_BFG,
    IT_WEAPON_CHAINGUN,
    IT_WEAPON_CHAINSAW,
    IT_WEAPON_RLAUNCHER,
    IT_WEAPON_PLASMARIFLE,
    IT_WEAPON_SHOTGUN,
    IT_WEAPON_SSHOTGUN,
    IT_AMMO_CLIP,
    IT_AMMO_CLIP_BOX,
    IT_AMMO_ROCKET,
    IT_AMMO_ROCKET_BOX,
    IT_AMMO_CELL,
    IT_AMMO_CELL_BOX,
    IT_AMMO_SHELL,
    IT_AMMO_SHELL_BOX,
    IT_KEY_BLUE,
    IT_KEY_YELLOW,
    IT_KEY_RED,
    IT_KEY_BLUESKULL,
    IT_KEY_YELLOWSKULL,
    IT_KEY_REDSKULL,
    IT_INVUL,
    IT_BERSERK,
    IT_INVIS,
    IT_SUIT,
    IT_ALLMAP,
    IT_VISOR,
    IT_BACKPACK,
    IT_MEGASPHERE
} itemtype_t;

static itemtype_t getItemTypeBySprite(spritetype_e sprite)
{
    static const struct item_s {
        itemtype_t      type;
        spritetype_e    sprite;
    } items[] = {
        { IT_HEALTH_PACK, SPR_STIM },
        { IT_HEALTH_KIT, SPR_MEDI },
        { IT_HEALTH_BONUS, SPR_BON1 },
        { IT_HEALTH_SOULSPHERE, SPR_SOUL },
        { IT_ARMOR_GREEN, SPR_ARM1 },
        { IT_ARMOR_BLUE, SPR_ARM2 },
        { IT_ARMOR_BONUS, SPR_BON2 },
        { IT_WEAPON_BFG, SPR_BFUG },
        { IT_WEAPON_CHAINGUN, SPR_MGUN },
        { IT_WEAPON_CHAINSAW, SPR_CSAW },
        { IT_WEAPON_RLAUNCHER, SPR_LAUN },
        { IT_WEAPON_PLASMARIFLE, SPR_PLAS },
        { IT_WEAPON_SHOTGUN, SPR_SHOT },
        { IT_WEAPON_SSHOTGUN, SPR_SGN2 },
        { IT_AMMO_CLIP, SPR_CLIP },
        { IT_AMMO_CLIP_BOX, SPR_AMMO },
        { IT_AMMO_ROCKET, SPR_ROCK },
        { IT_AMMO_ROCKET_BOX, SPR_BROK },
        { IT_AMMO_CELL, SPR_CELL },
        { IT_AMMO_CELL_BOX, SPR_CELP },
        { IT_AMMO_SHELL, SPR_SHEL },
        { IT_AMMO_SHELL_BOX, SPR_SBOX },
        { IT_KEY_BLUE, SPR_BKEY },
        { IT_KEY_YELLOW, SPR_YKEY },
        { IT_KEY_RED, SPR_RKEY },
        { IT_KEY_BLUESKULL, SPR_BSKU },
        { IT_KEY_YELLOWSKULL, SPR_YSKU },
        { IT_KEY_REDSKULL, SPR_RSKU },
        { IT_INVUL, SPR_PINV },
        { IT_BERSERK, SPR_PSTR },
        { IT_INVIS, SPR_PINS },
        { IT_SUIT, SPR_SUIT },
        { IT_ALLMAP, SPR_PMAP },
        { IT_VISOR, SPR_PVIS },
        { IT_BACKPACK, SPR_BPAK },
        { IT_MEGASPHERE, SPR_MEGA },
        { IT_NONE, 0 }
    };
    uint                i;

    for(i = 0; items[i].type != IT_NONE; ++i)
        if(items[i].sprite == sprite)
            return items[i].type;

    return IT_NONE;
}

/**
 * @param plr           Player being given item.
 * @param item          Type of item being given.
 * @param dropped       @c true = the item was dropped by some entity.
 *
 * @return              @c true iff the item should be destroyed.
 */
static boolean giveItem(player_t* plr, itemtype_t item, boolean dropped)
{
    GameMap* map;
    if(!plr)
        return false;
    map = Thinker_Map((thinker_t*) plr->plr->mo);
    switch(item)
    {
    case IT_ARMOR_GREEN:
        if(!P_GiveArmor(plr, armorClass[0],
                        armorPoints[MINMAX_OF(0, armorClass[0] - 1, 1)]))
            return false;

        P_SetMessage(plr, GOTARMOR, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ARMOR_BLUE:
        if(!P_GiveArmor(plr, armorClass[1],
                        armorPoints[MINMAX_OF(0, armorClass[1] - 1, 1)]))
            return false;
        P_SetMessage(plr, GOTMEGA, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ARMOR_BONUS:
        if(!plr->armorType)
            P_PlayerSetArmorType(plr, armorClass[0]);
        if(plr->armorPoints < armorPoints[1])
            P_PlayerGiveArmorBonus(plr, 1);

        P_SetMessage(plr, GOTARMBONUS, false);
        if(!map->inSetup)
        {
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
            // Maybe unhide the HUD?
            ST_HUDUnHide(plr - players, HUE_ON_PICKUP_ARMOR);
        }
        break;

    case IT_HEALTH_PACK:
        if(!P_GiveBody(plr, 10))
            return false;
        P_SetMessage(plr, GOTSTIM, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_HEALTH_KIT:
        {
        int                 oldHealth = plr->health;

        /**
         * DOOM bug:
         * The following test was originaly placed AFTER the call to
         * P_GiveBody thereby making the first outcome impossible as
         * the medikit gives 25 points of health. This resulted that
         * the GOTMEDINEED "Picked up a medikit that you REALLY need"
         * was never used.
         */

        if(!P_GiveBody(plr, 25))
            return false;

        P_SetMessage(plr, GET_TXT(
            (oldHealth < 25)? TXT_GOTMEDINEED : TXT_GOTMEDIKIT), false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;
        }
    case IT_HEALTH_BONUS:
        plr->health++; // Can go over 100%.
        if(plr->health > healthLimit)
            plr->health = healthLimit;
        plr->plr->mo->health = plr->health;
        plr->update |= PSF_HEALTH;
        P_SetMessage(plr, GOTHTHBONUS, false);
        if(!map->inSetup)
        {
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
            // Maybe unhide the HUD?
            ST_HUDUnHide(plr - players, HUE_ON_PICKUP_HEALTH);
        }
        break;

    case IT_HEALTH_SOULSPHERE:
        plr->health += soulSphereHealth;
        if(plr->health > soulSphereLimit)
            plr->health = soulSphereLimit;
        plr->plr->mo->health = plr->health;
        plr->update |= PSF_HEALTH;
        P_SetMessage(plr, GOTSUPER, false);
        if(!map->inSetup)
        {
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
            // Maybe unhide the HUD?
            ST_HUDUnHide(plr - players, HUE_ON_PICKUP_HEALTH);
        }
        break;

    case IT_KEY_BLUE:
        P_GiveKey(plr, KT_BLUECARD);
        if(!plr->keys[KT_BLUECARD])
            P_SetMessage(plr, GOTBLUECARD, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);

        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_YELLOW:
        P_GiveKey(plr, KT_YELLOWCARD);
        if(!plr->keys[KT_YELLOWCARD])
            P_SetMessage(plr, GOTYELWCARD, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);

        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_RED:
        P_GiveKey(plr, KT_REDCARD);
        if(!plr->keys[KT_REDCARD])
            P_SetMessage(plr, GOTREDCARD, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);

        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_BLUESKULL:
        P_GiveKey(plr, KT_BLUESKULL);
        if(!plr->keys[KT_BLUESKULL])
            P_SetMessage(plr, GOTBLUESKUL, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);

        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_YELLOWSKULL:
        P_GiveKey(plr, KT_YELLOWSKULL);
        if(!plr->keys[KT_YELLOWSKULL])
            P_SetMessage(plr, GOTYELWSKUL, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);

        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_REDSKULL:
        P_GiveKey(plr, KT_REDSKULL);
        if(!plr->keys[KT_REDSKULL])
            P_SetMessage(plr, GOTREDSKULL, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);

        if(IS_NETGAME)
            return false;
        break;

    case IT_MEGASPHERE:
        if(gameMode != commercial)
            return false;
        plr->health = megaSphereHealth;
        plr->plr->mo->health = plr->health;
        plr->update |= PSF_HEALTH;
        P_GiveArmor(plr, armorClass[1],
                    armorPoints[MINMAX_OF(0, armorClass[1] - 1, 1)]);
        P_SetMessage(plr, GOTMSPHERE, false);
        if(!map->inSetup)
        {
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
            // Maybe unhide the HUD?
            ST_HUDUnHide(plr - players, HUE_ON_PICKUP_HEALTH);
        }
        break;

    case IT_INVUL:
        if(!P_GivePower(plr, PT_INVULNERABILITY))
            return false;

        P_SetMessage(plr, GOTINVUL, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_BERSERK:
        if(!P_GivePower(plr, PT_STRENGTH))
            return false;

        P_SetMessage(plr, GOTBERSERK, false);
        if(plr->readyWeapon != WT_FIRST && cfg.berserkAutoSwitch)
        {
            plr->pendingWeapon = WT_FIRST;
            plr->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
        }
        if(!map->inSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_INVIS:
        if(!P_GivePower(plr, PT_INVISIBILITY))
            return false;

        P_SetMessage(plr, GOTINVIS, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_SUIT:
        if(!P_GivePower(plr, PT_IRONFEET))
            return false;

        P_SetMessage(plr, GOTSUIT, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_ALLMAP:
        if(!P_GivePower(plr, PT_ALLMAP))
            return false;

        P_SetMessage(plr, GOTMAP, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_VISOR:
        if(!P_GivePower(plr, PT_INFRARED))
            return false;

        P_SetMessage(plr, GOTVISOR, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_BACKPACK:
        P_GiveBackpack(plr);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_CLIP:
        if(!P_GiveAmmo(plr, AT_CLIP, dropped? 0 : 1))
            return false;

        P_SetMessage(plr, GOTCLIP, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_CLIP_BOX:
        if(!P_GiveAmmo(plr, AT_CLIP, 5))
            return false;

        P_SetMessage(plr, GOTCLIPBOX, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_ROCKET:
        if(!P_GiveAmmo(plr, AT_MISSILE, 1))
            return false;

        P_SetMessage(plr, GOTROCKET, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_ROCKET_BOX:
        if(!P_GiveAmmo(plr, AT_MISSILE, 5))
            return false;

        P_SetMessage(plr, GOTROCKBOX, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_CELL:
        if(!P_GiveAmmo(plr, AT_CELL, 1))
            return false;

        P_SetMessage(plr, GOTCELL, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_CELL_BOX:
        if(!P_GiveAmmo(plr, AT_CELL, 5))
            return false;

        P_SetMessage(plr, GOTCELLBOX, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_SHELL:
        if(!P_GiveAmmo(plr, AT_SHELL, 1))
            return false;

        P_SetMessage(plr, GOTSHELLS, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_SHELL_BOX:
        if(!P_GiveAmmo(plr, AT_SHELL, 5))
            return false;

        P_SetMessage(plr, GOTSHELLBOX, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_WEAPON_BFG:
        if(!P_GiveWeapon(plr, WT_SEVENTH, dropped))
            return false;

        P_SetMessage(plr, GOTBFG9000, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_WPNUP, NULL, plr - players);
        break;

    case IT_WEAPON_CHAINGUN:
        if(!P_GiveWeapon(plr, WT_FOURTH, dropped))
            return false;

        P_SetMessage(plr, GOTCHAINGUN, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_WPNUP, NULL, plr - players);
        break;

    case IT_WEAPON_CHAINSAW:
        if(!P_GiveWeapon(plr, WT_EIGHTH, dropped))
            return false;

        P_SetMessage(plr, GOTCHAINSAW, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_WPNUP, NULL, plr - players);
        break;

    case IT_WEAPON_RLAUNCHER:
        if(!P_GiveWeapon(plr, WT_FIFTH, dropped))
            return false;

        P_SetMessage(plr, GOTLAUNCHER, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_WPNUP, NULL, plr - players);
        break;

    case IT_WEAPON_PLASMARIFLE:
        if(!P_GiveWeapon(plr, WT_SIXTH, dropped))
            return false;

        P_SetMessage(plr, GOTPLASMA, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_WPNUP, NULL, plr - players);
        break;

    case IT_WEAPON_SHOTGUN:
        if(!P_GiveWeapon(plr, WT_THIRD, dropped))
            return false;

        P_SetMessage(plr, GOTSHOTGUN, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_WPNUP, NULL, plr - players);
        break;

    case IT_WEAPON_SSHOTGUN:
        if(!P_GiveWeapon(plr, WT_NINETH, dropped))
            return false;

        P_SetMessage(plr, GOTSHOTGUN2, false);
        if(!map->inSetup)
            S_ConsoleSound(SFX_WPNUP, NULL, plr - players);
        break;

    default:
        Con_Error("giveItem: Unknown item %i.", (int) item);
    }

    return true;
}

void P_TouchSpecialMobj(mobj_t* special, mobj_t* toucher)
{
    assert(special);
    assert(toucher);
    {
    GameMap* map = Thinker_Map((thinker_t*) toucher);
    player_t* player;
    float delta;
    itemtype_t item;

    delta = special->pos[VZ] - toucher->pos[VZ];
    if(delta > toucher->height || delta < -8)
    {   // Out of reach.
        return;
    }

    // Dead thing touching (can happen with a sliding player corpse).
    if(toucher->health <= 0)
        return;

    player = toucher->player;

    // Identify by sprite.
    if((item = getItemTypeBySprite(special->sprite)) != IT_NONE)
    {
        if(!giveItem(player, item, (special->flags & MF_DROPPED)? true : false))
            return; // Don't destroy the item.
    }
    else
    {
        Con_Message("P_TouchSpecialMobj: Unknown gettable thing %i.\n",
                    (int) special->type);
    }

    if(special->flags & MF_COUNTITEM)
        player->itemCount++;

    P_MobjRemove(special, false);

    if(!map->inSetup)
        player->bonusCount += BONUSADD;
    }
}

void P_KillMobj(mobj_t* source, mobj_t* target, boolean stomping)
{
    assert(target);
    {
    GameMap* map = Thinker_Map((thinker_t*) target);
    mobjtype_t item;
    mobj_t* mo;
    unsigned int an;
    angle_t angle;

    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY);

    if(target->type != MT_SKULL)
        target->flags &= ~MF_NOGRAVITY;

    target->flags |= MF_CORPSE | MF_DROPOFF;
    target->flags2 &= ~MF2_PASSMOBJ;
    target->corpseTics = 0;
    target->height /= 2*2;

    if(source && source->player)
    {
        // Count for intermission.
        if(target->flags & MF_COUNTKILL)
            source->player->killCount++;

        if(target->player)
        {
            source->player->frags[target->player - players]++;
            NetSv_FragsForAll(source->player);
            NetSv_KillMessage(source->player, target->player, stomping);
        }
    }
    else if(!IS_NETGAME && (target->flags & MF_COUNTKILL))
    {
        // Count all monster deaths (even those caused by other monsters).
        players[0].killCount++;
    }

    if(target->player)
    {
        // Count environment kills against the player.
        if(!source)
        {
            target->player->frags[target->player - players]++;
            NetSv_FragsForAll(target->player);
            NetSv_KillMessage(target->player, target->player, stomping);
        }

        target->flags &= ~MF_SOLID;
        target->flags2 &= ~MF2_FLY;
        target->player->powers[PT_FLIGHT] = 0;
        target->player->playerState = PST_DEAD;
        target->player->rebornWait = PLAYER_REBORN_TICS;
        target->player->update |= PSF_STATE;
        target->player->plr->flags |= DDPF_DEAD;
        P_DropWeapon(target->player);

        // Don't die with the automap open.
        AM_Open(AM_MapForPlayer(target->player - players), false, false);
    }

    if(target->health < -target->info->spawnHealth &&
       P_GetState(target->type, SN_XDEATH))
    {   // Extreme death.
        P_MobjChangeState(target, P_GetState(target->type, SN_XDEATH));
    }
    else
    {   // Normal death.
        P_MobjChangeState(target, P_GetState(target->type, SN_DEATH));
    }

    target->tics -= P_Random() & 3;

    if(target->tics < 1)
        target->tics = 1;

    // Drop stuff.
    // This determines the kind of object spawned during the death frame
    // of a thing.
    switch(target->type)
    {
    case MT_WOLFSS:
    case MT_POSSESSED:
        item = MT_CLIP;
        break;

    case MT_SHOTGUY:
        item = MT_SHOTGUN;
        break;

    case MT_CHAINGUY:
        item = MT_CHAINGUN;
        break;

    default:
        return;
    }

    // Don't drop at the exact same place, causes Z flickering with
    // 3D sprites.
    angle = P_Random() << 24;
    an = angle >> ANGLETOFINESHIFT;
    if((mo = GameMap_SpawnMobj3f(map, item, target->pos[VX] + 3 * FIX2FLT(finecosine[an]),
                           target->pos[VY] + 3 * FIX2FLT(finesine[an]),
                           0, angle, MSF_Z_FLOOR)))
        mo->flags |= MF_DROPPED; // Special versions of items.
    }
}

/**
 * Damages both enemies and players.
 *
 * @param inflictor     Mobj that caused the damage creature or missile,
 *                      can be NULL (slime, etc)
 * @param source        Mobj to target after taking damage. Can be @c NULL
 *                      for barrel explosions and other environmental stuff.
 *                      Source and inflictor are the same for melee attacks.
 * @return              Actual amount of damage done.
 */
int P_DamageMobj(mobj_t* target, mobj_t* inflictor, mobj_t* source,
                 int damageP, boolean stomping)
{
    angle_t             angle;
    int                 saved, originalHealth;
    player_t*           player;
    int                 temp, damage;

    if(!target)
        return 0; // Wha?

    originalHealth = target->health;

    // The actual damage (== damageP * netMobDamageModifier for any
    // non-player mobj).
    damage = damageP;

    if(IS_NETGAME && !stomping &&
       D_NetDamageMobj(target, inflictor, source, damage))
    {   // We're done here.
        return 0;
    }

    // Clients can't harm anybody.
    if(IS_CLIENT)
        return 0;

    if(!(target->flags & MF_SHOOTABLE))
        return 0; // Shouldn't happen...

    if(target->health <= 0)
        return 0;

    if(target->player)
    {   // Player specific.
        // Check if player-player damage is disabled.
        if(source && source->player && source->player != target->player)
        {
            // Co-op damage disabled?
            if(IS_NETGAME && !deathmatch && cfg.noCoopDamage)
                return 0;

            // Same color, no damage?
            if(cfg.noTeamDamage &&
               cfg.playerColor[target->player - players] ==
               cfg.playerColor[source->player - players])
                return 0;
        }
    }

    if(target->flags & MF_SKULLFLY)
    {
        target->mom[MX] = target->mom[MY] = target->mom[MZ] = 0;
    }

    player = target->player;
    if(player && gameSkill == SM_BABY)
        damage /= 2; // Take half damage in trainer mode.

    // Use the cvar damage multiplier netMobDamageModifier only if the
    // inflictor is not a player.
    if(inflictor && !inflictor->player &&
       (!source || (source && !source->player)))
    {
        // damage = (int) ((float) damage * netMobDamageModifier);
        if(IS_NETGAME)
            damage *= cfg.netMobDamageModifier;
    }

    // Some close combat weapons should not inflict thrust and push the
    // victim out of reach, thus kick away unless using a melee weapon.
    if(inflictor && !(target->flags & MF_NOCLIP) &&
       (!source || !source->player ||
        source->player->readyWeapon != WT_EIGHTH) &&
       !(inflictor->flags2 & MF2_NODMGTHRUST))
    {
        uint                an;
        float               thrust;

        angle = R_PointToAngle2(inflictor->pos[VX], inflictor->pos[VY],
                                target->pos[VX], target->pos[VY]);

        thrust = FIX2FLT(damage * (FRACUNIT>>3) * 100 / target->info->mass);

        // Make fall forwards sometimes.
        if(damage < 40 && damage > target->health &&
           target->pos[VZ] - inflictor->pos[VZ] > 64 && (P_Random() & 1))
        {
            angle += ANG180;
            thrust *= 4;
        }

        an = angle >> ANGLETOFINESHIFT;
        target->mom[MX] += thrust * FIX2FLT(finecosine[an]);
        target->mom[MY] += thrust * FIX2FLT(finesine[an]);
        if(target->dPlayer)
        {
            // Only fix momentum. Otherwise clients will find it difficult
            // to escape from the damage inflictor.
            target->dPlayer->flags |= DDPF_FIXMOM;
        }

        // $dropoff_fix: thrust objects hanging off ledges.
        if((target->intFlags & MIF_FALLING) && target->gear >= MAXGEAR)
            target->gear = 0;
    }

    if(player)
    {   // Player specific.
        // End of game hell hack.
        if(P_ToXSectorOfSubsector(target->subsector)->special == 11 &&
           damage >= target->health)
        {
            damage = target->health - 1;
        }

        // Below certain threshold, ignore damage in GOD mode, or with
        // INVUL power.
        if(damage < 1000 &&
           ((P_GetPlayerCheats(player) & CF_GODMODE) ||
            player->powers[PT_INVULNERABILITY]))
        {
            return 0;
        }

        if(player->armorType)
        {
            if(player->armorType == 1)
                saved = damage / 3;
            else
                saved = damage / 2;

            if(player->armorPoints <= saved)
            {   // Armor is used up.
                saved = player->armorPoints;
                player->armorType = 0;
            }

            player->armorPoints -= saved;
            player->update |= PSF_ARMOR_POINTS;
            damage -= saved;
        }

        player->health -= damage;
        if(player->health < 0)
            player->health = 0;

        player->update |= PSF_HEALTH;
        player->attacker = source;

        player->damageCount += damage; // Add damage after armor / invuln.
        if(player->damageCount > 100)
            player->damageCount = 100; // Teleport stomp does 10k points...

        temp = damage < 100 ? damage : 100;

        // Maybe unhide the HUD?
        ST_HUDUnHide(player - players, HUE_ON_DAMAGE);
    }

    // How about some particles, yes?
    // Only works when both target and inflictor are real mobjs.
    P_SpawnDamageParticleGen(target, inflictor, damage);

    // Do the damage.
    target->health -= damage;
    if(target->health > 0)
    {   // Still alive, phew!
        if((P_Random() < target->info->painChance) &&
           !(target->flags & MF_SKULLFLY))
        {
            statenum_t          state;

            target->flags |= MF_JUSTHIT; // Fight back!

            if((state = P_GetState(target->type, SN_PAIN)) != S_NULL)
                P_MobjChangeState(target, state);
        }

        target->reactionTime = 0; // We're awake now...

        if(source &&
           ((!target->threshold && !(source->flags3 & MF3_NOINFIGHT)) || target->type == MT_VILE) &&
           source != target && source->type != MT_VILE)
        {
            statenum_t          state;

            // Target mobj is not intent on another mobj, so make it chase
            // after the source of the damage.
            target->target = source;
            target->threshold = BASETHRESHOLD;

            if((state = P_GetState(target->type, SN_SEE)) != S_NULL &&
               target->state == &STATES[P_GetState(target->type, SN_SPAWN)])
            {
                P_MobjChangeState(target, state);
            }
        }
    }
    else
    {   // Death.
        P_KillMobj(source, target, stomping);
    }

    return originalHealth - target->health;

#undef BASETHRESHOLD
}
