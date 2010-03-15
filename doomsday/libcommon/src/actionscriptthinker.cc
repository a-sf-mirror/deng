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

#include <de/App>

#include "common/ActionScriptInterpreter"
#include "common/ActionScriptThinker"
#include "common/GameMap"

using namespace de;

namespace {
enum ProcessAction {
    CONTINUE,
    STOP,
    TERMINATE
};

// A handy helper for declaring action script statement interpreter functions.
#define DEFINE_STATEMENT(x) ProcessAction SI##x(ActionScriptInterpreter& asi, ActionScriptThinker::Process* proc, ActionScriptThinker* script)

/// Pointer to an action script statement interpreter.
typedef ProcessAction (*statement_interpreter_t) (ActionScriptInterpreter&, ActionScriptThinker::Process*, ActionScriptThinker*);

DEFINE_STATEMENT(NOP);
DEFINE_STATEMENT(Terminate);
DEFINE_STATEMENT(Suspend);
DEFINE_STATEMENT(PushNumber);
DEFINE_STATEMENT(LSpec1);
DEFINE_STATEMENT(LSpec2);
DEFINE_STATEMENT(LSpec3);
DEFINE_STATEMENT(LSpec4);
DEFINE_STATEMENT(LSpec5);
DEFINE_STATEMENT(LSpec1Direct);
DEFINE_STATEMENT(LSpec2Direct);
DEFINE_STATEMENT(LSpec3Direct);
DEFINE_STATEMENT(LSpec4Direct);
DEFINE_STATEMENT(LSpec5Direct);
DEFINE_STATEMENT(Add);
DEFINE_STATEMENT(Subtract);
DEFINE_STATEMENT(Multiply);
DEFINE_STATEMENT(Divide);
DEFINE_STATEMENT(Modulus);
DEFINE_STATEMENT(EQ);
DEFINE_STATEMENT(NE);
DEFINE_STATEMENT(LT);
DEFINE_STATEMENT(GT);
DEFINE_STATEMENT(LE);
DEFINE_STATEMENT(GE);
DEFINE_STATEMENT(AssignScriptVar);
DEFINE_STATEMENT(AssignMapVar);
DEFINE_STATEMENT(AssignWorldVar);
DEFINE_STATEMENT(PushScriptVar);
DEFINE_STATEMENT(PushMapVar);
DEFINE_STATEMENT(PushWorldVar);
DEFINE_STATEMENT(AddScriptVar);
DEFINE_STATEMENT(AddMapVar);
DEFINE_STATEMENT(AddWorldVar);
DEFINE_STATEMENT(SubScriptVar);
DEFINE_STATEMENT(SubMapVar);
DEFINE_STATEMENT(SubWorldVar);
DEFINE_STATEMENT(MulScriptVar);
DEFINE_STATEMENT(MulMapVar);
DEFINE_STATEMENT(MulWorldVar);
DEFINE_STATEMENT(DivScriptVar);
DEFINE_STATEMENT(DivMapVar);
DEFINE_STATEMENT(DivWorldVar);
DEFINE_STATEMENT(ModScriptVar);
DEFINE_STATEMENT(ModMapVar);
DEFINE_STATEMENT(ModWorldVar);
DEFINE_STATEMENT(IncScriptVar);
DEFINE_STATEMENT(IncMapVar);
DEFINE_STATEMENT(IncWorldVar);
DEFINE_STATEMENT(DecScriptVar);
DEFINE_STATEMENT(DecMapVar);
DEFINE_STATEMENT(DecWorldVar);
DEFINE_STATEMENT(Goto);
DEFINE_STATEMENT(IfGoto);
DEFINE_STATEMENT(Drop);
DEFINE_STATEMENT(Delay);
DEFINE_STATEMENT(DelayDirect);
DEFINE_STATEMENT(Random);
DEFINE_STATEMENT(RandomDirect);
DEFINE_STATEMENT(ThingCount);
DEFINE_STATEMENT(ThingCountDirect);
DEFINE_STATEMENT(TagWait);
DEFINE_STATEMENT(TagWaitDirect);
DEFINE_STATEMENT(PolyobjWait);
DEFINE_STATEMENT(PolyobjWaitDirect);
DEFINE_STATEMENT(ChangeFloor);
DEFINE_STATEMENT(ChangeFloorDirect);
DEFINE_STATEMENT(ChangeCeiling);
DEFINE_STATEMENT(ChangeCeilingDirect);
DEFINE_STATEMENT(Restart);
DEFINE_STATEMENT(AndLogical);
DEFINE_STATEMENT(OrLogical);
DEFINE_STATEMENT(AndBitwise);
DEFINE_STATEMENT(OrBitwise);
DEFINE_STATEMENT(EorBitwise);
DEFINE_STATEMENT(NegateLogical);
DEFINE_STATEMENT(LShift);
DEFINE_STATEMENT(RShift);
DEFINE_STATEMENT(UnaryMinus);
DEFINE_STATEMENT(IfNotGoto);
DEFINE_STATEMENT(LineDefSide);
DEFINE_STATEMENT(ScriptWait);
DEFINE_STATEMENT(ScriptWaitDirect);
DEFINE_STATEMENT(ClearLineDefSpecial);
DEFINE_STATEMENT(CaseGoto);
DEFINE_STATEMENT(BeginPrint);
DEFINE_STATEMENT(EndPrint);
DEFINE_STATEMENT(PrintString);
DEFINE_STATEMENT(PrintNumber);
DEFINE_STATEMENT(PrintCharacter);
DEFINE_STATEMENT(PlayerCount);
DEFINE_STATEMENT(GameType);
DEFINE_STATEMENT(GameSkill);
DEFINE_STATEMENT(Timer);
DEFINE_STATEMENT(SectorSound);
DEFINE_STATEMENT(AmbientSound);
DEFINE_STATEMENT(SoundSequence);
DEFINE_STATEMENT(SetSideDefMaterial);
DEFINE_STATEMENT(SetLineDefBlocking);
DEFINE_STATEMENT(SetLineDefSpecial);
DEFINE_STATEMENT(ThingSound);
DEFINE_STATEMENT(EndPrintBold);

const statement_interpreter_t statementInterpreters[] =
{
    SINOP, SITerminate, SISuspend, SIPushNumber, SILSpec1, SILSpec2,
    SILSpec3, SILSpec4, SILSpec5, SILSpec1Direct, SILSpec2Direct,
    SILSpec3Direct, SILSpec4Direct, SILSpec5Direct, SIAdd,
    SISubtract, SIMultiply, SIDivide, SIModulus, SIEQ, SINE,
    SILT, SIGT, SILE, SIGE, SIAssignScriptVar, SIAssignMapVar,
    SIAssignWorldVar, SIPushScriptVar, SIPushMapVar,
    SIPushWorldVar, SIAddScriptVar, SIAddMapVar, SIAddWorldVar,
    SISubScriptVar, SISubMapVar, SISubWorldVar, SIMulScriptVar,
    SIMulMapVar, SIMulWorldVar, SIDivScriptVar, SIDivMapVar,
    SIDivWorldVar, SIModScriptVar, SIModMapVar, SIModWorldVar,
    SIIncScriptVar, SIIncMapVar, SIIncWorldVar, SIDecScriptVar,
    SIDecMapVar, SIDecWorldVar, SIGoto, SIIfGoto, SIDrop,
    SIDelay, SIDelayDirect, SIRandom, SIRandomDirect,
    SIThingCount, SIThingCountDirect, SITagWait, SITagWaitDirect,
    SIPolyobjWait, SIPolyobjWaitDirect, SIChangeFloor,
    SIChangeFloorDirect, SIChangeCeiling, SIChangeCeilingDirect,
    SIRestart, SIAndLogical, SIOrLogical, SIAndBitwise,
    SIOrBitwise, SIEorBitwise, SINegateLogical, SILShift,
    SIRShift, SIUnaryMinus, SIIfNotGoto, SILineDefSide, SIScriptWait,
    SIScriptWaitDirect, SIClearLineDefSpecial, SICaseGoto,
    SIBeginPrint, SIEndPrint, SIPrintString, SIPrintNumber,
    SIPrintCharacter, SIPlayerCount, SIGameType, SIGameSkill,
    SITimer, SISectorSound, SIAmbientSound, SISoundSequence,
    SISetSideDefMaterial, SISetLineDefBlocking, SISetLineDefSpecial,
    SIThingSound, SIEndPrintBold
};
}

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
        statement_interpreter_t statement = statementInterpreters[LONG(*bytecodePos++)];
        action = statement(asi, &process, this);
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

