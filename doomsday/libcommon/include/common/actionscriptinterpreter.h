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

#ifndef LIBCOMMON_ACTIONSCRIPT_INTERPRETER_H
#define LIBCOMMON_ACTIONSCRIPT_INTERPRETER_H

#include <list>
#include <map>

#include <de/Thing>
#include <de/Error>

#include "common.h"

typedef de::dint FunctionName;

class ActionScriptThinker;
class GameMap;
class File;

struct Bytecode
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
    Bytecode() : base(NULL), _numStrings(0), _strings(NULL) {};

    ~Bytecode();

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
        throw UnknownFunctionNameError("ActionScriptInterpreter::Bytecode::function", "Invalid FunctionName");
    }

private:
    typedef std::map<FunctionName, Function> Functions;
    Functions _functions;

    de::dint _numStrings;
    de::dchar const** _strings;
};

/**
 * Interpreter for Hexen format ACS bytecode.
 */
class ActionScriptInterpreter
{
public:
    /// Only one instance of ActionScriptInterpreter is allowed. @ingroup errors
    DEFINE_ERROR(TooManyInstancesError);
    /// Invalid FunctionName specified. @ingroup errors
    DEFINE_ERROR(UnknownFunctionNameError);

    struct ScriptState {
        enum Status {
            INACTIVE,
            RUNNING,
            SUSPENDED,
            WAITING_FOR_TAG,
            WAITING_FOR_POLYOBJ,
            WAITING_FOR_SCRIPT,
            TERMINATING
        };
        Status status;

        de::dint waitValue;

        ScriptState(Status status=INACTIVE, de::dint waitValue = 0)
            : status(status), waitValue(waitValue) {};
    };

private:
    static const de::dint MAX_MAP_VARS = 32;
    static const de::dint MAX_WORLD_VARS = 64;

    static const de::dint PRINT_BUFFER_SIZE = 256;

    typedef std::map<FunctionName, ScriptState> ScriptStates;

    struct DeferredScriptEvent {
        /// Function name on the target map.
        FunctionName name;

        /// Target map.
        de::duint map;

        /// Arguments passed to the function (padded to 4 for alignment).
        de::dbyte args[4];

        DeferredScriptEvent(FunctionName name, de::duint map, de::dbyte arg1, de::dbyte arg2, de::dbyte arg3, de::dbyte arg4)
          : name(name), map(map) {
              args[0] = arg1;
              args[1] = arg2;
              args[2] = arg3;
              args[3] = arg4;
        }
    };

    typedef std::list<DeferredScriptEvent> DeferredScriptEvents;

public:
    ActionScriptInterpreter();
    ~ActionScriptInterpreter();

    ScriptState& scriptState(FunctionName name) {
        if(_scriptStates.find(name) == _scriptStates.end())
            /// @throw UnknownFunctionNameError An invalid FunctionName was specified
            /// when attempting to retrieve a ScriptState.
            throw UnknownFunctionNameError("ActionScriptInterpreter::scriptState", "Invalid FunctionName");

        return _scriptStates[name];
    }

    void load(const de::File& file);

    void writeWorldContext(de::Writer& to) const;

    void readWorldContext(de::Reader& from);

    void writeMapContext(de::Writer& to) const;

    void readMapContext(de::Reader& from);

    /**
     * Execute all deferred script events belonging to the specified map.
     */
    void runDeferredScriptEvents(de::duint map);

    bool start(FunctionName name, de::duint map, de::dbyte* args, de::Thing* activator, de::LineDef* lineDef, de::dint side);

    bool stop(FunctionName name);

    bool suspend(FunctionName name);

    /**
     * Resume any scripts waiting on the sector-tag-finished signal.
     */
    void tagFinished(de::dint tag);

    /**
     * Resume any scripts waiting on the polyobj-finished signal.
     */
    void polyobjFinished(de::dint po);

    /**
     * Resume any scripts waiting on the script-finished signal.
     */
    void scriptFinished(FunctionName name);

    void printScriptInfo(FunctionName name);

/// @todo Should be private.
    const Bytecode& bytecode() const {
        return _bytecode;
    }

    /// World context - A set of global variables that persist between maps.
    de::dint _worldContext[MAX_WORLD_VARS];

    /// Map context - A set of global variables the exist for the life of the current map.
    de::dint _mapContext[MAX_MAP_VARS];

    de::dchar _printBuffer[PRINT_BUFFER_SIZE];

public:
    /**
     * Returns the singleton ActionScriptInterpreter instance.
     */
    static ActionScriptInterpreter& actionScriptInterpreter();

private:
    /// Loaded bytecode for the current map.
    Bytecode _bytecode;

    /// Script state records. A record for each script in Bytecode::scriptInfo
    ScriptStates _scriptStates;

    /// Deferred script events.
    DeferredScriptEvents _deferredScriptEvents;

    bool startScript(FunctionName name, de::duint map, de::dbyte* args, de::Thing* activator,
        de::LineDef* lineDef, de::dint lineSide, ActionScriptThinker** newScript);

    bool deferScriptEvent(FunctionName name, de::duint map, de::dbyte* args);

    /**
     * (Re)start any scripts currently waiting on the specified signal.
     */
    void startWaitingScripts(ScriptState::Status status, de::dint waitValue);

    /// The singleton instance of the ActionScriptInterpreter.
    static ActionScriptInterpreter* _singleton;
};

#endif /* LIBCOMMON_ACTIONSCRIPT_INTERPRETER_H */
