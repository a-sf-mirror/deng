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

#include "de/app.h"
#include "de/libraryfile.h"
#include "de/library.h"

using namespace de;

// This will be set when the app is constructed.
App* App::singleton_ = 0;

App::App(const CommandLine& commandLine)
    : commandLine_(commandLine)
{
    if(singleton_)
    {
        throw TooManyInstancesError("App::App", "Only one instance allowed");
    }
    
    singleton_ = this;
    
    // Now we can proceed with the members.
    fs_ = new FS();
    fs_->refresh();
    
    // Load the basic plugins.
    loadPlugins();
}

App::~App()
{
    delete fs_;
    
    singleton_ = 0;
}

FS& App::fileSystem() 
{ 
    assert(fs_ != 0);
    return *fs_; 
}

void App::loadPlugins()
{
    const FS::Index& index = fs_->indexFor(TYPE_NAME(LibraryFile));
    for(FS::Index::const_iterator i = index.begin(); i != index.end(); ++i)
    {
        if(i->second->name().contains("dengplugin_"))
        {
            // Initialize the plugin.
            static_cast<LibraryFile*>(i->second)->library();
            std::cout << "App::loadPlugins() loaded " << i->second->path() << "\n";
        }
    }
}

App& App::the()
{
    if(!singleton_)
    {
        throw NoInstanceError("App::the", "App has not been constructed yet");
    }
    return *singleton_;
}
