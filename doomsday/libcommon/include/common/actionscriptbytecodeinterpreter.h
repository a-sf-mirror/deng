/*
 * The Doomsday Engine Project
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
 * Copyright © 1999 Activision
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

#ifndef LIBCOMMON_ACTIONSCRIPT_BYTECODEINTERPRETER_H
#define LIBCOMMON_ACTIONSCRIPT_BYTECODEINTERPRETER_H

#include <map>

#include <de/deng.h>

#include <de/File>

typedef de::dint FunctionName;

/**
 * Interpreter for Hexen format ACS bytecode.
 */
class ActionScriptBytecodeInterpreter
{
public:
    /// Invalid FunctionName specified. @ingroup errors
    DEFINE_ERROR(UnknownFunctionNameError);

    typedef de::dint StringId;

    const de::dbyte* base;

    struct Function {
        const FunctionName name;
        const de::dint* entryPoint;
        const de::dint numArguments;

        Function(FunctionName name, const de::dint* entryPoint, de::dint numArguments)
          : name(name), entryPoint(entryPoint), numArguments(numArguments) {};
    };

public:
    ActionScriptBytecodeInterpreter() : base(NULL), _numStrings(0), _strings(NULL) {};

    ~ActionScriptBytecodeInterpreter();

    void load(const de::File& file);

    void unload();

    const de::dchar* string(StringId id) const {
        assert(id >= 0 && id < _numStrings);
        return _strings[id];
    }

    const Function& function(FunctionName name) const {
        Functions::const_iterator found = _functions.find(name);
        if(found != _functions.end())
            return found->second;
        /// @throw UnknownFunctionNameError Invalid name specified when
        /// attempting to lookup a Function.
        throw UnknownFunctionNameError("ActionScriptBytecodeInterpreter::function", "Invalid FunctionName");
    }

private:
    typedef std::map<FunctionName, Function> Functions;
    Functions _functions;

    de::dint _numStrings;
    de::dchar const** _strings;
};

#endif /* LIBCOMMON_ACTIONSCRIPT_BYTECODEINTERPRETER_H */
