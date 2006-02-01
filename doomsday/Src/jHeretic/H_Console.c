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
 * H_Console.c: jHeretic specific console stuff
 */

// HEADER FILES ------------------------------------------------------------

#include "jHeretic/h_stat.h"
#include "jHeretic/h_config.h"
#include "jHeretic/G_game.h"
#include "jHeretic/Sounds.h"
#include "Common/hu_stuff.h"
#include "jHeretic/Mn_def.h"
#include "Common/f_infine.h"

// MACROS ------------------------------------------------------------------

#define OBSOLETE    CVF_HIDE|CVF_NO_ARCHIVE

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdCycleSpy);
DEFCC(CCmdCrosshair);
DEFCC(CCmdCheat);
DEFCC(CCmdCheatGod);
DEFCC(CCmdCheatClip);
DEFCC(CCmdCheatWarp);
DEFCC(CCmdCheatReveal);
DEFCC(CCmdCheatGive);
DEFCC(CCmdCheatMassacre);
DEFCC(CCmdCheatWhere);
DEFCC(CCmdCheatPig);
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

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

DEFCC(CCmdScreenShot);
DEFCC(CCmdViewSize);
DEFCC(CCmdPause);
DEFCC(CCmdHereticFont);
DEFCC(CCmdInventory);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     consoleFlat = 6;
float   consoleZoom = 1;

