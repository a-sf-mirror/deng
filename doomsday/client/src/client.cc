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

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "client.h"
#include "doomsday.h"

using namespace de;

Client::Client(const CommandLine& commandLine)
    : App(commandLine)
{}

Client::~Client()
{}

dint Client::mainLoop()
{
    CommandLine& args = commandLine();

    CommandLine svArgs = args;
#ifdef WIN32
    svArgs.insert(0, "dengsv.exe");
#endif
#ifdef MACOSX
    svArgs.insert(0, String::fileNamePath(args.at(0)).concatenatePath("../Resources/dengsv"));
#endif
    svArgs.remove(1);
    extern char** environ;
    svArgs.execute(environ);

    args.append("-wnd");

    args.append("-iwad");
    args.append("/Users/jaakko/IWADs/Doom.wad");

    args.append("-file");
    args.append("}Resources/doomsday.pk3");
    args.append("}Resources/doom.pk3");
    args.append("-cmd");
    args.append("net-port-control 13211; net-port-data 13212; after 30 \"net init\"; after 50 \"connect localhost:13209\"");
    args.append("-userdir");
    args.append("clientdir");
    
    return DD_Entry(0, NULL);
}
