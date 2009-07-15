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

#ifndef LIBDENG2_APP_H
#define LIBDENG2_APP_H

#include <de/deng.h>
#include <de/Error>
#include <de/CommandLine>
#include <de/FS>

/**
 * @defgroup core Core
 *
 * These classes contain the core functionality of libdeng2.
 */

namespace de
{
    class Library;
    class LibraryFile;
    class Zone;
    
    /**
     * The application. This is an abstract class. Subclasses will need to define
     * the iterate() method that performs tasks while the main loop is running.
     *
     * @note This is a singleton class. Only one instance per process is allowed.
     *
     * @ingroup core
     */
    class PUBLIC_API App
    {
    public:
        /// Only one instance of App is allowed. @ingroup errors
        DEFINE_ERROR(TooManyInstancesError);

        /// The App instance has not been created but someone is trying to access it. 
        /// @ingroup errors
        DEFINE_ERROR(NoInstanceError);
        
        /// An attempt is made to access the game library while one is not loaded.
        /// @ingroup errors
        DEFINE_ERROR(NoGameError);

        /// There was a problem with SDL. Contains the SDL error message. @ingroup errors
        DEFINE_ERROR(SDLError);
        
    public:
        App(const CommandLine& commandLine);
        virtual ~App();

        /**
         * Returns the command line arguments specified at the start of the application.
         */
        const CommandLine& commandLine() const { return commandLine_; }

        /**
         * Returns the command line arguments specified at the start of the application.
         */
        CommandLine& commandLine() { return commandLine_; }

        /**
         * Loads the basic plugins (named "dengplugin_").
         */
        void loadPlugins();

        /**
         * Unloads the game plugin.
         */
        void unloadGame();
        
        /**
         * Unloads all loaded plugins.
         */
        void unloadPlugins();
        
        /**
         * Determines whether a game library is currently available.
         */
        bool hasGame() const { return gameLib_ != 0; }
        
        /**
         * Returns the amount of time since the creation of the App.
         */
        Time::Delta uptime() const {
            return initializedAt_.since();
        }
                
        /**
         * Main loop of the application.
         *
         * @return  Application exit code.
         *
         * @see iterate(), setExitCode()
         */
        virtual dint mainLoop();
        
        /**
         * Called on every iteration of the main loop.
         */
        virtual void iterate() = 0;
        
        /** 
         * Determines whether the main loop should be running.
         *
         * @return @c true if main loop running.
         */
        bool runningMainLoop() const { return runMainLoop_; }
        
        /**
         * Sets the code to be returned from the deng_Main() function to the
         * operating system upon quitting.
         *
         * @param exitCode  Exit code.
         */
        virtual void setExitCode(dint exitCode) { exitCode_ = exitCode; }

        /**
         * Returns the current exit code.
         *
         * @return  Exit code for operating system.
         *
         * @see setExitCode()
         */
        dint exitCode() const { return exitCode_; }
        
        /**
         * Stops the main loop.
         */
        virtual void stop();
        
    public:
        /**
         * Returns the singleton App instance. With this the App can be accessed
         * anywhere.
         */
        static App& app();

        /** 
         * Returns the file system.
         */
        static FS& fileSystem();

        /**
         * Returns the game library.
         */
        static Library& game();
        
        /**
         * Returns the memory zone.
         */
        static Zone& memory();

    private:
        CommandLine commandLine_;
        
        /// Time when the App was initialized.
        Time initializedAt_;
        
        /// The memory zone.
        Zone* memory_;
        
        /// The file system.
        FS* fs_;

        /// The game library.
        LibraryFile* gameLib_;
        
        /// @c true while the main loop is running.
        bool runMainLoop_;
        
        /// Exit code passed to the operating system when quitting.
        dint exitCode_;
        
        static App* singleton_;
    };
};

#endif /* LIBDENG2_APP_H */
