/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_TIME_H
#define LIBDENG2_TIME_H

#include <de/deng.h>

namespace de
{
    /**
     * The Time class represents a single time measurement.  It should be
     * used wherever time needs to be measured, calculated or stored.
     */
    class PUBLIC_API Time
    {
    public:
        /// The type used to store time measurements.
        typedef ddouble Type;

    public:
        /// Constructor that initializes the time to the given initial
        /// value.
        Time(Type initial = 0);

        /// Conversion to the built-in type.
        operator const Type() const { return seconds_; }

        /// Add and assign.
        Time& operator += (const Time& t);

        /// Subtract and assign.
        Time& operator -= (const Time& t);

        /// Convert the time measurement to milliseconds.  @return The
        /// time as milliseconds.
        duint asMilliSeconds() const;

    public:
        /// Return the amount of time elapsed since the program was
        /// started.
        static Time now();
        
        /// Suspend execution for a period of time.
        static void sleep(const Time& duration);

    private:
        Type seconds_;
    };
}

#endif /* LIBDENG2_TIME_H */
