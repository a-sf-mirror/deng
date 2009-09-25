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
 * p_inter.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jhexen.h"

#include "am_map.h"
#include "p_inventory.h"
#include "p_player.h"
#include "p_map.h"
#include "p_user.h"

// MACROS ------------------------------------------------------------------

#define BONUSADD                (6)

// TYPES -------------------------------------------------------------------

typedef enum {
    IT_NONE = -1,
    IT_HEALTH_VIAL,
    IT_ARMOR_MESH,
    IT_ARMOR_SHIELD,
    IT_ARMOR_HELMET,
    IT_ARMOR_AMULET,
    IT_KEY_STEEL,
    IT_KEY_CAVE,
    IT_KEY_AXE,
    IT_KEY_FIRE,
    IT_KEY_EMERALD,
    IT_KEY_DUNGEON,
    IT_KEY_SILVER,
    IT_KEY_RUSTED,
    IT_KEY_HORN,
    IT_KEY_SWAMP,
    IT_KEY_CASTLE,
    IT_ITEM_QUARTZFLASK,
    IT_ITEM_WINGS,
    IT_ITEM_DEFENDER,
    IT_ITEM_SERVANT,
    IT_ITEM_PORKALATOR,
    IT_ITEM_MYSTICURN,
    IT_ITEM_AMBITINCANT,
    IT_ITEM_TORCH,
    IT_ITEM_CHAOSDEVICE,
    IT_ITEM_BANISHDEVICE,
    IT_ITEM_FLETCHETTE,
    IT_ITEM_BOOTSOFSPEED,
    IT_ITEM_KRATEROFMIGHT,
    IT_ITEM_BRACERS,
    IT_ITEM_REPULSION,
    IT_PUZZLE_SKULL,
    IT_PUZZLE_BIGGEM,
    IT_PUZZLE_REDGEM,
    IT_PUZZLE_GREENGEM1,
    IT_PUZZLE_GREENGEM2,
    IT_PUZZLE_BLUEGEM1,
    IT_PUZZLE_BLUEGEM2,
    IT_PUZZLE_BOOK1,
    IT_PUZZLE_BOOK2,
    IT_PUZZLE_SKULL2,
    IT_PUZZLE_FWEAPON,
    IT_PUZZLE_CWEAPON,
    IT_PUZZLE_MWEAPON,
    IT_PUZZLE_GEAR1,
    IT_PUZZLE_GEAR2,
    IT_PUZZLE_GEAR3,
    IT_PUZZLE_GEAR4,
    IT_MANA_BLUE,
    IT_MANA_GREEN,
    IT_MANA_COMBINED,
    IT_WEAPON_FROSTSHARDS,
    IT_WEAPON_ARCOFDEATH,
    IT_WEAPON_AXE,
    IT_WEAPON_HAMMER,
    IT_WEAPON_SERPENTSTAFF,
    IT_WEAPON_FIRESTORM,
    IT_WEAPON_QUIETUS1,
    IT_WEAPON_QUIETUS2,
    IT_WEAPON_QUIETUS3,
    IT_WEAPON_WRAITHVERGE1,
    IT_WEAPON_WRAITHVERGE2,
    IT_WEAPON_WRAITHVERGE3,
    IT_WEAPON_BLOODSCOURGE1,
    IT_WEAPON_BLOODSCOURGE2,
    IT_WEAPON_BLOODSCOURGE3
} itemtype_t;

// Item Info Flags:
#define IIF_LEAVE_COOP          0x1 // Leave for others in Cooperative games.
#define IIF_LEAVE_DEATHMATCH    0x2 // Leave for others in Deathmatch games.

typedef struct iteminfo_s {
    itemtype_t          type;
    short               flags; // IIF_* flags.
    boolean            (*giveFunc) (player_t*);
    textenum_t          pickupMsg;
    sfxenum_t           pickupSound;
} iteminfo_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void setDormantItem(mobj_t* mo);

static boolean pickupHealthVial(player_t* plr);
static boolean pickupMesh(player_t* plr);
static boolean pickupShield(player_t* plr);
static boolean pickupHelmet(player_t* plr);
static boolean pickupAmulet(player_t* plr);
static boolean pickupSteelKey(player_t* plr);
static boolean pickupCaveKey(player_t* plr);
static boolean pickupAxeKey(player_t* plr);
static boolean pickupFireKey(player_t* plr);
static boolean pickupEmeraldKey(player_t* plr);
static boolean pickupDungeonKey(player_t* plr);
static boolean pickupSilverKey(player_t* plr);
static boolean pickupRustedKey(player_t* plr);
static boolean pickupHornKey(player_t* plr);
static boolean pickupSwampKey(player_t* plr);
static boolean pickupCastleKey(player_t* plr);
static boolean pickupQuartzFlask(player_t* plr);
static boolean pickupWings(player_t* plr);
static boolean pickupDefender(player_t* plr);
static boolean pickupServant(player_t* plr);
static boolean pickupPorkalator(player_t* plr);
static boolean pickupMysticUrn(player_t* plr);
static boolean pickupAmbitIncant(player_t* plr);
static boolean pickupTorch(player_t* plr);
static boolean pickupChaosDevice(player_t* plr);
static boolean pickupBanishDevice(player_t* plr);
static boolean pickupFletchette(player_t* plr);
static boolean pickupBootsOfSpeed(player_t* plr);
static boolean pickupKraterOfMight(player_t* plr);
static boolean pickupBracers(player_t* plr);
static boolean pickupRepulsion(player_t* plr);
static boolean pickupSkull(player_t* plr);
static boolean pickupBigGem(player_t* plr);
static boolean pickupRedGem(player_t* plr);
static boolean pickupGreenGem1(player_t* plr);
static boolean pickupGreenGem2(player_t* plr);
static boolean pickupBlueGem1(player_t* plr);
static boolean pickupBlueGem2(player_t* plr);
static boolean pickupBook1(player_t* plr);
static boolean pickupBook2(player_t* plr);
static boolean pickupSkull2(player_t* plr);
static boolean pickupFWeapon(player_t* plr);
static boolean pickupCWeapon(player_t* plr);
static boolean pickupMWeapon(player_t* plr);
static boolean pickupGear1(player_t* plr);
static boolean pickupGear2(player_t* plr);
static boolean pickupGear3(player_t* plr);
static boolean pickupGear4(player_t* plr);
static boolean pickupBlueMana(player_t* plr);
static boolean pickupGreenMana(player_t* plr);
static boolean pickupCombinedMana(player_t* plr);
static boolean pickupFrostShards(player_t* plr);
static boolean pickupArcOfDeath(player_t* plr);
static boolean pickupAxe(player_t* plr);
static boolean pickupHammer(player_t* plr);
static boolean pickupSerpentStaff(player_t* plr);
static boolean pickupFireStorm(player_t* plr);
static boolean pickupQuietus1(player_t* plr);
static boolean pickupQuietus2(player_t* plr);
static boolean pickupQuietus3(player_t* plr);
static boolean pickupWraithVerge1(player_t* plr);
static boolean pickupWraithVerge2(player_t* plr);
static boolean pickupWraithVerge3(player_t* plr);
static boolean pickupBloodScourge1(player_t* plr);
static boolean pickupBloodScourge2(player_t* plr);
static boolean pickupBloodScourge3(player_t* plr);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int echoMsg = 1;

