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

//#ifdef WIN32
//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>
//#endif

#include "server.h"

using namespace de;

//#ifndef WIN32
int main(int argc, char** argv)
//#else
//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
//#endif
{
    try
    {
//#ifndef WIN32
        CommandLine args(argc, argv);
//#else
        //CommandLine args;
//#endif
        Server server(args);
        return server.mainLoop();
    }
    catch(const Error& error)
    {
        std::cout << error.what() << std::endl;
    }    
    return 0;
}
