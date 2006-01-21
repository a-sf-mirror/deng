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
 * Multiplayer Menu (for jDoom, jHeretic and jHexen)
 * Contains an extension for edit fields.
 * TODO: Remove unnecessary SC* declarations and other unused code.
 */

// HEADER FILES ------------------------------------------------------------

#if defined __JDOOM__
#  include "doomdef.h"
#  include "doomstat.h"
#  include "m_menu.h"
#  include "d_config.h"
#  include "m_misc.h"
#  include "m_random.h"
#  include "s_sound.h"
#  include "dstrings.h"
#  include "p_local.h"
#  include "Mn_def.h"
#elif defined __JHERETIC__
#  include "jHeretic/Doomdef.h"
#  include "h_config.h"
#  include "jHeretic/Soundst.h"
#  include "jHeretic/P_local.h"
#  include "jHeretic/Mn_def.h"
#elif defined __JHEXEN__
#  include "jHexen/h2def.h"
#  include "x_config.h"
#  include "jHexen/soundst.h"
#  include "jHexen/p_local.h"
#  include "jHexen/mn_def.h"
#elif defined __JSTRIFE__
#  include "jStrife/h2def.h"
#  include "jStrife/d_config.h"
#  include "jStrife/soundst.h"
#  include "jStrife/p_local.h"
#  include "jStrife/mn_def.h"
#endif

#include "Common/hu_stuff.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>

// MACROS ------------------------------------------------------------------

#define SLOT_WIDTH          200

#define IS_SERVER           Get(DD_SERVER)
#define IS_NETGAME          Get(DD_NETGAME)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    DrawMultiplayerMenu(void);
void    DrawGameSetupMenu(void);
void    DrawPlayerSetupMenu(void);

void    SCEnterHostMenu(int option, void *data);
void    SCEnterJoinMenu(int option, void *data);
void    SCEnterGameSetup(int option, void *data);
void    SCSetProtocol(int option, void *data);
void    SCGameSetupFunc(int option, void *data);
void    SCGameSetupEpisode(int option, void *data);
void    SCGameSetupMission(int option, void *data);
void    SCGameSetupSkill(int option, void *data);
void    SCGameSetupDeathmatch(int option, void *data);
void    SCGameSetupDamageMod(int option, void *data);
void    SCGameSetupHealthMod(int option, void *data);
void    SCOpenServer(int option, void *data);
void    SCCloseServer(int option, void *data);
void    SCChooseHost(int option, void *data);
void    SCStartStopDisconnect(int option, void *data);
void    SCEnterPlayerSetupMenu(int option, void *data);
void    SCPlayerClass(int option, void *data);
void    SCPlayerColor(int option, void *data);
void    SCAcceptPlayer(int option, void *data);

void    ResetJoinMenuItems();

// Edit fields.
void    DrawEditField(Menu_t * menu, int index, EditField_t * ef);
void    SCEditField(int efptr, void *data);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float menu_alpha;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

EditField_t *ActiveEdit = NULL; // No active edit field by default.
EditField_t plrNameEd;
int     CurrentPlrFrame = 0;

MenuItem_t MultiplayerItems[] = {
    {ITT_EFUNC, 0, "player setup", SCEnterPlayerSetupMenu, 0 },
    {ITT_EFUNC, 0, "join game", SCEnterJoinMenu, 0 },
    {ITT_EFUNC, 0, "host game", SCEnterHostMenu, 0 },
};

MenuItem_t MultiplayerServerItems[] = {
    {ITT_EFUNC, 0, "player setup", SCEnterPlayerSetupMenu, 0 },
    {ITT_EFUNC, 0, "game setup", SCEnterHostMenu, 0 },
    {ITT_EFUNC, 0, "close server", SCCloseServer, 0 }
};

MenuItem_t MultiplayerClientItems[] = {
    {ITT_EFUNC, 0, "player setup", SCEnterPlayerSetupMenu, 0 },
    {ITT_EFUNC, 0, "disconnect", SCEnterJoinMenu, 0 }
};

