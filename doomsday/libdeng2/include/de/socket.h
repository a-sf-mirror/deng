/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_SOCKET_H
#define LIBDENG2_SOCKET_H

#include <de/deng.h>
#include <de/IByteArray>

namespace de
{
    class Address;
    class Block;

    /**
     * Socket provides an interface for a TCP/IP network socket.
     * ListenSocket will generate Sockets for client connections.
     *
     * @ingroup net
     */
    class PUBLIC_API Socket
    {
    public:
        /// Creating the TCP/IP connection failed. @ingroup errors
        DEFINE_ERROR(ConnectionError);
        
        /// The TCP/IP connection was disconnected. @ingroup errors
        DEFINE_ERROR(DisconnectedError);
    
    public:
        Socket(const Address& address);
        virtual ~Socket();

        /**
         * Sends the given data over the socket.  Copies the data into
         * a temporary buffer before sending.
         *
         * @param data  Data to send.
         */
        void send(const IByteArray& data);

        /**
         * Receives an array of bytes by reading from the socket until a full
         * packet has been received.  Returns only after a full packet has been received. 
         *
         * @return  Received bytes. Caller is responsible for deleting the data.
         */
        Block* receive();
    
    private:
        /// Create a Socket object for a previously opened socket.  
        Socket(void* existingSocket);

        void initialize();

        /**
         * Receives a specific number of bytes from the socket. Blocks
         * until all the data has been read correctly. An exception is 
         * thrown if the connection fails.
         */
        void receiveBytes(duint count, dbyte* buffer);

        void writeHeader(IByteArray::Byte* buffer, const IByteArray& packet);

        void readHeader(duint header, duint& size);
    
        /// Pointer to the internal socket data.
        void* socket_;
        
        /// Set of sockets for waiting for incoming communication.
        void* socketSet_;

        /** 
         * ListenSocket creates instances of Socket so it needs to use
         * the special private constructor that takes an existing
         * socket data pointer as a parameter.
         */
        friend class ListenSocket;
    };
}

#endif /* LIBDENG2_SOCKET_H */
