/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * d_console.c: jDoom specific console stuff
 */

// HEADER FILES ------------------------------------------------------------

#include "doomstat.h"
#include "d_config.h"
#include "g_game.h"
#include "s_sound.h"
#include "hu_stuff.h"
#include "Mn_def.h"
#include "f_infine.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

DEFCC(CCmdCycleSpy);
DEFCC(CCmdCrosshair);
DEFCC(CCmdCheat);
DEFCC(CCmdCheatGod);
DEFCC(CCmdCheatNoClip);
DEFCC(CCmdCheatWarp);
DEFCC(CCmdCheatReveal);
DEFCC(CCmdCheatGive);
DEFCC(CCmdCheatMassacre);
DEFCC(CCmdCheatExitLevel);
DEFCC(CCmdCheatSuicide);
DEFCC(CCmdMakeLocal);
DEFCC(CCmdSetCamera);
DEFCC(CCmdSetViewLock);
DEFCC(CCmdSpawnMobj);
DEFCC(CCmdPlayDemo);
DEFCC(CCmdRecordDemo);
DEFCC(CCmdStopDemo);
DEFCC(CCmdPrintPlayerCoords);
DEFCC(CCmdCrosshair);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdScreenShot);
DEFCC(CCmdViewSize);
DEFCC(CCmdPause);
DEFCC(CCmdDoomFont);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean hu_showallfrags; // in hu_stuff.c currently

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     consoleFlat = 10;
float   consoleZoom = 1;

