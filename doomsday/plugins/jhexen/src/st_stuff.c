/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * st_stuff.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>

#include "jhexen.h"
#include "st_lib.h"
#include "d_net.h"

#include "p_tick.h" // for P_IsPaused
#include "am_map.h"
#include "g_common.h"
#include "p_inventory.h"
#include "hu_lib.h"
#include "hu_inventory.h"

// MACROS ------------------------------------------------------------------

// Inventory
#define ST_INVENTORYX       (50)
#define ST_INVENTORYY       (1)

// Current inventory item.
#define ST_INVITEMX         (143)
#define ST_INVITEMY         (1)

// Current inventory item count.
#define ST_INVITEMCWIDTH    (2) // Num digits
#define ST_INVITEMCX        (174)
#define ST_INVITEMCY        (22)

// HEALTH number pos.
#define ST_HEALTHWIDTH          3
#define ST_HEALTHX              64
#define ST_HEALTHY              14

// MANA A
#define ST_MANAAWIDTH           3
#define ST_MANAAX               91
#define ST_MANAAY               19

// MANA A ICON
#define ST_MANAAICONX           77
#define ST_MANAAICONY           2

// MANA A VIAL
#define ST_MANAAVIALX           94
#define ST_MANAAVIALY           2

// MANA B
#define ST_MANABWIDTH           3
#define ST_MANABX               123
#define ST_MANABY               19

// MANA B ICON
#define ST_MANABICONX           110
#define ST_MANABICONY           2

// MANA B VIAL
#define ST_MANABVIALX           102
#define ST_MANABVIALY           2

// ARMOR number pos.
#define ST_ARMORWIDTH           2
#define ST_ARMORX               274
#define ST_ARMORY               14

// Frags pos.
#define ST_FRAGSWIDTH           3
#define ST_FRAGSX               64
#define ST_FRAGSY               14

// TYPES -------------------------------------------------------------------

typedef struct {
    boolean         stopped;
    int             hideTics;
    float           hideAmount;

    float           showBar; // Slide statusbar amount 1.0 is fully open.
    float           alpha; // Fullscreen hud alpha value.

    float           statusbarCounterAlpha;
    boolean         firstTime; // ST_Start() has just been called.
    boolean         statusbarActive; // Whether left-side main status bar is active.

    boolean         hitCenterFrame;
    int             currentInvItemFlash;
    int             armorLevel; // Current armor level.
    int             manaAIcon; // Current mana A icon index. 0 = none.
    int             manaBIcon; // Current mana B icon index. 0 = none.
    int             manaAVial; // Current mana A vial index. 0 = none.
    int             manaBVial; // Current mana B vial index. 0 = none.
    int             manaACount;
    int             manaBCount;
    int             fragsCount; // Number of frags so far in deathmatch.
    boolean         blended; // Whether to use alpha blending.

    int             healthMarker;

    int             oldHealth;

    // Widgets:
    st_multiicon_t  wManaA; // Current mana A icon.
    st_multiicon_t  wManaB; // Current mana B icon.
    st_number_t     wManaACount; // Current mana A count.
    st_number_t     wManaBCount; // Current mana B count.
    st_multiicon_t  wManaAVial; // Current mana A vial.
    st_multiicon_t  wManaBVial; // Current mana B vial.
    st_number_t     wFrags; // In deathmatch only, summary of frags stats.
    st_number_t     wHealth; // Health.
    st_number_t     wArmor; // Armor.
} hudstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void DrINumber(signed int val, int x, int y, float r, float g,
                      float b, float a);
void ST_updateWidgets(int player);
static void updateViewWindow(cvar_t* cvar);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

static dpatch_t dpStatusBar;
static dpatch_t dpStatusBarTop;
static dpatch_t dpKills;
static dpatch_t dpStatBar;
static dpatch_t dpKeyBar;
static dpatch_t dpKeySlot[NUM_KEY_TYPES];
static dpatch_t dpArmorSlot[NUMARMOR];
static dpatch_t dpINumbers[10];
static dpatch_t dpNegative;
static dpatch_t dpManaAVials[2];
static dpatch_t dpManaBVials[2];
static dpatch_t dpManaAIcons[2];
static dpatch_t dpManaBIcons[2];
static dpatch_t dpInventoryBar;
static dpatch_t dpWeaponSlot[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpWeaponFull[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpLifeGem[3][8]; // [Fighter, Cleric, Mage][color]
static dpatch_t dpWeaponPiece1[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpWeaponPiece2[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpWeaponPiece3[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpChain[3]; // [Fighter, Cleric, Mage]
static dpatch_t dpInvItemFlash[5];
static dpatch_t dpSpinFly[16];
static dpatch_t dpSpinMinotaur[16];
static dpatch_t dpSpinSpeed[16];
static dpatch_t dpSpinDefense[16];
static dpatch_t dpTeleIcon;

// CVARs for the HUD/Statusbar
cvar_t sthudCVars[] = {
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 1},
    {"hud-wideoffset", 0, CVT_FLOAT, &cfg.hudWideOffset, 0, 1},

    {"hud-status-size", 0, CVT_FLOAT, &cfg.statusbarScale, 0.1f, 1, updateViewWindow},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1},

    {"hud-status-alpha", 0, CVT_FLOAT, &cfg.statusbarOpacity, 0, 1},
    {"hud-status-icon-a", 0, CVT_FLOAT, &cfg.statusbarCounterAlpha, 0, 1},

    // HUD icons
    {"hud-mana", 0, CVT_BYTE, &cfg.hudShown[HUD_MANA], 0, 2},
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-currentitem", 0, CVT_BYTE, &cfg.hudShown[HUD_CURRENTITEM], 0, 1},

    // HUD displays
    {"hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1},
    {"hud-unhide-pickup-invitem", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_INVITEM], 0, 1},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the HUD/Status bar.
 */
void ST_Register(void)
{
    int                 i;

    for(i = 0; sthudCVars[i].name; ++i)
        Con_AddVariable(sthudCVars + i);

    Hu_InventoryRegister();
}

void drawFlightWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];

    if(!plr->powers[PT_FLIGHT])
        return;

    if(plr->powers[PT_FLIGHT] > BLINKTHRESHOLD || !(plr->powers[PT_FLIGHT] & 16))
    {
        int frame = (mapTime / 3) & 15;
        if(plr->plr->mo->flags2 & MF2_FLY)
        {
            if(hud->hitCenterFrame && (frame != 15 && frame != 0))
                frame = 15;
            else
                hud->hitCenterFrame = false;
        }
        else
        {
            if(!hud->hitCenterFrame && (frame != 15 && frame != 0))
            {
                hud->hitCenterFrame = false;
            }
            else
            {
                frame = 15;
                hud->hitCenterFrame = true;
            }
        }
        GL_DrawPatchLitAlpha(16, 14, 1, iconAlpha, dpSpinFly[frame].lump);
    }
    *drawnWidth = 32;
    *drawnHeight = 28;
}

void drawBootsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    if(!plr->powers[PT_SPEED])
        return;
    if(plr->powers[PT_SPEED] > BLINKTHRESHOLD || !(plr->powers[PT_SPEED] & 16))
        GL_DrawPatchLitAlpha(12, 14, 1, iconAlpha, dpSpinSpeed[(mapTime / 3) & 15].lump);
    *drawnWidth = 24;
    *drawnHeight = 28;
}

void drawDefenseWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    if(!plr->powers[PT_INVULNERABILITY])
        return;
    if(plr->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD || !(plr->powers[PT_INVULNERABILITY] & 16))
        GL_DrawPatchLitAlpha(-13, 14, 1, iconAlpha, dpSpinDefense[(mapTime / 3) & 15].lump);
    *drawnWidth = 26;
    *drawnHeight = 28;
}

void drawServantWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    if(!plr->powers[PT_MINOTAUR])
        return;
    if(plr->powers[PT_MINOTAUR] > BLINKTHRESHOLD || !(plr->powers[PT_MINOTAUR] & 16))
        GL_DrawPatchLitAlpha(-13, 17, 1, iconAlpha, dpSpinMinotaur[(mapTime / 3) & 15].lump);
    *drawnWidth = 26;
    *drawnHeight = 34;
}

void drawWeaponPiecesWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT*hud->showBar)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int pClass = cfg.playerClass[player]; // Original player class (i.e. not pig).

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;

    if(plr->pieces == 7)
    {
        GL_DrawPatchLitAlpha(ORIGINX+190, ORIGINY, 1, hud->statusbarCounterAlpha, dpWeaponFull[pClass].lump);
    }
    else
    {
        if(plr->pieces & WPIECE1)
        {
            GL_DrawPatchLitAlpha(ORIGINX+PCLASS_INFO(pClass)->pieceX[0], ORIGINY, 1, hud->statusbarCounterAlpha, dpWeaponPiece1[pClass].lump);
        }

        if(plr->pieces & WPIECE2)
        {
            GL_DrawPatchLitAlpha(ORIGINX+PCLASS_INFO(pClass)->pieceX[1], ORIGINY, 1, hud->statusbarCounterAlpha, dpWeaponPiece2[pClass].lump);
        }

        if(plr->pieces & WPIECE3)
        {
            GL_DrawPatchLitAlpha(ORIGINX+PCLASS_INFO(pClass)->pieceX[2], ORIGINY, 1, hud->statusbarCounterAlpha, dpWeaponPiece3[pClass].lump);
        }
    }

    *drawnWidth = 57;
    *drawnHeight = 30;

#undef ORIGINX
#undef ORIGINY
}

void drawHealthChainWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (0)

    static int theirColors[] = {
        157, // Blue
        177, // Red
        137, // Yellow
        198, // Green
        215, // Jade
        32,  // White
        106, // Hazel
        234  // Purple
    };

    hudstate_t* hud = &hudStates[player];
    int x, y, w, h;
    int pClass, pColor;
    float healthPos;
    int gemXOffset;
    float gemglow, rgb[3];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive)
        return;

    hud->oldHealth = hud->healthMarker;

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];

    healthPos = MINMAX_OF(0, hud->healthMarker / 100.f, 100);

    if(!IS_NETGAME)
    {
        pColor = 1; // Always use the red life gem (the second gem).
    }
    else
    {
        pColor = cfg.playerColor[player];

        if(pClass == PCLASS_FIGHTER)
        {
            if(pColor == 0)
                pColor = 2;
            else if(pColor == 2)
                pColor = 0;
        }
    }

    gemglow = healthPos;

    // Draw the chain.
    x = ORIGINX+43;
    y = ORIGINY-7;
    w = ST_WIDTH - 43 - 43;
    h = 7;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    DGL_SetPatch(dpChain[pClass].lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

    DGL_Color4f(1, 1, 1, iconAlpha);

    gemXOffset = 7 + ROUND((w - 14) * healthPos) - dpLifeGem[pClass][pColor].width/2;

    if(gemXOffset > 0)
    {   // Left chain section.
        float cw = (float)(dpChain[pClass].width - gemXOffset) / dpChain[pClass].width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x, y);

            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(x + gemXOffset, y);

            DGL_TexCoord2f(0, 1, 1);
            DGL_Vertex2f(x + gemXOffset, y + h);

            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x, y + h);
        DGL_End();
    }

    if(gemXOffset + dpLifeGem[pClass][pColor].width < w)
    {   // Right chain section.
        float cw = (w - (float)gemXOffset - dpLifeGem[pClass][pColor].width) / dpChain[pClass].width;

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x + gemXOffset + dpLifeGem[pClass][pColor].width, y);

            DGL_TexCoord2f(0, cw, 0);
            DGL_Vertex2f(x + w, y);

            DGL_TexCoord2f(0, cw, 1);
            DGL_Vertex2f(x + w, y + h);

            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(x + gemXOffset + dpLifeGem[pClass][pColor].width, y + h);
        DGL_End();
    }

    // Draw the life gem.
    {
    int vX = x + MAX_OF(0, gemXOffset);
    int vWidth;
    float s1 = 0, s2 = 1;

    vWidth = dpLifeGem[pClass][pColor].width;
    if(gemXOffset + dpLifeGem[pClass][pColor].width > w)
    {
        vWidth -= gemXOffset + dpLifeGem[pClass][pColor].width - w;
        s2 = (float)vWidth / dpLifeGem[pClass][pColor].width;
    }
    if(gemXOffset < 0)
    {
        vWidth -= -gemXOffset;
        s1 = (float)(-gemXOffset) / dpLifeGem[pClass][pColor].width;
    }

    DGL_SetPatch(dpLifeGem[pClass][pColor].lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, s1, 0);
        DGL_Vertex2f(vX, y);

        DGL_TexCoord2f(0, s2, 0);
        DGL_Vertex2f(vX + vWidth, y);

        DGL_TexCoord2f(0, s2, 1);
        DGL_Vertex2f(vX + vWidth, y + h);

        DGL_TexCoord2f(0, s1, 1);
        DGL_Vertex2f(vX, y + h);
    DGL_End();
    }

    // How about a glowing gem?
    DGL_BlendMode(BM_ADD);
    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));

    R_GetColorPaletteRGBf(0, rgb, theirColors[pColor], false);
    DGL_DrawRect(x + gemXOffset + 23, y - 6, 41, 24, rgb[0], rgb[1], rgb[2], gemglow - (1 - iconAlpha));

    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = ST_WIDTH - 21 - 28;
    *drawnHeight = 8;

#undef ORIGINX
#undef ORIGINY
}

/**
 * Draws the whole statusbar backgound.
 * \todo There is a whole lot of constants in here. What if someone wants to
 * replace the statusbar with new patches?
 */
