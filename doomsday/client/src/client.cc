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
#include "localserver.h"
#include <de/Link>
#include <de/CommandPacket>
#include <de/RecordPacket>
#include <de/Record>
#include <de/Session>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "doomsday.h"

using namespace de;

Client::Client(const CommandLine& arguments)
    : App(arguments), localServer_(0), session_(0)
{        
    CommandLine& args = commandLine();
    
#ifdef MACOSX
    args.append("-iwad");
    args.append("/Users/jaakko/IWADs/Doom.wad");
#endif

    args.append("-file");
#if defined(MACOSX)
    args.append("}Resources/doomsday.pk3");
    args.append("}Resources/doom.pk3");
#elif defined(WIN32)
    args.append("..\\..\\data\\doomsday.pk3");
    args.append("..\\..\\data\\doom.pk3");
#else
    args.append("../../data/doomsday.pk3");
    args.append("../../data/doom.pk3");
#endif

    const duint16 SERVER_PORT = 13209;

    std::auto_ptr<LocalServer> svPtr(new LocalServer(SERVER_PORT));
    localServer_ = svPtr.get();

    /*
    args.append("-cmd");
    args.append("net-port-control 13211; net-port-data 13212; after 30 \"net init\"; after 50 \"connect localhost:13209\"");
    */
    
    args.append("-userdir");
    args.append("clientdir");

    // Initialize the engine.
    DD_Entry(0, NULL);
    
    // DEVEL: Join the game session.
    // Query the on-going sessions.
    Link* link = new Link(Address("localhost", SERVER_PORT));
    duint sessionToJoin;

    CommandPacket createSession("session.new");
    createSession.arguments().addText("map", "E1M1");
    protocol().decree(*link, createSession);
    
    *link << CommandPacket("status");
    // We should receive a reply directly.
    std::auto_ptr<RecordPacket> status(link->receive<RecordPacket>());
    std::cout << "Here's what the server said:\n" << status->label() << "\n" << status->record();
        
    //session_ = new UserSession(link);
    delete link;
    
    // Good to go.
    svPtr.release();
}

Client::~Client()
{
    delete localServer_;
    
    // Shutdown the engine.
    DD_Shutdown();
}

void Client::iterate()
{
    // libdeng main loop tasks.
    DD_GameLoop();
}
