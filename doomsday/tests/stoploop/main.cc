/*
 * The Doomsday Engine Project
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

#include <dengmain.h>
#include <de/ListenSocket>
#include <de/Socket>
#include <de/Link>
#include <de/Block>
#include <de/Writer>
#include <de/Reader>
#include <de/Time>
#include "../testapp.h"

using namespace de;

int deng_Main(int argc, char** argv)
{
    std::cout << "Server runs until client tells it to stop.\n";

    try
    {
        CommandLine args(argc, argv);
        TestApp app(args);
        
        if(args.has("--server"))
        {
            ListenSocket entry(8080);
            
            Socket* client = 0;
            while((client = entry.accept()) == NULL)
            {
                Time::sleep(.1);
            }

            Link link(client);
            Consignment* cons = 0;
            while((cons = link.receive()) == NULL)
            {
                Time::sleep(.1);
            }
            String str;
            Reader(*cons) >> str;
            std::cout << "Received '" << str << "'\n";
            delete cons;
        }
        else
        {
            Link link(Address("localhost", 8080));
            Block block;
            Writer(block) << "QUIT";        
            link << block;
        }
    }
    catch(const Error& err)
    {
        std::cout << err.what() << "\n";
    }

    std::cout << "Exiting deng_Main()...\n";
    return 0;        
}
