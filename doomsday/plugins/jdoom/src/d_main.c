/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * d_main.c: Game initialization (jDoom-specific).
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "m_argv.h"
#include "hu_stuff.h"
#include "hu_log.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "p_saveg.h"
#include "p_mapspec.h"
#include "p_switch.h"
#include "am_map.h"
#include "g_defs.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#define BGCOLOR                 (7)
#define FGCOLOR                 (8)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int verbose;

boolean devParm; // checkparm of -devparm
boolean noMonstersParm; // checkparm of -noMonstersParm
boolean respawnParm; // checkparm of -respawn
boolean fastParm; // checkparm of -fast
boolean turboParm; // checkparm of -turbo

float turboMul; // multiplier for turbo
boolean monsterInfight;

gamemode_t gameMode;
int gameModeBits;
gamemission_t gameMission = GM_DOOM;

// This is returned in D_Get(DD_GAME_MODE), max 16 chars.
char gameModeString[17];

// The patches used in drawing the view border.
char *borderLumps[] = {
    "FLOOR7_2",
    "brdr_t",
    "brdr_r",
    "brdr_b",
    "brdr_l",
    "brdr_tl",
    "brdr_tr",
    "brdr_br",
    "brdr_bl"
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static skillmode_t startSkill;
static int startEpisode;
static int startMap;
static boolean autoStart;

// CODE --------------------------------------------------------------------

/**
 * Attempt to change the current game mode. Can only be done when not
 * actually in a map.
 *
 * \todo Doesn't actually do anything yet other than set the game mode
 * global vars.
 *
 * @param mode          The game mode to change to.
 *
 * @return              @true, if we changed game modes successfully.
 */
boolean G_SetGameMode(gamemode_t mode)
{
    gameMode = mode;

    if(G_GetGameState() == GS_MAP)
        return false;

    switch(mode)
    {
    case shareware: // DOOM 1 shareware, E1, M9
        gameModeBits = GM_SHAREWARE;
        break;

    case registered: // DOOM 1 registered, E3, M27
        gameModeBits = GM_REGISTERED;
        break;

    case commercial: // DOOM 2 retail, E1 M34
        gameModeBits = GM_COMMERCIAL;
        break;

    // DOOM 2 german edition not handled

    case retail: // DOOM 1 retail, E4, M36
        gameModeBits = GM_RETAIL;
        break;

    case indetermined: // Well, no IWAD found.
        gameModeBits = GM_INDETERMINED;
        break;

    default:
        Con_Error("G_SetGameMode: Unknown gameMode %i", mode);
    }

    return true;
}

/**
 * Check which known IWADs are found. The purpose of this routine is to
 * find out which IWADs the user lets us to know about, but we don't decide
 * which one gets loaded or even see if the WADs are actually there.
 *
 * The default location for IWADs is Data\GAMENAMETEXT\.
 */
void G_DetectIWADs(void)
{
    typedef struct {
        char   *file;
        char   *override;
    } fspec_t;

    // The '}' means the paths are affected by the base path.
    char   *paths[] = {
        "}data\\"GAMENAMETEXT"\\",
        "}data\\",
        "}",
        "}iwads\\",
        "",
        0
    };
    fspec_t iwads[] = {
        { "tnt.wad",        "-tnt" },
        { "plutonia.wad",   "-plutonia" },
        { "doom2.wad",      "-doom2" },
        { "doom1.wad",      "-sdoom" },
        { "doom.wad",       "-doom" },
        { "doom.wad",       "-ultimate" },
        { "doomu.wad",      "-udoom" },
        { 0, 0 }
    };
    int                 i, k;
    boolean             overridden = false;
    char                fn[256];

    // First check if an overriding command line option is being used.
    for(i = 0; iwads[i].file; ++i)
        if(ArgExists(iwads[i].override))
        {
            overridden = true;
            break;
        }

    // Tell the engine about all the possible IWADs.
    for(k = 0; paths[k]; k++)
        for(i = 0; iwads[i].file; ++i)
        {
            // Are we allowed to use this?
            if(overridden && !ArgExists(iwads[i].override))
                continue;
            sprintf(fn, "%s%s", paths[k], iwads[i].file);
            DD_AddIWAD(fn);
        }
}

static boolean lumpsFound(char **list)
{
    for(; *list; list++)
        if(W_CheckNumForName(*list) == -1)
            return false;
    return true;
}

/**
 * Checks availability of IWAD files by name, to determine whether
 * registered/commercial features  should be executed (notably loading
 * PWAD's).
 */
static void identifyFromData(void)
{
    typedef struct {
        char  **lumps;
        gamemode_t mode;
    } identify_t;

    char   *shareware_lumps[] = {
        // List of lumps to detect shareware with.
        "e1m1", "e1m2", "e1m3", "e1m4", "e1m5", "e1m6",
        "e1m7", "e1m8", "e1m9",
        "d_e1m1", "floor4_8", "floor7_2", NULL
    };
    char   *registered_lumps[] = {
        // List of lumps to detect registered with.
        "e2m1", "e2m2", "e2m3", "e2m4", "e2m5", "e2m6",
        "e2m7", "e2m8", "e2m9",
        "e3m1", "e3m2", "e3m3", "e3m4", "e3m5", "e3m6",
        "e3m7", "e3m8", "e3m9",
        "cybre1", "cybrd8", "floor7_2", NULL
    };
    char   *retail_lumps[] = {
        // List of lumps to detect Ultimate Doom with.
        "e4m1", "e4m2", "e4m3", "e4m4", "e4m5", "e4m6",
        "e4m7", "e4m8", "e4m9",
        "m_epi4", NULL
    };
    char   *commercial_lumps[] = {
        // List of lumps to detect Doom II with.
        "map01", "map02", "map03", "map04", "map10", "map20",
        "map25", "map30",
        "vilen1", "vileo1", "vileq1", "grnrock", NULL
    };
    char   *plutonia_lumps[] = {
        "_deutex_", "mc5", "mc11", "mc16", "mc20", NULL
    };
    char   *tnt_lumps[] = {
        "cavern5", "cavern7", "stonew1", NULL
    };
    identify_t list[] = {
        {commercial_lumps, commercial}, // Doom2 is easiest to detect.
        {retail_lumps, retail}, // Ultimate Doom is obvious.
        {registered_lumps, registered},
        {shareware_lumps, shareware}
    };
    int         i, num = sizeof(list) / sizeof(identify_t);

    // First check the command line.
    if(ArgCheck("-sdoom"))
    {
        // Shareware DOOM.
        G_SetGameMode(shareware);
        return;
    }

    if(ArgCheck("-doom"))
    {
        // Registered DOOM.
        G_SetGameMode(registered);
        return;
    }

    if(ArgCheck("-doom2") || ArgCheck("-plutonia") || ArgCheck("-tnt"))
    {
        // DOOM 2.
        G_SetGameMode(commercial);
        gameMission = GM_DOOM2;
        if(ArgCheck("-plutonia"))
            gameMission = GM_PLUT;
        if(ArgCheck("-tnt"))
            gameMission = GM_TNT;
        return;
    }

    if(ArgCheck("-ultimate") || ArgCheck("-udoom"))
    {
        // Retail DOOM 1: Ultimate DOOM.
        G_SetGameMode(retail);
        return;
    }

    // Now we must look at the lumps.
    for(i = 0; i < num; ++i)
    {
        // If all the listed lumps are found, selection is made.
        // All found?
        if(lumpsFound(list[i].lumps))
        {
            G_SetGameMode(list[i].mode);
            // Check the mission packs.
            if(lumpsFound(plutonia_lumps))
                gameMission = GM_PLUT;
            else if(lumpsFound(tnt_lumps))
                gameMission = GM_TNT;
            else if(gameMode == commercial)
                gameMission = GM_DOOM2;
            else
                gameMission = GM_DOOM;
            return;
        }
    }

    // A detection couldn't be made.
    G_SetGameMode(shareware);       // Assume the minimum.
    Con_Message("\nIdentifyVersion: DOOM version unknown.\n"
                "** Important data might be missing! **\n\n");
}

/**
 * gameMode, gameMission and the gameModeString are set.
 */
void G_IdentifyVersion(void)
{
    identifyFromData();

    // The game mode string is returned in DD_Get(DD_GAME_MODE).
    // It is sent out in netgames, and the PCL_HELLO2 packet contains it.
    // A client can't connect unless the same game mode is used.
    memset(gameModeString, 0, sizeof(gameModeString));

    strcpy(gameModeString,
           gameMode == shareware ? "doom1-share" :
           gameMode == registered ? "doom1" :
           gameMode == retail ? "doom1-ultimate" :
           gameMode == commercial ?
                (gameMission == GM_PLUT ? "doom2-plut" :
                 gameMission == GM_TNT ? "doom2-tnt" : "doom2") :
           "-");
}

/**
 * Pre Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PreInit(void)
{
    int                 i;

    G_SetGameMode(indetermined);

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.playerMoveSpeed = 1;
    cfg.dclickUse = false;
    cfg.povLookAround = true;
    cfg.screenBlocks = cfg.setBlocks = 10;
    cfg.echoMsg = true;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;
    cfg.usePatchReplacement = 2; // Use built-in replacements if available.
    cfg.menuScale = .9f;
    cfg.menuGlitter = .5f;
    cfg.menuShadow = 0.33f;
    cfg.menuQuitSound = true;
    cfg.menuEffects = 1; // Do type-in effect.
    cfg.flashColor[0] = .7f;
    cfg.flashColor[1] = .9f;
    cfg.flashColor[2] = 1;
    cfg.flashSpeed = 4;
    cfg.turningSkull = true;
    cfg.hudKeysCombine = false;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_ARMOR] = true;
    cfg.hudShown[HUD_AMMO] = true;
    cfg.hudShown[HUD_KEYS] = true;
    cfg.hudShown[HUD_FRAGS] = true;
    cfg.hudShown[HUD_FACE] = false;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // when the hud/statusbar unhides.
        cfg.hudUnHide[i] = 1;
    cfg.hudScale = .6f;
    cfg.hudColor[0] = 1;
    cfg.hudColor[1] = cfg.hudColor[2] = 0;
    cfg.hudColor[3] = 1;
    cfg.hudFog = 1;
    cfg.hudIconAlpha = 1;
    cfg.xhairSize = .5f;
    cfg.xhairVitality = false;
    cfg.xhairColor[0] = 1;
    cfg.xhairColor[1] = 1;
    cfg.xhairColor[2] = 1;
    cfg.xhairColor[3] = 1;
    cfg.moveCheckZ = true;
    cfg.jumpPower = 9;
    cfg.airborneMovement = 1;
    cfg.weaponAutoSwitch = 1; // if better
    cfg.noWeaponAutoSwitchIfFiring = false;
    cfg.ammoAutoSwitch = 0; // never
    cfg.secretMsg = true;
    cfg.slidingCorpses = false;
    cfg.fastMonsters = false;
    cfg.netJumping = true;
    cfg.netEpisode = 1;
    cfg.netMap = 1;
    cfg.netSkill = SM_MEDIUM;
    cfg.netColor = 4;
    cfg.netBFGFreeLook = 0;    // allow free-aim 0=none 1=not BFG 2=All
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1;        // use map default
    cfg.plrViewHeight = 41;
    cfg.mapTitle = true;
    cfg.hideAuthorIdSoft = true;
    cfg.menuColor[0] = 1;
    cfg.menuColor2[0] = 1;
    cfg.menuSlam = false;
    cfg.menuHotkeys = true;
    cfg.askQuickSaveLoad = true;

    cfg.maxSkulls = true;
    cfg.allowSkullsInWalls = false;
    cfg.anyBossDeath = false;
    cfg.monstersStuckInDoors = false;
    cfg.avoidDropoffs = true;
    cfg.moveBlock = false;
    cfg.fallOff = true;
    cfg.fixOuchFace = true;
    cfg.fixStatusbarOwnedWeapons = true;

    cfg.statusbarScale = 20; // Full size.
    cfg.statusbarOpacity = 1;
    cfg.statusbarCounterAlpha = 1;

    cfg.automapCustomColors = 0; // Never.
    cfg.automapL0[0] = .4f; // Unseen areas
    cfg.automapL0[1] = .4f;
    cfg.automapL0[2] = .4f;

    cfg.automapL1[0] = 1.f; // onesided lines
    cfg.automapL1[1] = 0.f;
    cfg.automapL1[2] = 0.f;

    cfg.automapL2[0] = .77f; // floor height change lines
    cfg.automapL2[1] = .6f;
    cfg.automapL2[2] = .325f;

    cfg.automapL3[0] = 1.f; // ceiling change lines
    cfg.automapL3[1] = .95f;
    cfg.automapL3[2] = 0.f;

    cfg.automapMobj[0] = 0.f;
    cfg.automapMobj[1] = 1.f;
    cfg.automapMobj[2] = 0.f;

    cfg.automapBack[0] = 0.f;
    cfg.automapBack[1] = 0.f;
    cfg.automapBack[2] = 0.f;
    cfg.automapOpacity = .7f;
    cfg.automapLineAlpha = .7f;
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = false;
    cfg.automapZoomSpeed = .1f;
    cfg.automapPanSpeed = .5f;
    cfg.automapPanResetOnOpen = true;
    cfg.automapOpenSeconds = AUTOMAP_OPEN_SECONDS;

    cfg.counterCheatScale = .7f;

    cfg.msgShow = true;
    cfg.msgCount = 4;
    cfg.msgScale = .8f;
    cfg.msgUptime = 5 * TICSPERSEC;
    cfg.msgAlign = ALIGN_LEFT;
    cfg.msgBlink = 5;

    cfg.msgColor[0] = 1;
    cfg.msgColor[1] = cfg.msgColor[2] = 0;

    cfg.chatBeep = 1;

    cfg.killMessages = true;
    cfg.bobWeapon = 1;
    cfg.bobView = 1;
    cfg.bobWeaponLower = true;
    cfg.cameraNoClip = true;
    cfg.respawnMonstersNightmare = true;

    cfg.weaponOrder[0] = WT_SIXTH;
    cfg.weaponOrder[1] = WT_NINETH;
    cfg.weaponOrder[2] = WT_FOURTH;
    cfg.weaponOrder[3] = WT_THIRD;
    cfg.weaponOrder[4] = WT_SECOND;
    cfg.weaponOrder[5] = WT_EIGHTH;
    cfg.weaponOrder[6] = WT_FIFTH;
    cfg.weaponOrder[7] = WT_SEVENTH;
    cfg.weaponOrder[8] = WT_FIRST;

    cfg.berserkAutoSwitch = true;

    // Do the common pre init routine;
    G_CommonPreInit();
}

/**
 * Post Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PostInit(void)
{
    int                 p;
    filename_t          file;
    char                mapStr[6];

    // Border background changes depending on mission.
    if(gameMission == GM_DOOM2 || gameMission == GM_PLUT ||
       gameMission == GM_TNT)
        borderLumps[0] = "GRNROCK";

    // Common post init routine
    G_CommonPostInit();

    // Initialize ammo info.
    P_InitAmmoInfo();

    // Initialize weapon info.
    P_InitWeaponInfo();

    // Print a game mode banner with rulers.
    Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER,
                gameMode == retail ? "The Ultimate DOOM Startup\n" :
                gameMode == shareware ? "DOOM Shareware Startup\n" :
                gameMode == registered ? "DOOM Registered Startup\n" :
                gameMode == commercial ?
                    (gameMission == GM_PLUT ? "Final DOOM: The Plutonia Experiment\n" :
                     gameMission == GM_TNT ? "Final DOOM: TNT: Evilution\n" :
                     "DOOM 2: Hell on Earth\n") :
                "Public DOOM\n");

    Con_FPrintf(CBLF_RULER, "");

    // Game parameters.
    monsterInfight = GetDefInt("AI|Infight", 0);

    // Get skill / episode / map from parms.
    gameSkill = startSkill = SM_NOITEMS;
    startEpisode = 1;
    startMap = 1;
    autoStart = false;

    // Game mode specific settings.
    // Plutonia and TNT automatically turn on the full sky.
    if(gameMode == commercial &&
       (gameMission == GM_PLUT || gameMission == GM_TNT))
    {
        Con_SetInteger("rend-sky-full", 1, true);
    }

    // Command line options.
    noMonstersParm = ArgCheck("-nomonsters");
    respawnParm = ArgCheck("-respawn");
    fastParm = ArgCheck("-fast");
    devParm = ArgCheck("-devparm");

    if(ArgCheck("-altdeath"))
        cfg.netDeathmatch = 2;
    else if(ArgCheck("-deathmatch"))
        cfg.netDeathmatch = 1;

    p = ArgCheck("-skill");
    if(p && p < myargc - 1)
    {
        startSkill = Argv(p + 1)[0] - '1';
        autoStart = true;
    }

    p = ArgCheck("-episode");
    if(p && p < myargc - 1)
    {
        startEpisode = Argv(p + 1)[0] - '0';
        startMap = 1;
        autoStart = true;
    }

    p = ArgCheck("-timer");
    if(p && p < myargc - 1 && deathmatch)
    {
        int                 time;

        time = atoi(Argv(p + 1));
        Con_Message("Maps will end after %d minute", time);
        if(time > 1)
            Con_Message("s");
        Con_Message(".\n");
    }

    p = ArgCheck("-warp");
    if(p && p < myargc - 1)
    {
        if(gameMode == commercial)
        {
            startMap = atoi(Argv(p + 1));
            autoStart = true;
        }
        else if(p < myargc - 2)
        {
            startEpisode = Argv(p + 1)[0] - '0';
            startMap = Argv(p + 2)[0] - '0';
            autoStart = true;
        }
    }

    // Turbo option.
    p = ArgCheck("-turbo");
    turboMul = 1.0f;
    if(p)
    {
        int                 scale = 200;

        turboParm = true;
        if(p < myargc - 1)
            scale = atoi(Argv(p + 1));
        if(scale < 10)
            scale = 10;
        if(scale > 400)
            scale = 400;

        Con_Message("turbo scale: %i%%\n", scale);
        turboMul = scale / 100.f;
    }

    // Are we autostarting?
    if(autoStart)
    {
        if(gameMode == commercial)
            Con_Message("Warp to Map %d, Skill %d\n", startMap,
                        startSkill + 1);
        else
            Con_Message("Warp to Episode %d, Map %d, Skill %d\n",
                        startEpisode, startMap, startSkill + 1);
    }

    // Load a saved game?
    p = ArgCheck("-loadgame");
    if(p && p < myargc - 1)
    {
        SV_GetSaveGameFileName(file, Argv(p + 1)[0] - '0',
                               FILENAME_T_MAXLEN);
        G_LoadGame(file);
    }

    // Check valid episode and map
    if(autoStart || IS_NETGAME)
    {
        if(gameMode == commercial)
            sprintf(mapStr, "MAP%2.2d", startMap);
        else
            sprintf(mapStr, "E%d%d", startEpisode, startMap);

        if(!W_CheckNumForName(mapStr))
        {
            startEpisode = 1;
            startMap = 1;
        }
    }

    // Print a string showing the state of the game parameters
    Con_Message("Game state parameters:%s%s%s%s%s\n",
                noMonstersParm? " nomonsters" : "",
                respawnParm? " respawn" : "",
                fastParm? " fast" : "",
                turboParm? " turbo" : "",
                (cfg.netDeathmatch ==1)? " deathmatch" :
                    (cfg.netDeathmatch ==2)? " altdeath" : "");

    if(G_GetGameAction() != GA_LOADGAME)
    {
        if(autoStart || IS_NETGAME)
        {
            G_DeferedInitNew(startSkill, startEpisode, startMap);
        }
        else
        {
            G_StartTitle(); // Start up intro loop.
        }
    }
}

void G_Shutdown(void)
{
    Hu_MsgShutdown();
    Hu_UnloadData();
    Hu_LogShutdown();

    P_DestroyIterList(spechit);
    P_DestroyIterList(linespecials);
    P_DestroyLineTagLists();
    P_DestroySectorTagLists();
    AM_Shutdown();
    P_FreeWeaponSlots();
}

void G_EndFrame(void)
{
    // Nothing to do.
}
