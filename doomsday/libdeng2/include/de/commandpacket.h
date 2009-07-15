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
 
#ifndef LIBDENG2_COMMANDPACKET_H
#define LIBDENG2_COMMANDPACKET_H

#include <de/Packet>
#include <de/String>
#include <de/ArrayValue>

#include <list>

namespace de
{
    class Value;
    class Block;
    
    static const char* COMMAND_PACKET_TYPE = "CMND";
    
    /**
     * Command packet. Used for controlling a libdeng2 based application.
     *
     * @ingroup net
     */
    class PUBLIC_API CommandPacket : public Packet
    {    
    public:
        CommandPacket(const String& cmd = "");
        ~CommandPacket();
        
        const String& command() const { return command_; }
        
        void setCommand(const String& c) { command_ = c; }

        const ArrayValue& arguments() const { return arguments_; }
        
        ArrayValue& arguments() { return arguments_; }
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);

    public:
        /// Constructor for the Protocol.
        static Packet* fromBlock(const Block& block);
        
    private:
        String command_;
        
        /// Command arguments.
        ArrayValue arguments_;
    };
} 

#endif /* LIBDENG2_COMMANDPACKET_H */
 