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

#include "de/MultiplexLink"
#include "de/Link"
#include "de/Consignment"

using namespace de;

MultiplexLink::MultiplexLink(const Address& address) : Link(address), channel_(BASE)
{}

MultiplexLink::MultiplexLink(Socket* socket) : Link(socket), channel_(BASE)
{}

MultiplexLink::~MultiplexLink()
{}

Consignment* MultiplexLink::receive()
{
    demux();
    return buffers_[channel_].get();
}

bool MultiplexLink::hasIncoming() const
{
    return !buffers_[channel_].empty();
}

void MultiplexLink::tune(Channel channel)
{
    channel_ = channel;
}

void MultiplexLink::demux()
{
    while(Link::hasIncoming())
    {
        std::auto_ptr<Consignment> cons(Link::receive());
        // We will quietly ignore channels we can't receive.
        if(cons->channel() < NUM_CHANNELS)
        {
            buffers_[cons->channel()].put(cons.release());
        }
    }
}
