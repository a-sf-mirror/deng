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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != NOP)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("NOPStatement::operator <<", "Invalid ID");
        }
    }
};

class TerminateStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        return ActionScriptThinker::TERMINATE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != TERMINATE)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("TerminateStatement::operator <<", "Invalid ID");
        }
    }
};

class SuspendStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase.scriptState(script->name).status = ActionScriptEnvironment::ScriptState::SUSPENDED;
        return ActionScriptThinker::STOP;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SUSPEND)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("SuspendStatement::operator <<", "Invalid ID");
        }
    }
};

class PushNumberStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(_numberArgument);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _numberArgument;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != PUSHNUMBER)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("PushNumberStatement::operator <<", "Invalid ID");
        }

        from >> _numberArgument;
    }
};

class LSpec1Statement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        args[0] = proc->pop();
        P_ExecuteLineSpecial(_specialArgument, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _specialArgument;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LSPEC1)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LSpec1Statement::operator <<", "Invalid ID");
        }

        from >> _specialArgument;
    }
};

class LSpec2Statement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        args[1] = proc->pop();
        args[0] = proc->pop();
        P_ExecuteLineSpecial(_specialArgument, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _specialArgument;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LSPEC2)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LSpec2Statement::operator <<", "Invalid ID");
        }

        from >> _specialArgument;
    }
};

class LSpec3Statement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        args[2] = proc->pop();
        args[1] = proc->pop();
        args[0] = proc->pop();
        P_ExecuteLineSpecial(_specialArgument, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _specialArgument;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LSPEC3)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LSpec3Statement::operator <<", "Invalid ID");
        }

        from >> _specialArgument;
    }
};

class LSpec4Statement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        args[3] = proc->pop();
        args[2] = proc->pop();
        args[1] = proc->pop();
        args[0] = proc->pop();
        P_ExecuteLineSpecial(_specialArgument, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _specialArgument;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LSPEC4)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LSpec4Statement::operator <<", "Invalid ID");
        }

        from >> _specialArgument;
    }
};

class LSpec5Statement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        args[4] = proc->pop();
        args[3] = proc->pop();
        args[2] = proc->pop();
        args[1] = proc->pop();
        args[0] = proc->pop();
        P_ExecuteLineSpecial(_specialArgument, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _specialArgument;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LSPEC5)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LSpec5Statement::operator <<", "Invalid ID");
        }

        from >> _specialArgument;
    }
};

class LSpec1DirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        args[0] = dbyte(_arg0Argument);
        P_ExecuteLineSpecial(_specialArgument, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _specialArgument;
    dint32 _arg0Argument;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LSPEC1DIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LSpec1DirectStatement::operator <<", "Invalid ID");
        }

        from >> _specialArgument
             >> _arg0Argument;
    }
};

class LSpec2DirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        args[0] = dbyte(_arg0Argument);
        args[1] = dbyte(_arg1Argument);
        P_ExecuteLineSpecial(_specialArgument, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _specialArgument;
    dint32 _arg0Argument;
    dint32 _arg1Argument;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LSPEC2DIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LSpec2DirectStatement::operator <<", "Invalid ID");
        }

        from >> _specialArgument
             >> _arg0Argument
             >> _arg1Argument;
    }
};

class LSpec3DirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        args[0] = dbyte(_arg0Argument);
        args[1] = dbyte(_arg1Argument);
        args[2] = dbyte(_arg2Argument);
        P_ExecuteLineSpecial(_specialArgument, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _specialArgument;
    dint32 _arg0Argument;
    dint32 _arg1Argument;
    dint32 _arg2Argument;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LSPEC3DIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LSpec3DirectStatement::operator <<", "Invalid ID");
        }

        from >> _specialArgument
             >> _arg0Argument
             >> _arg1Argument
             >> _arg2Argument;
    }
};

class LSpec4DirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        args[0] = dbyte(_arg0Argument);
        args[1] = dbyte(_arg1Argument);
        args[2] = dbyte(_arg2Argument);
        args[3] = dbyte(_arg3Argument);
        P_ExecuteLineSpecial(_specialArgument, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _specialArgument;
    dint32 _arg0Argument;
    dint32 _arg1Argument;
    dint32 _arg2Argument;
    dint32 _arg3Argument;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LSPEC4DIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LSpec4DirectStatement::operator <<", "Invalid ID");
        }

        from >> _specialArgument
             >> _arg0Argument
             >> _arg1Argument
             >> _arg2Argument
             >> _arg3Argument;
    }
};

