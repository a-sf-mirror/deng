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

#include "de/Folder"
#include "de/Feed"
#include "de/FS"

using namespace de;

Folder::Folder(const std::string& name)
    : File(name)
{}

Folder::~Folder()
{
    deindex();
    
    // Empty the contents.
    clear();
    
    // Destroy all feeds that remain.
    for(Feeds::reverse_iterator i = feeds_.rbegin(); i != feeds_.rend(); ++i)
    {
        delete *i;
    }
}

void Folder::clear()
{
    if(contents_.empty()) return;
    
    // Destroy all the file objects.
    for(Contents::iterator i = contents_.begin(); i != contents_.end(); ++i)
    {
        i->second->setParent(NULL);
        delete i->second;
    }
    contents_.clear();
}

bool Folder::has(const String& name) const
{
    return (contents_.find(name.lower()) != contents_.end());
}

File& Folder::add(File* file)
{
    assert(file != 0);
    if(has(file->name()))
    {
        throw DuplicateNameError("Folder::add", "Folder cannot contain two files with the same name: '" +
            file->name() + "'");
    }
    contents_[file->name().lower()] = file;
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

File* Folder::locateFile(const String& path) const
{
    if(path.empty())
    {
        return const_cast<Folder*>(this);
    }

    if(path[0] == '/')
    {
        // Route back to the root of the file system.
        return fileSystem().root().locateFile(path.substr(1));
    }

    // Extract the next component.
    String::size_type end = path.find('/');
    if(end == String::npos)
    {
        // No more slashes. What remains is the final component.
        Contents::const_iterator found = contents_.find(path.lower());
        if(found != contents_.end())
        {
            return found->second;
        }
        return NULL;
    }

    String component = path.substr(0, end);
    String remainder = path.substr(end + 1);

    // Check for some special cases.
    if(component == ".")
    {
        return locateFile(remainder);
    }
    if(component == "..")
    {
        if(!parent())
        {
            // Can't go there.
            return NULL;
        }
        return parent()->locateFile(remainder);
    }
    
    // Do we have a folder for this?
    Contents::const_iterator found = contents_.find(component.lower());
    if(found != contents_.end())
    {
        Folder* subFolder = dynamic_cast<Folder*>(found->second);
        if(subFolder)
        {
            // Continue recursively to the next component.
            return subFolder->locateFile(remainder);
        }
    }
    
    // Dead end.
    return NULL;
}

void Folder::attach(Feed* feed)
{
    feeds_.push_back(feed);
}

Feed* Folder::detach(Feed& feed)
{
    feeds_.remove(&feed);
    return &feed;
}

void Folder::populate()
{
    // Prune the existing files first.
    for(Contents::iterator i = contents_.begin(); i != contents_.end(); )
    {
        // By default we will prune if there are no feeds left attached to the folder.
        bool mustPrune = feeds_.empty();

        File* file = i->second;
        
        // If the file has a designated feed, ask it about pruning.
        if(file->originFeed() && file->originFeed()->prune(*file))
        {
            std::cout << "Pruning " << file->path() << "\n";
            mustPrune = true;
        }
        else if(!file->originFeed())
        {
            // There is no designated feed, ask all feeds of this folder.
            // If even one of the feeds thinks that the file is out of date,
            // it will be pruned.
            for(Feeds::iterator f = feeds_.begin(); f != feeds_.end(); ++f)
            {
                if((*f)->prune(*file))
                {
                    std::cout << "Pruning " << file->path() << "\n";
                    mustPrune = true;
                    break;
                }
            }
        }

        if(mustPrune)
        {
            // It needs to go.
            contents_.erase(i++);
            delete file;
        }
        else
        {
            ++i;
        }
    }
    
    // Populate with new/updated ones.
    for(Feeds::reverse_iterator i = feeds_.rbegin(); i != feeds_.rend(); ++i)
    {
        (*i)->populate(*this);
    }
    
    // Call populate on subfolders.
    for(Contents::iterator i = contents_.begin(); i != contents_.end(); ++i)
    {
        Folder* folder = dynamic_cast<Folder*>(i->second);
        if(folder)
        {
            folder->populate();
        }
    }
}
