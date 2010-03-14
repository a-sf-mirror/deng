/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_view.c:
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "jdoom.h"
#  include "g_common.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#  include "g_common.h"
#elif __JHERETIC__
#  include "jheretic.h"
#  include "g_common.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "gamemap.h"
#include "p_player.h"
#include "p_tick.h"
#include "p_actor.h"

// MACROS ------------------------------------------------------------------

#define VIEW_HEIGHT         (cfg.plrViewHeight)

#define MAXBOB              (16) // pixels.

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Calculate the walking / running height adjustment.
 */
void P_CalcHeight(player_t* plr)
{
    GameMap* map = Thinker_Map((thinker_t*) plr->plr->mo);
    boolean airborne, morphed = false;
    ddplayer_t* ddplr = plr->plr;
    mobj_t* pmo = ddplr->mo;
    float target, step;

    // Regular movement bobbing (needs to be calculated for gun swing even
    // if not on ground).
    plr->bob = (pmo->mom[MX] * pmo->mom[MX]) + (pmo->mom[MY] * pmo->mom[MY]);
    plr->bob /= 4;
    if(plr->bob > MAXBOB)
        plr->bob = MAXBOB;

    // When flying, don't bob the view.
    if((pmo->flags2 & MF2_FLY) && pmo->pos[VZ] > pmo->floorZ)
    {
        plr->bob = 1.0f / 2;
    }

#if __JHERETIC__ || __JHEXEN__
    if(plr->morphTics)
        morphed = true;
#endif

    // During demo playback the view is thought to be airborne if viewheight
    // is zero (Cl_MoveLocalPlayer).
    if(Get(DD_PLAYBACK))
        airborne = !plr->viewHeight;
    else
        airborne = pmo->pos[VZ] > pmo->floorZ; // Truly in the air?

    // Morphed players don't bob their view.
    if(P_MobjIsCamera(ddplr->mo) /*$democam*/ ||
       (ddplr->flags & DDPF_CHASECAM) || airborne || morphed ||
       (P_GetPlayerCheats(plr) & CF_NOMOMENTUM))
    {
        // Reduce the bob offset to zero.
        target = 0;
    }
    else
    {
        angle_t angle = (FINEANGLES / 20 * map->time) & FINEMASK;
        target = cfg.bobView * ((plr->bob / 2) * FIX2FLT(finesine[angle]));
    }

    // Do the change gradually.
    if(airborne || plr->airCounter > 0)
        step = 4.0f - (plr->airCounter > 0 ? plr->airCounter * 0.2f : 3.5f);
    else
        step = 4.0f;

    if(plr->viewOffset[VZ] > target)
    {
        if(plr->viewOffset[VZ] - target > step)
            plr->viewOffset[VZ] -= step;
        else
            plr->viewOffset[VZ] = target;
    }
    else if(plr->viewOffset[VZ] < target)
    {
        if(target - plr->viewOffset[VZ] > step)
            plr->viewOffset[VZ] += step;
        else
            plr->viewOffset[VZ] = target;
    }

    // The aircounter will soften the touchdown after a fall.
    plr->airCounter--;
    if(airborne)
        plr->airCounter = TICSPERSEC / 2;

    // Should viewheight be moved? Not if camera or we're in demo.
    if(!((P_GetPlayerCheats(plr) & CF_NOMOMENTUM) ||
        P_MobjIsCamera(pmo) /*$democam*/ || Get(DD_PLAYBACK)))
    {
        // Move viewheight.
        if(plr->playerState == PST_LIVE)
        {
            plr->viewHeight += plr->viewHeightDelta;

            if(plr->viewHeight > VIEW_HEIGHT)
            {
                plr->viewHeight = VIEW_HEIGHT;
                plr->viewHeightDelta = 0;
            }
            else if(plr->viewHeight < VIEW_HEIGHT / 2.0f)
            {
                plr->viewHeight = VIEW_HEIGHT / 2.0f;
                if(plr->viewHeightDelta <= 0)
                    plr->viewHeightDelta = 1;
            }

            if(plr->viewHeightDelta)
            {
                plr->viewHeightDelta += 0.25f;
                if(!plr->viewHeightDelta)
                    plr->viewHeightDelta = 1;
            }
        }
    }

    // Set the plr's eye-level Z coordinate.
    plr->viewZ = pmo->pos[VZ] + (P_MobjIsCamera(pmo)? 0 : plr->viewHeight);

    // During demo playback (or camera mode) the viewz will not be modified
    // any further.
    if(!(Get(DD_PLAYBACK) || P_MobjIsCamera(pmo) ||
       (ddplr->flags & DDPF_CHASECAM)))
    {
        if(morphed)
        {   // Chicken or pig.
            plr->viewZ -= 20;
        }

        // Foot clipping is done for living players.
        if(plr->playerState != PST_DEAD)
        {
            if(pmo->floorClip && pmo->pos[VZ] <= pmo->floorZ)
            {
                plr->viewZ -= pmo->floorClip;
            }
        }
    }
}
