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
 
#ifndef LIBDENG2_PROTOCOL_H
#define LIBDENG2_PROTOCOL_H

#include <de/deng.h>

#include <list>

namespace de
{
    class Block;
    class Packet;
    
    /**
     * The protocol is responsible for recognizing an incoming data packet and
     * constructing a specialized packet object of the appropriate type.
     *
     * @ingroup net
     */
    class PUBLIC_API Protocol
    {
    public:
        /**
         * A constructor function examines a block of data and determines 
         * whether a specialized Packet can be constructed based on the data.
         *
         * @param block  Block of data.
         *
         * @return  Specialized Packet, or @c NULL.
         */
        typedef Packet* (*Constructor)(const Block&);
        
    public:
        Protocol();
        
        ~Protocol();
        
        /**
         * Registers a new constructor function.
         *
         * @param constructor  Constructor.
         */
        void define(Constructor constructor);
        
        /**
         * Interprets a block of data.
         *
         * @param block  Block of data that should contain a Packet of some type.
         *
         * @return  Specialized Packet, or @c NULL.
         */
        Packet* interpret(const Block& block) const;

    private:
        typedef std::list<Constructor> Constructors;
        Constructors constructors_;
    };
}

#endif /* LIBDENG2_PROTOCOL_H */
