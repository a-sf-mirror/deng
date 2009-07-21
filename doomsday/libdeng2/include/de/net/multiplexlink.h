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

#ifndef LIBDENG2_MULTIPLEXLINK_H
#define LIBDENG2_MULTIPLEXLINK_H

#include <de/deng.h>
#include <de/Link>

namespace de
{
    /**
     * Multiplexes one Link so that multiple isolated communication channels
     * can operate over it. Duplicates the Link class's sending and reception
     * interface.
     */
    class MultiplexLink : public Link
    {
    public:
        enum Channel {
            BASE = 0,
            UPDATES = 1,
            NUM_CHANNELS
        };
        
    public:
        /**
         * Constructs a new multiplex link. A new socket is created for the link.
         *
         * @param address  Address to connect to.
         */
        MultiplexLink(const Address& address);
        
        /**
         * Constructs a new multiplex linx. 
         *
         * @param socket  Socket to use for network traffic. MultiplexLink
         *      creates its own Link for this socket.
         */
        MultiplexLink(Socket* socket);
        
        virtual ~MultiplexLink();

        Consignment* receive();
        bool hasIncoming() const;
        
        /**
         * Sets the channel used for receiving packets.
         *
         * @param channel  Channel.
         */
        void tune(Channel channel);
        
    protected:
        /**
         * Gets all received packets and puts them into the channels' incoming buffers.
         */
        void demux();
        
    private:
        /// Each channel has its own incoming buffer.
        Link::IncomingBuffer buffers_[NUM_CHANNELS];
        
        /// Currently active channel for reception.
        Channel channel_;
    };
};

#endif /* LIBDENG2_MULTIPLEXLINK_H */
