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

#include "common/ActionScriptStatement"

using namespace de;

class NOPStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        return ActionScriptThinker::CONTINUE;
    }
};

class TerminateStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        return ActionScriptThinker::TERMINATE;
    }
};

class SuspendStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase.scriptState(script->name).status = ActionScriptEnvironment::ScriptState::SUSPENDED;
        return ActionScriptThinker::STOP;
    }
};

class PushNumberStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(LONG(*script->bytecodePos++));
        return ActionScriptThinker::CONTINUE;
    }
};

class LSpec1Statement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        dint special = LONG(*script->bytecodePos++);
        args[0] = proc->pop();
        P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }
};

class LSpec2Statement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        dint special = LONG(*script->bytecodePos++);
        args[1] = proc->pop();
        args[0] = proc->pop();
        P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }
};

class LSpec3Statement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        dint special = LONG(*script->bytecodePos++);
        args[2] = proc->pop();
        args[1] = proc->pop();
        args[0] = proc->pop();
        P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }
};

class LSpec4Statement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        dint special = LONG(*script->bytecodePos++);
        args[3] = proc->pop();
        args[2] = proc->pop();
        args[1] = proc->pop();
        args[0] = proc->pop();
        P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }
};

class LSpec5Statement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        dint special = LONG(*script->bytecodePos++);
        args[4] = proc->pop();
        args[3] = proc->pop();
        args[2] = proc->pop();
        args[1] = proc->pop();
        args[0] = proc->pop();
        P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }
};

class LSpec1DirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        dint special = LONG(*script->bytecodePos++);
        args[0] = LONG(*script->bytecodePos++);
        P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }
};

class LSpec2DirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        dint special = LONG(*script->bytecodePos++);
        args[0] = LONG(*script->bytecodePos++);
        args[1] = LONG(*script->bytecodePos++);
        P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }
};

class LSpec3DirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        dint special = LONG(*script->bytecodePos++);
        args[0] = LONG(*script->bytecodePos++);
        args[1] = LONG(*script->bytecodePos++);
        args[2] = LONG(*script->bytecodePos++);
        P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }
};

class LSpec4DirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        dint special = LONG(*script->bytecodePos++);
        args[0] = LONG(*script->bytecodePos++);
        args[1] = LONG(*script->bytecodePos++);
        args[2] = LONG(*script->bytecodePos++);
        args[3] = LONG(*script->bytecodePos++);
        P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }
};

class LSpec5DirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        dint special = LONG(*script->bytecodePos++);
        args[0] = LONG(*script->bytecodePos++);
        args[1] = LONG(*script->bytecodePos++);
        args[2] = LONG(*script->bytecodePos++);
        args[3] = LONG(*script->bytecodePos++);
        args[4] = LONG(*script->bytecodePos++);
        P_ExecuteLineSpecial(special, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }
};

class AddStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() + proc->pop());
        return ActionScriptThinker::CONTINUE;
    }
};

class SubtractStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() - operand2);
        return ActionScriptThinker::CONTINUE;
    }
};

class MultiplyStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() * proc->pop());
        return ActionScriptThinker::CONTINUE;
    }
};

class DivideStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() / operand2);
        return ActionScriptThinker::CONTINUE;
    }
};

class ModulusStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() % operand2);
        return ActionScriptThinker::CONTINUE;
    }
};

class EQStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() == proc->pop());
        return ActionScriptThinker::CONTINUE;
    }
};

class NEStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() != proc->pop());
        return ActionScriptThinker::CONTINUE;
    }
};

class LTStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() < operand2);
        return ActionScriptThinker::CONTINUE;
    }
};

class GTStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        int operand2 = proc->pop();
        proc->push(proc->pop() > operand2);
        return ActionScriptThinker::CONTINUE;
    }
};

class LEStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() <= operand2);
        return ActionScriptThinker::CONTINUE;
    }
};

class GEStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() >= operand2);
        return ActionScriptThinker::CONTINUE;
    }
};

class AssignScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[LONG(*script->bytecodePos++)] = proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class AssignMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[LONG(*script->bytecodePos++)] = proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class AssignWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[LONG(*script->bytecodePos++)] = proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class PushScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->_context[LONG(*script->bytecodePos++)]);
        return ActionScriptThinker::CONTINUE;
    }
};

class PushMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(ase._mapContext[LONG(*script->bytecodePos++)]);
        return ActionScriptThinker::CONTINUE;
    }
};

class PushWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(ase._worldContext[LONG(*script->bytecodePos++)]);
        return ActionScriptThinker::CONTINUE;
    }
};

class AddScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[LONG(*script->bytecodePos++)] += proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class AddMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[LONG(*script->bytecodePos++)] += proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class AddWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[LONG(*script->bytecodePos++)] += proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class SubScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[LONG(*script->bytecodePos++)] -= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class SubMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[LONG(*script->bytecodePos++)] -= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class SubWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[LONG(*script->bytecodePos++)] -= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class MulScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[LONG(*script->bytecodePos++)] *= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class MulMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[LONG(*script->bytecodePos++)] *= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class MulWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[LONG(*script->bytecodePos++)] *= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class DivScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[LONG(*script->bytecodePos++)] /= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class DivMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[LONG(*script->bytecodePos++)] /= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class DivWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[LONG(*script->bytecodePos++)] /= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class ModScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[LONG(*script->bytecodePos++)] %= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class ModMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[LONG(*script->bytecodePos++)] %= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class ModWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[LONG(*script->bytecodePos++)] %= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }
};

class IncScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[LONG(*script->bytecodePos++)]++;
        return ActionScriptThinker::CONTINUE;
    }
};

class IncMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[LONG(*script->bytecodePos++)]++;
        return ActionScriptThinker::CONTINUE;
    }
};

class IncWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[LONG(*script->bytecodePos++)]++;
        return ActionScriptThinker::CONTINUE;
    }
};

class DecScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[LONG(*script->bytecodePos++)]--;
        return ActionScriptThinker::CONTINUE;
    }
};

class DecMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[LONG(*script->bytecodePos++)]--;
        return ActionScriptThinker::CONTINUE;
    }
};

class DecWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[LONG(*script->bytecodePos++)]--;
        return ActionScriptThinker::CONTINUE;
    }
};

class GotoStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        script->bytecodePos = (dint*) (ase.bytecode().base + LONG(*script->bytecodePos));
        return ActionScriptThinker::CONTINUE;
    }
};

class IfGotoStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        if(proc->pop())
        {
            script->bytecodePos = (dint*) (ase.bytecode().base + LONG(*script->bytecodePos));
        }
        else
        {
            script->bytecodePos++;
        }
        return ActionScriptThinker::CONTINUE;
    }
};

class DropStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->drop();
        return ActionScriptThinker::CONTINUE;
    }
};

class DelayStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        script->delayCount = proc->pop();
        return ActionScriptThinker::STOP;
    }
};

class DelayDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        script->delayCount = LONG(*script->bytecodePos++);
        return ActionScriptThinker::STOP;
    }
};

class RandomStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint low, high;
        high = proc->pop();
        low = proc->pop();
        proc->push(low + (P_Random() % (high - low + 1)));
        return ActionScriptThinker::CONTINUE;
    }
};

class RandomDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint low, high;
        low = LONG(*script->bytecodePos++);
        high = LONG(*script->bytecodePos++);
        proc->push(low + (P_Random() % (high - low + 1)));
        return ActionScriptThinker::CONTINUE;
    }
};

class ThingCountStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint tid;
        tid = proc->pop();
        proc->push(P_CurrentMap().countThingsOfType(proc->pop(), tid));
        return ActionScriptThinker::CONTINUE;
    }
};

class ThingCountDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint type;
        type = LONG(*script->bytecodePos++);
        proc->push(P_CurrentMap().countThingsOfType(type, LONG(*script->bytecodePos++)));
        return ActionScriptThinker::CONTINUE;
    }
};

class TagWaitStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ActionScriptEnvironment::ScriptState& state = ase.scriptState(script->name);
        state.waitValue = proc->pop();
        state.status = ActionScriptEnvironment::ScriptState::WAITING_FOR_TAG;
        return ActionScriptThinker::STOP; 
    }
};

class TagWaitDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ActionScriptEnvironment::ScriptState& state = ase.scriptState(script->name);
        state.waitValue = LONG(*script->bytecodePos++);
        state.status = ActionScriptEnvironment::ScriptState::WAITING_FOR_TAG;
        return ActionScriptThinker::STOP;
    }
};

class PolyobjWaitStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ActionScriptEnvironment::ScriptState& state = ase.scriptState(script->name);
        state.waitValue = proc->pop();
        state.status = ActionScriptEnvironment::ScriptState::WAITING_FOR_POLYOBJ;
        return ActionScriptThinker::STOP;
    }
};

class PolyobjWaitDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ActionScriptEnvironment::ScriptState& state = ase.scriptState(script->name);
        state.waitValue = LONG(*script->bytecodePos++);
        state.status = ActionScriptEnvironment::ScriptState::WAITING_FOR_POLYOBJ;
        return ActionScriptThinker::STOP;
    }
};

class ChangeFloorStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        const dchar* flatName = ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(proc->pop()));
        dint tag = proc->pop();
        IterList* list;

        if((list = P_CurrentMap()->sectorIterListForTag(tag, false)))
        {
            Material* mat = P_MaterialForName(MN_FLATS, flatName);
            Sector* sec = NULL;

            list->resetIterator(true);
            while((sec = list->iterator()) != NULL)
            {
                DMU_SetPtrp(sec, DMU_FLOOR_MATERIAL, mat);
            }
        }

        return ActionScriptThinker::CONTINUE;
    }
};

class ChangeFloorDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint tag = LONG(*script->bytecodePos++);
        const dchar* flatName = ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(LONG(*script->bytecodePos++)));
        IterList* list;

        if((list = P_CurrentMap()->sectorIterListForTag(tag, false)))
        {
            Material* mat = P_MaterialForName(MN_FLATS, flatName);
            Sector* sec = NULL;

            list->resetIterator(true);
            while((sec = list->iterator()) != NULL)
            {
                DMU_SetPtrp(sec, DMU_FLOOR_MATERIAL, mat);
            }
        }

        return ActionScriptThinker::CONTINUE;
    }
};

class ChangeCeilingStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        const dchar* flatName = ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(proc->pop()));
        dint tag = proc->pop();
        IterList* list;

        if((list = P_CurrentMap()->sectorIterListForTag(tag, false)))
        {
            Material* mat = P_MaterialForName(MN_FLATS, flatName);
            Sector* sec = NULL;

            list->resetIterator(true);
            while((sec = list->iterator()) != NULL)
            {
                DMU_SetPtrp(sec, DMU_CEILING_MATERIAL, mat);
            }
        }

        return ActionScriptThinker::CONTINUE;
    }
};

class ChangeCeilingDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint tag = LONG(*script->bytecodePos++);
        const dchar* flatName = ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(LONG(*script->bytecodePos++)));
        IterList* list;

        if((list = P_CurrentMap()->sectorIterListForTag(tag, false)))
        {
            Material* mat = P_MaterialForName(MN_FLATS, flatName);
            Sector* sec = NULL;

            list->resetIterator(true);
            while((sec = list->iterator()) != NULL)
            {
                DMU_SetPtrp(sec, DMU_CEILING_MATERIAL, mat);
            }
        }

        return ActionScriptThinker::CONTINUE;
    }
};

class RestartStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        script->bytecodePos = ase.bytecode().function(script->name).entryPoint;
        return ActionScriptThinker::CONTINUE;
    }
};

class AndLogicalStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() && proc->pop());
        return ActionScriptThinker::CONTINUE;
    }
};

class OrLogicalStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() || proc->pop());
        return ActionScriptThinker::CONTINUE;
    }
};

class AndBitwiseStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() & proc->pop());
        return ActionScriptThinker::CONTINUE;
    }
};

class OrBitwiseStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() | proc->pop());
        return ActionScriptThinker::CONTINUE;
    }
};

class EorBitwiseStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() ^ proc->pop());
        return ActionScriptThinker::CONTINUE;
    }
};

class NegateLogicalStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(!proc->pop());
        return ActionScriptThinker::CONTINUE;
    }
};

class LShiftStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() << operand2);
        return ActionScriptThinker::CONTINUE;
    }
};

class RShiftStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() >> operand2);
        return ActionScriptThinker::CONTINUE;
    }
};

class UnaryMinusStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(-proc->pop());
        return ActionScriptThinker::CONTINUE;
    }
};

class IfNotGotoStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        if(proc->pop())
        {
            script->bytecodePos++;
        }
        else
        {
            script->bytecodePos = (dint*) (ase.bytecode().base + LONG(*script->bytecodePos));
        }
        return ActionScriptThinker::CONTINUE;
    }
};

class LineDefSideStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->_lineSide);
        return ActionScriptThinker::CONTINUE;
    }
};

class ScriptWaitStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ActionScriptEnvironment::ScriptState& state = ase.scriptState(script->name);
        state.waitValue = proc->pop();
        state.status = ActionScriptEnvironment::ScriptState::WAITING_FOR_SCRIPT;
        return ActionScriptThinker::STOP;
    }
};

class ScriptWaitDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ActionScriptEnvironment::ScriptState& state = ase.scriptState(script->name);
        state.waitValue = LONG(*script->bytecodePos++);
        state.status = ActionScriptEnvironment::ScriptState::WAITING_FOR_SCRIPT;
        return ActionScriptThinker::STOP;
    }
};

class ClearLineDefSpecialStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        if(proc->_lineDef)
        {
            P_ToXLine(proc->_lineDef)->special = 0;
        }
        return ActionScriptThinker::CONTINUE;
    }
};

class CaseGotoStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        if(proc->top() == LONG(*script->bytecodePos++))
        {
            script->bytecodePos = (dint*) (ase.bytecode().base + LONG(*script->bytecodePos));
            proc->drop();
        }
        else
        {
            script->bytecodePos++;
        }
        return ActionScriptThinker::CONTINUE;
    }
};

class BeginPrintStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._printBuffer[0] = 0;
        return ActionScriptThinker::CONTINUE;
    }
};

class EndPrintStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        if(proc->_activator && proc->_activator->player)
        {
            P_SetMessage(proc->_activator->player, ase._printBuffer, false);
        }
        else
        {   // Send to everybody.
            for(dint i = 0; i < MAXPLAYERS; ++i)
                if(players[i].plr->inGame)
                    P_SetMessage(&players[i], ase._printBuffer, false);
        }

        return ActionScriptThinker::CONTINUE;
    }
};

class PrintStringStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        strcat(ase._printBuffer, ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(proc->pop())));
        return ActionScriptThinker::CONTINUE;
    }
};

class PrintNumberStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dchar tempStr[16];
        sprintf(tempStr, "%d", proc->pop());
        strcat(ase._printBuffer, tempStr);
        return ActionScriptThinker::CONTINUE;
    }
};

class PrintCharacterStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dchar* bufferEnd;
        bufferEnd = ase._printBuffer + strlen(ase._printBuffer);
        *bufferEnd++ = proc->pop();
        *bufferEnd = 0;
        return ActionScriptThinker::CONTINUE;
    }
};

class PlayerCountStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint count = 0;
        for(dint i = 0; i < MAXPLAYERS; ++i)
        {
            count += players[i].plr->inGame;
        }
        proc->push(count);
        return ActionScriptThinker::CONTINUE;
    }
};

class GameTypeStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
#define GAMETYPE_SINGLE         0
#define GAMETYPE_COOPERATIVE    1
#define GAMETYPE_DEATHMATCH     2

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

        return ActionScriptThinker::CONTINUE;

#undef GAMETYPE_SINGLE         0
#undef GAMETYPE_COOPERATIVE    1
#undef GAMETYPE_DEATHMATCH     2
    }
};

class GameSkillStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(gameSkill);
        return ActionScriptThinker::CONTINUE;
    }
};

class TimerStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        GameMap* map = P_CurrentMap();
        proc->push(map->time);
        return ActionScriptThinker::CONTINUE;
    }
};

class SectorSoundStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint volume;
        Thing* th = NULL;

        if(proc->_lineDef)
        {
            th = DMU_GetPtrp(DMU_GetPtrp(proc->_lineDef, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
        }
        volume = proc->pop();

        S_StartSoundAtVolume(S_GetSoundID(ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(proc->pop()))), th, volume / 127.0f);
        return ActionScriptThinker::CONTINUE;
    }
};

class AmbientSoundStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
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

        sound = S_GetSoundID(ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(proc->pop())));
        S_StartSoundAtVolume(sound, th, volume / 127.0f);
        return ActionScriptThinker::CONTINUE;
    }
};

class SoundSequenceStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        Thing* th = NULL;
        if(proc->_lineDef)
            th = DMU_GetPtrp(DMU_GetPtrp(proc->_lineDef, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
        SN_StartSequenceName(mo, ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(proc->pop())));
        return ActionScriptThinker::CONTINUE;
    }
};

class SetSideDefMaterialStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
#define SIDEDEF_SECTION_TOP     0
#define SIDEDEF_SECTION_MIDDLE  1
#define SIDEDEF_SECTION_BOTTOM  2

        GameMap* map = P_CurrentMap();
        dint lineTag, side, position;
        Material* mat;
        LineDef* line;
        IterList* list;

        mat = P_MaterialForName(MN_TEXTURES, ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(proc->pop())));
        position = proc->pop();
        side = proc->pop();
        lineTag = proc->pop();

        list = map->iterListForTag(lineTag, false);
        if(list)
        {
            list->resetIterator(true);
            while((line = list->iterator()) != NULL)
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

        return ActionScriptThinker::CONTINUE;

#undef SIDEDEF_SECTION_TOP
#undef SIDEDEF_SECTION_MIDDLE
#undef SIDEDEF_SECTION_BOTTOM
    }
};

class SetLineDefBlockingStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
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
            list->resetIterator(true);
            while((line = list->iterator()) != NULL)
            {
                DMU_SetIntp(line, DMU_FLAGS,
                    (DMU_GetIntp(line, DMU_FLAGS) & ~DDLF_BLOCKING) | blocking);
            }
        }

        return ActionScriptThinker::CONTINUE;
    }
};

class SetLineDefSpecialStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
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
            list->resetIterator(true);
            while((line = list->iterator()) != NULL)
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

        return ActionScriptThinker::CONTINUE;
    }
};

class ThingSoundStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint tid, sound, volume, searcher;
        Thing* th;

        volume = proc->pop();
        sound = S_GetSoundID(ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(proc->pop())));
        tid = proc->pop();
        searcher = -1;
        while(sound && (th = P_FindMobjFromTID(P_CurrentMap(), tid, &searcher)) != NULL)
        {
            S_StartSoundAtVolume(sound, th, volume / 127.0f);
        }

        return ActionScriptThinker::CONTINUE;
    }
};

class EndPrintBoldStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        for(dint i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
            {
                P_SetYellowMessage(&players[i], ase._printBuffer, false);
            }
        }

        return ActionScriptThinker::CONTINUE;
    }
};

