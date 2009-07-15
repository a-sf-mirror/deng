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
#include "de/directoryfeed.h"
#include "de/zone.h"
#include "sdl.h"

using namespace de;

// This will be set when the app is constructed.
App* App::singleton_ = 0;

App::App(const CommandLine& commandLine)
    : commandLine_(commandLine), memory_(0), fs_(0), gameLib_(0), runMainLoop_(true), exitCode_(0)
{
    if(singleton_)
    {
        throw TooManyInstancesError("App::App", "Only one instance allowed");
    }
    
    singleton_ = this;

    // Start by initializing SDL.
    if(SDL_Init(0) == -1)
    {
        throw SDLError("App::App", SDL_GetError());
    }
    if(SDLNet_Init() == -1)
    {
        throw SDLError("App::App", SDLNet_GetError());
    }
    
    // The memory zone.
    memory_ = new Zone();

#ifdef MACOSX
    // When the application is started through Finder, we get a special command
    // line argument. The working directory needs to be changed.
    if(commandLine_.count() >= 2 && String(commandLine_.at(1)).beginsWith("-psn"))
    {
        DirectoryFeed::changeWorkingDir(String::fileNamePath(commandLine_.at(0)) + "/..");
    }
#endif
    
    // Now we can proceed with the members.
    fs_ = new FS();
    fs_->refresh();
    
    // Load the basic plugins.
    loadPlugins();
}

App::~App()
{
    // Deleting the file system will unload everything owned by the files, including 
    // all plugin libraries.
    delete fs_;
    
    delete memory_;
 
    // Shut down SDL.
    SDLNet_Quit();
    SDL_Quit();

    singleton_ = 0;
}

void App::loadPlugins()
{
    try
    {
        // Get the index of libraries.
        const FS::Index& index = fs_->indexFor(TYPE_NAME(LibraryFile));
    
        for(FS::Index::const_iterator i = index.begin(); i != index.end(); ++i)
        {
            LibraryFile& libFile = *static_cast<LibraryFile*>(i->second);
            if(libFile.name().contains("dengplugin_"))
            {
                // Initialize the plugin.
                libFile.library();
                std::cout << "App::loadPlugins() loaded " << libFile.path() << "\n";
            }
        }
    
        // Also load the specified game plugin, if there is one.
        std::string gameName = "doom";
        dint pos = commandLine_.check("-game", 1);
        if(pos)
        {
            gameName = commandLine_.at(pos + 1);
        }
    
        std::cout << "Looking for game '" << gameName << "'\n";
        
        for(FS::Index::const_iterator i = index.begin(); i != index.end(); ++i)
        {
            LibraryFile& libFile = *static_cast<LibraryFile*>(i->second);
            if(libFile.name().contains("_" + gameName + "."))
            {
                // This is the one.
                gameLib_ = &libFile;
                std::cout << "App::loadPlugins() located the game " << libFile.path() << "\n";
                break;
            }
        }
    }
    catch(const FS::UnknownTypeError&)
    {}
}

void App::unloadGame()
{
    // Unload the game first.
    if(gameLib_)
    {
        gameLib_->unload();
        gameLib_ = 0;
    }
}

void App::unloadPlugins()
{
    unloadGame();
    
    // Get the index of libraries.
    const FS::Index& index = fs_->indexFor(TYPE_NAME(LibraryFile));
    
    for(FS::Index::const_iterator i = index.begin(); i != index.end(); ++i)
    {
        LibraryFile& libFile = *static_cast<LibraryFile*>(i->second);
        if(libFile.name().contains("dengplugin_"))
        {
            // Initialize the plugin.
            libFile.unload();
            std::cout << "App::unloadPlugins() unloaded " << libFile.path() << "\n";
        }
    }
}

dint App::mainLoop()
{
    // Now running the main loop.
    runMainLoop_ = true;
    
    while(runMainLoop_)
    {
        iterate();
    }
    
    return exitCode_;
}

void App::stop(dint code)
{
    runMainLoop_ = false;
    setExitCode(code);
}

App& App::app()
{
    if(!singleton_)
    {
        throw NoInstanceError("App::app", "App has not been constructed yet");
    }
    return *singleton_;
}

Zone& App::memory()
{
    App& self = app();
    assert(self.memory_ != 0);
    return *self.memory_;
}

FS& App::fileSystem() 
{ 
    App& self = app();
    assert(self.fs_ != 0);
    return *self.fs_; 
}

Library& App::game()
{
    App& self = app();
    if(!self.gameLib_)
    {
        throw NoGameError("App::game", "No game library located");
    }
    return self.gameLib_->library();
}
