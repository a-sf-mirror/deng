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

namespace {
// A handy helper for declaring action script commands.
#define D_ASCMD(x) script_action_t Cmd##x(actionscript_thinker_t* script)

#define GAMETYPE_SINGLE         0
#define GAMETYPE_COOPERATIVE    1
#define GAMETYPE_DEATHMATCH     2

#define SIDEDEF_SECTION_TOP     0
#define SIDEDEF_SECTION_MIDDLE  1
#define SIDEDEF_SECTION_BOTTOM  2

typedef enum {
    SS_INACTIVE,
    SS_RUNNING,
    SS_SUSPENDED,
    SS_WAITING_FOR_TAG,
    SS_WAITING_FOR_POLY,
    SS_WAITING_FOR_SCRIPT,
    SS_TERMINATING
} script_state_t;

typedef enum {
    SA_CONTINUE,
    SA_STOP,
    SA_TERMINATE
} script_action_t;

// Pointer to a script bytecode command interpreter
typedef script_action_t (*script_bytecode_cmdinterpreter_t) (actionscript_thinker_t*);

typedef int script_bytecode_stringid_t;

typedef struct script_info_s {
    actionscriptid_t scriptId;
    const int*      entryPoint;
    int             argCount;
    script_state_t  scriptState;
    int             waitValue;
} script_info_t;

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
D_ASCMD(PolyWait);
D_ASCMD(PolyWaitDirect);
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
D_ASCMD(MobjSound);
D_ASCMD(EndPrintBold);

static const script_bytecode_cmdinterpreter_t bytecodeCommandInterpreters[] =
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
    CmdMobjCount, CmdMobjCountDirect, CmdTagWait, CmdTagWaitDirect,
    CmdPolyWait, CmdPolyWaitDirect, CmdChangeFloor,
    CmdChangeFloorDirect, CmdChangeCeiling, CmdChangeCeilingDirect,
    CmdRestart, CmdAndLogical, CmdOrLogical, CmdAndBitwise,
    CmdOrBitwise, CmdEorBitwise, CmdNegateLogical, CmdLShift,
    CmdRShift, CmdUnaryMinus, CmdIfNotGoto, CmdLineDefSide, CmdScriptWait,
    CmdScriptWaitDirect, CmdClearLineDefSpecial, CmdCaseGoto,
    CmdBeginPrint, CmdEndPrint, CmdPrintString, CmdPrintNumber,
    CmdPrintCharacter, CmdPlayerCount, CmdGameType, CmdGameSkill,
    CmdTimer, CmdSectorSound, CmdAmbientSound, CmdSoundSequence,
    CmdSetSideDefMaterial, CmdSetLineDefBlocking, CmdSetLineDefSpecial,
    CmdMobjSound, CmdEndPrintBold
};
}

ActionScriptInterpreter::ActionScriptInterpreter()
{}

ActionScriptInterpreter::~ActionScriptInterpreter()
{
    unloadBytecode();

    if(_scriptStore)
        free(_scriptStore);
}

actionscript_thinker_t* ActionScriptInterpreter::createActionScriptThinker(GameMap* map,
    actionscriptid_t scriptId, const dint* bytecodePos, dint delayCount,
    dint infoIndex, Thing* activator, LineDef* lineDef, dint lineSide,
    const dbyte* args, dint numArgs)
{
    actionscript_thinker_t* script;

    script = Z_Calloc(sizeof(*script), PU_MAP, 0);
    script->thinker.function = ActionScriptThinker_Think;
    Map_ThinkerAdd(map, (thinker_t*) script);

    script->scriptId = scriptId;
    script->bytecodePos = bytecodePos;
    script->delayCount = delayCount;
    script->infoIndex = infoIndex;
    script->activator = activator;
    script->lineDef = lineDef;
    script->lineSide = lineSide;

    if(args)
    {
        for(dint i = 0; i < numArgs; ++i)
            script->vars[i] = args[i];
    }

    return script;
}

