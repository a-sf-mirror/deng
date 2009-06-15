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

namespace de
{
    /**
     * Stores and provides access to the command line arguments passed
     * to an application at launch.
     */
    class CommandLine
    {
    public:
        CommandLine(int argc, char** args);

        dint count() const { return arguments_.size(); }
        
        char** argv() const { return argv_; }
        
    private:
        typedef std::vector<std::string> Arguments;
        Arguments arguments_;
    
        char** argv_;
    };
}

#endif /* LIBDENG2_COMMANDLINE_H */
