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

#ifndef LIBDENG2_USERSESSION_H
#define LIBDENG2_USERSESSION_H

#include <de/Session>

namespace de
{
    class User;
    class World;
    class Link;
    class Address;
    
    /**
     * A session instance is the host for a game session. It owns the game world
     * and is responsible for synchronizing the clients' UserSession instances.
     */
    class UserSession
    {
    public:
        /**
         * Constructs a new user session.
         *
         * @param link  Open connection to the server. Ownership given to UserSession.
         * @param session  Session to join.
         */
        UserSession(Link* link, Id session = 0);
        
        virtual ~UserSession();
        
    private:
        /// Link to the server.
        Link* link_;

        /// The user that owns the UserSession.
        User* user_;

        /// The game world. Mirrors the game world in the server's Session.
        World* world_;
    };
};

#endif /* LIBDENG2_USERSESSION_H */
