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

#include "de/Protocol"
#include "de/CommandPacket"
#include "de/RecordPacket"
#include "de/TextValue"
#include "de/Record"
#include "de/Transceiver"

using namespace de;

Protocol::Protocol()
{
    define(CommandPacket::fromBlock);
    define(RecordPacket::fromBlock);
}

Protocol::~Protocol()
{}

void Protocol::define(Constructor constructor)
{
    constructors_.push_back(constructor);
}

Packet* Protocol::interpret(const Block& block) const
{
    for(Constructors::const_iterator i = constructors_.begin(); i != constructors_.end(); ++i)
    {
        Packet* p = (*i)(block);
        if(p)
        {
            return p;
        }
    }
    return 0;
}

void Protocol::decree(Transceiver& to, const CommandPacket& command, RecordPacket** response)
{
    to << command;
    std::auto_ptr<RecordPacket> rep(to.receivePacket<RecordPacket>());

    // Check the answer.
    if(rep->label() == "failure")
    {
        /// @throw FailureError The response to @a command was FAILURE.
        throw FailureError("Protocol::decree", "Command '" + command.command() + 
            "' failed: " + rep->valueAsText("message"));
    }
    else if(rep->label() == "deny")
    {
        /// @throw DenyError The response to @a command was DENY.
        throw DenyError("Protocol::decree", "Command '" + command.command() +
            "' was denied: " + rep->valueAsText("message"));
    }
    std::cout << "Reply to the decree '" << command.command() << "' was:\n" <<
        rep->label() << ":\n" << rep->record();
    if(response)
    {
        *response = rep.release();
    }
}

void Protocol::reply(Transceiver& to, Reply type, Record* record)
{
    std::string label;
    switch(type)
    {
    case OK:
        label = "ok";
        break;
        
    case FAILURE:
        label = "failure";
        break;
        
    case DENY:
        label = "deny";
        break;
    }
    RecordPacket packet(label);
    if(record)
    {
        packet.take(record);
    }
    to << packet;
}

void Protocol::reply(Transceiver& to, Reply type, const std::string& message)
{
    Record* rec = new Record();
    if(!message.empty())
    {
        rec->addText("message", message);
    }
    reply(to, type, rec);
}
