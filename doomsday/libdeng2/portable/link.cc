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

#include "de/link.h"
#include "de/socket.h"
#include "de/address.h"
#include "de/block.h"

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
    assert(socket_ != NULL);
    
    sender_ = new SenderThread(*socket_, outgoing_);
    receiver_ = new ReceiverThread(*socket_, incoming_);
    
    sender_->start();
    receiver_->start();
}

Link::~Link()
{
    // Inform the threads that they can stop as soon as possible.
    receiver_->stop();
    sender_->stop();

    // Close the socket.
    socket_->close();
    
    // Wake up the sender thread.
    outgoing_.post();
        
    delete sender_;
    delete receiver_;
    delete socket_;
}

void Link::send(const IByteArray& data)
{
    outgoing_.put(new Block(data));
    outgoing_.post();
}

IByteArray* Link::receive()
{
    return incoming_.get();
}
