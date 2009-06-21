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
    
    args.append("-game");
    args.append("libdeng_doom.dylib");
    args.append("-file");
    args.append("../../data/doomsday.pk3");
    args.append("../../data/doom.pk3");
    args.append("-cmd");
    args.append("\"net-port-control 13211; net-port-data 13212; after 30 \"\"net init\"\"; after 50 \"\"connect localhost:13209\"\"\"");
    args.append("-userdir");
    args.append("clientdir");
    args.append("-libdir");
    args.append("../plugins");

    CommandLine svArgs = args;
    svArgs.remove(0);
    svArgs.insert(0, "./dengsv");
    extern char** environ;
    svArgs.execute(environ);
    
    return DD_Entry(args.count(), const_cast<char**>(args.argv()));
}
