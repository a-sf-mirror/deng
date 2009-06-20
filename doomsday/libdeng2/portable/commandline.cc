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

#include "de/commandline.h"

using namespace de;

CommandLine::CommandLine(int argc, char** v)
{
    for(int i = 0; i < argc; ++i)
    {
        arguments_.push_back(v[i]);
        pointers_.push_back(arguments_[i].c_str());
    }
}

void CommandLine::append(const std::string& arg)
{
    arguments_.push_back(arg);
    pointers_.push_back(arguments_.rbegin()->c_str());
}

const char* const* CommandLine::argv() const
{
    return &pointers_[0];
}
