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
#include <de/Address>
#include <de/Lockable>

/**
 * @defgroup net Network
 *
 * Classes responsible for network communications.
 */

namespace de
{
    class Consignment;

    /**
     * Socket provides an interface for a TCP/IP network socket.
     * ListenSocket will generate Sockets for client connections.
     *
     * @ingroup net
     */
    class PUBLIC_API Socket : public Lockable
    {
    public:
        // Flags.
        enum 
        {
            HUFFMAN = 0x1,      ///< Payload is in Huffman code.
            CHANNEL_1 = 0x2,    ///< Payload belongs to channel 1 instead of the default channel 0.
        };
        
        /// Creating the TCP/IP connection failed. @ingroup errors
        DEFINE_ERROR(ConnectionError);

        DEFINE_ERROR(BrokenError);
        
        /// The TCP/IP connection was disconnected. @ingroup errors
        DEFINE_SUB_ERROR(BrokenError, DisconnectedError);
        
        /// Incoming packet has an unknown block protocol. @ingroup errors
        DEFINE_SUB_ERROR(BrokenError, UnknownProtocolError);

        /// There is no peer connected. @ingroup errors
        DEFINE_SUB_ERROR(BrokenError, PeerError);
    
    public:
        Socket(const Address& address);
        virtual ~Socket();

        /**
         * Sends the given data over the socket.  Copies the data into
         * a temporary buffer before sending. The data is always sent on
         * channel zero.
         *
         * @param data  Data to send.
         *
         * @return  Reference to this socket.
         */
        Socket& operator << (const IByteArray& data);

        /**
         * Sends the given data over the socket. Copes the data into
         * a temporary buffer before sending.
         *
         * @param packet  Data to send.
         * @param channel  Channel to send the data on.
         */
        void send(const IByteArray& packet, duint channel = 0);

        /**
         * Receives an array of bytes by reading from the socket until a full
         * packet has been received.  Returns only after a full packet has been received. 
         * The block is marked with the address where it was received from.
         *
         * @return  Received bytes. Caller is responsible for deleting the data.
         */
        Consignment* receive();
        
        /**
         * Determines the IP address of the remote end of a connected socket.
         *
         * @return  Address of the peer.
         */
        Address peerAddress() const;
        
        void close();
        
    protected:
        struct Header {
            duint version;
            bool huffman;
            duint channel;
            duint size;
            Header();
        };
        
        /// Create a Socket object for a previously opened socket.  
        Socket(void* existingSocket);

        void initialize();

        /**
         * Receives a specific number of bytes from the socket. Blocks
         * until all the data has been read correctly. An exception is 
         * thrown if the connection fails.
         */
        void receiveBytes(duint count, dbyte* buffer);

        void writeHeader(const Header& header, IByteArray::Byte* buffer);

        void readHeader(duint headerBytes, Header& header);
        
        inline void checkValid();
    
    private:
        /// Pointer to the internal socket data.
        void* socket_;

        /// Used for waiting on activity.
        void* socketSet_;
        
        Address peerAddress_;
        
        /** 
         * ListenSocket creates instances of Socket so it needs to use
         * the special private constructor that takes an existing
         * socket data pointer as a parameter.
         */
        friend class ListenSocket;
    };
}

#endif /* LIBDENG2_SOCKET_H */
