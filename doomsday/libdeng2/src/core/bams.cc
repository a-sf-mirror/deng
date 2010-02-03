/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/math.h"

using namespace de;

namespace de
{
    #define BAMS_TABLE_ACCURACY_SHIFT   13
    #define BAMS_TABLE_ACCURACY         (1 << BAMS_TABLE_ACCURACY_SHIFT)

    static binangle_t atantable[BAMS_TABLE_ACCURACY];
}

/**
 * Fills the BAM LUTs.
 */
void de::InitBAMLUTs(void)
{
    dfloat fbta = (dfloat) BAMS_TABLE_ACCURACY;
    for(duint i = 0; i < BAMS_TABLE_ACCURACY; ++i)
    {
        atantable[i] = rad2bang(atan(i / fbta));
    }
}

binangle_t de::bamsAtan2(dint y, dint x)
{
    if(!x && !y)
        return BANG_0; // Indeterminate.

    dint64 absy = y, absx = x; // << TABLE_ACCURACY needs space.
    // Make sure the absolute values are absolute.
    if(absy < 0)
        absy = -absy;
    if(absx < 0)
        absx = -absx;

    // We'll first determine what the angle is in the first quadrant.
    // That's what the tables are for.
    binangle_t bang;
    if(!absy)
        bang = BANG_0;
    else if(absy == absx)
        bang = BANG_45;
    else if(!absx)
        bang = BANG_90;
    else
    {
        // The special cases didn't help. Use the tables.
        // absx and absy can't be zero here.
        if(absy > absx)
            bang = BANG_90 -
                atantable[(absx << BAMS_TABLE_ACCURACY_SHIFT) / absy];
        else
            bang = atantable[(absy << BAMS_TABLE_ACCURACY_SHIFT) / absx];
    }

    // Now we know the angle in the first quadrant. Let's look at the signs
    // and choose the right quadrant.
    if(x < 0) // Flip horizontally?
    {
        bang = BANG_180 - bang;
    }
    if(y < 0) // Flip vertically?
    {
        // At the moment bang must be smaller than 180.
        bang = BANG_180 + BANG_180 - bang;
    }
    // This is the final angle.
    return bang;
}