int TextKeyMessages[] = {
    TXT_TXT_KEY_STEEL,
    TXT_TXT_KEY_CAVE,
    TXT_TXT_KEY_AXE,
    TXT_TXT_KEY_FIRE,
    TXT_TXT_KEY_EMERALD,
    TXT_TXT_KEY_DUNGEON,
    TXT_TXT_KEY_SILVER,
    TXT_TXT_KEY_RUSTED,
    TXT_TXT_KEY_HORN,
    TXT_TXT_KEY_SWAMP,
    TXT_TXT_KEY_CASTLE
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Index using itemtype_t - 1;
static const iteminfo_t items[] = {
    { IT_HEALTH_VIAL, 0, pickupHealthVial, TXT_TXT_ITEMHEALTH, SFX_PICKUP_PUZZ },
    { IT_ARMOR_MESH, 0, pickupMesh, TXT_TXT_ARMOR1, SFX_PICKUP_PUZZ },
    { IT_ARMOR_SHIELD, 0, pickupShield, TXT_TXT_ARMOR2, SFX_PICKUP_PUZZ },
    { IT_ARMOR_HELMET, 0, pickupHelmet, TXT_TXT_ARMOR3, SFX_PICKUP_PUZZ },
    { IT_ARMOR_AMULET, 0, pickupAmulet, TXT_TXT_ARMOR4, SFX_PICKUP_PUZZ },
    { IT_KEY_STEEL, IIF_LEAVE_COOP | IIF_LEAVE_DEATHMATCH, pickupSteelKey, TXT_TXT_KEY_STEEL, SFX_PICKUP_KEY },
    { IT_KEY_CAVE, IIF_LEAVE_COOP | IIF_LEAVE_DEATHMATCH, pickupCaveKey, TXT_TXT_KEY_CAVE, SFX_PICKUP_KEY },
    { IT_KEY_AXE, IIF_LEAVE_COOP | IIF_LEAVE_DEATHMATCH, pickupAxeKey, TXT_TXT_KEY_AXE, SFX_PICKUP_KEY },
    { IT_KEY_FIRE, IIF_LEAVE_COOP | IIF_LEAVE_DEATHMATCH, pickupFireKey, TXT_TXT_KEY_FIRE, SFX_PICKUP_KEY },
    { IT_KEY_EMERALD, IIF_LEAVE_COOP | IIF_LEAVE_DEATHMATCH, pickupEmeraldKey, TXT_TXT_KEY_EMERALD, SFX_PICKUP_KEY },
    { IT_KEY_DUNGEON, IIF_LEAVE_COOP | IIF_LEAVE_DEATHMATCH, pickupDungeonKey, TXT_TXT_KEY_DUNGEON, SFX_PICKUP_KEY },
    { IT_KEY_SILVER, IIF_LEAVE_COOP | IIF_LEAVE_DEATHMATCH, pickupSilverKey, TXT_TXT_KEY_SILVER, SFX_PICKUP_KEY },
    { IT_KEY_RUSTED, IIF_LEAVE_COOP | IIF_LEAVE_DEATHMATCH, pickupRustedKey, TXT_TXT_KEY_RUSTED, SFX_PICKUP_KEY },
    { IT_KEY_HORN, IIF_LEAVE_COOP | IIF_LEAVE_DEATHMATCH, pickupHornKey, TXT_TXT_KEY_HORN, SFX_PICKUP_KEY },
    { IT_KEY_SWAMP, IIF_LEAVE_COOP | IIF_LEAVE_DEATHMATCH, pickupSwampKey, TXT_TXT_KEY_SWAMP, SFX_PICKUP_KEY },
    { IT_KEY_CASTLE, IIF_LEAVE_COOP | IIF_LEAVE_DEATHMATCH, pickupCastleKey, TXT_TXT_KEY_CASTLE, SFX_PICKUP_KEY },
    { IT_ITEM_QUARTZFLASK, 0, pickupQuartzFlask, TXT_TXT_INV_HEALTH, SFX_PICKUP_ITEM },
    { IT_ITEM_WINGS, 0, pickupWings, TXT_TXT_INV_FLY, SFX_PICKUP_ITEM },
    { IT_ITEM_DEFENDER, 0, pickupDefender, TXT_TXT_INV_INVULNERABILITY, SFX_PICKUP_ITEM },
    { IT_ITEM_SERVANT, 0, pickupServant, TXT_TXT_INV_SUMMON, SFX_PICKUP_ITEM },
    { IT_ITEM_PORKALATOR, 0, pickupPorkalator, TXT_TXT_INV_EGG, SFX_PICKUP_ITEM },
    { IT_ITEM_MYSTICURN, 0, pickupMysticUrn, TXT_TXT_INV_SUPERHEALTH, SFX_PICKUP_ITEM },
    { IT_ITEM_AMBITINCANT, 0, pickupAmbitIncant, TXT_TXT_INV_HEALINGRADIUS, SFX_PICKUP_ITEM },
    { IT_ITEM_TORCH, 0, pickupTorch, TXT_TXT_INV_TORCH, SFX_PICKUP_ITEM },
    { IT_ITEM_CHAOSDEVICE, 0, pickupChaosDevice, TXT_TXT_INV_TELEPORT, SFX_PICKUP_ITEM },
    { IT_ITEM_BANISHDEVICE, 0, pickupBanishDevice, TXT_TXT_INV_TELEPORTOTHER, SFX_PICKUP_ITEM },
    { IT_ITEM_FLETCHETTE, 0, pickupFletchette, TXT_TXT_INV_POISONBAG, SFX_PICKUP_ITEM },
    { IT_ITEM_BOOTSOFSPEED, 0, pickupBootsOfSpeed, TXT_TXT_INV_SPEED, SFX_PICKUP_ITEM },
    { IT_ITEM_KRATEROFMIGHT, 0, pickupKraterOfMight, TXT_TXT_INV_BOOSTMANA, SFX_PICKUP_ITEM },
    { IT_ITEM_BRACERS, 0, pickupBracers, TXT_TXT_INV_BOOSTARMOR, SFX_PICKUP_ITEM },
    { IT_ITEM_REPULSION, 0, pickupRepulsion, TXT_TXT_INV_BLASTRADIUS, SFX_PICKUP_ITEM },
    { IT_PUZZLE_SKULL, IIF_LEAVE_COOP, pickupSkull, TXT_TXT_INV_PUZZSKULL, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_BIGGEM, IIF_LEAVE_COOP, pickupBigGem, TXT_TXT_INV_PUZZGEMBIG, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_REDGEM, IIF_LEAVE_COOP, pickupRedGem, TXT_TXT_INV_PUZZGEMRED, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_GREENGEM1, IIF_LEAVE_COOP, pickupGreenGem1, TXT_TXT_INV_PUZZGEMGREEN1, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_GREENGEM2, IIF_LEAVE_COOP, pickupGreenGem2, TXT_TXT_INV_PUZZGEMGREEN2, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_BLUEGEM1, IIF_LEAVE_COOP, pickupBlueGem1, TXT_TXT_INV_PUZZGEMBLUE1, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_BLUEGEM2, IIF_LEAVE_COOP, pickupBlueGem2, TXT_TXT_INV_PUZZGEMBLUE2, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_BOOK1, IIF_LEAVE_COOP, pickupBook1, TXT_TXT_INV_PUZZBOOK1, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_BOOK2, IIF_LEAVE_COOP, pickupBook2, TXT_TXT_INV_PUZZBOOK2, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_SKULL2, IIF_LEAVE_COOP, pickupSkull2, TXT_TXT_INV_PUZZSKULL2, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_FWEAPON, IIF_LEAVE_COOP, pickupFWeapon, TXT_TXT_INV_PUZZFWEAPON, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_CWEAPON, IIF_LEAVE_COOP, pickupCWeapon, TXT_TXT_INV_PUZZCWEAPON, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_MWEAPON, IIF_LEAVE_COOP, pickupMWeapon, TXT_TXT_INV_PUZZMWEAPON, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_GEAR1, IIF_LEAVE_COOP, pickupGear1, TXT_TXT_INV_PUZZGEAR1, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_GEAR2, IIF_LEAVE_COOP, pickupGear2, TXT_TXT_INV_PUZZGEAR2, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_GEAR3, IIF_LEAVE_COOP, pickupGear3, TXT_TXT_INV_PUZZGEAR3, SFX_PICKUP_PUZZ },
    { IT_PUZZLE_GEAR4, IIF_LEAVE_COOP, pickupGear4, TXT_TXT_INV_PUZZGEAR4, SFX_PICKUP_PUZZ },
    { IT_MANA_BLUE, 0, pickupBlueMana, TXT_TXT_MANA_1, SFX_PICKUP_PUZZ },
    { IT_MANA_GREEN, 0, pickupGreenMana, TXT_TXT_MANA_2, SFX_PICKUP_PUZZ },
    { IT_MANA_COMBINED, 0, pickupCombinedMana, TXT_TXT_MANA_BOTH, SFX_PICKUP_PUZZ },
    { IT_WEAPON_FROSTSHARDS, IIF_LEAVE_COOP, pickupFrostShards, TXT_TXT_WEAPON_M2, SFX_PICKUP_WEAPON },
    { IT_WEAPON_ARCOFDEATH, IIF_LEAVE_COOP, pickupArcOfDeath, TXT_TXT_WEAPON_M3, SFX_PICKUP_WEAPON },
    { IT_WEAPON_AXE, IIF_LEAVE_COOP, pickupAxe, TXT_TXT_WEAPON_F2, SFX_PICKUP_WEAPON },
    { IT_WEAPON_HAMMER, IIF_LEAVE_COOP, pickupHammer, TXT_TXT_WEAPON_F3, SFX_PICKUP_WEAPON },
    { IT_WEAPON_SERPENTSTAFF, IIF_LEAVE_COOP, pickupSerpentStaff, TXT_TXT_WEAPON_C2, SFX_PICKUP_WEAPON },
    { IT_WEAPON_FIRESTORM, IIF_LEAVE_COOP, pickupFireStorm, TXT_TXT_WEAPON_C3, SFX_PICKUP_WEAPON },
    { IT_WEAPON_QUIETUS1, IIF_LEAVE_COOP, pickupQuietus1, TXT_TXT_QUIETUS_PIECE, SFX_PICKUP_WEAPON },
    { IT_WEAPON_QUIETUS2, IIF_LEAVE_COOP, pickupQuietus2, TXT_TXT_QUIETUS_PIECE, SFX_PICKUP_WEAPON },
    { IT_WEAPON_QUIETUS3, IIF_LEAVE_COOP, pickupQuietus3, TXT_TXT_QUIETUS_PIECE, SFX_PICKUP_WEAPON },
    { IT_WEAPON_WRAITHVERGE1, IIF_LEAVE_COOP, pickupWraithVerge1, TXT_TXT_WRAITHVERGE_PIECE, SFX_PICKUP_WEAPON },
    { IT_WEAPON_WRAITHVERGE2, IIF_LEAVE_COOP, pickupWraithVerge2, TXT_TXT_WRAITHVERGE_PIECE, SFX_PICKUP_WEAPON },
    { IT_WEAPON_WRAITHVERGE3, IIF_LEAVE_COOP, pickupWraithVerge3, TXT_TXT_WRAITHVERGE_PIECE, SFX_PICKUP_WEAPON },
    { IT_WEAPON_BLOODSCOURGE1, IIF_LEAVE_COOP, pickupBloodScourge1, TXT_TXT_BLOODSCOURGE_PIECE, SFX_PICKUP_WEAPON },
    { IT_WEAPON_BLOODSCOURGE2, IIF_LEAVE_COOP, pickupBloodScourge2, TXT_TXT_BLOODSCOURGE_PIECE, SFX_PICKUP_WEAPON },
    { IT_WEAPON_BLOODSCOURGE3, IIF_LEAVE_COOP, pickupBloodScourge3, TXT_TXT_BLOODSCOURGE_PIECE, SFX_PICKUP_WEAPON }
};

// CODE --------------------------------------------------------------------

void P_HideSpecialThing(mobj_t *thing)
{
    thing->flags &= ~MF_SPECIAL;
    thing->flags2 |= MF2_DONTDRAW;
    P_MobjChangeState(thing, S_HIDESPECIAL1);
}

/**
 * @return              @c true, if the player accepted the mana,
 *                      @c false, if it was refused (player has MAX_MANA)
 */
boolean P_GiveMana(player_t *plr, ammotype_t ammo, int num)
{
    int                 prevMana;

    if(ammo == AT_NOAMMO)
        return false;

    if(ammo < 0 || ammo > NUM_AMMO_TYPES)
        Con_Error("P_GiveMana: bad type %i", ammo);

    if(!(plr->ammo[ammo].owned < MAX_MANA))
        return false;

    if(gameSkill == SM_BABY || gameSkill == SM_NIGHTMARE)
    {   // Extra mana in baby mode and nightmare mode.
        num += num / 2;
    }
    prevMana = plr->ammo[ammo].owned;

    // We are about to receive some more ammo. Does the plr want to
    // change weapon automatically?
    P_MaybeChangeWeapon(plr, WT_NOCHANGE, ammo, false);

    if(plr->ammo[ammo].owned + num > MAX_MANA)
        plr->ammo[ammo].owned = MAX_MANA;
    else
        plr->ammo[ammo].owned += num;
    plr->update |= PSF_AMMO;

    //// \fixme - DJS: This shouldn't be actioned from here.
    if(plr->class == PCLASS_FIGHTER && plr->readyWeapon == WT_SECOND &&
       ammo == AT_BLUEMANA && prevMana <= 0)
    {
        P_SetPsprite(plr, ps_weapon, S_FAXEREADY_G);
    } // < FIXME

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_AMMO);

    return true;
}

