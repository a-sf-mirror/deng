/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * h_console.c: Console stuff - jHeretic specific.
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "hu_stuff.h"
#include "f_infine.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdCheat);
DEFCC(CCmdCheatGod);
DEFCC(CCmdCheatClip);
DEFCC(CCmdCheatWarp);
DEFCC(CCmdCheatReveal);
DEFCC(CCmdCheatGive);
DEFCC(CCmdCheatMassacre);
DEFCC(CCmdCheatWhere);
DEFCC(CCmdCheatPig);
DEFCC(CCmdCheatLeaveMap);
DEFCC(CCmdCheatSuicide);

DEFCC(CCmdMakeLocal);
DEFCC(CCmdSetCamera);
DEFCC(CCmdSetViewLock);
DEFCC(CCmdSetViewMode);

DEFCC(CCmdCycleSpy);

DEFCC(CCmdPlayDemo);
DEFCC(CCmdRecordDemo);
DEFCC(CCmdStopDemo);

DEFCC(CCmdSpawnMobj);

DEFCC(CCmdPrintPlayerCoords);

void G_UpdateEyeHeight(cvar_t* unused);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

DEFCC(CCmdScreenShot);
DEFCC(CCmdViewSize);
DEFCC(CCmdHereticFont);
DEFCC(CCmdConBackground);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

material_t* consoleBG = NULL;
float consoleZoom = 1;

