/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * H_Main.c: HERETIC specifc Initialization.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "jHeretic/Doomdef.h"
#include "jHeretic/h_stat.h"

#include "Common/d_net.h"
#include "jHeretic/P_local.h"
#include "jHeretic/Soundst.h"
#include "jHeretic/h_config.h"
#include "jHeretic/m_menu.h"
#include "jHeretic/G_game.h"

#include "Common/hu_msg.h"
#include "Common/hu_stuff.h"
#include "Common/am_map.h"
#include "Common/p_saveg.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean automapactive;
extern boolean amap_fullyopen;
extern float lookOffset;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean devparm;                // checkparm of -devparm
boolean nomonsters;             // checkparm of -nomonsters
boolean respawnparm;            // checkparm of -respawn

boolean cdrom;                  // true if cd-rom mode active
boolean singletics;             // debug flag to cancel adaptiveness
boolean artiskip;             // whether shift-enter skips an artifact

skill_t startskill;
int     startepisode;
int     startmap;
boolean autostart;
FILE   *debugfile;
boolean debugmode;              // checkparm of -debug

boolean shareware = false;      // true if only episode 1 present
boolean ExtendedWAD = false;    // true if episodes 4 and 5 present

// default font colours
const float deffontRGB[] = { .425f, 0.986f, 0.378f};
const float deffontRGB2[] = { 1.0f, 1.0f, 1.0f};

char   *borderLumps[] = {
    "FLAT513",                  // background
    "bordt",                    // top
    "bordr",                    // right
    "bordb",                    // bottom
    "bordl",                    // left
    "bordtl",                   // top left
    "bordtr",                   // top right
    "bordbr",                   // bottom right
    "bordbl"                    // bottom left
};

// This is returned in D_Get(DD_GAME_MODE), max 16 chars.
char gameModeString[17];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean devMap;

// CODE --------------------------------------------------------------------

#define MAXWADFILES 20

// MAPDIR should be defined as the directory that holds development maps
// for the -wart # # command

#define MAPDIR "\\data\\"

char   *wadfiles[MAXWADFILES] = {
    "heretic.wad",
    "texture1.lmp",
    "texture2.lmp",
    "pnames.lmp"
};

char   *basedefault = "heretic.cfg";

char    exrnwads[80];
char    exrnwads2[80];

void wadprintf(void)
{
    if(debugmode)
    {
        return;
    }
}

void D_AddFile(char *file)
{
    int     numwadfiles;
    char   *new;

    //  char text[256];

    for(numwadfiles = 0; wadfiles[numwadfiles]; numwadfiles++);
    new = malloc(strlen(file) + 1);
    strcpy(new, file);
    if(strlen(exrnwads) + strlen(file) < 78)
    {
        if(strlen(exrnwads))
        {
            strcat(exrnwads, ", ");
        }
        else
        {
            strcpy(exrnwads, "External Wadfiles: ");
        }
        strcat(exrnwads, file);
    }
    else if(strlen(exrnwads2) + strlen(file) < 79)
    {
        if(strlen(exrnwads2))
        {
            strcat(exrnwads2, ", ");
        }
        else
        {
            strcpy(exrnwads2, "     ");
            strcat(exrnwads, ",");
        }
        strcat(exrnwads2, file);
    }
    wadfiles[numwadfiles] = new;
}

/*
 * Check which known IWADs are found. The purpose of this routine is to
 * find out which IWADs the user lets us to know about, but we don't
 * decide which one gets loaded or even see if the WADs are actually
 * there. The default location for IWADs is Data\GAMENAMETEXT\.
 */
void DetectIWADs(void)
{
    // Add a couple of probable locations for Heretic.wad.
    DD_AddIWAD("}Data\\"GAMENAMETEXT"\\Heretic.wad");
    DD_AddIWAD("}Data\\Heretic.wad");
    DD_AddIWAD("}Heretic.wad");
    DD_AddIWAD("Heretic.wad");
}

/*
 * gamemode, gamemission and the gameModeString are set.
 */
void H_IdentifyVersion(void)
{
    // The game mode string is used in netgames.
    strcpy(gameModeString, "heretic");

    if(W_CheckNumForName("E2M1") == -1)
    {
        // Can't find episode 2 maps, must be the shareware WAD
        strcpy(gameModeString, "heretic-share");
    }
    else if(W_CheckNumForName("EXTENDED") != -1)
    {
        // Found extended lump, must be the extended WAD
        strcpy(gameModeString, "heretic-ext");
    }
}

/*
 *  Pre Engine Initialization routine.
 *    All game-specific actions that should take place at this time go here.
 */
