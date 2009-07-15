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
 
#include "de/commandpacket.h"
#include "de/value.h"
#include "de/writer.h"
#include "de/reader.h"

using namespace de;

CommandPacket::CommandPacket() : Packet(COMMAND_PACKET_TYPE)
{}

CommandPacket::~CommandPacket()
{
    // Delete the argument values.
    for(Arguments::iterator i = arguments_.begin(); i != arguments_.end(); ++i)
    {
        delete *i;
    }
}

void CommandPacket::operator >> (Writer& to) const
{
    Packet::operator >> (to);
    
    // The command itself.
    to << command_;
    
    // Arguments.
    to << duint(arguments_.size());
    for(Arguments::const_iterator i = arguments_.begin(); i != arguments_.end(); ++i)
    {
        to << **i;
    }
}

void CommandPacket::operator << (Reader& from)
{
    Packet::operator << (from);
    
    // The command.
    from >> command_;
    
    duint count = 0;
    from >> count;
    while(count-- > 0)
    {
        arguments_.push_back(Value::constructFrom(from));
    }
}