namespace {
DEFINE_STATEMENT(NOP)
{
    return CONTINUE;
}

DEFINE_STATEMENT(Terminate)
{
    return TERMINATE;
}

DEFINE_STATEMENT(Suspend)
{
    asi.scriptState(script->name).status = ActionScriptInterpreter::ScriptState::SUSPENDED;
    return STOP;
}

DEFINE_STATEMENT(PushNumber)
{
    proc->push(LONG(*script->bytecodePos++));
    return CONTINUE;
}

DEFINE_STATEMENT(LSpec1)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = proc->pop();
    P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
    return CONTINUE;
}

DEFINE_STATEMENT(LSpec2)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[1] = proc->pop();
    args[0] = proc->pop();
    P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
    return CONTINUE;
}

DEFINE_STATEMENT(LSpec3)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[2] = proc->pop();
    args[1] = proc->pop();
    args[0] = proc->pop();
    P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
    return CONTINUE;
}

DEFINE_STATEMENT(LSpec4)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[3] = proc->pop();
    args[2] = proc->pop();
    args[1] = proc->pop();
    args[0] = proc->pop();
    P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
    return CONTINUE;
}

DEFINE_STATEMENT(LSpec5)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[4] = proc->pop();
    args[3] = proc->pop();
    args[2] = proc->pop();
    args[1] = proc->pop();
    args[0] = proc->pop();
    P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
    return CONTINUE;
}

