/*
 * The Doomsday Engine Project -- dengcl
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

#include "usersession.h"
#include "client.h"
#include <de/data.h>
#include <de/Id>
#include <de/Library>
#include <de/Protocol>
#include <de/CommandPacket>
#include <de/BlockValue>
#include <de/Writer>

using namespace de;

UserSession::UserSession(MuxLink* link, const Id& session)
    : link_(link), user_(0), world_(0)
{
    // Create a blank user and world. The user is configured jointly
    // from configuration and by the game. The world is mirrored from
    // the server's session when we join.
    user_ = App::game().SYMBOL(deng_NewUser)();
    world_ = App::game().SYMBOL(deng_NewWorld)();
    
    // Ask to join the session.
    CommandPacket join("session.join");
    join.arguments().addText("id", session);

    // Include our initial user state in the arguments.
    Writer(join.arguments().addBlock("userState").value<BlockValue>()) << *user_;

    RecordPacket* response;
    App::app().protocol().decree(*link_, join, &response);
    // Get the user id.
    user_->setId(response->valueAsText("userId"));
}   

UserSession::~UserSession()
{
    delete user_;
    delete world_;    
    delete link_;
}

void UserSession::processPacket(const Packet& packet)
{
    
}

void UserSession::listenForUpdates()
{
    for(;;)
    {
        std::auto_ptr<Message> message(link_->updates().receive());
        if(!message.get())
        {
            // That was all.
            break;
        }
        std::auto_ptr<Packet> packet(App::protocol().interpret(*message));
        if(packet.get())
        {
            // It's always from the server.
            packet.get()->setFrom(message->address());
            processPacket(*packet.get());
        }
    }
}

void UserSession::listen()
{
    try
    {
        listenForUpdates();
    }
    catch(const ISerializable::DeserializationError&)
    {
        // Malformed packet!
        std::cout << "Server sent sent nonsense.\n";
    }
}
