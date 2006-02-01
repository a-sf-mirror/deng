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
 * HConsole.c: jHexen specific console stuff
 */

// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "x_config.h"
#include "d_net.h"
#include "jHexen/mn_def.h"
#include "../Common/hu_stuff.h"
#include "f_infine.h"
#include "g_common.h"

// MACROS ------------------------------------------------------------------

#define OBSOLETE    CVF_HIDE|CVF_NO_ARCHIVE

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    S_InitScript();
void    SN_InitSequenceScript(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdPause);
DEFCC(CCmdScriptInfo);
DEFCC(CCmdSetDemoMode);
DEFCC(CCmdCrosshair);
DEFCC(CCmdViewSize);
DEFCC(CCmdInventory);
DEFCC(CCmdScreenShot);

DEFCC(CCmdHexenFont);

DEFCC(CCmdCycleSpy);
DEFCC(CCmdTest);
DEFCC(CCmdSpawnMobj);
DEFCC(CCmdPrintPlayerCoords);
DEFCC(CCmdMovePlane);

// The cheats.
DEFCC(CCmdCheat);
DEFCC(CCmdCheatGod);
DEFCC(CCmdCheatClip);
DEFCC(CCmdCheatGive);
DEFCC(CCmdCheatWarp);
DEFCC(CCmdCheatPig);
DEFCC(CCmdCheatMassacre);
DEFCC(CCmdCheatShadowcaster);
DEFCC(CCmdCheatWhere);
DEFCC(CCmdCheatRunScript);
DEFCC(CCmdCheatReveal);
DEFCC(CCmdCheatSuicide);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern ccmd_t netCCmds[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     consoleFlat = 60;
float   consoleZoom = 1;

cvar_t  gameCVars[] = {
    "i_MouseSensiX", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 25,
    "Mouse X axis sensitivity.",
    "i_MouseSensiY", OBSOLETE | CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 25,
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
    //  "i_JoyDeadZone", OBSOLETE, CVT_INT, &cfg.joydead, 10, 90, "Joystick dead zone, in percents.",
    //"FPS", OBSOLETE | CVF_NO_ARCHIVE, CVT_INT, &cfg.showFPS, 0, 1,
    //"1=Show the frames per second counter.",
    "EchoMsg", OBSOLETE, CVT_INT, &cfg.echoMsg, 0, 1,
    "1=Echo all messages to the console.",
    "IceCorpse", OBSOLETE, CVT_INT, &cfg.translucentIceCorpse, 0, 1,
    "1=Translucent frozen monsters.",
    "ImmediateUse", OBSOLETE, CVT_INT, &cfg.chooseAndUse, 0, 1,
    "1=Use items immediately from the inventory.",
    "LookSpeed", OBSOLETE, CVT_INT, &cfg.lookSpeed, 1, 5,
    "The speed of looking up/down.",
    "bgFlat", OBSOLETE | CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0,
    "The number of the flat to use for the console background.",
    "bgZoom", OBSOLETE, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f,
    "Zoom factor for the console background.",
    "povLook", OBSOLETE, CVT_BYTE, &cfg.povLookAround, 0, 1,
    "1=Look around using the POV hat.",
    "i_mLook", OBSOLETE, CVT_INT, &cfg.usemlook, 0, 1, "1=Mouse look active.",
    "i_jLook", OBSOLETE, CVT_INT, &cfg.usejlook, 0, 1,
    "1=Joystick look active.",
    "AlwaysRun", OBSOLETE, CVT_INT, &cfg.alwaysRun, 0, 1, "1=Always run.",
    "LookSpring", OBSOLETE, CVT_INT, &cfg.lookSpring, 0, 1,
    "1=Lookspring active.",
    "NoAutoAim", OBSOLETE, CVT_INT, &cfg.noAutoAim, 0, 1,
    "1=Autoaiming disabled.",
    "h_ViewSize", OBSOLETE | CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 11,
    "View window size (3-11).",
    "h_sbSize", OBSOLETE | CVF_PROTECTED, CVT_INT, &cfg.sbarscale, 1, 20,
    "Status bar size (1-20).",
    "dClickUse", OBSOLETE, CVT_INT, &cfg.dclickuse, 0, 1,
    "1=Double click forward/strafe equals pressing the use key.",
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
    "s_3D", OBSOLETE, CVT_INT, &cfg.snd_3D, 0, 1, "1=Play sounds in 3D.",
    "s_ReverbVol", OBSOLETE, CVT_FLOAT, &cfg.snd_ReverbFactor, 0, 1,
    "General reverb strength (0-1).",
    "SoundDebug", OBSOLETE | CVF_NO_ARCHIVE, CVT_INT, &DebugSound, 0, 1,
    "1=Display sound debug information.",
    "ReverbDebug", OBSOLETE | CVF_NO_ARCHIVE, CVT_BYTE, &cfg.reverbDebug, 0, 1,
    "1=Reverberation debug information in the console.",
    "ShowMana", OBSOLETE, CVT_BYTE, &cfg.hudShown[HUD_MANA], 0, 2,
    "Show mana when the status bar is hidden.",
    //"SaveDir", OBSOLETE|CVF_PROTECTED, CVT_CHARPTR, &SavePath, 0, 0, "The directory for saved games.",
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
    "RandClass", OBSOLETE, CVT_BYTE, &cfg.netRandomclass, 0, 1,
    "1=Respawn in a random class (deathmatch).",
    "n_Skill", OBSOLETE, CVT_BYTE, &cfg.netSkill, 0, 4,
    "Skill level in multiplayer games.",
    "n_Map", OBSOLETE, CVT_BYTE, &cfg.netMap, 1, 99,
    "Map to use in multiplayer games.",
    "Deathmatch", OBSOLETE, CVT_BYTE, &cfg.netDeathmatch, 0, 1,
    "1=Start multiplayers games as deathmatch.",
    "n_Class", OBSOLETE, CVT_BYTE, &cfg.netClass, 0, 2,
    "Player class in multiplayer games.",
    "n_Color", OBSOLETE, CVT_BYTE, &cfg.netColor, 0, 8,
    "Player color: 0=blue, 1=red, 2=yellow, 3=green, 4=jade, 5=white, \n6=hazel, 7=purple, 8=auto.",
    "n_mobDamage", OBSOLETE, CVT_BYTE, &cfg.netMobDamageModifier, 1, 100,
    "Enemy (mob) damage modifier, multiplayer (1..100).",
    "n_mobHealth", OBSOLETE, CVT_BYTE, &cfg.netMobHealthModifier, 1, 20,
    "Enemy (mob) health modifier, multiplayer (1..20).",
    "OverrideHubMsg", OBSOLETE, CVT_BYTE, &cfg.overrideHubMsg, 0, 2,
    "Override the transition hub message.",
    "DemoDisabled", OBSOLETE, CVT_BYTE, &cfg.demoDisabled, 0, 2,
    "Disable demos.",
    "MaulatorTime", OBSOLETE | CVF_NO_MAX, CVT_INT, &MaulatorSeconds, 1, 0,
    "Dark Servant lifetime, in seconds (default: 25).",
    "FastMonsters", OBSOLETE, CVT_BYTE, &cfg.fastMonsters, 0, 1,
    "1=Fast monsters in non-demo single player.",
    "MapTitle", OBSOLETE, CVT_BYTE, &cfg.levelTitle, 0, 1,
    "1=Show map title after entering map.",
    "CounterCheat", OBSOLETE, CVT_BYTE, &cfg.counterCheat, 0, 63,
    "6-bit bitfield. Show kills, items and secret counters in automap.",

    //===========================================================================
    // New names (1.1.0 =>)
    //===========================================================================
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
    "con-flat", CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0,
    "The number of the flat to use for the console background.",
    "con-zoom", 0, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f,
    "Zoom factor for the console background.",

    "view-bob-height", 0, CVT_FLOAT, &cfg.bobView, 0, 1,
    "Scaling factor for viewheight bobbing.",
    "view-bob-weapon", 0, CVT_FLOAT, &cfg.bobWeapon, 0, 1,
    "Scaling factor for player weapon bobbing.",
    "ctl-aim-noauto", 0, CVT_INT, &cfg.noAutoAim, 0, 1,
    "1=Autoaiming disabled.",
    "ctl-look-joy", 0, CVT_INT, &cfg.usejlook, 0, 1, "1=Joystick look active.",
    "ctl-look-joy-inverse", 0, CVT_INT, &cfg.jlookInverseY, 0, 1,
    "1=Inverse joystick look Y axis.",
    "ctl-look-joy-delta", 0, CVT_INT, &cfg.jlookDeltaMode, 0, 1,
    "1=Joystick values => look angle delta.",
    "ctl-look-mouse", 0, CVT_INT, &cfg.usemlook, 0, 1, "1=Mouse look active.",
    "ctl-look-mouse-inverse", 0, CVT_INT, &cfg.mlookInverseY, 0, 1,
    "1=Inverse mouse look Y axis.",
    "ctl-look-pov", 0, CVT_BYTE, &cfg.povLookAround, 0, 1,
    "1=Look around using the POV hat.",
    "ctl-look-speed", 0, CVT_INT, &cfg.lookSpeed, 1, 5,
    "The speed of looking up/down.",
    "ctl-turn-speed", 0, CVT_INT, &cfg.turnSpeed, 1, 5,
    "The speed of turning left/right.",
    "ctl-look-spring", 0, CVT_INT, &cfg.lookSpring, 0, 1,
    "1=Lookspring active.",
    "ctl-run", 0, CVT_INT, &cfg.alwaysRun, 0, 1, "1=Always run.",
    "ctl-use-dclick", 0, CVT_INT, &cfg.dclickuse, 0, 1,
    "1=Double click forward/strafe equals pressing the use key.",
    "ctl-use-immediate", 0, CVT_INT, &cfg.chooseAndUse, 0, 1,
    "1=Use items immediately from the inventory.",
    "game-fastmonsters", 0, CVT_BYTE, &cfg.fastMonsters, 0, 1,
    "1=Fast monsters in non-demo single player.",
    "game-icecorpse", 0, CVT_INT, &cfg.translucentIceCorpse, 0, 1,
    "1=Translucent frozen monsters.",
    "game-maulator-time", CVF_NO_MAX, CVT_INT, &MaulatorSeconds, 1, 0,
    "Dark Servant lifetime, in seconds (default: 25).",

    "hud-title", 0, CVT_BYTE, &cfg.levelTitle, 0, 1,
    "1=Show map title after entering map.",
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
    "input-mouse-x-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiX, 0, 25,
    "Mouse X axis sensitivity.",
    "input-mouse-y-sensi", CVF_NO_MAX, CVT_INT, &cfg.mouseSensiY, 0, 25,
    "Mouse Y axis sensitivity.",

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
    "msg-hub-override", 0, CVT_BYTE, &cfg.overrideHubMsg, 0, 2,
    "Override the transition hub message.",
    "msg-color-r", 0, CVT_FLOAT, &cfg.msgColor[0], 0, 1, "Normal color of HUD messages red component.",
    "msg-color-g", 0, CVT_FLOAT, &cfg.msgColor[1], 0, 1, "Normal color of HUD messages green component.",
    "msg-color-b", 0, CVT_FLOAT, &cfg.msgColor[2], 0, 1, "Normal color of HUD messages blue component.",

    "player-class", 0, CVT_BYTE, &cfg.netClass, 0, 2,
    "Player class in multiplayer games.",
    "player-color", 0, CVT_BYTE, &cfg.netColor, 0, 8,
    "Player color: 0=blue, 1=red, 2=yellow, 3=green, 4=jade, 5=white,\n6=hazel, 7=purple, 8=auto.",
    "player-camera-noclip", 0, CVT_INT, &cfg.cameraNoClip, 0, 1,
    "1=Camera players have no movement clipping.",
    "player-jump-power", 0, CVT_FLOAT, &cfg.jumpPower, 0, 100, "Jump power.",
    "server-game-deathmatch", 0, CVT_BYTE, &cfg.netDeathmatch, 0, 1,
    "1=Start multiplayers games as deathmatch.",
    "player-move-speed", 0, CVT_FLOAT, &cfg.playerMoveSpeed, 0, 1,
    "Player movement speed modifier.",
    {"player-eyeheight", 0, CVT_INT, &cfg.plrViewHeight, 41, 54,
        "Player eye height (the original is 41)."},

    {"player-autoswitch", 0, CVT_BYTE,
        &cfg.weaponAutoSwitch, 0, 2,
        "Change weapon automatically when picking one up. 1=If better 2=Always"},

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

    "server-game-map", 0, CVT_BYTE, &cfg.netMap, 1, 99,
    "Map to use in multiplayer games.",
    "server-game-mod-damage", 0, CVT_BYTE, &cfg.netMobDamageModifier, 1, 100,
    "Enemy (mob) damage modifier, multiplayer (1..100).",
    "server-game-mod-health", 0, CVT_BYTE, &cfg.netMobHealthModifier, 1, 20,
    "Enemy (mob) health modifier, multiplayer (1..20).",
    "server-game-nomonsters", 0, CVT_BYTE, &cfg.netNomonsters, 0, 1,
    "1=No monsters.",
    "server-game-randclass", 0, CVT_BYTE, &cfg.netRandomclass, 0, 1,
    "1=Respawn in a random class (deathmatch).",
    "server-game-skill", 0, CVT_BYTE, &cfg.netSkill, 0, 4,
    "Skill level in multiplayer games.",
    "view-size", CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 13,
    "View window size (3-13).",
    NULL
};

//  Console commands
ccmd_t  gameCCmds[] = {
    {"spy",         CCmdCycleSpy,       "Change the viewplayer when not in deathmatch.", 0 },
    {"screenshot",  CCmdScreenShot,     "Take a screenshot.", 0 },
    {"viewsize",    CCmdViewSize,       "Set the view size.", 0 },
    {"pause",       CCmdPause,          "Pause the game (same as pressing the pause key).", 0 },

    // $cheats
    {"cheat",       CCmdCheat,          "Issue a cheat code using the original Hexen cheats.", 0 },
    {"god",         CCmdCheatGod,       "I don't think He needs any help...", 0 },
    {"noclip",      CCmdCheatClip,      "Movement clipping on/off.", 0 },
    {"warp",        CCmdCheatWarp,      "Warp to a map.", 0 },
    {"reveal",      CCmdCheatReveal,    "Map cheat.", 0 },
    {"give",        CCmdCheatGive,      "Cheat command to give you various kinds of things.", 0 },
    {"kill",        CCmdCheatMassacre,  "Kill all the monsters on the level.", 0 },
    {"suicide",     CCmdCheatSuicide,   "Kill yourself. What did you think?", 0 },

    {"hexenfont",   CCmdHexenFont,      "Use the Hexen font.", 0 },
    {"message",     CCmdLocalMessage,   "Show a local game message.", 0 },
    /*{"exitlevel",   CCmdExitLevel,      "Exit the current level.", 0 },*/

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

    // jHexen specific
    {"invleft",     CCmdInventory,      "Move inventory cursor to the left.", 0 },
    {"invright",    CCmdInventory,      "Move inventory cursor to the right.", 0 },
    {"pig",         CCmdCheatPig,       "Turn yourself into a pig. Go ahead.", 0 },
    {"runscript",   CCmdCheatRunScript, "Run a script.", 0 },
    {"scriptinfo",  CCmdScriptInfo,     "Show information about all scripts or one particular script.", 0 },
    {"where",       CCmdCheatWhere,     "Prints your map number and exact location.", 0 },
    {"class",       CCmdCheatShadowcaster,"Change player class.", 0 },
#ifdef DEMOCAM
    {"demomode",    CCmdSetDemoMode,    "Set demo external camera mode.", 0 },
#endif
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

DEFCC(CCmdHexenFont)
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
