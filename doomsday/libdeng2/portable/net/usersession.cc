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

#include "de/UserSession"
#include "de/App"
#include "de/Library"
#include "de/Protocol"
#include "de/Link"
#include "de/User"
#include "de/World"
#include "de/CommandPacket"

using namespace de;

UserSession::UserSession(Link* link, Id session)
    : link_(link), user_(0), world_(0)
{
    // Create a blank user and world. The user is configured jointly
    // from configuration and by the game. The world is mirrored from
    // the server's session when we join.
    user_ = App::game().SYMBOL(deng_NewUser)();
    world_ = App::game().SYMBOL(deng_NewWorld)();
    
    // Ask to join the session.
    CommandPacket join("session.join");
    join.arguments().addNumber("id", session);
    App::app().protocol().decree(*link_, join);
}   

UserSession::~UserSession()
{
    delete user_;
    delete world_;    
    delete link_;
}