DEFINE_STATEMENT(LSpec1Direct)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
    return CONTINUE;
}

DEFINE_STATEMENT(LSpec2Direct)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = LONG(*script->bytecodePos++);
    args[1] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
    return CONTINUE;
}

DEFINE_STATEMENT(LSpec3Direct)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = LONG(*script->bytecodePos++);
    args[1] = LONG(*script->bytecodePos++);
    args[2] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
    return CONTINUE;
}

DEFINE_STATEMENT(LSpec4Direct)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = LONG(*script->bytecodePos++);
    args[1] = LONG(*script->bytecodePos++);
    args[2] = LONG(*script->bytecodePos++);
    args[3] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
    return CONTINUE;
}

DEFINE_STATEMENT(LSpec5Direct)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = LONG(*script->bytecodePos++);
    args[1] = LONG(*script->bytecodePos++);
    args[2] = LONG(*script->bytecodePos++);
    args[3] = LONG(*script->bytecodePos++);
    args[4] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
    return CONTINUE;
}

DEFINE_STATEMENT(Add)
{
    proc->push(proc->pop() + proc->pop());
    return CONTINUE;
}

DEFINE_STATEMENT(Subtract)
{
    dint operand2 = proc->pop();
    proc->push(proc->pop() - operand2);
    return CONTINUE;
}

DEFINE_STATEMENT(Multiply)
{
    proc->push(proc->pop() * proc->pop());
    return CONTINUE;
}

DEFINE_STATEMENT(Divide)
{
    dint operand2 = proc->pop();
    proc->push(proc->pop() / operand2);
    return CONTINUE;
}

