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

#include "de/WADFeed"
#include "de/WADFile"
#include "de/WAD"
#include "de/Writer"
#include "de/Folder"
#include "de/FS"
#include "de/Log"

using namespace de;

WADFeed::WADFeed(File& wadFile) : _file(wadFile), _wad(0)
{
    // Open the archive.
    _wad = new WAD(wadFile);
}

WADFeed::~WADFeed()
{
    LOG_AS("WADFeed::~WADFeed");
    if(_wad)
        delete _wad;
}

void WADFeed::populate(Folder& folder)
{
    LOG_AS("WADFeed::populate");

    WAD::Names names;
    
    // Get a list of the files in this directory.
    wad().listFiles(names);
    
    for(WAD::Names::iterator i = names.begin(); i != names.end(); ++i)
    {
        if(folder.has(*i))
        {
            // Already has an entry for this, skip it (wasn't pruned so it's OK).
            return;
        }
        
        String entry = *i;
        
        std::auto_ptr<WADFile> wadFile(new WADFile(*i, wad(), entry));
        // Use the status of the entry within the archive.
        wadFile->setStatus(wad().status(entry));
        
        // Create a new file that accesses this feed's archive and interpret the contents.
        File* file = folder.fileSystem().interpret(wadFile.release());
        folder.add(file);

        // We will decide on pruning this.
        file->setOriginFeed(this);
        
        // Include the file in the main index.
        folder.fileSystem().index(*file);
    }
}

bool WADFeed::prune(File& file) const
{
    // Do nothing.
    return true;
}

WAD& WADFeed::wad()
{
    return *_wad;
}
