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
 * m_cheat.c: Cheat sequence checking.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "jdoom.h"

#include "f_infine.h"
#include "d_net.h"
#include "g_common.h"
#include "p_player.h"
#include "am_map.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "dmu_lib.h"
#include "p_user.h"
#include "p_start.h"
#include "g_eventsequence.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int Cht_GodFunc(const int* args, int player);
int Cht_NoClipFunc(const int* args, int player);
int Cht_ChoppersFunc(const int* args, int player);
int Cht_MyPosFunc(const int* args, int player);
int Cht_GiveWeaponsAmmoArmorKeys(const int* args, int player);
int Cht_GiveWeaponsAmmoArmor(const int* args, int player);
int Cht_WarpFunc(const int* args, int player);
int Cht_MusicFunc(const int* args, int player);
int Cht_PowerupMessage(const int* args, int player);
int Cht_PowerupFunc(const int* args, int player);
int Cht_Reveal(const int* args, int player);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static unsigned char cheatMusSeq[] = {
    'i', 'd', 'm', 'u', 's', 1, 0, 0
};

static unsigned char cheatChoppersSeq[] = {
    'i', 'd', 'c', 'h', 'o', 'p', 'p', 'e', 'r', 's'
};

static unsigned char cheatGodSeq[] = {
    'i', 'd', 'd', 'q', 'd'
};

static unsigned char cheatAmmoSeq[] = {
    'i', 'd', 'k', 'f', 'a'
};

static unsigned char cheatAmmoNoKeySeq[] = {
    'i', 'd', 'f', 'a'
};

static unsigned char cheatNoClipSeq[] = {
    'i', 'd', 's', 'p', 'i', 's', 'p', 'o', 'p', 'd'
};

static unsigned char cheatCommercialNoClipSeq[] = {
    'i', 'd', 'c', 'l', 'i', 'p'
};

static unsigned char cheatPowerupSeq[] = {
    'i', 'd', 'b', 'e', 'h', 'o', 'l', 'd'
};

static unsigned char cheatPowerupSeq1[] = {
    'i', 'd', 'b', 'e', 'h', 'o', 'l', 'd', 1, 0
};

static unsigned char cheatChangeMapSeq[] = {
    'i', 'd', 'c', 'l', 'e', 'v', 1, 0, 0
};

static unsigned char cheatMyPosSeq[] = {
    'i', 'd', 'm', 'y', 'p', 'o', 's'
};

static unsigned char cheatAutomapSeq[] = {
    'i', 'd', 'd', 't'
};

// CODE --------------------------------------------------------------------

static boolean cheatsEnabled(void)
{
    return !IS_NETGAME;
}

void Cht_Init(void)
{
    G_AddEventSequence(cheatAutomapSeq, sizeof(cheatAutomapSeq), Cht_Reveal);
    G_AddEventSequence(cheatChangeMapSeq, sizeof(cheatChangeMapSeq), Cht_WarpFunc);
    G_AddEventSequence(cheatGodSeq, sizeof(cheatGodSeq), Cht_GodFunc);
    G_AddEventSequence(cheatAmmoNoKeySeq, sizeof(cheatAmmoNoKeySeq), Cht_GiveWeaponsAmmoArmor);
    G_AddEventSequence(cheatAmmoSeq, sizeof(cheatAmmoSeq), Cht_GiveWeaponsAmmoArmorKeys);
    G_AddEventSequence(cheatMusSeq, sizeof(cheatMusSeq), Cht_MusicFunc);
    G_AddEventSequence(cheatNoClipSeq, sizeof(cheatNoClipSeq), Cht_NoClipFunc);
    G_AddEventSequence(cheatCommercialNoClipSeq, sizeof(cheatCommercialNoClipSeq), Cht_NoClipFunc);
    G_AddEventSequence(cheatPowerupSeq1, sizeof(cheatPowerupSeq1), Cht_PowerupFunc);
    G_AddEventSequence(cheatPowerupSeq, sizeof(cheatPowerupSeq), Cht_PowerupMessage);
    G_AddEventSequence(cheatChoppersSeq, sizeof(cheatChoppersSeq), Cht_ChoppersFunc);
    G_AddEventSequence(cheatMyPosSeq, sizeof(cheatMyPosSeq), Cht_MyPosFunc);
}

/**
 * 'iddqd' cheat for toggleable god mode.
 */
int Cht_GodFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
#if !_DEBUG
    if(gameSkill == SM_NIGHTMARE)
        return false;
