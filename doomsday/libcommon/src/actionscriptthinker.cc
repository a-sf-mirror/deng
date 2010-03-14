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
typedef enum {
    SA_CONTINUE,
    SA_STOP,
    SA_TERMINATE
} script_action_t;

// A handy helper for declaring action script command interpreter.
#define D_ASCMD(x) script_action_t Cmd##x(ActionScriptInterpreter& asi, ActionScriptThinker* script)

/// Pointer to a script bytecode command interpreter.
typedef script_action_t (*script_bytecode_cmdinterpreter_t) (ActionScriptInterpreter& asi, ActionScriptThinker*);

D_ASCMD(NOP);
D_ASCMD(Terminate);
D_ASCMD(Suspend);
D_ASCMD(PushNumber);
D_ASCMD(LSpec1);
D_ASCMD(LSpec2);
D_ASCMD(LSpec3);
D_ASCMD(LSpec4);
D_ASCMD(LSpec5);
D_ASCMD(LSpec1Direct);
D_ASCMD(LSpec2Direct);
D_ASCMD(LSpec3Direct);
D_ASCMD(LSpec4Direct);
D_ASCMD(LSpec5Direct);
D_ASCMD(Add);
D_ASCMD(Subtract);
D_ASCMD(Multiply);
D_ASCMD(Divide);
D_ASCMD(Modulus);
D_ASCMD(EQ);
D_ASCMD(NE);
D_ASCMD(LT);
D_ASCMD(GT);
D_ASCMD(LE);
D_ASCMD(GE);
D_ASCMD(AssignScriptVar);
D_ASCMD(AssignMapVar);
D_ASCMD(AssignWorldVar);
D_ASCMD(PushScriptVar);
D_ASCMD(PushMapVar);
D_ASCMD(PushWorldVar);
D_ASCMD(AddScriptVar);
D_ASCMD(AddMapVar);
D_ASCMD(AddWorldVar);
D_ASCMD(SubScriptVar);
D_ASCMD(SubMapVar);
D_ASCMD(SubWorldVar);
D_ASCMD(MulScriptVar);
D_ASCMD(MulMapVar);
D_ASCMD(MulWorldVar);
D_ASCMD(DivScriptVar);
D_ASCMD(DivMapVar);
D_ASCMD(DivWorldVar);
D_ASCMD(ModScriptVar);
D_ASCMD(ModMapVar);
D_ASCMD(ModWorldVar);
D_ASCMD(IncScriptVar);
D_ASCMD(IncMapVar);
D_ASCMD(IncWorldVar);
D_ASCMD(DecScriptVar);
D_ASCMD(DecMapVar);
D_ASCMD(DecWorldVar);
D_ASCMD(Goto);
D_ASCMD(IfGoto);
D_ASCMD(Drop);
D_ASCMD(Delay);
D_ASCMD(DelayDirect);
D_ASCMD(Random);
D_ASCMD(RandomDirect);
D_ASCMD(ThingCount);
D_ASCMD(ThingCountDirect);
D_ASCMD(TagWait);
D_ASCMD(TagWaitDirect);
D_ASCMD(PolyobjWait);
D_ASCMD(PolyobjWaitDirect);
D_ASCMD(ChangeFloor);
D_ASCMD(ChangeFloorDirect);
D_ASCMD(ChangeCeiling);
D_ASCMD(ChangeCeilingDirect);
D_ASCMD(Restart);
D_ASCMD(AndLogical);
D_ASCMD(OrLogical);
D_ASCMD(AndBitwise);
D_ASCMD(OrBitwise);
D_ASCMD(EorBitwise);
D_ASCMD(NegateLogical);
D_ASCMD(LShift);
D_ASCMD(RShift);
D_ASCMD(UnaryMinus);
D_ASCMD(IfNotGoto);
D_ASCMD(LineDefSide);
D_ASCMD(ScriptWait);
D_ASCMD(ScriptWaitDirect);
D_ASCMD(ClearLineDefSpecial);
D_ASCMD(CaseGoto);
D_ASCMD(BeginPrint);
D_ASCMD(EndPrint);
D_ASCMD(PrintString);
D_ASCMD(PrintNumber);
D_ASCMD(PrintCharacter);
D_ASCMD(PlayerCount);
D_ASCMD(GameType);
D_ASCMD(GameSkill);
D_ASCMD(Timer);
D_ASCMD(SectorSound);
D_ASCMD(AmbientSound);
D_ASCMD(SoundSequence);
D_ASCMD(SetSideDefMaterial);
D_ASCMD(SetLineDefBlocking);
D_ASCMD(SetLineDefSpecial);
D_ASCMD(ThingSound);
D_ASCMD(EndPrintBold);

