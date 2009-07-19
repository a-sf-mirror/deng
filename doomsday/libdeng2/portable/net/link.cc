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

#include "de/Link"
#include "de/Socket"
#include "de/Address"
#include "de/Block"
#include "de/Time"
#include "de/Writer"
#include "de/Packet"
#include "de/App"
#include "de/Protocol"

using namespace de;

Link::Link(const Address& address) : socket_(0), sender_(0), receiver_(0)
{
    socket_ = new Socket(address);
    initialize();
}

Link::Link(Socket* socket) : socket_(socket), sender_(0), receiver_(0)
{
    initialize();
}

void Link::initialize()
{
    assert(socket_ != 0);
    
    sender_ = new SenderThread(*socket_, outgoing_);
    receiver_ = new ReceiverThread(*socket_, incoming_);
    
    sender_->start();
    receiver_->start();
    
    peerAddress_ = socket_->peerAddress();
}

Link::~Link()
{
    flush();
    
    // Inform the threads that they can stop as soon as possible.
    receiver_->stop();
    sender_->stop();

    // Close the socket.
    socket_->close();
    
    // Wake up the sender thread (it's waiting for outgoing packets).
    outgoing_.post();
        
    delete sender_;
    delete receiver_;
    delete socket_;
}

Link& Link::operator << (const IByteArray& data)
{
    outgoing_.put(new Block(data));
    outgoing_.post();
    return *this;
}

Link& Link::operator << (const Packet& packet)
{
    Block data;
    Writer(data) << packet;
    return *this << data;
}

Packet* Link::receivePacket(const Time::Delta& timeOut)
{
    Time startedAt;
    while(startedAt.since() <= timeOut)
    {
        std::auto_ptr<AddressedBlock> block(receive());
        if(!block.get())
        {
            // Wait for a bit.
            Time::sleep(.05);
            continue;
        }
        Packet* packet = App::protocol().interpret(*block.get());
        if(!packet)
        {
            throw UnexpectedError("Link::receivePacket", 
                "Expected a packet, but something else was received");
        }
        return packet;
    }
    throw TimeOutError("Link::receivePacket", "Timeout expired before anything was received");
}

AddressedBlock* Link::receive()
{
    AddressedBlock* b = incoming_.get();
    if(b)
    {
        // A packet was waiting.
        return b;
    }    
    if(!receiver_->isRunning())
    {
        // Receiver has stopped, which means the remote end closed.
        throw DisconnectedError("Link::receive", "Link has been closed");
    }
    return 0;
}

void Link::flush()
{
    while(sender_->isRunning() && !outgoing_.empty())
    {
        Time::sleep(.01);
    }
}

Address Link::peerAddress() const
{
    return peerAddress_;
}
