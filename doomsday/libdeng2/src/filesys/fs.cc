/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2009-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2010 Daniel Swanson <danij@dengine.net>
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

#include "de/FS"
#include "de/LibraryFile"
#include "de/DirectoryFeed"
#include "de/ArchiveFeed"
#include "de/Archive"
#include "de/WADFeed"
#include "de/WAD"
#include "de/Log"

using namespace de;

FS::FS()
{}

FS::~FS()
{}

void FS::refresh()
{
    _root.populate();
    
    printIndex();
}

Folder& FS::getFolder(const String& path)
{
    Folder* subFolder = _root.tryLocate<Folder>(path);
    if(!subFolder)
    {
        // This folder does not exist yet. Let's create it.
        Folder& parentFolder = getFolder(path.fileNamePath());
        subFolder = new Folder(path.fileName());
        parentFolder.add(subFolder);
        index(*subFolder);
    }
    return *subFolder;
}

File* FS::interpret(File* sourceData)
{
    LOG_AS("FS::interpret");
    
    /// @todo  One should be able to define new interpreters dynamically.
    
    try
    {
        if(LibraryFile::recognize(*sourceData))
        {
            LOG_VERBOSE("Interpreted ") << sourceData->name() << " as a shared library";
        
            // It is a shared library intended for Doomsday.
            return new LibraryFile(sourceData);
        }
        if(Archive::recognize(*sourceData))
        {
            LOG_VERBOSE("Interpreted ") << sourceData->name() << " as a ZIP archive";
        
            // It is a ZIP archive. The folder will own the source file.
            std::auto_ptr<Folder> zip(new Folder(sourceData->name()));
            zip->setSource(sourceData);    
            zip->attach(new ArchiveFeed(*sourceData));
            return zip.release();
        }
        /// @todo  Move the WAD interpreter into a plugin.
        if(WAD::recognize(*sourceData))
        {
            LOG_VERBOSE("Interpreted ") << sourceData->name() << " as a WAD archive";

            // It is a WAD archive. The folder will own the source file.
            std::auto_ptr<Folder> wad(new Folder(sourceData->name()));
            wad->setSource(sourceData);
            wad->attach(new WADFeed(*sourceData));
            return wad.release();
        }
    }
    catch(const Error& err)
    {
        LOG_ERROR("") << err.asText();

        // We were given responsibility of the source file.
        delete sourceData;
        err.raise();
    }
    return sourceData;
}

void FS::find(const String& path, FoundFiles& found) const
{
    found.clear();
    String baseName = path.fileName().lower();
    String dir = path.fileNamePath().lower();
    if(!dir.beginsWith("/"))
    {
        // Always begin with a slash. We don't want to match partial folder names.
        dir = "/" + dir;
    }
    ConstIndexRange range = _index.equal_range(baseName);
    for(Index::const_iterator i = range.first; i != range.second; ++i)    
    {
        File* file = i->second;
        if(file->path().endsWith(dir))
        {
            found.push_back(file);
        }
    }
}

File& FS::findSingle(const String& path) const
{
    return find<File>(path);
}

void FS::index(File& file)
{
    const String lowercaseName = file.name().lower();
    
    _index.insert(IndexEntry(lowercaseName, &file));
    
    // Also make an entry in the type index.
    Index& indexOfType = _typeIndex[TYPE_NAME(file)];
    indexOfType.insert(IndexEntry(lowercaseName, &file));
}

static void removeFromIndex(FS::Index& idx, File& file)
{
    if(idx.empty()) 
    {
        return;
    }

    // Look up the ones that might be this file.
    FS::IndexRange range = idx.equal_range(file.name().lower());

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
    removeFromIndex(_index, file);
    removeFromIndex(_typeIndex[TYPE_NAME(file)], file);
}

const FS::Index& FS::indexFor(const String& typeName) const
{
    TypeIndex::const_iterator found = _typeIndex.find(typeName);
    if(found != _typeIndex.end())
    {
        return found->second;
    }
    /// @throw UnknownTypeError No files of type @a typeName have been indexed.
    throw UnknownTypeError("FS::indexForType", "No files of type '" + typeName + "' have been indexed");
}

void FS::printIndex()
{
    for(Index::iterator i = _index.begin(); i != _index.end(); ++i)
    {
        LOG_DEBUG("\"%s\": ") << i->first << i->second->path();
    }
    
    for(TypeIndex::iterator k = _typeIndex.begin(); k != _typeIndex.end(); ++k)
    {
        LOG_DEBUG("\nIndex for type '%s':") << k->first;
        for(Index::iterator i = k->second.begin(); i != k->second.end(); ++i)
        {
            LOG_DEBUG("\"%s\": ") << i->first << i->second->path();
        }
    }
}
