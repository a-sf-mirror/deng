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

#include "common.h"

#include "common/GameMap"
#include "common/ActionScriptThinker"

typedef de::dint actionscriptid_t;

/**
 * Interpreter for Hexen format ACS bytecode.
 */
class ActionScriptInterpreter
{
private:
    static const de::dint MAX_MAP_VARS = 32;
    static const de::dint MAX_WORLD_VARS = 64;

    static const de::dint PRINT_BUFFER_SIZE = 256;

    struct Bytecode {
        const de::dbyte* base;
        de::dint numScripts;
        struct script_info_s* scriptInfo;

        de::dint numStrings;
        de::dchar const** strings;
    };

    struct script_store_t {
        /// Target map.
        de::duint map;

        /// Script number on target map.
        actionscriptid_t scriptId;

        /// Arguments passed to script (padded to 4 for alignment).
        de::dbyte args[4];
    };

public:
    ActionScriptInterpreter();
    ~ActionScriptInterpreter();

    void load(de::duint map, lumpnum_t lumpNum);

    void writeWorldState() const;

    void readWorldState();

    void writeMapState() const;

    void readMapState();

    /**
     * Executes all deferred script start commands belonging to the specified map.
     */
    void startAll(de::duint map);

    bool start(actionscriptid_t scriptId, de::duint map, de::dbyte* args, de::Thing* activator, de::LineDef* lineDef, de::dint side);

    bool stop(actionscriptid_t scriptId, de::duint map);

    bool suspend(actionscriptid_t scriptId, de::duint map);

    void tagFinished(de::dint tag) const;

    void polyobjFinished(de::dint po) const;

    void printScriptInfo(actionscriptid_t scriptId);

private:
    /// Loaded bytecode for the current map.
    Bytecode _bytecode;

    /// List of deferred script start commands.
    de::dint _scriptStoreSize;
    script_store_t* _scriptStore;

    de::dint _worldVars[MAX_WORLD_VARS];
    de::dbyte _specArgs[8];
    de::dint _mapVars[MAX_MAP_VARS];

    de::dchar _printBuffer[PRINT_BUFFER_SIZE];

    void unloadBytecode();

    ActionScriptThinker* createActionScriptThinker(GameMap* map,
        actionscriptid_t scriptId, const de::dint* bytecodePos, de::dint delayCount,
        de::dint infoIndex, de::Thing* activator, de::LineDef* lineDef, de::dint lineSide,
        const de::dbyte* args, de::dint numArgs);

    bool startScript(actionscriptid_t scriptId, de::duint map, de::dbyte* args, de::Thing* activator,
        de::LineDef* lineDef, de::dint lineSide, actionscript_thinker_t** newScript);

    void scriptFinished(actionscriptid_t scriptId);

    bool addToScriptStore(actionscriptid_t scriptId, de::duint map, de::dbyte* args);

    de::dint indexForScriptId(actionscriptid_t scriptId);

    bool tagBusy(de::dint tag);

    /// @todo Does not belong in this class.
    de::dint countThingsOfType(GameMap* map, de::dint type, de::dint tid);
};

#endif /* LIBCOMMON_ACTIONSCRIPT_INTERPRETER_H */
