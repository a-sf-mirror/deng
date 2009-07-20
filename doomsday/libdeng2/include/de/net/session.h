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

#ifndef LIBDENG2_SESSION_H
#define LIBDENG2_SESSION_H

#include <de/deng.h>
#include <de/Id>

namespace de
{
    class CommandPacket;
    class Link;
    class World;
    
    /**
     * A session instance is the host for a game session. It owns the game world
     * and is responsible for synchronizing the clients' UserSession instances.
     */
    class Session
    {
    public:
        Session();
        
        virtual ~Session();
        
        Id id() const { return id_; }

        /**
         * Process a command related to the session. Any access rights must be
         * checked before calling this.
         *
         * @param sender  Sender of the command. A reply will be sent here.
         * @param packet  Packet received from the network.
         */
        void processCommand(Link& sender, const CommandPacket& packet);
        
    private:
        Id id_;
        
        /// The game world.
        World* world_;
    };
};

#endif /* LIBDENG2_SESSION_H */
