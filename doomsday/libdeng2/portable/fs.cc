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
    
    printIndex();
}

void FS::index(File& file)
{
    const String lowercaseName = file.name().lower();
    
    index_.insert(IndexEntry(lowercaseName, &file));
    
    // Also make an entry in the type index.
    Index& indexOfType = typeIndex_[TYPE_NAME(file)];
    indexOfType.insert(IndexEntry(lowercaseName, &file));
}

static void removeFromIndex(FS::Index& idx, File& file)
{
    if(idx.empty()) 
    {
        return;
    }

    // Look up the ones that might be this file.
    std::pair<FS::Index::iterator, FS::Index::iterator> range =
        idx.equal_range(file.name().lower());

    for(FS::Index::iterator i = range.first; i != range.second; ++i)
    {
        if(i->second == &file)
        {
            // This is the one to deindex.
            idx.erase(i);
            break;
        }
    }
}

void FS::deindex(File& file)
{
    std::cout << "Deindexing " << file.name() << " of type " << TYPE_NAME(file) << "\n";
    
    removeFromIndex(index_, file);
    removeFromIndex(typeIndex_[TYPE_NAME(file)], file);
}

const FS::Index& FS::indexFor(const std::string& typeName) const
{
    TypeIndex::const_iterator found = typeIndex_.find(typeName);
    if(found != typeIndex_.end())
    {
        return found->second;
    }
    throw UnknownTypeError("FS::indexForType", "No files of type '" + typeName + "' have been indexed");
}

void FS::printIndex()
{
    for(Index::iterator i = index_.begin(); i != index_.end(); ++i)
    {
        std::cout << "[" << i->first << "]: " << i->second->path() << "\n";
    }
    
    for(TypeIndex::iterator k = typeIndex_.begin(); k != typeIndex_.end(); ++k)
    {
        std::cout << "\nIndex for type '" << k->first << "':\n";
        for(Index::iterator i = k->second.begin(); i != k->second.end(); ++i)
        {
            std::cout << "[" << i->first << "]: " << i->second->path() << "\n";
        }
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