class LSpec5DirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dbyte args[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        args[0] = dbyte(_arg0Argument);
        args[1] = dbyte(_arg1Argument);
        args[2] = dbyte(_arg2Argument);
        args[3] = dbyte(_arg3Argument);
        args[4] = dbyte(_arg4Argument);
        P_ExecuteLineSpecial(_specialArgument, args, proc->_lineDef, proc->_lineSide, proc->_activator);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _specialArgument;
    dint32 _arg0Argument;
    dint32 _arg1Argument;
    dint32 _arg2Argument;
    dint32 _arg3Argument;
    dint32 _arg4Argument;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LSPEC5DIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LSpec5DirectStatement::operator <<", "Invalid ID");
        }

        from >> _specialArgument
             >> _arg0Argument
             >> _arg1Argument
             >> _arg2Argument
             >> _arg3Argument
             >> _arg4Argument;
    }
};

class AddStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() + proc->pop());
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ADD)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("AddStatement::operator <<", "Invalid ID");
        }
    }
};

class SubtractStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() - operand2);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SUBTRACT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("SubtractStatement::operator <<", "Invalid ID");
        }
    }
};

class MultiplyStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() * proc->pop());
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != MULTIPLY)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("MultiplyStatement::operator <<", "Invalid ID");
        }
    }
};

class DivideStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() / operand2);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != DIVIDE)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("DivideStatement::operator <<", "Invalid ID");
        }
    }
};

class ModulusStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() % operand2);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != MODULUS)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ModulusStatement::operator <<", "Invalid ID");
        }
    }
};

class EQStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() == proc->pop());
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != EQ)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("EQStatement::operator <<", "Invalid ID");
        }
    }
};

class NEStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() != proc->pop());
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != NE)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("NEStatement::operator <<", "Invalid ID");
        }
    }
};

class LTStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() < operand2);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LTStatement::operator <<", "Invalid ID");
        }
    }
};

class GTStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        int operand2 = proc->pop();
        proc->push(proc->pop() > operand2);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != GT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("GTStatement::operator <<", "Invalid ID");
        }
    }
};

class LEStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() <= operand2);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LE)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LEStatement::operator <<", "Invalid ID");
        }
    }
};

class GEStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() >= operand2);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != GE)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("GEStatement::operator <<", "Invalid ID");
        }
    }
};

class AssignScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[_variableName] = proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _variableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ASSIGNSCRIPTVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("AssignScriptVarStatement::operator <<", "Invalid ID");
        }

        from >> _variableName;
    }
};

class AssignMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[_mapVariableName] = proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _mapVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ASSIGNMAPVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("AssignMapVarStatement::operator <<", "Invalid ID");
        }

        from >> _mapVariableName;
    }
};

class AssignWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[_worldVariableName] = proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _worldVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ASSIGNWORLDVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("AssignWorldVarStatement::operator <<", "Invalid ID");
        }

        from >> _worldVariableName;
    }
};

class PushScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->_context[_variableName]);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _variableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != PUSHSCRIPTVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("PushScriptVarStatement::operator <<", "Invalid ID");
        }

        from >> _variableName;
    }
};

class PushMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(ase._mapContext[_mapVariableName]);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _mapVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != PUSHMAPVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("PushMapVarStatement::operator <<", "Invalid ID");
        }

        from >> _mapVariableName;
    }
};

class PushWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(ase._worldContext[_worldVariableName]);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _worldVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != PUSHWORLDVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("PushWorldVarStatement::operator <<", "Invalid ID");
        }

        from >> _worldVariableName;
    }
};

class AddScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[_variableName] += proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _variableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ADDSCRIPTVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("AddScriptVarStatement::operator <<", "Invalid ID");
        }

        from >> _variableName;
    }
};

class AddMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[_mapVariableName] += proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _mapVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ADDMAPVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("AddMapVarStatement::operator <<", "Invalid ID");
        }

        from >> _mapVariableName;
    }
};

class AddWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[_worldVariableName] += proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _worldVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ADDWORLDVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("AddWorldVarStatement::operator <<", "Invalid ID");
        }

        from >> _worldVariableName;
    }
};

class SubScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[_variableName] -= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _variableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SUBSCRIPTVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("SubScriptVarStatement::operator <<", "Invalid ID");
        }

        from >> _variableName;
    }
};

class SubMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[_mapVariableName] -= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _mapVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SUBMAPVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("SubMapVarStatement::operator <<", "Invalid ID");
        }

        from >> _mapVariableName;
    }
};

class SubWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[_worldVariableName] -= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _worldVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SUBWORLDVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("SubWorldVarStatement::operator <<", "Invalid ID");
        }

        from >> _worldVariableName;
    }
};

class MulScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[_variableName] *= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _variableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != MULSCRIPTVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("MulScriptVarStatement::operator <<", "Invalid ID");
        }

        from >> _variableName;
    }
};

class MulMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[_mapVariableName] *= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _mapVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != MULMAPVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("MulMapVarStatement::operator <<", "Invalid ID");
        }

        from >> _mapVariableName;
    }
};

class MulWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[_worldVariableName] *= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _worldVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != MULWORLDVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("MulWorldVarStatement::operator <<", "Invalid ID");
        }

        from >> _worldVariableName;
    }
};

class DivScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[_variableName] /= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _variableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != DIVSCRIPTVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("DivScriptVarStatement::operator <<", "Invalid ID");
        }

        from >> _variableName;
    }
};

class DivMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[_mapVariableName] /= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _mapVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != DIVMAPVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("DivMapVarStatement::operator <<", "Invalid ID");
        }

        from >> _mapVariableName;
    }
};

class DivWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[_worldVariableName] /= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _worldVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != DIVWORLDVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("DivWorldVarStatement::operator <<", "Invalid ID");
        }

        from >> _worldVariableName;
    }
};

class ModScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[_variableName] %= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _variableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != MODSCRIPTVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ModScriptVarStatement::operator <<", "Invalid ID");
        }

        from >> _variableName;
    }
};

class ModMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[_mapVariableName] %= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _mapVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != MODMAPVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ModMapVarStatement::operator <<", "Invalid ID");
        }

        from >> _mapVariableName;
    }
};

class ModWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[_worldVariableName] %= proc->pop();
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _worldVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != MODWORLDVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ModWorldVarStatement::operator <<", "Invalid ID");
        }

        from >> _worldVariableName;
    }
};

class IncScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[_variableName]++;
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _variableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != INCSCRIPTVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("IncScriptVarStatement::operator <<", "Invalid ID");
        }

        from >> _variableName;
    }
};

class IncMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[_mapVariableName]++;
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _mapVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != INCMAPVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("IncMapVarStatement::operator <<", "Invalid ID");
        }

        from >> _mapVariableName;
    }
};

class IncWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[_worldVariableName]++;
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _worldVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != INCWORLDVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("IncWorldVarStatement::operator <<", "Invalid ID");
        }

        from >> _worldVariableName;
    }
};

class DecScriptVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->_context[_variableName]--;
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _variableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != DECSCRIPTVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("DecScriptVarStatement::operator <<", "Invalid ID");
        }

        from >> _variableName;
    }
};

class DecMapVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._mapContext[_mapVariableName]--;
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _mapVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != DECMAPVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("DecMapVarStatement::operator <<", "Invalid ID");
        }

        from >> _mapVariableName;
    }
};

class DecWorldVarStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._worldContext[_worldVariableName]--;
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _worldVariableName;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != DECWORLDVAR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("DecWorldVarStatement::operator <<", "Invalid ID");
        }

        from >> _worldVariableName;
    }
};

class GotoStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        script->bytecodePos = static_cast<IByteArray::Offset>(_bytecodeOffset);
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _bytecodeOffset;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != GOTO)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("GotoStatement::operator <<", "Invalid ID");
        }

        from >> _bytecodeOffset;
    }
};

class IfGotoStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        if(proc->pop())
        {
            script->bytecodePos = static_cast<IByteArray::Offset>(_bytecodeOffset);
        }

        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _bytecodeOffset;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != IFGOTO)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("IfGotoStatement::operator <<", "Invalid ID");
        }

        from >> _bytecodeOffset;
    }
};

class DropStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->drop();
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != DROP)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("DropStatement::operator <<", "Invalid ID");
        }
    }
};

class DelayStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        script->delayCount = proc->pop();
        return ActionScriptThinker::STOP;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != DELAY)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("DelayStatement::operator <<", "Invalid ID");
        }
    }
};

class DelayDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        script->delayCount = _delayCount;
        return ActionScriptThinker::STOP;
    }

public:
    dint32 _delayCount;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != DELAYDIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("DelayDirectStatement::operator <<", "Invalid ID");
        }

        from >> _delayCount;
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != RANDOM)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("RandomStatement::operator <<", "Invalid ID");
        }
    }
};

class RandomDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(_low + (P_Random() % (_high - _low + 1)));
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _low;
    dint32 _high;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != RANDOMDIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("RandomDirectStatement::operator <<", "Invalid ID");
        }

        from >> _low
             >> _high;
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != THINGCOUNT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ThingCountStatement::operator <<", "Invalid ID");
        }
    }
};

class ThingCountDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(P_CurrentMap().countThingsOfType(_type, _tid));
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _type;
    dint32 _tid;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != THINGCOUNTDIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ThingCountDirectStatement::operator <<", "Invalid ID");
        }

        from >> _type
             >> _tid;
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != TAGWAIT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("TagWaitStatement::operator <<", "Invalid ID");
        }
    }
};

class TagWaitDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ActionScriptEnvironment::ScriptState& state = ase.scriptState(script->name);
        state.waitValue = _waitValue;
        state.status = ActionScriptEnvironment::ScriptState::WAITING_FOR_TAG;
        return ActionScriptThinker::STOP;
    }

public:
    dint32 _waitValue;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != TAGWAITDIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("TagWaitDirectStatement::operator <<", "Invalid ID");
        }

        from >> _waitValue;
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != POLYOBJWAIT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("PolyobjWaitStatement::operator <<", "Invalid ID");
        }
    }
};

class PolyobjWaitDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ActionScriptEnvironment::ScriptState& state = ase.scriptState(script->name);
        state.waitValue = _waitValue;
        state.status = ActionScriptEnvironment::ScriptState::WAITING_FOR_POLYOBJ;
        return ActionScriptThinker::STOP;
    }

public:
    dint32 _waitValue;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != POLYOBJWAITDIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("PolyobjWaitDirectStatement::operator <<", "Invalid ID");
        }

        from >> _waitValue;
    }
};

class ChangeFloorStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        const String& flatName = ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(proc->pop()));
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != CHANGEFLOOR)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ChangeFloorStatement::operator <<", "Invalid ID");
        }
    }
};

class ChangeFloorDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        const String& flatName = ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(_stringId));
        IterList* list;

        if((list = P_CurrentMap()->sectorIterListForTag(_tag, false)))
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

public:
    dint32 _tag;
    dint32 _stringId;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != CHANGEFLOORDIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ChangeFloorDirectStatement::operator <<", "Invalid ID");
        }

        from >> _tag
             >> _stringId;
    }
};

class ChangeCeilingStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        const String& flatName = ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(proc->pop()));
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != CHANGECEILING)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ChangeCeilingStatement::operator <<", "Invalid ID");
        }
    }
};

class ChangeCeilingDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        const String& flatName = ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(_stringId));
        IterList* list;

        if((list = P_CurrentMap()->sectorIterListForTag(_tag, false)))
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

public:
    dint32 _tag;
    dint32 _stringId;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != CHANGECEILINGDIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ChangeCeilingDirectStatement::operator <<", "Invalid ID");
        }

        from >> _tag
             >> _stringId;
    }
};

class RestartStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        script->bytecodePos = ase.bytecode().function(script->name).entryPoint;
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != RESTART)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("RestartStatement::operator <<", "Invalid ID");
        }
    }
};

class AndLogicalStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() && proc->pop());
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ANDLOGICAL)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("AndLogicalStatement::operator <<", "Invalid ID");
        }
    }
};

class OrLogicalStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() || proc->pop());
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ORLOGICAL)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("OrLogicalStatement::operator <<", "Invalid ID");
        }
    }
};

class AndBitwiseStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() & proc->pop());
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ANDBITWISE)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("AndBitwiseStatement::operator <<", "Invalid ID");
        }
    }
};

class OrBitwiseStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() | proc->pop());
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ORBITWISE)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("OrBitwiseStatement::operator <<", "Invalid ID");
        }
    }
};

class EOrBitwiseStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->pop() ^ proc->pop());
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != EORBITWISE)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("EOrBitwiseStatement::operator <<", "Invalid ID");
        }
    }
};

class NegateLogicalStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(!proc->pop());
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != NEGATELOGICAL)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("NegateLogicalStatement::operator <<", "Invalid ID");
        }
    }
};

class LShiftStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() << operand2);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LSHIFT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LShiftStatement::operator <<", "Invalid ID");
        }
    }
};

class RShiftStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        dint operand2 = proc->pop();
        proc->push(proc->pop() >> operand2);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != RSHIFT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("RShiftStatement::operator <<", "Invalid ID");
        }
    }
};

class UnaryMinusStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(-proc->pop());
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != UNARYMINUS)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("UnaryMinusStatement::operator <<", "Invalid ID");
        }
    }
};

class IfNotGotoStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        if(!proc->pop())
        {
            script->bytecodePos = (dint*) (ase.bytecode().base + _bytecodeOffset);
        }
        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _bytecodeOffset;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != IFNOTGOTO)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("IfNotGotoStatement::operator <<", "Invalid ID");
        }

        from >> _bytecodeOffset;
    }
};

class LineDefSideStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(proc->_lineSide);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != LINEDEFSIDE)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("LineDefSideStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SCRIPTWAIT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ScriptWaitStatement::operator <<", "Invalid ID");
        }
    }
};

class ScriptWaitDirectStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ActionScriptEnvironment::ScriptState& state = ase.scriptState(script->name);
        state.waitValue = _waitValue;
        state.status = ActionScriptEnvironment::ScriptState::WAITING_FOR_SCRIPT;
        return ActionScriptThinker::STOP;
    }

public:
    dint32 _waitValue;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SCRIPTWAITDIRECT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ScriptWaitDirectStatement::operator <<", "Invalid ID");
        }

        from >> _waitValue;
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != CLEARLINEDEFSPECIAL)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ClearLineDefSpecialStatement::operator <<", "Invalid ID");
        }
    }
};

class CaseGotoStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        if(proc->top() == _caseValue)
        {
            script->bytecodePos = (dint*) (ase.bytecode().base + _bytecodeOffset);
            proc->drop();
        }

        return ActionScriptThinker::CONTINUE;
    }

public:
    dint32 _caseValue;
    dint32 _bytecodeOffset;

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != CASEGOTO)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("CaseGotoStatement::operator <<", "Invalid ID");
        }

        from >> _caseValue
             >> _bytecodeOffset;
    }
};

class BeginPrintStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        ase._printBuffer[0] = 0;
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != BEGINPRINT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("BeginPrintStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ENDPRINT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("EndPrintStatement::operator <<", "Invalid ID");
        }
    }
};

class PrintStringStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        strcat(ase._printBuffer, ase.bytecode().string(static_cast<ActionScriptBytecodeInterpreter::StringId>(proc->pop())));
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != PRINTSTRING)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("PrintStringStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != PRINTNUMBER)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("PrintNumberStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != PRINTCHARACTER)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("PrintCharacterStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != PLAYERCOUNT)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("PlayerCountStatement::operator <<", "Invalid ID");
        }
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

#undef GAMETYPE_SINGLE
#undef GAMETYPE_COOPERATIVE
#undef GAMETYPE_DEATHMATCH
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != GAMETYPE)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("GameTypeStatement::operator <<", "Invalid ID");
        }
    }
};

class GameSkillStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        proc->push(gameSkill);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != GAMESKILL)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("GameSkillStatement::operator <<", "Invalid ID");
        }
    }
};

class TimerStatement : public Statement
{
    ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const {
        GameMap* map = P_CurrentMap();
        proc->push(map->time);
        return ActionScriptThinker::CONTINUE;
    }

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != TIMER)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("TimerStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SECTORSOUND)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("SectorSoundStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != AMBIENTSOUND)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("AmbientSoundStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SOUNDSEQUENCE)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("SoundSequenceStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SETSIDEDEFMATERIAL)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("SetSideDefMaterialStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SETLINEDEFBLOCKING)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("SetLineDefBlockingStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != SETLINEDEFSPECIAL)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("SetLineDefSpecialStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != THINGSOUND)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("ThingSoundStatement::operator <<", "Invalid ID");
        }
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

    void operator << (Reader& from)
    {
        SerialId id;
        from >> id;
        if(id != ENDPRINTBOLD)
        {
            /// @throw DeserializationError The identifier that species the type of the 
            /// serialized statement was invalid.
            throw DeserializationError("EndPrintBoldStatement::operator <<", "Invalid ID");
        }
    }
};

Statement* Statement::constructFrom(Reader& reader)
{
    SerialId id;
    reader >> id;
    reader.rewind(sizeof(id));

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
        result.reset(new EOrBitwiseStatement);
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
    reader >> *result.get();
    return result.release();    
}