void drawStatusBarBackground(int player, float textAlpha, float iconAlpha, int* drawnWidth, int* drawnHeight)
{
#define WIDTH (ST_WIDTH)
#define HEIGHT (ST_HEIGHT)
#define ORIGINX (-WIDTH/2)
#define ORIGINY (-HEIGHT * hud->showBar)

    hudstate_t* hud = &hudStates[player];
    int x, y, w, h, pClass = cfg.playerClass[player]; // Original class (i.e. not pig).
    float cw, ch;

    if(!hud->statusbarActive)
        return;

    if(!(iconAlpha < 1))
    {
        GL_DrawPatch(ORIGINX, ORIGINY-28, dpStatusBar.lump);
        /**
         * \kludge The Hexen statusbar graphic has a chain already in the
         * image, which shows through the modified chain patches.
         * Mask out the chain on the statusbar by drawing a solid black
         * rectangle over it.
         */
        DGL_SetNoMaterial();
        DGL_DrawRect(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, 1);
        //// \kludge end
        GL_DrawPatch(ORIGINX, ORIGINY-28, dpStatusBarTop.lump);

        if(!Hu_InventoryIsOpen(player))
        {
            // Main interface
            if(!AM_IsActive(AM_MapForPlayer(player)))
            {
                GL_DrawPatch(ORIGINX+38, ORIGINY, dpStatBar.lump);

                if(deathmatch)
                {
                    GL_DrawPatch_CS(ORIGINX+38, ORIGINY, dpKills.lump);
                }

                GL_DrawPatch(ORIGINX+190, ORIGINY, dpWeaponSlot[pClass].lump);
            }
            else
            {
                GL_DrawPatch(ORIGINX+38, ORIGINY, dpKeyBar.lump);
            }
        }
        else
        {
            GL_DrawPatch(ORIGINX+38, ORIGINY, dpInventoryBar.lump);
        }
    }
    else
    {
        DGL_Color4f(1, 1, 1, iconAlpha);
        DGL_SetPatch(dpStatusBar.lump, DGL_CLAMP, DGL_CLAMP);

        DGL_Begin(DGL_QUADS);

        // top
        x = ORIGINX;
        y = ORIGINY-27;
        w = ST_WIDTH;
        h = 27;
        ch = 0.41538461538461538461538461538462f;

        DGL_TexCoord2f(0, 0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, 1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, 1, ch);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, ch);
        DGL_Vertex2f(x, y + h);

        // left statue
        x = ORIGINX;
        y = ORIGINY;
        w = 38;
        h = 38;
        cw = (float) 38 / ST_WIDTH;
        ch = 0.41538461538461538461538461538462f;

        DGL_TexCoord2f(0, 0, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, cw, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, cw, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, 1);
        DGL_Vertex2f(x, y + h);

        // right statue
        x = ORIGINX+282;
        y = ORIGINY;
        w = 38;
        h = 38;
        cw = (float) (ST_WIDTH - 38) / ST_WIDTH;
        ch = 0.41538461538461538461538461538462f;

        DGL_TexCoord2f(0, cw, ch);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, 1, ch);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, 1, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, cw, 1);
        DGL_Vertex2f(x, y + h);
        DGL_End();

        /**
         * \kludge The Hexen statusbar graphic has a chain already in the
         * image, which shows through the modified chain patches.
         * Mask out the chain on the statusbar by cutting a window out and
         * drawing a solid near-black rectangle to fill the hole.
         */
        DGL_DrawCutRectTiled(ORIGINX+38, ORIGINY+31, 244, 8, 320, 65, 38, 192-134, ORIGINX+44, ORIGINY+31, 232, 7);
        DGL_SetNoMaterial();
        DGL_DrawRect(ORIGINX+44, ORIGINY+31, 232, 7, .1f, .1f, .1f, iconAlpha);
        DGL_Color4f(1, 1, 1, iconAlpha);
        //// \kludge end

        if(!Hu_InventoryIsOpen(player))
        {
            // Main interface
            if(!AM_IsActive(AM_MapForPlayer(player)))
            {
                x = ORIGINX + (deathmatch ? 68 : 38);
                y = ORIGINY;
                w = deathmatch?214:244;
                h = 31;
                DGL_SetPatch(dpStatBar.lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
                DGL_DrawCutRectTiled(x, y, w, h, dpStatBar.width, dpStatBar.height, deathmatch?30:0, 0, ORIGINX+190, ORIGINY, 57, 30);

                GL_DrawPatch_CS(ORIGINX+190, ORIGINY, dpWeaponSlot[pClass].lump);
                if(deathmatch)
                    GL_DrawPatch_CS(ORIGINX+38, ORIGINY, dpKills.lump);
            }
            else
            {
                GL_DrawPatch_CS(ORIGINX+38, ORIGINY, dpKeyBar.lump);
            }
        }
        else
        {
            // INVBAR
            DGL_SetPatch(dpInventoryBar.lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
            DGL_Begin(DGL_QUADS);

            x = ORIGINX+38;
            y = ORIGINY;
            w = 244;
            h = 30;
            ch = 0.96774193548387096774193548387097f;

            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(x, y);
            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(x + w, y);
            DGL_TexCoord2f(0, 1, ch);
            DGL_Vertex2f(x + w, y + h);
            DGL_TexCoord2f(0, 0, ch);
            DGL_Vertex2f(x, y + h);

            DGL_End();
        }
    }

    *drawnWidth = WIDTH;
    *drawnHeight = HEIGHT;

#undef WIDTH
#undef HEIGHT
#undef ORIGINX
#undef ORIGINY
}

