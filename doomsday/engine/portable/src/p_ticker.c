/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_ticker.c: Timed Playsim Events
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"

#include "r_sky.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

int P_MobjTicker(void* p, void* context)
{
    uint                i;
    mobj_t*             mo = (mobj_t*) p;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        int                 f;
        byte*               haloFactor = &mo->haloFactors[i];

        // Set the high bit of halofactor if the light is clipped. This will
        // make P_Ticker diminish the factor to zero. Take the first step here
        // and now, though.
        if(mo->lumIdx == 0 || LO_IsClipped(mo->lumIdx, i))
        {
            if(*haloFactor & 0x80)
            {
                f = (*haloFactor & 0x7f); // - haloOccludeSpeed;
                if(f < 0)
                    f = 0;
                *haloFactor = f;
            }
        }
        else
        {
            if(!(*haloFactor & 0x80))
            {
                f = (*haloFactor & 0x7f); // + haloOccludeSpeed;
                if(f > 127)
                    f = 127;
                *haloFactor = 0x80 | f;
            }
        }

        // Handle halofactor.
        f = *haloFactor & 0x7f;
        if(*haloFactor & 0x80)
        {
            // Going up.
            f += haloOccludeSpeed;
            if(f > 127)
                f = 127;
        }
        else
        {
            // Going down.
            f -= haloOccludeSpeed;
            if(f < 0)
                f = 0;
        }

        *haloFactor &= ~0x7f;
        *haloFactor |= f;
    }

    return true; // Continue iteration.
}

boolean PIT_ClientMobjTicker(clmobj_t *cmo, void *parm)
{
    P_MobjTicker((thinker_t*) &cmo->mo, NULL);

    // Continue iteration.
    return true;
}

/**
 * Doomsday's own play-ticker.
 */
void P_Ticker(timespan_t time)
{
    static trigger_t fixed = { 1.0 / 35, 0 };
    gamemap_t* map;

    P_ControlTicker(time);
    Materials_Ticker(time);

    if(!M_RunTrigger(&fixed, time))
        return;

    R_SkyTicker();

    map = P_CurrentMap();
    if(map)
    {
        // New ptcgens for planes?
        P_CheckPtcPlanes(map);

        P_IterateThinkers(map, gx.MobjThinker, ITF_PUBLIC | ITF_PRIVATE, P_MobjTicker, NULL);

        // Check all client mobjs.
        Cl_MobjIterator(map, PIT_ClientMobjTicker, NULL);
    }
}
