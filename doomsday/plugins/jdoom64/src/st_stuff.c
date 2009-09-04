/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 * st_stuff.c: Fullscreen HUD code.
 *
 * Does palette indicators as well (red pain/berserk, bright pickup)
 */

 // HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "jdoom64.h"

#include "d_net.h"
#include "st_lib.h"
#include "hu_stuff.h"
#include "am_map.h"
#include "p_tick.h" // for P_IsPaused
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// Radiation suit, green shift.
#define RADIATIONPAL        13

// Frags pos.
#define ST_FRAGSX           138
#define ST_FRAGSY           171
#define ST_FRAGSWIDTH       2

// TYPES -------------------------------------------------------------------

typedef struct {
    boolean         stopped;
    int             hideTics;
    float           hideAmount;
    float           alpha; // Fullscreen hud alpha value.

    boolean         firstTime;  // ST_Start() has just been called.
    boolean         blended; // Whether to use alpha blending.
    boolean         statusbarActive; // Whether the HUD is on.
    int             currentFragsCount; // Number of frags so far in deathmatch.

    // Widgets:
    st_number_t     wFrags; // In deathmatch only, summary of frags stats.
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

// 0-9, tall numbers.
static dpatch_t tallnum[10];

// CVARs for the HUD/Statusbar.
cvar_t sthudCVars[] =
{
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 10},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1},

    // HUD icons
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1},
    {"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1},
    {"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1},
    {"hud-power", 0, CVT_BYTE, &cfg.hudShown[HUD_INVENTORY], 0, 1},

    // HUD displays
    {"hud-frags", 0, CVT_BYTE, &cfg.hudShown[HUD_FRAGS], 0, 1},

    {"hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the HUD/Status bar
 */
void ST_Register(void)
{
    uint                i;

    for(i = 0; sthudCVars[i].name; ++i)
        Con_AddVariable(sthudCVars + i);
}

/**
 * Unhides the current HUD display if hidden.
 *
 * @param player        The player whoose HUD to (maybe) unhide.
 * @param event         The HUD Update Event type to check for triggering.
 */
void ST_HUDUnHide(int player, hueevent_t ev)
{
    player_t*           plr;

    if(ev < HUE_FORCE || ev > NUMHUDUNHIDEEVENTS)
        return;

    plr = &players[player];
    if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
        return;

    if(ev == HUE_FORCE || cfg.hudUnHide[ev])
    {
        hudStates[player].hideTics = (cfg.hudTimer * TICSPERSEC);
        hudStates[player].hideAmount = 0;
    }
}

void ST_updateWidgets(int player)
{
    int                 i;
    player_t*           plr = &players[player];
    hudstate_t*         hud = &hudStates[player];

    // Used by wFrags widget.
    hud->currentFragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->currentFragsCount += plr->frags[i] * (i != player ? 1 : -1);
    }
}

void ST_Ticker(void)
{
    int                 i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           plr = &players[i];
        hudstate_t*         hud = &hudStates[i];

        if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
            continue;

        if(!P_IsPaused())
        {
            if(cfg.hudTimer == 0)
            {
                hud->hideTics = hud->hideAmount = 0;
            }
            else
            {
                if(hud->hideTics > 0)
                    hud->hideTics--;
                if(hud->hideTics == 0 && cfg.hudTimer > 0 && hud->hideAmount < 1)
                    hud->hideAmount += 0.1f;
            }

            ST_updateWidgets(i);
        }
    }
}

void ST_doPaletteStuff(int player)
{
    int                 palette = 0, cnt, bzc;
    player_t*           plr = &players[player];

    cnt = plr->damageCount;

    if(plr->powers[PT_STRENGTH])
    {   // Slowly fade the berzerk out.
        bzc = 12 - (plr->powers[PT_STRENGTH] >> 6);

        if(bzc > cnt)
            cnt = bzc;
    }

    if(cnt)
    {
        palette = (cnt + 7) >> 3;

        if(palette >= NUMREDPALS)
            palette = NUMREDPALS - 1;

        palette += STARTREDPALS;
    }
    else if(plr->bonusCount)
    {
        palette = (plr->bonusCount + 7) >> 3;
        if(palette >= NUMBONUSPALS)
            palette = NUMBONUSPALS - 1;
        palette += STARTBONUSPALS;
    }
    else if(plr->powers[PT_IRONFEET] > 4 * 32 ||
            plr->powers[PT_IRONFEET] & 8)
    {
        palette = RADIATIONPAL;
    }

    // $democam
    if(palette)
    {
        plr->plr->flags |= DDPF_VIEW_FILTER;
        R_GetFilterColor(plr->plr->filterColor, palette);
    }
    else
        plr->plr->flags &= ~DDPF_VIEW_FILTER;
}

static void drawWidgets(hudstate_t* hud)
{
    if(deathmatch)
        STlib_DrawNum(&hud->wFrags, hud->alpha);
}

void ST_doRefresh(int player)
{
    hudstate_t*         hud;

    if(player < 0 || player > MAXPLAYERS)
        return;

    hud = &hudStates[player];

    hud->firstTime = false;

    drawWidgets(hud);
}

void ST_HUDSpriteSize(int sprite, int *w, int *h)
{
    spriteinfo_t        sprInfo;

    R_GetSpriteInfo(sprite, 0, &sprInfo);
    *w = sprInfo.width;
    *h = sprInfo.height;
}

void ST_drawHUDSprite(int sprite, float x, float y, hotloc_t hotspot,
                      float scale, float alpha, boolean flip)
{
    int                 w, h, w2, h2;
    float               s, t;
    spriteinfo_t        sprInfo;

    if(!(alpha > 0))
        return;

    alpha = MINMAX_OF(0.f, alpha, 1.f);
    R_GetSpriteInfo(sprite, 0, &sprInfo);
    w = sprInfo.width;
    h = sprInfo.height;
    w2 = M_CeilPow2(w);
    h2 = M_CeilPow2(h);

    switch(hotspot)
    {
    case HOT_BRIGHT:
        y -= h * scale;

    case HOT_TRIGHT:
        x -= w * scale;
        break;

    case HOT_BLEFT:
        y -= h * scale;
        break;
    }

    DGL_SetPSprite(sprInfo.material);

    // Let's calculate texture coordinates.
    // To remove a possible edge artifact, move the corner a bit up/left.
    s = (w - 0.4f) / w2;
    t = (h - 0.4f) / h2;

    DGL_Color4f(1, 1, 1, alpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, flip * s, 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, !flip * s, 0);
        DGL_Vertex2f(x + w * scale, y);

        DGL_TexCoord2f(0, !flip * s, t);
        DGL_Vertex2f(x + w * scale, y + h * scale);

        DGL_TexCoord2f(0, flip * s, t);
        DGL_Vertex2f(x, y + h * scale);
    DGL_End();
}

void ST_doFullscreenStuff(int player)
{
    static const int    ammo_sprite[NUM_AMMO_TYPES] = {
        SPR_AMMO,
        SPR_SBOX,
        SPR_CELL,
        SPR_RCKT
    };

    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];
    char                buf[20];
    int                 w, h, pos = 0, oldPos = 0, spr,i;
    int                 h_width = 320 / cfg.hudScale;
    int                 h_height = 200 / cfg.hudScale;
    float               textalpha =
        hud->alpha - hud->hideAmount - ( 1 - cfg.hudColor[3]);
    float               iconalpha =
        hud->alpha - hud->hideAmount - ( 1 - cfg.hudIconAlpha);

    textalpha = MINMAX_OF(0.f, textalpha, 1.f);
    iconalpha = MINMAX_OF(0.f, iconalpha, 1.f);

    if(IS_NETGAME && deathmatch && cfg.hudShown[HUD_FRAGS])
    {
        // Display the frag counter.
        i = 199 - HUDBORDERY;
        if(cfg.hudShown[HUD_HEALTH])
        {
            i -= 18 * cfg.hudScale;
        }
        sprintf(buf, "FRAGS:%i", hud->currentFragsCount);
        M_WriteText2(HUDBORDERX, i, buf, GF_FONTA, cfg.hudColor[0], cfg.hudColor[1],
                     cfg.hudColor[2], textalpha);
    }

    // Setup the scaling matrix.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(cfg.hudScale, cfg.hudScale, 1);

    // Draw the visible HUD data, first health.
    if(cfg.hudShown[HUD_HEALTH])
    {
        sprintf(buf, "HEALTH");
        pos = M_StringWidth(buf, GF_FONTA)/2;
        M_WriteText2(HUDBORDERX, h_height - HUDBORDERY - M_StringHeight("A", GF_FONTA) - 4,
                     buf, GF_FONTA, 1, 1, 1, iconalpha);

        sprintf(buf, "%i", plr->health);
        M_WriteText2(HUDBORDERX + pos - (M_StringWidth(buf, GF_FONTB)/2),
                     h_height - HUDBORDERY, buf, GF_FONTB,
                     cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);

        oldPos = pos;
        pos = HUDBORDERX * 2 + M_StringWidth(buf, GF_FONTB);
    }

    // Keys  | use a bit of extra scale.
    if(cfg.hudShown[HUD_KEYS])
    {
Draw_BeginZoom(0.75f, pos , h_height - HUDBORDERY);
        for(i = 0; i < 3; ++i)
        {
            spr = 0;
            if(plr->
               keys[i == 0 ? KT_REDCARD : i ==
                     1 ? KT_YELLOWCARD : KT_BLUECARD])
                spr = i == 0 ? SPR_RKEY : i == 1 ? SPR_YKEY : SPR_BKEY;
            if(plr->
               keys[i == 0 ? KT_REDSKULL : i ==
                     1 ? KT_YELLOWSKULL : KT_BLUESKULL])
                spr = i == 0 ? SPR_RSKU : i == 1 ? SPR_YSKU : SPR_BSKU;
            if(spr)
            {
                ST_drawHUDSprite(spr, pos, h_height - 2,
                                 HOT_BLEFT, 1, iconalpha, false);
                ST_HUDSpriteSize(spr, &w, &h);
                pos += w + 2;
            }
        }
Draw_EndZoom();
    }
    pos = oldPos;

    // Inventory
    if(cfg.hudShown[HUD_INVENTORY])
    {
        if(P_InventoryCount(player, IIT_DEMONKEY1))
        {
            spr = SPR_ART1;
            ST_HUDSpriteSize(spr, &w, &h);
            ST_drawHUDSprite(spr, HUDBORDERX + pos -w/2, h_height - 44,
                             HOT_BLEFT, 1, iconalpha, false);
        }

        if(P_InventoryCount(player, IIT_DEMONKEY2))
        {
            spr = SPR_ART2;
            ST_HUDSpriteSize(spr, &w, &h);
            ST_drawHUDSprite(spr, HUDBORDERX + pos -w/2, h_height - 84,
                             HOT_BLEFT, 1, iconalpha, false);
        }

        if(P_InventoryCount(player, IIT_DEMONKEY3))
        {
            spr = SPR_ART3;
            ST_HUDSpriteSize(spr, &w, &h);
            ST_drawHUDSprite(spr, HUDBORDERX + pos -w/2, h_height - 124,
                             HOT_BLEFT, 1, iconalpha, false);
        }
    }

    if(cfg.hudShown[HUD_AMMO])
    {
        ammotype_t          ammotype;

        //// \todo Only supports one type of ammo per weapon.
        //// for each type of ammo this weapon takes.
        for(ammotype=0; ammotype < NUM_AMMO_TYPES; ++ammotype)
        {
            if(!weaponInfo[plr->readyWeapon][plr->class].mode[0].ammoType[ammotype])
                continue;

            sprintf(buf, "%i", plr->ammo[ammotype]);
            pos = (h_width/2) - (M_StringWidth(buf, GF_FONTB)/2);
            M_WriteText2(pos, h_height - HUDBORDERY, buf, GF_FONTB,
                         cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
            break;
        }
    }

    pos = h_width - 1;
    if(cfg.hudShown[HUD_ARMOR])
    {
        sprintf(buf, "ARMOR");
        w = M_StringWidth(buf, GF_FONTA);
        M_WriteText2(h_width - w - HUDBORDERX,
                     h_height - HUDBORDERY - M_StringHeight("A", GF_FONTA) - 4,
                     buf, GF_FONTA, 1, 1, 1, iconalpha);

        sprintf(buf, "%i", plr->armorPoints);
        M_WriteText2(h_width - (w/2) - (M_StringWidth(buf, GF_FONTB)/2) - HUDBORDERX,
                     h_height - HUDBORDERY,
                     buf, GF_FONTB, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2],
                     textalpha);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ST_Drawer(int player, int fullscreenmode)
{
    hudstate_t*         hud;
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    hud->firstTime = hud->firstTime;
    hud->statusbarActive = (fullscreenmode < 2) ||
        (AM_IsActive(AM_MapForPlayer(player)) &&
         (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts.
    ST_doPaletteStuff(player);

    // Fade in/out the fullscreen HUD.
    if(hud->statusbarActive)
    {
        if(hud->alpha > 0.0f)
        {
            hud->statusbarActive = 0;
            hud->alpha-=0.1f;
        }
    }
    else
    {
        if(fullscreenmode == 3)
        {
            if(hud->alpha > 0.0f)
            {
                hud->alpha-=0.1f;
                fullscreenmode = 2;
            }
        }
        else
        {
            if(hud->alpha < 1.0f)
                hud->alpha += 0.1f;
        }
    }

    // Always try to render statusbar with alpha in fullscreen modes.
    if(fullscreenmode)
        hud->blended = 1;
    else
        hud->blended = 0;

    ST_doFullscreenStuff(player);
}

void ST_loadGraphics(void)
{
    int                 i;
    char                namebuf[9];

    // Load the numbers, tall and short.
    for(i = 0; i < 10; ++i)
    {
        sprintf(namebuf, "STTNUM%d", i);
        R_CachePatch(&tallnum[i], namebuf);
    }
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

static void initData(hudstate_t* hud)
{
    int                 player = hud - hudStates;

    hud->firstTime = true;
    hud->statusbarActive = true;
    hud->blended = false;
    hud->stopped = true;
    hud->alpha = 0.f;

    ST_HUDUnHide(player, HUE_FORCE);
}

void ST_createWidgets(int player)
{
    hudstate_t*         hud = &hudStates[player];

    // Frags sum.
    STlib_InitNum(&hud->wFrags, ST_FRAGSX, ST_FRAGSY, tallnum, &hud->currentFragsCount,
                  ST_FRAGSWIDTH, 1);
}

void ST_Start(int player)
{
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];

    if(!hud->stopped)
        ST_Stop(player);

    initData(hud);
    ST_createWidgets(player);

    hud->stopped = false;
}

void ST_Stop(int player)
{
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];
    if(hud->stopped)
        return;

    hud->stopped = true;
}

void ST_Init(void)
{
    ST_loadData();
}