#endif
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    plr->cheats ^= CF_GODMODE;
    plr->update |= PSF_STATE;

    if(P_GetPlayerCheats(plr) & CF_GODMODE)
    {
        if(plr->plr->mo)
            plr->plr->mo->health = maxHealth;
        plr->health = godModeHealth;
        plr->update |= PSF_HEALTH;
    }

    P_SetMessage(plr,
                 ((P_GetPlayerCheats(plr) & CF_GODMODE) ? STSTR_DQDON : STSTR_DQDOFF), false);
    return true;
}

static void giveArmor(int player, int val)
{
    player_t* plr = &players[player];

    // Support idfa/idkfa DEH Misc values
    val = MINMAX_OF(1, val, 3);
    plr->armorPoints = armorPoints[val];
    plr->armorType = armorClass[val];

    plr->update |= PSF_STATE | PSF_ARMOR_POINTS;
}

static void giveWeapons(player_t* plr)
{
    int i;

    plr->update |= PSF_OWNED_WEAPONS;
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        plr->weapons[i].owned = true;
    }
}

static void giveAmmo(player_t* plr)
{
    int i;

    plr->update |= PSF_AMMO;
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
        plr->ammo[i].owned = plr->ammo[i].max;
}

static void giveKeys(player_t* plr)
{
    int i;

    plr->update |= PSF_KEYS;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
        plr->keys[i] = true;
}

/**
 * 'idfa' cheat for killer fucking arsenal.
 */
int Cht_GiveWeaponsAmmoArmor(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
#if !_DEBUG
    if(gameSkill == SM_NIGHTMARE)
        return false;
#endif
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    giveWeapons(plr);
    giveAmmo(plr);
    giveArmor(player, 2);

    P_SetMessage(plr, STSTR_FAADDED, false);
    return true;
}

/**
 * 'idkfa' cheat for key full ammo.
 */
int Cht_GiveWeaponsAmmoArmorKeys(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
#if !_DEBUG
    if(gameSkill == SM_NIGHTMARE)
        return false;
#endif
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    giveWeapons(plr);
    giveAmmo(plr);
    giveKeys(plr);
    giveArmor(player, 3);

    P_SetMessage(plr, STSTR_KFAADDED, false);
    return true;
}

/**
 * 'idmus' cheat for changing music.
 */
int Cht_MusicFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    int musnum;

    if(IS_NETGAME)
        return false;
#if !_DEBUG
    if(gameSkill == SM_NIGHTMARE)
        return false;
#endif
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    musnum = (args[0] - '0') * 10 + (args[1] - '0');

    if(S_StartMusicNum(musnum, true))
    {
        P_SetMessage(plr, STSTR_MUS, false);
        return true;
    }

    P_SetMessage(plr, STSTR_NOMUS, false);
    return false;
}

/**
 * 'idclip' no clipping mode cheat.
 */
int Cht_NoClipFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
#if !_DEBUG
    if(gameSkill == SM_NIGHTMARE)
        return false;
#endif
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    plr->cheats ^= CF_NOCLIP;
    plr->update |= PSF_STATE;
    P_SetMessage(plr,
                 ((P_GetPlayerCheats(plr) & CF_NOCLIP) ? STSTR_NCON : STSTR_NCOFF), false);
    return true;
}

/**
 * 'clev' change map cheat.
 */
int Cht_WarpFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    uint epsd, map;

    if(IS_NETGAME)
        return false;

    if(gameMode == commercial)
    {
        epsd = 0;
        map = (args[0] - '0') * 10 + args[1] - '0';
        if(map != 0) map -= 1;
    }
    else
    {
        epsd = (args[0] > '0')? args[0] - '1' : 0;
        map = (args[1] > '0')? args[1] - '1' : 0;
    }

    // Catch invalid maps.
    if(!G_ValidateMap(&epsd, &map))
        return false;

    P_SetMessage(plr, STSTR_CLEV, false);

    // Clear the menu if open.
    Hu_MenuCommand(MCMD_CLOSE);

    // So be it.
    briefDisabled = true;
    G_DeferedInitNew(gameSkill, epsd, map);

    return true;
}

int Cht_Reveal(const int* args, int player)
{
    player_t* plr = &players[player];
    automapid_t map;

    if(IS_NETGAME && deathmatch)
        return false;
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    map = AM_MapForPlayer(plr - players);
    if(AM_IsActive(map))
    {
        AM_IncMapCheatLevel(map);
    }

    return true;
}

/**
 * 'idbehold' power-up menu
 */