// Console variables.
cvar_t gameCVars[] = {
// Console
    {"con-zoom", 0, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f},

// View/Refresh
    {"view-size", CVF_PROTECTED, CVT_INT, &cfg.screenBlocks, 3, 13},
    {"hud-title", 0, CVT_BYTE, &cfg.mapTitle, 0, 1},

    {"view-bob-height", 0, CVT_FLOAT, &cfg.bobView, 0, 1},
    {"view-bob-weapon", 0, CVT_FLOAT, &cfg.bobWeapon, 0, 1},
    {"view-bob-weapon-switch-lower", 0, CVT_BYTE, &cfg.bobWeaponLower, 0, 1},

    {"view-ringfilter", 0, CVT_INT, &cfg.ringFilter, 1, 2},

// Server-side options
    // Game state
    {"server-game-skill", 0, CVT_BYTE, &cfg.netSkill, 0, 4},
    {"server-game-map", 0, CVT_BYTE, &cfg.netMap, 1, 31},
    {"server-game-episode", 0, CVT_BYTE, &cfg.netEpisode, 1, 6},
    {"server-game-deathmatch", 0, CVT_BYTE,
        &cfg.netDeathmatch, 0, 1}, /* jHeretic only has one deathmatch mode */

    // Modifiers
    {"server-game-mod-damage", 0, CVT_BYTE, &cfg.netMobDamageModifier, 1, 100},
    {"server-game-mod-health", 0, CVT_BYTE, &cfg.netMobHealthModifier, 1, 20},
    {"server-game-mod-gravity", 0, CVT_INT, &cfg.netGravity, -1, 100},

    // Gameplay options
    {"server-game-jump", 0, CVT_BYTE, &cfg.netJumping, 0, 1},
    {"server-game-nomonsters", 0, CVT_BYTE, &cfg.netNoMonsters, 0, 1},
    {"server-game-respawn", 0, CVT_BYTE, &cfg.netRespawn, 0, 1},
    {"server-game-respawn-monsters-nightmare", 0, CVT_BYTE,
        &cfg.respawnMonstersNightmare, 0, 1},
    {"server-game-radiusattack-nomaxz", 0, CVT_BYTE,
        &cfg.netNoMaxZRadiusAttack, 0, 1},
    {"server-game-monster-meleeattack-nomaxz", 0, CVT_BYTE,
        &cfg.netNoMaxZMonsterMeleeAttack, 0, 1},

    {"server-game-coop-nodamage", 0, CVT_BYTE, &cfg.noCoopDamage, 0, 1},
    {"server-game-noteamdamage", 0, CVT_BYTE, &cfg.noTeamDamage, 0, 1},

    // Misc
    {"server-game-announce-secret", 0, CVT_BYTE, &cfg.secretMsg, 0, 1},

// Player
    // Player data
    {"player-color", 0, CVT_BYTE, &cfg.netColor, 0, 4},
    {"player-eyeheight", 0, CVT_INT, &cfg.plrViewHeight, 41, 54, G_UpdateEyeHeight},

    // Movment
    {"player-move-speed", 0, CVT_FLOAT, &cfg.playerMoveSpeed, 0, 1},
    {"player-jump", 0, CVT_INT, &cfg.jumpEnabled, 0, 1},
    {"player-jump-power", 0, CVT_FLOAT, &cfg.jumpPower, 0, 100},
    {"player-air-movement", 0, CVT_BYTE, &cfg.airborneMovement, 0, 32},

    // Weapon switch preferences
    {"player-autoswitch", 0, CVT_BYTE, &cfg.weaponAutoSwitch, 0, 2},
    {"player-autoswitch-ammo", 0, CVT_BYTE, &cfg.ammoAutoSwitch, 0, 2},
    {"player-autoswitch-notfiring", 0, CVT_BYTE,
        &cfg.noWeaponAutoSwitchIfFiring, 0, 1},

    // Weapon Order preferences
    {"player-weapon-order0", 0, CVT_INT, &cfg.weaponOrder[0], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order1", 0, CVT_INT, &cfg.weaponOrder[1], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order2", 0, CVT_INT, &cfg.weaponOrder[2], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order3", 0, CVT_INT, &cfg.weaponOrder[3], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order4", 0, CVT_INT, &cfg.weaponOrder[4], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order5", 0, CVT_INT, &cfg.weaponOrder[5], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order6", 0, CVT_INT, &cfg.weaponOrder[6], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order7", 0, CVT_INT, &cfg.weaponOrder[7], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order8", 0, CVT_INT, &cfg.weaponOrder[8], 0, NUM_WEAPON_TYPES},

    {"player-weapon-nextmode", 0, CVT_BYTE,
        &cfg.weaponNextMode, 0, 1},

    // Misc
    {"player-camera-noclip", 0, CVT_INT, &cfg.cameraNoClip, 0, 1},

// Compatibility options
    {"game-monsters-stuckindoors", 0, CVT_BYTE, &cfg.monstersStuckInDoors, 0, 1},
    {"game-objects-neverhangoverledges", 0, CVT_BYTE, &cfg.avoidDropoffs, 0, 1},
    {"game-objects-clipping", 0, CVT_BYTE, &cfg.moveBlock, 0, 1},
    {"game-player-wallrun-northonly", 0, CVT_BYTE, &cfg.wallRunNorthOnly, 0, 1},
    {"game-objects-falloff", 0, CVT_BYTE, &cfg.fallOff, 0, 1},
    {"game-zclip", 0, CVT_BYTE, &cfg.moveCheckZ, 0, 1},
    {"game-corpse-sliding", 0, CVT_BYTE, &cfg.slidingCorpses, 0, 1},
    {"server-game-maulotaur-fixfloorfire", 0, CVT_BYTE, &cfg.fixFloorFire, 0, 1},

// Game state
    {"game-fastmonsters", 0, CVT_BYTE, &cfg.fastMonsters, 0, 1},

// Gameplay
    {"game-corpse-time", CVF_NO_MAX, CVT_INT, &cfg.corpseTime, 0, 0},

// Misc
    {"msg-echo", 0, CVT_BYTE, &cfg.echoMsg, 0, 1},
    {NULL}
};

