/*
 * The Doomsday Engine Project -- dengcl
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

#ifndef USERSESSION_H
#define USERSESSION_H

#include <de/Id>
#include <de/User>
#include <de/World>
#include <de/MuxLink>
#include <de/Packet>

#include <map>

/**
 * UserSession maintain the game session on the clientside.
 *
 * @ingroup client
 */
class UserSession
{
public:
    /**
     * Constructs a new user session.
     *
     * @param link  Open connection to the server. Ownership given to UserSession.
     * @param id  Session to join. Ongoing sessions on a server can be queried with
     *      the "status" command.
     */
    UserSession(de::MuxLink* link, const de::Id& id = 0);
    
    virtual ~UserSession();
    
    /**
     * Listen to updates and other data coming from the server.
     */
    void listen();
    
protected:
    /// Listens on the updates channel.
    void listenForUpdates();
    
    /// Processes a packet received from the server.
    void processPacket(const de::Packet& packet);

    void clearOthers();
    
private:
    /// Link to the server.
    de::MuxLink* link_;
    
    /// Id of the session on the server.
    de::Id sessionId_;

    /// The game world. Mirrors the game world in the server's Session.
    de::World* world_;

    /// The user that owns the UserSession.
    de::User* user_;

    /// The others.
    typedef std::map<de::Id, de::User*> Others;
    Others others_;
};

#endif /* USERSESSION_H */
