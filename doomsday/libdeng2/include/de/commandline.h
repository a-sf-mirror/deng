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

#ifndef LIBDENG2_COMMANDLINE_H
#define LIBDENG2_COMMANDLINE_H

#include <vector>
#include <string>

#include <de/deng.h>
#include <de/Error>

namespace de
{
    /**
     * Stores and provides access to the command line arguments passed
     * to an application at launch.
     */
    class PUBLIC_API CommandLine
    {
    public:
        DEFINE_ERROR(OutOfRangeError);
        
    public:
        CommandLine(int argc, char** args);
        CommandLine(const CommandLine& other);        
        
#ifdef WIN32
        /**
         * Constructs a command line out of the arguments of the current process.
         */
        CommandLine();
#endif

        dint count() const { return arguments_.size(); }

        /**
         * Appends a new argument to the list of arguments.
         *
         * @param arg  Argument to append.
         */
        void append(const std::string& arg);

        /**
         * Inserts a new argument to the list of arguments at index @c pos.
         * 
         * @param pos  Index at which the new argument will be at.
         */
        void insert(duint pos, const std::string& arg);

        /**
         * Removes an argument by index.
         *
         * @param pos  Index of argument to remove.
         */
        void remove(duint pos);

        /**
         * Returns a list of pointers to the arguments. The list contains
         * count() strings.
         */
        const char* const* argv() const;
        
        /**
         * Spawns a new process using the command line. The first argument
         * specifies the file name of the executable. Returns immediately
         * after the process has been started.
         *
         * @param envs  Environment variables passed to the new process.
         */
        void execute(char** envs) const;
        
    private:
        typedef std::vector<std::string> Arguments;
        Arguments arguments_;
    
        typedef std::vector<const char*> ArgumentPointers;
        ArgumentPointers pointers_;
    };
}

#endif /* LIBDENG2_COMMANDLINE_H */