bool ActionScriptInterpreter::startScript(actionscriptid_t scriptId, duint map,
    dbyte* args, Thing* activator, LineDef* lineDef, dint lineSide,
    actionscript_thinker_t** newScript)
{
    actionscript_thinker_t* script;
    script_info_t* info;
    dint infoIndex;

    if(newScript)
        *newScript = NULL;

    if(map && map-1 != gameMap)
    {   // Add to the script store.
        return addToScriptStore(scriptId, map, args);
    }

    infoIndex = indexForScriptId(scriptId);
    if(infoIndex == -1)
    {   // Script not found.
        dchar buf[128];
        sprintf(buf, "ACS.StartScript: Warning, unknown script #%d", scriptId);
        P_SetMessage(&players[CONSOLEPLAYER], buf, false);
        return false;
    }

    info = &_bytecode.scriptInfo[infoIndex];

    if(info->scriptState == SS_SUSPENDED)
    {   // Resume a suspended script.
        info->scriptState = SS_RUNNING;
        return true;
    }

    if(info->scriptState != SS_INACTIVE)
    {   // Script is already executing.
        return false;
    }

    script = createActionScriptThinker(P_CurrentMap(), scriptId, info->entryPoint,
        0, infoIndex, activator, lineDef, lineSide, args, info->argCount);

    info->scriptState = SS_RUNNING;
    if(newScript)
        *newScript = script;
    return true;
}

static __inline void Drop(actionscript_thinker_t* script)
{
    script->stackDepth--;
}

static __inline dint Pop(actionscript_thinker_t* script)
{
    return script->stack[--script->stackDepth];
}

static __inline void Push(actionscript_thinker_t* script, dint value)
{
    script->stack[script->stackDepth++] = value;
}

static __inline dint Top(actionscript_thinker_t* script)
{
    return script->stack[script->stackDepth - 1];
}

static const dchar* getString(ActionScriptInterpreter::Bytecode* bytecode, script_bytecode_stringid_t id)
{
    assert(id >= 0 && id < bytecode->numStrings);
    return bytecode->strings[id];
}

void ActionScriptInterpreter::unloadBytecode()
{
    if(_bytecode.base)
        Z_Free((void*) _bytecode.base); _bytecode.base = NULL;

    if(_bytecode.scriptInfo)
        Z_Free(_bytecode.scriptInfo); _bytecode.scriptInfo = NULL;
    _bytecode.numScripts = 0;

    if(_bytecode.strings)
        Z_Free((void*) _bytecode.strings); _bytecode.strings = NULL;
    _bytecode.numStrings = 0;
}

namespace {
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

    _bytecode.numScripts = numScripts;
    _bytecode.numStrings = numStrings;
    _bytecode.base = (const dbyte*) W_CacheLumpNum(lumpNum, PU_STATIC);

    // Load the script info.
    dsize infoOffset = (dsize) LONG(*((dint32*) (_bytecode.base + 4)));
    _bytecode.scriptInfo = Z_Malloc(_bytecode.numScripts * sizeof(script_info_t), PU_STATIC, 0);

    const dbyte* ptr = (_bytecode.base + infoOffset + 4);
    for(dint i = 0; i < _bytecode.numScripts; ++i, ptr += 12)
    {
        script_info_t* info = &_bytecode.scriptInfo[i];
        dint scriptId = (dint) LONG(*((dint32*) (ptr)));
        dsize entryPointOffset = (dsize) LONG(*((dint32*) (ptr + 4)));
        dint argCount = (dint) LONG(*((dint32*) (ptr + 8)));

        info->entryPoint = (const dint*) (_bytecode.base + entryPointOffset);
        info->argCount = argCount;
        info->waitValue = 0;

        if(scriptId >= OPEN_SCRIPTS_BASE)
        {   // Auto-activate
            info->scriptId = scriptId - OPEN_SCRIPTS_BASE;
            // World objects are allotted 1 second for initialization.
            createActionScriptThinker(P_CurrentMap(), info->scriptId,
                info->entryPoint, TICSPERSEC, i, NULL, NULL, 0, NULL, 0);
            info->scriptState = SS_RUNNING;
        }
        else
        {
            info->scriptId = (actionscriptid_t) scriptId;
            info->scriptState = SS_INACTIVE;
        }
    }

