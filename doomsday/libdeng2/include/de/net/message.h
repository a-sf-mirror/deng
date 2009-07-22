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

#ifndef LIBDENG2_MESSAGE_H
#define LIBDENG2_MESSAGE_H

#include <de/Block>
#include <de/Address>

namespace de
{
    /**
     * Data block with the sender's network address and a multiplex channel.
     *
     * @ingroup net
     */
    class PUBLIC_API Message : public Block
    {
    public:
        typedef duint Channel;
        
    public:
        Message(const IByteArray& other);    
        Message(const Address& addr, Channel channel, Size initialSize = 0);
        Message(const Address& addr, Channel channel, const IByteArray& other);    
        Message(const Address& addr, Channel channel, const IByteArray& other, Offset at, Size count);        

        /**
         * Returns the address associated with the block.
         */
        const Address& address() const {
            return address_;
        }

        /**
         * Sets the channel over which the block was received.
         *
         * @param channel  Multiplex channel.
         */
        void setChannel(Channel channel) { channel_ = channel; }
        
        /**
         * Returns the channel over which the block was received.
         */
        Channel channel() const { return channel_; }
        
    private:
        Address address_;
        Channel channel_;
    };    
}

#endif /* LIBDENG2_MESSAGE_H */