Menu_t  MultiplayerMenu = {
    116, 70,
    DrawMultiplayerMenu,
    3, MultiplayerItems,
    0, MENU_MAIN, 0,
    hu_font_a,cfg.menuColor2,
    LINEHEIGHT_A,
    0, 3
};

#if __JHEXEN__ || __JSTRIFE__

#  define NUM_GAMESETUP_ITEMS       9

MenuItem_t GameSetupItems1[] = {
    {ITT_LRFUNC, 0, "MAP:", SCGameSetupMission, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_LRFUNC, 0, "SKILL:", SCGameSetupSkill, 0},
    {ITT_EFUNC, 0, "DEATHMATCH:", SCGameSetupFunc, 0, NULL, &cfg.netDeathmatch },
    {ITT_EFUNC, 0, "MONSTERS:", SCGameSetupFunc, 0, NULL, &cfg.netNomonsters },
    {ITT_EFUNC, 0, "RANDOM CLASSES:", SCGameSetupFunc, 0, NULL, &cfg.netRandomclass },
    {ITT_LRFUNC, 0, "DAMAGE MOD:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MOD:", SCGameSetupHealthMod, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0}
};

#else

#  if __JHERETIC__

#    define NUM_GAMESETUP_ITEMS     10

MenuItem_t GameSetupItems1[] =  // for Heretic
{
    {ITT_LRFUNC, 0, "EPISODE :", SCGameSetupEpisode, 0},
    {ITT_LRFUNC, 0, "MISSION :", SCGameSetupMission, 0},
    {ITT_LRFUNC, 0, "SKILL :", SCGameSetupSkill, 0},
    {ITT_LRFUNC, 0, "DEATHMATCH :", SCGameSetupDeathmatch, 0},
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netNomonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netRespawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &cfg.netJumping },
    {ITT_LRFUNC, 0, "DAMAGE MOD:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MOD:", SCGameSetupHealthMod, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0 }
};

#  elif __JDOOM__

#    define NUM_GAMESETUP_ITEMS     16

MenuItem_t GameSetupItems1[] =  // for Doom 1
{
    {ITT_LRFUNC, 0, "EPISODE :", SCGameSetupEpisode, 0},
    {ITT_LRFUNC, 0, "MISSION :", SCGameSetupMission, 0},
    {ITT_LRFUNC, 0, "SKILL :", SCGameSetupSkill, 0},
    {ITT_LRFUNC, 0, "MODE :", SCGameSetupDeathmatch, 0},
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netNomonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netRespawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &cfg.netJumping },
    {ITT_EFUNC, 0, "ALLOW BFG AIMING :", SCGameSetupFunc, 0, NULL, &cfg.netBFGFreeLook},
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noCoopDamage },
    {ITT_EFUNC, 0, "NO COOP WEAPONS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopWeapons },
    {ITT_EFUNC, 0, "NO COOP OBJECTS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopAnything },
    {ITT_EFUNC, 0, "NO BFG 9000 :", SCGameSetupFunc, 0, NULL, &cfg.noNetBFG },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noTeamDamage },
    {ITT_LRFUNC, 0, "DAMAGE MOD:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MOD:", SCGameSetupHealthMod, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0 }
};

