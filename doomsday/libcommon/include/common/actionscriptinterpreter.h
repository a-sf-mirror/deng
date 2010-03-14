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

typedef de::dint ActionScriptId;

class ActionScriptThinker;
class GameMap;

/**
 * Interpreter for Hexen format ACS bytecode.
 */
class ActionScriptInterpreter
{
public:
    /// Only one instance of ActionScriptInterpreter is allowed. @ingroup errors
    DEFINE_ERROR(TooManyInstancesError);
    /// Invalid ActionScriptId specified. @ingroup errors
    DEFINE_ERROR(UnknownActionScriptIdError);

    struct ScriptStateRecord {
        enum State {
            INACTIVE,
            RUNNING,
            SUSPENDED,
            WAITING_FOR_TAG,
            WAITING_FOR_POLYOBJ,
            WAITING_FOR_SCRIPT,
            TERMINATING
        };
        State state;

        de::dint waitValue;

        ScriptStateRecord(State state=INACTIVE, de::dint waitValue = 0)
            : state(state), waitValue(waitValue) {};
    };

private:
    static const de::dint MAX_MAP_VARS = 32;
    static const de::dint MAX_WORLD_VARS = 64;

    static const de::dint PRINT_BUFFER_SIZE = 256;

    typedef std::map<ActionScriptId, ScriptStateRecord> ScriptStateRecords;

    struct Bytecode {
        typedef de::dint StringId;

        const de::dbyte* base;

        struct ScriptInfoRecord {
            const ActionScriptId scriptId;
            const de::dint* entryPoint;
            const de::dint argCount;

            ScriptInfoRecord(ActionScriptId scriptId, const de::dint* entryPoint, de::dint argCount)
              : scriptId(scriptId), entryPoint(entryPoint), argCount(argCount) {};
        };

        typedef std::vector<ScriptInfoRecord> ScriptInfoRecords;
        ScriptInfoRecords scriptInfoRecords;

        de::dint numStrings;
        de::dchar const** strings;

        const de::dchar* string(StringId id) const {
            assert(id >= 0 && id < numStrings);
            return strings[id];
        }

        de::duint toIndex(ActionScriptId scriptId) const {
            FOR_EACH(i, scriptInfoRecords, ScriptInfoRecords::const_iterator)
            {
                const ScriptInfoRecord& info = *i;
                if(info.scriptId == scriptId)
                    return de::dint(i - scriptInfoRecords.begin());
            }

            /// @throw UnknownActionScriptIdError Invalid ActionScriptId specified when
            /// attempting to retrieve a ScriptInfoRecord.
            throw UnknownActionScriptIdError("ActionScriptInterpreter::Bytecode::toIndex", "Invalid ActionScriptId");
        }
    };

    struct DeferredScriptEvent {
        /// Script number on target map.
        ActionScriptId scriptId;

        /// Target map.
        de::duint map;

        /// Arguments passed to script (padded to 4 for alignment).
        de::dbyte args[4];

        DeferredScriptEvent(ActionScriptId scriptId, de::duint map, de::dbyte arg1, de::dbyte arg2, de::dbyte arg3, de::dbyte arg4)
          : scriptId(scriptId), map(map) {
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

    ScriptStateRecord& scriptStateRecord(ActionScriptId scriptId) {
        if(_scriptStateRecords.find(scriptId) == _scriptStateRecords.end())
            /// @throw UnknownActionScriptIdError An invalid ActionScriptId was specified
            /// when attempting to retrieve a ScriptStateRecord.
            throw UnknownActionScriptIdError("ActionScriptInterpreter::scriptStateRecord", "Invalid ActionScriptId");

        return _scriptStateRecords[scriptId];
    }

    void load(de::duint map, lumpnum_t lumpNum);

    void writeWorldState(de::Writer& to) const;

    void readWorldState(de::Reader& from);

    void writeMapState(de::Writer& to) const;

    void readMapState(de::Reader& from);

    /**
     * Execute all deferred script events belonging to the specified map.
     */
    void runDeferredScriptEvents(de::duint map);

    bool start(ActionScriptId scriptId, de::duint map, de::dbyte* args, de::Thing* activator, de::LineDef* lineDef, de::dint side);

    bool stop(ActionScriptId scriptId);

    bool suspend(ActionScriptId scriptId);

    /**
     * Signal sector tag as finished.
     */
    void tagFinished(de::dint tag);

    /**
     * Signal polyobj as finished.
     */
    void polyobjFinished(de::dint po);

    /**
     * Signal script as finished.
     */
    void scriptFinished(ActionScriptId scriptId);

    void printScriptInfo(ActionScriptId scriptId);

/// @todo Should be private.
    const Bytecode& bytecode() const {
        return _bytecode;
    }

    de::dint _worldVars[MAX_WORLD_VARS];
    de::dint _mapVars[MAX_MAP_VARS];

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
    ScriptStateRecords _scriptStateRecords;

    /// Deferred script events.
    DeferredScriptEvents _deferredScriptEvents;

    void unloadBytecode();

    bool startScript(ActionScriptId scriptId, de::duint map, de::dbyte* args, de::Thing* activator,
        de::LineDef* lineDef, de::dint lineSide, ActionScriptThinker** newScript);

    bool deferScriptEvent(ActionScriptId scriptId, de::duint map, de::dbyte* args);

    /**
     * (Re)start any scripts currently waiting on the specified signal.
     */
    void startWaitingScripts(ScriptStateRecord::State state, de::dint waitValue);

    /// @todo Does not belong in this class.
    de::dint countThingsOfType(GameMap* map, de::dint type, de::dint tid);

    /// The singleton instance of the ActionScriptInterpreter.
    static ActionScriptInterpreter* _singleton;
};

#endif /* LIBCOMMON_ACTIONSCRIPT_INTERPRETER_H */