cvar_t  gameCVars[] = {
    "i_MouseSensiX", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 100,
    "Mouse X axis sensitivity.",
    "i_MouseSensiY", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 100,
    "Mouse Y axis sensitivity.",
    "i_jLookInvY", OBSOLETE, CVT_INT, &cfg.jlookInverseY, 0, 1,
    "1=Inverse joystick look Y axis.",
    "i_mLookInvY", OBSOLETE, CVT_INT, &cfg.mlookInverseY, 0, 1,
    "1=Inverse mouse look Y axis.",
    "i_JoyXAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[0], 0, 4,
    "0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.",
    "i_JoyYAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[1], 0, 4,
    "0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.",
    "i_JoyZAxis", OBSOLETE, CVT_INT, &cfg.joyaxis[2], 0, 4,
    "0=None, 1=Move, 2=Turn, 3=Strafe, 4=Look.",
    "EchoMsg", OBSOLETE, CVT_INT, &cfg.echoMsg, 0, 1,
    "1=Echo all messages to the console.",
    "ImmediateUse", OBSOLETE, CVT_INT, &cfg.chooseAndUse, 0, 1,
    "1=Use items immediately from the inventory.",
    "LookSpeed", OBSOLETE, CVT_INT, &cfg.lookSpeed, 1, 5,
    "The speed of looking up/down.",
    "dClickUse", OBSOLETE, CVT_INT, &cfg.dclickuse, 0, 1,
    "1=Doubleclick forward/strafe equals use key.",
    "bgFlat", OBSOLETE | CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0,
    "The number of the flat to use for the console background.",
    "bgZoom", OBSOLETE, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f,
    "Zoom factor for the console background.",
    "PovLook", OBSOLETE, CVT_BYTE, &cfg.povLookAround, 0, 1,
    "1=Look around using the POV hat.",
    "i_mLook", OBSOLETE, CVT_INT, &cfg.usemlook, 0, 1, "1=Mouse look active.",
    "i_jLook", OBSOLETE, CVT_INT, &cfg.usejlook, 0, 1,
    "1=Joystick look active.",
    "AlwaysRun", OBSOLETE, CVT_INT, &cfg.alwaysRun, 0, 1, "1=Always run.",
    "LookSpring", OBSOLETE, CVT_INT, &cfg.lookSpring, 0, 1,
    "1=Lookspring active.",
    "NoAutoAim", OBSOLETE, CVT_INT, &cfg.noAutoAim, 0, 1,
    "1=Autoaiming disabled.",
    "h_ViewSize", OBSOLETE | CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 13,
    "View window size (3-13).",
    "h_sbSize", OBSOLETE | CVF_PROTECTED, CVT_INT, &cfg.sbarscale, 1, 20,
    "Status bar size (1-20).",
    "XHair", OBSOLETE | CVF_NO_MAX | CVF_PROTECTED, CVT_INT, &cfg.xhair, 0, 0,
    "The current crosshair.",
    "XHairR", OBSOLETE, CVT_BYTE, &cfg.xhairColor[0], 0, 255,
    "Red crosshair color component.",
    "XHairG", OBSOLETE, CVT_BYTE, &cfg.xhairColor[1], 0, 255,
    "Green crosshair color component.",
    "XHairB", OBSOLETE, CVT_BYTE, &cfg.xhairColor[2], 0, 255,
    "Blue crosshair color component.",
    "XHairSize", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.xhairSize, 0, 0,
    "Crosshair size: 1=Normal.",
    "Messages", OBSOLETE, CVT_BYTE, &cfg.msgShow, 0, 1, "1=Show messages.",
    "ShowAmmo", OBSOLETE, CVT_BYTE, &cfg.hudShown[0], 0, 1,
    "1=Show ammo when the status bar is hidden.",
    "ShowArmor", OBSOLETE, CVT_BYTE, &cfg.hudShown[1], 0, 1,
    "1=Show armor when the status bar is hidden.",
    "ShowKeys", OBSOLETE, CVT_BYTE, &cfg.hudShown[2], 0, 1,
    "1=Show keys when the status bar is hidden.",
    "ChatMacro0", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[0], 0, 0,
    "Chat macro 1.",
    "ChatMacro1", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[1], 0, 0,
    "Chat macro 2.",
    "ChatMacro2", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[2], 0, 0,
    "Chat macro 3.",
    "ChatMacro3", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[3], 0, 0,
    "Chat macro 4.",
    "ChatMacro4", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[4], 0, 0,
    "Chat macro 5.",
    "ChatMacro5", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[5], 0, 0,
    "Chat macro 6.",
    "ChatMacro6", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[6], 0, 0,
    "Chat macro 7.",
    "ChatMacro7", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[7], 0, 0,
    "Chat macro 8.",
    "ChatMacro8", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[8], 0, 0,
    "Chat macro 9.",
    "ChatMacro9", OBSOLETE, CVT_CHARPTR, &cfg.chat_macros[9], 0, 0,
    "Chat macro 10.",
    "NoMonsters", OBSOLETE, CVT_BYTE, &cfg.netNomonsters, 0, 1,
    "1=No monsters.",
    "Respawn", OBSOLETE, CVT_BYTE, &cfg.netRespawn, 0, 1,
    "1= -respawn was used.",
    "n_Skill", OBSOLETE, CVT_BYTE, &cfg.netSkill, 0, 4,
    "Skill level in multiplayer games.",
    "n_Map", OBSOLETE, CVT_BYTE, &cfg.netMap, 1, 31,
    "Map to use in multiplayer games.",
    "n_Episode", OBSOLETE, CVT_BYTE, &cfg.netEpisode, 1, 6,
    "Episode to use in multiplayer games.",
    "n_Jump", OBSOLETE, CVT_BYTE, &cfg.netJumping, 0, 1,
    "1=Allow jumping in multiplayer games.",
    "Deathmatch", OBSOLETE, CVT_BYTE, &cfg.netDeathmatch, 0, 1,
    "1=Start multiplayers games as deathmatch.",
    "n_Color", OBSOLETE, CVT_BYTE, &cfg.netColor, 0, 4,
    "Player color: 0=green, 1=yellow, 2=red, 3=blue, 4=default.",
    "RingFilter", OBSOLETE, CVT_INT, &cfg.ringFilter, 1, 2,
    "Ring effect filter. 1=Brownish, 2=Blue.",
    "AllowJump", OBSOLETE, CVT_INT, &cfg.jumpEnabled, 0, 1, "1=Allow jumping.",
    "TomeTimer", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.tomeCounter, 0, 0,
    "Countdown seconds for the Tome of Power.",
    "TomeSound", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.tomeSound, 0, 0,
    "Seconds for countdown sound of Tome of Power.",
    "FastMonsters", OBSOLETE, CVT_BYTE, &cfg.fastMonsters, 0, 1,
    "1=Fast monsters in non-demo single player.",
    "EyeHeight", OBSOLETE, CVT_INT, &cfg.plrViewHeight, 41, 54,
    "Player eye height (the original is 41).",

    "LevelTitle", OBSOLETE, CVT_BYTE, &cfg.levelTitle, 0, 1,
    "1=Show level title and author in the beginning.",
    "CounterCheat", OBSOLETE, CVT_BYTE, &cfg.counterCheat, 0, 63,
    "6-bit bitfield. Show kills, items and secret counters in automap.",

    //===========================================================================
    // New names (1.2.0 =>)
    //===========================================================================

    "input-mouse-x-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 100,
    "Mouse X axis sensitivity.",
    "input-mouse-y-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 100,
    "Mouse Y axis sensitivity.",
    "ctl-look-joy-inverse", 0, CVT_INT, &cfg.jlookInverseY, 0, 1,
    "1=Inverse joystick look Y axis.",
    "ctl-look-mouse-inverse", 0, CVT_INT, &cfg.mlookInverseY, 0, 1,
    "1=Inverse mouse look Y axis.",
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
    "player-camera-noclip", 0, CVT_INT, &cfg.cameraNoClip, 0, 1,
    "1=Camera players have no movement clipping.",
    "ctl-look-joy-delta", 0, CVT_INT, &cfg.jlookDeltaMode, 0, 1,
    "1=Joystick values => look angle delta.",

    "ctl-use-immediate", 0, CVT_INT, &cfg.chooseAndUse, 0, 1,
    "1=Use items immediately from the inventory.",
    "ctl-look-speed", 0, CVT_INT, &cfg.lookSpeed, 1, 5,
    "The speed of looking up/down.",
    "ctl-turn-speed", 0, CVT_INT, &cfg.turnSpeed, 1, 5,
    "The speed of turning left/right.",
    "ctl-use-dclick", 0, CVT_INT, &cfg.dclickuse, 0, 1,
    "1=Doubleclick forward/strafe equals use key.",
    "con-flat", CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0,
    "The number of the flat to use for the console background.",
    "con-zoom", 0, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f,
    "Zoom factor for the console background.",
    "ctl-look-pov", 0, CVT_BYTE, &cfg.povLookAround, 0, 1,
    "1=Look around using the POV hat.",

    "ctl-look-mouse", 0, CVT_INT, &cfg.usemlook, 0, 1, "1=Mouse look active.",
    "ctl-look-joy", 0, CVT_INT, &cfg.usejlook, 0, 1, "1=Joystick look active.",
    "ctl-run", 0, CVT_INT, &cfg.alwaysRun, 0, 1, "1=Always run.",
    "ctl-look-spring", 0, CVT_INT, &cfg.lookSpring, 0, 1,
    "1=Lookspring active.",
    "ctl-aim-noauto", 0, CVT_INT, &cfg.noAutoAim, 0, 1,
    "1=Autoaiming disabled.",
    "view-size", CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 13,
    "View window size (3-13).",

    "view-bob-height", 0, CVT_FLOAT, &cfg.bobView, 0, 1,
    "Scaling factor for viewheight bobbing.",
    "view-bob-weapon", 0, CVT_FLOAT, &cfg.bobWeapon, 0, 1,
    "Scaling factor for player weapon bobbing.",
    "view-bob-weapon-switch-lower", 0, CVT_BYTE, &cfg.bobWeaponLower, 0, 1,
    "HUD weapon lowered during weapon switching.",

    "chat-macro0", 0, CVT_CHARPTR, &cfg.chat_macros[0], 0, 0, "Chat macro 1.",
    "chat-macro1", 0, CVT_CHARPTR, &cfg.chat_macros[1], 0, 0, "Chat macro 2.",
    "chat-macro2", 0, CVT_CHARPTR, &cfg.chat_macros[2], 0, 0, "Chat macro 3.",
    "chat-macro3", 0, CVT_CHARPTR, &cfg.chat_macros[3], 0, 0, "Chat macro 4.",
    "chat-macro4", 0, CVT_CHARPTR, &cfg.chat_macros[4], 0, 0, "Chat macro 5.",
    "chat-macro5", 0, CVT_CHARPTR, &cfg.chat_macros[5], 0, 0, "Chat macro 6.",
    "chat-macro6", 0, CVT_CHARPTR, &cfg.chat_macros[6], 0, 0, "Chat macro 7.",
    "chat-macro7", 0, CVT_CHARPTR, &cfg.chat_macros[7], 0, 0, "Chat macro 8.",
    "chat-macro8", 0, CVT_CHARPTR, &cfg.chat_macros[8], 0, 0, "Chat macro 9.",
    "chat-macro9", 0, CVT_CHARPTR, &cfg.chat_macros[9], 0, 0, "Chat macro 10.",

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
    "server-game-deathmatch", 0, CVT_BYTE, &cfg.netDeathmatch, 0, 1,
    "1=Start multiplayers games as deathmatch.",

    // Player data.
    "player-color", 0, CVT_BYTE, &cfg.netColor, 0, 4,
    "Player color: 0=green, 1=yellow, 2=red, 3=blue, 4=default.",
    {"player-air-movement", 0, CVT_BYTE,
        &cfg.airborneMovement, 0, 32,
        "Player movement speed while airborne."},
    "view-ringfilter", 0, CVT_INT, &cfg.ringFilter, 1, 2,
    "Ring effect filter. 1=Brownish, 2=Blue.",
    "player-jump", 0, CVT_INT, &cfg.jumpEnabled, 0, 1, "1=Allow jumping.",
    "player-jump-power", 0, CVT_FLOAT, &cfg.jumpPower, 0, 100, "Jump power.",

    "game-fastmonsters", 0, CVT_BYTE, &cfg.fastMonsters, 0, 1,
    "1=Fast monsters in non-demo single player.",
    "player-eyeheight", 0, CVT_INT, &cfg.plrViewHeight, 41, 54,
    "Player eye height (the original is 41).",
    "player-move-speed", 0, CVT_FLOAT, &cfg.playerMoveSpeed, 0, 1,
    "Player movement speed modifier.",

    {"server-game-mod-damage", 0, CVT_BYTE,
        &cfg.netMobDamageModifier, 1, 100,
        "Enemy (mob) damage modifier, multiplayer (1..100)."},
    {"server-game-mod-health", 0, CVT_BYTE,
        &cfg.netMobHealthModifier, 1, 20,
        "Enemy (mob) health modifier, multiplayer (1..20)."},

    "hud-title", 0, CVT_BYTE, &cfg.levelTitle, 0, 1,
    "1=Show level title and author in the beginning.",
    "game-corpsetime", CVF_NO_MAX, CVT_INT, &cfg.corpseTime, 0, 0,
    "Number of seconds after which dead monsters disappear.",

    {"msg-secret", 0, CVT_BYTE,
        &cfg.secretMsg, 0, 1,
        "1=Announce the discovery of secret areas."},

    "msg-align", 0, CVT_INT, &cfg.msgAlign, 0, 2,
    "Alignment of HUD messages. 0 = left, 1 = center, 2 = right.",
    "msg-echo", 0, CVT_INT, &cfg.echoMsg, 0, 1,
    "1=Echo all messages to the console.",
    "msg-show", 0, CVT_BYTE, &cfg.msgShow, 0, 1, "1=Show messages.",
    "msg-count", 0, CVT_INT, &cfg.msgCount, 0, 8,
    "Number of HUD messages displayed at the same time.",
    "msg-scale", CVF_NO_MAX, CVT_FLOAT, &cfg.msgScale, 0, 0,
    "Scaling factor for HUD messages.",
    "msg-uptime", CVF_NO_MAX, CVT_INT, &cfg.msgUptime, 35, 0,
    "Number of tics to keep HUD messages on screen.",
    "msg-blink", 0, CVT_BYTE, &cfg.msgBlink, 0, 1,
    "1=HUD messages blink when they're printed.",
    "msg-color-r", 0, CVT_FLOAT, &cfg.msgColor[0], 0, 1, "Color of HUD messages red component.",
    "msg-color-g", 0, CVT_FLOAT, &cfg.msgColor[1], 0, 1, "Color of HUD messages green component.",
    "msg-color-b", 0, CVT_FLOAT, &cfg.msgColor[2], 0, 1, "Color of HUD messages blue component.",

    "xg-dev", 0, CVT_INT, &xgDev, 0, 1, "1=Print XG debug messages.",

    NULL
};

