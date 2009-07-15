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
#include "de/block.h"

using namespace de;

CommandPacket::CommandPacket(const String& cmd) : Packet(COMMAND_PACKET_TYPE), command_(cmd)
{}

CommandPacket::~CommandPacket()
{}

void CommandPacket::operator >> (Writer& to) const
{
    Packet::operator >> (to);
    to << command_ << arguments_;
}

void CommandPacket::operator << (Reader& from)
{
    Packet::operator << (from);
    from >> command_ >> arguments_;
}

Packet* CommandPacket::fromBlock(const Block& block)
{
    Reader from(block);
    if(checkType(from, COMMAND_PACKET_TYPE))
    {    
        std::auto_ptr<CommandPacket> p(new CommandPacket);
        from >> *p.get();
        return p.release();
    }
    return NULL;
}
