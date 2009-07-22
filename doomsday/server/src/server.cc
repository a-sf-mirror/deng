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
#include "session.h"
#include <de/data.h>
#include <de/Date>
#include <de/CommandPacket>
#include <de/RecordPacket>
#include <de/Socket>

#include <sstream>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "doomsday.h"

using namespace de;

Server::Server(const CommandLine& arguments)
    : App(arguments, "none", "none"), listenSocket_(0), session_(0)
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
    delete session_;

    // Close all links.
    for(Clients::iterator i = clients_.begin(); i != clients_.end(); ++i)
    {
        delete *i;
    }
    clients_.clear();
    
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
        std::cout << "New client connected from " << incoming->peerAddress() << "!\n";
        clients_.push_back(new Client(incoming));
    }

    tendClients();
    
    // libdeng main loop tasks.
    DD_GameLoop();
}

Server::Client& Server::clientByAddress(const Address& address) const
{
    for(Clients::const_iterator i = clients_.begin(); i != clients_.end(); ++i)
    {
        if((*i)->peerAddress() == address)
        {
            return **i;
        }
    }
    throw UnknownAddressError("Server::clientByAddress", "Address not in use by any client");
}

void Server::tendClients()
{
    for(Clients::iterator i = clients_.begin(); i != clients_.end(); )
    {
        bool deleteClient = false;
        
        try
        {
            // Process incoming packets.
            std::auto_ptr<Message> message((*i)->base().receive());
            if(message.get())
            {
                std::auto_ptr<Packet> packet(protocol().interpret(*message));
                if(packet.get())
                {
                    packet.get()->setFrom(message->address());
                    processPacket(*packet.get());
                }            
            }
        }
        catch(const ISerializable::DeserializationError&)
        {
            // Malformed packet!
            std::cout << "Client from " << (*i)->peerAddress() << " sent nonsense.\n";
            deleteClient = true;
        }
        catch(const NoSessionError&)
        {
            std::cout << "Client from " << (*i)->peerAddress() << 
                " tried to access nonexistent session.\n";
            deleteClient = true;
        }
        catch(const UnknownAddressError&)
        {}
        catch(const Link::DisconnectedError&)
        {
            // The client was disconnected.
            std::cout << "Client from " << (*i)->peerAddress() << " disconnected.\n";
            deleteClient = true;
        }

        // Move on.
        if(deleteClient)
        {
            delete *i;
            clients_.erase(i++);
        }
        else
        {
            ++i;
        }
    }
}

void Server::processPacket(const Packet& packet)
{
    /// @todo  Check IP-based access rights.
    
    const CommandPacket* cmd = dynamic_cast<const CommandPacket*>(&packet);
    if(cmd)
    {
        std::cout << "Server received command (from " << packet.from() << 
            "): " << cmd->command() << "\n";
          
        // Session commands are handled by the session.
        if(cmd->command().beginsWith("session."))
        {
            if(cmd->command() == "session.new")
            {
                if(session_)
                {
                    // Could allow several...
                    delete session_;
                }
                // Start a new session.
                session_ = new Session();
            }
            if(!session_)
            {
                throw NoSessionError("Server::processPacket", "No session available");
            }
            // Execute the command.
            session_->processCommand(clientByAddress(packet.from()), *cmd);
        }
        else if(cmd->command() == "status")
        {
            replyStatus(packet.from());
        }
        else if(cmd->command() == "quit")
        {
            stop();
        }
    }
    
    // Perform any function the packet may define for itself.    
    packet.execute();
}

void Server::replyStatus(const Address& to)
{
    RecordPacket status("server.status");
    Record& rec = status.record();
    
    // Version.
    const Version v = version();
    ArrayValue& array = rec.addArray("version").value<ArrayValue>();
    array.add(new NumberValue(v.major));
    array.add(new NumberValue(v.minor));
    array.add(new NumberValue(v.patchlevel));
    array.add(new NumberValue(v.revision));
    
    // The sessions.
    Record& sub = rec.addRecord("sessions");
    if(session_)
    {
        /*Record& sesSub =*/ sub.addRecord(session_->id());
        // Information about the session.
        
    }
    
    clientByAddress(to).base() << status;
}

Server& Server::server()
{
    return static_cast<Server&>(App::app());
}
