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

#include "remoteuser.h"
#include <de/App>
#include <de/Library>

using namespace de;

RemoteUser::RemoteUser(Server::Client& client, Session& session) 
    : client_(&client), session_(session), user_(0)
{
    user_ = App::game().SYMBOL(deng_NewUser)();
}

RemoteUser::~RemoteUser()
{
    delete user_;
}

Server::Client& RemoteUser::client() const
{
    assert(client_ != NULL);
    return *client_;
}

Session& RemoteUser::session() const
{
    return session_;
}

de::User& RemoteUser::user()
{
    return *user_;
}

de::Address RemoteUser::address() const
{
    return client().peerAddress();
}