static boolean giveWeaponWrongClassMana(player_t* plr, playerclass_t weaponClass,
                                        weapontype_t weaponType)
{
    if(IS_NETGAME && !deathmatch)
    {   // Can't pick up wrong-class weapons in coop netplay.
        return false;
    }

    if(!P_GiveMana(plr, (weaponType == WT_SECOND)? AT_BLUEMANA : AT_GREENMANA, 25))
    {   // Didn't need the mana, so don't pick it up.
        return false;
    }

    return true;
}

static boolean giveWeaponPieceWrongClassMana(player_t* plr, playerclass_t pieceClass)
{
    if(IS_NETGAME && !deathmatch)
    {   // Can't pick up wrong-class weapons in coop netplay.
        return false;
    }

    if(!P_GiveMana(plr, AT_BLUEMANA, 20) + P_GiveMana(plr, AT_GREENMANA, 20))
    {   // Didn't need the mana, so don't pick it up.
        return false;
    }

    return true;
}

boolean P_GiveWeapon(player_t* plr, playerclass_t matchClass,
                     weapontype_t weaponType)
{
    boolean             gaveMana = false;

    if(plr->class != matchClass)
        return giveWeaponWrongClassMana(plr, matchClass, weaponType);

    plr->update |= PSF_WEAPONS;

    // Always attempt to give mana unless this a cooperative game and the
    // player already has this weapon piece.
    if(!(IS_NETGAME && !deathmatch && plr->weapons[weaponType].owned))
        gaveMana = P_GiveMana(plr, (weaponType == WT_SECOND)? AT_BLUEMANA : AT_GREENMANA, 25);

    if(!plr->weapons[weaponType].owned)
    {
        plr->weapons[weaponType].owned = true;
        plr->update |= PSF_OWNED_WEAPONS;

        // Maybe unhide the HUD?
        ST_HUDUnHide(plr - players, HUE_ON_PICKUP_WEAPON);

        // Should we change weapon automatically?
        P_MaybeChangeWeapon(plr, weaponType, AT_NOAMMO, false);
    }
    else if(!gaveMana)
    {    // Player didn't need the weapon or any mana.
        return false;
    }

    return true;
}

boolean P_GiveWeaponPiece(player_t* plr, playerclass_t matchClass,
                          int pieceValue)
{
    static int          pieceValueTrans[] = {
        0,                      // 0: never
        WPIECE1 | WPIECE2 | WPIECE3,    // WPIECE1 (1)
        WPIECE2 | WPIECE3,      // WPIECE2 (2)
        0,                      // 3: never
        WPIECE3                 // WPIECE3 (4)
    };
    boolean             gaveMana = false;

    if(plr->class != matchClass)
        return giveWeaponPieceWrongClassMana(plr, matchClass);

    // Always attempt to give mana unless this a cooperative game and the
    // player already has this weapon piece.
    if(!((plr->pieces & pieceValue) && IS_NETGAME && !deathmatch))
    {
        gaveMana = P_GiveMana(plr, AT_BLUEMANA, 20) ||
                   P_GiveMana(plr, AT_GREENMANA, 20);
    }

    if(plr->pieces & pieceValue)
    {   // Already has the piece.
        if(IS_NETGAME && !deathmatch) // Cooperative net-game.
            return false;
        // Deathmatch or single player.
        if(!gaveMana) // Didn't need the mana, so don't pick it up.
            return false;
    }

    // Check if fourth weapon assembled.
    if(IS_NETGAME && !deathmatch) // Cooperative net-game.
        pieceValue = pieceValueTrans[pieceValue];
    if(!(plr->pieces & pieceValue))
    {
        plr->pieces |= pieceValue;
        if(plr->pieces == (WPIECE1 | WPIECE2 | WPIECE3))
        {
            plr->weapons[WT_FOURTH].owned = true;
            plr->pendingWeapon = WT_FOURTH;
            plr->update |= PSF_WEAPONS | PSF_OWNED_WEAPONS;

            // Should we change weapon automatically?
            P_MaybeChangeWeapon(plr, WT_FOURTH, AT_NOAMMO, false);
        }

        // Maybe unhide the HUD?
        ST_HUDUnHide(plr - players, HUE_ON_PICKUP_WEAPON);
    }

    return true;
}

/**
 * @returns             @c false, if the body isn't needed at all.
 */
boolean P_GiveBody(player_t* plr, int num)
{
    int                 max;

    if(plr->morphTics)
        max = MAXMORPHHEALTH;
    else
        max = maxHealth;

    if(plr->health >= max)
    {
        return false;
    }

    plr->health += num;
    if(plr->health > max)
    {
        plr->health = max;
    }
    plr->plr->mo->health = plr->health;
    plr->update |= PSF_HEALTH;

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_HEALTH);

    return true;
}

/**
 * @return              @c true, iff the armor was given.
 */
boolean P_GiveArmor(player_t* plr, armortype_t type, int points)
{
    if(plr->armorPoints[type] >= points)
        return false;

    P_PlayerGiveArmorBonus(plr, type, points - plr->armorPoints[type]);

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_ARMOR);

    return true;
}

/**
 * @return              @c true, iff the armor was given.
 */
boolean P_GiveArmor2(player_t* plr, armortype_t type, int amount)
{
    int             hits, totalArmor;

    hits = amount * 5 * FRACUNIT;
    totalArmor =
        plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET] + PCLASS_INFO(plr->class)->autoArmorSave;
    if(totalArmor >= PCLASS_INFO(plr->class)->maxArmor * 5 * FRACUNIT)
        return false;

    plr->armorPoints[type] += hits;
    plr->update |= PSF_ARMOR;

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_ARMOR);

    return true;
}

int P_GiveKey(player_t *plr, keytype_t key)
{
    if(plr->keys & (1 << key))
    {
        return false;
    }

    plr->bonusCount += BONUSADD;
    plr->keys |= 1 << key;
    plr->update |= PSF_KEYS;

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_KEY);

    return true;
}

/**
 * @return              @c true, if power accepted.
 */
boolean P_GivePower(player_t *plr, powertype_t power)
{
    boolean     retval = false;

    plr->update |= PSF_POWERS;

    switch(power)
    {
    case PT_INVULNERABILITY:
        if(!(plr->powers[power] > BLINKTHRESHOLD))
        {
            plr->powers[power] = INVULNTICS;
            plr->plr->mo->flags2 |= MF2_INVULNERABLE;
            if(plr->class == PCLASS_MAGE)
            {
                plr->plr->mo->flags2 |= MF2_REFLECTIVE;
            }
            retval = true;
        }
        break;

    case PT_FLIGHT:
        if(!(plr->powers[power] > BLINKTHRESHOLD))
        {
            plr->powers[power] = FLIGHTTICS;
            plr->plr->mo->flags2 |= MF2_FLY;
            plr->plr->mo->flags |= MF_NOGRAVITY;
            if(plr->plr->mo->pos[VZ] <= plr->plr->mo->floorZ)
            {
                plr->flyHeight = 10; // Thrust the plr in the air a bit.
                plr->plr->flags |= DDPF_FIXMOM;
            }
            retval = true;
        }
        break;

    case PT_INFRARED:
        if(!(plr->powers[power] > BLINKTHRESHOLD))
        {
            plr->powers[power] = INFRATICS;
            retval = true;
        }
        break;

    case PT_SPEED:
        if(!(plr->powers[power] > BLINKTHRESHOLD))
        {
            plr->powers[power] = SPEEDTICS;
            retval = true;
        }
        break;

    case PT_MINOTAUR:
        // Doesn't matter if already have power, renew ticker.
        plr->powers[power] = MAULATORTICS;
        retval = true;
        break;

    default:
        if(!(plr->powers[power]))
        {
            plr->powers[power] = 1;
            retval = true;
        }
        break;
    }

    if(retval)
    {
        // Maybe unhide the HUD?
        ST_HUDUnHide(plr - players, HUE_ON_PICKUP_POWER);
    }

    return retval;
}

boolean P_GiveItem(player_t* plr, inventoryitemtype_t item)
{
    if(plr)
        return P_InventoryGive(plr - players, item, false);

    return false;
}

/**
 * Removes the MF_SPECIAL flag and initiates the item pickup animation.
 */
static void setDormantItem(mobj_t* mo)
{
    mo->flags &= ~MF_SPECIAL;
    if(deathmatch && !(mo->flags2 & MF2_DROPPED))
    {
        if(mo->type == MT_ARTIINVULNERABILITY)
        {
            P_MobjChangeState(mo, S_DORMANTARTI3_1);
        }
        else if(mo->type == MT_SUMMONMAULATOR || mo->type == MT_ARTIFLY)
        {
            P_MobjChangeState(mo, S_DORMANTARTI2_1);
        }
        else
        {
            P_MobjChangeState(mo, S_DORMANTARTI1_1);
        }
    }
    else
    {   // Don't respawn.
        P_MobjChangeState(mo, S_DEADARTI1);
    }
}