int Cht_PowerupMessage(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
#if !_DEBUG
    if(gameSkill == SM_NIGHTMARE)
        return false;
#endif
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    P_SetMessage(plr, STSTR_BEHOLD, false);
    return true;
}

static void givePower(player_t* plr, powertype_t type)
{
    if(type < 0 && type >= NUM_POWER_TYPES)
        return;

    if(!plr->powers[type])
    {
        P_GivePower(plr, type);
    }
    else if(type == PT_STRENGTH || type == PT_FLIGHT || type == PT_ALLMAP)
    {
        P_TakePower(plr, type);
    }
}

/**
 * 'idbehold?' power-up cheats.
 */
int Cht_PowerupFunc(const int* args, int player)
{
    static const char values[] = { 'v', 's', 'i', 'r', 'a', 'l' };
    static const size_t numValues = sizeof(values) / sizeof(values[0]);

    player_t* plr = &players[player];
    size_t i;

    if(IS_NETGAME)
        return false;
#if !_DEBUG
    if(gameSkill == SM_NIGHTMARE)
        return false;
#endif
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    for(i = 0; i < numValues; ++i)
    {
        if(args[0] != values[i])
            continue;
        givePower(plr, (powertype_t) i);
        P_SetMessage(plr, STSTR_BEHOLDX, false);
        return true;
    }

    return false;
}

/**
 * 'choppers' invulnerability & chainsaw
 */
int Cht_ChoppersFunc(const int* args, int player)
{
    player_t* plr = &players[player];

    if(IS_NETGAME)
        return false;
#if !_DEBUG
    if(gameSkill == SM_NIGHTMARE)
        return false;
#endif
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    plr->weapons[WT_EIGHTH].owned = true;
    plr->powers[PT_INVULNERABILITY] = true;
    P_SetMessage(plr, STSTR_CHOPPERS, false);
    return true;
}

/**
 * 'mypos' for plr position
 */
int Cht_MyPosFunc(const int* args, int player)
{
    player_t* plr = &players[player];
    char buf[80];

    if(IS_NETGAME)
        return false;
#if !_DEBUG
    if(gameSkill == SM_NIGHTMARE)
        return false;
#endif
    if(plr->health <= 0)
        return false; // Dead players can't cheat.

    sprintf(buf, "ang=0x%x;x,y,z=(%g,%g,%g)",
            players[CONSOLEPLAYER].plr->mo->angle,
            players[CONSOLEPLAYER].plr->mo->pos[VX],
            players[CONSOLEPLAYER].plr->mo->pos[VY],
            players[CONSOLEPLAYER].plr->mo->pos[VZ]);
    P_SetMessage(plr, buf, false);
    return true;
}

static void printDebugInfo(player_t* plr)
{
    char lumpName[9], textBuffer[256];
    subsector_t* sub;

    if(!plr->plr->mo || !userGame)
        return;

    P_GetMapLumpName(lumpName, gameEpisode, gameMap);
    sprintf(textBuffer, "MAP [%s]  X:%g  Y:%g  Z:%g",
            lumpName, plr->plr->mo->pos[VX], plr->plr->mo->pos[VY],
            plr->plr->mo->pos[VZ]);
    P_SetMessage(plr, textBuffer, false);

    // Also print some information to the console.
    Con_Message(textBuffer);
    sub = plr->plr->mo->subsector;
    Con_Message("\nSubsector %i:\n", P_ToIndex(sub));
    Con_Message("  FloorZ:%g Material:%s\n",
                DMU_GetFloatp(sub, DMU_FLOOR_HEIGHT),
                DMU_GetPtrp(DMU_GetPtrp(sub, DMU_FLOOR_MATERIAL), DMU_NAME));
    Con_Message("  CeilingZ:%g Material:%s\n",
                DMU_GetFloatp(sub, DMU_CEILING_HEIGHT),
                DMU_GetPtrp(DMU_GetPtrp(sub, DMU_CEILING_MATERIAL), DMU_NAME));
    Con_Message("Player height:%g   Player radius:%g\n",
                plr->plr->mo->height, plr->plr->mo->radius);
}

/**
 * This is the multipurpose cheat ccmd.
 */
DEFCC(CCmdCheat)
{
    size_t i;

    // Give each of the characters in argument two to the ST event handler.
    for(i = 0; i < strlen(argv[1]); ++i)
    {
        event_t ev;

        ev.type = EV_KEY;
        ev.state = EVS_DOWN;
        ev.data1 = argv[1][i];
        ev.data2 = ev.data3 = 0;
        G_EventSequenceResponder(&ev);
    }
    return true;
}