// Console variables.
cvar_t  gameCVars[] = {
    "input-mouse-x-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 25,
    "Mouse X axis sensitivity.",
    "input-mouse-y-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 25,
    "Mouse Y axis sensitivity.",
    "ctl-look-joy-inverse", 0, CVT_INT, &cfg.jlookInverseY, 0, 1,
    "1=Inverse joystick look Y axis.",
    "ctl-look-mouse-inverse", 0, CVT_INT, &cfg.mlookInverseY, 0, 1,
    "1=Inverse mouse look Y axis.",
    "ctl-look-joy-delta", 0, CVT_INT, &cfg.jlookDeltaMode, 0, 1,
    "1=Joystick values => look angle delta.",
    "input-joy-x", 0, CVT_INT, &cfg.joyaxis[0], 0, 4,
    "X axis control: 0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.",
    "input-joy-y", 0, CVT_INT, &cfg.joyaxis[1], 0, 4, "Y axis control.",
    "input-joy-z", 0, CVT_INT, &cfg.joyaxis[2], 0, 4, "Z axis control.",
    "input-joy-rx", 0, CVT_INT, &cfg.joyaxis[3], 0, 4,
    "X rotational axis control.",
    "input-joy-ry", 0, CVT_INT, &cfg.joyaxis[4], 0, 4,
    "Y rotational axis control.",
    "input-joy-rz", 0, CVT_INT, &cfg.joyaxis[5], 0, 4,
    "Z rotational axis control.",
    "input-joy-slider1", 0, CVT_INT, &cfg.joyaxis[6], 0, 4,
    "First slider control.",
    "input-joy-slider2", 0, CVT_INT, &cfg.joyaxis[7], 0, 4,
    "Second slider control.",

    "ctl-look-speed", 0, CVT_INT, &cfg.lookSpeed, 1, 5,
    "The speed of looking up/down.",
    "ctl-turn-speed", 0, CVT_INT, &cfg.turnSpeed, 1, 5,
    "The speed of turning left/right.",
    "ctl-use-dclick", 0, CVT_INT, &cfg.dclickuse, 0, 1,
    "1=Doubleclick forward/strafe equals use key.",

    "ctl-look-pov", 0, CVT_BYTE, &cfg.povLookAround, 0, 1,
    "1=Look around using the POV hat.",
    "ctl-look-mouse", 0, CVT_INT, &cfg.usemlook, 0, 1, "1=Mouse look active.",
    "ctl-look-joy", 0, CVT_INT, &cfg.usejlook, 0, 1, "1=Joystick look active.",
    "ctl-run", 0, CVT_INT, &cfg.alwaysRun, 0, 1, "1=Always run.",
    "ctl-look-spring", 0, CVT_INT, &cfg.lookSpring, 0, 1,
    "1=Lookspring active.",
    "ctl-aim-noauto", 0, CVT_INT, &cfg.noAutoAim, 0, 1,
    "1=Autoaiming disabled.",

    "con-flat", CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0,
    "The number of the flat to use for the console background.",
    "con-zoom", 0, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f,
    "Zoom factor for the console background.",

    "view-size", CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 13,
    "View window size (3-13).",

    "view-bob-height", 0, CVT_FLOAT, &cfg.bobView, 0, 1,
    "Scale for viewheight bobbing.",
    "view-bob-weapon", 0, CVT_FLOAT, &cfg.bobWeapon, 0, 1,
    "Scale for player weapon bobbing.",
    "view-bob-weapon-switch-lower", 0, CVT_BYTE, &cfg.bobWeaponLower, 0, 1,
    "HUD weapon lowered during weapon switching.",

    // Weapon Order preferences
    {"weapon-order0", 0, CVT_BYTE,
        &cfg.weaponOrder[0], 0, NUMWEAPONS,
        "Weapon change order, slot 0."},
    {"weapon-order1", 0, CVT_BYTE,
        &cfg.weaponOrder[1], 0, NUMWEAPONS,
        "Weapon change order, slot 1."},
    {"weapon-order2", 0, CVT_BYTE,
        &cfg.weaponOrder[2], 0, NUMWEAPONS,
        "Weapon change order, slot 2."},
    {"weapon-order3", 0, CVT_BYTE,
        &cfg.weaponOrder[3], 0, NUMWEAPONS,
        "Weapon change order, slot 3."},
    {"weapon-order4", 0, CVT_BYTE,
        &cfg.weaponOrder[4], 0, NUMWEAPONS,
        "Weapon change order, slot 4."},
    {"weapon-order5", 0, CVT_BYTE,
        &cfg.weaponOrder[5], 0, NUMWEAPONS,
        "Weapon change order, slot 5."},
    {"weapon-order6", 0, CVT_BYTE,
        &cfg.weaponOrder[6], 0, NUMWEAPONS,
        "Weapon change order, slot 6."},
    {"weapon-order7", 0, CVT_BYTE,
        &cfg.weaponOrder[7], 0, NUMWEAPONS,
        "Weapon change order, slot 7."},
    {"weapon-order8", 0, CVT_BYTE,
        &cfg.weaponOrder[8], 0, NUMWEAPONS,
        "Weapon change order, slot 8."},

    // Game settings for servers.
    "server-game-nomonsters", 0, CVT_BYTE, &cfg.netNomonsters, 0, 1,
    "1=No monsters.",
    "server-game-respawn", 0, CVT_BYTE, &cfg.netRespawn, 0, 1,
    "1= -respawn was used.",
    "server-game-skill", 0, CVT_BYTE, &cfg.netSkill, 0, 4,
    "Skill level in multiplayer games.",
    "server-game-map", 0, CVT_BYTE, &cfg.netMap, 1, 31,
    "Map to use in multiplayer games.",
    "server-game-episode", 0, CVT_BYTE, &cfg.netEpisode, 1, 6,
    "Episode to use in multiplayer games.",
    "server-game-jump", 0, CVT_BYTE, &cfg.netJumping, 0, 1,
    "1=Allow jumping in multiplayer games.",
    "server-game-deathmatch", 0, CVT_BYTE, &cfg.netDeathmatch, 0, 2,
    "Start multiplayers games as deathmatch.",
    "server-game-coop-nodamage", 0, CVT_BYTE, &cfg.noCoopDamage, 0, 1,
    "1=Disable player-player damage in co-op games.",
    "server-game-coop-noweapons", 0, CVT_BYTE, &cfg.noCoopWeapons, 0, 1,
    "1=Disable multiplayer weapons during co-op games.",
    "server-game-noteamdamage", 0, CVT_BYTE, &cfg.noTeamDamage, 0, 1,
    "1=Disable team damage (player color = team).",
    "server-game-deathmatch-killmsg", 0, CVT_BYTE, &cfg.killMessages, 0, 1,
    "1=Announce frags in deathmatch.",
    "server-game-nobfg", 0, CVT_BYTE, &cfg.noNetBFG, 0, 1,
    "1=Disable BFG9000 in all netgames.",
    "server-game-coop-nothing", 0, CVT_BYTE, &cfg.noCoopAnything, 0, 1,
    "1=Disable all multiplayer objects in co-op games.",
    "server-game-coop-respawn-items", 0, CVT_BYTE, &cfg.coopRespawnItems, 0, 1,
    "1=Respawn items in co-op games.",
    {"server-game-bfg-freeaim", 0, CVT_BYTE,
        &cfg.netBFGFreeLook, 0, 1,
        "Allow free-aim with BFG in deathmatch."},
    {"server-game-mod-damage", 0, CVT_BYTE,
        &cfg.netMobDamageModifier, 1, 100,
        "Enemy (mob) damage modifier, multiplayer (1..100)."},
    {"server-game-mod-health", 0, CVT_BYTE,
        &cfg.netMobHealthModifier, 1, 20,
        "Enemy (mob) health modifier, multiplayer (1..20)."},
    {"server-game-respawn-monsters-nightmare", 0, CVT_BYTE,
        &cfg.respawnMonstersNightmare, 0, 1,
        "1=Monster respawning in Nightmare difficulty enabled."},

    // Player data.
    "player-color", 0, CVT_BYTE, &cfg.netColor, 0, 3,
    "Player color: 0=green, 1=gray, 2=brown, 3=red.",
    "player-jump", 0, CVT_INT, &cfg.jumpEnabled, 0, 1, "1=Allow jumping.",
    "player-jump-power", 0, CVT_FLOAT, &cfg.jumpPower, 0, 100, "Jump power.",
    "player-air-movement", 0, CVT_BYTE, &cfg.airborneMovement, 0, 32,
    "Player movement speed while airborne.",
    {"player-autoswitch", 0, CVT_BYTE,
        &cfg.weaponAutoSwitch, 0, 2,
        "Change weapon automatically when picking one up. 1=If better 2=Always"},
    {"player-autoswitch-berserk", 0, CVT_BYTE,
        &cfg.berserkAutoSwitch, 0, 1,
        "Change to fist automatically when picking up berserk pack"},
    {"player-death-lookup", 0, CVT_BYTE,
        &cfg.deathLookUp, 0, 1,
        "1= Look up when killed"},
    "player-eyeheight", 0, CVT_INT, &cfg.plrViewHeight, 41, 54,
    "Player eye height. The original is 41.",
    "player-camera-noclip", 0, CVT_INT, &cfg.cameraNoClip, 0, 1,
    "1=Camera players have no movement clipping.",
    "player-move-speed", 0, CVT_FLOAT, &cfg.playerMoveSpeed, 0, 1,
    "Player movement speed modifier.",

    // Compatibility options
    "game-raiseghosts", 0, CVT_BYTE, &cfg.raiseghosts, 0, 1,
    "1= Archviles raise ghosts from squished corpses.",
    "game-maxskulls", 0, CVT_BYTE, &cfg.maxskulls, 0, 1,
    "1= Pain Elementals can't spawn Lost Souls if more than twenty already exist.",
    "game-skullsinwalls", 0, CVT_BYTE, &cfg.allowskullsinwalls, 0, 1,
    "1= Pain Elementals can spawn Lost Souls inside walls.",
    {"game-anybossdeath666", 0, CVT_BYTE,
        &cfg.anybossdeath, 0, 1,
        "1= The death of ANY boss monster triggers a 666 special (on applicable maps)."},

    "game-fastmonsters", 0, CVT_BYTE, &fastparm, 0, 1,
    "1=Fast monsters in non-demo single player.",
    "game-zclip", 0, CVT_BYTE, &cfg.moveCheckZ, 0, 1,
    "1=Allow mobjs to move under/over each other.",
    "game-corpse-time", CVF_NO_MAX, CVT_INT, &cfg.corpseTime, 0, 0,
    "Corpse vanish time in seconds, 0=disabled.",
    "game-corpse-sliding", 0, CVT_BYTE, &cfg.slidingCorpses, 0, 1,
    "1=Corpses slide down stairs and ledges.",

    "hud-title", 0, CVT_BYTE, &cfg.levelTitle, 0, 1,
    "1=Show level title and author in the beginning.",
    "hud-title-noidsoft", 0, CVT_BYTE, &cfg.hideAuthorIdSoft, 0, 1,
    "1=Don't show map author if it's \"id Software\".",

    "xg-dev", 0, CVT_INT, &xgDev, 0, 1, "1=Print XG debug messages.",

    NULL
};