void C_DECL A_RestoreArtifact(mobj_t* mo)
{
    mo->flags |= MF_SPECIAL;
    P_MobjChangeState(mo, P_GetState(mo->type, SN_SPAWN));
    S_StartSound(SFX_RESPAWN, mo);
}

/**
 * Make a special thing visible again.
 */
void C_DECL A_RestoreSpecialThing1(mobj_t* thing)
{
    thing->flags2 &= ~MF2_DONTDRAW;
    S_StartSound(SFX_RESPAWN, thing);
}

void C_DECL A_RestoreSpecialThing2(mobj_t* thing)
{
    thing->flags |= MF_SPECIAL;
    P_MobjChangeState(thing, P_GetState(thing->type, SN_SPAWN));
}

static itemtype_t getItemTypeBySprite(spritetype_e sprite)
{
    static const struct item_s {
        itemtype_t      type;
        spritetype_e    sprite;
    } items[] = {
        { IT_HEALTH_VIAL, SPR_PTN1 },
        { IT_ARMOR_MESH, SPR_ARM1 },
        { IT_ARMOR_SHIELD, SPR_ARM2 },
        { IT_ARMOR_HELMET, SPR_ARM3 },
        { IT_ARMOR_AMULET, SPR_ARM4 },
        { IT_KEY_STEEL, SPR_KEY1 },
        { IT_KEY_CAVE, SPR_KEY2 },
        { IT_KEY_AXE, SPR_KEY3 },
        { IT_KEY_FIRE, SPR_KEY4 },
        { IT_KEY_EMERALD, SPR_KEY5 },
        { IT_KEY_DUNGEON, SPR_KEY6 },
        { IT_KEY_SILVER, SPR_KEY7 },
        { IT_KEY_RUSTED, SPR_KEY8 },
        { IT_KEY_HORN, SPR_KEY9 },
        { IT_KEY_SWAMP, SPR_KEYA },
        { IT_KEY_CASTLE, SPR_KEYB },
        { IT_ITEM_QUARTZFLASK, SPR_PTN2 },
        { IT_ITEM_WINGS, SPR_SOAR },
        { IT_ITEM_DEFENDER, SPR_INVU },
        { IT_ITEM_SERVANT, SPR_SUMN },
        { IT_ITEM_PORKALATOR, SPR_PORK },
        { IT_ITEM_MYSTICURN, SPR_SPHL },
        { IT_ITEM_AMBITINCANT, SPR_HRAD },
        { IT_ITEM_TORCH, SPR_TRCH },
        { IT_ITEM_CHAOSDEVICE, SPR_ATLP },
        { IT_ITEM_BANISHDEVICE, SPR_TELO },
        { IT_ITEM_FLETCHETTE, SPR_PSBG },
        { IT_ITEM_BOOTSOFSPEED, SPR_SPED },
        { IT_ITEM_KRATEROFMIGHT, SPR_BMAN },
        { IT_ITEM_BRACERS, SPR_BRAC },
        { IT_ITEM_REPULSION, SPR_BLST },
        { IT_PUZZLE_SKULL, SPR_ASKU },
        { IT_PUZZLE_BIGGEM, SPR_ABGM },
        { IT_PUZZLE_REDGEM, SPR_AGMR },
        { IT_PUZZLE_GREENGEM1, SPR_AGMG },
        { IT_PUZZLE_GREENGEM2, SPR_AGG2 },
        { IT_PUZZLE_BLUEGEM1, SPR_AGMB },
        { IT_PUZZLE_BLUEGEM2, SPR_AGB2 },
        { IT_PUZZLE_BOOK1, SPR_ABK1 },
        { IT_PUZZLE_BOOK2, SPR_ABK2 },
        { IT_PUZZLE_SKULL2, SPR_ASK2 },
        { IT_PUZZLE_FWEAPON, SPR_AFWP },
        { IT_PUZZLE_CWEAPON, SPR_ACWP },
        { IT_PUZZLE_MWEAPON, SPR_AMWP },
        { IT_PUZZLE_GEAR1, SPR_AGER },
        { IT_PUZZLE_GEAR2, SPR_AGR2 },
        { IT_PUZZLE_GEAR3, SPR_AGR3 },
        { IT_PUZZLE_GEAR4, SPR_AGR4 },
        { IT_MANA_BLUE, SPR_MAN1 },
        { IT_MANA_GREEN, SPR_MAN2 },
        { IT_MANA_COMBINED, SPR_MAN3 },
        { IT_WEAPON_FROSTSHARDS, SPR_WMCS },
        { IT_WEAPON_ARCOFDEATH, SPR_WMLG },
        { IT_WEAPON_AXE, SPR_WFAX },
        { IT_WEAPON_HAMMER, SPR_WFHM },
        { IT_WEAPON_SERPENTSTAFF, SPR_WCSS },
        { IT_WEAPON_FIRESTORM, SPR_WCFM },
        { IT_WEAPON_QUIETUS1, SPR_WFR1 },
        { IT_WEAPON_QUIETUS2, SPR_WFR2 },
        { IT_WEAPON_QUIETUS3, SPR_WFR3 },
        { IT_WEAPON_WRAITHVERGE1, SPR_WCH1 },
        { IT_WEAPON_WRAITHVERGE2, SPR_WCH2 },
        { IT_WEAPON_WRAITHVERGE3, SPR_WCH3 },
        { IT_WEAPON_BLOODSCOURGE1, SPR_WMS1 },
        { IT_WEAPON_BLOODSCOURGE2, SPR_WMS2 },
        { IT_WEAPON_BLOODSCOURGE3, SPR_WMS3 },
        { IT_NONE, 0 }
    };
    uint                i;

    for(i = 0; items[i].type != IT_NONE; ++i)
        if(items[i].sprite == sprite)
            return items[i].type;

    return IT_NONE;
}

static boolean pickupHealthVial(player_t* plr)
{
    return P_GiveBody(plr, 10);
}

static boolean pickupMesh(player_t* plr)
{
    return P_GiveArmor(plr, ARMOR_ARMOR,
                       PCLASS_INFO(plr->class)->armorIncrement[ARMOR_ARMOR]);
}

static boolean pickupShield(player_t* plr)
{
    return P_GiveArmor(plr, ARMOR_SHIELD,
                       PCLASS_INFO(plr->class)->armorIncrement[ARMOR_SHIELD]);
}

static boolean pickupHelmet(player_t* plr)
{
    return P_GiveArmor(plr, ARMOR_HELMET,
                       PCLASS_INFO(plr->class)->armorIncrement[ARMOR_HELMET]);
}

static boolean pickupAmulet(player_t* plr)
{
    return P_GiveArmor(plr, ARMOR_AMULET,
                        PCLASS_INFO(plr->class)->armorIncrement[ARMOR_AMULET]);
}

static boolean pickupSteelKey(player_t* plr)
{
    return P_GiveKey(plr, KT_KEY1);
}

static boolean pickupCaveKey(player_t* plr)
{
    return P_GiveKey(plr, KT_KEY2);
}

static boolean pickupAxeKey(player_t* plr)
{
    return P_GiveKey(plr, KT_KEY3);
}

static boolean pickupFireKey(player_t* plr)
{
    return P_GiveKey(plr, KT_KEY4);
}

static boolean pickupEmeraldKey(player_t* plr)
{
    return P_GiveKey(plr, KT_KEY5);
}

static boolean pickupDungeonKey(player_t* plr)
{
    return P_GiveKey(plr, KT_KEY6);
}

static boolean pickupSilverKey(player_t* plr)
{
    return P_GiveKey(plr, KT_KEY7);
}

static boolean pickupRustedKey(player_t* plr)
{
    return P_GiveKey(plr, KT_KEY8);
}

static boolean pickupHornKey(player_t* plr)
{
    return P_GiveKey(plr, KT_KEY9);
}

static boolean pickupSwampKey(player_t* plr)
{
    return P_GiveKey(plr, KT_KEYA);
}

static boolean pickupCastleKey(player_t* plr)
{
    return P_GiveKey(plr, KT_KEYB);
}

static boolean pickupQuartzFlask(player_t* plr)
{
    return P_GiveItem(plr, IIT_HEALTH);
}

static boolean pickupWings(player_t* plr)
{
    return P_GiveItem(plr, IIT_FLY);
}

static boolean pickupDefender(player_t* plr)
{
    return P_GiveItem(plr, IIT_INVULNERABILITY);
}

static boolean pickupServant(player_t* plr)
{
    return P_GiveItem(plr, IIT_SUMMON);
}

static boolean pickupPorkalator(player_t* plr)
{
    return P_GiveItem(plr, IIT_EGG);
}

static boolean pickupMysticUrn(player_t* plr)
{
    return P_GiveItem(plr, IIT_SUPERHEALTH);
}

static boolean pickupAmbitIncant(player_t* plr)
{
    return P_GiveItem(plr, IIT_HEALINGRADIUS);
}

static boolean pickupTorch(player_t* plr)
{
    return P_GiveItem(plr, IIT_TORCH);
}

static boolean pickupChaosDevice(player_t* plr)
{
    return P_GiveItem(plr, IIT_TELEPORT);
}

static boolean pickupBanishDevice(player_t* plr)
{
    return P_GiveItem(plr, IIT_TELEPORTOTHER);
}

static boolean pickupFletchette(player_t* plr)
{
    return P_GiveItem(plr, IIT_POISONBAG);
}

static boolean pickupBootsOfSpeed(player_t* plr)
{
    return P_GiveItem(plr, IIT_SPEED);
}

static boolean pickupKraterOfMight(player_t* plr)
{
    return P_GiveItem(plr, IIT_BOOSTMANA);
}

static boolean pickupBracers(player_t* plr)
{
    return P_GiveItem(plr, IIT_BOOSTARMOR);
}

static boolean pickupRepulsion(player_t* plr)
{
    return P_GiveItem(plr, IIT_BLASTRADIUS);
}

static boolean pickupSkull(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZSKULL);
}

static boolean pickupBigGem(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZGEMBIG);
}

static boolean pickupRedGem(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZGEMRED);
}

