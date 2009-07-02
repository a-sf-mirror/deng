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

#include "de/folder.h"

using namespace de;

Folder::Folder(const std::string& name)
    : File(name)
{}

Folder::~Folder()
{
    // Empty the contents.
    clear();
}

void Folder::clear()
{
    // Destroy all the file objects.
    for(Contents::iterator i = contents_.begin(); i != contents_.end(); ++i)
    {
        delete i->second;
    }
    contents_.clear();
}

File& Folder::add(File* file)
{
    assert(file != 0);
    if(contents_.find(file->name()) != contents_.end())
    {
        throw DuplicateNameError("Folder::add", "Folder cannot contain two files with the same name: '" +
            file->name() + "'");
    }
    contents_[file->name()] = file;
    file->setParent(this);
    return *file;
}

File* Folder::remove(File* file)
{
    for(Contents::iterator i = contents_.begin(); i != contents_.end(); ++i)
    {
        if(i->second == file)
        {
            contents_.erase(i);
            break;
        }
    }    
    file->setParent(0);
    return file;
}