//  Console commands
ccmd_t  gameCCmds[] = {
    {"spy",         CCmdCycleSpy,       "Spy mode: cycle player views in co-op.", 0 },
    {"screenshot",  CCmdScreenShot,     "Takes a screenshot. Saved to DOOMnn.TGA.", 0 },
    {"viewsize",    CCmdViewSize,       "View size adjustment.", 0 },
    {"pause",       CCmdPause,          "Pause the game.", 0 },

    // $cheats
    {"cheat",       CCmdCheat,          "Issue a cheat code using the original Doom cheats.", 0 },
    {"god",         CCmdCheatGod,       "God mode.", 0 },
    {"noclip",      CCmdCheatNoClip,    "No movement clipping (walk through walls).", 0 },
    {"warp",        CCmdCheatWarp,      "Warp to another map.", 0 },
    {"reveal",      CCmdCheatReveal,    "Map cheat.", 0 },
    {"give",        CCmdCheatGive,      "Gives you weapons, ammo, power-ups, etc.", 0 },
    {"kill",        CCmdCheatMassacre,  "Kill all the monsters on the level.", 0 },
    {"exitlevel",   CCmdCheatExitLevel, "Exit the current level.", 0 },
    {"suicide",     CCmdCheatSuicide,   "Kill yourself. What did you think?", 0 },

    {"doomfont",    CCmdDoomFont,       "Use the Doom font in the console.", 0 },

    // $infine
    {"startinf",    CCmdStartInFine,    "Start an InFine script.", 0 },
    {"stopinf",     CCmdStopInFine,     "Stop the currently playing interlude/finale.", 0 },
    {"stopfinale",  CCmdStopInFine,     "Stop the currently playing interlude/finale.", 0 },

    {"spawnmobj",   CCmdSpawnMobj,      "Spawn a new mobj.", 0 },
    {"coord",       CCmdPrintPlayerCoords,"Print the coordinates of the consoleplayer.", 0 },

    // $democam
    {"makelocp",    CCmdMakeLocal,      "Make local player.", 0 },
    {"makecam",     CCmdSetCamera,      "Toggle camera mode.", 0 },
    {"setlock",     CCmdSetViewLock,    "Set camera viewlock.", 0 },
    {"lockmode",    CCmdSetViewLock,    "Set camera viewlock mode.", 0 },

    // $moveplane
    {"movefloor",   CCmdMovePlane,      "Move a sector's floor plane.", 0 },
    {"moveceil",    CCmdMovePlane,      "Move a sector's ceiling plane.", 0 },
    {"movesec",     CCmdMovePlane,      "Move a sector's both planes.", 0 },
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

// Add the console variables and commands.
void G_ConsoleRegistration()
{
    int     i;

    for(i = 0; gameCVars[i].name; i++)
        Con_AddVariable(gameCVars + i);
    for(i = 0; gameCCmds[i].name; i++)
        Con_AddCommand(gameCCmds + i);
}

void D_ConsoleBg(int *width, int *height)
{
    extern int consoleFlat;
    extern float consoleZoom;

    GL_SetFlat(consoleFlat + W_CheckNumForName("F_START") + 1);
    *width = (int) (64 * consoleZoom);
    *height = (int) (64 * consoleZoom);
}

DEFCC(CCmdScreenShot)
{
    G_ScreenShot();
    return true;
}

DEFCC(CCmdViewSize)
{
    int     min = 3, max = 13, *val = &cfg.screenblocks;

    if(argc != 2)
    {
        Con_Printf("Usage: %s (size)\n", argv[0]);
        Con_Printf("Size can be: +, -, (num).\n");
        return true;
    }

    if(!stricmp(argv[1], "+"))
        (*val)++;
    else if(!stricmp(argv[1], "-"))
        (*val)--;
    else
        *val = strtol(argv[1], NULL, 0);

    if(*val < min)
        *val = min;
    if(*val > max)
        *val = max;

    // Update the view size if necessary.
    R_SetViewSize(cfg.screenblocks, 0);
    return true;
}

DEFCC(CCmdPause)
{
    extern boolean sendpause;

    if(!menuactive)
        sendpause = true;
    return true;
}

int ConTextOut(char *text, int x, int y)
{
    extern int typein_time;
    int     old = typein_time;

    typein_time = 0xffffff;
    M_WriteText2(x, y, text, hu_font_a, -1, -1, -1, -1);
    typein_time = old;
    return 0;
}

int ConTextWidth(char *text)
{
    return M_StringWidth(text, hu_font_a);
}

void ConTextFilter(char *text)
{
    strupr(text);
}

DEFCC(CCmdDoomFont)
{
    ddfont_t cfont;

    cfont.flags = DDFONT_WHITE;
    cfont.height = 8;
    cfont.sizeX = 1.5f;
    cfont.sizeY = 2;
    cfont.TextOut = ConTextOut;
    cfont.Width = ConTextWidth;
    cfont.Filter = ConTextFilter;
    Con_SetFont(&cfont);
    return true;
}