static boolean pickupGreenGem1(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZGEMGREEN1);
}

static boolean pickupGreenGem2(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZGEMGREEN2);
}

static boolean pickupBlueGem1(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZGEMBLUE1);
}

static boolean pickupBlueGem2(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZGEMBLUE2);
}

static boolean pickupBook1(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZBOOK1);
}

static boolean pickupBook2(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZBOOK2);
}

static boolean pickupSkull2(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZSKULL2);
}

static boolean pickupFWeapon(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZFWEAPON);
}

static boolean pickupCWeapon(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZCWEAPON);
}

static boolean pickupMWeapon(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZMWEAPON);
}

static boolean pickupGear1(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZGEAR1);
}

static boolean pickupGear2(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZGEAR2);
}

static boolean pickupGear3(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZGEAR3);
}

static boolean pickupGear4(player_t* plr)
{
    return P_GiveItem(plr, IIT_PUZZGEAR4);
}

static boolean pickupBlueMana(player_t* plr)
{
    return P_GiveMana(plr, AT_BLUEMANA, 15);
}

static boolean pickupGreenMana(player_t* plr)
{
    return P_GiveMana(plr, AT_GREENMANA, 15);
}

static boolean pickupCombinedMana(player_t* plr)
{
    if(!P_GiveMana(plr, AT_BLUEMANA, 20))
    {
        if(!P_GiveMana(plr, AT_GREENMANA, 20))
            return false;
    }
    else
    {
        P_GiveMana(plr, AT_GREENMANA, 20);
    }

    return true;
}

static boolean pickupFrostShards(player_t* plr)
{
    return P_GiveWeapon(plr, PCLASS_MAGE, WT_SECOND);
}

static boolean pickupArcOfDeath(player_t* plr)
{
    return P_GiveWeapon(plr, PCLASS_MAGE, WT_THIRD);
}

static boolean pickupAxe(player_t* plr)
{
    return P_GiveWeapon(plr, PCLASS_FIGHTER, WT_SECOND);
}

static boolean pickupHammer(player_t* plr)
{
    return P_GiveWeapon(plr, PCLASS_FIGHTER, WT_THIRD);
}

static boolean pickupSerpentStaff(player_t* plr)
{
    return P_GiveWeapon(plr, PCLASS_CLERIC, WT_SECOND);
}

static boolean pickupFireStorm(player_t* plr)
{
    return P_GiveWeapon(plr, PCLASS_CLERIC, WT_THIRD);
}

static boolean pickupQuietus1(player_t* plr)
{
    return P_GiveWeaponPiece(plr, PCLASS_FIGHTER, WPIECE1);
}

static boolean pickupQuietus2(player_t* plr)
{
    return P_GiveWeaponPiece(plr, PCLASS_FIGHTER, WPIECE2);
}

static boolean pickupQuietus3(player_t* plr)
{
    return P_GiveWeaponPiece(plr, PCLASS_FIGHTER, WPIECE3);
}

static boolean pickupWraithVerge1(player_t* plr)
{
    return P_GiveWeaponPiece(plr, PCLASS_CLERIC, WPIECE1);
}

static boolean pickupWraithVerge2(player_t* plr)
{
    return P_GiveWeaponPiece(plr, PCLASS_CLERIC, WPIECE2);
}

static boolean pickupWraithVerge3(player_t* plr)
{
    return P_GiveWeaponPiece(plr, PCLASS_CLERIC, WPIECE3);
}

static boolean pickupBloodScourge1(player_t* plr)
{
    return P_GiveWeaponPiece(plr, PCLASS_MAGE, WPIECE1);
}

static boolean pickupBloodScourge2(player_t* plr)
{
    return P_GiveWeaponPiece(plr, PCLASS_MAGE, WPIECE2);
}

static boolean pickupBloodScourge3(player_t* plr)
{
    return P_GiveWeaponPiece(plr, PCLASS_MAGE, WPIECE3);
}

static boolean giveItem(player_t* plr, itemtype_t item)
{
    const iteminfo_t*   info = &items[item];
    int                 oldPieces = plr->pieces;

    if(!plr)
        return false;

    // Attempt to pickup the item.
    if(!info->giveFunc(plr))
        return false; // Did not make use of it.

    switch(item)
    {
    case IT_WEAPON_QUIETUS1:
    case IT_WEAPON_QUIETUS2:
    case IT_WEAPON_QUIETUS3:
    case IT_WEAPON_WRAITHVERGE1:
    case IT_WEAPON_WRAITHVERGE2:
    case IT_WEAPON_WRAITHVERGE3:
    case IT_WEAPON_BLOODSCOURGE1:
    case IT_WEAPON_BLOODSCOURGE2:
    case IT_WEAPON_BLOODSCOURGE3:
        if(plr->pieces != oldPieces &&
           plr->pieces == (WPIECE1 | WPIECE2 | WPIECE3))
        {
            int                 msg;

            switch(item)
            {
            case IT_WEAPON_QUIETUS1:
            case IT_WEAPON_QUIETUS2:
            case IT_WEAPON_QUIETUS3:
                msg = TXT_TXT_WEAPON_F4;
                break;

            case IT_WEAPON_WRAITHVERGE1:
            case IT_WEAPON_WRAITHVERGE2:
            case IT_WEAPON_WRAITHVERGE3:
                msg = TXT_TXT_WEAPON_C4;
                break;

            case IT_WEAPON_BLOODSCOURGE1:
            case IT_WEAPON_BLOODSCOURGE2:
            case IT_WEAPON_BLOODSCOURGE3:
                msg = TXT_TXT_WEAPON_M4;
                break;
            }

            P_SetMessage(plr, GET_TXT(msg), false);
            // Play the build-sound full volume for all players.
            S_StartSound(SFX_WEAPON_BUILD, NULL);
            break;
        }
        // Fall-through:

    default:
        S_StartSound(info->pickupSound, plr->plr->mo);
        P_SetMessage(plr, GET_TXT(info->pickupMsg), false);
        break;
    }

    return true;
}

void P_TouchSpecialMobj(mobj_t* special, mobj_t* toucher)
{
    player_t*           player;
    float               delta;
    itemtype_t          item;
    boolean             wasUsed = false, removeItem = false;

    if(IS_CLIENT)
        return;

    delta = special->pos[VZ] - toucher->pos[VZ];
    if(delta > toucher->height || delta < -32)
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
        const iteminfo_t*   info = &items[item];

        if((wasUsed = giveItem(player, item)))
        {
            // Should we leave this item for others?
            if(!((info->flags & IIF_LEAVE_COOP) && IS_NETGAME && !deathmatch) &&
               !((info->flags & IIF_LEAVE_DEATHMATCH) && IS_NETGAME && deathmatch))
                removeItem = true;
        }
    }
    else
    {
        Con_Message("P_TouchSpecialMobj: Unknown gettable thing %i.\n",
                    (int) special->type);
    }

    if(wasUsed && special->special)
    {
        P_ExecuteLineSpecial(special->special, special->args, NULL, 0,
                             toucher);
        special->special = 0;
    }

    if(removeItem)
    {
        player->bonusCount += BONUSADD;

        /**
         * Taken items are handled differently depending upon the type of
         * item: inventory, puzzle or other.
         */
        switch(item)
        {
        // Inventory:
        case IT_ITEM_QUARTZFLASK:
        case IT_ITEM_WINGS:
        case IT_ITEM_DEFENDER:
        case IT_ITEM_SERVANT:
        case IT_ITEM_PORKALATOR:
        case IT_ITEM_MYSTICURN:
        case IT_ITEM_AMBITINCANT:
        case IT_ITEM_TORCH:
        case IT_ITEM_CHAOSDEVICE:
        case IT_ITEM_BANISHDEVICE:
        case IT_ITEM_FLETCHETTE:
        case IT_ITEM_BOOTSOFSPEED:
        case IT_ITEM_KRATEROFMIGHT:
        case IT_ITEM_BRACERS:
        case IT_ITEM_REPULSION:
            setDormantItem(special);
            break;

        // Puzzle items:
        case IT_PUZZLE_SKULL:
        case IT_PUZZLE_BIGGEM:
        case IT_PUZZLE_REDGEM:
        case IT_PUZZLE_GREENGEM1:
        case IT_PUZZLE_GREENGEM2:
        case IT_PUZZLE_BLUEGEM1:
        case IT_PUZZLE_BLUEGEM2:
        case IT_PUZZLE_BOOK1:
        case IT_PUZZLE_BOOK2:
        case IT_PUZZLE_SKULL2:
        case IT_PUZZLE_FWEAPON:
        case IT_PUZZLE_CWEAPON:
        case IT_PUZZLE_MWEAPON:
        case IT_PUZZLE_GEAR1:
        case IT_PUZZLE_GEAR2:
        case IT_PUZZLE_GEAR3:
        case IT_PUZZLE_GEAR4:
            P_MobjRemove(special, false);
            break;

        default:
            if(deathmatch && !(special->flags2 & MF2_DROPPED))
                P_HideSpecialThing(special);
            else
                P_MobjRemove(special, false);
            break;
        }
    }
}

typedef struct {
    player_t*           master;
    mobj_t*             foundMobj;
} findactiveminotaurparams_t;

static int findActiveMinotaur(void* p, void* context)
{
    findactiveminotaurparams_t* params =
        (findactiveminotaurparams_t*) context;
    mobj_t*             mo = (mobj_t*) p;

    if(mo->type != MT_MINOTAUR)
        return true; // Continue iteration.
    if(mo->health <= 0)
        return true; // Continue iteration.
    if(!(mo->flags & MF_COUNTKILL)) // For morphed minotaurs.
        return true; // Continue iteration.
    if(mo->flags & MF_CORPSE)
        return true; // Continue iteration.

    if((mapTime - *((unsigned int *) mo->args)) >= MAULATORTICS)
        return true; // Continue iteration.

    if(mo->tracer->player == params->master)
    {   // Found it!
        params->foundMobj = mo;
        return false; // Stop iteration.
    }

    return true; // Continue iteration.
}

