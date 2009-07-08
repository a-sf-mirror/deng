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

#include "client.h"

#include <de/Link>
#include <de/Address>
#include <de/Reader>
#include <de/Time>

using namespace de;

int main(int argc, char** argv)
{
    try
    {
        Client client(CommandLine(argc, argv));
        
        Link link(Address("localhost", 8080));
        while(!link.hasIncoming())
        {
            std::cout << "Waiting for data\n";
            Time::sleep(.1);
        }
        
        IByteArray* data = link.receive();
        std::string str;
        Reader(*data) >> str;        
        
        std::cout << "Received '" << str << "'\n";
        
        return 0;
        
        return client.mainLoop();
    }
    catch(const Error& error)
    {
        std::cout << error.what() << std::endl;
    }    
    return 0;
}
