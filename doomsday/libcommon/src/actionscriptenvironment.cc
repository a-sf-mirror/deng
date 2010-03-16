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

#include "common/ActionScriptEnvironment"
#include "common/ActionScriptThinker"
#include "common/GameMap"

using namespace de;

namespace {
bool recognize(const de::File& file, dint* numScripts, dint* numStrings)
{
    dsize infoOffset;
    dbyte buff[12];

    // Smaller than the bytecode header?
    if(file.size() < 12)
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
    FunctionName name, const dint* bytecodePos, dint numArgs, const dbyte* args,
    Thing* activator, LineDef* lineDef, dint lineSide, dint delayCount)
{
    ActionScriptThinker* script = new ActionScriptThinker(name, bytecodePos, numArgs, args,
        activator, lineDef, lineSide, delayCount);
    map->add(script);
    return script;
}
}

// This will be set when the ActionScriptEnvironment is constructed.
ActionScriptEnvironment* ActionScriptEnvironment::_singleton = 0;

ActionScriptEnvironment::ActionScriptEnvironment()
{
    if(_singleton)
    {
        /// @throw TooManyInstancesError Attempted to construct a new instance of
        /// ActionScriptEnvironment while one already exists. There can only be one
        /// instance of ActionScriptEnvironment per process.
        throw TooManyInstancesError("ActionScriptEnvironment::ActionScriptEnvironment", "Only one instance allowed");
    }
    _singleton = this;
}

ActionScriptEnvironment::~ActionScriptEnvironment()
{
    _bytecode.unload();

    _deferredScriptEvents.clear();

    _singleton = 0;
}

bool ActionScriptEnvironment::startScript(FunctionName name, duint map,
    dbyte* args, Thing* activator, LineDef* lineDef, dint lineSide,
    ActionScriptThinker** newScript)
{
    if(newScript)
        *newScript = NULL;

    if(map && map-1 != gameMap)
    {   // Add to the script store.
        return deferScriptEvent(name, map, args);
    }

    ScriptState& state = scriptState(name);
    switch(state.status)
    {
    case ScriptState::SUSPENDED:
        // Resume suspended script.
        state.status = ScriptState::RUNNING;
        return true;

    case ScriptState::INACTIVE:
        // Start an inactive script.
        {
        const ActionScriptBytecodeInterpreter::Function& func = _bytecode.function(name);

        ActionScriptThinker* script = createActionScriptThinker(P_CurrentMap(), name,
            func.entryPoint, func.numArguments, args, activator, lineDef, lineSide, 0);

        if(newScript)
            *newScript = script;

        state.status = ScriptState::RUNNING;
        return true;
        }
    default: // Script is already running.
        return false;
    }
}

ActionScriptBytecodeInterpreter::~ActionScriptBytecodeInterpreter()
{
    unload();
}

void ActionScriptBytecodeInterpreter::unload()
{
    if(base)
        std::free(const_cast<de::dbyte*>(base)); base = NULL;

    _functions.clear();

    if(_strings)
        std::free(_strings); _strings = NULL;
    _numStrings = 0;
}

void ActionScriptBytecodeInterpreter::load(const de::File& file)
{
#define OPEN_SCRIPTS_BASE       1000

    unload();

    dint numScripts, numStrings;
    if(!recognize(file, &numScripts, &numStrings))
    {
        LOG_WARNING("ActionScriptBytecodeInterpreter::load: Warning, file \"") << file.name() <<
            "\" is not valid ACS bytecode.";
        return;
    }
    if(numScripts == 0)
        return; // Empty lump.

    _numStrings = numStrings;
    base = (const dbyte*) W_CacheLumpNum(lumpNum, PU_STATIC);

    // Load the script info.
    dsize infoOffset = (dsize) LONG(*((dint32*) (base + 4)));

    const dbyte* ptr = (base + infoOffset + 4);
    for(dint i = 0; i < numScripts; ++i, ptr += 12)
    {
        dint _scriptId = (dint) LONG(*((dint32*) (ptr)));
        dsize entryPointOffset = (dsize) LONG(*((dint32*) (ptr + 4)));
        dint argCount = (dint) LONG(*((dint32*) (ptr + 8)));
        const dint* entryPoint = (const dint*) (base + entryPointOffset);

        FunctionName name;
        ActionScriptEnvironment::ScriptState::Status status;
        if(_scriptId >= OPEN_SCRIPTS_BASE)
        {   // Auto-activate
            name = static_cast<FunctionName>(_scriptId - OPEN_SCRIPTS_BASE);
            status = ActionScriptEnvironment::ScriptState::RUNNING;
        }
        else
        {
            name = static_cast<FunctionName>(_scriptId);
            status = ActionScriptEnvironment::ScriptState::INACTIVE;
        }

        _functions.insert(std::pair<FunctionName, Function>(name, Function(name, entryPoint, argCount)));

        if(status == ActionScriptEnvironment::ScriptState::RUNNING)
        {
            // World scripts are allotted 1 second for initialization.
            createActionScriptThinker(P_CurrentMap(), name,
                entryPoint, TICSPERSEC, i, NULL, NULL, 0, NULL, 0);
        }

        _scriptStates[name] = ActionScriptEnvironment::ScriptState(status);
    }

    // Load the string offsets.
    if(_numStrings > 0)
    {
        _strings = reinterpret_cast<dchar const**>(std::malloc(_numStrings * sizeof(dchar*)));

        const dbyte* ptr = (base + infoOffset + 4 + numScripts * 12 + 4);
        for(dint i = 0; i < _numStrings; ++i, ptr += 4)
        {
            _strings[i] = (const dchar*) base + LONG(*((dint32*) (ptr)));
        }
    }

#undef OPEN_SCRIPTS_BASE
}

