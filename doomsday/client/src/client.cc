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

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "doomsday.h"

using namespace de;

Client::Client(const CommandLine& arguments)
    : App(arguments)
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

    localServer_ = new LocalServer(13209);

    /*
    args.append("-cmd");
    args.append("net-port-control 13211; net-port-data 13212; after 30 \"net init\"; after 50 \"connect localhost:13209\"");
    */
    
    args.append("-userdir");
    args.append("clientdir");

    // Initialize the engine.
    DD_Entry(0, NULL);
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