//  Console commands
ccmd_t  gameCCmds[] = {
    {"spy",        CCmdCycleSpy,       "Change the viewplayer when not in deathmatch.", 0 },
    {"screenshot", CCmdScreenShot,     "Take a screenshot.", 0 },
    {"viewsize",   CCmdViewSize,       "Set the view size.", 0 },
    {"pause",      CCmdPause,          "Pause the game (same as pressing the pause key).", 0 },

    // $cheats
    {"cheat",      CCmdCheat,          "Issue a cheat code using the original Hexen cheats.", 0 },
    {"god",        CCmdCheatGod,       "I don't think He needs any help...", 0 },
    {"noclip",     CCmdCheatClip,      "Movement clipping on/off.", 0 },
    {"warp",       CCmdCheatWarp,      "Warp to a map.", 0 },
    {"reveal",     CCmdCheatReveal,    "Map cheat.", 0 },
    {"give",       CCmdCheatGive,      "Cheat command to give you various kinds of things.", 0 },
    {"kill",       CCmdCheatMassacre,  "Kill all the monsters on the level.", 0 },
    {"exitlevel",  CCmdCheatExitLevel, "Exit the current level.", 0 },
    {"suicide",    CCmdCheatSuicide,   "Kill yourself. What did you think?", 0 },

    {"hereticfont",CCmdHereticFont,    "Use the Heretic font.", 0 },
    {"message",    CCmdLocalMessage,   "Show a local game message.", 0 },

    // $infine
    {"startinf",   CCmdStartInFine,    "Start an InFine script.", 0 },
    {"stopinf",    CCmdStopInFine,     "Stop the currently playing interlude/finale.", 0 },
    {"stopfinale", CCmdStopInFine,     "Stop the currently playing interlude/finale.", 0 },

    {"spawnmobj",  CCmdSpawnMobj,      "Spawn a new mobj.", 0 },
    {"coord",      CCmdPrintPlayerCoords,   "Print the coordinates of the consoleplayer.", 0 },

    // $democam
    {"makelocp",   CCmdMakeLocal,      "Make local player.", 0 },
    {"makecam",    CCmdSetCamera,      "Toggle camera mode.", 0 },
    {"setlock",    CCmdSetViewLock,    "Set camera viewlock.", 0 },
    {"lockmode",   CCmdSetViewLock,    "Set camera viewlock mode.", 0 },

    // $moveplane
    {"movefloor",   CCmdMovePlane,      "Move a sector's floor plane.", 0 },
    {"moveceil",    CCmdMovePlane,      "Move a sector's ceiling plane.", 0 },
    {"movesec",     CCmdMovePlane,      "Move a sector's both planes.", 0 },

    // Heretic specific
    {"invleft",    CCmdInventory,      "Move inventory cursor to the left.", 0 },
    {"invright",   CCmdInventory,      "Move inventory cursor to the right.", 0 },
    {"chicken",    CCmdCheatPig,       "Turn yourself into a chicken. Go ahead.", 0 },
    {"where",      CCmdCheatWhere,     "Prints your map number and exact location.", 0 },
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

void H_ConsoleBg(int *width, int *height)
{
    extern int consoleFlat;
    extern float consoleZoom;

    GL_SetFlat(consoleFlat + W_CheckNumForName("F_START") + 1);
    *width = (int) (64 * consoleZoom);
    *height = (int) (64 * consoleZoom);
}

DEFCC(CCmdPause)
{
    extern boolean sendpause;

    if(!menuactive)
        sendpause = true;
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

DEFCC(CCmdScreenShot)
{
    G_ScreenShot();
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

DEFCC(CCmdHereticFont)
{
    ddfont_t cfont;

    cfont.flags = DDFONT_WHITE;
    cfont.height = 9;
    cfont.sizeX = 1.2f;
    cfont.sizeY = 2;
    cfont.TextOut = ConTextOut;
    cfont.Width = ConTextWidth;
    cfont.Filter = ConTextFilter;
    Con_SetFont(&cfont);
    return true;
}