    // Load the string offsets.
    if(_bytecode.numStrings > 0)
    {
        _bytecode.strings = Z_Malloc(_bytecode.numStrings * sizeof(dchar*), PU_STATIC, 0);

        const dbyte* ptr = (_bytecode.base + infoOffset + 4 + numScripts * 12 + 4);
        for(dint i = 0; i < _bytecode.numStrings; ++i, ptr += 4)
        {
            _bytecode.strings[i] = (const dchar*) _bytecode.base + LONG(*((dint32*) (ptr)));
        }
    }

#undef OPEN_SCRIPTS_BASE
}

void ActionScriptInterpreter::writeWorldState() const
{
    SV_WriteByte(3); // Version byte.

    for(dint i = 0; i < MAX_WORLD_VARS; ++i)
        SV_WriteLong(_worldVars[i]);

    SV_WriteLong(_scriptStoreSize);
    for(dint i = 0; i < _scriptStoreSize; ++i)
    {
        const script_store_t* store = &_scriptStore[i];

        SV_WriteLong(store->map);
        SV_WriteLong((dint) store->scriptId);
        for(dint j = 0; j < 4; ++j)
            SV_WriteByte(store->args[j]);
    }
}

void ActionScriptInterpreter::readWorldState()
{
    dint ver = (saveVersion >= 7)? SV_ReadByte() : 1;

    for(dint i = 0; i < MAX_WORLD_VARS; ++i)
        _worldVars[i] = SV_ReadLong();

    if(ver >= 3)
    {
        _scriptStoreSize = SV_ReadLong();
        if(_scriptStoreSize)
        {
            if(_scriptStore)
                _scriptStore = realloc(_scriptStore, sizeof(script_store_t) * _scriptStoreSize);
            else
                _scriptStore = malloc(sizeof(script_store_t) * _scriptStoreSize);

            for(dint i = 0; i < _scriptStoreSize; ++i)
            {
                script_store_t* store = &_scriptStore[i];

                store->map = SV_ReadLong();
                store->scriptId = SV_ReadLong();
                for(dint j = 0; j < 4; ++j)
                    store->args[j] = SV_ReadByte();
            }
        }
    }
    else
    {   // Old format.
        script_store_t tempStore[20];

        _scriptStoreSize = 0;
        for(dint i = 0; i < 20; ++i)
        {
            dint map = SV_ReadLong();
            script_store_t* store = &tempStore[map < 0? 19 : _scriptStoreSize++];

            store->map = map < 0? 0 : map-1;
            store->scriptId = SV_ReadLong();
            for(dint j = 0; j < 4; ++j)
                store->args[j] = SV_ReadByte();
        }

        if(saveVersion < 7)
        {
            dbyte junk[2];
            SV_Read(junk, 12);
        }

        if(_scriptStoreSize)
        {
            if(_scriptStore)
                _scriptStore = realloc(_scriptStore, sizeof(script_store_t) * _scriptStoreSize);
            else
                _scriptStore = malloc(sizeof(script_store_t) * _scriptStoreSize);
            memcpy(_scriptStore, tempStore, sizeof(script_store_t) * _scriptStoreSize);
        }
    }

    if(!_scriptStoreSize && _scriptStore)
    {
        free(_scriptStore); _scriptStore = NULL;
    }
}

void ActionScriptInterpreter::writeMapState()
{
    for(dint i = 0; i < _bytecode.numScripts; ++i)
    {
        const script_info_t* info = &_bytecode.scriptInfo[i];
        SV_WriteShort(info->scriptState);
        SV_WriteShort(info->waitValue);
    }

    for(dint i = 0; i < MAX_MAP_VARS; ++i)
        SV_WriteLong(_mapVars[i]);
}

void ActionScriptInterpreter::readMapState()
{
    for(dint i = 0; i < _bytecode.numScripts; ++i)
    {
        script_info_t* info = &_bytecode.scriptInfo[i];
        info->scriptState = SV_ReadShort();
        info->waitValue = SV_ReadShort();
    }

    for(dint i = 0; i < MAX_MAP_VARS; ++i)
        _mapVars[i] = SV_ReadLong();
}

