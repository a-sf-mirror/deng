/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/date.h"

#include <ctime>
#include <sstream>
#include <iomanip>

using namespace de;

Date::Date(const Time& at) : microSeconds(at.micro_)
{
#ifdef UNIX
    struct tm t;
    localtime_r(&at.time_, &t);
#endif

#ifdef WIN32
    struct tm t;
    memcpy(&t, _localtime64(&at.time_), sizeof(t));
#endif

    seconds = t.tm_sec;
    minutes = t.tm_min;
    hours = t.tm_hour;
    month = t.tm_mon + 1;
    year = t.tm_year + 1900;
    weekDay = t.tm_wday;
    dayOfMonth = t.tm_mday;
    dayOfYear = t.tm_yday;
}

std::string Date::asText() const
{
    using std::setfill;
    using std::setw;

    std::ostringstream os;
    os << setfill('0') << 
        year << "-" << 
        setw(2) << month << "-" << 
        setw(2) << dayOfMonth << " " << 
        setw(2) << hours << ":" << 
        setw(2) << minutes << ":" << 
        setw(2) << seconds << "." << 
        setw(2) << microSeconds/1000;
    return os.str();
}