DEFCC(CCmdCheatGod)
{
    if(G_GetGameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("god");
        }
        else
        {
            int player = CONSOLEPLAYER;

            if(IS_NETGAME && !netSvAllowCheats)
                return false;

            if(argc == 2)
            {
                player = atoi(argv[1]);
                if(player < 0 || player >= MAXPLAYERS)
                    return false;
            }

            if(!players[player].plr->inGame)
                return false;

            Cht_GodFunc(NULL, player);
        }
    }
    return true;
}

DEFCC(CCmdCheatNoClip)
{
    if(G_GetGameState() == GS_MAP)
    {
        if(IS_CLIENT)
        {
            NetCl_CheatRequest("noclip");
        }
        else
        {
            int player = CONSOLEPLAYER;

            if(IS_NETGAME && !netSvAllowCheats)
                return false;

            if(argc == 2)
            {
                player = atoi(argv[1]);
                if(player < 0 || player >= MAXPLAYERS)
                    return false;
            }

            if(!players[player].plr->inGame)
                return false;

            Cht_NoClipFunc(NULL, player);
        }
    }
    return true;
}

static int suicideResponse(msgresponse_t response, void* context)
{
    if(response == MSG_YES)
    {
        if(IS_NETGAME && IS_CLIENT)
        {
            NetCl_CheatRequest("suicide");
        }
        else
        {
            player_t* plr = &players[CONSOLEPLAYER];
            P_DamageMobj(plr->plr->mo, NULL, NULL, 10000, false);
        }
    }
    return true;
}

DEFCC(CCmdCheatSuicide)
{
    if(G_GetGameState() == GS_MAP)
    {
        player_t* plr;

        if(IS_NETGAME && !netSvAllowCheats)
            return false;

        if(argc == 2)
        {
            int i = atoi(argv[1]);
            if(i < 0 || i >= MAXPLAYERS)
                return false;
            plr = &players[i];
        }
        else
            plr = &players[CONSOLEPLAYER];

        if(!plr->plr->inGame)
            return false;

        if(plr->playerState == PST_DEAD)
            return false;

        if(!IS_NETGAME || IS_CLIENT)
        {
            Hu_MsgStart(MSG_YESNO, SUICIDEASK, suicideResponse, NULL);
            return true;
        }

        P_DamageMobj(plr->plr->mo, NULL, NULL, 10000, false);
        return true;
    }
    else
    {
        Hu_MsgStart(MSG_ANYKEY, SUICIDEOUTMAP, NULL, NULL);
    }

    return true;
}

DEFCC(CCmdCheatWarp)
{
    int args[2];

    if(!cheatsEnabled())
        return false;

    if(gameMode == commercial)
    {
        int num;

        if(argc != 2)
            return false;

        num = atoi(argv[1]);
        args[0] = num / 10 + '0';
        args[1] = num % 10 + '0';
    }
    else
    {
        if(argc != 3)
            return false;

        args[0] = (int) argv[1][0];
        args[1] = (int) argv[2][0];
    }

    Cht_WarpFunc(args, CONSOLEPLAYER);
    return true;
}

DEFCC(CCmdCheatReveal)
{
    int option;
    automapid_t map;

    if(!cheatsEnabled())
        return false;

    map = AM_MapForPlayer(CONSOLEPLAYER);
    AM_SetCheatLevel(map, 0);
    AM_RevealMap(map, false);

    option = atoi(argv[1]);
    if(option < 0 || option > 3)
        return false;

    if(option == 1)
        AM_RevealMap(map, true);
    else if(option != 0)
        AM_SetCheatLevel(map, option -1);

    return true;
}

