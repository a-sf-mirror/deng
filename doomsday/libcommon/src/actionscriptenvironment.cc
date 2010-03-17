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
ActionScriptThinker* createActionScriptThinker(GameMap* map,
    FunctionName name, de::IByteArray::Offset bytecodePos, dint numArgs, const dbyte* args,
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

    _scriptStates.clear();
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

void ActionScriptEnvironment::load(const de::File& file)
{
    LOG_VERBOSE("Load ACS scripts.");

    memset(_mapContext, 0, sizeof(_mapContext));

    _bytecode.load(file);
    _scriptStates.clear();

    FOR_EACH(i, _bytecode.functions(), ActionScriptBytecodeInterpreter::Functions::const_iterator)
    {
        const ActionScriptBytecodeInterpreter::Function& func = i->second;

        if(func.callOnMapStart)
        {
            // World scripts are allotted 1 second for initialization.
            createActionScriptThinker(P_CurrentMap(), func.name,
                func.entryPoint, func.numArguments, NULL, NULL, NULL, 0, TICSPERSEC);
        }

        ScriptState::Status status = func.callOnMapStart? ScriptState::RUNNING : ScriptState::INACTIVE;
        _scriptStates[func.name] = ScriptState(status);
    }
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

    dint num = 20;
    if(ver >= 3)
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

            if(!(map < 0))
            {
                _deferredScriptEvents.push_back(
                    DeferredScriptEvent(name, (ver >= 3)? map : map-1, args[0], args[1], args[2], args[3]));
            }
        }
    }

    if(saveVersion < 7)
        from.seek(12);
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
