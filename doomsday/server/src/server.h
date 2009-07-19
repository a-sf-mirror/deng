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

#ifndef SERVER_H
#define SERVER_H

#include <de/App>
#include <de/ListenSocket>
#include <de/Protocol>
#include <de/Link>
#include <de/Session>

#define DEFAULT_LISTEN_PORT 13209

/**
 * The server application.
 */
class Server : public de::App
{
public:
    /// No session is currently active. @ingroup errors
    DEFINE_ERROR(NoSessionError);
    
    /// Specified address was not in use by any client. @ingroup errors
    DEFINE_ERROR(UnknownAddressError);
    
    typedef de::Link Client;
    
public:
    Server(const de::CommandLine& commandLine);
    ~Server();
    
    void iterate();

    /**
     * Returns the client with the given address. 
     *
     * @param address  Address of client.
     *
     * @return  Client using the address.
     */
    Client& clientByAddress(const de::Address& address) const;

    /**
     * Check if there are any incoming requests from connected clients.
     * Process any incoming packets.
     */
    void tendClients();

    /**
     * Process a packet received from the network.
     *
     * @param packet  Packet.
     */    
    void processPacket(const de::Packet& packet);

    /**
     * Sends information about the current status of the server to a client.
     *
     * @param to  Address of the client to send the status to.
     */
    void replyStatus(const de::Address& to);
    
private:
    /// The server listens on this socket.
    de::ListenSocket* listenSocket_;
    
    /// The active game session.
    de::Session* session_;
    
    typedef std::list<Client*> Clients;
    Clients clients_;
};

#endif /* SERVER_H */