const script_bytecode_cmdinterpreter_t bytecodeCommandInterpreters[] =
{
    CmdNOP, CmdTerminate, CmdSuspend, CmdPushNumber, CmdLSpec1, CmdLSpec2,
    CmdLSpec3, CmdLSpec4, CmdLSpec5, CmdLSpec1Direct, CmdLSpec2Direct,
    CmdLSpec3Direct, CmdLSpec4Direct, CmdLSpec5Direct, CmdAdd,
    CmdSubtract, CmdMultiply, CmdDivide, CmdModulus, CmdEQ, CmdNE,
    CmdLT, CmdGT, CmdLE, CmdGE, CmdAssignScriptVar, CmdAssignMapVar,
    CmdAssignWorldVar, CmdPushScriptVar, CmdPushMapVar,
    CmdPushWorldVar, CmdAddScriptVar, CmdAddMapVar, CmdAddWorldVar,
    CmdSubScriptVar, CmdSubMapVar, CmdSubWorldVar, CmdMulScriptVar,
    CmdMulMapVar, CmdMulWorldVar, CmdDivScriptVar, CmdDivMapVar,
    CmdDivWorldVar, CmdModScriptVar, CmdModMapVar, CmdModWorldVar,
    CmdIncScriptVar, CmdIncMapVar, CmdIncWorldVar, CmdDecScriptVar,
    CmdDecMapVar, CmdDecWorldVar, CmdGoto, CmdIfGoto, CmdDrop,
    CmdDelay, CmdDelayDirect, CmdRandom, CmdRandomDirect,
    CmdThingCount, CmdThingCountDirect, CmdTagWait, CmdTagWaitDirect,
    CmdPolyobjWait, CmdPolyobjWaitDirect, CmdChangeFloor,
    CmdChangeFloorDirect, CmdChangeCeiling, CmdChangeCeilingDirect,
    CmdRestart, CmdAndLogical, CmdOrLogical, CmdAndBitwise,
    CmdOrBitwise, CmdEorBitwise, CmdNegateLogical, CmdLShift,
    CmdRShift, CmdUnaryMinus, CmdIfNotGoto, CmdLineDefSide, CmdScriptWait,
    CmdScriptWaitDirect, CmdClearLineDefSpecial, CmdCaseGoto,
    CmdBeginPrint, CmdEndPrint, CmdPrintString, CmdPrintNumber,
    CmdPrintCharacter, CmdPlayerCount, CmdGameType, CmdGameSkill,
    CmdTimer, CmdSectorSound, CmdAmbientSound, CmdSoundSequence,
    CmdSetSideDefMaterial, CmdSetLineDefBlocking, CmdSetLineDefSpecial,
    CmdThingSound, CmdEndPrintBold
};

__inline void Drop(ActionScriptThinker* script)
{
    script->stackDepth--;
}

__inline dint Pop(ActionScriptThinker* script)
{
    return script->stack[--script->stackDepth];
}

__inline void Push(ActionScriptThinker* script, dint value)
{
    script->stack[script->stackDepth++] = value;
}

__inline dint Top(ActionScriptThinker* script)
{
    return script->stack[script->stackDepth - 1];
}
}

