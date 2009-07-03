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

#include "de/nativefolder.h"
#include "de/nativefile.h"
#include "de/string.h"
#include "de/fs.h"

#ifdef UNIX
#   include <sys/types.h>
#   include <dirent.h>
#endif

#ifdef WIN32
#   include <io.h>
#endif

using namespace de;

NativeFolder::NativeFolder(const std::string& name, const std::string& nativePath)
    : Folder(name), nativePath_(nativePath)
{
    std::cout << "NativeFolder: " << name << ": " << nativePath << "\n";
}

NativeFolder::~NativeFolder()
{}

void NativeFolder::populate()
{
#ifdef UNIX
    DIR* dir = opendir(nativePath_.empty()? "." : nativePath_.c_str());
    if(!dir)
    {
        throw NotFoundError("NativeFolder::populate", "Path '" + nativePath_ + "' not found");
    }
    struct dirent* entry;
    while((entry = readdir(dir)) != NULL)   
    {
        const std::string entryName = entry->d_name;
        switch(entry->d_type)
        {
        case DT_DIR:
            populateSubFolder(entryName);
            break;
            
        default:
            populateFile(entryName);
            break;
        }
    } 
    closedir(dir);
#endif

#ifdef WIN32
    _finddata_t fd;
    intptr_t handle;
    if((handle = _findfirst(nativePath_.concatenateNativePath("*").c_str(), &fd)) != -1L)
    {
        // Found something.
        do
        {
            const std::string entryName = fd.name;
            if(fd.attrib & _A_SUBDIR)
            {
                populateSubFolder(entryName);
            }
            else
            {
                populateFile(entryName);
            }
        } 
        while(!_findnext(handle, &fd));
        _findclose(handle);
    }
#endif
}

void NativeFolder::populateSubFolder(const std::string& entryName)
{
    if(entryName != "." && entryName != "..")
    {
        add(new NativeFolder(entryName,
            nativePath_.concatenateNativePath(entryName))).populate();
    }
}

void NativeFolder::populateFile(const std::string& entryName)
{
    NativeFile& file = add(new NativeFile(entryName, 
        nativePath_.concatenateNativePath(entryName)));
        
    // Include files the main index.
    fileSystem().index(file);
}