mobj_t* ActiveMinotaur(player_t* master)
{
    findactiveminotaurparams_t params;

    params.master = master;
    params.foundMobj = NULL;

    if(!DD_IterateThinkers(P_MobjThinker, findActiveMinotaur, &params))
        return params.foundMobj;

    return NULL;
}

void P_KillMobj(mobj_t* source, mobj_t* target)
{
    int                 dummy;
    mobj_t*             master;
    statenum_t          state;

    if(!target)
        return; // Nothing to kill.

    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY | MF_NOGRAVITY);
    target->flags |= MF_CORPSE | MF_DROPOFF;
    target->flags2 &= ~MF2_PASSMOBJ;
    target->height /= 2*2;
    if((target->flags & MF_COUNTKILL || target->type == MT_ZBELL) &&
       target->special)
    {   // Initiate monster death actions.
        if(target->type == MT_SORCBOSS)
        {
            dummy = 0;
            P_StartACS(target->special, 0, (byte *) &dummy, target, NULL, 0);
        }
        else
        {
            P_ExecuteLineSpecial(target->special, target->args, NULL, 0,
                                 target);
        }
    }

    if(source && source->player)
    {   // Check for frag changes.
        if(target->player && deathmatch)
        {
            if(target == source)
            {   // Self-frag.
                target->player->frags[target->player - players]--;
                NetSv_FragsForAll(target->player);
            }
            else
            {
                source->player->frags[target->player - players]++;
                NetSv_FragsForAll(source->player);
            }
        }
    }

    if(target->player)
    {   // Player death.
        if(!source)
        {   // Self-frag
            target->player->frags[target->player - players]--;
            NetSv_FragsForAll(target->player);
        }

        target->flags &= ~MF_SOLID;
        target->flags2 &= ~MF2_FLY;
        target->player->powers[PT_FLIGHT] = 0;
        target->player->playerState = PST_DEAD;
        target->player->rebornWait = PLAYER_REBORN_TICS;
        target->player->update |= PSF_STATE | PSF_POWERS;

        // Let the engine know about this, too. The DEAD flag will be
        // cleared when the player is reborn.
        target->player->plr->flags |= DDPF_DEAD;
        P_DropWeapon(target->player);
        if(target->flags2 & MF2_FIREDAMAGE)
        {   // Player flame death.
            //// \todo Should be pulled from the player class definition.
            switch(target->player->class)
            {
            case PCLASS_FIGHTER:
                S_StartSound(SFX_PLAYER_FIGHTER_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_F_FDTH1);
                return;

            case PCLASS_CLERIC:
                S_StartSound(SFX_PLAYER_CLERIC_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_C_FDTH1);
                return;

            case PCLASS_MAGE:
                S_StartSound(SFX_PLAYER_MAGE_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_M_FDTH1);
                return;

            default:
                break;
            }
        }

        if(target->flags2 & MF2_ICEDAMAGE)
        {   // Player ice death.
            target->flags &= ~(7 << MF_TRANSSHIFT); //no translation
            target->flags |= MF_ICECORPSE;
            //// \todo Should be pulled from the player class definition.
            switch(target->player->class)
            {
            case PCLASS_FIGHTER:
                P_MobjChangeState(target, S_FPLAY_ICE);
                return;

            case PCLASS_CLERIC:
                P_MobjChangeState(target, S_CPLAY_ICE);
                return;

            case PCLASS_MAGE:
                P_MobjChangeState(target, S_MPLAY_ICE);
                return;

            case PCLASS_PIG:
                P_MobjChangeState(target, S_PIG_ICE);
                return;

            default:
                break;
            }
        }

        // Don't die with the automap open.
        AM_Open(AM_MapForPlayer(target->player - players), false, false);
    }
    else
    {   // Target is some monster or an object.

        /**
         * mobj death, record as player's kill in netgame + coop could not
         * find MF_ targets->flags that indicated *only* enemies (not trees,
         * pots, etc), so built a list.
         *
         * \todo This should be a Thing definition flag.
         */
        if(IS_NETGAME && !deathmatch && source && source->player &&
           source->player->plr && (target->type == MT_CENTAUR ||
                                   target->type == MT_CENTAURLEADER ||
                                   target->type == MT_DEMON ||
                                   target->type == MT_DEMON2 ||
                                   target->type == MT_ETTIN ||
                                   target->type == MT_PIG ||
                                   target->type == MT_FIREDEMON ||
                                   target->type == MT_SERPENT ||
                                   target->type == MT_SERPENTLEADER ||
                                   target->type == MT_WRAITH ||
                                   target->type == MT_WRAITHB ||
                                   target->type == MT_BISHOP ||
                                   target->type == MT_ICEGUY ||
                                   target->type == MT_FIGHTER_BOSS ||
                                   target->type == MT_CLERIC_BOSS ||
                                   target->type == MT_MAGE_BOSS ||
                                   target->type == MT_MINOTAUR))
        {
            source->player->frags[0]++;
        }
    }

    if(target->flags2 & MF2_FIREDAMAGE)
    {
        if(target->type == MT_FIGHTER_BOSS || target->type == MT_CLERIC_BOSS ||
           target->type == MT_MAGE_BOSS)
        {
            switch(target->type)
            {
            case MT_FIGHTER_BOSS:
                S_StartSound(SFX_PLAYER_FIGHTER_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_F_FDTH1);
                return;

            case MT_CLERIC_BOSS:
                S_StartSound(SFX_PLAYER_CLERIC_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_C_FDTH1);
                return;

            case MT_MAGE_BOSS:
                S_StartSound(SFX_PLAYER_MAGE_BURN_DEATH, target);
                P_MobjChangeState(target, S_PLAY_M_FDTH1);
                return;

            default:
                break;
            }
        }
        else if(target->type == MT_TREEDESTRUCTIBLE)
        {
            P_MobjChangeState(target, S_ZTREEDES_X1);
            target->height = 24;
            S_StartSound(SFX_TREE_EXPLODE, target);
            return;
        }
    }

    if(target->flags2 & MF2_ICEDAMAGE)
    {
        target->flags |= MF_ICECORPSE;
        switch(target->type)
        {
        case MT_BISHOP:
            P_MobjChangeState(target, S_BISHOP_ICE);
            return;

        case MT_CENTAUR:
        case MT_CENTAURLEADER:
            P_MobjChangeState(target, S_CENTAUR_ICE);
            return;

        case MT_DEMON:
        case MT_DEMON2:
            P_MobjChangeState(target, S_DEMON_ICE);
            return;

        case MT_SERPENT:
        case MT_SERPENTLEADER:
            P_MobjChangeState(target, S_SERPENT_ICE);
            return;

        case MT_WRAITH:
        case MT_WRAITHB:
            P_MobjChangeState(target, S_WRAITH_ICE);
            return;

        case MT_ETTIN:
            P_MobjChangeState(target, S_ETTIN_ICE1);
            return;

        case MT_FIREDEMON:
            P_MobjChangeState(target, S_FIRED_ICE1);
            return;

        case MT_FIGHTER_BOSS:
            P_MobjChangeState(target, S_FIGHTER_ICE);
            return;

        case MT_CLERIC_BOSS:
            P_MobjChangeState(target, S_CLERIC_ICE);
            return;

        case MT_MAGE_BOSS:
            P_MobjChangeState(target, S_MAGE_ICE);
            return;

        case MT_PIG:
            P_MobjChangeState(target, S_PIG_ICE);
            return;

        default:
            target->flags &= ~MF_ICECORPSE;
            break;
        }
    }

    if(target->type == MT_MINOTAUR)
    {
        master = target->tracer;
        if(master && master->health > 0)
        {
            if(!ActiveMinotaur(master->player))
            {
                master->player->powers[PT_MINOTAUR] = 0;
            }
        }
    }
    else if(target->type == MT_TREEDESTRUCTIBLE)
    {
        target->height = 24;
    }

    if((state = P_GetState(target->type, SN_XDEATH)) != S_NULL &&
       target->health < -(target->info->spawnHealth / 2))
    {   // Extreme death.
        P_MobjChangeState(target, state);
    }
    else
    {   // Normal death.
        if((state = P_GetState(target->type, SN_XDEATH)) != S_NULL &&
           target->type == MT_FIREDEMON &&
           target->pos[VZ] <= target->floorZ + 2)
        {
            // This is to fix the imps' staying in fall state.
            P_MobjChangeState(target, state);
        }
        else
        {
            P_MobjChangeState(target, P_GetState(target->type, SN_DEATH));
        }
    }

    target->tics -= P_Random() & 3;
}

/**
 * @return              @c true, if the player gets turned into a pig.
 */
boolean P_MorphPlayer(player_t *player)
{
    mobj_t         *pmo, *fog, *beastMo;
    float           pos[3];
    angle_t         angle;
    int             oldFlags2;

    if(player->powers[PT_INVULNERABILITY])
        return false; // Immune when invulnerable.

    if(player->morphTics)
        return false; // Player is already morphed.

    pmo = player->plr->mo;

    pos[VX] = pmo->pos[VX];
    pos[VY] = pmo->pos[VY];
    pos[VZ] = pmo->pos[VZ];
    angle = pmo->angle;
    oldFlags2 = pmo->flags2;

    if(!(beastMo = P_SpawnMobj3fv(MT_PIGPLAYER, pos, angle, 0)))
        return false;

    P_MobjChangeState(pmo, S_FREETARGMOBJ);

    if((fog = P_SpawnMobj3f(MT_TFOG, pos[VX], pos[VY],
                            pos[VZ] + TELEFOGHEIGHT, angle + ANG180, 0)))
        S_StartSound(SFX_TELEPORT, fog);

    beastMo->special1 = player->readyWeapon;
    beastMo->player = player;
    beastMo->dPlayer = player->plr;

    player->health = beastMo->health = MAXMORPHHEALTH;
    player->plr->mo = beastMo;
    memset(&player->armorPoints[0], 0, NUMARMOR * sizeof(int));
    player->class = PCLASS_PIG;

    if(oldFlags2 & MF2_FLY)
        beastMo->flags2 |= MF2_FLY;

    player->morphTics = MORPHTICS;
    player->update |= PSF_MORPH_TIME | PSF_HEALTH;
    player->plr->flags |= DDPF_FIXPOS | DDPF_FIXMOM;
    P_ActivateMorphWeapon(player);
    return true;
}

