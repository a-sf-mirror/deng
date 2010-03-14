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

#include <de/Writer>
#include <de/Reader>

#include "common/ActionScriptInterpreter"
#include "common/ActionScriptThinker"
#include "common/GameMap"

using namespace de;

namespace {
#define GAMETYPE_SINGLE         0
#define GAMETYPE_COOPERATIVE    1
#define GAMETYPE_DEATHMATCH     2

#define SIDEDEF_SECTION_TOP     0
#define SIDEDEF_SECTION_MIDDLE  1
#define SIDEDEF_SECTION_BOTTOM  2

bool isValidACSBytecode(lumpnum_t lumpNum, dint* numScripts, dint* numStrings)
{
    dsize lumpLength, infoOffset;
    dbyte buff[12];

    if(lumpNum == -1)
        return false;

    lumpLength = W_LumpLength(lumpNum);

    // Smaller than the bytecode header?
    if(lumpLength < 12)
        return false;

    // Smaller than the expected size inclusive of the start of script info table?
    W_ReadLumpSection(lumpNum, buff, 0, 12);
    infoOffset = (size_t) LONG(*((int32_t*) (buff + 4)));
    if(infoOffset > lumpLength)
        return false;

    // Smaller than the expected size inclusive of the whole script info table plus
    // the string offset table size value?
    W_ReadLumpSection(lumpNum, buff, infoOffset, 4);
    *numScripts = (int) LONG(*((int32_t*) (buff)));
    if(infoOffset + 4 + *numScripts * 12 + 4 > lumpLength)
        return false;

    // Smaller than the expected size inclusive of the string offset table?
    W_ReadLumpSection(lumpNum, buff, infoOffset + 4 + *numScripts * 12, 4);
    *numStrings = (int) LONG(*((int32_t*) (buff)));
    if(infoOffset + 4 + *numScripts * 12 + 4 + *numStrings * 4 > lumpLength)
        return false;

    // Passed basic lump structure checks.
    return true;
}

ActionScriptThinker* createActionScriptThinker(GameMap* map,
    ActionScriptId scriptId, const dint* bytecodePos, dint delayCount,
    dint infoIndex, Thing* activator, LineDef* lineDef, dint lineSide,
    const dbyte* args, dint numArgs)
{
    ActionScriptThinker* script = new ActionScriptThinker(scriptId, bytecodePos,
        delayCount, infoIndex, activator, lineDef, lineSide, args, numArgs);
    map->add(script);
    return script;
}
}

// This will be set when the ActionScriptInterpreter is constructed.
ActionScriptInterpreter* ActionScriptInterpreter::_singleton = 0;

ActionScriptInterpreter::ActionScriptInterpreter()
{
    if(_singleton)
    {
        /// @throw TooManyInstancesError Attempted to construct a new instance of
        /// ActionScriptInterpreter while one already exists. There can only be one
        /// instance of ActionScriptInterpreter per process.
        throw TooManyInstancesError("ActionScriptInterpreter::ActionScriptInterpreter", "Only one instance allowed");
    }
    _singleton = this;
}

ActionScriptInterpreter::~ActionScriptInterpreter()
{
    unloadBytecode();

    _deferredScriptEvents.clear();

    _singleton = 0;
}

bool ActionScriptInterpreter::startScript(ActionScriptId scriptId, duint map,
    dbyte* args, Thing* activator, LineDef* lineDef, dint lineSide,
    ActionScriptThinker** newScript)
{
    if(newScript)
        *newScript = NULL;

    if(map && map-1 != gameMap)
    {   // Add to the script store.
        return deferScriptEvent(scriptId, map, args);
    }

    ScriptStateRecord& rec = scriptStateRecord(scriptId);
    switch(rec.state)
    {
    case ScriptStateRecord::SUSPENDED:
        // Resume suspended script.
        rec.state = ScriptStateRecord::RUNNING;
        return true;

    case ScriptStateRecord::INACTIVE:
        // Start an inactive script.
        {
        duint infoIndex = _bytecode.toIndex(scriptId);
        const Bytecode::ScriptInfoRecord& info = _bytecode.scriptInfoRecords[infoIndex];

        ActionScriptThinker* script = createActionScriptThinker(P_CurrentMap(), scriptId,
            info.entryPoint, 0, infoIndex, activator, lineDef,
            lineSide, args, info.argCount);

        if(newScript)
            *newScript = script;

        rec.state = ScriptStateRecord::RUNNING;
        return true;
        }
    default: // Script is already running.
        return false;
    }
}