void ActionScriptThinker::write() const
{
    SV_WriteByte(1); // Write a version byte.

    SV_WriteLong(SV_ThingArchiveNum(activator));
    SV_WriteLong(lineDef ? DMU_ToIndex(lineDef) : -1);
    SV_WriteLong(lineSide);
    SV_WriteLong((dint) scriptId);
    SV_WriteLong(infoIndex);
    SV_WriteLong(delayCount);
    for(duint i = 0; i < AST_STACK_DEPTH; ++i)
        SV_WriteLong(stack[i]);
    SV_WriteLong(stackDepth);
    for(duint i = 0; i < AST_MAX_VARS; ++i)
        SV_WriteLong(vars[i]);

    SV_WriteLong((dint) (bytecodePos) - (dint) ActionScriptInterpreter->_bytecode.base);
}

dint ActionScriptThinker::read()
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
        scriptId = (actionscriptid_t) SV_ReadLong();
        infoIndex = SV_ReadLong();
        delayCount = SV_ReadLong();
        for(duint i = 0; i < AST_STACK_DEPTH; ++i)
            stack[i] = SV_ReadLong();
        stackDepth = SV_ReadLong();
        for(duint i = 0; i < AST_MAX_VARS; ++i)
            vars[i] = SV_ReadLong();

        bytecodePos = (const dint*) (ActionScriptInterpreter->_bytecode.base + SV_ReadLong());
    }
    else
    {
        // Its in the old pre V4 format which serialized actionscript_thinker_t
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
        scriptId = (actionscriptid_t) SV_ReadLong();
        infoIndex = SV_ReadLong();
        delayCount = SV_ReadLong();
        for(duint i = 0; i < AST_STACK_DEPTH; ++i)
            stack[i] = SV_ReadLong();
        stackDepth = SV_ReadLong();
        for(duint i = 0; i < AST_MAX_VARS; ++i)
            vars[i] = SV_ReadLong();

        bytecodePos = (const dint*) (ActionScriptInterpreter->_bytecode.base + SV_ReadLong());
    }

    thinker.function = ActionScriptThinker_Think;

    return true; // Add this thinker.
}

void ActionScriptInterpreter::startAll(duint map)
{
    dint i = 0, origSize = _scriptStoreSize;
    while(i < _scriptStoreSize)
    {
        script_store_t* store = &_scriptStore[i];
        actionscript_thinker_t* newScript;

        if(store->map != map)
        {
            i++;
            continue;
        }

        startScript(store->scriptId, 0, store->args, NULL, NULL, 0, &newScript);
        if(newScript)
        {
            newScript->delayCount = TICSPERSEC;
        }

        _scriptStoreSize -= 1;
        if(i == _scriptStoreSize)
            break;
        memmove(&_scriptStore[i], &_scriptStore[i+1], sizeof(script_store_t) * (_scriptStoreSize-i));
    }

    if(_scriptStoreSize != origSize)
    {
        if(_scriptStoreSize)
            _scriptStore = realloc(_scriptStore, sizeof(script_store_t) * _scriptStoreSize);
        else
            free(_scriptStore); _scriptStore = NULL;
    }
}

bool ActionScriptInterpreter::start(actionscriptid_t scriptId, duint map, dbyte* args,
    Thing* activator, LineDef* lineDef, dint side)
{
    return startScript(scriptId, map, args, activator, lineDef, side, NULL);
}

bool ActionScriptInterpreter::addToScriptStore(actionscriptid_t scriptId, duint map,
    dbyte* args)
{
    script_store_t* store;

    if(_scriptStoreSize)
    {
        // Don't allow duplicates.
        for(dint i = 0; i < _scriptStoreSize; ++i)
        {
            store = &_scriptStore[i];
            if(store->scriptId == scriptId && store->map == map)
                return false;
        }

        _scriptStore = realloc(_scriptStore, ++_scriptStoreSize * sizeof(script_store_t));
    }
    else
    {
        _scriptStore = malloc(sizeof(script_store_t));
        _scriptStoreSize = 1;
    }

    store = &_scriptStore[_scriptStoreSize-1];

    store->map = map;
    store->scriptId = scriptId;
    store->args[0] = args[0];
    store->args[1] = args[1];
    store->args[2] = args[2];
    store->args[3] = args[3];

    return true;
}