void ActionScriptEnvironment::load(const de::File& file)
{
    LOG_VERBOSE("Load ACS scripts.");

    memset(_mapContext, 0, sizeof(_mapContext));

    _bytecode.load(file);
}

void ActionScriptEnvironment::writeWorldContext(de::Writer& to) const
{
    to << dbyte(3); // Version byte.

    for(dint i = 0; i < MAX_WORLD_VARS; ++i)
        to << _worldContext[i];

    // Although not actually considered as belonging to the World context, deferred
    // events are (de)serialized along with it.

    to << dint(_deferredScriptEvents.size());

    FOR_EACH(i, _deferredScriptEvents, DeferredScriptEvents::const_iterator)
    {
        const DeferredScriptEvent& ev = *i;

        to << ev.map
           << ev.name;

        for(dint j = 0; j < 4; ++j)
            to << ev.args[j];
    }
}

void ActionScriptEnvironment::readWorldContext(de::Reader& from)
{
    dbyte ver = 1;
    
    if(saveVersion >= 7)
        from >> ver;

    for(dint i = 0; i < MAX_WORLD_VARS; ++i)
        from >> _worldContext[i];

    // Although not actually considered as belonging to the World context, deferred
    // events are (de)serialized along with it.

    _deferredScriptEvents.clear();
    if(ver >= 3)
    {
        dint num;
        from >> num;
        if(num)
        {
            for(dint i = 0; i < num; ++i)
            {
                duint map, name;
                dbyte args[4];

                from >> map;
                from >> name;
                for(dint j = 0; j < 4; ++j)
                    from >> args[j];

                _deferredScriptEvents.push_back(
                    DeferredScriptEvent(name, map, args[0], args[1], args[2], args[3]));
            }
        }
    }
    else
    {   // Old format.
        DeferredScriptEvent tempStore[20];

        dint num = 0;
        for(dint i = 0; i < 20; ++i)
        {
            dint map, name;
            from >> map;
            from >> name;

            DeferredScriptEvent& ev = tempStore[map < 0? 19 : num++];
            ev.map = map < 0? 0 : map-1;
            ev.name = name;
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

void ActionScriptEnvironment::writeMapContext(de::Writer& to) const
{
    // Currently the state of running scripts is split. The internal state of a script
    // is owned by ActionScriptThinker and (de)serialized with it. The logical state
    // of all scripts (running or not) are (de)serialized along with the Map context.

    FOR_EACH(i, _scriptStates, ScriptStates::const_iterator)
    {
        const ScriptState& state = i->second;
        to << dshort(state.status)
           << dshort(state.waitValue);
    }

    for(dint i = 0; i < MAX_MAP_VARS; ++i)
        to << _mapContext[i];
}

void ActionScriptEnvironment::readMapContext(de::Reader& from)
{
    // Currently the state of running scripts is split. The internal state of a script
    // is owned by ActionScriptThinker and (de)serialized with it. The logical state
    // of all scripts (running or not) are (de)serialized along with the Map context.

    FOR_EACH(i, _scriptStates, ScriptStates::iterator)
    {
        ScriptState& state = i->second;
        dshort status, waitValue;

        from >> status
             >> waitValue;

        state.status = static_cast<ScriptState::Status>(status);
        state.waitValue = dint(waitValue);
    }

    for(dint i = 0; i < MAX_MAP_VARS; ++i)
        from >> _mapContext[i];
}

void ActionScriptEnvironment::runDeferredScriptEvents(duint map)
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
        startScript(ev.name, 0, ev.args, NULL, NULL, 0, &newScript);
        if(newScript)
        {
            newScript->delayCount = TICSPERSEC;
        }

        _deferredScriptEvents.erase(i++);
    }
}

bool ActionScriptEnvironment::start(FunctionName name, duint map, dbyte* args,
    Thing* activator, LineDef* lineDef, dint side)
{
    try
    {
        return startScript(name, map, args, activator, lineDef, side, NULL);
    }
    catch(const UnknownFunctionNameError& err)
    {   /// Non-critical. Log and continue.
        LOG_WARNING(err.asText()) << " #" << name;
        return false;
    }
}

bool ActionScriptEnvironment::deferScriptEvent(FunctionName name, duint map,
    dbyte* args)
{
    // Don't allow duplicates.
    FOR_EACH(i, _deferredScriptEvents, DeferredScriptEvents::const_iterator)
    {
        const DeferredScriptEvent& ev = *i;
        if(ev.name == name && ev.map == map)
            return false;
    }

    _deferredScriptEvents.push_back(DeferredScriptEvent(map, name, args[0], args[1], args[2], args[3]));
    return true;
}

bool ActionScriptEnvironment::stop(FunctionName name)
{
    try
    {
        ScriptState& state = scriptState(name);
        // Is explicit termination allowed?
        switch(state.status)
        {
        case ScriptState::INACTIVE:
        case ScriptState::TERMINATING:
            return false; // No.

        default:
            state.status = ScriptState::TERMINATING;
            return true;
        };
    }
    catch(const UnknownFunctionNameError& err)
    {   /// Non-critical. Log and continue.
        LOG_WARNING(err.asText()) << " #" << name;
        return false;
    }
}

bool ActionScriptEnvironment::suspend(FunctionName name)
{
    try
    {
        ScriptState& state = scriptState(name);
        // Is explicit suspension allowed?
        switch(state.status)
        {
        case ScriptState::INACTIVE:
        case ScriptState::SUSPENDED:
        case ScriptState::TERMINATING:
            return false; // No.

        default:
            state.status = ScriptState::SUSPENDED;
            return true;
        };
    }
    catch(const UnknownFunctionNameError& err)
    {   /// Non-critical. Log and continue.
        LOG_WARNING(err.asText()) << " #" << name;
        return false;
    }
}

void ActionScriptEnvironment::startWaitingScripts(ScriptState::Status status, dint waitValue)
{
    FOR_EACH(i, _scriptStates, ScriptStates::iterator)
    {
        ScriptState& state = i->second;
        if(state.status == status && state.waitValue == waitValue)
        {
            state.status = ScriptState::RUNNING;
        }
    }
}

void ActionScriptEnvironment::tagFinished(dint tag)
{
    startWaitingScripts(ScriptState::WAITING_FOR_TAG, tag);
}

void ActionScriptEnvironment::polyobjFinished(dint po)
{
    startWaitingScripts(ScriptState::WAITING_FOR_POLYOBJ, po);
}

void ActionScriptEnvironment::scriptFinished(FunctionName name)
{
    startWaitingScripts(ScriptState::WAITING_FOR_SCRIPT, dint(name));
}

void ActionScriptEnvironment::printScriptInfo(FunctionName name)
{
    static const char* scriptStatusDescriptions[] = {
        "Inactive",
        "Running",
        "Suspended",
        "Waiting for tag",
        "Waiting for polyobj",
        "Waiting for script",
        "Terminating"
    };

    if(name != -1)
    {
        try
        {
            const ScriptState& state = scriptState(name);
            LOG_MESSAGE("") << name << " ("
                << scriptStatusDescriptions[state.status] << " w: " << state.waitValue << ")";
        }
        catch(const UnknownFunctionNameError& err)
        {   // Non-critical. Log and continue.
            LOG_WARNING(err.asText()) << " #" << name;
        }
        return;
    }

    FOR_EACH(i, _scriptStates, ScriptStates::const_iterator)
    {
        const ScriptState& state = i->second;
        LOG_MESSAGE("") << i->first << " ("
            << scriptStatusDescriptions[state.status] << " w: " << state.waitValue << ")";
    }
}