//  Console commands
ccmd_t gameCCmds[] = {
    {"spy",        "",      CCmdCycleSpy},
    {"screenshot", "",      CCmdScreenShot},
    {"viewsize",   "s",     CCmdViewSize},

    // $cheats
    {"cheat",       "s",    CCmdCheat},
    {"god",         "",     CCmdCheatGod},
    {"noclip",      "",     CCmdCheatClip},
    {"warp",        NULL,   CCmdCheatWarp},
    {"reveal",      "i",    CCmdCheatReveal},
    {"give",        NULL,   CCmdCheatGive},
    {"kill",        "",     CCmdCheatMassacre},
    {"leavemap",    "",     CCmdCheatLeaveMap},
    {"suicide",     "",     CCmdCheatSuicide},
    {"where",       "",     CCmdCheatWhere},

    {"hereticfont", "",     CCmdHereticFont},
    {"conbg",       "s",    CCmdConBackground},

    // $infine
    {"startinf",    "s",    CCmdStartInFine},
    {"stopinf",     "",     CCmdStopInFine},
    {"stopfinale",  "",     CCmdStopInFine},

    {"spawnmobj",   NULL,   CCmdSpawnMobj},
    {"coord",       "",     CCmdPrintPlayerCoords},

    // $democam
    {"makelocp",    "i",    CCmdMakeLocal},
    {"makecam",     "i",    CCmdSetCamera},
    {"setlock",     NULL,   CCmdSetViewLock},
    {"lockmode",    "i",    CCmdSetViewLock},
    {"viewmode",    NULL,   CCmdSetViewMode},

    // Heretic specific
    {"chicken",     "",     CCmdCheatPig},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Add the console variables and commands.
 */
void G_ConsoleRegistration(void)
{
    int                 i;

    for(i = 0; gameCVars[i].name; ++i)
        Con_AddVariable(gameCVars + i);
    for(i = 0; gameCCmds[i].name; ++i)
        Con_AddCommand(gameCCmds + i);
}

/**
 * Settings for console background drawing.
 * Called EVERY FRAME by the console drawer.
 */
void H_ConsoleBg(int* width, int* height)
{
    if(consoleBG)
    {
        DGL_SetMaterial(consoleBG);
        *width = (int) (64 * consoleZoom);
        *height = (int) (64 * consoleZoom);
    }
    else
    {
        DGL_SetNoMaterial();
        *width = *height = 0;
    }
}

/**
 * Called when the player-eyeheight cvar is changed.
 */
void G_UpdateEyeHeight(cvar_t* unused)
{
    player_t*           plr = &players[CONSOLEPLAYER];

    if(!(plr->plr->flags & DDPF_CAMERA))
        plr->plr->viewHeight = (float) cfg.plrViewHeight;
}

/**
 * Draw (char *) text in the game's font.
 * Called by the console drawer.
 */
int ConTextOut(const char* text, int x, int y)
{
    M_WriteText3(x, y, text, GF_FONTA, -1, -1, -1, -1, false, false, 0);
    return 0;
}

/**
 * Get the visual width of (char*) text in the game's font.
 */
int ConTextWidth(const char* text)
{
    return M_StringWidth(text, GF_FONTA);
}

/**
 * Console command to take a screenshot (duh).
 */
DEFCC(CCmdScreenShot)
{
    G_ScreenShot();
    return true;
}

/**
 * Console command to change the size of the view window.
 */
DEFCC(CCmdViewSize)
{
    int                 min = 3, max = 13, *val = &cfg.screenBlocks;

    if(argc != 2)
    {
        Con_Printf("Usage: %s (size)\n", argv[0]);
        Con_Printf("Size can be: +, -, (num).\n");
        return true;
    }

    // Adjust/set the value
    if(!stricmp(argv[1], "+"))
        (*val)++;
    else if(!stricmp(argv[1], "-"))
        (*val)--;
    else
        *val = strtol(argv[1], NULL, 0);

    // Clamp it
    if(*val < min)
        *val = min;
    if(*val > max)
        *val = max;

    // Update the view size if necessary.
    R_SetViewSize(cfg.screenBlocks);
    return true;
}

/**
 * Configure the console to use the game's font.
 */
DEFCC(CCmdHereticFont)
{
    ddfont_t    cfont;

    cfont.flags = DDFONT_WHITE;
    cfont.height = 9;
    cfont.sizeX = 1.2f;
    cfont.sizeY = 2;
    cfont.drawText = ConTextOut;
    cfont.getWidth = ConTextWidth;
    cfont.filterText = NULL;

    Con_SetFont(&cfont);
    return true;
}

/**
 * Configure the console background.
 */
DEFCC(CCmdConBackground)
{
    material_t*         mat;

    if(!stricmp(argv[1], "off") || !stricmp(argv[1], "none"))
    {
        consoleBG = NULL;
        return true;
    }

    if((mat = P_ToPtr(DMU_MATERIAL,
            P_MaterialCheckNumForName(argv[1], MN_ANY))))
        consoleBG = mat;

    return true;
}
