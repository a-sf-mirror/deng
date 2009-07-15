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

#include <cmath>
#include <sstream>
#include <iomanip>

#ifdef UNIX
#   include <ctime>
#   include <sys/time.h>
#endif

using namespace de;

duint64 Time::Delta::asMilliSeconds() const
{
    return duint64(seconds_ * 1000);
}

Time::Delta Time::Delta::operator + (const ddouble& d) const
{
    return seconds_ + d;
}

Time::Delta Time::Delta::operator - (const ddouble& d) const
{
    return seconds_ - d;
}

void Time::Delta::sleep() const
{
    SDL_Delay(asMilliSeconds());
}

Time::Time() 
{
#ifdef UNIX
    struct timeval tv;
    gettimeofday(&tv, 0);
    time_ = tv.tv_sec;
    micro_ = tv.tv_usec;
#endif
}

bool Time::operator < (const Time& t) const
{
#ifdef UNIX
    if(time_ > t.time_)
    {
        return false;
    }
    return time_ < t.time_ || micro_ < t.micro_;
#endif
}

bool Time::operator == (const Time& t) const
{
#ifdef UNIX
    return time_ == t.time_ && micro_ == t.micro_;
#endif
}

Time Time::operator + (const Delta& delta) const
{
    Time result = *this;
    result += delta;
    return result;
}

Time& Time::operator += (const Delta& delta)
{
#ifdef UNIX
    ddouble amount = std::fabs(delta);
    ddouble fullSeconds = std::floor(amount);
    ddouble fraction = amount - fullSeconds;
    if(delta > 0.0)
    {
        time_ += time_t(fullSeconds);
        micro_ += dint(fraction * 1.0e6);
        if(micro_ > 1e6)
        {
            micro_ -= 1e6;
            time_ += 1;
        }
    }
    else
    {
        time_ -= time_t(fullSeconds);
        micro_ -= dint(fraction * 1.0e6);
        if(micro_ < 0)
        {
            micro_ += 1e6;
            time_ -= 1;
        }
    }
#endif
    return *this;
}

Time::Delta Time::operator - (const Time& earlierTime) const
{
#ifdef UNIX
    ddouble seconds = std::difftime(time_, earlierTime.time_);
    // The fraction.
    seconds += (micro_ - earlierTime.micro_) / 1.0e6;
    return seconds;
#endif    
}

std::string Time::asText() const
{
    std::ostringstream os;
    os << time_ << "." << std::setw(6) << std::setfill('0') << micro_;
    return os.str();
}