void H_PreInit(void)
{
    int i;

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.playerMoveSpeed = 1;
    cfg.dclickuse = false;
    cfg.mouseSensiX = cfg.mouseSensiY = 8;
    cfg.povLookAround = true;
    cfg.joyaxis[0] = JOYAXIS_TURN;
    cfg.joyaxis[1] = JOYAXIS_MOVE;
    cfg.sbarscale = 20;         // Full size.
    cfg.screenblocks = cfg.setblocks = 10;
    cfg.echoMsg = true;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 2;
    cfg.usePatchReplacement = 1;
    cfg.menuScale = .9f;
    cfg.menuGlitter = 0;
    cfg.menuShadow = 0;
  //cfg.menuQuitSound = true;
    cfg.flashcolor[0] = .7f;
    cfg.flashcolor[1] = .9f;
    cfg.flashcolor[2] = 1;
    cfg.flashspeed = 4;
    cfg.turningSkull = false;
    cfg.hudShown[HUD_AMMO] = true;
    cfg.hudShown[HUD_ARMOR] = true;
    cfg.hudShown[HUD_KEYS] = true;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_ARTI] = true;
    cfg.hudScale = .7f;
    cfg.hudColor[0] = .325f;
    cfg.hudColor[1] = .686f;
    cfg.hudColor[2] = .278f;
    cfg.hudColor[3] = 1;
    cfg.hudIconAlpha = 1;
    cfg.xhairSize = 1;
    for(i = 0; i < 4; i++)
        cfg.xhairColor[i] = 255;
  //cfg.snd_3D = false;
  //cfg.snd_ReverbFactor = 100;
  //cfg.moveCheckZ = true;
    cfg.jumpPower = 9;
    cfg.airborneMovement = 1;
    cfg.weaponAutoSwitch = true;
    cfg.secretMsg = true;
    cfg.netJumping = true;
    cfg.netEpisode = 1;
    cfg.netMap = 1;
    cfg.netSkill = sk_medium;
    cfg.netColor = 4;           // Use the default color by default.
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.plrViewHeight = 41;
    cfg.levelTitle = true;
  //cfg.hideAuthorIdSoft = true;
    cfg.menuColor[0] = deffontRGB[0];   // use the default colour by default.
    cfg.menuColor[1] = deffontRGB[1];
    cfg.menuColor[2] = deffontRGB[2];
    cfg.menuColor2[0] = deffontRGB2[0]; // use the default colour by default.
    cfg.menuColor2[1] = deffontRGB2[1];
    cfg.menuColor2[2] = deffontRGB2[2];
    cfg.menuSlam = true;
    cfg.askQuickSaveLoad = true;

    cfg.statusbarAlpha = 1;
    cfg.statusbarCounterAlpha = 1;

/*    cfg.automapPos = 5;
    cfg.automapWidth = 1.0f;
    cfg.automapHeight = 1.0f;*/

    cfg.automapL0[0] = 0.42f;   // Unseen areas
    cfg.automapL0[1] = 0.42f;
    cfg.automapL0[2] = 0.42f;

    cfg.automapL1[0] = 0.41f;   // onesided lines
    cfg.automapL1[1] = 0.30f;
    cfg.automapL1[2] = 0.15f;

    cfg.automapL2[0] = 0.82f;   // floor height change lines
    cfg.automapL2[1] = 0.70f;
    cfg.automapL2[2] = 0.52f;

    cfg.automapL3[0] = 0.47f;   // ceiling change lines
    cfg.automapL3[1] = 0.30f;
    cfg.automapL3[2] = 0.16f;

    cfg.automapBack[0] = 1.0f;
    cfg.automapBack[1] = 1.0f;
    cfg.automapBack[2] = 1.0f;
    cfg.automapBack[3] = 1.0f;
    cfg.automapLineAlpha = 1.0f;
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = true;
    cfg.counterCheatScale = .7f; //From jHeretic

    cfg.msgShow = true;
    cfg.msgCount = 4;
    cfg.msgScale = .8f;
    cfg.msgUptime = 5 * TICSPERSEC;
    cfg.msgAlign = ALIGN_CENTER;
    cfg.msgBlink = true;

    cfg.msgColor[0] = deffontRGB2[0];
    cfg.msgColor[1] = deffontRGB2[1];
    cfg.msgColor[2] = deffontRGB2[2];

  //cfg.killMessages = true;
    cfg.bobView = 1;
    cfg.bobWeapon = 1;
    cfg.bobWeaponLower = true;
    cfg.cameraNoClip = true;
  //cfg.respawnMonstersNightmare = true;

    cfg.weaponOrder[0] = wp_mace;
    cfg.weaponOrder[1] = wp_phoenixrod;
    cfg.weaponOrder[2] = wp_skullrod;
    cfg.weaponOrder[3] = wp_blaster;
    cfg.weaponOrder[4] = wp_crossbow;
    cfg.weaponOrder[5] = wp_goldwand;
    cfg.weaponOrder[6] = wp_gauntlets;
    cfg.weaponOrder[7] = wp_staff;
    cfg.weaponOrder[8] = wp_beak;

    cfg.menuEffects = 1;
    cfg.menuFog = 4;

    cfg.ringFilter = 1;
    cfg.tomeCounter = 10;
    cfg.tomeSound = 3;

    // Shareware WAD has different border background
    if(W_CheckNumForName("E2M1") == -1)
        borderLumps[0] = "FLOOR04";

    // Do the common pre init routine;
    G_PreInit();
}