MenuItem_t GameSetupItems2[] =  // for Doom 2
{
    {ITT_LRFUNC, 0, "LEVEL :", SCGameSetupMission, 0},
    {ITT_LRFUNC, 0, "SKILL :", SCGameSetupSkill, 0},
    {ITT_LRFUNC, 0, "MODE :", SCGameSetupDeathmatch, 0},
    {ITT_EFUNC, 0, "MONSTERS :", SCGameSetupFunc, 0, NULL,  &cfg.netNomonsters },
    {ITT_EFUNC, 0, "RESPAWN MONSTERS :", SCGameSetupFunc, 0, NULL, &cfg.netRespawn },
    {ITT_EFUNC, 0, "ALLOW JUMPING :", SCGameSetupFunc, 0, NULL, &cfg.netJumping },
    {ITT_EFUNC, 0, "ALLOW BFG AIMING :", SCGameSetupFunc, 0, NULL, &cfg.netBFGFreeLook},
    {ITT_EFUNC, 0, "NO COOP DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noCoopDamage },
    {ITT_EFUNC, 0, "NO COOP WEAPONS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopWeapons },
    {ITT_EFUNC, 0, "NO COOP OBJECTS :", SCGameSetupFunc, 0, NULL, &cfg.noCoopAnything },
    {ITT_EFUNC, 0, "NO BFG 9000 :", SCGameSetupFunc, 0, NULL, &cfg.noNetBFG },
    {ITT_EFUNC, 0, "NO TEAM DAMAGE :", SCGameSetupFunc, 0, NULL, &cfg.noTeamDamage },
    {ITT_LRFUNC, 0, "DAMAGE MOD:", SCGameSetupDamageMod, 0},
    {ITT_LRFUNC, 0, "HEALTH MOD:", SCGameSetupHealthMod, 0},
    {ITT_EFUNC, 0, "PROCEED...", SCOpenServer, 0 }
};

#  endif

#endif

Menu_t  GameSetupMenu = {
#  if __JDOOM__
    90, 54,
#  elif __JHERETIC__
    74, 64,
#  elif __JHEXEN__  || __JSTRIFE__
    90, 64,
#  endif
    DrawGameSetupMenu,
    NUM_GAMESETUP_ITEMS, GameSetupItems1,
    0, MENU_MULTIPLAYER, 0,
    hu_font_a,                  //1, 0, 0,
    cfg.menuColor2,
    LINEHEIGHT_A,
    0, NUM_GAMESETUP_ITEMS
};



#if __JHEXEN__ || __JSTRIFE__
#  define NUM_PLAYERSETUP_ITEMS 6
#else
#  define NUM_PLAYERSETUP_ITEMS 6
#endif

MenuItem_t PlayerSetupItems[] = {
    {ITT_EFUNC, 0, "", SCEditField, 0, NULL, &plrNameEd },
    {ITT_EMPTY, 0, NULL, NULL, 0},
#if __JHEXEN__
    {ITT_LRFUNC, 0, "Class:", SCPlayerClass, 0},
#else
    {ITT_EMPTY, 0, NULL, NULL, 0},
#endif
    {ITT_LRFUNC, 0, "Color:", SCPlayerColor, 0},
    {ITT_EMPTY, 0, NULL, NULL, 0},
    {ITT_EFUNC, 0, "Accept Changes", SCAcceptPlayer, 0 }
};

Menu_t  PlayerSetupMenu = {
    60, 52,
    DrawPlayerSetupMenu,
    NUM_PLAYERSETUP_ITEMS, PlayerSetupItems,
    0, MENU_MULTIPLAYER, 0,
    hu_font_b, cfg.menuColor, LINEHEIGHT_B,
    0, NUM_PLAYERSETUP_ITEMS
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int plrColor;

#if __JHEXEN__
static int plrClass;
#endif

// CODE --------------------------------------------------------------------

int Executef(int silent, char *format, ...)
{
    va_list argptr;
    char    buffer[512];

    va_start(argptr, format);
    vsprintf(buffer, format, argptr);
    va_end(argptr);
    return DD_Execute(buffer, silent);
}

void DrANumber(int number, int x, int y)
{
    char    buff[40];

    sprintf(buff, "%i", number);

    M_WriteText2(x, y, buff, hu_font_a, 1, 1, 1, menu_alpha);
}

void MN_DrCenterTextA_CS(char *text, int center_x, int y)
{
    M_WriteText2(center_x - M_StringWidth(text, hu_font_a) / 2, y, text,
                 hu_font_a, 1, 0, 0, menu_alpha);
}

void MN_DrCenterTextB_CS(char *text, int center_x, int y)
{
    M_WriteText2(center_x - M_StringWidth(text, hu_font_b) / 2, y, text,
                 hu_font_b, 1, 0, 0, menu_alpha);
}

void DrawMultiplayerMenu(void)
{
    M_DrawTitle("MULTIPLAYER", MultiplayerMenu.y - 30);
}

void DrawGameSetupMenu(void)
{
    char   *boolText[2] = { "NO", "YES" }, buf[50];
    char   *skillText[5] = { "BABY", "EASY", "MEDIUM", "HARD", "NIGHTMARE" };
#if __JDOOM__
    char   *freeLookText[3] = { "NO", "NOT BFG", "ALL" };
    char   *dmText[3] = { "COOPERATIVE", "DEATHMATCH 1", "DEATHMATCH 2" };
#else
    char   *dmText[3] = { "NO", "YES", "YES" };
#endif
    Menu_t *menu = &GameSetupMenu;
    int     idx;

#if __JHEXEN__
    char   *mapName = P_GetMapName(P_TranslateMap(cfg.netMap));
#elif __JSTRIFE__
    char   *mapName = "unnamed";
#endif

    M_DrawTitle("GAME SETUP", menu->y - 20);

    idx = 0;

#if __JDOOM__ || __JHERETIC__

#  ifdef __JDOOM__
    if(gamemode != commercial)
#  endif
    {
        sprintf(buf, "%i", cfg.netEpisode);
        M_WriteMenuText(menu, idx++, buf);
    }
    sprintf(buf, "%i", cfg.netMap);
    M_WriteMenuText(menu, idx++, buf);
    M_WriteMenuText(menu, idx++, skillText[cfg.netSkill]);
    M_WriteMenuText(menu, idx++, dmText[cfg.netDeathmatch]);
    M_WriteMenuText(menu, idx++, boolText[!cfg.netNomonsters]);
    M_WriteMenuText(menu, idx++, boolText[cfg.netRespawn]);
    M_WriteMenuText(menu, idx++, boolText[cfg.netJumping]);

#  if __JDOOM__
    M_WriteMenuText(menu, idx++, boolText[cfg.netBFGFreeLook]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noCoopDamage]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noCoopWeapons]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noCoopAnything]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noNetBFG]);
    M_WriteMenuText(menu, idx++, boolText[cfg.noTeamDamage]);