void ST_loadGraphics(void)
{
    int                 i;
    char                namebuf[9];


    R_CachePatch(&dpStatusBar, "H2BAR");
    R_CachePatch(&dpStatusBarTop, "H2TOP");
    R_CachePatch(&dpInventoryBar, "INVBAR");
    R_CachePatch(&dpStatBar, "STATBAR");
    R_CachePatch(&dpKeyBar, "KEYBAR");

    R_CachePatch(&dpManaAVials[0], "MANAVL1D");
    R_CachePatch(&dpManaBVials[0], "MANAVL2D");
    R_CachePatch(&dpManaAVials[1], "MANAVL1");
    R_CachePatch(&dpManaBVials[1], "MANAVL2");

    R_CachePatch(&dpManaAIcons[0], "MANADIM1");
    R_CachePatch(&dpManaBIcons[0], "MANADIM2");
    R_CachePatch(&dpManaAIcons[1], "MANABRT1");
    R_CachePatch(&dpManaBIcons[1], "MANABRT2");

    R_CachePatch(&dpNegative, "NEGNUM");
    R_CachePatch(&dpKills, "KILLS");

    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        sprintf(namebuf, "KEYSLOT%X", i + 1);
        R_CachePatch(&dpKeySlot[i], namebuf);
    }

    for(i = 0; i < NUMARMOR; ++i)
    {
        sprintf(namebuf, "ARMSLOT%d", i + 1);
        R_CachePatch(&dpArmorSlot[i], namebuf);
    }

    for(i = 0; i < 16; ++i)
    {
        sprintf(namebuf, "SPFLY%d", i);
        R_CachePatch(&dpSpinFly[i], namebuf);

        sprintf(namebuf, "SPMINO%d", i);
        R_CachePatch(&dpSpinMinotaur[i], namebuf);

        sprintf(namebuf, "SPBOOT%d", i);
        R_CachePatch(&dpSpinSpeed[i], namebuf);

        sprintf(namebuf, "SPSHLD%d", i);
        R_CachePatch(&dpSpinDefense[i], namebuf);
    }

    // Fighter:
    R_CachePatch(&dpWeaponPiece1[PCLASS_FIGHTER], "WPIECEF1");
    R_CachePatch(&dpWeaponPiece2[PCLASS_FIGHTER], "WPIECEF2");
    R_CachePatch(&dpWeaponPiece3[PCLASS_FIGHTER], "WPIECEF3");
    R_CachePatch(&dpChain[PCLASS_FIGHTER], "CHAIN");
    R_CachePatch(&dpWeaponSlot[PCLASS_FIGHTER], "WPSLOT0");
    R_CachePatch(&dpWeaponFull[PCLASS_FIGHTER], "WPFULL0");
    R_CachePatch(&dpLifeGem[PCLASS_FIGHTER][0], "LIFEGEM");
    for(i = 1; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMF%d", i + 1);
        R_CachePatch(&dpLifeGem[PCLASS_FIGHTER][i], namebuf);
    }

    // Cleric:
    R_CachePatch(&dpWeaponPiece1[PCLASS_CLERIC], "WPIECEC1");
    R_CachePatch(&dpWeaponPiece2[PCLASS_CLERIC], "WPIECEC2");
    R_CachePatch(&dpWeaponPiece3[PCLASS_CLERIC], "WPIECEC3");
    R_CachePatch(&dpChain[PCLASS_CLERIC], "CHAIN2");
    R_CachePatch(&dpWeaponSlot[PCLASS_CLERIC], "WPSLOT1");
    R_CachePatch(&dpWeaponFull[PCLASS_CLERIC], "WPFULL1");
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMC%d", i + 1);
        R_CachePatch(&dpLifeGem[PCLASS_CLERIC][i], namebuf);
    }

    // Mage:
    R_CachePatch(&dpWeaponPiece1[PCLASS_MAGE], "WPIECEM1");
    R_CachePatch(&dpWeaponPiece2[PCLASS_MAGE], "WPIECEM2");
    R_CachePatch(&dpWeaponPiece3[PCLASS_MAGE], "WPIECEM3");
    R_CachePatch(&dpChain[PCLASS_MAGE], "CHAIN3");
    R_CachePatch(&dpWeaponSlot[PCLASS_MAGE], "WPSLOT2");
    R_CachePatch(&dpWeaponFull[PCLASS_MAGE], "WPFULL2");
    for(i = 0; i < 8; ++i)
    {
        sprintf(namebuf, "LIFEGMM%d", i + 1);
        R_CachePatch(&dpLifeGem[PCLASS_MAGE][i], namebuf);
    }

    for(i = 0; i < 10; ++i)
    {
        sprintf(namebuf, "IN%d", i);
        R_CachePatch(&dpINumbers[i], namebuf);
    }

    // Inventory item flash anim.
    {
    const char          invItemFlashAnim[5][9] = {
        {"USEARTIA"},
        {"USEARTIB"},
        {"USEARTIC"},
        {"USEARTID"},
        {"USEARTIE"}
    };

    for(i = 0; i < 5; ++i)
    {
        R_CachePatch(&dpInvItemFlash[i], invItemFlashAnim[i]);
    }
    }

    R_CachePatch(&dpTeleIcon, "TELEICON");
}

void ST_loadData(void)
{
    ST_loadGraphics();
}

static void initData(hudstate_t* hud)
{
    int player = hud - hudStates;
    player_t* plr = &players[player];

    hud->firstTime = true;
    hud->statusbarActive = true;
    hud->stopped = true;
    hud->oldHealth = -1;
    // Health marker chain animates up to the actual health value.
    hud->healthMarker = 0;
    hud->blended = false;
    hud->showBar = 0.0f;

    ST_updateWidgets(player);

    ST_HUDUnHide(player, HUE_FORCE);
}

void ST_createWidgets(int player)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];

    // Health num.
    STlib_InitNum(&hud->wHealth, ORIGINX+ST_HEALTHX, ORIGINY+ST_HEALTHY, dpINumbers, &plr->health, ST_HEALTHWIDTH, 1);

    // Frags sum.
    STlib_InitNum(&hud->wFrags, ORIGINX+ST_FRAGSX, ORIGINY+ST_FRAGSY, dpINumbers, &hud->fragsCount, ST_FRAGSWIDTH, 1);

    // Armor num - should be colored later.
    STlib_InitNum(&hud->wArmor, ORIGINX+ST_ARMORX, ORIGINY+ST_ARMORY, dpINumbers, &hud->armorLevel, ST_ARMORWIDTH, 1);

    // ManaA count.
    STlib_InitNum(&hud->wManaACount, ORIGINX+ST_MANAAX, ORIGINY+ST_MANAAY, dpSmallNumbers, &hud->manaACount, ST_MANAAWIDTH, 1);

    // ManaB count.
    STlib_InitNum(&hud->wManaBCount, ORIGINX+ST_MANABX, ORIGINY+ST_MANABY, dpSmallNumbers, &hud->manaBCount, ST_MANABWIDTH, 1);

    // Current mana A icon.
    STlib_InitMultiIcon(&hud->wManaA, ORIGINX+ST_MANAAICONX, ORIGINY+ST_MANAAICONY, dpManaAIcons, 1);

    // Current mana B icon.
    STlib_InitMultiIcon(&hud->wManaB, ORIGINX+ST_MANABICONX, ORIGINY+ST_MANABICONY, dpManaBIcons, 1);

    // Current mana A vial.
    STlib_InitMultiIcon(&hud->wManaAVial, ORIGINX+ST_MANAAVIALX, ORIGINY+ST_MANAAVIALY, dpManaAVials, 1);

    // Current mana B vial.
    STlib_InitMultiIcon(&hud->wManaBVial, ORIGINX+ST_MANABVIALX, ORIGINY+ST_MANABVIALY, dpManaBVials, 1);

#undef ORIGINX
#undef ORIGINY
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

void ST_FlashCurrentItem(int player)
{
    player_t*           plr;
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    hud->currentInvItemFlash = 4;
}

