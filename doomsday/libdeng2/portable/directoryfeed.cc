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

#include "de/directoryfeed.h"
#include "de/folder.h"
#include "de/nativefile.h"
#include "de/string.h"
#include "de/fs.h"
#include "de/date.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <cstring>

#ifdef UNIX
#   include <dirent.h>
#   include <unistd.h>
#endif

#ifdef WIN32
#   include <io.h>
#   include <direct.h>
#endif

using namespace de;

DirectoryFeed::DirectoryFeed(const std::string& nativePath) : nativePath_(nativePath) {}

DirectoryFeed::~DirectoryFeed()
{}

void DirectoryFeed::populate(Folder& folder)
{
#ifdef UNIX
    DIR* dir = opendir(nativePath_.empty()? "." : nativePath_.c_str());
    if(!dir)
    {
        throw NotFoundError("DirectoryFeed::populate", "Path '" + nativePath_ + "' not found");
    }
    File* f;
    struct dirent* entry;
    while((entry = readdir(dir)) != NULL)   
    {
        const std::string entryName = entry->d_name;
        switch(entry->d_type)
        {
        case DT_DIR:
            populateSubFolder(folder, entryName);
            break;
            
        default:
            populateFile(folder, entryName);
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
        do
        {
            const std::string entryName = fd.name;
            if(fd.attrib & _A_SUBDIR)
            {
                populateSubFolder(folder, entryName);
            }
            else
            {
                populateFile(folder, entryName);
            }
        } 
        while(!_findnext(handle, &fd));
        _findclose(handle);
    }
#endif
}

void DirectoryFeed::populateSubFolder(Folder& folder, const std::string& entryName)
{
    if(entryName != "." && entryName != "..")
    {
        String subFeedPath = nativePath_.concatenateNativePath(entryName);
        Folder* subFolder = folder.locate<Folder>(entryName);
        if(!subFolder)
        {
            // This folder does not exist yet. Let's create it.
            subFolder = new Folder(entryName);
            folder.add(subFolder);
            folder.fileSystem().index(*subFolder);
        }
        else
        {        
            // The folder already exists. It may also already be fed by a DirectoryFeed.
            for(Folder::Feeds::const_iterator i = subFolder->feeds().begin();
                i != subFolder->feeds().end(); ++i)
            {
                const DirectoryFeed* dirFeed = dynamic_cast<const DirectoryFeed*>(*i);
                if(dirFeed && dirFeed->nativePath_ == subFeedPath)
                {
                    // Already got this fed. Nothing else needs done.
                    std::cout << "Feed for " << subFeedPath << " already there.\n";
                    return;
                }
            }
        }

        // Add a new feed.
        DirectoryFeed* feed = new DirectoryFeed(subFeedPath);
        subFolder->attach(feed);
    }
}

void DirectoryFeed::populateFile(Folder& folder, const std::string& entryName)
{
    if(folder.has(entryName))
    {
        // Already has an entry for this, skip it (wasn't pruned so it's OK).
        return;
    }
    
    String entryPath = nativePath_.concatenateNativePath(entryName);

    // Protect against errors.
    std::auto_ptr<NativeFile> nativeFile(new NativeFile(entryName, entryPath));

    File* file = folder.fileSystem().interpret(nativeFile.get());
    file->setStatus(fileStatus(entryPath));

    // We will decide on pruning this.
    file->setOriginFeed(this);
    
    folder.add(file);
        
    // Include files the main index.
    folder.fileSystem().index(*file);
    
    // We're in the clear.
    nativeFile.release();
}

bool DirectoryFeed::prune(File& file) const
{
    /// Rules for pruning:
    /// - A file sourced by NativeFile will be pruned if it's out of sync with the hard 
    ///   drive version (size, time of last modification).
    NativeFile* nativeFile = dynamic_cast<NativeFile*>(file.source());
    if(nativeFile)
    {
        try
        {
            if(fileStatus(nativeFile->nativePath()) != nativeFile->status())
            {
                // It's not up to date.
                std::cout << nativeFile->nativePath() << ": status has changed, pruning!\n";
                return true;
            }
        }
        catch(const StatusError&)
        {
            // Get rid of it.
            return true;
        }
    }
    
    /// - A Folder will be pruned if the corresponding directory does not exist (providing
    ///   a DirectoryFeed is the sole feed in the folder).
    Folder* subFolder = dynamic_cast<Folder*>(&file);
    if(subFolder)
    {
        if(subFolder->feeds().size() == 1)
        {
            DirectoryFeed* dirFeed = dynamic_cast<DirectoryFeed*>(subFolder->feeds().front());
            if(dirFeed && !exists(dirFeed->nativePath_))
            {
                std::cout << nativePath_ << " no longer there, pruning!\n";
                return true;
            }
        }
    }

    /// - Other types of Files will not be pruned.
    return false;
}

void DirectoryFeed::changeWorkingDir(const std::string& nativePath)
{
#ifdef UNIX
    if(chdir(nativePath.c_str()))
    {
        throw WorkingDirError("DirectoryFeed::changeWorkingDir",
            nativePath + ": " + strerror(errno));
    }
#endif

#ifdef WIN32
    if(_chdir(nativePath.c_str()))
    {
        throw WorkingDirError("DirectoryFeed::changeWorkingDir",
            nativePath + ": " + strerror(errno));
    }
#endif
}

bool DirectoryFeed::exists(const std::string& nativePath)
{
#ifdef UNIX
    struct stat s;
    return !stat(nativePath.c_str(), &s);
#endif

#ifdef WIN32
    return !_access_s(nativePath.c_str(), 0);
#endif
}

File::Status DirectoryFeed::fileStatus(const std::string& nativePath)
{
#ifdef UNIX
    // Get file status information.
    struct stat s;
    if(!stat(nativePath.c_str(), &s))
    {                                                    
        return File::Status(s.st_size, s.st_mtime);
    }
    throw StatusError("DirectoryFeed::fileStatus", nativePath + ": " + strerror(errno));
#endif

#ifdef WIN32
    struct _stat s;
    if(!_stat(nativePath.c_str(), &s))
    {
        return File::Status(s.st_size, s.st_mtime);
    }
    throw StatusError("DirectoryFeed::fileStatus", nativePath + ": " + strerror(errno));
#endif
}
