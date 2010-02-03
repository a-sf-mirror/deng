/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2004-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_MATH_H
#define LIBDENG2_MATH_H

#include <de/deng.h>

#ifdef min
#   undef min
#endif

#ifdef max
#   undef max
#endif

namespace de
{
    const ddouble PI = 3.14159265358979323846;
    const ddouble EPSILON = 1.0e-7;

    /// Absolute value.
    template <typename Type>
    inline Type abs(const Type& a) {
        if(a < 0.0) {
            return -a;
        }
        return a;
    }

    // Special case, this is never negative.
    inline duint abs(const duint& a) {
        return a;
    }

    /// Minimum of two values.
    template <typename Type>
    inline const Type& min(const Type& a, const Type& b) {
        return (a < b? a : b);
    }

    /// Maximum of two values.
    template <typename Type>
    inline const Type& max(const Type& a, const Type& b) {
        return (a > b? a : b);
    }

    /// Clamp value within range.
    template <typename Type>
    inline Type clamp(const Type& low, const Type& value, const Type& high) {
        return min(max(value, low), high);
    }

    /// Compare two floating-point values for equality, with the precision of EPSILON.
    inline ddouble fequal(ddouble a, ddouble b) {
        return abs(a - b) < EPSILON;
    }

    /// Return the first 32-bit integer larger than the specified value.
    template <typename Type>
    inline dint ceilpow2(const Type& val) {
        dint cumul;
        for(cumul = 1; val > cumul; cumul <<= 1);
        return cumul;
    }

    /// General comparison function.
    template <typename Type>
    inline dint cmp(const Type& a, const Type& b) {
        if(a < b) return -1;
        if(a > b) return 1;
        return 0;
    }

    /**
     * Binary Angle Mathematics (BAMS).
     */
#if LIBDENG2_BAMS_BITS == 32
    typedef duint32 dbinangle;
#elif LIBDENG2_BAMS_BITS == 16
    typedef duint16 dbinangle;
#else
    typedef duint8 dbinangle;
#endif

    /// Some predefined angles.
#if LIBDENG2_BAMS_BITS == 32
    static const dbinangle BANG_0   = 0; // East.
    static const dbinangle BANG_45  = 0x20000000; // Northeast.
    static const dbinangle BANG_90  = 0x40000000; // North.
    static const dbinangle BANG_135 = 0x60000000; // Northwest.
    static const dbinangle BANG_180 = 0x80000000; // West.
    static const dbinangle BANG_225 = 0xa0000000; // Southwest.
    static const dbinangle BANG_270 = 0xc0000000; // South.
    static const dbinangle BANG_315 = 0xe0000000; // Southeast.
    static const dbinangle BANG_360 = BANG_0;
    static const dbinangle BANG_MAX = 0xffffffff; // Largest representable value.
#elif LIBDENG2_BAMS_BITS == 16
    static const dbinangle BANG_0   = 0; // East.
    static const dbinangle BANG_45  = 0x2000; // Northeast.
    static const dbinangle BANG_90  = 0x4000; // North.
    static const dbinangle BANG_135 = 0x6000; // Northwest.
    static const dbinangle BANG_180 = 0x8000; // West.
    static const dbinangle BANG_225 = 0xa000; // Southwest.
    static const dbinangle BANG_270 = 0xc000; // South.
    static const dbinangle BANG_315 = 0xe000; // Southeast.
    static const dbinangle BANG_360 = BANG_0;
    static const dbinangle BANG_MAX = 0xffff; // Largest representable value.
#else // Byte-sized.
    static const dbinangle BANG_0   = 0; // East.
    static const dbinangle BANG_45  = 0x20; // Northeast.
    static const dbinangle BANG_90  = 0x40; // North.
    static const dbinangle BANG_135 = 0x60; // Northwest.
    static const dbinangle BANG_180 = 0x80; // West.
    static const dbinangle BANG_225 = 0xa0; // Southwest.
    static const dbinangle BANG_270 = 0xc0; // South.
    static const dbinangle BANG_315 = 0xe0; // Southeast.
    static const dbinangle BANG_360 = BANG_0;
    static const dbinangle BANG_MAX = 0xff; // Largest representable value.
#endif

    /// Compass directions, for convenience.
    static const dbinangle BANG_EAST      = BANG_0;
    static const dbinangle BANG_NORTHEAST = BANG_45;
    static const dbinangle BANG_NORTH     = BANG_90;
    static const dbinangle BANG_NORTHWEST = BANG_135;
    static const dbinangle BANG_WEST      = BANG_180;
    static const dbinangle BANG_SOUTHWEST = BANG_225;
    static const dbinangle BANG_SOUTH     = BANG_270;
    static const dbinangle BANG_SOUTHEAST = BANG_315;

    static const dfloat BAMS_PI = 3.14159265359f;

    /// Radians to binary angle.
    template <typename Type>
    inline dbinangle rad2bang(const Type& rad) {
        return static_cast<dbinangle>(rad / BAMS_PI * BANG_180);
    }

    /// Binary angle to radians.
    inline dfloat bang2rad(dbinangle bang) {
        return static_cast<dfloat>((bang / (dfloat) BANG_180 * BAMS_PI));
    }

    /// Binary angle to degrees.
    inline dfloat bang2deg(dbinangle bang) {
        return static_cast<dfloat>(bang / (dfloat) BANG_180 * 180);
    }

    void            InitBAMLUTs(void); // Fill in the tables.
    dbinangle      bamsAtan2(dint y, dint x);
}

#endif /* LIBDENG2_MATH_H */
