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

#include "de/fs.h"
#include "de/libraryfile.h"
#include "de/directoryfeed.h"

using namespace de;

FS::FS()
{
    root_.attach(new DirectoryFeed("."));
}

FS::~FS()
{}

void FS::refresh()
{
    root_.populate();
}

void FS::index(File& file)
{
    index_.insert(IndexEntry(file.name().lower(), &file));
}

void FS::deindex(File& file)
{
    if(!index_.empty()) 
    {
        for(Index::iterator i = index_.begin(); i != index_.end(); ++i)
        {
            if(i->second == &file)
            {
                // This is the one to deindex.
                index_.erase(i);
                break;
            }
        }
    }
}

void FS::printIndex()
{
    for(Index::iterator i = index_.begin(); i != index_.end(); ++i)
    {
        std::cout << "[" << i->first << "]: " << i->second->path() << "\n";
    }
}

Folder& FS::getFolder(const String& path)
{
    return root_;
}

File* FS::interpret(File* sourceData)
{
    if(LibraryFile::recognize(*sourceData))
    {
        std::cout << "Interpreted " << sourceData->name() << " as a shared library\n";
        
        // It is a shared library intended for Doomsday.
        return new LibraryFile(sourceData);
    }

    return sourceData;
}
