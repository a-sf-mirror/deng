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
#include "de/nativefile.h"
#include "de/library.h"

using namespace de;

LibraryFile::LibraryFile(File* source)
    : File(source->name()), source_(source), library_(0)
{
    assert(source_ != 0);
    
    std::cout << "LibraryFile: " << name() << ": " << source->path() << "\n";
}

LibraryFile::~LibraryFile()
{
    delete library_;
    delete source_;
}

Library& LibraryFile::library()
{
    assert(source_ != 0);
    
    if(library_)
    {
        return *library_;
    }
    
    // Currently we only load shared libraries directly from native files.
    // Other kinds of files would require a temporary native file.
    /// @todo A method for File for making a NativeFile out of any File.
    NativeFile* native = dynamic_cast<NativeFile*>(source_);
    if(!native)
    {
        throw UnsupportedSourceError("LibraryFile::library", source_->path() + 
            ": can only load from NativeFile");
    }
    
    library_ = new Library(native->nativePath());
    return *library_;
}

void LibraryFile::unload()
{
    if(library_)
    {
        delete library_;
        library_ = 0;
    }
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
