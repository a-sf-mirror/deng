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

#include "de/commandline.h"

#ifdef UNIX
#include <unistd.h>
#endif

using namespace de;

CommandLine::CommandLine(int argc, char** v)
{
    for(int i = 0; i < argc; ++i)
    {
        arguments_.push_back(v[i]);
        pointers_.push_back(arguments_[i].c_str());
    }
    // Keep this null terminated.
    pointers_.push_back(0);
}

CommandLine::CommandLine(const CommandLine& other)
    : arguments_(other.arguments_)
{
    // Use pointers to the already copied strings.
    for(Arguments::iterator i = arguments_.begin(); i != arguments_.end(); ++i)
    {
        pointers_.push_back(i->c_str());
    }
    pointers_.push_back(0);
}

void CommandLine::append(const std::string& arg)
{
    arguments_.push_back(arg);
    pointers_.insert(pointers_.end() - 1, arguments_.rbegin()->c_str());
}

void CommandLine::insert(duint pos, const std::string& arg)
{
    if(pos > arguments_.size())
    {
        throw OutOfRangeError("CommandLine::insert", "Index out of range.");
    }
    arguments_.insert(arguments_.begin() + pos, arg);
    pointers_.insert(pointers_.begin() + pos, arguments_[pos].c_str());
}

void CommandLine::remove(duint pos)
{
    if(pos >= arguments_.size())
    {
        throw OutOfRangeError("CommandLine::remove", "Index out of range.");
    }
    arguments_.erase(arguments_.begin() + pos);
    pointers_.erase(pointers_.begin() + pos);
}

const char* const* CommandLine::argv() const
{
    assert(*pointers_.rbegin() == 0);
    return &pointers_[0];
}

void CommandLine::execute(char** envs) const
{
#ifdef UNIX
    // Fork and execute new file.
    pid_t result = fork();
    if(!result)
    {
        // Here we go in the child process.
        printf("Child loads %s...\n", pointers_[0]);
        execve(pointers_[0], const_cast<char* const*>(argv()), const_cast<char* const*>(envs));
    }
    else
    {
        if(result < 0)
        {
            perror("CommandLine::execute");
        }
    }
#endif    
}