#  endif                        // __JDOOM__
    sprintf(buf, "%i", cfg.netMobDamageModifier);
    M_WriteMenuText(menu, idx++, buf);
    sprintf(buf, "%i", cfg.netMobHealthModifier);
    M_WriteMenuText(menu, idx++, buf);

#elif __JHEXEN__ || __JSTRIFE__

    sprintf(buf, "%i", cfg.netMap);
    M_WriteMenuText(menu, 0, buf);
    M_WriteText2(160 - M_StringWidth(mapName, hu_font_a) / 2,
                 menu->y + menu->itemHeight, mapName,
                 hu_font_a, 1, 0.7f, 0.3f, menu_alpha);

    M_WriteMenuText(menu, 2, skillText[cfg.netSkill]);
    M_WriteMenuText(menu, 3, dmText[cfg.netDeathmatch]);
    M_WriteMenuText(menu, 4, boolText[!cfg.netNomonsters]);
    M_WriteMenuText(menu, 5, boolText[cfg.netRandomclass]);
    sprintf(buf, "%i", cfg.netMobDamageModifier);
    M_WriteMenuText(menu, 6, buf);
    sprintf(buf, "%i", cfg.netMobHealthModifier);
    M_WriteMenuText(menu, 7, buf);

#endif                          // __JHEXEN__
}

/*
 * Finds the power of 2 that is equal to or greater than
 * the specified number.
 */
static int CeilPow2(int num)
{
    int     cumul;

    for(cumul = 1; num > cumul; cumul <<= 1);
    return cumul;
}

