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

#include "de/Id"
#include "de/String"

#include <sstream>

using namespace de;

const Id::Type Id::NONE = 0;

// The Id generator starts from one.
Id::Type Id::generator_ = 1;

Id::Id() : id_(generator_++)
{
    if(id_ == NONE) 
    {
        ++id_;   
    }
}

Id::~Id()
{}

Id::operator std::string () const
{
    return asText();
}
    
String Id::asText() const
{
    std::ostringstream os;
    os << *this;
    return os.str();
}

std::ostream& de::operator << (std::ostream& os, const Id& id)
{
    os << "{" << duint(id) << "}";
    return os;
}