/*
 *  Post Engine Initialization routine.
 *    All game-specific actions that should take place at this time go here.
 */
void H_PostInit(void)
{
    int     e, m;
    int     p;
    char    file[256];
    char    mapstr[6];

    if(W_CheckNumForName("E2M1") == -1)
        // Can't find episode 2 maps, must be the shareware WAD
        shareware = true;
    else if(W_CheckNumForName("EXTENDED") != -1)
        // Found extended lump, must be the extended WAD
        ExtendedWAD = true;

    // Common post init routine
    G_PostInit();

    // Initialize weapon info using definitions.
    P_InitWeaponInfo();

    // Print a game mode banner with rulers.
    Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER,
                GAMENAMETEXT " " VERSIONTEXT "\n");
    Con_FPrintf(CBLF_RULER, "");

    // Game parameters.
    /* None */

    // get skill / episode / map from parms
    startskill = sk_medium;
    startepisode = 1;
    startmap = 1;
    autostart = false;
    cdrom = false;

    // Game mode specific settings
    /* None */

    // Command line options
    nomonsters = ArgCheck("-nomonsters");
    respawnparm = ArgCheck("-respawn");
    devparm = ArgCheck("-devparm");
    artiskip = !(ArgCheck("-noartiskip"));
    debugmode = ArgCheck("-debug");

    if(ArgCheck("-deathmatch"))
    {
        cfg.netDeathmatch = true;
    }

    p = ArgCheck("-skill");
    if(p && p < myargc - 1)
    {
        startskill = Argv(p + 1)[0] - '1';
        autostart = true;
    }

    p = ArgCheck("-episode");
    if(p && p < myargc - 1)
    {
        startepisode = Argv(p + 1)[0] - '0';
        startmap = 1;
        autostart = true;
    }

    p = ArgCheck("-warp");
    if(p && p < myargc - 2)
    {
        startepisode = Argv(p + 1)[0] - '0';
        startmap = Argv(p + 2)[0] - '0';
        autostart = true;
    }

    // -DEVMAP <episode> <map>
    // Adds a map wad from the development directory to the wad list,
    // and sets the start episode and the start map.
    devMap = false;
    p = ArgCheck("-devmap");
    if(p && p < myargc - 2)
    {
        e = Argv(p + 1)[0];
        m = Argv(p + 2)[0];
        sprintf(file, MAPDIR "E%cM%c.wad", e, m);
        D_AddFile(file);
        printf("DEVMAP: Episode %c, Map %c.\n", e, m);
        startepisode = e - '0';
        startmap = m - '0';
        autostart = true;
        devMap = true;
    }

    // Are we autostarting?
    if(autostart)
    {
        Con_Message("Warp to Episode %d, Map %d, Skill %d\n", startepisode,
                    startmap, startskill + 1);
    }

    // Load a saved game?
    p = ArgCheck("-loadgame");
    if(p && p < myargc - 1)
    {
        SV_SaveGameFile(Argv(p + 1)[0] - '0', file);
        G_LoadGame(file);
    }

    // Check valid episode and map
    if((autostart || IS_NETGAME) && (devMap == false))
    {
        sprintf(mapstr,"E%d%d",startepisode, startmap);
        if(!W_CheckNumForName(mapstr))
        {
            startepisode = 1;
            startmap = 1;
        }
    }

    if(gameaction != ga_loadgame)
    {
        GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
        if(autostart || IS_NETGAME)
        {
            G_InitNew(startskill, startepisode, startmap);
        }
        else
        {
            G_StartTitle();     // start up intro loop
        }
    }
}

void H_Shutdown(void)
{
    HU_UnloadData();
}

void H_Ticker(void)
{
    MN_Ticker();
    G_Ticker();
}

void H_EndFrame(void)
{
}
