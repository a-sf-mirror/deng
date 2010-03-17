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

#include <de/String>
#include <de/File>
#include <de/IByteArray>

typedef de::dint FunctionName;

/**
 * Interpreter for Hexen format ACS bytecode.
 */
class ActionScriptBytecodeInterpreter
{
public:
    /// Invalid FunctionName specified. @ingroup errors
    DEFINE_ERROR(UnknownFunctionNameError);

    const de::IByteArray* base;

    struct Function {
        FunctionName name;
        de::dint numArguments;
        bool callOnMapStart;
        de::IByteArray::Offset entryPoint;

        Function(FunctionName name, de::dint numArguments, bool callOnMapStart,
                 const de::IByteArray::Offset entryPoint)
          : name(name),
            numArguments(numArguments),
            callOnMapStart(callOnMapStart),
            entryPoint(entryPoint)
        {};
    };

    typedef std::map<FunctionName, Function> Functions;

    typedef de::dint StringId;
    typedef std::vector<de::String> Strings;

public:
    ActionScriptBytecodeInterpreter() : base(NULL) {};

    ~ActionScriptBytecodeInterpreter();

    void load(const de::File& file);

    void unload();

    const de::String& string(StringId id) const {
        assert(id >= 0 && (unsigned) id < _strings.size());
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

    /**
     * Returns all currently loaded functions.
     */
    const Functions& functions() const {
        return _functions;
    }

private:
    /// Function table.
    Functions _functions;

    /// String table.
    Strings _strings;
};

#endif /* LIBCOMMON_ACTIONSCRIPT_BYTECODEINTERPRETER_H */