void DrawPlayerSetupMenu(void)
{
    spriteinfo_t sprInfo;
    Menu_t *menu = &PlayerSetupMenu;
    int     useColor = plrColor;

#if __JHEXEN__
    int     numColors = 8;
    int     sprites[3] = { SPR_PLAY, SPR_CLER, SPR_MAGE };
#else
    int     plrClass = 0;
    int     numColors = 4;
    int     sprites[3] = { SPR_PLAY, SPR_PLAY, SPR_PLAY };
#endif

    M_DrawTitle("PLAYER SETUP", menu->y - 28);

    DrawEditField(menu, 0, &plrNameEd);

    if(useColor == numColors)
        useColor = MenuTime / 5 % numColors;

    // Draw the color selection as a random player frame.
    //#ifndef __JHEXEN__
    //  gi.GetSpriteInfo(SPR_PLAY, CurrentPlrFrame, &sprInfo);
    //#else

    R_GetSpriteInfo(sprites[plrClass], CurrentPlrFrame, &sprInfo);

#if __JHEXEN__
    if(plrClass == PCLASS_FIGHTER)
    {
        // Fighter's colors are a bit different.
        if(useColor == 0)
            useColor = 2;
        else if(useColor == 2)
            useColor = 0;
    }
#endif
    Set(DD_TRANSLATED_SPRITE_TEXTURE,
        DD_TSPR_PARM(sprInfo.lump, plrClass, useColor));

    GL_DrawRect(162 - sprInfo.offset,
#ifdef __JDOOM__
                menu->y + 70 - sprInfo.topOffset,
#elif defined __JHERETIC__
                menu->y + 80 - sprInfo.topOffset,
#else
                menu->y + 90 - sprInfo.topOffset,
#endif
                CeilPow2(sprInfo.width), CeilPow2(sprInfo.height), 1, 1, 1,
                menu_alpha);

    if(plrColor == numColors)
    {
        M_WriteText2(184,
#ifdef __JDOOM__
                      menu->y + 49,
#elif defined __JHERETIC__
                      menu->y + 65,
#else
                      menu->y + 64,
#endif
                      "AUTOMATIC", hu_font_a, 1, 1, 1, menu_alpha);
    }

}

void SCEnterMultiplayerMenu(int option, void *data)
{
    int     count;

    // Choose the correct items for the Game Setup menu.
#ifdef __JDOOM__
    if(gamemode == commercial)
    {
        GameSetupMenu.items = GameSetupItems2;
        GameSetupMenu.itemCount = GameSetupMenu.numVisItems =
            NUM_GAMESETUP_ITEMS - 1;
    }
    else
#endif
    {
        GameSetupMenu.items = GameSetupItems1;
        GameSetupMenu.itemCount = GameSetupMenu.numVisItems =
            NUM_GAMESETUP_ITEMS;
    }

    // Show the appropriate menu.
    if(IS_NETGAME)
    {
        MultiplayerMenu.items =
            IS_SERVER ? MultiplayerServerItems : MultiplayerClientItems;
        count = IS_SERVER ? 3 : 2;
    }
    else
    {
        MultiplayerMenu.items = MultiplayerItems;
        count = 3;
    }
    MultiplayerMenu.itemCount = MultiplayerMenu.numVisItems = count;

    MultiplayerMenu.lastOn = 0;

    SetMenu(MENU_MULTIPLAYER);
}

void SCEnterHostMenu(int option, void *data)
{
    SCEnterGameSetup(0, NULL);
}

void SCEnterJoinMenu(int option, void *data)
{
    if(IS_NETGAME)
    {
        DD_Execute("net disconnect", false);
        M_ClearMenus();
        return;
    }
    DD_Execute("net setup client", false);
}

