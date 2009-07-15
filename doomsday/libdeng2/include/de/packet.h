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

#ifndef LIBDENG2_PACKET_H
#define LIBDENG2_PACKET_H

#include <de/ISerializable>
#include <de/Address>

namespace de
{
    /// Type identifier for instances of the Packet class.
    static const char* PACKET_TYPE = "NONE";
    
    /**
     * Base class for all network packets in the libdeng2 network communications protocol.
     * All packets are based on this.
     *
     * @ingroup net
     */
    class Packet : public ISerializable
    {
    public:
        /// Length of a type identifier.
        static const duint TYPE_SIZE = 4;
        
        typedef std::string Type;
        
    public:
        /**
         * Constructs an empty packet.
         *
         * @param type  Type identifier of the packet.
         */
        Packet(const Type& type = PACKET_TYPE);
        
        virtual ~Packet() {}
        
        /**
         * Returns the type identifier of the packet.
         */
        const Type& type() const { return type_; }
        
        /**
         * Sets the type identifier.
         *
         * @param t  New type identifier. Must be exactly TYPE_SIZE characters long.
         */
        void setType(const Type& t);

        /** 
         * Determines where the packet was received from.
         */
        const Address& from() const { return from_; }

        /**
         * Sets the address where the packet was received from.
         *
         * @param from  Address of the sender.
         */ 
        void setFrom(Address& from) { from_ = from; }

        /**
         * Execute whatever action the packet defines. This is called for all packets
         * once received and interpreted by the protocol. A packet defined outside 
         * libdeng2 may use this to add functionality to the packet.
         */
        virtual void execute() const;

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    private:  
        /// The type is identifier with a four-character string.
        Type type_;
        
        /// Address where the packet was received from.
        Address from_;
    };
};

#endif /* LIBDENG2_PACKET_H */