boolean P_MorphMonster(mobj_t *actor)
{
    mobj_t*         master, *monster, *fog;
    mobjtype_t      moType;
    float           pos[3];
    mobj_t          oldMonster;
    angle_t         oldAngle;

    if(actor->player)
        return (false);
    if(!(actor->flags & MF_COUNTKILL))
        return false;
    if(actor->flags2 & MF2_BOSS)
        return false;

    moType = actor->type;
    switch(moType)
    {
    case MT_PIG:
    case MT_FIGHTER_BOSS:
    case MT_CLERIC_BOSS:
    case MT_MAGE_BOSS:
        return false;

    default:
        break;
    }

    //// \fixme Do this properly!
    oldMonster = *actor;

    pos[VX] = actor->pos[VX];
    pos[VY] = actor->pos[VY];
    pos[VZ] = actor->pos[VZ];
    oldAngle = actor->angle;

    if(!(monster = P_SpawnMobj3fv(MT_PIG, pos, oldMonster.angle, 0)))
        return false;

    P_MobjRemoveFromTIDList(actor);
    P_MobjChangeState(actor, S_FREETARGMOBJ);

    if((fog = P_SpawnMobj3f(MT_TFOG, pos[VX], pos[VY],
                            pos[VZ] + TELEFOGHEIGHT, oldAngle + ANG180, 0)))
        S_StartSound(SFX_TELEPORT, fog);

    monster->special2 = moType;
    monster->special1 = MORPHTICS + P_Random();
    monster->flags |= (oldMonster.flags & MF_SHADOW);
    monster->target = oldMonster.target;
    monster->tid = oldMonster.tid;
    monster->special = oldMonster.special;
    P_MobjInsertIntoTIDList(monster, oldMonster.tid);
    memcpy(monster->args, oldMonster.args, 5);

    // Check for turning off minotaur power for active icon.
    if(moType == MT_MINOTAUR)
    {
        master = oldMonster.tracer;
        if(master->health > 0)
        {
            if(!ActiveMinotaur(master->player))
            {
                master->player->powers[PT_MINOTAUR] = 0;
            }
        }
    }

    return true;
}

void P_AutoUseHealth(player_t* player, int saveHealth)
{
    uint                i, count;
    int                 plrnum = player - players;
    int                 normalCount = P_InventoryCount(plrnum, IIT_HEALTH);
    int                 superCount = P_InventoryCount(plrnum, IIT_SUPERHEALTH);

    //// \todo Do this in the inventory code?
    if(gameSkill == SM_BABY && normalCount * 25 >= saveHealth)
    {
        // Use quartz flasks.
        count = (saveHealth + 24) / 25;
        for(i = 0; i < count; ++i)
        {
            player->health += 25;
            P_InventoryTake(plrnum, IIT_HEALTH, false);
        }
    }
    else if(superCount * 100 >= saveHealth)
    {
        // Use mystic urns.
        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; ++i)
        {
            player->health += 100;
            P_InventoryTake(plrnum, IIT_SUPERHEALTH, false);
        }
    }
    else if(gameSkill == SM_BABY &&
            superCount * 100 + normalCount * 25 >= saveHealth)
    {
        // Use mystic urns and quartz flasks.
        count = (saveHealth + 24) / 25;
        saveHealth -= count * 25;
        for(i = 0; i < count; ++i)
        {
            player->health += 25;
            P_InventoryTake(plrnum, IIT_HEALTH, false);
        }

        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; ++i)
        {
            player->health += 100;
            P_InventoryTake(plrnum, IIT_SUPERHEALTH, false);
        }
    }

    player->plr->mo->health = player->health;
}

/**
 * Sets up all data concerning poisoning.
 */
void P_PoisonPlayer(player_t *player, mobj_t *poisoner, int poison)
{
    if((P_GetPlayerCheats(player) & CF_GODMODE) ||
       player->powers[PT_INVULNERABILITY])
        return;

    player->poisonCount += poison;
    player->poisoner = poisoner;

    if(player->poisonCount > 100)
        player->poisonCount = 100;
}

/**
 * Damages both enemies and players
 * \note 'source' and 'inflictor' are the same for melee attacks.
 * 'source' can be NULL for slime, barrel explosions and other environmental
 * stuff.
 *
 * @param inflictor         Is the mobj that caused the damage, creature or
 *                          missile, can be @c NULL (slime, etc).
 * @param source            Is the mobj to target after taking damage
 *                          creature or @c NULL.
 */
