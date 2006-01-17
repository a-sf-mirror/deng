
// D_main.c

#include <stdio.h>
#include <stdlib.h>
//#include <direct.h>
#include <ctype.h>
#include "jHeretic/Doomdef.h"
#include "Common/d_net.h"
#include "jHeretic/P_local.h"
#include "jHeretic/Soundst.h"
#include "jHeretic/h_config.h"
#include "jHeretic/AcFnLink.h"
#include "jHeretic/Mn_def.h"
#include "Common/hu_msg.h"
#include "Common/hu_stuff.h"
#include "Common/am_map.h"
#include "f_infine.h"
#include "g_update.h"
#include "jHeretic/m_ctrl.h"

#define viewheight  Get(DD_VIEWWINDOW_HEIGHT)

game_import_t gi;
game_export_t gx;

boolean shareware = false;      // true if only episode 1 present
boolean ExtendedWAD = false;    // true if episodes 4 and 5 present

boolean nomonsters;             // checkparm of -nomonsters
boolean respawnparm;            // checkparm of -respawn
boolean debugmode;              // checkparm of -debug
boolean devparm;                    // checkparm of -devparm
boolean cdrom;                  // true if cd-rom mode active
boolean singletics;             // debug flag to cancel adaptiveness
boolean noartiskip;             // whether shift-enter skips an artifact

// Set if homebrew PWAD stuff has been added.
boolean modifiedgame;

skill_t startskill;
int     startepisode;
int     startmap;
boolean autostart;
extern boolean automapactive;
extern boolean amap_fullyopen;
extern float lookOffset;

static boolean devMap;
static char gameModeString[17];

// default font colours
const float deffontRGB[] = { .425f, 0.986f, 0.378f};
const float deffontRGB2[] = { 1.0f, 1.0f, 1.0f};

FILE   *debugfile;

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

// In m_multi.c.
void    MN_DrCenterTextA_CS(char *text, int center_x, int y);
void    MN_DrCenterTextB_CS(char *text, int center_x, int y);

void    G_ConsoleRegistration();
void    R_DrawPlayerSprites(ddplayer_t *viewplr);
void    R_SetAllDoomsdayFlags();
void    R_DrawRingFilter();
void    X_Drawer();
int     D_PrivilegedResponder(event_t *event);

void R_DrawLevelTitle(void)
{
    float   alpha = 1;
    int     y = 13;
    char   *lname, *lauthor, *ptr;

    if(!cfg.levelTitle || actual_leveltime > 6 * 35)
        return;

    if(actual_leveltime < 35)
        alpha = actual_leveltime / 35.0f;
    if(actual_leveltime > 5 * 35)
        alpha = 1 - (actual_leveltime - 5 * 35) / 35.0f;

    lname = (char *) DD_GetVariable(DD_MAP_NAME);
    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);
    Draw_BeginZoom((1 + cfg.hudScale)/2, 160, y);

    if(lname)
    {
        // Skip the ExMx.
        ptr = strchr(lname, ':');
        if(ptr)
        {
            lname = ptr + 1;
            while(*lname && isspace(*lname))
                lname++;
        }
        M_WriteText3(160 - M_StringWidth(lname, hu_font_b) / 2, y, lname,
                    hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], alpha, false, 0);
        y += 20;
    }

    if(lauthor && stricmp(lauthor, "raven software"))
    {
        M_WriteText3(160 - M_StringWidth(lauthor, hu_font_a) / 2, y, lauthor,
                    hu_font_a, .5f, .5f, .5f, alpha, false, 0);
    }
    Draw_EndZoom();
}

//---------------------------------------------------------------------------
//
// PROC D_Display
//
// Draw current display, possibly wiping it from the previous.
//
//---------------------------------------------------------------------------

void    R_ExecuteSetViewSize(void);

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t wipegamestate = GS_DEMOSCREEN;
extern boolean inhelpscreens;

