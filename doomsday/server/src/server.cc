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

#include "server.h"
#include <de/Date>
#include <de/CommandPacket>
#include <de/Socket>

#include <sstream>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "doomsday.h"

using namespace de;

Server::Server(const CommandLine& arguments)
    : App(arguments), listenSocket_(0)
{
    CommandLine& args = commandLine();

    // Start listening.
    duint16 port = DEFAULT_LISTEN_PORT;
    std::string param;
    if(args.getParameter("--port", param))
    {
        std::istringstream(param) >> port;
    }
    
    std::cout << "Server uses port " << port << "\n";
    listenSocket_ = new ListenSocket(port);
    
    args.append("-dedicated");
    
    args.append("-userdir");
    args.append("serverdir");

    // Initialize the engine.
    DD_Entry(0, NULL);
}

Server::~Server()
{
    // Close all links.
    for(Clients::iterator i = clients_.begin(); i != clients_.end(); ++i)
    {
        delete *i;
    }
    
    // Shutdown the engine.
    DD_Shutdown();

    delete listenSocket_;
}

void Server::iterate()
{
    // Check for incoming connections.
    Socket* incoming = listenSocket_->accept();
    if(incoming)
    {
        std::cout << "New client connected from " << incoming->peerAddress().asText() << "!\n";
        clients_.push_back(new Link(incoming));
    }

    tendClients();
    
    // libdeng main loop tasks.
    DD_GameLoop();
}

void Server::tendClients()
{
    for(Clients::iterator i = clients_.begin(); i != clients_.end(); ++i)
    {
        std::auto_ptr<AddressedBlock> message((*i)->receive());
        if(message.get())
        {
            std::auto_ptr<Packet> packet(protocol_.interpret(*message));
            if(packet.get())
            {
                packet.get()->setFrom(message->address());
                processPacket(packet.get());
            }            
        }
    }
}

void Server::processPacket(const Packet* packet)
{
    /// @todo  Check IP-based access rights.
    
    assert(packet != NULL);
    
    const CommandPacket* cmd = dynamic_cast<const CommandPacket*>(packet);
    if(cmd)
    {
        std::cout << "Server received command (from " << packet->from().asText() << 
            "): " << cmd->command() << "\n";
        
        if(cmd->command() == "quit")
        {
            stop();
        }
    }
    
    // Perform any function the packet may define for itself.    
    packet->execute();
}