void ST_updateWidgets(int player)
{
    int                 i, pClass;
    hudstate_t*         hud = &hudStates[player];
    player_t*           plr = &players[player];

    // Original player class (i.e. not pig).
    pClass = cfg.playerClass[player];

    if(hud->blended)
    {
        hud->statusbarCounterAlpha =
            MINMAX_OF(0.f, cfg.statusbarCounterAlpha - hud->hideAmount, 1.f);
    }
    else
        hud->statusbarCounterAlpha = 1.0f;

    // Used by w_frags widget.
    hud->fragsCount = 0;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        hud->fragsCount += plr->frags[i] * (i != player ? 1 : -1);
    }

    // Armor
    hud->armorLevel = FixedDiv(
        PCLASS_INFO(pClass)->autoArmorSave + plr->armorPoints[ARMOR_ARMOR] +
        plr->armorPoints[ARMOR_SHIELD] +
        plr->armorPoints[ARMOR_HELMET] +
        plr->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;

    // mana A
    hud->manaACount = plr->ammo[AT_BLUEMANA].owned;

    // mana B
    hud->manaBCount = plr->ammo[AT_GREENMANA].owned;

    hud->manaAIcon = hud->manaBIcon = hud->manaAVial = hud->manaBVial = -1;

    // Mana
    if(!(plr->ammo[AT_BLUEMANA].owned > 0))
        hud->manaAIcon = 0; // Draw dim Mana icon.

    if(!(plr->ammo[AT_GREENMANA].owned > 0))
        hud->manaBIcon = 0; // Draw dim Mana icon.

    // Update mana graphics based upon mana count weapon type
    if(plr->readyWeapon == WT_FIRST)
    {
        hud->manaAIcon = 0;
        hud->manaBIcon = 0;

        hud->manaAVial = 0;
        hud->manaBVial = 0;
    }
    else if(plr->readyWeapon == WT_SECOND)
    {
        // If there is mana for this weapon, make it bright!
        if(hud->manaAIcon == -1)
        {
            hud->manaAIcon = 1;
        }

        hud->manaAVial = 1;

        hud->manaBIcon = 0;
        hud->manaBVial = 0;
    }
    else if(plr->readyWeapon == WT_THIRD)
    {
        hud->manaAIcon = 0;
        hud->manaAVial = 0;

        // If there is mana for this weapon, make it bright!
        if(hud->manaBIcon == -1)
        {
            hud->manaBIcon = 1;
        }

        hud->manaBVial = 1;
    }
    else
    {
        hud->manaAVial = 1;
        hud->manaBVial = 1;

        // If there is mana for this weapon, make it bright!
        if(hud->manaAIcon == -1)
        {
            hud->manaAIcon = 1;
        }
        if(hud->manaBIcon == -1)
        {
            hud->manaBIcon = 1;
        }
    }
}

void ST_Ticker(void)
{
    int                 i;

    Hu_InventoryTicker();

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t*           plr = &players[i];
        hudstate_t*         hud = &hudStates[i];

        if(!(plr->plr->inGame && (plr->plr->flags & DDPF_LOCAL)))
            continue;

        ST_updateWidgets(i);

        if(!P_IsPaused())
        {
            int                 delta;
            int                 curHealth;

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

            if(hud->currentInvItemFlash > 0)
                hud->currentInvItemFlash--;

            curHealth = MAX_OF(plr->plr->mo->health, 0);
            if(curHealth < hud->healthMarker)
            {
                delta = MINMAX_OF(1, (hud->healthMarker - curHealth) >> 2, 6);
                hud->healthMarker -= delta;
            }
            else if(curHealth > hud->healthMarker)
            {
                delta = MINMAX_OF(1, (curHealth - hud->healthMarker) >> 2, 6);
                hud->healthMarker += delta;
            }
        }
    }
}

/**
 * Sets the new palette based upon the current values of
 * player_t->damageCount and player_t->bonusCount.
 */
void ST_doPaletteStuff(int player)
{
    int                 palette = 0;
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];

    if(G_GetGameState() == GS_MAP)
    {
        plr = &players[CONSOLEPLAYER];
        if(plr->poisonCount)
        {
            palette = 0;
            palette = (plr->poisonCount + 7) >> 3;
            if(palette >= NUMPOISONPALS)
            {
                palette = NUMPOISONPALS - 1;
            }
            palette += STARTPOISONPALS;
        }
        else if(plr->damageCount)
        {
            palette = (plr->damageCount + 7) >> 3;
            if(palette >= NUMREDPALS)
            {
                palette = NUMREDPALS - 1;
            }
            palette += STARTREDPALS;
        }
        else if(plr->bonusCount)
        {
            palette = (plr->bonusCount + 7) >> 3;
            if(palette >= NUMBONUSPALS)
            {
                palette = NUMBONUSPALS - 1;
            }
            palette += STARTBONUSPALS;
        }
        else if(plr->plr->mo->flags2 & MF2_ICEDAMAGE)
        {   // Frozen player
            palette = STARTICEPAL;
        }
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

void drawSBarInventoryWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    if(!hud->statusbarActive || !Hu_InventoryIsOpen(player))
        return;

    Hu_InventoryDraw2(player, -ST_WIDTH/2 + ST_INVENTORYX, -ST_HEIGHT + yOffset + ST_INVENTORYY, iconAlpha);

    // \fixme calculate dimensions properly!
    *drawnWidth = ST_WIDTH-(43*2);
    *drawnHeight = 41;
}

void drawKeysWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT*hud->showBar)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int i, numDrawn, pClass = cfg.playerClass[player]; // Original player class (i.e. not pig).

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || !AM_IsActive(AM_MapForPlayer(player)))
        return;

    *drawnWidth = 0;
    *drawnHeight = 0;

    numDrawn = 0;
    for(i = 0; i < NUM_KEY_TYPES; ++i)
    {
        const dpatch_t* patch;

        if(!(plr->keys & (1 << i)))
            continue;

        patch = &dpKeySlot[i];
        GL_DrawPatchLitAlpha(ORIGINX + 46 + numDrawn * 20, ORIGINY + 1, 1, iconAlpha, patch->lump);

        *drawnWidth += patch->width;
        if(patch->height > *drawnHeight)
            *drawnHeight = patch->height;

        numDrawn++;
    }
    if(numDrawn)
        *drawnWidth += (numDrawn-1)*20;

#undef ORIGINX
#undef ORIGINY
}

void drawSBarArmorIconsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT*hud->showBar)

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int i, numDrawn, pClass = cfg.playerClass[player]; // Original player class (i.e. not pig).

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || !AM_IsActive(AM_MapForPlayer(player)))
        return;

    *drawnWidth = 0;
    *drawnHeight = 0;

    numDrawn = 0;
    for(i = 0; i < NUMARMOR; ++i)
    {
        const dpatch_t* patch;
        float alpha;

        if(!plr->armorPoints[i])
            continue;

        patch = &dpArmorSlot[i];
        if(plr->armorPoints[i] <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 2))
            alpha = .3f;
        else if(plr->armorPoints[i] <= (PCLASS_INFO(pClass)->armorIncrement[i] >> 1))
            alpha = .6f;
        else
            alpha = 1;

        GL_DrawPatchLitAlpha(ORIGINX + 150 + 31 * i, ORIGINY + 2, 1, iconAlpha * alpha, patch->lump);

        *drawnWidth += patch->width;
        if(patch->height > *drawnHeight)
            *drawnHeight = patch->height;

        numDrawn++;
    }
    if(numDrawn)
        *drawnWidth += (numDrawn-1)*31;

#undef ORIGINX
#undef ORIGINY
}

void drawSBarFragsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive || !deathmatch || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    STlib_DrawNum(&hud->wFrags, textAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    /// \fixme calculate dimensions properly!
    *drawnWidth = dpINumbers[0].width*3;
    *drawnHeight = dpINumbers[0].height;
}

void drawSBarHealthWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive || deathmatch || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    STlib_DrawNum(&hud->wHealth, textAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    /// \fixme calculate dimensions properly!
    *drawnWidth = dpINumbers[0].width*3;
    *drawnHeight = dpINumbers[0].height;
}

void drawSBarArmorWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    STlib_DrawNum(&hud->wArmor, textAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    /// \fixme calculate dimensions properly!
    *drawnWidth = dpINumbers[0].width*2;
    *drawnHeight = dpINumbers[0].height;
}

void drawSBarBlueManaWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive || hud->manaACount <= 0 || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    STlib_DrawNum(&hud->wManaACount, textAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    /// \fixme calculate dimensions properly!
    *drawnWidth = dpSmallNumbers[0].width*3;
    *drawnHeight = dpSmallNumbers[0].height;
}

void drawSBarGreenManaWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive || hud->manaBCount <= 0 || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    STlib_DrawNum(&hud->wManaBCount, textAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    /// \fixme calculate dimensions properly!
    *drawnWidth = dpSmallNumbers[0].width*3;
    *drawnHeight = dpSmallNumbers[0].height;
}

void drawSBarCurrentItemWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    hudstate_t* hud = &hudStates[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);
    inventoryitemtype_t readyItem;
    lumpnum_t patch;
    int x, y;

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;

    if((readyItem = P_InventoryReadyItem(player)) == IIT_NONE)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    if(hud->currentInvItemFlash > 0)
    {
        patch = dpInvItemFlash[hud->currentInvItemFlash % 5].lump;
        x = ST_INVITEMX + 4;
        y = ST_INVITEMY;
    }
    else
    {
        patch = P_GetInvItem(readyItem-1)->patchLump;
        x = ST_INVITEMX;
        y = ST_INVITEMY;
    }

    DGL_Color4f(1, 1, 1, iconAlpha);
    GL_DrawPatch_CS(ORIGINX+x, ORIGINY+y, patch);

    if(!(hud->currentInvItemFlash > 0))
    {
        uint count = P_InventoryCount(player, readyItem);
        if(count > 1)
            Hu_DrawSmallNum(count, ST_INVITEMCWIDTH, ORIGINX+ST_INVITEMCX, ORIGINY+ST_INVITEMCY, textAlpha);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = dpInvItemBox.width;
    *drawnHeight = dpInvItemBox.height;

#undef ORIGINX
#undef ORIGINY
}

void drawBlueManaIconWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    STlib_DrawMultiIcon(&hud->wManaA, hud->manaAIcon, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = dpManaAIcons[hud->manaAIcon%2].width;
    *drawnHeight = dpManaAIcons[hud->manaAIcon%2].height;
}

void drawGreenManaIconWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    float yOffset = ST_HEIGHT*(1-hud->showBar);

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, yOffset, 0);

    STlib_DrawMultiIcon(&hud->wManaB, hud->manaBIcon, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -yOffset, 0);

    *drawnWidth = dpManaBIcons[hud->manaBIcon%2].width;
    *drawnHeight = dpManaBIcons[hud->manaBIcon%2].height;
}

void drawBlueManaVialWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (ST_HEIGHT*(1-hud->showBar))

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, ORIGINY, 0);

    STlib_DrawMultiIcon(&hud->wManaAVial, hud->manaAVial, iconAlpha);
    DGL_SetNoMaterial();
    DGL_DrawRect(ORIGINX+95, -ST_HEIGHT+3, 3, 22 - (22 * plr->ammo[AT_BLUEMANA].owned) / MAX_MANA, 0, 0, 0, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -ORIGINY, 0);

    *drawnWidth = dpManaAVials[hud->manaAVial%2].width;
    *drawnHeight = dpManaAVials[hud->manaAVial%2].height;

#undef ORIGINX
#undef ORIGINY
}

void drawGreenManaVialWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (ST_HEIGHT*(1-hud->showBar))

    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];

    if(!hud->statusbarActive || Hu_InventoryIsOpen(player) || AM_IsActive(AM_MapForPlayer(player)))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, ORIGINY, 0);

    STlib_DrawMultiIcon(&hud->wManaBVial, hud->manaBVial, iconAlpha);
    DGL_SetNoMaterial();
    DGL_DrawRect(ORIGINX+103, -ST_HEIGHT+3, 3, 22 - (22 * plr->ammo[AT_GREENMANA].owned) / MAX_MANA, 0, 0, 0, iconAlpha);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(0, -ORIGINY, 0);

    *drawnWidth = dpManaBVials[hud->manaBVial%2].width;
    *drawnHeight = dpManaBVials[hud->manaBVial%2].height;

#undef ORIGINX
#undef ORIGINY
}

/**
 * Draws a three digit number.
 */
static void DrINumber(signed int val, int x, int y, float r, float g,
                      float b, float a)
{
    int         oldval;

    DGL_Color4f(r,g,b,a);

    // Make sure it's a three digit number.
    if(val < -999)
        val = -999;
    if(val > 999)
        val = 999;

    oldval = val;
    if(val < 0)
    {
        val = -val;
        if(val > 99)
        {
            val = 99;
        }
        if(val > 9)
        {
            GL_DrawPatch_CS(x + 8, y, dpINumbers[val / 10].lump);
            GL_DrawPatch_CS(x, y, dpNegative.lump);
        }
        else
        {
            GL_DrawPatch_CS(x + 8, y, dpNegative.lump);
        }
        val = val % 10;
        GL_DrawPatch_CS(x + 16, y, dpINumbers[val].lump);
        return;
    }
    if(val > 99)
    {
        GL_DrawPatch_CS(x, y, dpINumbers[val / 100].lump);
    }
    val = val % 100;
    if(val > 9 || oldval > 99)
    {
        GL_DrawPatch_CS(x + 8, y, dpINumbers[val / 10].lump);
    }
    val = val % 10;
    GL_DrawPatch_CS(x + 16, y, dpINumbers[val].lump);

}

#if 0 // Unused atm
/**
 * Displays sound debugging information.
 */
static void DrawSoundInfo(void)
{
    int         i;
    SoundInfo_t s;
    ChanInfo_t *c;
    char        text[32];
    int         x;
    int         y;
    int         xPos[7] = { 1, 75, 112, 156, 200, 230, 260 };

    if(mapTime & 16)
    {
        MN_DrTextA("*** SOUND DEBUG INFO ***", xPos[0], 20);
    }
    S_GetChannelInfo(&s);
    if(s.channelCount == 0)
    {
        return;
    }
    x = 0;
    MN_DrTextA("NAME", xPos[x++], 30);
    MN_DrTextA("MO.T", xPos[x++], 30);
    MN_DrTextA("MO.X", xPos[x++], 30);
    MN_DrTextA("MO.Y", xPos[x++], 30);
    MN_DrTextA("ID", xPos[x++], 30);
    MN_DrTextA("PRI", xPos[x++], 30);
    MN_DrTextA("DIST", xPos[x++], 30);
    for(i = 0; i < s.channelCount; ++i)
    {
        c = &s.chan[i];
        x = 0;
        y = 40 + i * 10;
        if(c->mo == NULL)
        {                       // Channel is unused
            MN_DrTextA("------", xPos[0], y);
            continue;
        }
        sprintf(text, "%s", c->name);
        strupr(text);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->mo->type);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->mo->x >> FRACBITS);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->mo->y >> FRACBITS);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->id);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", S_sfx[c->id].usefulness);
        MN_DrTextA(text, xPos[x++], y);
        sprintf(text, "%d", c->distance);
    }
}
#endif

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

