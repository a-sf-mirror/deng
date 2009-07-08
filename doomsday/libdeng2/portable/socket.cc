/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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

#include "de/socket.h"
#include "de/block.h"
#include "internal.h"
#include "sdl.h"

#include <string>

using namespace de;

const duint RECEIVE_TIMEOUT = 500; //0000; // milliseconds

Socket::Socket(const Address& address) : socket_(0)
{
    IPaddress ip;

    internal::convertAddress(address, &ip);

    socket_ = SDLNet_TCP_Open(&ip);
    if(socket_ == NULL)
    {
        throw ConnectionError("Socket::Socket", "Failed to connect: " +
            std::string(SDLNet_GetError()));
    }
}

Socket::Socket(void* existingSocket) : socket_(existingSocket)
{}

Socket::~Socket()
{
    close();
}

void Socket::close()
{
    // Close the socket.
    if(socket_)
    {
        SDLNet_TCP_Close(static_cast<TCPsocket>(socket_));
        socket_ = 0;
    }
}

/// Write the 4-byte header to the beginning of the buffer.
void Socket::writeHeader(IByteArray::Byte* buffer, const IByteArray& packet)
{
    /** - 3 bits for flags. */
    duint flags = 0;

    /** - 2 bits for the protocol version number. */
    const duint PROTOCOL_VERSION = 0;
    
    /** - 16+11 bits for the packet length (max: 134 MB). */
    duint header = ( (packet.size() & 0x7ffffff) |
                     (PROTOCOL_VERSION << 27) |
                     (flags << 29) );

    SDLNet_Write32(header, buffer);
}

void Socket::readHeader(duint header, duint& size)
{
    duint hostHeader = SDLNet_Read32(&header);
    size = hostHeader & 0x7ffffff;

    // These aren't used for anything yet.
    /*
    unsigned protocol = (hostHeader >> 27) & 3;
    unsigned flags    = (hostHeader >> 29) & 7;
     */    
}

void Socket::send(const IByteArray& packet)
{
    if(!socket_) return;

    duint packetSize = packet.size() + 4;
    IByteArray::Byte* buffer = new IByteArray::Byte[packetSize];

    // Write the packet header: packet length, version, possible flags.
    writeHeader(buffer, packet);
    
    packet.get(0, buffer + 4, packet.size());
    unsigned int sentBytes = SDLNet_TCP_Send(
        static_cast<TCPsocket>(socket_), buffer, packetSize);
    delete [] buffer;

    // Did the transmission fail?
    if(sentBytes != packetSize)
    {
        throw DisconnectedError("Socket::send", std::string(SDLNet_GetError()));
    }
}

void Socket::receiveBytes(duint count, dbyte* buffer)
{
    duint received = 0;
    
    // Wait indefinitely until there is something to receive.
    while(received < count)
    {
        // There is something to receive.
        int recvResult = SDLNet_TCP_Recv(static_cast<TCPsocket>(socket_),
            buffer + received, count - received);
            
        if(recvResult <= 0)
        {
            // There is an error!
            throw DisconnectedError("Socket::receive",
                "While receiving data: " + std::string(SDLNet_GetError()));
        }
        
        received += recvResult;
    }
}

Block* Socket::receive()
{
    if(!socket_) return NULL;

    // First read the header.
    duint header = 0;
    receiveBytes(4, reinterpret_cast<dbyte*>(&header));

    duint incomingSize = 0;
    readHeader(header, incomingSize);

    std::auto_ptr<Block> data(new Block(incomingSize)); 
    receiveBytes(incomingSize, const_cast<dbyte*>(data.get()->data()));
    return data.release();
}
