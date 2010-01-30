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

#include <string>
#include <vector>
#include <map>

#include "../deng.h"
#include "../String"

namespace de
{
    /**
     * Stores and provides access to the command line arguments passed
     * to an application at launch.
     *
     * @ingroup core
     */
    class LIBDENG2_API CommandLine
    {
    public:
        /// Tried to access an argument that does not exist. @ingroup errors
        DEFINE_ERROR(OutOfRangeError);

        /// Execution of the command line failed. @ingroup errors
        DEFINE_ERROR(ExecuteError);
        
    public:
        /**
         * Constructs a CommandLine out of the provided strings. It is assumed
         * that @a argc and @a args are the ones passed from the system to the main() 
         * function. The strings that begin with a @@ character are parsed, the
         * rest are used without modification.
         *
         * @param argc  Number of argument strings.
         * @param args  The argument strings.
         */
        CommandLine(int argc, char** args);

        CommandLine(const CommandLine& other);        

        /**
         * Returns the number of arguments. This includes the program name, which
         * is the first argument in the list.
         */
        dint count() const { return _arguments.size(); }

        void clear();

        /**
         * Appends a new argument to the list of arguments.
         *
         * @param arg  Argument to append.
         */
        void append(const String& arg);

        /**
         * Inserts a new argument to the list of arguments at index @a pos.
         * 
         * @param pos  Index at which the new argument will be at.
         * @param arg  Argument to insert.
         */
        void insert(duint pos, const String& arg);

        /**
         * Removes an argument by index.
         *
         * @param pos  Index of argument to remove.
         */
        void remove(duint pos);

        /**
         * Checks whether @a arg is in the arguments. Since the first argument is
         * the program name, it is not included in the search.
         *
         * @param arg  Argument to look for. Don't use aliases here.
         * @param count  Number of parameters (non-option arguments) that follow 
         *      the located argument.* @see isOption()
         *
         * @return  Index of the argument, if found. Otherwise zero.
         */
        dint check(const String& arg, dint count = 0) const;

        /**
         * Gets the parameter for an argument.
         *
         * @param arg  Argument to look for. Don't use aliases here.
         * @param param  The parameter is returned here, if found.
         *
         * @return  @c true, if parameter was successfully returned.
         *      Otherwise @c false, in which case @a param is not modified.
         */
        bool getParameter(const String& arg, String& param) const;

        /**
         * Determines whether @a arg exists in the list of arguments.
         *
         * @param arg  Argument to look for. Don't use aliases here.
         *
         * @return  Number of times @a arg is found in the arguments.
         */
        dint has(const String& arg) const;

        /**
         * Determines whether an argument is an option, i.e., it begins with a hyphen.
         */
        bool isOption(duint pos) const;

        /**
         * Determines whether an argument is an option, i.e., it begins with a hyphen.
         */
        static bool isOption(const String& arg);

        const String& at(duint pos) const { return _arguments.at(pos); }

        /**
         * Returns a list of pointers to the arguments. The list contains
         * count() strings and is NULL-terminated.
         */
        const char* const* argv() const;
        
        /**
         * Breaks down a single string containing the arguments.
         *
         * Examples of behavior:
         * - -cmd "echo ""this is a command""" => [-cmd] [echo "this is a command"]
         * - Hello" My"Friend => [Hello MyFriend]
         * - @@test.rsp [reads contents of test.rsp]
         * - @@\"Program Files"\\test.rsp [reads contents of "\Program Files\test.rsp"]
         *
         * @param cmdLine  String containing the arguments.
         */ 
        void parse(const String& cmdLine);
        
        /**
         * Defines a new alias for a full argument.
         *
         * @param full  The full argument, e.g., "--help"
         * @param alias  Alias for the full argument, e.g., "-h"
         */
        void alias(const String& full, const String& alias);

        bool matches(const String& full, const String& fullOrAlias) const;
        
        /**
         * Spawns a new process using the command line. The first argument
         * specifies the file name of the executable. Returns immediately
         * after the process has been started.
         *
         * @param envs  Environment variables passed to the new process.
         */
        void execute(char** envs) const;
        
    private:
        typedef std::vector<String> Arguments;
        Arguments _arguments;
    
        typedef std::vector<const char*> ArgumentPointers;
        ArgumentPointers _pointers;
        
        typedef std::map<String, Arguments> Aliases;
        Aliases _aliases;
    };
}

#endif /* LIBDENG2_COMMANDLINE_H */