void ActionScriptInterpreter::unloadBytecode()
{
    if(_bytecode.base)
        std::free(const_cast<dbyte*>(_bytecode.base)); _bytecode.base = NULL;

    _bytecode.scriptInfoRecords.clear();

    if(_bytecode.strings)
        std::free(_bytecode.strings); _bytecode.strings = NULL;
    _bytecode.numStrings = 0;
}

void ActionScriptInterpreter::load(duint map, lumpnum_t lumpNum)
{
#define OPEN_SCRIPTS_BASE       1000
 
    LOG_VERBOSE("Load ACS scripts.");

    if(_bytecode.base)
        unloadBytecode();

    memset(_mapVars, 0, sizeof(_mapVars));

    if(IS_CLIENT)
        return;

    dint numScripts, numStrings;
    if(!isValidACSBytecode(lumpNum, &numScripts, &numStrings))
    {
        LOG_WARNING("ActionScriptInterpreter::load: Warning, lump #") << lumpNum <<
            "is not valid ACS bytecode.";
        return;
    }
    if(numScripts == 0)
        return; // Empty lump.

    _bytecode.numStrings = numStrings;
    _bytecode.base = (const dbyte*) W_CacheLumpNum(lumpNum, PU_STATIC);

    // Load the script info.
    dsize infoOffset = (dsize) LONG(*((dint32*) (_bytecode.base + 4)));

    const dbyte* ptr = (_bytecode.base + infoOffset + 4);
    for(dint i = 0; i < numScripts; ++i, ptr += 12)
    {
        dint _scriptId = (dint) LONG(*((dint32*) (ptr)));
        dsize entryPointOffset = (dsize) LONG(*((dint32*) (ptr + 4)));
        dint argCount = (dint) LONG(*((dint32*) (ptr + 8)));
        const dint* entryPoint = (const dint*) (_bytecode.base + entryPointOffset);

        ActionScriptId scriptId;
        ScriptStateRecord::State state;
        if(_scriptId >= OPEN_SCRIPTS_BASE)
        {   // Auto-activate
            scriptId = static_cast<ActionScriptId>(_scriptId - OPEN_SCRIPTS_BASE);
            state = ScriptStateRecord::RUNNING;
        }
        else
        {
            scriptId = static_cast<ActionScriptId>(_scriptId);
            state = ScriptStateRecord::INACTIVE;
        }

        _bytecode.scriptInfoRecords.push_back(
            Bytecode::ScriptInfoRecord(scriptId, entryPoint, argCount));

        if(state == ScriptStateRecord::RUNNING)
        {
            // World scripts are allotted 1 second for initialization.
            createActionScriptThinker(P_CurrentMap(), scriptId,
                entryPoint, TICSPERSEC, i, NULL, NULL, 0, NULL, 0);
        }

        _scriptStateRecords[scriptId] = ScriptStateRecord(state);
    }

    // Load the string offsets.
    if(_bytecode.numStrings > 0)
    {
        _bytecode.strings = reinterpret_cast<dchar const**>(std::malloc(_bytecode.numStrings * sizeof(dchar*)));

        const dbyte* ptr = (_bytecode.base + infoOffset + 4 + numScripts * 12 + 4);
        for(dint i = 0; i < _bytecode.numStrings; ++i, ptr += 4)
        {
            _bytecode.strings[i] = (const dchar*) _bytecode.base + LONG(*((dint32*) (ptr)));
        }
    }

#undef OPEN_SCRIPTS_BASE
}