void SCEnterGameSetup(int option, void *data)
{
    // See to it that the episode and mission numbers are correct.
#ifdef __JDOOM__
    if(gamemode == commercial)
    {
        cfg.netEpisode = 1;
    }
    else if(gamemode == retail)
    {
        if(cfg.netEpisode > 4)
            cfg.netEpisode = 4;
        if(cfg.netMap > 9)
            cfg.netMap = 9;
    }
    else if(gamemode == registered)
    {
        if(cfg.netEpisode > 3)
            cfg.netEpisode = 3;
        if(cfg.netMap > 9)
            cfg.netMap = 9;
    }
    else if(gamemode == shareware)
    {
        cfg.netEpisode = 1;
        if(cfg.netMap > 9)
            cfg.netMap = 9;
    }
#elif defined __JHERETIC__
    if(cfg.netMap > 9)
        cfg.netMap = 9;
    if(cfg.netEpisode > 6)
        cfg.netEpisode = 6;
    if(cfg.netEpisode == 6 && cfg.netMap > 3)
        cfg.netMap = 3;
#elif defined __JHEXEN__ || __JSTRIFE__
    if(cfg.netMap < 1)
        cfg.netMap = 1;
    if(cfg.netMap > 31)
        cfg.netMap = 31;
#endif
    SetMenu(MENU_GAMESETUP);
}

void SCGameSetupFunc(int option, void *data)
{
    byte   *ptr = data;

    *ptr ^= 1;
}

void SCGameSetupDeathmatch(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
#ifdef __JDOOM__
        if(cfg.netDeathmatch < 2)
#else
        if(cfg.netDeathmatch < 1)
#endif
            cfg.netDeathmatch++;
    }
    else if(cfg.netDeathmatch > 0)
    {
        cfg.netDeathmatch--;
    }
}

void SCGameSetupEpisode(int option, void *data)
{
#ifdef __JDOOM__
    if(gamemode == shareware)
    {
        cfg.netEpisode = 1;
        return;
    }
    if(option == RIGHT_DIR)
    {
        if(cfg.netEpisode < (gamemode == retail ? 4 : 3))
            cfg.netEpisode++;
    }
    else if(cfg.netEpisode > 1)
    {
        cfg.netEpisode--;
    }
#elif defined(__JHERETIC__)
    if(shareware)
    {
        cfg.netEpisode = 1;
        return;
    }
    if(option == RIGHT_DIR)
    {
        if(cfg.netEpisode < 6)
            cfg.netEpisode++;
    }
    else if(cfg.netEpisode > 1)
    {
        cfg.netEpisode--;
    }
#endif
    return;
}

void SCGameSetupMission(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
#ifdef __JDOOM__
        if(cfg.netMap < (gamemode == commercial ? 32 : 9))
            cfg.netMap++;
#elif defined __JHERETIC__
        if(cfg.netMap < 9)
            cfg.netMap++;
#elif defined __JHEXEN__ || __JSTRIFE__
        if(cfg.netMap < 31)
            cfg.netMap++;
#endif
    }
    else if(cfg.netMap > 1)
    {
        cfg.netMap--;
    }
    return;
}

void SCGameSetupSkill(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netSkill < 4)
            cfg.netSkill++;
    }
    else if(cfg.netSkill > 0)
    {
        cfg.netSkill--;
    }
    return;
}

void SCOpenServer(int option, void *data)
{
    if(IS_NETGAME)
    {
        // Game already running, just change level.
#if __JHEXEN__ || __JSTRIFE__
        Executef(false, "setmap %i", cfg.netMap);
#else
        Executef(false, "setmap %i %i", cfg.netEpisode, cfg.netMap);
#endif

        M_ClearMenus();
        return;
    }

    // Go to DMI to setup server.
    DD_Execute("net setup server", false);
}

void SCCloseServer(int option, void *data)
{
    DD_Execute("net server close", false);
    M_ClearMenus();
}

void SCEnterPlayerSetupMenu(int option, void *data)
{
    strncpy(plrNameEd.text, *(char **) Con_GetVariable("net-name")->ptr, 255);
    plrColor = cfg.netColor;
#if __JHEXEN__
    plrClass = cfg.netClass;
#endif
    SetMenu(MENU_PLAYERSETUP);
}

#if __JHEXEN__
void SCPlayerClass(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(plrClass < 2)
            plrClass++;
    }
    else if(plrClass > 0)
        plrClass--;
}
#endif

