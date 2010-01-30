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

#include "de/DirectoryFeed"
#include "de/Folder"
#include "de/NativeFile"
#include "de/String"
#include "de/FS"
#include "de/Date"
#include "de/Log"

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

DirectoryFeed::DirectoryFeed(const String& nativePath, const Mode& mode) 
    : _nativePath(nativePath), _mode(mode) {}

DirectoryFeed::~DirectoryFeed()
{}

void DirectoryFeed::populate(Folder& folder)
{
    if(_mode[ALLOW_WRITE_BIT])
    {
        folder.setMode(File::WRITE);
    }
    if(_mode[CREATE_IF_MISSING_BIT] && !exists(_nativePath))
    {
        createDir(_nativePath);
    }
    
#ifdef UNIX
    DIR* dir = opendir(_nativePath.empty()? "." : _nativePath.c_str());
    if(!dir)
    {
        /// @throw NotFoundError The native directory was not accessible.
        throw NotFoundError("DirectoryFeed::populate", "Path '" + _nativePath + "' not found");
    }
    struct dirent* entry;
    while((entry = readdir(dir)) != 0)   
    {
        const String entryName = entry->d_name;
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
    if((handle = _findfirst(_nativePath.concatenateNativePath("*").c_str(), &fd)) != -1L)
    {
        do
        {
            const String entryName = fd.name;
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

void DirectoryFeed::populateSubFolder(Folder& folder, const String& entryName)
{
    LOG_AS("DirectoryFeed::populateSubFolder");

    if(entryName != "." && entryName != "..")
    {
        String subFeedPath = _nativePath.concatenateNativePath(entryName);
        Folder& subFolder = folder.fileSystem().getFolder(folder.path() / entryName);

        if(_mode[ALLOW_WRITE_BIT])
        {
            subFolder.setMode(File::WRITE);
        }

        // It may already be fed by a DirectoryFeed.
        for(Folder::Feeds::const_iterator i = subFolder.feeds().begin();
            i != subFolder.feeds().end(); ++i)
        {
            const DirectoryFeed* dirFeed = dynamic_cast<const DirectoryFeed*>(*i);
            if(dirFeed && dirFeed->_nativePath == subFeedPath)
            {
                // Already got this fed. Nothing else needs done.
                LOG_DEBUG("Feed for ") << subFeedPath << " already there.";
                return;
            }
        }

        // Add a new feed. Mode inherited.
        subFolder.attach(new DirectoryFeed(subFeedPath, _mode));
    }
}

void DirectoryFeed::populateFile(Folder& folder, const String& entryName)
{
    if(folder.has(entryName))
    {
        // Already has an entry for this, skip it (wasn't pruned so it's OK).
        return;
    }
    
    String entryPath = _nativePath.concatenateNativePath(entryName);

    // Open the native file.
    std::auto_ptr<NativeFile> nativeFile(new NativeFile(entryName, entryPath));
    nativeFile->setStatus(fileStatus(entryPath));
    if(_mode[ALLOW_WRITE_BIT])
    {
        nativeFile->setMode(File::WRITE);
    }

    File* file = folder.fileSystem().interpret(nativeFile.release());
    folder.add(file);

    // We will decide on pruning this.
    file->setOriginFeed(this);
        
    // Include files the main index.
    folder.fileSystem().index(*file);
}

bool DirectoryFeed::prune(File& file) const
{
    LOG_AS("DirectoryFeed::prune");
    
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
                LOG_VERBOSE("%s: status has changed, pruning!") << nativeFile->nativePath();
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
            if(dirFeed && !exists(dirFeed->_nativePath))
            {
                LOG_VERBOSE("%s: no longer exists, pruning!") << _nativePath;
                return true;
            }
        }
    }

    /// - Other types of Files will not be pruned.
    return false;
}

File* DirectoryFeed::newFile(const String& name)
{
    String newPath = _nativePath.concatenateNativePath(name);
    if(exists(newPath))
    {
        /// @throw AlreadyExistsError  The file @a name already exists in the native directory.
        throw AlreadyExistsError("DirectoryFeed::newFile", name + ": already exists");
    }
    File* file = new NativeFile(name, newPath);
    file->setOriginFeed(this);
    return file;
}

void DirectoryFeed::removeFile(const String& name)
{
    String path = _nativePath.concatenateNativePath(name);
    if(!exists(path))
    {
        /// @throw NotFoundError  The file @a name does not exist in the native directory.
        throw NotFoundError("DirectoryFeed::removeFile", name + ": not found");
    }
    
#ifdef UNIX
    if(unlink(path.c_str()))
    {
        /// @throw RemoveError  Failed to remove the native file.
        throw RemoveError("DirectoryFeed::removeFile", name + ": " + strerror(errno));
    }
#endif

#ifdef WIN32
    if(_unlink(path.c_str()))
    {
        throw RemoveError("DirectoryFeed::removeFile", name + ": " + strerror(errno));
    }
#endif
}

void DirectoryFeed::changeWorkingDir(const String& nativePath)
{
#ifdef UNIX
    if(chdir(nativePath.c_str()))
    {
        /// @throw WorkingDirError Changing to @a nativePath failed.
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

void DirectoryFeed::createDir(const String& nativePath)
{
    String parentPath = nativePath.fileNameNativePath();
    if(!parentPath.empty() && !exists(parentPath))
    {
        createDir(parentPath);
    }
    
#ifdef UNIX
    if(mkdir(nativePath.c_str(), 0755))
    {
        /// @throw CreateDirError Failed to create directory @a nativePath.
        throw CreateDirError("DirectoryFeed::createDir", nativePath + ": " + strerror(errno));
    }
#endif

#ifdef WIN32
    if(_mkdir(nativePath.c_str()))  
    {
        throw CreateDirError("DirectoryFeed::createDir", nativePath + ": " + strerror(errno));
    }
#endif    
}

bool DirectoryFeed::exists(const String& nativePath)
{
#ifdef UNIX
    struct stat s;
    return !stat(nativePath.c_str(), &s);
#endif

#ifdef WIN32
    return !_access_s(nativePath.c_str(), 0);
#endif
}

File::Status DirectoryFeed::fileStatus(const String& nativePath)
{
#ifdef UNIX
    // Get file status information.
    struct stat s;
    if(!stat(nativePath.c_str(), &s))
    {                                                    
        return File::Status(s.st_size, s.st_mtime);
    }
    /// @throw StatusError Determining the file status was not possible.
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
