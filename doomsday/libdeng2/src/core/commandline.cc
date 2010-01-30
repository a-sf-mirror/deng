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

#include "de/CommandLine"
#include "de/String"

#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

#ifdef UNIX
#   include <unistd.h>
#endif

#include <fstream>
#include <sstream>
#include <cctype>

using namespace de;

CommandLine::CommandLine(int argc, char** v)
{
    // The pointers list is kept null terminated.
    _pointers.push_back(0);

    for(int i = 0; i < argc; ++i)
    {
        if(v[i][0] == '@')
        {
            // This is a response file or something else that requires parsing.
            parse(v[i]);
        }
        else
        {
            _arguments.push_back(v[i]);
            _pointers.insert(_pointers.end() - 1, _arguments[i].c_str());
        }
    }
}

CommandLine::CommandLine(const CommandLine& other)
    : _arguments(other._arguments)
{
    // Use pointers to the already copied strings.
    FOR_EACH(i, _arguments, Arguments::iterator)
    {
        _pointers.push_back(i->c_str());
    }
    _pointers.push_back(0);
}

void CommandLine::clear()
{
    _arguments.clear();
    _pointers.clear();
    _pointers.push_back(0);
}

void CommandLine::append(const String& arg)
{
    _arguments.push_back(arg);
    _pointers.insert(_pointers.end() - 1, _arguments.rbegin()->c_str());
}

void CommandLine::insert(duint pos, const String& arg)
{
    if(pos > _arguments.size())
    {
        /// @throw OutOfRangeError @a pos is out of range.
        throw OutOfRangeError("CommandLine::insert", "Index out of range");
    }
    _arguments.insert(_arguments.begin() + pos, arg);
    _pointers.insert(_pointers.begin() + pos, _arguments[pos].c_str());
}

void CommandLine::remove(duint pos)
{
    if(pos >= _arguments.size())
    {
        /// @throw OutOfRangeError @a pos is out of range.
        throw OutOfRangeError("CommandLine::remove", "Index out of range");
    }
    _arguments.erase(_arguments.begin() + pos);
    _pointers.erase(_pointers.begin() + pos);
}

dint CommandLine::check(const String& arg, dint numParams) const
{
    // Do a search for arg.
    Arguments::const_iterator i = _arguments.begin();
    for(; i != _arguments.end() && !matches(arg, *i); ++i);
    
    if(i == _arguments.end())
    {
        // Not found.
        return 0;
    }
    
    // It was found, check for the number of non-option parameters.
    Arguments::const_iterator k = i;
    while(numParams-- > 0)
    {
        if(++k == _arguments.end() || isOption(*k))
        {
            // Ran out of arguments, or encountered an option.
            return 0;
        }
    }
    
    return i - _arguments.begin();
}

bool CommandLine::getParameter(const String& arg, String& param) const
{
    dint pos = check(arg, 1);
    if(pos > 0)
    {
        param = at(pos + 1);
        return true;
    }
    return false;
}

dint CommandLine::has(const String& arg) const
{
    dint howMany = 0;
    
    FOR_EACH(i, _arguments, Arguments::const_iterator)
    {
        if(matches(arg, *i))
        {
            howMany++;
        }
    }
    return howMany;
}

bool CommandLine::isOption(duint pos) const
{
    if(pos >= _arguments.size())
    {
        /// @throw OutOfRangeError @a pos is out of range.
        throw OutOfRangeError("CommandLine::isOption", "Index out of range");
    }
    assert(!_arguments[pos].empty());
    return isOption(_arguments[pos]);
}

bool CommandLine::isOption(const String& arg)
{
    return !(arg.empty() || arg[0] != '-');
}

const char* const* CommandLine::argv() const
{
    assert(*_pointers.rbegin() == 0);
    return &_pointers[0];
}

void CommandLine::parse(const String& cmdLine)
{
    String::const_iterator i = cmdLine.begin();

    // This is unset if we encounter a terminator token.
    bool isDone = false;

    // Are we currently inside quotes?
    bool quote = false;

    while(i != cmdLine.end() && !isDone)
    {
        // Skip initial whitespace.
        String::skipSpace(i, cmdLine.end());
        
        // Check for response files.
        bool isResponse = false;
        if(*i == '@')
        {
            isResponse = true;
            String::skipSpace(++i, cmdLine.end());
        }

        String word;

        while(i != cmdLine.end() && (quote || !std::isspace(*i)))
        {
            bool copyChar = true;
            if(!quote)
            {
                // We're not inside quotes.
                if(*i == '\"') // Quote begins.
                {
                    quote = true;
                    copyChar = false;
                }
            }
            else
            {
                // We're inside quotes.
                if(*i == '\"') // Quote ends.
                {
                    if(i + 1 != cmdLine.end() && *(i + 1) == '\"') // Doubles?
                    {
                        // Normal processing, but output only one quote.
                        i++;
                    }
                    else
                    {
                        quote = false;
                        copyChar = false;
                    }
                }
            }

            if(copyChar)
            {
                word.push_back(*i);
            }
            
            i++;
        }

        // Word has been extracted, examine it.
        if(isResponse) // Response file?
        {
            // This will quietly ignore missing response files.
            std::stringbuf response;
            std::ifstream(word.c_str()) >> &response;
            parse(response.str());
        }
        else if(word == "--") // End of arguments.
        {
            isDone = true;
        }
        else if(!word.empty()) // Make sure there *is* a word.
        {
            _arguments.push_back(word);
            _pointers.insert(_pointers.end() - 1, _arguments.rbegin()->c_str());
        }
    }
}

void CommandLine::alias(const String& full, const String& alias)
{
    _aliases[full].push_back(alias);
}

bool CommandLine::matches(const String& full, const String& fullOrAlias) const
{
    if(!full.compareWithoutCase(fullOrAlias))
    {
        // They are, in fact, the same.
        return true;
    }
    
    Aliases::const_iterator found = _aliases.find(full);
    if(found != _aliases.end())
    {
        FOR_EACH(i, found->second, Arguments::const_iterator)
        {
            if(i->compareWithoutCase(fullOrAlias))
            {
                // Found it among the aliases.
                return true;
            }
        }
    }
    
    return false;
}

void CommandLine::execute(char** envs) const
{
#ifdef UNIX
    // Fork and execute new file.
    pid_t result = fork();
    if(!result)
    {
        // Here we go in the child process.
        printf("Child loads %s...\n", _pointers[0]);
        execve(_pointers[0], const_cast<char* const*>(argv()), const_cast<char* const*>(envs));
    }
    else
    {
        if(result < 0)
        {
            perror("CommandLine::execute");
        }
    }
#endif    

#ifdef WIN32
    String quotedCmdLine;
    FOR_EACH(i, _arguments, Arguments::const_iterator)
    {
        quotedCmdLine += "\"" + *i + "\" ";
    }

    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    ZeroMemory(&processInfo, sizeof(processInfo));

    if(!CreateProcess(_pointers[0], const_cast<char*>(quotedCmdLine.c_str()), 
        NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo))
    {
        /// @throw ExecuteError The system call to start a new process failed.
        throw ExecuteError("CommandLine::execute", "Could not create process");
    }
#endif
}
