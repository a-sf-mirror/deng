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
#include "de/Record"
#include "de/Link"

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

void Protocol::decree(Link& to, const CommandPacket& command, RecordPacket** response)
{
    to << command;
    std::auto_ptr<RecordPacket> rep(to.receive<RecordPacket>());
    // Check the answer.
    if(rep->label() == "failure")
    {
        throw FailureError("Protocol::decree", "Command '" + command.command() + 
            "' failed: " + (*rep)["message"].value().asText());
    }
    else if(rep->label() == "deny")
    {
        throw DenyError("Protocol::decree", "Command '" + command.command() +
            "' was denied: " + (*rep)["message"].value().asText());
    }
    if(response)
    {
        *response = rep.release();
    }
}

void Protocol::reply(Link& to, Reply type, const std::string& message)
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
    if(!message.empty())
    {
        packet.record().addText("message", message);
    }
    to << packet;
}