int P_DamageMobj(mobj_t* target, mobj_t* inflictor, mobj_t* source,
                 int damageP, boolean stomping)
{
    uint            an;
    angle_t         angle;
    int             i, temp, originalHealth;
    float           thrust, saved, savedPercent;
    player_t*       player;
    mobj_t*         master;
    int             damage;

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
        return 0; // Shouldn't happen.

    if(target->health <= 0)
    {
        if(!(inflictor && (inflictor->flags2 & MF2_ICEDAMAGE)) &&
           (target->flags & MF_ICECORPSE))
        {   // Frozen.
            target->tics = 1;
            target->mom[MX] = target->mom[MY] = 0;
        }

        return 0;
    }

    if((target->flags2 & MF2_INVULNERABLE) && damage < 10000)
    {   // mobj is invulnerable.
        if(target->player)
            return 0; // for player, no exceptions.

        if(!inflictor)
            return 0;

        switch(inflictor->type)
        {
        // These inflictors aren't foiled by invulnerability.
        case MT_HOLY_FX:
        case MT_POISONCLOUD:
        case MT_FIREBOMB:
            break;

        default:
            return 0;
        }
    }

    if(target->player)
    {   // Player specific.
        // Check if player-player damage is disabled.
        if(source && source->player && source->player != target->player)
        {
#if 0
            // Co-op damage disabled?
            if(IS_NETGAME && !deathmatch && cfg.noCoopDamage)
                return 0;

            // Same color, no damage?
            if(cfg.noTeamDamage &&
               cfg.playerColor[target->player - players] ==
               cfg.playerColor[source->player - players])
                return 0;
#endif
        }
    }

    if(target->flags & MF_SKULLFLY)
    {
        target->mom[MX] = target->mom[MY] = target->mom[MZ] = 0;
    }

    if(target->flags2 & MF2_DORMANT)
        return 0; // Invulnerable, and won't wake up.

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

    // Special damage types.
    if(inflictor)
    {
        switch(inflictor->type)
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
            return 0; // Does no actual "damage" but health IS modified.

        case MT_TELOTHER_FX1:
        case MT_TELOTHER_FX2:
        case MT_TELOTHER_FX3:
        case MT_TELOTHER_FX4:
        case MT_TELOTHER_FX5:
            if((target->flags & MF_COUNTKILL) && (target->type != MT_SERPENT)
               && (target->type != MT_SERPENTLEADER) &&
               (!(target->flags2 & MF2_BOSS)))
            {
                if(target->player)
                {
                    if(deathmatch)
                        P_TeleportToDeathmatchStarts(target);
                    else
                        P_TeleportToPlayerStarts(target);
                }
                else
                {
                    // If death action, run it upon teleport.
                    if(target->flags & MF_COUNTKILL && target->special)
                    {
                        P_MobjRemoveFromTIDList(target);
                        P_ExecuteLineSpecial(target->special, target->args, NULL, 0,
                                             target);
                        target->special = 0;
                    }

                    // Send all monsters to deathmatch spots.
                    P_TeleportToDeathmatchStarts(target);
                }
            }
            return 0;

        case MT_MINOTAUR:
            if(inflictor->flags & MF_SKULLFLY)
            {   // Slam only when in charge mode.
                uint            an;
                int             damageDone;
                angle_t         angle;
                float           thrust;

                angle = R_PointToAngle2(inflictor->pos[VX], inflictor->pos[VY],
                                        target->pos[VX], target->pos[VY]);

                an = angle >> ANGLETOFINESHIFT;
                thrust = 16 + FIX2FLT(P_Random() << 10);
                target->mom[MX] += thrust * FIX2FLT(finecosine[an]);
                target->mom[MY] += thrust * FIX2FLT(finesine[an]);
                damageDone = P_DamageMobj(target, NULL, inflictor, HITDICE(4), false);
                if(target->player)
                {
                    target->reactionTime = 14 + (P_Random() & 7);
                }

                inflictor->args[0] = 0; // Stop charging.
                return damageDone;
            }
            break;

        case MT_BISH_FX:
            // Bishops are just too nasty.
            damage /= 2;
            break;

        case MT_SHARDFX1:
            switch(inflictor->special2)
            {
            case 3:
                damage *= 8;
                break;

            case 2:
                damage *= 4;
                break;

            case 1:
                damage *= 2;
                break;

            default:
                break;
            }
            break;

        case MT_CSTAFF_MISSILE:
            // Cleric Serpent Staff does poison damage.
            if(target->player)
            {
                P_PoisonPlayer(target->player, source, 20);
                damage /= 2;
            }
            break;

        case MT_ICEGUY_FX2:
            damage /= 2;
            break;

        case MT_POISONDART:
            if(target->player)
            {
                P_PoisonPlayer(target->player, source, 20);
                damage /= 2;
            }
            break;

        case MT_POISONCLOUD:
            if(target->player)
            {
                int                 damageDone = 0;

                if(target->player->poisonCount < 4)
                {
                    damageDone = P_PoisonDamage(target->player, source, 15 + (P_Random() & 15), false);  // Don't play painsound
                    P_PoisonPlayer(target->player, source, 50);
                    S_StartSound(SFX_PLAYER_POISONCOUGH, target);
                }

                return damageDone;
            }
            else if(!(target->flags & MF_COUNTKILL))
            {   // Only damage monsters/players with the poison cloud.
                return 0;
            }
            break;

        case MT_FSWORD_MISSILE:
            if(target->player)
            {
                damage -= damage / 4;
            }
            break;

        default:
            break;
        }
    }

    // Some close combat weapons should not inflict thrust and push the
    // victim out of reach, thus kick away unless using a melee weapon.
    if(inflictor && (!source || !source->player) &&
       !(inflictor->flags2 & MF2_NODMGTHRUST))
    {
        angle =
            R_PointToAngle2(inflictor->pos[VX], inflictor->pos[VY],
                            target->pos[VX], target->pos[VY]);

        if(!target->info->mass)
            Con_Error("P_DamageMobj: No target->info->mass!\n");

        thrust = FIX2FLT(damage * (FRACUNIT>>3) * 100 / target->info->mass);

        // Make fall forwards sometimes.
        if((damage < 40) && (damage > target->health) &&
           (target->pos[VZ] - inflictor->pos[VZ] > 64) && (P_Random() & 1))
        {
            angle += ANG180;
            thrust *= 4;
        }

        an = angle >>ANGLETOFINESHIFT;
        target->mom[MX] += thrust * FIX2FLT(finecosine[an]);
        target->mom[MY] += thrust * FIX2FLT(finesine[an]);
        if(target->dPlayer)
        {
            // Only fix momentum. Otherwise clients will find it difficult
            // to escape from the damage inflictor.
            target->dPlayer->flags |= DDPF_FIXMOM;
        }
    }

    // Player specific.
    if(player)
    {
        target->player->update |= PSF_HEALTH;

        if(damage < 1000 &&
           ((P_GetPlayerCheats(target->player) & CF_GODMODE) ||
            target->player->powers[PT_INVULNERABILITY]))
        {
            return 0;
        }

        savedPercent = FIX2FLT(
            PCLASS_INFO(player->class)->autoArmorSave + player->armorPoints[ARMOR_ARMOR] +
            player->armorPoints[ARMOR_SHIELD] +
            player->armorPoints[ARMOR_HELMET] +
            player->armorPoints[ARMOR_AMULET]);
        if(savedPercent)
        {   // Armor absorbed some damage.
            if(savedPercent > 100)
            {
                savedPercent = 100;
            }

            for(i = 0; i < NUMARMOR; ++i)
            {
                if(player->armorPoints[i])
                {
                    player->armorPoints[i] -=
                        FLT2FIX(((float) damage * FIX2FLT(PCLASS_INFO(player->class)->armorIncrement[i])) /
                                 300);

                    if(player->armorPoints[i] < 2 * FRACUNIT)
                    {
                        player->armorPoints[i] = 0;
                    }
                }
            }
            saved = ((float) damage * savedPercent) / 100;

            if(saved > savedPercent * 2)
                saved = savedPercent * 2;

            damage -= (int) saved;
        }

        if(damage >= player->health &&
           ((gameSkill == SM_BABY) || deathmatch) && !player->morphTics)
        {   // Try to use some inventory health.
            P_AutoUseHealth(player, damage - player->health + 1);
        }

        player->health -= damage;

        if(player->health < 0)
            player->health = 0;

        player->attacker = source;
        player->damageCount += damage; // Add damage after armor / invuln.

        if(player->damageCount > 100)
            player->damageCount = 100; // Teleport stomp does 10k points...

        temp = (damage < 100 ? damage : 100);

        // Maybe unhide the HUD?
        ST_HUDUnHide(player - players, HUE_ON_DAMAGE);

        ST_doPaletteStuff(player - players, false);
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
            if(inflictor &&
               (inflictor->type >= MT_LIGHTNING_FLOOR &&
                inflictor->type <= MT_LIGHTNING_ZAP))
            {
                if(P_Random() < 96)
                {
                    statenum_t          state;

                    target->flags |= MF_JUSTHIT; // fight back!

                    if((state = P_GetState(target->type, SN_PAIN)) != S_NULL)
                        P_MobjChangeState(target, state);
                }
                else
                {   // "electrocute" the target.
    //// \fixme make fullbright for this frame -->
                    //target->frame |= FF_FULLBRIGHT;
    // <-- fixme
                    if((target->flags & MF_COUNTKILL) && P_Random() < 128 &&
                       !S_IsPlaying(SFX_PUPPYBEAT, target))
                    {
                        if((target->type == MT_CENTAUR) ||
                           (target->type == MT_CENTAURLEADER) ||
                           (target->type == MT_ETTIN))
                        {
                            S_StartSound(SFX_PUPPYBEAT, target);
                        }
                    }
                }
            }
            else
            {
                statenum_t          state;

                target->flags |= MF_JUSTHIT; // fight back!

                if((state = P_GetState(target->type, SN_PAIN)) != S_NULL)
                    P_MobjChangeState(target, state);

                if(inflictor && inflictor->type == MT_POISONCLOUD)
                {
                    if(target->flags & MF_COUNTKILL && P_Random() < 128 &&
                       !S_IsPlaying(SFX_PUPPYBEAT, target))
                    {
                        if((target->type == MT_CENTAUR) ||
                           (target->type == MT_CENTAURLEADER) ||
                           (target->type == MT_ETTIN))
                        {
                            S_StartSound(SFX_PUPPYBEAT, target);
                        }
                    }
                }
            }
        }

        target->reactionTime = 0; // We're awake now...

        if(!target->threshold && source && !(source->flags3 & MF3_NOINFIGHT) &&
           !(target->type == MT_BISHOP) && !(target->type == MT_MINOTAUR))
        {
            // Target is not intent on another, so make it chase source.
            if(!((target->type == MT_CENTAUR && source->type == MT_CENTAURLEADER) ||
                 (target->type == MT_CENTAURLEADER && source->type == MT_CENTAUR)))
            {
                statenum_t          state;

                target->target = source;
                target->threshold = BASETHRESHOLD;

                if((state = P_GetState(target->type, SN_SEE)) != S_NULL &&
                   target->state == &STATES[P_GetState(target->type, SN_SPAWN)])
                {
                    P_MobjChangeState(target, state);
                }
            }
        }
    }
    else
    {   // Death.
        if(inflictor)
        {   // Check for special fire damage or ice damage deaths.
            if(inflictor->flags2 & MF2_FIREDAMAGE)
            {
                if(player && !player->morphTics)
                {   // Check for flame death.
                    if(target->health > -50 && damage > 25)
                    {
                        target->flags2 |= MF2_FIREDAMAGE;
                    }
                }
                else
                {
                    target->flags2 |= MF2_FIREDAMAGE;
                }
            }
            else if(inflictor->flags2 & MF2_ICEDAMAGE)
            {
                target->flags2 |= MF2_ICEDAMAGE;
            }
        }

        if(source && (source->type == MT_MINOTAUR))
        {   // Minotaur's kills go to his master.
            master = source->tracer;
            // Make sure still alive and not a pointer to fighter head.
            if(master->player && (master->player->plr->mo == master))
            {
                source = master;
            }
        }

        if(source && (source->player) &&
           (source->player->readyWeapon == WT_FOURTH))
        {
            // Always extreme death from fourth weapon.
            target->health = -5000;
        }

        P_KillMobj(source, target);
    }

    return originalHealth - target->health;
}

int P_FallingDamage(player_t *player)
{
    int             damage;
    float           mom, dist;

    mom = fabs(player->plr->mo->mom[MZ]);
    dist = mom * (16.0f / 23);

    if(mom >= 63)
    {   // Automatic death.
        return P_DamageMobj(player->plr->mo, NULL, NULL, 10000, false);
    }

    damage = ((dist * dist) / 10) - 24;
    if(player->plr->mo->mom[MZ] > -39 && damage > player->plr->mo->health &&
       player->plr->mo->health != 1)
    {   // No-death threshold.
        damage = player->plr->mo->health - 1;
    }

    S_StartSound(SFX_PLAYER_LAND, player->plr->mo);

    return P_DamageMobj(player->plr->mo, NULL, NULL, damage, false);
}

int P_PoisonDamage(player_t* player, mobj_t* source, int damage,
                   boolean playPainSound)
{
    int             originalHealth;
    mobj_t*         target, *inflictor;

    target = player->plr->mo;
    originalHealth = target->health;
    inflictor = source;

    if(target->health <= 0)
        return 0; // Already dead.

    if((target->flags2 & MF2_INVULNERABLE) && damage < 10000)
        return 0; // mobj is invulnerable.

    if(gameSkill == SM_BABY)
    {   // Take half damage in trainer mode
        damage /= 2;
    }

    if(damage < 1000 &&
       ((P_GetPlayerCheats(player) & CF_GODMODE) || player->powers[PT_INVULNERABILITY]))
    {
        return 0;
    }

    if(damage >= player->health && ((gameSkill == SM_BABY) || deathmatch) &&
       !player->morphTics)
    {   // Try to use some inventory health.
        P_AutoUseHealth(player, damage - player->health + 1);
    }

    // Maybe unhide the HUD?
    ST_HUDUnHide(player - players, HUE_ON_DAMAGE);

    player->health -= damage;
    if(player->health < 0)
    {
        player->health = 0;
    }
    player->attacker = source;

    // Do the damage.
    target->health -= damage;
    if(target->health > 0)
    {   // Still alive, phew!
        if(!(mapTime & 63) && playPainSound)
        {
            statenum_t          state;

            if((state = P_GetState(target->type, SN_PAIN)) != S_NULL)
                P_MobjChangeState(target, state);
        }
    }
    else
    {   // Death
        target->special1 = damage;
        if(player && inflictor && !player->morphTics)
        {   // Check for flame death.
            if((inflictor->flags2 & MF2_FIREDAMAGE) &&
               (target->health > -50) && (damage > 25))
            {
                target->flags2 |= MF2_FIREDAMAGE;
            }

            if(inflictor->flags2 & MF2_ICEDAMAGE)
            {
                target->flags2 |= MF2_ICEDAMAGE;
            }
        }

        P_KillMobj(source, target);
    }

    return originalHealth - target->health;
}
