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

#include "de/time.h"
#include "sdl.h"

using namespace de;

Time::Time(Type initial) : seconds_(initial)
{}

Time& Time::operator += (const Time& t)
{
    seconds_ += t.seconds_;
    return *this;
}

Time& Time::operator -= (const Time& t)
{
    seconds_ -= t.seconds_;
    return *this;
}

duint Time::asMilliSeconds() const
{
    return static_cast<duint>(seconds_ * 1000);
}

Time Time::now()
{
    /// @todo Detect when the number of ticks wraps around (49 days).
    return SDL_GetTicks() / 1000.0;
}