bool ActionScriptInterpreter::stop(actionscriptid_t scriptId, duint map)
{
    script_info_t* info;
    dint infoIndex = indexForScriptId(scriptId);

    if(infoIndex == -1)
    {   // Script not found.
        return false;
    }

    info = &_bytecode.scriptInfo[infoIndex];

    if(info->scriptState == SS_INACTIVE || info->scriptState == SS_TERMINATING)
    {   // States that disallow termination.
        return false;
    }

    info->scriptState = SS_TERMINATING;
    return true;
}

bool ActionScriptInterpreter::suspend(actionscriptid_t scriptId, duint map)
{
    script_info_t* info;
    dint infoIndex = indexForScriptId(scriptId);
    if(infoIndex == -1)
    {   // Script not found.
        return false;
    }

    info = &_bytecode.scriptInfo[infoIndex];

    if(info->scriptState == SS_INACTIVE ||
       info->scriptState == SS_SUSPENDED ||
       info->scriptState == SS_TERMINATING)
    {   // States that disallow suspension.
        return false;
    }

    info->scriptState = SS_SUSPENDED;
    return true;
}

void ActionScriptThinker::think()
{
    ActionScriptInterpreter* asi = ActionScriptInterpreter;
    script_info_t* info = &asi->_bytecode.scriptInfo[infoIndex];
    script_action_t action;

    if(info->scriptState == SS_TERMINATING)
    {
        info->scriptState = SS_INACTIVE;
        scriptFinished(asi, scriptId);
        Map_RemoveThinker(Thinker_Map(this), this);
        return;
    }

    if(info->scriptState != SS_RUNNING)
    {
        return;
    }

    if(delayCount)
    {
        delayCount--;
        return;
    }

    do
    {
        script_bytecode_cmdinterpreter_t cmd = bytecodeCommandInterpreters[LONG(*bytecodePos++)];
        action = cmd(this);
    } while(action == SA_CONTINUE);

    if(action == SA_TERMINATE)
    {
        info->scriptState = SS_INACTIVE;
        scriptFinished(asi, scriptId);
        Map_RemoveThinker(Thinker_Map(this), this);
    }
}

void ActionScriptInterpreter::tagFinished(dint tag)
{
    if(!tagBusy(tag))
    {
        for(dint i = 0; i < _bytecode.numScripts; ++i)
        {
            script_info_t* info = &_bytecode.scriptInfo[i];

            if(info->scriptState == SS_WAITING_FOR_TAG && info->waitValue == tag)
            {
                info->scriptState = SS_RUNNING;
            }
        }
    }
}

void ActionScriptInterpreter::polyobjFinished(dint po)
{
    if(!PO_Busy(P_CurrentMap(), po))
    {
        for(dint i = 0; i < _bytecode.numScripts; ++i)
        {
            script_info_t* info = &_bytecode.scriptInfo[i];

            if(info->scriptState == SS_WAITING_FOR_POLY && info->waitValue == po)
            {
                info->scriptState = SS_RUNNING;
            }
        }
    }
}

void ActionScriptInterpreter::scriptFinished(actionscriptid_t scriptId)
{
    for(dint i = 0; i < _bytecode.numScripts; ++i)
    {
        script_info_t* info = &_bytecode.scriptInfo[i];

        if(info->scriptState == SS_WAITING_FOR_SCRIPT && info->waitValue == scriptId)
        {
            info->scriptState = SS_RUNNING;
        }
    }
}

bool ActionScriptInterpreter::tagBusy(dint tag)
{
    GameMap* map = P_CurrentMap();

    // @note We can't use the sector tag lists here as we might already be in an
    // iteration at a higher level.
    for(duint i = 0; i < map->numSectors(); ++i)
    {
        Sector* sec = DMU_ToPtr(DMU_SECTOR, i);
        XSector* xsec = P_ToXSector(sec);

        if(xsec->tag != tag)
            continue;

        if(xsec->specialData)
            return true;
    }

    return false;
}

dint ActionScriptInterpreter::indexForScriptId(actionscriptid_t scriptId)
{
    for(dint i = 0; i < _bytecode.numScripts; ++i)
        if(_bytecode.scriptInfo[i].scriptId == scriptId)
            return i;
    return -1;
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
    ActionScriptInterpreter->_bytecode.scriptInfo[script->infoIndex].scriptState = SS_SUSPENDED;
    return SA_STOP;
}