void ActionScriptInterpreter::writeWorldState(de::Writer& to) const
{
    to << dbyte(3); // Version byte.

    for(dint i = 0; i < MAX_WORLD_VARS; ++i)
        to << _worldVars[i];

    to << dint(_deferredScriptEvents.size());

    FOR_EACH(i, _deferredScriptEvents, DeferredScriptEvents::const_iterator)
    {
        const DeferredScriptEvent& ev = *i;

        to << ev.map
           << ev.scriptId;

        for(dint j = 0; j < 4; ++j)
            to << ev.args[j];
    }
}

void ActionScriptInterpreter::readWorldState(de::Reader& from)
{
    dbyte ver = 1;
    
    if(saveVersion >= 7)
        from >> ver;

    for(dint i = 0; i < MAX_WORLD_VARS; ++i)
        from >> _worldVars[i];

    _deferredScriptEvents.clear();

    if(ver >= 3)
    {
        dint num;
        from >> num;
        if(num)
        {
            for(dint i = 0; i < num; ++i)
            {
                duint map, scriptId;
                dbyte args[4];

                from >> map;
                from >> scriptId;
                for(dint j = 0; j < 4; ++j)
                    from >> args[j];

                _deferredScriptEvents.push_back(
                    DeferredScriptEvent(scriptId, map, args[0], args[1], args[2], args[3]));
            }
        }
    }
    else
    {   // Old format.
        DeferredScriptEvent tempStore[20];

        dint num = 0;
        for(dint i = 0; i < 20; ++i)
        {
            dint map, scriptId;
            from >> map;
            from >> scriptId;

            DeferredScriptEvent& ev = tempStore[map < 0? 19 : num++];
            ev.map = map < 0? 0 : map-1;
            ev.scriptId = scriptId;
            for(dint j = 0; j < 4; ++j)
                from >> ev.args[j];
        }

        if(saveVersion < 7)
            from.seek(12);

        for(dint i = 0; i < num; ++i)
        {
            _deferredScriptEvents.push_back(tempStore[i]);
        }
    }
}

void ActionScriptInterpreter::writeMapState(de::Writer& to) const
{
    FOR_EACH(i, _scriptStateRecords, ScriptStateRecords::const_iterator)
    {
        const ScriptStateRecord& rec = i->second;
        to << dshort(rec.state)
           << dshort(rec.waitValue);
    }

    for(dint i = 0; i < MAX_MAP_VARS; ++i)
        to << _mapVars[i];
}

void ActionScriptInterpreter::readMapState(de::Reader& from)
{
    FOR_EACH(i, _scriptStateRecords, ScriptStateRecords::iterator)
    {
        ScriptStateRecord& rec = i->second;
        dshort state, waitValue;

        from >> state
             >> waitValue;

        rec.state = static_cast<ScriptStateRecord::State>(state);
        rec.waitValue = dint(waitValue);
    }

    for(dint i = 0; i < MAX_MAP_VARS; ++i)
        from >> _mapVars[i];
}

void ActionScriptInterpreter::runDeferredScriptEvents(duint map)
{
    for(DeferredScriptEvents::iterator i = _deferredScriptEvents.begin(); i != _deferredScriptEvents.end(); )
    {
        DeferredScriptEvent& ev = *i;
        if(ev.map != map)
        {
            ++i;
            continue;
        }

        ActionScriptThinker* newScript;
        startScript(ev.scriptId, 0, ev.args, NULL, NULL, 0, &newScript);
        if(newScript)
        {
            newScript->delayCount = TICSPERSEC;
        }

        _deferredScriptEvents.erase(i++);
    }
}

bool ActionScriptInterpreter::start(ActionScriptId scriptId, duint map, dbyte* args,
    Thing* activator, LineDef* lineDef, dint side)
{
    try
    {
        return startScript(scriptId, map, args, activator, lineDef, side, NULL);
    }
    catch(const UnknownActionScriptIdError& err)
    {   /// Non-critical. Log and continue.
        LOG_WARNING(err.asText()) << " #" << scriptId;
        return false;
    }
}

