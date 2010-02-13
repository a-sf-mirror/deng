/*
 * The Doomsday Engine Project
 *
 * Copyright © 2009-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
#include <de/data.h>
#include <de/types.h>
#include <de/fs.h>
#include "../testapp.h"

#include <iostream>

using namespace de;

int deng_Main(int argc, char** argv)
{
    try
    {
        CommandLine args(argc, argv);
        TestApp app(args);

        Block b;
        Writer(b, littleEndianByteOrder) << duint32(0x11223344);
        duint32 v;
        Reader(b, littleEndianByteOrder) >> v;
        LOG_MESSAGE("%x") << v;

        Folder& wad = app.fileSystem().find<WAD>("test.wad");

        LOG_MESSAGE("Here's test.wad's info:\n") << wad.info();
        LOG_MESSAGE("Root's info:\n") << app.fileRoot().info();

        const File& hello = wad.locate<File>("HELLO");
        File::Status stats = hello.status();
        LOG_MESSAGE("HELLO size: %i bytes") << stats.size;

        String content(hello);
        LOG_MESSAGE("The contents: \"%s\"") << content;
    }
    catch(const Error& err)
    {
        std::cerr << err.asText() << "\n";
    }

    std::cout << "Exiting deng_Main()...\n";
    return 0;
}