void ActionScriptThinker::think(const de::Time::Delta& /* elapsed */)
{
    ActionScriptInterpreter& asi = ActionScriptInterpreter::actionScriptInterpreter();
    ActionScriptInterpreter::ScriptStateRecord& rec = asi.scriptStateRecord(scriptId);

    if(rec.state == ActionScriptInterpreter::ScriptStateRecord::TERMINATING)
    {
        rec.state = ActionScriptInterpreter::ScriptStateRecord::INACTIVE;
        asi.scriptFinished(scriptId);
        Map_RemoveThinker(Thinker_Map(this), this);
        return;
    }

    if(rec.state != ActionScriptInterpreter::ScriptStateRecord::RUNNING)
    {
        return;
    }

    if(delayCount)
    {
        delayCount--;
        return;
    }

    script_action_t action;
    do
    {
        script_bytecode_cmdinterpreter_t cmd = bytecodeCommandInterpreters[LONG(*bytecodePos++)];
        action = cmd(asi, this);
    } while(action == SA_CONTINUE);

    if(action == SA_TERMINATE)
    {
        rec.state = ActionScriptInterpreter::ScriptStateRecord::INACTIVE;
        asi.scriptFinished(scriptId);
        Map_RemoveThinker(Thinker_Map(this), this);
    }
}

void ActionScriptThinker::operator >> (de::Writer& to) const
{
    Thinker::operator >> (to);

    to << dbyte(1) // Write a version byte.
       << dint(SV_ThingArchiveNum(activator))
       << (lineDef ? dint(DMU_ToIndex(lineDef)) : dint(-1))
       << lineSide
       << scriptId
       << infoIndex
       << delayCount;

    for(duint i = 0; i < AST_STACK_DEPTH; ++i)
        to << stack[i];

    to << stackDepth;
    for(duint i = 0; i < AST_MAX_VARS; ++i)
        to << vars[i];

    ActionScriptInterpreter& asi = ActionScriptInterpreter::actionScriptInterpreter();
    to << (dint(bytecodePos) - dint(asi.bytecode().base));
}

void ActionScriptThinker::operator << (de::Reader& from)
{
    dint temp;

    if(saveVersion >= 4)
    {
        // @note the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        activator = (Thing*) SV_ReadLong();
        activator = SV_GetArchiveThing((dint) activator, &activator);
        temp = SV_ReadLong();
        if(temp == -1)
            lineDef = NULL;
        else
            lineDef = DMU_ToPtr(DMU_LINEDEF, temp);
        lineSide = SV_ReadLong();
        scriptId = (ActionScriptId) SV_ReadLong();
        infoIndex = SV_ReadLong();
        delayCount = SV_ReadLong();
        for(duint i = 0; i < AST_STACK_DEPTH; ++i)
            stack[i] = SV_ReadLong();
        stackDepth = SV_ReadLong();
        for(duint i = 0; i < AST_MAX_VARS; ++i)
            vars[i] = SV_ReadLong();

        ActionScriptInterpreter& asi = ActionScriptInterpreter::actionScriptInterpreter();
        bytecodePos = (const dint*) (asi.bytecode().base + SV_ReadLong());
    }
    else
    {
        // Its in the old pre V4 format which serialized ActionScriptThinker
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        activator = (Thing*) SV_ReadLong();
        activator = SV_GetArchiveThing((dint) activator, &activator);
        temp = SV_ReadLong();
        if(temp == -1)
            lineDef = NULL;
        else
            lineDef = DMU_ToPtr(DMU_LINEDEF, temp);
        lineSide = SV_ReadLong();
        scriptId = (ActionScriptId) SV_ReadLong();
        infoIndex = SV_ReadLong();
        delayCount = SV_ReadLong();
        for(duint i = 0; i < AST_STACK_DEPTH; ++i)
            stack[i] = SV_ReadLong();
        stackDepth = SV_ReadLong();
        for(duint i = 0; i < AST_MAX_VARS; ++i)
            vars[i] = SV_ReadLong();

        ActionScriptInterpreter& asi = ActionScriptInterpreter::actionScriptInterpreter();
        bytecodePos = (const dint*) (asi.bytecode().base + SV_ReadLong());
    }
}