DEFINE_STATEMENT(Modulus)
{
    dint operand2 = proc->pop();
    proc->push(proc->pop() % operand2);
    return CONTINUE;
}

DEFINE_STATEMENT(EQ)
{
    proc->push(proc->pop() == proc->pop());
    return CONTINUE;
}

DEFINE_STATEMENT(NE)
{
    proc->push(proc->pop() != proc->pop());
    return CONTINUE;
}

DEFINE_STATEMENT(LT)
{
    dint operand2 = proc->pop();
    proc->push(proc->pop() < operand2);
    return CONTINUE;
}

DEFINE_STATEMENT(GT)
{
    int operand2 = proc->pop();
    proc->push(proc->pop() > operand2);
    return CONTINUE;
}

DEFINE_STATEMENT(LE)
{
    dint operand2 = proc->pop();
    proc->push(proc->pop() <= operand2);
    return CONTINUE;
}

DEFINE_STATEMENT(GE)
{
    dint operand2 = proc->pop();
    proc->push(proc->pop() >= operand2);
    return CONTINUE;
}

DEFINE_STATEMENT(AssignScriptVar)
{
    proc->_context[LONG(*script->bytecodePos++)] = proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(AssignMapVar)
{
    asi._mapContext[LONG(*script->bytecodePos++)] = proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(AssignWorldVar)
{
    asi._worldContext[LONG(*script->bytecodePos++)] = proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(PushScriptVar)
{
    proc->push(proc->_context[LONG(*script->bytecodePos++)]);
    return CONTINUE;
}

DEFINE_STATEMENT(PushMapVar)
{
    proc->push(asi._mapContext[LONG(*script->bytecodePos++)]);
    return CONTINUE;
}

DEFINE_STATEMENT(PushWorldVar)
{
    proc->push(asi._worldContext[LONG(*script->bytecodePos++)]);
    return CONTINUE;
}

DEFINE_STATEMENT(AddScriptVar)
{
    proc->_context[LONG(*script->bytecodePos++)] += proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(AddMapVar)
{
    asi._mapContext[LONG(*script->bytecodePos++)] += proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(AddWorldVar)
{
    asi._worldContext[LONG(*script->bytecodePos++)] += proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(SubScriptVar)
{
    proc->_context[LONG(*script->bytecodePos++)] -= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(SubMapVar)
{
    asi._mapContext[LONG(*script->bytecodePos++)] -= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(SubWorldVar)
{
    asi._worldContext[LONG(*script->bytecodePos++)] -= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(MulScriptVar)
{
    proc->_context[LONG(*script->bytecodePos++)] *= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(MulMapVar)
{
    asi._mapContext[LONG(*script->bytecodePos++)] *= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(MulWorldVar)
{
    asi._worldContext[LONG(*script->bytecodePos++)] *= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(DivScriptVar)
{
    proc->_context[LONG(*script->bytecodePos++)] /= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(DivMapVar)
{
    asi._mapContext[LONG(*script->bytecodePos++)] /= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(DivWorldVar)
{
    asi._worldContext[LONG(*script->bytecodePos++)] /= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(ModScriptVar)
{
    proc->_context[LONG(*script->bytecodePos++)] %= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(ModMapVar)
{
    asi._mapContext[LONG(*script->bytecodePos++)] %= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(ModWorldVar)
{
    asi._worldContext[LONG(*script->bytecodePos++)] %= proc->pop();
    return CONTINUE;
}

DEFINE_STATEMENT(IncScriptVar)
{
    proc->_context[LONG(*script->bytecodePos++)]++;
    return CONTINUE;
}

DEFINE_STATEMENT(IncMapVar)
{
    asi._mapContext[LONG(*script->bytecodePos++)]++;
    return CONTINUE;
}

DEFINE_STATEMENT(IncWorldVar)
{
    asi._worldContext[LONG(*script->bytecodePos++)]++;
    return CONTINUE;
}

DEFINE_STATEMENT(DecScriptVar)
{
    proc->_context[LONG(*script->bytecodePos++)]--;
    return CONTINUE;
}

DEFINE_STATEMENT(DecMapVar)
{
    asi._mapContext[LONG(*script->bytecodePos++)]--;
    return CONTINUE;
}

DEFINE_STATEMENT(DecWorldVar)
{
    asi._worldContext[LONG(*script->bytecodePos++)]--;
    return CONTINUE;
}

DEFINE_STATEMENT(Goto)
{
    script->bytecodePos = (dint*) (asi.bytecode().base + LONG(*script->bytecodePos));
    return CONTINUE;
}

DEFINE_STATEMENT(IfGoto)
{
    if(proc->pop())
    {
        script->bytecodePos = (dint*) (asi.bytecode().base + LONG(*script->bytecodePos));
    }
    else
    {
        script->bytecodePos++;
    }
    return CONTINUE;
}

DEFINE_STATEMENT(Drop)
{
    proc->drop();
    return CONTINUE;
}

DEFINE_STATEMENT(Delay)
{
    script->delayCount = proc->pop();
    return STOP;
}

DEFINE_STATEMENT(DelayDirect)
{
    script->delayCount = LONG(*script->bytecodePos++);
    return STOP;
}

DEFINE_STATEMENT(Random)
{
    dint low, high;
    high = proc->pop();
    low = proc->pop();
    proc->push(low + (P_Random() % (high - low + 1)));
    return CONTINUE;
}

DEFINE_STATEMENT(RandomDirect)
{
    dint low, high;
    low = LONG(*script->bytecodePos++);
    high = LONG(*script->bytecodePos++);
    proc->push(low + (P_Random() % (high - low + 1)));
    return CONTINUE;
}

DEFINE_STATEMENT(ThingCount)
{
    dint tid;
    tid = proc->pop();
    proc->push(P_CurrentMap().countThingsOfType(proc->pop(), tid));
    return CONTINUE;
}

DEFINE_STATEMENT(ThingCountDirect)
{
    dint type;
    type = LONG(*script->bytecodePos++);
    proc->push(P_CurrentMap().countThingsOfType(type, LONG(*script->bytecodePos++)));
    return CONTINUE;
}

DEFINE_STATEMENT(TagWait)
{
    ActionScriptInterpreter::ScriptState& state = asi.scriptState(script->name);
    state.waitValue = proc->pop();
    state.status = ActionScriptInterpreter::ScriptState::WAITING_FOR_TAG;
    return STOP;
}

DEFINE_STATEMENT(TagWaitDirect)
{
    ActionScriptInterpreter::ScriptState& state = asi.scriptState(script->name);
    state.waitValue = LONG(*script->bytecodePos++);
    state.status = ActionScriptInterpreter::ScriptState::WAITING_FOR_TAG;
    return STOP;
}

DEFINE_STATEMENT(PolyobjWait)
{
    ActionScriptInterpreter::ScriptState& state = asi.scriptState(script->name);
    state.waitValue = proc->pop();
    state.status = ActionScriptInterpreter::ScriptState::WAITING_FOR_POLYOBJ;
    return STOP;
}

DEFINE_STATEMENT(PolyobjWaitDirect)
{
    ActionScriptInterpreter::ScriptState& state = asi.scriptState(script->name);
    state.waitValue = LONG(*script->bytecodePos++);
    state.status = ActionScriptInterpreter::ScriptState::WAITING_FOR_POLYOBJ;
    return STOP;
}

DEFINE_STATEMENT(ChangeFloor)
{
    const dchar* flatName = asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(proc->pop()));
    dint tag = proc->pop();
    IterList* list;

    if((list = P_CurrentMap()->sectorIterListForTag(tag, false)))
    {
        Material* mat = P_MaterialForName(MN_FLATS, flatName);
        Sector* sec = NULL;

        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_FLOOR_MATERIAL, mat);
        }
    }

    return CONTINUE;
}

DEFINE_STATEMENT(ChangeFloorDirect)
{
    dint tag = LONG(*script->bytecodePos++);
    const dchar* flatName = asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(LONG(*script->bytecodePos++)));
    IterList* list;

    if((list = P_CurrentMap()->sectorIterListForTag(tag, false)))
    {
        Material* mat = P_MaterialForName(MN_FLATS, flatName);
        Sector* sec = NULL;

        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_FLOOR_MATERIAL, mat);
        }
    }

    return CONTINUE;
}

DEFINE_STATEMENT(ChangeCeiling)
{
    const dchar* flatName = asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(proc->pop()));
    dint tag = proc->pop();
    IterList* list;

    if((list = P_CurrentMap()->sectorIterListForTag(tag, false)))
    {
        Material* mat = P_MaterialForName(MN_FLATS, flatName);
        Sector* sec = NULL;

        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_CEILING_MATERIAL, mat);
        }
    }

    return CONTINUE;
}

DEFINE_STATEMENT(ChangeCeilingDirect)
{
    dint tag = LONG(*script->bytecodePos++);
    const dchar* flatName = asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(LONG(*script->bytecodePos++)));
    IterList* list;

    if((list = P_CurrentMap()->sectorIterListForTag(tag, false)))
    {
        Material* mat = P_MaterialForName(MN_FLATS, flatName);
        Sector* sec = NULL;

        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_CEILING_MATERIAL, mat);
        }
    }

    return CONTINUE;
}

DEFINE_STATEMENT(Restart)
{
    script->bytecodePos = asi.bytecode().function(script->name).entryPoint;
    return CONTINUE;
}

DEFINE_STATEMENT(AndLogical)
{
    proc->push(proc->pop() && proc->pop());
    return CONTINUE;
}

DEFINE_STATEMENT(OrLogical)
{
    proc->push(proc->pop() || proc->pop());
    return CONTINUE;
}

DEFINE_STATEMENT(AndBitwise)
{
    proc->push(proc->pop() & proc->pop());
    return CONTINUE;
}

DEFINE_STATEMENT(OrBitwise)
{
    proc->push(proc->pop() | proc->pop());
    return CONTINUE;
}

DEFINE_STATEMENT(EorBitwise)
{
    proc->push(proc->pop() ^ proc->pop());
    return CONTINUE;
}

DEFINE_STATEMENT(NegateLogical)
{
    proc->push(!proc->pop());
    return CONTINUE;
}

DEFINE_STATEMENT(LShift)
{
    dint operand2 = proc->pop();
    proc->push(proc->pop() << operand2);
    return CONTINUE;
}

DEFINE_STATEMENT(RShift)
{
    dint operand2 = proc->pop();
    proc->push(proc->pop() >> operand2);
    return CONTINUE;
}

DEFINE_STATEMENT(UnaryMinus)
{
    proc->push(-proc->pop());
    return CONTINUE;
}

DEFINE_STATEMENT(IfNotGoto)
{
    if(proc->pop())
    {
        script->bytecodePos++;
    }
    else
    {
        script->bytecodePos = (dint*) (asi.bytecode().base + LONG(*script->bytecodePos));
    }
    return CONTINUE;
}

DEFINE_STATEMENT(LineDefSide)
{
    proc->push(proc->_lineSide);
    return CONTINUE;
}

DEFINE_STATEMENT(ScriptWait)
{
    ActionScriptInterpreter::ScriptState& state = asi.scriptState(script->name);
    state.waitValue = proc->pop();
    state.status = ActionScriptInterpreter::ScriptState::WAITING_FOR_SCRIPT;
    return STOP;
}

DEFINE_STATEMENT(ScriptWaitDirect)
{
    ActionScriptInterpreter::ScriptState& state = asi.scriptState(script->name);
    state.waitValue = LONG(*script->bytecodePos++);
    state.status = ActionScriptInterpreter::ScriptState::WAITING_FOR_SCRIPT;
    return STOP;
}

DEFINE_STATEMENT(ClearLineDefSpecial)
{
    if(proc->_lineDef)
    {
        P_ToXLine(proc->_lineDef)->special = 0;
    }
    return CONTINUE;
}

DEFINE_STATEMENT(CaseGoto)
{
    if(proc->top() == LONG(*script->bytecodePos++))
    {
        script->bytecodePos = (dint*) (asi.bytecode().base + LONG(*script->bytecodePos));
        proc->drop();
    }
    else
    {
        script->bytecodePos++;
    }
    return CONTINUE;
}

DEFINE_STATEMENT(BeginPrint)
{
    asi._printBuffer[0] = 0;
    return CONTINUE;
}

DEFINE_STATEMENT(EndPrint)
{
    if(proc->_activator && proc->_activator->player)
    {
        P_SetMessage(proc->_activator->player, asi._printBuffer, false);
    }
    else
    {   // Send to everybody.
        for(dint i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame)
                P_SetMessage(&players[i], asi._printBuffer, false);
    }

    return CONTINUE;
}

DEFINE_STATEMENT(EndPrintBold)
{
    for(dint i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            P_SetYellowMessage(&players[i], asi._printBuffer, false);
        }
    }

    return CONTINUE;
}

DEFINE_STATEMENT(PrintString)
{
    strcat(asi._printBuffer, asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(proc->pop())));
    return CONTINUE;
}

DEFINE_STATEMENT(PrintNumber)
{
    dchar tempStr[16];
    sprintf(tempStr, "%d", proc->pop());
    strcat(asi._printBuffer, tempStr);
    return CONTINUE;
}

DEFINE_STATEMENT(PrintCharacter)
{
    dchar* bufferEnd;
    bufferEnd = asi._printBuffer + strlen(asi._printBuffer);
    *bufferEnd++ = proc->pop();
    *bufferEnd = 0;
    return CONTINUE;
}

DEFINE_STATEMENT(PlayerCount)
{
    dint count = 0;
    for(dint i = 0; i < MAXPLAYERS; ++i)
    {
        count += players[i].plr->inGame;
    }
    proc->push(count);
    return CONTINUE;
}

DEFINE_STATEMENT(GameType)
{
    dint gameType;

    if(IS_NETGAME == false)
    {
        gameType = GAMETYPE_SINGLE;
    }
    else if(deathmatch)
    {
        gameType = GAMETYPE_DEATHMATCH;
    }
    else
    {
        gameType = GAMETYPE_COOPERATIVE;
    }
    proc->push(gameType);

    return CONTINUE;
}

DEFINE_STATEMENT(GameSkill)
{
    proc->push(gameSkill);
    return CONTINUE;
}

DEFINE_STATEMENT(Timer)
{
    GameMap* map = P_CurrentMap();
    proc->push(map->time);
    return CONTINUE;
}

DEFINE_STATEMENT(SectorSound)
{
    dint volume;
    Thing* th = NULL;

    if(proc->_lineDef)
    {
        th = DMU_GetPtrp(DMU_GetPtrp(proc->_lineDef, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
    }
    volume = proc->pop();

    S_StartSoundAtVolume(S_GetSoundID(asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(proc->pop()))), th, volume / 127.0f);
    return CONTINUE;
}

DEFINE_STATEMENT(ThingSound)
{
    dint tid, sound, volume, searcher;
    Thing* th;

    volume = proc->pop();
    sound = S_GetSoundID(asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(proc->pop())));
    tid = proc->pop();
    searcher = -1;
    while(sound && (th = P_FindMobjFromTID(P_CurrentMap(), tid, &searcher)) != NULL)
    {
        S_StartSoundAtVolume(sound, th, volume / 127.0f);
    }

    return CONTINUE;
}

DEFINE_STATEMENT(AmbientSound)
{
    dint volume, sound;
    Thing* th = NULL; // For 3D positioning.
    Thing* plrth = players[DISPLAYPLAYER].plr->thing;

    volume = proc->pop();
    // If we are playing 3D sounds, create a temporary source for the sound.
    if(cfg.snd3D && plrth && plrth->hasObject())
    {
        Object& obj = plrth->object();
        GameMap* map = obj.map(plrth);

        Vector3f pos = obj.pos +
            Vector3f((((M_Random() - 127) * 2) << FRACBITS),
                     (((M_Random() - 127) * 2) << FRACBITS),
                     (((M_Random() - 127) * 2) << FRACBITS));

        if((th = map->spawnThing(MT_CAMERA, pos, 0, 0))) // A camera is a good temporary source.
            th->tics = 5 * TICSPERSEC; // Five seconds should be enough.
    }

    sound = S_GetSoundID(asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(proc->pop())));
    S_StartSoundAtVolume(sound, th, volume / 127.0f);
    return CONTINUE;
}

DEFINE_STATEMENT(SoundSequence)
{
    Thing* th = NULL;
    if(proc->_lineDef)
        th = DMU_GetPtrp(DMU_GetPtrp(proc->_lineDef, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
    SN_StartSequenceName(mo, asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(proc->pop())));
    return CONTINUE;
}

DEFINE_STATEMENT(SetSideDefMaterial)
{
    GameMap* map = P_CurrentMap();
    dint lineTag, side, position;
    Material* mat;
    LineDef* line;
    IterList* list;

    mat = P_MaterialForName(MN_TEXTURES, asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(proc->pop())));
    position = proc->pop();
    side = proc->pop();
    lineTag = proc->pop();

    list = map->iterListForTag(lineTag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((line = P_IterListIterator(list)) != NULL)
        {
            SideDef* sdef = DMU_GetPtrp(line, (side == 0? DMU_SIDEDEF0 : DMU_SIDEDEF1));

            if(position == SIDEDEF_SECTION_MIDDLE)
            {
                DMU_SetPtrp(sdef, DMU_MIDDLE_MATERIAL, mat);
            }
            else if(position == SIDEDEF_SECTION_BOTTOM)
            {
                DMU_SetPtrp(sdef, DMU_BOTTOM_MATERIAL, mat);
            }
            else
            {
                DMU_SetPtrp(sdef, DMU_TOP_MATERIAL, mat);
            }
        }
    }

    return CONTINUE;
}

DEFINE_STATEMENT(SetLineDefBlocking)
{
    GameMap* map = P_CurrentMap();
    LineDef* line;
    dint lineTag;
    bool blocking;
    IterList* list;

    blocking = proc->pop()? DDLF_BLOCKING : 0;
    lineTag = proc->pop();

    list = map->iterListForTag(lineTag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((line = P_IterListIterator(list)) != NULL)
        {
            DMU_SetIntp(line, DMU_FLAGS,
                (DMU_GetIntp(line, DMU_FLAGS) & ~DDLF_BLOCKING) | blocking);
        }
    }

    return CONTINUE;
}

DEFINE_STATEMENT(SetLineDefSpecial)
{
    GameMap* map = P_CurrentMap();
    LineDef* line;
    dint lineTag, special, arg1, arg2, arg3, arg4, arg5;
    IterList* list;

    arg5 = proc->pop();
    arg4 = proc->pop();
    arg3 = proc->pop();
    arg2 = proc->pop();
    arg1 = proc->pop();
    special = proc->pop();
    lineTag = proc->pop();

    list = map->iterListForTag(lineTag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((line = P_IterListIterator(list)) != NULL)
        {
            XLineDef* xline = P_ToXLine(line);
            xline->special = special;
            xline->arg1 = arg1;
            xline->arg2 = arg2;
            xline->arg3 = arg3;
            xline->arg4 = arg4;
            xline->arg5 = arg5;
        }
    }

    return CONTINUE;
}
}
