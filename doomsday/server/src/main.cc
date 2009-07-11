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

#include "server.h"

#include <de/ListenSocket>
#include <de/Socket>
#include <de/Time>
#include <de/Link>
#include <de/Block>
#include <de/Writer>

using namespace de;

int deng_Main(int argc, char** argv)
{
    try
    {
        Server server(CommandLine(argc, argv));
        
        /*
        ListenSocket entry(8080);
        Socket* client = 0;
        
        while((client = entry.accept()) == NULL)
        {
            std::cout << "Still waiting for incoming...\n";
            Time::sleep(.5);
        }

        std::cout << "Sending...\n";
        
        Link link(client);
        Block packet;
        Writer(packet) << "Hello world!";
        link.send(packet);
        
        Time::sleep(1);

        std::cout << "Closing...\n";
        return 0;
        */

        return server.mainLoop();
    }
    catch(const Error& error)
    {
        std::cout << error.what() << std::endl;
    }    
    return 0;
}