static boolean pickStatusbarScalingStrategy(int viewportWidth, int viewportHeight)
{
    float a = (float)viewportWidth/viewportHeight;
    float b = (float)SCREENWIDTH/SCREENHEIGHT;

    if(INRANGE_OF(a, b, .001f))
        return true; // The same, so stretch.
    if(Con_GetByte("rend-hud-nostretch") || !INRANGE_OF(a, b, .18f))
        return false; // No stretch; translate and scale to fit.
    // Otherwise stretch.
    return true;
}

void drawHealthWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];
    player_t* plr = &players[player];
    int w, h, health = MAX_OF(plr->plr->mo->health, 0);
    char buf[20];
    if(hud->statusbarActive)
        return;
    dd_snprintf(buf, 20, "%i", health);
    w = M_StringWidth(buf, GF_FONTB);
    h = M_StringHeight(buf, GF_FONTB);
    M_WriteText2(-1, -(h+1), buf, GF_FONTB, cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textAlpha);
    *drawnWidth = w;
    *drawnHeight = h;
}

void drawBlueManaWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    const dpatch_t* dim =  &dpManaAIcons[0];
    const dpatch_t* bright = &dpManaAIcons[1];
    const dpatch_t* patch = NULL;

    if(hud->statusbarActive)
        return;

    if(!(plr->ammo[AT_BLUEMANA].owned > 0))
        patch = dim;

    switch(plr->readyWeapon)
    {
    case WT_FIRST:
        patch = dim;
        break;
    case WT_SECOND:
        if(!patch)
            patch = bright;
        break;
    case WT_THIRD:
        patch = dim;
        break;
    case WT_FOURTH:
        if(!patch)
            patch = bright;
        break;
    default:
        break;
    }

    GL_DrawPatchLitAlpha(0, 0, 1, iconAlpha, patch->lump);
    DrINumber(plr->ammo[AT_BLUEMANA].owned, patch->width+2, 0, 1, 1, 1, textAlpha);
    *drawnWidth = patch->width+2+dpINumbers[0].width*3;
    *drawnHeight = MAX_OF(patch->height, dpINumbers[0].height);
}

void drawGreenManaWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    const dpatch_t* dim = &dpManaBIcons[0];
    const dpatch_t* bright = &dpManaBIcons[1];
    const dpatch_t* patch = NULL;

    if(hud->statusbarActive)
        return;

    if(!(plr->ammo[AT_GREENMANA].owned > 0))
        patch = dim;

    switch(plr->readyWeapon)
    {
    case WT_FIRST:
        patch = dim;
        break;
    case WT_SECOND:
        patch = dim;
        break;
    case WT_THIRD:
        if(!patch)
            patch = bright;
        break;
    case WT_FOURTH:
        if(!patch)
            patch = bright;
        break;
    default:
        break;
    }

    GL_DrawPatchLitAlpha(0, 0, 1, iconAlpha, patch->lump);
    DrINumber(plr->ammo[AT_GREENMANA].owned, patch->width+2, 0, 1, 1, 1, textAlpha);
    *drawnWidth = patch->width+2+dpINumbers[0].width*3;
    *drawnHeight = MAX_OF(patch->height, dpINumbers[0].height);
}

void drawFragsWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    player_t* plr = &players[player];
    hudstate_t* hud = &hudStates[player];
    int i, numFrags = 0;
    if(hud->statusbarActive || !deathmatch)
        return;
    for(i = 0; i < MAXPLAYERS; ++i)
        if(plr->plr->inGame)
            numFrags += plr->frags[i];
    DrINumber(numFrags, 0, -13, 1, 1, 1, textAlpha);
    /// \kludge calculate the visual dimensions properly!
    *drawnWidth = (dpINumbers[0].width+1) * 3;
    *drawnHeight = dpINumbers[0].height;
}

void drawCurrentItemWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    hudstate_t* hud = &hudStates[player];

    if(hud->statusbarActive || Hu_InventoryIsOpen(player))
        return;

    if(hud->currentInvItemFlash > 0)
    {
        const dpatch_t* dp = &dpInvItemFlash[hud->currentInvItemFlash % 5];

        GL_DrawPatchLitAlpha(-30, -30, 1, iconAlpha / 2, dpInvItemBox.lump);
        GL_DrawPatchLitAlpha(-27, -30, 1, iconAlpha, dp->lump);
    }
    else
    {
        inventoryitemtype_t readyItem = P_InventoryReadyItem(player);

        if(readyItem != IIT_NONE)
        {
            lumpnum_t patch = P_GetInvItem(readyItem-1)->patchLump;
            uint count;

            GL_DrawPatchLitAlpha(-30, -30, 1, iconAlpha / 2, dpInvItemBox.lump);
            GL_DrawPatchLitAlpha(-32, -31, 1, iconAlpha, patch);
            if((count = P_InventoryCount(player, readyItem)) > 1)
                Hu_DrawSmallNum(count, ST_INVITEMCWIDTH, -2, -7, textAlpha);
        }
    }
    *drawnWidth = dpInvItemBox.width;
    *drawnHeight = dpInvItemBox.height;
}

void drawInventoryWidget(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
#define INVENTORY_HEIGHT    29

    hudstate_t* hud = &hudStates[player];
    if(hud->statusbarActive || !Hu_InventoryIsOpen(player))
        return;
    Hu_InventoryDraw(player, 0, -INVENTORY_HEIGHT, textAlpha, iconAlpha);
    *drawnWidth = 31 * 7 + 16 * 2;
    *drawnHeight = INVENTORY_HEIGHT;

#undef INVENTORY_HEIGHT
}

uiwidget_t widgetsStatusBar[] = {
    { -1, &cfg.statusbarScale, 1, drawStatusBarBackground, &cfg.statusbarOpacity, &cfg.statusbarOpacity },
    { -1, &cfg.statusbarScale, 1, drawWeaponPiecesWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawHealthChainWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawSBarInventoryWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawKeysWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawSBarArmorIconsWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawSBarFragsWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawSBarHealthWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawSBarArmorWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawSBarCurrentItemWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawBlueManaIconWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawSBarBlueManaWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawBlueManaVialWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawGreenManaIconWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawSBarGreenManaWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha },
    { -1, &cfg.statusbarScale, 1, drawGreenManaVialWidget, &cfg.statusbarCounterAlpha, &cfg.statusbarCounterAlpha }
};

uiwidget_t widgetsTopLeft[] = {
    { HUD_MANA, &cfg.hudScale, 1, drawBlueManaWidget },
    { HUD_MANA, &cfg.hudScale, 1, drawGreenManaWidget }
};