D_ASCMD(PushNumber)
{
    Push(script, LONG(*script->bytecodePos++));
    return SA_CONTINUE;
}

D_ASCMD(LSpec1)
{
    dint special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[0] = Pop(script);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->_specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec2)
{
    dint special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[1] = Pop(script);
    ActionScriptInterpreter->_specArgs[0] = Pop(script);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->_specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec3)
{
    dint special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[2] = Pop(script);
    ActionScriptInterpreter->_specArgs[1] = Pop(script);
    ActionScriptInterpreter->_specArgs[0] = Pop(script);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->_specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec4)
{
    dint special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[3] = Pop(script);
    ActionScriptInterpreter->_specArgs[2] = Pop(script);
    ActionScriptInterpreter->_specArgs[1] = Pop(script);
    ActionScriptInterpreter->_specArgs[0] = Pop(script);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->_specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec5)
{
    dint special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[4] = Pop(script);
    ActionScriptInterpreter->_specArgs[3] = Pop(script);
    ActionScriptInterpreter->_specArgs[2] = Pop(script);
    ActionScriptInterpreter->_specArgs[1] = Pop(script);
    ActionScriptInterpreter->_specArgs[0] = Pop(script);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->_specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec1Direct)
{
    dint special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[0] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->_specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec2Direct)
{
    dint special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[0] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[1] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->_specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec3Direct)
{
    dint special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[0] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[1] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[2] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->_specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec4Direct)
{
    dint special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[0] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[1] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[2] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[3] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->_specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

D_ASCMD(LSpec5Direct)
{
    dint special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[0] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[1] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[2] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[3] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->_specArgs[4] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->_specArgs, script->lineDef, script->lineSide, script->activator);
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
    ActionScriptInterpreter->_mapVars[LONG(*script->bytecodePos++)] = Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(AssignWorldVar)
{
    ActionScriptInterpreter->_worldVars[LONG(*script->bytecodePos++)] = Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(PushScriptVar)
{
    Push(script, script->vars[LONG(*script->bytecodePos++)]);
    return SA_CONTINUE;
}

D_ASCMD(PushMapVar)
{
    Push(script, ActionScriptInterpreter->_mapVars[LONG(*script->bytecodePos++)]);
    return SA_CONTINUE;
}

D_ASCMD(PushWorldVar)
{
    Push(script, ActionScriptInterpreter->_worldVars[LONG(*script->bytecodePos++)]);
    return SA_CONTINUE;
}

D_ASCMD(AddScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] += Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(AddMapVar)
{
    ActionScriptInterpreter->_mapVars[LONG(*script->bytecodePos++)] += Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(AddWorldVar)
{
    ActionScriptInterpreter->_worldVars[LONG(*script->bytecodePos++)] += Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(SubScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] -= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(SubMapVar)
{
    ActionScriptInterpreter->_mapVars[LONG(*script->bytecodePos++)] -= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(SubWorldVar)
{
    ActionScriptInterpreter->_worldVars[LONG(*script->bytecodePos++)] -= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(MulScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] *= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(MulMapVar)
{
    ActionScriptInterpreter->_mapVars[LONG(*script->bytecodePos++)] *= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(MulWorldVar)
{
    ActionScriptInterpreter->_worldVars[LONG(*script->bytecodePos++)] *= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(DivScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] /= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(DivMapVar)
{
    ActionScriptInterpreter->_mapVars[LONG(*script->bytecodePos++)] /= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(DivWorldVar)
{
    ActionScriptInterpreter->_worldVars[LONG(*script->bytecodePos++)] /= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(ModScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] %= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(ModMapVar)
{
    ActionScriptInterpreter->_mapVars[LONG(*script->bytecodePos++)] %= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(ModWorldVar)
{
    ActionScriptInterpreter->_worldVars[LONG(*script->bytecodePos++)] %= Pop(script);
    return SA_CONTINUE;
}

D_ASCMD(IncScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)]++;
    return SA_CONTINUE;
}

D_ASCMD(IncMapVar)
{
    ActionScriptInterpreter->_mapVars[LONG(*script->bytecodePos++)]++;
    return SA_CONTINUE;
}

D_ASCMD(IncWorldVar)
{
    ActionScriptInterpreter->_worldVars[LONG(*script->bytecodePos++)]++;
    return SA_CONTINUE;
}

D_ASCMD(DecScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)]--;
    return SA_CONTINUE;
}

D_ASCMD(DecMapVar)
{
    ActionScriptInterpreter->_mapVars[LONG(*script->bytecodePos++)]--;
    return SA_CONTINUE;
}

D_ASCMD(DecWorldVar)
{
    ActionScriptInterpreter->_worldVars[LONG(*script->bytecodePos++)]--;
    return SA_CONTINUE;
}

D_ASCMD(Goto)
{
    script->bytecodePos = (dint*) (ActionScriptInterpreter->_bytecode.base + LONG(*script->bytecodePos));
    return SA_CONTINUE;
}

D_ASCMD(IfGoto)
{
    if(Pop(script))
    {
        script->bytecodePos = (dint*) (ActionScriptInterpreter->_bytecode.base + LONG(*script->bytecodePos));
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
    script_info_t* info = &ActionScriptInterpreter->_bytecode.scriptInfo[script->infoIndex];
    info->waitValue = Pop(script);
    info->scriptState = SS_WAITING_FOR_TAG;
    return SA_STOP;
}

D_ASCMD(TagWaitDirect)
{
    script_info_t* info = &ActionScriptInterpreter->_bytecode.scriptInfo[script->infoIndex];
    info->waitValue = LONG(*script->bytecodePos++);
    info->scriptState = SS_WAITING_FOR_TAG;
    return SA_STOP;
}

D_ASCMD(PolyWait)
{
    script_info_t* info = &ActionScriptInterpreter->_bytecode.scriptInfo[script->infoIndex];
    info->waitValue = Pop(script);
    info->scriptState = SS_WAITING_FOR_POLY;
    return SA_STOP;
}

D_ASCMD(PolyWaitDirect)
{
    script_info_t* info = &ActionScriptInterpreter->_bytecode.scriptInfo[script->infoIndex];
    info->waitValue = LONG(*script->bytecodePos++);
    info->scriptState = SS_WAITING_FOR_POLY;
    return SA_STOP;
}

D_ASCMD(ChangeFloor)
{
    const dchar* flatName = getString(&ActionScriptInterpreter->_bytecode, (script_bytecode_stringid_t) Pop(script));
    dint tag = Pop(script);
    iterlist_t* list;

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
    const dchar* flatName = getString(&ActionScriptInterpreter->_bytecode, (script_bytecode_stringid_t) LONG(*script->bytecodePos++));
    iterlist_t* list;

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
    const dchar* flatName = getString(&ActionScriptInterpreter->_bytecode, (script_bytecode_stringid_t) Pop(script));
    dint tag = Pop(script);
    iterlist_t* list;

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
    const dchar* flatName = getString(&ActionScriptInterpreter->_bytecode, (script_bytecode_stringid_t) LONG(*script->bytecodePos++));
    iterlist_t* list;

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
    script->bytecodePos = ActionScriptInterpreter->_bytecode.scriptInfo[script->infoIndex].entryPoint;
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
        script->bytecodePos = (dint*) (ActionScriptInterpreter->_bytecode.base + LONG(*script->bytecodePos));
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
    script_info_t* info = &ActionScriptInterpreter->_bytecode.scriptInfo[script->infoIndex];
    info->waitValue = Pop(script);
    info->scriptState = SS_WAITING_FOR_SCRIPT;
    return SA_STOP;
}

D_ASCMD(ScriptWaitDirect)
{
    script_info_t* info = &ActionScriptInterpreter->_bytecode.scriptInfo[script->infoIndex];
    info->waitValue = LONG(*script->bytecodePos++);
    info->scriptState = SS_WAITING_FOR_SCRIPT;
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
        script->bytecodePos = (dint*) (ActionScriptInterpreter->_bytecode.base + LONG(*script->bytecodePos));
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
    ActionScriptInterpreter->_printBuffer[0] = 0;
    return SA_CONTINUE;
}

D_ASCMD(EndPrint)
{
    if(script->activator && script->activator->player)
    {
        P_SetMessage(script->activator->player, ActionScriptInterpreter->_printBuffer, false);
    }
    else
    {   // Send to everybody.
        for(dint i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame)
                P_SetMessage(&players[i], ActionScriptInterpreter->_printBuffer, false);
    }

    return SA_CONTINUE;
}

D_ASCMD(EndPrintBold)
{
    for(dint i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            P_SetYellowMessage(&players[i], ActionScriptInterpreter->_printBuffer, false);
        }
    }

    return SA_CONTINUE;
}

D_ASCMD(PrintString)
{
    strcat(ActionScriptInterpreter->_printBuffer, getString(&ActionScriptInterpreter->_bytecode, (script_bytecode_stringid_t) Pop(script)));
    return SA_CONTINUE;
}

D_ASCMD(PrintNumber)
{
    dchar tempStr[16];
    sprintf(tempStr, "%d", Pop(script));
    strcat(ActionScriptInterpreter->_printBuffer, tempStr);
    return SA_CONTINUE;
}

D_ASCMD(PrintCharacter)
{
    dchar* bufferEnd;
    bufferEnd = ActionScriptInterpreter->_printBuffer + strlen(ActionScriptInterpreter->_printBuffer);
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

    S_StartSoundAtVolume(S_GetSoundID(getString(&ActionScriptInterpreter->_bytecode, (script_bytecode_stringid_t) Pop(script))), th, volume / 127.0f);
    return SA_CONTINUE;
}

D_ASCMD(MobjSound)
{
    dint tid, sound, volume, searcher;
    Thing* th;

    volume = Pop(script);
    sound = S_GetSoundID(getString(&ActionScriptInterpreter->_bytecode, (script_bytecode_stringid_t) Pop(script)));
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

    sound = S_GetSoundID(getString(&ActionScriptInterpreter->_bytecode, (script_bytecode_stringid_t) Pop(script)));
    S_StartSoundAtVolume(sound, th, volume / 127.0f);

    return SA_CONTINUE;
}

D_ASCMD(SoundSequence)
{
    Thing* th = NULL;
    if(script->lineDef)
        th = DMU_GetPtrp(DMU_GetPtrp(script->lineDef, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
    SN_StartSequenceName(mo, getString(&ActionScriptInterpreter->_bytecode, (script_bytecode_stringid_t) Pop(script)));
    return SA_CONTINUE;
}

D_ASCMD(SetSideDefMaterial)
{
    GameMap* map = P_CurrentMap();
    dint lineTag, side, position;
    Material* mat;
    LineDef* line;
    iterlist_t* list;

    mat = P_MaterialForName(MN_TEXTURES, getString(&ActionScriptInterpreter->_bytecode, (script_bytecode_stringid_t) Pop(script)));
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
    iterlist_t* list;

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
    iterlist_t* list;

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

void ActionScriptInterpreter::printScriptInfo(actionscriptid_t scriptId)
{
    if(!_bytecode.numScripts)
        return;

    static const char* scriptStateDescriptions[] = {
        "Inactive",
        "Running",
        "Suspended",
        "Waiting for tag",
        "Waiting for poly",
        "Waiting for script",
        "Terminating"
    };

    for(dint i = 0; i < _bytecode.numScripts; ++i)
    {
        const script_info_t* info = &_bytecode.scriptInfo[i];

        if(scriptId != -1 && scriptId != info->scriptId)
            continue;

        LOG_MESSAGE("") << info->scriptId << " "
            << scriptStateDescriptions[info->scriptState] << " (a: " << info->argCount
            << ", w: " << info->waitValue << ")";
    }
}

DEFCC(CCmdScriptInfo)
{
    if(ActionScriptInterpreter)
    {
        actionscriptid_t scriptId = -1;

        if(argc == 2)
            scriptId = (actionscriptid_t) atoi(argv[1]);
        else
            scriptId = -1;

        printScriptInfo(ActionScriptInterpreter, scriptId);
    }

    return true;
}