extern boolean finalestage;
#define SIZEFACT 4
#define SIZEFACT2 16
void D_Display(void)
{
    static boolean viewactivestate = false;
    static boolean menuactivestate = false;
    static boolean inhelpscreensstate = false;
    static int targx =0, targy = 0, targw =0, targh = 0;
    static int /*x = 0, y = 0, */w = 320, h = 200, offy = 0;
    static int fullscreenmode = 0;
    static gamestate_t oldgamestate = -1;
    int     ay;
    player_t *vplayer = &players[displayplayer];
    boolean iscam = (vplayer->plr->flags & DDPF_CAMERA) != 0;   // $democam

    // $democam: can be set on every frame
    if(cfg.setblocks > 10 || iscam)
    {
        // Full screen.
        targx = 0;
        targy = 0;
        targw = 320;
        targh = 200;
    }
    else
    {
        targw = cfg.setblocks * 32;
        targh = cfg.setblocks * (200 - SBARHEIGHT * cfg.sbarscale / 20) / 10;
        targx = 160 - (targw >> 1);
        targy = (200 - SBARHEIGHT * cfg.sbarscale / 20 - targh) >> 1;
    }

    if(targw > w)
        w+= (((targw-w)>> 1)+SIZEFACT2)>>SIZEFACT;
    if(targw < w)
        w-= (((w-targw)>> 1)+SIZEFACT2)>>SIZEFACT;
    if(targh > h)
        h+= (((targh-h)>> 1)+SIZEFACT2)>>SIZEFACT;
    if(targh < h)
        h-= (((h-targh)>> 1)+SIZEFACT2)>>SIZEFACT;

    if(cfg.setblocks < 10)
    {
        offy = (SBARHEIGHT * cfg.sbarscale/20);
        R_ViewWindow(160-(w>>1), 100-((h+offy) >>1), w, h);
    } else
        R_ViewWindow(targx, targy, targw, targh);

    // Do buffered drawing
    switch (gamestate)
    {
    case GS_LEVEL:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;
        if(leveltime < 2)
        {
            // Don't render too early; the first couple of frames
            // might be a bit unstable -- this should be considered
            // a bug, but since there's an easy fix...
            break;
        }
        if(!automapactive || !amap_fullyopen || cfg.automapBack[3] < 1 /*|| cfg.automapWidth < 1 || cfg.automapHeight < 1*/)
        {
            // Draw the player view.
            if(IS_CLIENT)
            {
                // Server updates mobj flags in NetSv_Ticker.
                R_SetAllDoomsdayFlags();
            }
            // The view angle offset.
            Set(DD_VIEWANGLE_OFFSET, ANGLE_MAX * -lookOffset);
            GL_SetFilter(vplayer->plr->filter);

            // How about fullbright?
            Set(DD_FULLBRIGHT, vplayer->powers[pw_invulnerability]);

            // Render the view with possible custom filters.
            R_RenderPlayerView(vplayer->plr);

            if(vplayer->powers[pw_invulnerability])
                R_DrawRingFilter();

            // Crosshair.
            if(!iscam)
                X_Drawer();
        }

        // Draw the automap?
        if(automapactive)
            AM_Drawer();

        // These various HUD's will be drawn unless Doomsday advises not to
        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        {

            // Level information is shown for a few seconds in the
            // beginning of a level.
            R_DrawLevelTitle();

            // Do we need to render a full status bar at this point?
            if (!(automapactive && cfg.automapHudDisplay == 0 )){

                if(!iscam)
                    if(true == (viewheight == 200) )
                    {
                        // Fullscreen. Which mode?
                        ST_Drawer(cfg.setblocks - 10, true);    // $democam
                    } else {
                        ST_Drawer(0 , true);    // $democam
                    }

                    fullscreenmode = viewheight == 200;

                }

            HU_Drawer();
        }

        // Need to update the borders?
        if(oldgamestate != GS_LEVEL ||
            ((Get(DD_VIEWWINDOW_WIDTH) != 320 || menuactive ||
                cfg.sbarscale < 20 || (cfg.sbarscale == 20 && h < targh) || (automapactive && cfg.automapHudDisplay == 0 ))))
        {
            // Update the borders.
            GL_Update(DDUF_BORDER);
        }
        break;

    case GS_INTERMISSION:
        IN_Drawer();
        break;

    case GS_WAITING:
        // Clear the screen while waiting, doesn't mess up the menu.
        gl.Clear(DGL_COLOR_BUFFER_BIT);
        break;

    default:
        break;
    }

    GL_Update(DDUF_FULLSCREEN);

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;

    // draw pause pic (but not if InFine active)
    if(paused && !fi_active)
    {
        if(automapactive)
            ay = 4;
        else
            ay = 4; // in jDOOM this is viewwindowy + 4

        GL_DrawPatch(160, ay, W_GetNumForName("PAUSED"));
    }

    // InFine is drawn whenever active.
    FI_Drawer();
}

/*
   ===============
   =
   = D_AddFile
   =
   ===============
 */

#define MAXWADFILES 20

// MAPDIR should be defined as the directory that holds development maps
// for the -wart # # command

#ifdef __NeXT__
#  define MAPDIR "/Novell/Heretic/data/"
#  define SHAREWAREWADNAME "/Novell/Heretic/source/heretic1.wad"
char   *wadfiles[MAXWADFILES] = {
    "/Novell/Heretic/source/heretic.wad",
    "/Novell/Heretic/data/texture1.lmp",
    "/Novell/Heretic/data/texture2.lmp",
    "/Novell/Heretic/data/pnames.lmp"
};
#else

