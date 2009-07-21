/*
 * The Doomsday Engine Project -- dengsv
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

#ifndef REMOTEUSER_H
#define REMOTEUSER_H

#include "server.h"
#include "session.h"
#include <de/User>
#include <de/Address>

/**
 * RemoteUser represents a User on the serverside.
 */
class RemoteUser
{
public:
    /**
     * Constructs a new remote user.
     *
     * @param client  Network link for communicating with the user.
     * @param session  Session to which this RemoteUser belongs.
     */
    RemoteUser(Server::Client& client, Session& session);
    
    virtual ~RemoteUser();

    /**
     * Returns the user's id.
     */
    const de::Id& id() const { return user_->id(); }

    /**
     * Returns the address of the remote user.
     */
    de::Address address() const;
    
    /**
     * Returns the network link for communicating with the remote user.
     */
    Server::Client& client() const;
    
    /**
     * Returns the session to which this remote user belongs.
     */
    Session& session() const;
    
    /**
     * Returns the User instance of the remote user.
     */
    de::User& user();
    
private:
    Server::Client* client_;
    
    /// Session to which this remote user belongs.
    Session& session_;
    
    /// The game user.
    de::User* user_;
};

#endif /* REMOTEUSER_H */
