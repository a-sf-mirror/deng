/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * hu_pspr.c: Common HUD psprite handling.
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "dmu_lib.h"
#include "g_controls.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#if __JHERETIC__
static float PSpriteSY[NUM_PLAYER_CLASSES][NUM_WEAPON_TYPES] = {
// Player
    { 0, 5, 15, 15, 15, 15, 15, 15},
// Chicken
    {15, 15, 15, 15, 15, 15, 15, 15}
};
#endif

#if __JHEXEN__
static float PSpriteSY[NUM_PLAYER_CLASSES][NUM_WEAPON_TYPES] = {
// Fighter
    {0, -12, -10, 10},
// Cleric
    {-8, 10, 10, 0},
// Mage
    {9, 20, 20, 20},
// Pig
    {10, 10, 10, 10}
};
#endif

// CODE --------------------------------------------------------------------

/**
 * Calculates the Y offset for the player's psprite. The offset depends
 * on the size of the game window.
 */
float HU_PSpriteYOffset(player_t* pl)
{
    int viewWindowHeight = Get(DD_VIEWWINDOW_HEIGHT);
    float offy = (cfg.plrViewHeight - DEFAULT_PLAYER_VIEWHEIGHT) * 2;

#if __JHERETIC__ || __JHEXEN__
    if(viewWindowHeight == SCREENHEIGHT)
        offy += PSpriteSY[pl->class][pl->readyWeapon];
#endif

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    // If the status bar is visible, the sprite is moved up a bit.
    if(viewWindowHeight < SCREENHEIGHT)
        offy -= (float) (ST_HEIGHT - 2) * (cfg.statusbarScale / 20.f) - 20;
#endif

    return offy;
}

/**
 * Calculates the Y offset for the player's psprite. The offset depends
 * on the size of the game window.
 */
void HU_UpdatePlayerSprite(int pnum)
{
    int         i;
    pspdef_t   *psp;
    ddpsprite_t *ddpsp;
    float       fov = 90; //*(float*) Con_GetVariable("r_fov")->ptr;
    player_t   *pl = players + pnum;

    for(i = 0; i < NUMPSPRITES; ++i)
    {
        psp = pl->pSprites + i;
        ddpsp = pl->plr->pSprites + i;
        if(!psp->state) // A NULL state?
        {
            //ddpsp->sprite = -1;
            ddpsp->statePtr = 0;
            continue;
        }

        ddpsp->statePtr = psp->state;
        ddpsp->tics = psp->tics;
        ddpsp->flags = 0;

        // Fullbright?
        if((psp->state->flags & STF_FULLBRIGHT) ||
            (pl->powers[PT_INFRARED] > 4 * 32) || (pl->powers[PT_INFRARED] & 8)
# if __JDOOM__ || __JDOOM64__
            || (pl->powers[PT_INVULNERABILITY] > 30)
# endif
            )
        {
            ddpsp->flags |= DDPSPF_FULLBRIGHT;
        }

        // Translucent?
        ddpsp->alpha = 1;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        if((pl->powers[PT_INVISIBILITY] > 4 * 32) ||
           (pl->powers[PT_INVISIBILITY] & 8))
        {   // shadow draw
            ddpsp->alpha = .25f;
        }
#elif __JHEXEN__
        if(pl->powers[PT_INVULNERABILITY] && pl->class == PCLASS_CLERIC)
        {
            if(pl->powers[PT_INVULNERABILITY] > 4 * 32)
            {
                if(pl->plr->mo->flags2 & MF2_DONTDRAW)
                {               // don't draw the psprite
                    ddpsp->alpha = .333f;
                }
                else if(pl->plr->mo->flags & MF_SHADOW)
                {
                    ddpsp->alpha = .666f;
                }
            }
            else if(pl->powers[PT_INVULNERABILITY] & 8)
            {
                ddpsp->alpha = .333f;
            }
        }
#endif

        // Offset from center.
        ddpsp->pos[VX] = psp->pos[VX] - G_GetLookOffset(pnum) * 1300;

        if(fov > 90)
            fov = 90;

        ddpsp->pos[VY] = psp->pos[VY] + (90 - fov) / 90 * 80;
    }
}

/**
 * Updates the state of the player sprites (gives their data to the
 * engine so it can render them). Servers handle psprites of all players.
 */
void HU_UpdatePsprites(void)
{
    int         i;
    float       offsetY = HU_PSpriteYOffset(&players[CONSOLEPLAYER]);

    DD_SetVariable(DD_PSPRITE_OFFSET_Y, &offsetY);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            if(!IS_CLIENT || CONSOLEPLAYER == i)
            {
                HU_UpdatePlayerSprite(i);
            }
        }
    }
}