#  define MAPDIR "\\data\\"

#  define SHAREWAREWADNAME "heretic1.wad"

char   *wadfiles[MAXWADFILES] = {
    "heretic.wad",
    "texture1.lmp",
    "texture2.lmp",
    "pnames.lmp"
};

#endif

char   *basedefault = "heretic.cfg";

char    exrnwads[80];
char    exrnwads2[80];

void wadprintf(void)
{
    if(debugmode)
    {
        return;
    }
#ifdef __WATCOMC__
    _settextposition(23, 2);
    _setbkcolor(1);
    _settextcolor(0);
    _outtext(exrnwads);
    _settextposition(24, 2);
    _outtext(exrnwads2);
#endif
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

//===========================================================================
// DetectIWADs
//  Check which known IWADs are found. The purpose of this routine is to
//  find out which IWADs the user lets us to know about, but we don't
//  decide which one gets loaded or even see if the WADs are actually
//  there. The default location for IWADs is Data\GAMENAMETEXT\.
//===========================================================================
void DetectIWADs(void)
{
    // Add a couple of probable locations for Heretic.wad.
    DD_AddIWAD("}Data\\"GAMENAMETEXT"\\Heretic.wad");
    DD_AddIWAD("}Data\\Heretic.wad");
    DD_AddIWAD("}Heretic.wad");
    DD_AddIWAD("Heretic.wad");
}

/*
 * Set the game mode string.
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
    cfg.mouseSensiX = 8;
    cfg.mouseSensiY = 8;
    cfg.joyaxis[0] = JOYAXIS_TURN;
    cfg.joyaxis[1] = JOYAXIS_MOVE;
    cfg.screenblocks = cfg.setblocks = 10;
    cfg.ringFilter = 1;
    cfg.eyeHeight = 41;
    cfg.menuScale = .9f;
    cfg.menuColor[0] = deffontRGB[0];   // use the default colour by default.
    cfg.menuColor[1] = deffontRGB[1];
    cfg.menuColor[2] = deffontRGB[2];
    cfg.menuColor2[0] = deffontRGB2[0]; // use the default colour by default.
    cfg.menuColor2[1] = deffontRGB2[1];
    cfg.menuColor2[2] = deffontRGB2[2];
    cfg.menuEffects = 1;
    cfg.menuFog = 4;
    cfg.menuSlam = true;
    cfg.flashcolor[0] = .7f;
    cfg.flashcolor[1] = .9f;
    cfg.flashcolor[2] = 1;
    cfg.flashspeed = 4;
    cfg.turningSkull = false;
    cfg.sbarscale = 20;
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
    cfg.usePatchReplacement = true;

    cfg.tomeCounter = 10;
    cfg.tomeSound = 3;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 2;
    cfg.xhairSize = 1;
    for(i = 0; i < 4; i++)
        cfg.xhairColor[i] = 255;
    cfg.netJumping = true;
    cfg.netEpisode = 1;
    cfg.netMap = 1;
    cfg.netSkill = sk_medium;
    cfg.netColor = 4;           // Use the default color by default.
    cfg.levelTitle = true;
    cfg.customMusic = true;

    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;

    cfg.cameraNoClip = true;
    cfg.bobView = cfg.bobWeapon = 1;
    cfg.jumpPower = 9;

    cfg.statusbarAlpha = 1;
    cfg.statusbarCounterAlpha = 1;

    cfg.automapPos = 5;
    cfg.automapWidth = 1.0f;
    cfg.automapHeight = 1.0f;

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

    cfg.secretMsg = true;
    cfg.weaponAutoSwitch = true;

    cfg.weaponOrder[0] = wp_mace;
    cfg.weaponOrder[1] = wp_phoenixrod;
    cfg.weaponOrder[2] = wp_skullrod;
    cfg.weaponOrder[3] = wp_blaster;
    cfg.weaponOrder[4] = wp_crossbow;
    cfg.weaponOrder[5] = wp_goldwand;
    cfg.weaponOrder[6] = wp_gauntlets;
    cfg.weaponOrder[7] = wp_staff;
    cfg.weaponOrder[8] = wp_beak;

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
    noartiskip = ArgCheck("-noartiskip");
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

void H_Ticker(void)
{
    MN_Ticker();
    G_Ticker();
}

char *G_Get(int id)
{
    switch (id)
    {
    case DD_GAME_NAME:
        return GAMENAMETEXT;

    case DD_GAME_ID:
        return GAMENAMETEXT " " VERSION_TEXT;

    case DD_GAME_MODE:
        return gameModeString;

    case DD_GAME_CONFIG:
        return gameConfigString;

    case DD_VERSION_SHORT:
        return VERSION_TEXT;

    case DD_VERSION_LONG:
        return VERSIONTEXT
            "\n"GAMENAMETEXT" is based on Heretic v1.3 by Raven Software.";

    case DD_ACTION_LINK:
        return (char *) actionlinks;

    case DD_ALT_MOBJ_THINKER:
        return (char *) P_BlasterMobjThinker;

    case DD_PSPRITE_BOB_X:
        return (char *) (FRACUNIT +
                         FixedMul(FixedMul
                                  (FRACUNIT * cfg.bobWeapon,
                                   players[consoleplayer].bob),
                                  finecosine[(128 * leveltime) & FINEMASK]));

    case DD_PSPRITE_BOB_Y:
        return (char *) (32 * FRACUNIT +
                         FixedMul(FixedMul
                                  (FRACUNIT * cfg.bobWeapon,
                                   players[consoleplayer].bob),
                                  finesine[(128 *
                                            leveltime) & FINEMASK & (FINEANGLES
                                                                     / 2 -
                                                                     1)]));
    case DD_XGFUNC_LINK:
        return (char *) xgClasses;

    }
    // ID not recognized, return NULL.
    return 0;
}

void H_EndFrame(void)
{
    //  S_UpdateSounds(players[displayplayer].plr->mo);
}

void H_ConsoleBg(int *width, int *height)
{
    extern int consoleFlat;
    extern float consoleZoom;

    GL_SetFlat(consoleFlat + W_CheckNumForName("F_START") + 1);
    *width = (int) (64 * consoleZoom);
    *height = (int) (64 * consoleZoom);
}

void H_Shutdown(void)
{
    HU_UnloadData();
}

/*
   void G_DiscardTiccmd(ticcmd_t *discarded, ticcmd_t *current)
   {
   // We're only interested in buttons.
   // Old Attack and Use buttons apply, if they're set.
   current->buttons |= discarded->buttons & (BT_ATTACK | BT_USE);
   if(discarded->buttons & BT_SPECIAL || current->buttons & BT_SPECIAL)
   return;
   if(discarded->buttons & BT_CHANGE && !(current->buttons & BT_CHANGE))
   {
   // Use the old weapon change.
   current->buttons |= discarded->buttons & (BT_CHANGE | BT_WEAPONMASK);
   }
   }
 */

/*
 * Takes a copy of the engine's entry points and exported data. Returns
 * a pointer to the structure that contains our entry points and exports.
 */
game_export_t *GetGameAPI(game_import_t * imports)
{
    // Take a copy of the imports, but only copy as much data as is
    // allowed and legal.
    memset(&gi, 0, sizeof(gi));
    memcpy(&gi, imports, MIN_OF(sizeof(game_import_t), imports->apiSize));

    // Clear all of our exports.
    memset(&gx, 0, sizeof(gx));

    // Fill in the data for the exports.
    gx.apiSize = sizeof(gx);
    gx.PreInit = H_PreInit;
    gx.PostInit = H_PostInit;
    gx.Shutdown = H_Shutdown;
    gx.BuildTicCmd = (void (*)(void*, float)) G_BuildTiccmd;
    gx.MergeTicCmd = (void (*)(void*, void*)) G_MergeTiccmd;
    gx.G_Drawer = D_Display;
    gx.Ticker = H_Ticker;
    gx.MN_Drawer = M_Drawer;
    gx.PrivilegedResponder = (boolean (*)(event_t *)) D_PrivilegedResponder;
    gx.FallbackResponder = M_Responder;
    gx.G_Responder = G_Responder;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = (fixed_t (*)(void *)) P_GetMobjFriction;
    gx.EndFrame = H_EndFrame;
    gx.ConsoleBackground = H_ConsoleBg;
    gx.UpdateState = G_UpdateState;
#undef Get
    gx.Get = G_Get;

    gx.R_Init = R_InitTranslationTables;

    gx.NetServerStart = D_NetServerStarted;
    gx.NetServerStop = D_NetServerClose;
    gx.NetConnect = D_NetConnect;
    gx.NetDisconnect = D_NetDisconnect;
    gx.NetPlayerEvent = D_NetPlayerEvent;
    gx.NetWorldEvent = D_NetWorldEvent;
    gx.HandlePacket = D_HandlePacket;

    // The structure sizes.
    gx.ticcmd_size = sizeof(ticcmd_t);

    gx.SetupForThings = P_SetupForThings;
    gx.SetupForLines = P_SetupForLines;
    gx.SetupForSides = P_SetupForSides;
    gx.SetupForSectors = P_SetupForSectors;

    gx.HandleMapDataProperty = P_HandleMapDataProperty;

    return &gx;
}
