/*
 * The Doomsday Engine Project -- dengsv
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

#include "session.h"
#include "server.h"
#include "remoteuser.h"
#include <de/data.h>
#include <de/Library>
#include <de/Protocol>

using namespace de;

Session::Session() : world_(0)
{
    // Create a blank world.
    world_ = App::game().SYMBOL(deng_NewWorld)();
}

Session::~Session()
{
    if(!users_.empty())
    {
        for(Users::iterator i = users_.begin(); i != users_.end(); ++i)
        {
            i->second->client().observers.remove(this);
            delete i->second;
        }
        users_.clear();
    }
    delete world_;
}

void Session::processCommand(Server::Client& sender, const CommandPacket& packet)
{
    try
    {
        if(packet.command() == "session.new")
        {
            // Initialize the session with the provided settings.
            world_->setMap(packet.arguments().value<TextValue>("map"));
            // Respond.
            App::protocol().reply(sender);
        }
        else if(packet.command() == "session.join")
        {
            // Request to join this session?
            Id which(packet.arguments().value<TextValue>("id"));
            if(which != id_)
            {
                // Not intended for this session.
                return;
            }
            RemoteUser& newUser = promote(sender);
     
            // The sender has provided us with the initial state.
            Reader(packet.arguments().value<BlockValue>("userState")) >> newUser.user();
     
            // Reply with the official user id.
            Record* reply = new Record();
            reply->addText("userId", newUser.id());
            App::protocol().reply(sender, Protocol::OK, reply);
        }
    }
    catch(const Error& err)
    {
        // No go, pal.
        App::protocol().reply(sender, Protocol::FAILURE, err.what());
    }
}

RemoteUser& Session::promote(Server::Client& client)
{
    try
    {
        userByAddress(client.peerAddress());
        throw AlreadyPromotedError("Session::promote", "Client from " + 
            client.peerAddress().asText() + " already is a user");
    }
    catch(const UnknownAddressError&)
    {
        // Compose a welcome packet for the new user.
        RecordPacket welcome("user.welcome");
        Writer(welcome.record().addBlock("worldState").value<BlockValue>()) << *world_;
        Record& userStateRec = welcome.record().addRecord("users");
        for(Users::iterator i = users_.begin(); i != users_.end(); ++i)
        {
            Writer(userStateRec.addBlock(i->second->user().id()).value<BlockValue>())
                << i->second->user();
        }
        client.send(welcome, client.UPDATES);

        RemoteUser* remote = new RemoteUser(client, *this);
        users_[remote->id()] = remote;

        // Start observing when this link closes.
        client.observers.add(this);

        // Update the others.
        RecordPacket userJoined("user.joined");
        userJoined.record().addText("id", remote->id());
        Writer(userJoined.record().addBlock("userState").value<BlockValue>()) 
            << remote->user();
        for(Users::iterator i = users_.begin(); i != users_.end(); ++i)
        {
            if(i->second != remote)
            {                
                i->second->client().send(userJoined, client.UPDATES);
            }
        }
        
        return *remote;
    }
}

RemoteUser& Session::userByAddress(const Address& address) const
{
    for(Users::const_iterator i = users_.begin(); i != users_.end(); ++i)
    {
        // Make RemoteUser a class of its own, that has a User instance.
        if(i->second->address() == address)
        {
            return *i->second;
        }
    }
    throw UnknownAddressError("Session::userByAddress", "No one has address " + address.asText());
}

void Session::linkBeingDeleted(de::Link& link)
{
    for(Users::iterator i = users_.begin(); i != users_.end(); ++i)
    {
        if(&i->second->client() == &link)
        {
            // This user's link has been closed. The remote user will disappear.
            delete i->second;
            users_.erase(i);
            return;
        }
    }
    std::cout << "Session::linkBeingDeleted: " << link.peerAddress() << " not used by any user\n";
}
