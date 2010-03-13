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

#ifndef LIBCOMMON_ACTIONSCRIPTINTERPRETER_H
#define LIBCOMMON_ACTIONSCRIPTINTERPRETER_H

/**
 * Action Script Thinker.
 */
struct actionscript_thinker_t {
    static const dint AST_MAX_VARS = 10;
    static const dint AST_STACK_DEPTH = 32;

    thinker_t thinker;
    dint delayCount;
    actionscriptid_t scriptId;
    const dint* bytecodePos;
    dint infoIndex;
    dint stack[AST_STACK_DEPTH];
    dint stackDepth;
    dint vars[AST_MAX_VARS];
    Thing* activator;
    LineDef* lineDef;
    dint lineSide;

    void think();

    void write() const;

    dint read();
};

typedef dint actionscriptid_t;

/**
 * Interpreter for Hexen format ACS bytecode.
 */
class ActionScriptInterpreter
{
private:
    static const dint MAX_MAP_VARS = 32;
    static const dint MAX_WORLD_VARS = 64;

    static const dint PRINT_BUFFER_SIZE = 256;

    struct Bytecode {
        const dbyte* base;
        dint numScripts;
        struct script_info_s* scriptInfo;

        dint numStrings;
        char const** strings;
    };

    struct script_store_t {
        /// Target map.
        duint map;

        /// Script number on target map.
        actionscriptid_t scriptId;

        /// Arguments passed to script (padded to 4 for alignment).
        dbyte args[4];
    };

public:
    ActionScriptInterpreter();
    ~ActionScriptInterpreter();

    void load(duint map, lumpnum_t lumpNum);

    void writeWorldState() const;

    void readWorldState();

    void writeMapState() const;

    void readMapState();

    /**
     * Executes all deferred script start commands belonging to the specified map.
     */
    void startAll(duint map);

    bool start(actionscriptid_t scriptId, duint map, dbyte* args, Thing* activator, LineDef* lineDef, dint side);

    bool stop(actionscriptid_t scriptId, duint map);

    bool suspend(actionscriptid_t scriptId, duint map);

    void tagFinished(dint tag) const;

    void polyobjFinished(dint po) const;

    void printScriptInfo(actionscriptid_t scriptId);

private:
    /// Loaded bytecode for the current map.
    Bytecode _bytecode;

    /// List of deferred script start commands.
    dint _scriptStoreSize;
    script_store_t* _scriptStore;

    dint _worldVars[MAX_WORLD_VARS];
    dbyte _specArgs[8];
    dint _mapVars[MAX_MAP_VARS];

    dchar _printBuffer[PRINT_BUFFER_SIZE];

    void unloadBytecode();

    actionscript_thinker_t* createActionScriptThinker(GameMap* map,
        actionscriptid_t scriptId, const dint* bytecodePos, dint delayCount,
        dint infoIndex, Thing* activator, LineDef* lineDef, dint lineSide,
        const dbyte* args, dint numArgs);

    bool startScript(actionscriptid_t scriptId, duint map, dbyte* args, Thing* activator,
        LineDef* lineDef, dint lineSide, actionscript_thinker_t** newScript);

    void scriptFinished(actionscriptid_t scriptId);

    bool addToScriptStore(actionscriptid_t scriptId, duint map, dbyte* args);

    dint indexForScriptId(actionscriptid_t scriptId);

    bool tagBusy(dint tag);

    /// @todo Does not belong in this class.
    dint countThingsOfType(GameMap* map, dint type, dint tid);
};

#endif /* LIBCOMMON_ACTIONSCRIPTINTERPRETER_H */