Statement* Statement::constructFrom(SerialId id /*Reader& reader*/)
{
    /* SerialId id;
    reader >> id;
    reader.rewind(sizeof(id)); */

    std::auto_ptr<Statement> result;
    switch(id)
    {
    case NOP:
        result.reset(new NOPStatement);
        break;

    case ActionScriptThinker::TERMINATE:
        result.reset(new TerminateStatement);
        break;

    case SUSPEND:
        result.reset(new SuspendStatement);
        break;

    case PUSHNUMBER:
        result.reset(new PushNumberStatement);
        break;

    case LSPEC1:
        result.reset(new LSpec1Statement);
        break;

    case LSPEC2:
        result.reset(new LSpec2Statement);
        break;

    case LSPEC3:
        result.reset(new LSpec3Statement);
        break;

    case LSPEC4:
        result.reset(new LSpec4Statement);
        break;

    case LSPEC5:
        result.reset(new LSpec5Statement);
        break;

    case LSPEC1DIRECT:
        result.reset(new LSpec1DirectStatement);
        break;

    case LSPEC2DIRECT:
        result.reset(new LSpec2DirectStatement);
        break;

    case LSPEC3DIRECT:
        result.reset(new LSpec3DirectStatement);
        break;

    case LSPEC4DIRECT:
        result.reset(new LSpec4DirectStatement);
        break;

    case LSPEC5DIRECT:
        result.reset(new LSpec5DirectStatement);
        break;

    case ADD:
        result.reset(new AddStatement);
        break;

    case SUBTRACT:
        result.reset(new SubtractStatement);
        break;

    case MULTIPLY:
        result.reset(new MultiplyStatement);
        break;

    case DIVIDE:
        result.reset(new DivideStatement);
        break;

    case MODULUS:
        result.reset(new ModulusStatement);
        break;

    case EQ:
        result.reset(new EQStatement);
        break;

    case NE:
        result.reset(new NEStatement);
        break;

    case LT:
        result.reset(new LTStatement);
        break;

    case GT:
        result.reset(new GTStatement);
        break;

    case LE:
        result.reset(new LEStatement);
        break;

    case GE:
        result.reset(new GEStatement);
        break;

    case ASSIGNSCRIPTVAR:
        result.reset(new AssignScriptVarStatement);
        break;

    case ASSIGNMAPVAR:
        result.reset(new AssignMapVarStatement);
        break;

    case ASSIGNWORLDVAR:
        result.reset(new AssignWorldVarStatement);
        break;

    case PUSHSCRIPTVAR:
        result.reset(new PushScriptVarStatement);
        break;

    case PUSHMAPVAR:
        result.reset(new PushMapVarStatement);
        break;

    case PUSHWORLDVAR:
        result.reset(new PushWorldVarStatement);
        break;

    case ADDSCRIPTVAR:
        result.reset(new AddScriptVarStatement);
        break;

    case ADDMAPVAR:
        result.reset(new AddMapVarStatement);
        break;

    case ADDWORLDVAR:
        result.reset(new AddWorldVarStatement);
        break;

    case SUBSCRIPTVAR:
        result.reset(new SubScriptVarStatement);
        break;

    case SUBMAPVAR:
        result.reset(new SubMapVarStatement);
        break;

    case SUBWORLDVAR:
        result.reset(new SubWorldVarStatement);
        break;

    case MULSCRIPTVAR:
        result.reset(new MulScriptVarStatement);
        break;

    case MULMAPVAR:
        result.reset(new MulMapVarStatement);
        break;

    case MULWORLDVAR:
        result.reset(new MulWorldVarStatement);
        break;

    case DIVSCRIPTVAR:
        result.reset(new DivScriptVarStatement);
        break;

    case DIVMAPVAR:
        result.reset(new DivMapVarStatement);
        break;

    case DIVWORLDVAR:
        result.reset(new DivWorldVarStatement);
        break;

    case MODSCRIPTVAR:
        result.reset(new ModScriptVarStatement);
        break;

    case MODMAPVAR:
        result.reset(new ModMapVarStatement);
        break;

    case MODWORLDVAR:
        result.reset(new ModWorldVarStatement);
        break;

    case INCSCRIPTVAR:
        result.reset(new IncScriptVarStatement);
        break;

    case INCMAPVAR:
        result.reset(new IncMapVarStatement);
        break;

    case INCWORLDVAR:
        result.reset(new IncWorldVarStatement);
        break;

    case DECSCRIPTVAR:
        result.reset(new DecScriptVarStatement);
        break;

    case DECMAPVAR:
        result.reset(new DecMapVarStatement);
        break;

    case DECWORLDVAR:
        result.reset(new DecWorldVarStatement);
        break;

    case GOTO:
        result.reset(new GotoStatement);
        break;

    case IFGOTO:
        result.reset(new IfGotoStatement);
        break;

    case DROP:
        result.reset(new DropStatement);
        break;

    case DELAY:
        result.reset(new DelayStatement);
        break;

    case DELAYDIRECT:
        result.reset(new DelayDirectStatement);
        break;

    case RANDOM:
        result.reset(new RandomStatement);
        break;

    case RANDOMDIRECT:
        result.reset(new RandomDirectStatement);
        break;

    case THINGCOUNT:
        result.reset(new ThingCountStatement);
        break;

    case THINGCOUNTDIRECT:
        result.reset(new ThingCountDirectStatement);
        break;

    case TAGWAIT:
        result.reset(new TagWaitStatement);
        break;

    case TAGWAITDIRECT:
        result.reset(new TagWaitDirectStatement);
        break;

    case POLYOBJWAIT:
        result.reset(new PolyobjWaitStatement);
        break;

    case POLYOBJWAITDIRECT:
        result.reset(new PolyobjWaitDirectStatement);
        break;

    case CHANGEFLOOR:
        result.reset(new ChangeFloorStatement);
        break;

    case CHANGEFLOORDIRECT:
        result.reset(new ChangeFloorDirectStatement);
        break;

    case CHANGECEILING:
        result.reset(new ChangeCeilingStatement);
        break;

    case CHANGECEILINGDIRECT:
        result.reset(new ChangeCeilingDirectStatement);
        break;

    case RESTART:
        result.reset(new RestartStatement);
        break;

    case ANDLOGICAL:
        result.reset(new AndLogicalStatement);
        break;

    case ORLOGICAL:
        result.reset(new OrLogicalStatement);
        break;

    case ANDBITWISE:
        result.reset(new AndBitwiseStatement);
        break;

    case ORBITWISE:
        result.reset(new OrBitwiseStatement);
        break;

    case EORBITWISE:
        result.reset(new EorBitwiseStatement);
        break;

    case NEGATELOGICAL:
        result.reset(new NegateLogicalStatement);
        break;

    case LSHIFT:
        result.reset(new LShiftStatement);
        break;

    case RSHIFT:
        result.reset(new RShiftStatement);
        break;

    case UNARYMINUS:
        result.reset(new UnaryMinusStatement);
        break;

    case IFNOTGOTO:
        result.reset(new IfNotGotoStatement);
        break;

    case LINEDEFSIDE:
        result.reset(new LineDefSideStatement);
        break;

    case SCRIPTWAIT:
        result.reset(new ScriptWaitStatement);
        break;

    case SCRIPTWAITDIRECT:
        result.reset(new ScriptWaitDirectStatement);
        break;

    case CLEARLINEDEFSPECIAL:
        result.reset(new ClearLineDefSpecialStatement);
        break;

    case CASEGOTO:
        result.reset(new CaseGotoStatement);
        break;

    case BEGINPRINT:
        result.reset(new BeginPrintStatement);
        break;

    case ENDPRINT:
        result.reset(new EndPrintStatement);
        break;

    case PRINTSTRING:
        result.reset(new PrintStringStatement);
        break;

    case PRINTNUMBER:
        result.reset(new PrintNumberStatement);
        break;

    case PRINTCHARACTER:
        result.reset(new PrintCharacterStatement);
        break;

    case PLAYERCOUNT:
        result.reset(new PlayerCountStatement);
        break;

    case GAMETYPE:
        result.reset(new GameTypeStatement);
        break;

    case GAMESKILL:
        result.reset(new GameSkillStatement);
        break;

    case TIMER:
        result.reset(new TimerStatement);
        break;

    case SECTORSOUND:
        result.reset(new SectorSoundStatement);
        break;

    case AMBIENTSOUND:
        result.reset(new AmbientSoundStatement);
        break;

    case SOUNDSEQUENCE:
        result.reset(new SoundSequenceStatement);
        break;

    case SETSIDEDEFMATERIAL:
        result.reset(new SetSideDefMaterialStatement);
        break;

    case SETLINEDEFBLOCKING:
        result.reset(new SetLineDefBlockingStatement);
        break;

    case SETLINEDEFSPECIAL:
        result.reset(new SetLineDefSpecialStatement);
        break;

    case THINGSOUND:
        result.reset(new ThingSoundStatement);
        break;

    case ENDPRINTBOLD:
        result.reset(new EndPrintBoldStatement);
        break;
                
    default:
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized statement was invalid.
        throw DeserializationError("Statement::constructFrom", "Invalid statement identifier");
    }

    // Deserialize it.
    //reader >> *result.get();
    return result.release();    
}