bool ActionScriptInterpreter::deferScriptEvent(ActionScriptId scriptId, duint map,
    dbyte* args)
{
    // Don't allow duplicates.
    FOR_EACH(i, _deferredScriptEvents, DeferredScriptEvents::const_iterator)
    {
        const DeferredScriptEvent& ev = *i;
        if(ev.scriptId == scriptId && ev.map == map)
            return false;
    }

    _deferredScriptEvents.push_back(DeferredScriptEvent(map, scriptId, args[0], args[1], args[2], args[3]));
    return true;
}

bool ActionScriptInterpreter::stop(ActionScriptId scriptId)
{
    try
    {
        ScriptStateRecord& rec = scriptStateRecord(scriptId);
        // Is explicit termination allowed?
        switch(rec.state)
        {
        case ScriptStateRecord::INACTIVE:
        case ScriptStateRecord::TERMINATING:
            return false; // No.

        default:
            rec.state = ScriptStateRecord::TERMINATING;
            return true;
        };
    }
    catch(const UnknownActionScriptIdError& err)
    {   /// Non-critical. Log and continue.
        LOG_WARNING(err.asText()) << " #" << scriptId;
        return false;
    }
}

bool ActionScriptInterpreter::suspend(ActionScriptId scriptId)
{
    try
    {
        ScriptStateRecord& rec = scriptStateRecord(scriptId);
        // Is explicit suspension allowed?
        switch(rec.state)
        {
        case ScriptStateRecord::INACTIVE:
        case ScriptStateRecord::SUSPENDED:
        case ScriptStateRecord::TERMINATING:
            return false; // No.

        default:
            rec.state = ScriptStateRecord::SUSPENDED;
            return true;
        };
    }
    catch(const UnknownActionScriptIdError& err)
    {   /// Non-critical. Log and continue.
        LOG_WARNING(err.asText()) << " #" << scriptId;
        return false;
    }
}

void ActionScriptInterpreter::startWaitingScripts(ScriptStateRecord::State state, dint waitValue)
{
    FOR_EACH(i, _scriptStateRecords, ScriptStateRecords::iterator)
    {
        ScriptStateRecord& rec = i->second;
        if(rec.state == state && rec.waitValue == waitValue)
        {
            rec.state = ScriptStateRecord::RUNNING;
        }
    }
}

void ActionScriptInterpreter::tagFinished(dint tag)
{
    if(P_CurrentMap().isSectorTagBusy(tag))
        return;
    // Start any scripts currently waiting for this signal.
    startWaitingScripts(ScriptStateRecord::WAITING_FOR_TAG, tag);
}

void ActionScriptInterpreter::polyobjFinished(dint po)
{
    if(P_CurrentMap().isPolyobjBusy(po))
        return;
    // Start any scripts currently waiting for this signal.
    startWaitingScripts(ScriptStateRecord::WAITING_FOR_POLYOBJ, po);
}

void ActionScriptInterpreter::scriptFinished(ActionScriptId scriptId)
{
    // Start any scripts currently waiting for this signal.
    startWaitingScripts(ScriptStateRecord::WAITING_FOR_SCRIPT, dint(scriptId));
}

void ActionScriptInterpreter::printScriptInfo(ActionScriptId scriptId)
{
    static const char* scriptStateDescriptions[] = {
        "Inactive",
        "Running",
        "Suspended",
        "Waiting for tag",
        "Waiting for polyobj",
        "Waiting for script",
        "Terminating"
    };

    if(scriptId != -1)
    {
        try
        {
            const ScriptStateRecord& rec = scriptStateRecord(scriptId);
            LOG_MESSAGE("") << scriptId << " ("
                << scriptStateDescriptions[rec.state] << " w: " << rec.waitValue << ")";
        }
        catch(const UnknownActionScriptIdError& err)
        {   // Non-critical. Log and continue.
            LOG_WARNING(err.asText()) << " #" << scriptId;
        }
        return;
    }

    FOR_EACH(i, _scriptStateRecords, ScriptStateRecords::const_iterator)
    {
        const ScriptStateRecord& rec = i->second;
        LOG_MESSAGE("") << i->first << " ("
            << scriptStateDescriptions[rec.state] << " w: " << rec.waitValue << ")";
    }
}