namespace {
D_ASCMD(NOP)
{
    return SA_CONTINUE;
}

D_ASCMD(Terminate)
{
    return SA_TERMINATE;
}

D_ASCMD(Suspend)
{
    asi.scriptStateRecord(script->scriptId).state = ActionScriptInterpreter::ScriptStateRecord::SUSPENDED;
    return SA_STOP;
}

D_ASCMD(PushNumber)
{
    Push(script, LONG(*script->bytecodePos++));
    return SA_CONTINUE;
}

D_ASCMD(LSpec1)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = Pop(script);
    P_ExecuteLineSpecial(special, args, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec2)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[1] = Pop(script);
    args[0] = Pop(script);
    P_ExecuteLineSpecial(special, args, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec3)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[2] = Pop(script);
    args[1] = Pop(script);
    args[0] = Pop(script);
    P_ExecuteLineSpecial(special, args, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec4)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[3] = Pop(script);
    args[2] = Pop(script);
    args[1] = Pop(script);
    args[0] = Pop(script);
    P_ExecuteLineSpecial(special, args, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec5)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[4] = Pop(script);
    args[3] = Pop(script);
    args[2] = Pop(script);
    args[1] = Pop(script);
    args[0] = Pop(script);
    P_ExecuteLineSpecial(special, args, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec1Direct)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, args, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec2Direct)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = LONG(*script->bytecodePos++);
    args[1] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, args, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec3Direct)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = LONG(*script->bytecodePos++);
    args[1] = LONG(*script->bytecodePos++);
    args[2] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, args, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec4Direct)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = LONG(*script->bytecodePos++);
    args[1] = LONG(*script->bytecodePos++);
    args[2] = LONG(*script->bytecodePos++);
    args[3] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, args, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec5Direct)
{
    dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    dint special = LONG(*script->bytecodePos++);
    args[0] = LONG(*script->bytecodePos++);
    args[1] = LONG(*script->bytecodePos++);
    args[2] = LONG(*script->bytecodePos++);
    args[3] = LONG(*script->bytecodePos++);
    args[4] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, args, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(Add)
{
    Push(script, Pop(script) + Pop(script));
    return SA_CONTINUE;
}

D_ASCMD(Subtract)
{
    dint operand2 = Pop(script);
    Push(script, Pop(script) - operand2);
    return SA_CONTINUE;
}

D_ASCMD(Multiply)
{
    Push(script, Pop(script) * Pop(script));
    return SA_CONTINUE;
}

D_ASCMD(Divide)
{
    dint operand2 = Pop(script);
    Push(script, Pop(script) / operand2);
    return SA_CONTINUE;
}

D_ASCMD(Modulus)
{
    dint operand2 = Pop(script);
    Push(script, Pop(script) % operand2);
    return SA_CONTINUE;
}

D_ASCMD(EQ)
{
    Push(script, Pop(script) == Pop(script));
    return SA_CONTINUE;
}

D_ASCMD(NE)
{
    Push(script, Pop(script) != Pop(script));
    return SA_CONTINUE;
}

D_ASCMD(LT)
{
    dint operand2 = Pop(script);
    Push(script, Pop(script) < operand2);
    return SA_CONTINUE;
}

D_ASCMD(GT)
{
    int operand2 = Pop(script);
    Push(script, Pop(script) > operand2);
    return SA_CONTINUE;
}

D_ASCMD(LE)
{
    dint operand2 = Pop(script);
    Push(script, Pop(script) <= operand2);
    return SA_CONTINUE;
}

D_ASCMD(GE)
{
    dint operand2 = Pop(script);
    Push(script, Pop(script) >= operand2);
    return SA_CONTINUE;
}

D_ASCMD(AssignScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] = Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(AssignMapVar)
{
    asi._mapVars[LONG(*script->bytecodePos++)] = Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(AssignWorldVar)
{
    asi._worldVars[LONG(*script->bytecodePos++)] = Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(PushScriptVar)
{
    Push(script, script->vars[LONG(*script->bytecodePos++)]);
    return SA_CONTINUE;
}

D_ASCMD(PushMapVar)
{
    Push(script, asi._mapVars[LONG(*script->bytecodePos++)]);
    return SA_CONTINUE;
}

D_ASCMD(PushWorldVar)
{
    Push(script, asi._worldVars[LONG(*script->bytecodePos++)]);
    return SA_CONTINUE;
}

D_ASCMD(AddScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] += Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(AddMapVar)
{
    asi._mapVars[LONG(*script->bytecodePos++)] += Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(AddWorldVar)
{
    asi._worldVars[LONG(*script->bytecodePos++)] += Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(SubScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] -= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(SubMapVar)
{
    asi._mapVars[LONG(*script->bytecodePos++)] -= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(SubWorldVar)
{
    asi._worldVars[LONG(*script->bytecodePos++)] -= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(MulScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] *= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(MulMapVar)
{
    asi._mapVars[LONG(*script->bytecodePos++)] *= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(MulWorldVar)
{
    asi._worldVars[LONG(*script->bytecodePos++)] *= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(DivScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] /= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(DivMapVar)
{
    asi._mapVars[LONG(*script->bytecodePos++)] /= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(DivWorldVar)
{
    asi._worldVars[LONG(*script->bytecodePos++)] /= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(ModScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] %= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(ModMapVar)
{
    asi._mapVars[LONG(*script->bytecodePos++)] %= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(ModWorldVar)
{
    asi._worldVars[LONG(*script->bytecodePos++)] %= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(IncScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)]++;
    return SA_CONTINUE;
}

D_ASCMD(IncMapVar)
{
    asi._mapVars[LONG(*script->bytecodePos++)]++;
    return SA_CONTINUE;
}

D_ASCMD(IncWorldVar)
{
    asi._worldVars[LONG(*script->bytecodePos++)]++;
    return SA_CONTINUE;
}

D_ASCMD(DecScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)]--;
    return SA_CONTINUE;
}

D_ASCMD(DecMapVar)
{
    asi._mapVars[LONG(*script->bytecodePos++)]--;
    return SA_CONTINUE;
}

D_ASCMD(DecWorldVar)
{
    asi._worldVars[LONG(*script->bytecodePos++)]--;
    return SA_CONTINUE;
}

D_ASCMD(Goto)
{
    script->bytecodePos = (dint*) (asi.bytecode().base + LONG(*script->bytecodePos));
    return SA_CONTINUE;
}

D_ASCMD(IfGoto)
{
    if(Pop(script))
    {
        script->bytecodePos = (dint*) (asi.bytecode().base + LONG(*script->bytecodePos));
    }
    else
    {
        script->bytecodePos++;
    }
    return SA_CONTINUE;
}

D_ASCMD(Drop)
{
    Drop(script);
    return SA_CONTINUE;
}

D_ASCMD(Delay)
{
    script->delayCount = Pop(script);
    return SA_STOP;
}

D_ASCMD(DelayDirect)
{
    script->delayCount = LONG(*script->bytecodePos++);
    return SA_STOP;
}

D_ASCMD(Random)
{
    dint low, high;
    high = Pop(script);
    low = Pop(script);
    Push(script, low + (P_Random() % (high - low + 1)));
    return SA_CONTINUE;
}

D_ASCMD(RandomDirect)
{
    dint low, high;
    low = LONG(*script->bytecodePos++);
    high = LONG(*script->bytecodePos++);
    Push(script, low + (P_Random() % (high - low + 1)));
    return SA_CONTINUE;
}

D_ASCMD(ThingCount)
{
    dint tid;
    tid = Pop(script);
    Push(script, countThingsOfType(P_CurrentMap(), Pop(script), tid));
    return SA_CONTINUE;
}

D_ASCMD(ThingCountDirect)
{
    dint type;
    type = LONG(*script->bytecodePos++);
    Push(script, countThingsOfType(P_CurrentMap(), type, LONG(*script->bytecodePos++)));
    return SA_CONTINUE;
}

typedef struct {
    mobjtype_t      type;
    dint            count;
} countthingoftypeparams_t;

dint countThingOfType(void* p, void* paramaters)
{
    countthingoftypeparams_t* params = reinterpret_cast<countthingoftypeparams_t*>(paramaters);
    Thing* th = reinterpret_cast<Thing*>(p);

    // Does the type match?
    if(th->type != params->type)
        return true; // Continue iteration.

    // Minimum health requirement?
    if((th->flags & MF_COUNTKILL) && th->health <= 0)
        return true; // Continue iteration.

    params->count++;

    return true; // Continue iteration.
}

dint countThingsOfType(GameMap* map, dint type, idnt tid)
{
    mobjtype_t thingType;
    dint count;

    if(!(type + tid))
    {   // Nothing to count.
        return 0;
    }

    thingType = P_MapScriptThingIdToMobjType(type);
    count = 0;

    if(tid)
    {   // Count TID things.
        Thing* th;
        dint searcher = -1;

        while((th = P_FindMobjFromTID(map, tid, &searcher)) != NULL)
        {
            if(type == 0)
            {   // Just count TIDs.
                count++;
            }
            else if(thingType == th->type)
            {
                if((th->flags & MF_COUNTKILL) && th->health <= 0)
                {   // Don't count dead monsters.
                    continue;
                }

                count++;
            }
        }
    }
    else
    {   // Count only types.
        countthingoftypeparams_t params;

        params.type = thingType;
        params.count = 0;
        Map_IterateThinkers(map, P_MobjThinker, countThingOfType, &params);

        count = params.count;
    }

    return count;
}

D_ASCMD(TagWait)
{
    ActionScriptInterpreter::ScriptStateRecord& rec = asi.scriptStateRecord(script->scriptId);
    rec.waitValue = Pop(script);
    rec.state = ActionScriptInterpreter::ScriptStateRecord::WAITING_FOR_TAG;
    return SA_STOP;
}

D_ASCMD(TagWaitDirect)
{
    ActionScriptInterpreter::ScriptStateRecord& rec = asi.scriptStateRecord(script->scriptId);
    rec.waitValue = LONG(*script->bytecodePos++);
    rec.state = ActionScriptInterpreter::ScriptStateRecord::WAITING_FOR_TAG;
    return SA_STOP;
}

D_ASCMD(PolyobjWait)
{
    ActionScriptInterpreter::ScriptStateRecord& rec = asi.scriptStateRecord(script->scriptId);
    rec.waitValue = Pop(script);
    rec.state = ActionScriptInterpreter::ScriptStateRecord::WAITING_FOR_POLYOBJ;
    return SA_STOP;
}

D_ASCMD(PolyobjWaitDirect)
{
    ActionScriptInterpreter::ScriptStateRecord& rec = asi.scriptStateRecord(script->scriptId);
    rec.waitValue = LONG(*script->bytecodePos++);
    rec.state = ActionScriptInterpreter::ScriptStateRecord::WAITING_FOR_POLYOBJ;
    return SA_STOP;
}

D_ASCMD(ChangeFloor)
{
    const dchar* flatName = asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(Pop(script)));
    dint tag = Pop(script);
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

    return SA_CONTINUE;
}

D_ASCMD(ChangeFloorDirect)
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

    return SA_CONTINUE;
}

D_ASCMD(ChangeCeiling)
{
    const dchar* flatName = asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(Pop(script)));
    dint tag = Pop(script);
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

    return SA_CONTINUE;
}

D_ASCMD(ChangeCeilingDirect)
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

    return SA_CONTINUE;
}

D_ASCMD(Restart)
{
    script->bytecodePos = asi.bytecode().scriptInfo[script->infoIndex].entryPoint;
    return SA_CONTINUE;
}

D_ASCMD(AndLogical)
{
    Push(script, Pop(script) && Pop(script));
    return SA_CONTINUE;
}

D_ASCMD(OrLogical)
{
    Push(script, Pop(script) || Pop(script));
    return SA_CONTINUE;
}

D_ASCMD(AndBitwise)
{
    Push(script, Pop(script) & Pop(script));
    return SA_CONTINUE;
}

D_ASCMD(OrBitwise)
{
    Push(script, Pop(script) | Pop(script));
    return SA_CONTINUE;
}

D_ASCMD(EorBitwise)
{
    Push(script, Pop(script) ^ Pop(script));
    return SA_CONTINUE;
}

D_ASCMD(NegateLogical)
{
    Push(script, !Pop(script));
    return SA_CONTINUE;
}

D_ASCMD(LShift)
{
    dint operand2 = Pop(script);
    Push(script, Pop(script) << operand2);
    return SA_CONTINUE;
}

D_ASCMD(RShift)
{
    dint operand2 = Pop(script);
    Push(script, Pop(script) >> operand2);
    return SA_CONTINUE;
}

D_ASCMD(UnaryMinus)
{
    Push(script, -Pop(script));
    return SA_CONTINUE;
}

D_ASCMD(IfNotGoto)
{
    if(Pop(script))
    {
        script->bytecodePos++;
    }
    else
    {
        script->bytecodePos = (dint*) (asi.bytecode().base + LONG(*script->bytecodePos));
    }
    return SA_CONTINUE;
}

D_ASCMD(LineDefSide)
{
    Push(script, script->lineSide);
    return SA_CONTINUE;
}

D_ASCMD(ScriptWait)
{
    ActionScriptInterpreter::ScriptStateRecord& rec = asi.scriptStateRecord(script->scriptId);
    rec.waitValue = Pop(script);
    rec.state = ActionScriptInterpreter::ScriptStateRecord::WAITING_FOR_SCRIPT;
    return SA_STOP;
}

D_ASCMD(ScriptWaitDirect)
{
    ActionScriptInterpreter::ScriptStateRecord& rec = asi.scriptStateRecord(script->scriptId);
    rec.waitValue = LONG(*script->bytecodePos++);
    rec.state = ActionScriptInterpreter::ScriptStateRecord::WAITING_FOR_SCRIPT;
    return SA_STOP;
}

D_ASCMD(ClearLineDefSpecial)
{
    if(script->lineDef)
    {
        P_ToXLine(script->lineDef)->special = 0;
    }
    return SA_CONTINUE;
}

D_ASCMD(CaseGoto)
{
    if(Top(script) == LONG(*script->bytecodePos++))
    {
        script->bytecodePos = (dint*) (asi.bytecode().base + LONG(*script->bytecodePos));
        Drop(script);
    }
    else
    {
        script->bytecodePos++;
    }
    return SA_CONTINUE;
}

D_ASCMD(BeginPrint)
{
    asi._printBuffer[0] = 0;
    return SA_CONTINUE;
}

D_ASCMD(EndPrint)
{
    if(script->activator && script->activator->player)
    {
        P_SetMessage(script->activator->player, asi._printBuffer, false);
    }
    else
    {   // Send to everybody.
        for(dint i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame)
                P_SetMessage(&players[i], asi._printBuffer, false);
    }

    return SA_CONTINUE;
}

D_ASCMD(EndPrintBold)
{
    for(dint i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            P_SetYellowMessage(&players[i], asi._printBuffer, false);
        }
    }

    return SA_CONTINUE;
}

D_ASCMD(PrintString)
{
    strcat(asi._printBuffer, asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(Pop(script))));
    return SA_CONTINUE;
}

D_ASCMD(PrintNumber)
{
    dchar tempStr[16];
    sprintf(tempStr, "%d", Pop(script));
    strcat(asi._printBuffer, tempStr);
    return SA_CONTINUE;
}

D_ASCMD(PrintCharacter)
{
    dchar* bufferEnd;
    bufferEnd = asi._printBuffer + strlen(asi._printBuffer);
    *bufferEnd++ = Pop(script);
    *bufferEnd = 0;
    return SA_CONTINUE;
}

D_ASCMD(PlayerCount)
{
    dint count = 0;
    for(dint i = 0; i < MAXPLAYERS; ++i)
    {
        count += players[i].plr->inGame;
    }
    Push(script, count);
    return SA_CONTINUE;
}

D_ASCMD(GameType)
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
    Push(script, gameType);

    return SA_CONTINUE;
}

D_ASCMD(GameSkill)
{
    Push(script, gameSkill);
    return SA_CONTINUE;
}

D_ASCMD(Timer)
{
    GameMap* map = P_CurrentMap();
    Push(script, map->time);
    return SA_CONTINUE;
}

D_ASCMD(SectorSound)
{
    dint volume;
    Thing* th = NULL;

    if(script->lineDef)
    {
        th = DMU_GetPtrp(DMU_GetPtrp(script->lineDef, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
    }
    volume = Pop(script);

    S_StartSoundAtVolume(S_GetSoundID(asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(Pop(script)))), th, volume / 127.0f);
    return SA_CONTINUE;
}

D_ASCMD(ThingSound)
{
    dint tid, sound, volume, searcher;
    Thing* th;

    volume = Pop(script);
    sound = S_GetSoundID(asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(Pop(script))));
    tid = Pop(script);
    searcher = -1;
    while(sound && (th = P_FindMobjFromTID(P_CurrentMap(), tid, &searcher)) != NULL)
    {
        S_StartSoundAtVolume(sound, th, volume / 127.0f);
    }

    return SA_CONTINUE;
}

D_ASCMD(AmbientSound)
{
    dint volume, sound;
    Thing* th = NULL; // For 3D positioning.
    Thing* plrth = players[DISPLAYPLAYER].plr->thing;

    volume = Pop(script);
    // If we are playing 3D sounds, create a temporary source mobj
    // for the sound.
    if(cfg.snd3D && plrth)
    {
        GameMap* map = Thinker_Map(plrth);
        Vector3f pos = plrth->pos +
            Vector3f((((M_Random() - 127) * 2) << FRACBITS),
                     (((M_Random() - 127) * 2) << FRACBITS),
                     (((M_Random() - 127) * 2) << FRACBITS));

        if((th = map->spawnThing(MT_CAMERA, pos, 0, 0))) // A camera is a good temporary source.
            th->tics = 5 * TICSPERSEC; // Five seconds should be enough.
    }

    sound = S_GetSoundID(asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(Pop(script))));
    S_StartSoundAtVolume(sound, th, volume / 127.0f);

    return SA_CONTINUE;
}

D_ASCMD(SoundSequence)
{
    Thing* th = NULL;
    if(script->lineDef)
        th = DMU_GetPtrp(DMU_GetPtrp(script->lineDef, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
    SN_StartSequenceName(mo, asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(Pop(script))));
    return SA_CONTINUE;
}

D_ASCMD(SetSideDefMaterial)
{
    GameMap* map = P_CurrentMap();
    dint lineTag, side, position;
    Material* mat;
    LineDef* line;
    IterList* list;

    mat = P_MaterialForName(MN_TEXTURES, asi.bytecode().string(static_cast<ActionScriptInterpreter::Bytecode::StringId>(Pop(script))));
    position = Pop(script);
    side = Pop(script);
    lineTag = Pop(script);

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

    return SA_CONTINUE;
}

D_ASCMD(SetLineDefBlocking)
{
    GameMap* map = P_CurrentMap();
    LineDef* line;
    dint lineTag;
    bool blocking;
    IterList* list;

    blocking = Pop(script)? DDLF_BLOCKING : 0;
    lineTag = Pop(script);

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

    return SA_CONTINUE;
}

D_ASCMD(SetLineDefSpecial)
{
    GameMap* map = P_CurrentMap();
    LineDef* line;
    dint lineTag, special, arg1, arg2, arg3, arg4, arg5;
    IterList* list;

    arg5 = Pop(script);
    arg4 = Pop(script);
    arg3 = Pop(script);
    arg2 = Pop(script);
    arg1 = Pop(script);
    special = Pop(script);
    lineTag = Pop(script);

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

    return SA_CONTINUE;
}
}
