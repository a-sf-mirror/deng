/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/address.h"
#include "sdl.h"

#include <cstring>
#include <string>
#include <sstream>

using namespace de;

Address::Address() : ip_(0), port_(0)
{}

Address::Address(const char* address, duint16 port)
{
    set(address, port);
}

bool Address::operator == (const Address& other) const
{
    return ip_ == other.ip_ && port_ == other.port_;
}

void Address::set(const char* address, duint16 port)
{
    using std::string;
    using std::istringstream;

    string hostName;

    if(address)
    {
        const char *separator = strchr(address, ':');
        if(separator)
        {
            hostName = string(address, separator);

            // Use the port number that has been specified in the
            // address string.
            istringstream(separator + 1) >> port;
        }
        else
        {
            // We'll assume the entire address string is just a host
            // name.
            hostName = address;
        }
    }
    
    // Use SDL_net to resolve the name.
    IPaddress resolved;
    if(SDLNet_ResolveHost(&resolved, address? hostName.c_str() : NULL, port) < 0)
    {
        throw ResolveError("Address::set", SDLNet_GetError());
    }

    ip_   = SDLNet_Read32(&resolved.host);
    port_ = SDLNet_Read16(&resolved.port);
}

std::string Address::asText() const
{
    duint part[4] = {
        (ip_ >> 24) & 0xff,
        (ip_ >> 16) & 0xff,
        (ip_ >> 8) & 0xff,
        ip_ & 0xff
    };
    std::ostringstream os;
    os << part[0] << "." << part[1] << "." << part[2] << "." << part[3] << ":" << duint(port_);
    return os.str();
}