uiwidget_t widgetsTopLeft2[] = {
    { -1, &cfg.hudScale, 1, drawFlightWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
    { -1, &cfg.hudScale, 1, drawBootsWidget, &cfg.hudColor[3], &cfg.hudIconAlpha }
};

uiwidget_t widgetsTopRight[] = {
    { -1, &cfg.hudScale, 1, drawServantWidget, &cfg.hudColor[3], &cfg.hudIconAlpha },
    { -1, &cfg.hudScale, 1, drawDefenseWidget, &cfg.hudColor[3], &cfg.hudIconAlpha }
};

uiwidget_t widgetsBottomLeft[] = {
    { HUD_HEALTH, &cfg.hudScale, 1, drawHealthWidget },
    { -1, &cfg.hudScale, 1, drawFragsWidget }
};

uiwidget_t widgetsBottomRight[] = {
    { HUD_CURRENTITEM, &cfg.hudScale, 1, drawCurrentItemWidget }
};

uiwidget_t widgetsBottom[] = {
    { -1, &cfg.hudScale, .75f, drawInventoryWidget }
};

static void drawStatusbar(int player, int x, int y, int viewW, int viewH)
{
    hudstate_t* hud = &hudStates[player];
    int needWidth;
    float scaleX, scaleY;

    if(!hud->statusbarActive)
        return;

    needWidth = ((viewW >= viewH)? (float)viewH/SCREENHEIGHT : (float)viewW/SCREENWIDTH) * ST_WIDTH;
    scaleX = scaleY = cfg.statusbarScale;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);

    if(pickStatusbarScalingStrategy(viewW, viewH))
    {
        scaleX *= (float)viewW/needWidth;
    }
    else
    {
        if(needWidth > viewW)
        {
            scaleX *= (float)viewW/needWidth;
            scaleY *= (float)viewW/needWidth;
        }
    }

    DGL_Scalef(scaleX, scaleY, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ST_Drawer(int player, int fullscreenmode, boolean refresh)
{
    hudstate_t* hud;
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];

    hud->firstTime = hud->firstTime || refresh;
    hud->statusbarActive = (fullscreenmode < 2) ||
        (AM_IsActive(AM_MapForPlayer(player)) &&
         (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts
    ST_doPaletteStuff(player);

    // Either slide the status bar in or fade out the fullscreen hud
    if(hud->statusbarActive)
    {
        if(hud->alpha > 0.0f)
        {
            hud->statusbarActive = 0;
            hud->alpha-=0.1f;
        }
        else if(hud->showBar < 1.0f)
            hud->showBar += 0.1f;
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
            if(hud->showBar > 0.0f)
            {
                hud->showBar -= 0.1f;
                hud->statusbarActive = 1;
            }
            else if(hud->alpha < 1.0f)
                hud->alpha += 0.1f;
        }
    }
    hud->oldHealth = -1;

    // Always try to render statusbar with alpha in fullscreen modes
    if(fullscreenmode)
        hud->blended = 1;
    else
        hud->blended = 0;

    if(hud->statusbarActive || fullscreenmode != 3)
    {
        int viewW, viewH, x, y, width, height;
        float alpha = (hud->statusbarActive? (hud->blended? (1-hud->hideAmount) : 1.0f) : hud->alpha * (1-hud->hideAmount));
        float scale;

        R_GetViewPort(player, NULL, NULL, &viewW, &viewH);

        if(viewW >= viewH)
            scale = (float)viewH/SCREENHEIGHT;
        else
            scale = (float)viewW/SCREENWIDTH;

        x = y = 0;
        width = viewW / scale;
        height = viewH / scale;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();

        DGL_Scalef(scale, scale, 1);

        /**
         * Draw widgets.
         */
        {
#define PADDING 2 // In fixed 320x200 units.

        int posX, posY, drawnWidth, drawnHeight;

        posX = x + width/2;
        posY = y + height;
        UI_DrawWidgets(widgetsStatusBar, sizeof(widgetsStatusBar)/sizeof(widgetsStatusBar[0]),
            (!hud->blended? UWF_OVERRIDE_ALPHA : 0), 0, posX, posY, player, alpha, &drawnWidth, &drawnHeight);

        /**
         * Wide offset scaling.
         * Used with ultra-wide/tall resolutions to move the uiwidgets into
         * the viewer's primary field of vision (without this, uiwidgets
         * would be positioned at the very edges of the view window and
         * likely into the viewer's peripheral vision range).
         *
         * \note Statusbar is exempt because it is intended to extend over
         * the entire width of the view window and as such, uses another
         * special-case scale-positioning calculation.
         */
        if(cfg.hudWideOffset != 1)
        {
            if(viewW > viewH)
            {
                x = (viewW/2/scale - SCREENWIDTH/2) * (1-cfg.hudWideOffset);
                width -= x*2;
            }
            else
            {
                y = (viewH/2/scale - SCREENHEIGHT/2) * (1-cfg.hudWideOffset);
                height -= y*2;
            }
        }

        posX = x + PADDING;
        posY = y + PADDING;
        UI_DrawWidgets(widgetsTopLeft, sizeof(widgetsTopLeft)/sizeof(widgetsTopLeft[0]),
            UWF_TOP2BOTTOM, PADDING, posX, posY, player, alpha, &drawnWidth, &drawnHeight);

        posX = x + PADDING + (drawnWidth > 0 ? drawnWidth + PADDING : 0);
        posY = y + PADDING;
        UI_DrawWidgets(widgetsTopLeft2, sizeof(widgetsTopLeft2)/sizeof(widgetsTopLeft2[0]),
            UWF_LEFT2RIGHT, PADDING, posX, posY, player, alpha, &drawnWidth, &drawnHeight);

        posX = x + width - PADDING;
        posY = y + PADDING;
        UI_DrawWidgets(widgetsTopRight, sizeof(widgetsTopRight)/sizeof(widgetsTopRight[0]),
            UWF_RIGHT2LEFT, PADDING, posX, posY, player, alpha, &drawnWidth, &drawnHeight);

        posX = x + PADDING;
        posY = y + height - PADDING;
        UI_DrawWidgets(widgetsBottomLeft, sizeof(widgetsBottomLeft)/sizeof(widgetsBottomLeft[0]),
            UWF_BOTTOM2TOP, PADDING, posX, posY, player, alpha, &drawnWidth, &drawnHeight);

        posX = x + width - PADDING;
        posY = y + height - PADDING;
        UI_DrawWidgets(widgetsBottomRight, sizeof(widgetsBottomRight)/sizeof(widgetsBottomRight[0]),
            UWF_RIGHT2LEFT, PADDING, posX, posY, player, alpha, &drawnWidth, &drawnHeight);

        posX = x + PADDING + (width-PADDING*2)/2;
        posY = y + height - PADDING;
        UI_DrawWidgets(widgetsBottom, sizeof(widgetsBottom)/sizeof(widgetsBottom[0]),
            UWF_BOTTOM2TOP, PADDING, posX, posY, player, alpha, &drawnWidth, &drawnHeight);

#undef PADDING
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

/**
 * Draw teleport icon and show it on screen.
 */
void Draw_TeleportIcon(void)
{
    // Dedicated servers don't draw anything.
    if(IS_DEDICATED)
        return;

    GL_DrawRawScreen(W_CheckNumForName("TRAVLPIC"), 0, 0);
    GL_DrawPatch(100, 68, dpTeleIcon.lump);
}

/**
 * Called when the statusbar scale cvar changes.
 */
static void updateViewWindow(cvar_t* cvar)
{
    R_UpdateViewWindow(true);
    ST_HUDUnHide(CONSOLEPLAYER, HUE_FORCE); // So the user can see the change.
}
