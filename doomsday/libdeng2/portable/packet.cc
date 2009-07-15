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

#include "de/packet.h"
#include "de/writer.h"
#include "de/reader.h"

using namespace de;

Packet::Packet(const Type& t)
{
    setType(t);
}

void Packet::setType(const Type& t)
{
    assert(t.size() == TYPE_SIZE);
    type_ = t;
}

void Packet::operator >> (Writer& to) const
{
    to << type_[0] << type_[1] << type_[2] << type_[3];
}

void Packet::operator << (Reader& from)
{
    char ident[5];
    from >> ident[0] >> ident[1] >> ident[2] >> ident[3];
    ident[4] = 0;
    type_ = std::string(ident);
}

void Packet::execute() const
{}
