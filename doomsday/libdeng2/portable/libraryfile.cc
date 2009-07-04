/*
 * The Doomsday Engine Project -- libdeng2
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

#include "de/libraryfile.h"

using namespace de;

LibraryFile::LibraryFile(File* source)
    : File(source->name()), source_(source)
{}

LibraryFile::~LibraryFile()
{
    delete source_;
}

bool LibraryFile::recognize(const File& file)
{
#if defined(MACOSX)
    if((file.name().beginsWith("libdeng_") ||
        file.name().beginsWith("libdengplugin_")) &&
        String::fileNameExtension(file.name()) == ".dylib")
    {
        return true;
    }
#elif defined(UNIX)
    if((file.name().beginsWith("libdeng_") ||
        file.name().beginsWith("libdengplugin_")) &&
        String::fileNameExtension(file.name()) == ".so")
    {
        return true;
    }
#elif defined(WIN32)
    if((file.name().beginsWith("deng_") ||
        file.name().beginsWith("dengplugin_")) &&
        String::fileNameExtension(file.name()) == ".dll")
    {
        return true;
    }
#endif
    return false;
}
