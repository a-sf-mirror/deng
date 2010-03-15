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

#include "common/ActionScriptInterpreter"
#include "common/ActionScriptThinker"
#include "common/ActionScriptStatement"
#include "common/GameMap"

using namespace de;

void ActionScriptThinker::think(const de::Time::Delta& /* elapsed */)
{
    ActionScriptInterpreter& asi = ActionScriptInterpreter::actionScriptInterpreter();
    ActionScriptInterpreter::ScriptState& state = asi.scriptState(name);

    if(state.status == ActionScriptInterpreter::ScriptState::TERMINATING)
    {
        state.status = ActionScriptInterpreter::ScriptState::INACTIVE;
        asi.scriptFinished(name);
        this->map()->destroy(this);
        return;
    }

    if(state.status != ActionScriptInterpreter::ScriptState::RUNNING)
    {
        return;
    }

    if(delayCount)
    {
        delayCount--;
        return;
    }

    ProcessAction action;
    do
    {
        Statement* statement = Statement::constructFrom(LONG(*bytecodePos++));
        action = statement->execute(asi, &process, this);
    } while(action == CONTINUE);

    if(action == TERMINATE)
    {
        state.status = ActionScriptInterpreter::ScriptState::INACTIVE;
        asi.scriptFinished(name);
        this->map()->destroy(this);
    }
}

void ActionScriptThinker::operator >> (de::Writer& to) const
{
    Thinker::operator >> (to);

    to << dbyte(2) // Write a version byte.
       << dint(SV_ThingArchiveNum(process._activator))
       << (process._lineDef ? dint(DMU_ToIndex(process._lineDef)) : dint(-1))
       << process._lineSide
       << name
       << delayCount;

    for(duint i = 0; i < STACK_DEPTH; ++i)
        to << process._stack[i];
    to << process._stackDepth;

    for(duint i = 0; i < MAX_VARS; ++i)
        to << process._context[i];

    ActionScriptInterpreter& asi = ActionScriptInterpreter::actionScriptInterpreter();
    to << (dint(bytecodePos) - dint(asi.bytecode().base));
}

void ActionScriptThinker::operator << (de::Reader& from)
{
    dbyte ver = 1;

    if(saveVersion >= 4)
    {   // @note the thinker class byte has already been read.
        from >> ver; // version byte.
    }
    else
    {
        // Its in the old pre V4 format which serialized ActionScriptThinker
        // Padding at the start (an old thinker_t struct)
        from.seek(16);
    }

    // Start of used data members.
    dint activator; from >> activator;
    process._activator = SV_GetArchiveThing(activator, &process._activator);

    dint lineDef; from >> lineDef;
    if(lineDef == -1)
        process._lineDef = NULL;
    else
        process._lineDef = DMU_ToPtr(DMU_LINEDEF, lineDef);

    from >> process._lineSide
         >> name;

    if(ver < 2)
        from.seek(4); // Was infoIndex (32bit signed integer).

    from >> delayCount;

    for(duint i = 0; i < STACK_DEPTH; ++i)
        from >> process._stack[i];
    from >> process._stackDepth;

    for(duint i = 0; i < MAX_VARS; ++i)
        from >> process._context[i];

    ActionScriptInterpreter& asi = ActionScriptInterpreter::actionScriptInterpreter();
    dint offset; from >> offset;
    bytecodePos = (const dint*) (asi.bytecode().base + offset);
}