DEFCC(CCmdCheatGive)
{
    char buf[100];
    int player = CONSOLEPLAYER;
    player_t* plr;
    size_t i, stuffLen;

    if(IS_CLIENT)
    {
        if(argc != 2)
            return false;

        sprintf(buf, "give %s", argv[1]);
        NetCl_CheatRequest(buf);
        return true;
    }

    if(IS_NETGAME && !netSvAllowCheats)
        return false;

    if(argc != 2 && argc != 3)
    {
        Con_Printf("Usage:\n  give (stuff)\n");
        Con_Printf("  give (stuff) (plr)\n");
        Con_Printf("Stuff consists of one or more of (type:id). "
                   "If no id; give all of type:\n");
        Con_Printf(" a - ammo\n");
        Con_Printf(" b - berserk\n");
        Con_Printf(" f - the power of flight\n");
        Con_Printf(" g - light amplification visor\n");
        Con_Printf(" h - health\n");
        Con_Printf(" i - invulnerability\n");
        Con_Printf(" k - key cards/skulls\n");
        Con_Printf(" m - computer area map\n");
        Con_Printf(" p - backpack full of ammo\n");
        Con_Printf(" r - armor\n");
        Con_Printf(" s - radiation shielding suit\n");
        Con_Printf(" v - invisibility\n");
        Con_Printf(" w - weapons\n");
        Con_Printf("Example: 'give arw' corresponds the cheat IDFA.\n");
        Con_Printf("Example: 'give w2k1' gives weapon two and key one.\n");
        return true;
    }

    if(argc == 3)
    {
        player = atoi(argv[2]);
        if(player < 0 || player >= MAXPLAYERS)
            return false;
    }

    if(G_GetGameState() != GS_MAP)
    {
        Con_Printf("Can only \"give\" when in a game!\n");
        return true;
    }

    if(!players[player].plr->inGame)
        return true; // Can't give to a plr who's not playing
    plr = &players[player];

    strcpy(buf, argv[1]); // Stuff is the 2nd arg.
    strlwr(buf);
    stuffLen = strlen(buf);
    for(i = 0; buf[i]; ++i)
    {
        switch(buf[i])
        {
        case 'a':
            {
            boolean giveAll = true;

            if(i < stuffLen)
            {
                int idx;

                idx = ((int) buf[i+1]) - 48;
                if(idx >= 0 && idx < NUM_AMMO_TYPES)
                {   // Give one specific ammo type.
                    plr->update |= PSF_AMMO;
                    plr->ammo[idx].owned = plr->ammo[idx].max;
                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                giveAmmo(plr);
            }
            break;
            }
        case 'b':
            givePower(plr, PT_STRENGTH);
            break;

        case 'f':
            givePower(plr, PT_FLIGHT);
            break;

        case 'g':
            givePower(plr, PT_INFRARED);
            break;

        case 'h':
            P_GiveBody(plr, healthLimit);
            break;

        case 'i':
            givePower(plr, PT_INVULNERABILITY);
            break;

        case 'k':
            {
            boolean giveAll = true;

            if(i < stuffLen)
            {
                int idx;

                idx = ((int) buf[i+1]) - 48;
                if(idx >= 0 && idx < NUM_KEY_TYPES)
                {   // Give one specific key.
                    plr->update |= PSF_KEYS;
                    plr->keys[idx] = true;
                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                giveKeys(plr);
            }
            break;
            }
        case 'm':
            givePower(plr, PT_ALLMAP);
            break;

        case 'p':
            P_GiveBackpack(plr);
            break;

        case 'r':
            giveArmor(player, 1);
            break;

        case 's':
            givePower(plr, PT_IRONFEET);
            break;

        case 'v':
            givePower(plr, PT_INVISIBILITY);
            break;

        case 'w':
            {
            boolean giveAll = true;

            if(i < stuffLen)
            {
                int idx;

                idx = ((int) buf[i+1]) - 48;
                if(idx >= 0 && idx < NUM_WEAPON_TYPES)
                {   // Give one specific weapon.
                    P_GiveWeapon(plr, idx, false);
                    giveAll = false;
                    i++;
                }
            }

            if(giveAll)
            {
                giveWeapons(plr);
            }
            break;
            }
        default:
            // Unrecognized
            Con_Printf("What do you mean, '%c'?\n", buf[i]);
            break;
        }
    }

    return true;
}

DEFCC(CCmdCheatMassacre)
{
    // Only massacre when actually in a map.
    if(G_GetGameState() == GS_MAP)
    {
        int numKilled = P_Massacre(P_CurrentMap());
        Con_Printf("%i monsters killed.\n", numKilled);
    }
    return true;
}

DEFCC(CCmdCheatWhere)
{
    printDebugInfo(&players[CONSOLEPLAYER]);
    return true;
}

/**
 * Exit the current map and go to the intermission.
 */
DEFCC(CCmdCheatLeaveMap)
{
    if(!cheatsEnabled())
        return false;

    if(G_GetGameState() != GS_MAP)
    {
        S_LocalSound(SFX_OOF, NULL);
        Con_Printf("Can only exit a map when in a game!\n");
        return true;
    }

    G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, false), 0, false);
    return true;
}