void SCPlayerColor(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
#if __JHEXEN__
        if(plrColor < 8)
            plrColor++;
#else
        if(plrColor < 4)
            plrColor++;
#endif
    }
    else if(plrColor > 0)
        plrColor--;
}

void SCAcceptPlayer(int option, void *data)
{
    char    buf[300];

    cfg.netColor = plrColor;
#if __JHEXEN__
    cfg.netClass = plrClass;
#endif

    strcpy(buf, "net-name ");
    M_StrCatQuoted(buf, plrNameEd.text);
    DD_Execute(buf, false);

    if(IS_NETGAME)
    {
        sprintf(buf, "setname ");
        M_StrCatQuoted(buf, plrNameEd.text);
        DD_Execute(buf, false);
#if __JHEXEN__
        // Must do 'setclass' first; the real class and color do not change
        // until the server sends us a notification -- this means if we do
        // 'setcolor' first, the 'setclass' after it will override the color
        // change (or such would appear to be the case).
        Executef(false, "setclass %i", plrClass);
#endif
        Executef(false, "setcolor %i", plrColor);
    }

    SetMenu(MENU_MULTIPLAYER);
}

void SCGameSetupDamageMod(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netMobDamageModifier < 100)
            cfg.netMobDamageModifier++;
    }
    else if(cfg.netMobDamageModifier > 1)
        cfg.netMobDamageModifier--;
}

void SCGameSetupHealthMod(int option, void *data)
{
    if(option == RIGHT_DIR)
    {
        if(cfg.netMobHealthModifier < 20)
            cfg.netMobHealthModifier++;
    }
    else if(cfg.netMobHealthModifier > 1)
        cfg.netMobHealthModifier--;
}

void MN_TickerEx(void)          // The extended ticker.
{
    static int FrameTimer = 0;

    if(currentMenu == &PlayerSetupMenu)
    {
        if(FrameTimer++ >= 14)
        {
            FrameTimer = 0;
            CurrentPlrFrame = M_Random() % 8;
        }
    }
}

int Ed_VisibleSlotChars(char *text, int (*widthFunc) (char *text, dpatch_t *font))
{
    char    cbuf[2] = { 0, 0 };
    int     i, w;

    for(i = 0, w = 0; text[i]; i++)
    {
        cbuf[0] = text[i];
        w += widthFunc(cbuf, hu_font_a);
        if(w > SLOT_WIDTH)
            break;
    }
    return i;
}

void Ed_MakeCursorVisible()
{
    char    buf[MAX_EDIT_LEN + 1];
    int     i, len, vis;

    strcpy(buf, ActiveEdit->text);
    strupr(buf);
    strcat(buf, "_");           // The cursor.
    len = strlen(buf);
    for(i = 0; i < len; i++)
    {
        vis = Ed_VisibleSlotChars(buf + i, M_StringWidth);

        if(i + vis >= len)
        {
            ActiveEdit->firstVisible = i;
            break;
        }
    }
}

void DrawEditField(Menu_t * menu, int index, EditField_t * ef)
{
    int     x = menu->x, y = menu->y + menu->itemHeight * index, vis;
    char    buf[MAX_EDIT_LEN + 1], *text;


    M_DrawSaveLoadBorder(x + 11, y + 5);
    strcpy(buf, ef->text);
    strupr(buf);
    if(ActiveEdit == ef && MenuTime & 0x8)
        strcat(buf, "_");
    text = buf + ef->firstVisible;
    vis = Ed_VisibleSlotChars(text, M_StringWidth);
    text[vis] = 0;
    M_WriteText2(x + 8, y + 5, text, hu_font_a, 1, 1, 1, menu_alpha);

}

void SCEditField(int efptr, void *data)
{
    EditField_t *ef = data;

    // Activate this edit field.
    ActiveEdit = ef;
    strcpy(ef->oldtext, ef->text);
    Ed_MakeCursorVisible();
}
