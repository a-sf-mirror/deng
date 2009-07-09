/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
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
 * p_maputl.c: Movement/collision map utility functions.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#include "jdoom64.h"

#include "dmu_lib.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Apply "torque" to objects hanging off of ledges, so that they fall off.
 * It's not really torque, since Doom has no concept of rotation, but it's
 * a convincing effect which avoids anomalies such as lifeless objects
 * hanging more than halfway off of ledges, and allows objects to roll off
 * of the edges of moving lifts, or to slide up and then back down stairs,
 * or to fall into a ditch.
 *
 * If more than one linedef is contacted, the effects are cumulative, so
 * balancing is possible.
 */
static boolean PIT_ApplyTorque(linedef_t* ld, void* data)
{
    mobj_t*             mo = tmThing;
    float               dist;
    sector_t*           frontsec, *backsec;
    float               ffloor, bfloor;
    float               d1[2], vtx[2];

    if(tmThing->player)
        return true; // Skip players!

    if(!(frontsec = DMU_GetPtrp(ld, DMU_FRONT_SECTOR)) ||
       !(backsec = DMU_GetPtrp(ld, DMU_BACK_SECTOR)))
        return true; // Shouldn't ever happen.

    ffloor = DMU_GetFloatp(frontsec, DMU_FLOOR_HEIGHT);
    bfloor = DMU_GetFloatp(backsec, DMU_FLOOR_HEIGHT);
    DMU_GetFloatpv(ld, DMU_DXY, d1);
    DMU_GetFloatpv(DMU_GetPtrp(ld, DMU_VERTEX0), DMU_XY, vtx);

    // Lever-arm:
    dist = +d1[0] * mo->pos[VY] - d1[1] * mo->pos[VX] -
            d1[0] * vtx[VY]    +  d1[1] * vtx[VX];

    if((dist < 0  && ffloor < mo->pos[VZ] && bfloor >= mo->pos[VZ]) ||
       (dist >= 0 && bfloor < mo->pos[VZ] && ffloor >= mo->pos[VZ]))
    {
        // At this point, we know that the object straddles a two-sided
        // linedef, and that the object's center of mass is above-ground.
        float               x = fabs(d1[0]), y = fabs(d1[1]);

        if(y > x)
        {
            float       tmp = x;
            x = y;
            y = tmp;
        }

        y = FIX2FLT(finesine[(tantoangle[FLT2FIX(y / x) >> DBITS] +
                             ANG90) >> ANGLETOFINESHIFT]);

        /**
         * Momentum is proportional to distance between the object's center
         * of mass and the pivot linedef.
         *
         * It is scaled by 2^(OVERDRIVE - gear). When gear is increased, the
         * momentum gradually decreases to 0 for the same amount of
         * pseudotorque, so that oscillations are prevented, yet it has a
         * chance to reach equilibrium.
         */

        if(mo->gear < OVERDRIVE)
            dist = (dist * FIX2FLT(FLT2FIX(y) << -(mo->gear - OVERDRIVE))) / x;
        else
            dist = (dist * FIX2FLT(FLT2FIX(y) >> +(mo->gear - OVERDRIVE))) / x;

        // Apply momentum away from the pivot linedef.
        x = d1[1] * dist;
        y = d1[0] * dist;

        // Avoid moving too fast all of a sudden (step into "overdrive").
        dist = (x * x) + (y * y);
        while(dist > 4 && mo->gear < MAXGEAR)
        {
            ++mo->gear;
            x /= 2;
            y /= 2;
            dist /= 2;
        }

        mo->mom[MX] -= x;
        mo->mom[MY] += y;
    }

    return true;
}

/**
 * Applies "torque" to objects, based on all contacted linedefs.
 * $dropoff_fix
 */
void P_ApplyTorque(mobj_t *mo)
{
    int                 flags = mo->intFlags;

    // Corpse sliding anomalies, made configurable.
    if(!cfg.slidingCorpses)
        return;

    tmThing = mo;

    // Use VALIDCOUNT to prevent checking the same line twice.
    VALIDCOUNT++;

    P_MobjLinesIterator(mo, PIT_ApplyTorque, 0);

    // If any momentum, mark object as 'falling' using engine-internal
    // flags.
    if(mo->mom[MX] != 0 || mo->mom[MY] != 0)
        mo->intFlags |= MIF_FALLING;
    else
        // Clear the engine-internal flag indicating falling object.
        mo->intFlags &= ~MIF_FALLING;

    /**
     * If the object has been moving, step up the gear. This helps reach
     * equilibrium and avoid oscillations.
     *
     * DOOM has no concept of potential energy, much less of rotation, so we
     * have to creatively simulate these systems somehow :)
     */

    // If not falling for a while, reset it to full strength.
    if(!((mo->intFlags | flags) & MIF_FALLING))
        mo->gear = 0;
    else if(mo->gear < MAXGEAR) // Else if not at max gear, move up a gear.
        mo->gear++;
}
