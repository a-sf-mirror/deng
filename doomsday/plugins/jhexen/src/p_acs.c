/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_acs.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "gamemap.h"
#include "dmu_lib.h"
#include "p_player.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"
#include "p_saveg.h"

// MACROS ------------------------------------------------------------------

// A handy helper for declaring action script commands.
#define D_ASCMD(x) script_action_t Cmd##x(actionscript_thinker_t* script)

#define GAMETYPE_SINGLE         0
#define GAMETYPE_COOPERATIVE    1
#define GAMETYPE_DEATHMATCH     2

#define SIDEDEF_SECTION_TOP     0
#define SIDEDEF_SECTION_MIDDLE  1
#define SIDEDEF_SECTION_BOTTOM  2

// TYPES -------------------------------------------------------------------

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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void scriptFinished(actionscriptinterpreter_t* asi, actionscriptid_t scriptId);
static boolean addToScriptStore(actionscriptinterpreter_t* asi, actionscriptid_t scriptId, uint map,  byte* args);
static int indexForScriptId(actionscriptinterpreter_t* asi, actionscriptid_t scriptId);

static boolean tagBusy(int tag);
static int countMobjsOfType(map_t* map, int type, int tid);

static D_ASCMD(NOP);
static D_ASCMD(Terminate);
static D_ASCMD(Suspend);
static D_ASCMD(PushNumber);
static D_ASCMD(LSpec1);
static D_ASCMD(LSpec2);
static D_ASCMD(LSpec3);
static D_ASCMD(LSpec4);
static D_ASCMD(LSpec5);
static D_ASCMD(LSpec1Direct);
static D_ASCMD(LSpec2Direct);
static D_ASCMD(LSpec3Direct);
static D_ASCMD(LSpec4Direct);
static D_ASCMD(LSpec5Direct);
static D_ASCMD(Add);
static D_ASCMD(Subtract);
static D_ASCMD(Multiply);
static D_ASCMD(Divide);
static D_ASCMD(Modulus);
static D_ASCMD(EQ);
static D_ASCMD(NE);
static D_ASCMD(LT);
static D_ASCMD(GT);
static D_ASCMD(LE);
static D_ASCMD(GE);
static D_ASCMD(AssignScriptVar);
static D_ASCMD(AssignMapVar);
static D_ASCMD(AssignWorldVar);
static D_ASCMD(PushScriptVar);
static D_ASCMD(PushMapVar);
static D_ASCMD(PushWorldVar);
static D_ASCMD(AddScriptVar);
static D_ASCMD(AddMapVar);
static D_ASCMD(AddWorldVar);
static D_ASCMD(SubScriptVar);
static D_ASCMD(SubMapVar);
static D_ASCMD(SubWorldVar);
static D_ASCMD(MulScriptVar);
static D_ASCMD(MulMapVar);
static D_ASCMD(MulWorldVar);
static D_ASCMD(DivScriptVar);
static D_ASCMD(DivMapVar);
static D_ASCMD(DivWorldVar);
static D_ASCMD(ModScriptVar);
static D_ASCMD(ModMapVar);
static D_ASCMD(ModWorldVar);
static D_ASCMD(IncScriptVar);
static D_ASCMD(IncMapVar);
static D_ASCMD(IncWorldVar);
static D_ASCMD(DecScriptVar);
static D_ASCMD(DecMapVar);
static D_ASCMD(DecWorldVar);
static D_ASCMD(Goto);
static D_ASCMD(IfGoto);
static D_ASCMD(Drop);
static D_ASCMD(Delay);
static D_ASCMD(DelayDirect);
static D_ASCMD(Random);
static D_ASCMD(RandomDirect);
static D_ASCMD(MobjCount);
static D_ASCMD(MobjCountDirect);
static D_ASCMD(TagWait);
static D_ASCMD(TagWaitDirect);
static D_ASCMD(PolyWait);
static D_ASCMD(PolyWaitDirect);
static D_ASCMD(ChangeFloor);
static D_ASCMD(ChangeFloorDirect);
static D_ASCMD(ChangeCeiling);
static D_ASCMD(ChangeCeilingDirect);
static D_ASCMD(Restart);
static D_ASCMD(AndLogical);
static D_ASCMD(OrLogical);
static D_ASCMD(AndBitwise);
static D_ASCMD(OrBitwise);
static D_ASCMD(EorBitwise);
static D_ASCMD(NegateLogical);
static D_ASCMD(LShift);
static D_ASCMD(RShift);
static D_ASCMD(UnaryMinus);
static D_ASCMD(IfNotGoto);
static D_ASCMD(LineDefSide);
static D_ASCMD(ScriptWait);
static D_ASCMD(ScriptWaitDirect);
static D_ASCMD(ClearLineDefSpecial);
static D_ASCMD(CaseGoto);
static D_ASCMD(BeginPrint);
static D_ASCMD(EndPrint);
static D_ASCMD(PrintString);
static D_ASCMD(PrintNumber);
static D_ASCMD(PrintCharacter);
static D_ASCMD(PlayerCount);
static D_ASCMD(GameType);
static D_ASCMD(GameSkill);
static D_ASCMD(Timer);
static D_ASCMD(SectorSound);
static D_ASCMD(AmbientSound);
static D_ASCMD(SoundSequence);
static D_ASCMD(SetSideDefMaterial);
static D_ASCMD(SetLineDefBlocking);
static D_ASCMD(SetLineDefSpecial);
static D_ASCMD(MobjSound);
static D_ASCMD(EndPrintBold);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

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

actionscriptinterpreter_t* ActionScriptInterpreter = NULL;

// CODE --------------------------------------------------------------------

static actionscript_thinker_t* createActionScriptThinker(map_t* map,
    actionscriptid_t scriptId, const int* bytecodePos, int delayCount,
    int infoIndex, mobj_t* activator, linedef_t* lineDef, int lineSide,
    const byte* args, int numArgs)
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
        int i;
        for(i = 0; i < numArgs; ++i)
        {
            script->vars[i] = args[i];
        }
    }

    return script;
}

static boolean startScript(actionscriptinterpreter_t* asi,
    actionscriptid_t scriptId, uint map, byte* args, mobj_t* activator,
    linedef_t* lineDef, int lineSide, actionscript_thinker_t** newScript)
{
    actionscript_thinker_t* script;
    script_info_t* info;
    int infoIndex;

    if(newScript)
        *newScript = NULL;

    if(map && map-1 != gameMap)
    {   // Add to the script store.
        return addToScriptStore(asi, scriptId, map, args);
    }

    infoIndex = indexForScriptId(asi, scriptId);
    if(infoIndex == -1)
    {   // Script not found.
        char buf[128];
        sprintf(buf, "ACS.StartScript: Warning, unknown script #%d", scriptId);
        P_SetMessage(&players[CONSOLEPLAYER], buf, false);
        return false;
    }

    info = &asi->bytecode.scriptInfo[infoIndex];

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

static __inline int Pop(actionscript_thinker_t* script)
{
    return script->stack[--script->stackDepth];
}

static __inline void Push(actionscript_thinker_t* script, int value)
{
    script->stack[script->stackDepth++] = value;
}

static __inline int Top(actionscript_thinker_t* script)
{
    return script->stack[script->stackDepth - 1];
}

static const char* getString(script_bytecode_t* bytecode, script_bytecode_stringid_t id)
{
    assert(id >= 0 && id < bytecode->numStrings);
    return bytecode->strings[id];
}

static void unloadBytecode(actionscriptinterpreter_t* asi)
{
    if(asi->bytecode.base)
        Z_Free((void*) asi->bytecode.base);
    asi->bytecode.base = NULL;

    if(asi->bytecode.scriptInfo)
        Z_Free(asi->bytecode.scriptInfo);
    asi->bytecode.scriptInfo = NULL;
    asi->bytecode.numScripts = 0;

    if(asi->bytecode.strings)
        Z_Free((void*) asi->bytecode.strings);
    asi->bytecode.strings = NULL;
    asi->bytecode.numStrings = 0;
}

actionscriptinterpreter_t* P_CreateActionScriptInterpreter(void)
{
    // Ensure only one instance (i.e., Singleton pattern).
    assert(!ActionScriptInterpreter);
    {
    actionscriptinterpreter_t* asi = Z_Calloc(sizeof(*asi), PU_STATIC, 0);
    ActionScriptInterpreter = asi;
    return asi;
    }
}

void P_DestroyActionScriptInterpreter(actionscriptinterpreter_t* asi)
{
    assert(asi);
    unloadBytecode(asi);
    if(asi->scriptStore)
        free(asi->scriptStore);
    Z_Free(asi);
    // Only one instance (i.e., Singleton pattern).
    ActionScriptInterpreter = NULL;
}

static boolean isValidACSBytecode(lumpnum_t lumpNum, int* numScripts, int* numStrings)
{
    size_t lumpLength, infoOffset;
    byte buff[12];

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

void ActionScriptInterpreter_Load(actionscriptinterpreter_t* asi, uint map, lumpnum_t lumpNum)
{
    assert(asi);
    {
#define OPEN_SCRIPTS_BASE       1000

    int numScripts, numStrings;
    size_t infoOffset;

    Con_Message("Load ACS scripts\n");

    if(asi->bytecode.base)
        unloadBytecode(asi);
    memset(asi->mapVars, 0, sizeof(asi->mapVars));

    if(IS_CLIENT)
        return;

    if(!isValidACSBytecode(lumpNum, &numScripts, &numStrings))
    {
        Con_Message("ActionScriptInterpreter_Load: Warning, lump #%i not valid "
                    "ACS bytecode.\n", (int) lumpNum);
        return;
    }
    if(numScripts == 0)
        return; // Empty lump.

    asi->bytecode.base = (const byte*) W_CacheLumpNum(lumpNum, PU_STATIC);
    infoOffset = (size_t) LONG(*((int32_t*) (asi->bytecode.base + 4)));
    asi->bytecode.numScripts = numScripts;
    asi->bytecode.numStrings = numStrings;

    // Load the script info.
    {
    int i;
    const byte* ptr;

    asi->bytecode.scriptInfo = Z_Malloc(asi->bytecode.numScripts * sizeof(script_info_t), PU_STATIC, 0);

    ptr = (asi->bytecode.base + infoOffset + 4);
    for(i = 0; i < asi->bytecode.numScripts; ++i, ptr += 12)
    {
        script_info_t* info = &asi->bytecode.scriptInfo[i];
        int scriptId = (int) LONG(*((int32_t*) (ptr)));
        size_t entryPointOffset = (size_t) LONG(*((int32_t*) (ptr + 4)));
        int argCount = (int) LONG(*((int32_t*) (ptr + 8)));

        info->entryPoint = (const int*) (asi->bytecode.base + entryPointOffset);
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
    }

    // Load the string offsets.
    if(asi->bytecode.numStrings > 0)
    {
        const byte* ptr;
        int i;

        asi->bytecode.strings = Z_Malloc(asi->bytecode.numStrings * sizeof(char*), PU_STATIC, 0);

        ptr = (asi->bytecode.base + infoOffset + 4 + numScripts * 12 + 4);
        for(i = 0; i < asi->bytecode.numStrings; ++i, ptr += 4)
        {
            asi->bytecode.strings[i] = (const char*) asi->bytecode.base + LONG(*((int32_t*) (ptr)));
        }
    }

#undef OPEN_SCRIPTS_BASE
    }
}

void ActionScriptInterpreter_WriteWorldState(actionscriptinterpreter_t* asi)
{
    assert(asi);
    {
    int i;

    SV_WriteByte(3); // version byte

    for(i = 0; i < MAX_WORLD_VARS; ++i)
        SV_WriteLong(asi->worldVars[i]);

    SV_WriteLong(asi->scriptStoreSize);
    for(i = 0; i < asi->scriptStoreSize; ++i)
    {
        const script_store_t* store = &asi->scriptStore[i];
        int j;

        SV_WriteLong(store->map);
        SV_WriteLong((int) store->scriptId);
        for(j = 0; j < 4; ++j)
            SV_WriteByte(store->args[j]);
    }
    }
}

void ActionScriptInterpreter_ReadWorldState(actionscriptinterpreter_t* asi)
{
    assert(asi);
    {
    int i, ver;

    if(saveVersion >= 7)
        ver = SV_ReadByte();
    else
        ver = 1;

    for(i = 0; i < MAX_WORLD_VARS; ++i)
        asi->worldVars[i] = SV_ReadLong();

    if(ver >= 3)
    {
        asi->scriptStoreSize = SV_ReadLong();
        if(asi->scriptStoreSize)
        {
            if(asi->scriptStore)
                asi->scriptStore = realloc(asi->scriptStore, sizeof(script_store_t) * asi->scriptStoreSize);
            else
                asi->scriptStore = malloc(sizeof(script_store_t) * asi->scriptStoreSize);

            for(i = 0; i < asi->scriptStoreSize; ++i)
            {
                script_store_t* store = &asi->scriptStore[i];
                int j;

                store->map = SV_ReadLong();
                store->scriptId = SV_ReadLong();
                for(j = 0; j < 4; ++j)
                    store->args[j] = SV_ReadByte();
            }
        }
    }
    else
    {   // Old format.
        script_store_t tempStore[20];

        asi->scriptStoreSize = 0;
        for(i = 0; i < 20; ++i)
        {
            int map = SV_ReadLong();
            script_store_t* store = &tempStore[map < 0? 19 : asi->scriptStoreSize++];
            int j;

            store->map = map < 0? 0 : map-1;
            store->scriptId = SV_ReadLong();
            for(j = 0; j < 4; ++j)
                store->args[j] = SV_ReadByte();
        }

        if(saveVersion < 7)
        {
            byte junk[2];
            SV_Read(junk, 12);
        }

        if(asi->scriptStoreSize)
        {
            if(asi->scriptStore)
                asi->scriptStore = realloc(asi->scriptStore, sizeof(script_store_t) * asi->scriptStoreSize);
            else
                asi->scriptStore = malloc(sizeof(script_store_t) * asi->scriptStoreSize);
            memcpy(asi->scriptStore, tempStore, sizeof(script_store_t) * asi->scriptStoreSize);
        }
    }

    if(!asi->scriptStoreSize && asi->scriptStore)
    {
        free(asi->scriptStore); asi->scriptStore = NULL;
    }
    }
}

void ActionScriptInterpreter_WriteMapState(actionscriptinterpreter_t* asi)
{
    assert(asi);
    {
    int i;

    for(i = 0; i < asi->bytecode.numScripts; ++i)
    {
        const script_info_t* info = &asi->bytecode.scriptInfo[i];
        SV_WriteShort(info->scriptState);
        SV_WriteShort(info->waitValue);
    }

    for(i = 0; i < MAX_MAP_VARS; ++i)
        SV_WriteLong(asi->mapVars[i]);
    }
}

void ActionScriptInterpreter_ReadMapState(actionscriptinterpreter_t* asi)
{
    assert(asi);
    {
    int i;

    for(i = 0; i < asi->bytecode.numScripts; ++i)
    {
        script_info_t* info = &asi->bytecode.scriptInfo[i];
        info->scriptState = SV_ReadShort();
        info->waitValue = SV_ReadShort();
    }

    for(i = 0; i < MAX_MAP_VARS; ++i)
        asi->mapVars[i] = SV_ReadLong();
    }
}

void ActionScriptThinker_Write(const thinker_t* thinker)
{
    assert(thinker);
    {
    const actionscript_thinker_t* th = (const actionscript_thinker_t*) thinker;
    uint i;

    SV_WriteByte(1); // Write a version byte.

    SV_WriteLong(SV_ThingArchiveNum(th->activator));
    SV_WriteLong(th->lineDef ? DMU_ToIndex(th->lineDef) : -1);
    SV_WriteLong(th->lineSide);
    SV_WriteLong((int) th->scriptId);
    SV_WriteLong(th->infoIndex);
    SV_WriteLong(th->delayCount);
    for(i = 0; i < AST_STACK_DEPTH; ++i)
        SV_WriteLong(th->stack[i]);
    SV_WriteLong(th->stackDepth);
    for(i = 0; i < AST_MAX_VARS; ++i)
        SV_WriteLong(th->vars[i]);
    {
    SV_WriteLong((int) (th->bytecodePos) - (int) ActionScriptInterpreter->bytecode.base);
    }
    }
}

int ActionScriptThinker_Read(thinker_t* thinker)
{
    assert(thinker);
    {
    actionscript_thinker_t* th = (actionscript_thinker_t*) thinker;
    int temp;
    uint i;

    if(saveVersion >= 4)
    {
        // @note the thinker class byte has already been read.
        /*int ver =*/ SV_ReadByte(); // version byte.

        th->activator = (mobj_t*) SV_ReadLong();
        th->activator = SV_GetArchiveThing((int) th->activator, &th->activator);
        temp = SV_ReadLong();
        if(temp == -1)
            th->lineDef = NULL;
        else
            th->lineDef = DMU_ToPtr(DMU_LINEDEF, temp);
        th->lineSide = SV_ReadLong();
        th->scriptId = (actionscriptid_t) SV_ReadLong();
        th->infoIndex = SV_ReadLong();
        th->delayCount = SV_ReadLong();
        for(i = 0; i < AST_STACK_DEPTH; ++i)
            th->stack[i] = SV_ReadLong();
        th->stackDepth = SV_ReadLong();
        for(i = 0; i < AST_MAX_VARS; ++i)
            th->vars[i] = SV_ReadLong();
        {
            th->bytecodePos = (const int*) (ActionScriptInterpreter->bytecode.base + SV_ReadLong());
        }
    }
    else
    {
        // Its in the old pre V4 format which serialized actionscript_thinker_t
        // Padding at the start (an old thinker_t struct)
        thinker_t junk;
        SV_Read(&junk, (size_t) 16);

        // Start of used data members.
        th->activator = (mobj_t*) SV_ReadLong();
        th->activator = SV_GetArchiveThing((int) th->activator, &th->activator);
        temp = SV_ReadLong();
        if(temp == -1)
            th->lineDef = NULL;
        else
            th->lineDef = DMU_ToPtr(DMU_LINEDEF, temp);
        th->lineSide = SV_ReadLong();
        th->scriptId = (actionscriptid_t) SV_ReadLong();
        th->infoIndex = SV_ReadLong();
        th->delayCount = SV_ReadLong();
        for(i = 0; i < AST_STACK_DEPTH; ++i)
            th->stack[i] = SV_ReadLong();
        th->stackDepth = SV_ReadLong();
        for(i = 0; i < AST_MAX_VARS; ++i)
            th->vars[i] = SV_ReadLong();
        {
            th->bytecodePos = (const int*) (ActionScriptInterpreter->bytecode.base + SV_ReadLong());
        }
    }

    th->thinker.function = ActionScriptThinker_Think;

    return true; // Add this thinker.
    }
}

/**
 * Scans the ACS store and executes all scripts belonging to the specified map.
 */
void ActionScriptInterpreter_StartAll(actionscriptinterpreter_t* asi, uint map)
{
    assert(asi);
    {
    int i = 0, origSize = asi->scriptStoreSize;
    while(i < asi->scriptStoreSize)
    {
        script_store_t* store = &asi->scriptStore[i];
        actionscript_thinker_t* newScript;

        if(store->map != map)
        {
            i++;
            continue;
        }

        startScript(asi, store->scriptId, 0, store->args, NULL, NULL, 0, &newScript);
        if(newScript)
        {
            newScript->delayCount = TICSPERSEC;
        }

        asi->scriptStoreSize -= 1;
        if(i == asi->scriptStoreSize)
            break;
        memmove(&asi->scriptStore[i], &asi->scriptStore[i+1], sizeof(script_store_t) * (asi->scriptStoreSize-i));
    }

    if(asi->scriptStoreSize != origSize)
    {
        if(asi->scriptStoreSize)
            asi->scriptStore = realloc(asi->scriptStore, sizeof(script_store_t) * asi->scriptStoreSize);
        else
            free(asi->scriptStore); asi->scriptStore = NULL;
    }
    }
}

boolean ActionScriptInterpreter_Start(actionscriptinterpreter_t* asi,
    actionscriptid_t scriptId, uint map, byte* args, mobj_t* activator,
    linedef_t* lineDef, int side)
{
    assert(asi);
    return startScript(asi, scriptId, map, args, activator, lineDef, side, NULL);
}

static boolean addToScriptStore(actionscriptinterpreter_t* asi,
    actionscriptid_t scriptId, uint map, byte* args)
{
    script_store_t* store;

    if(asi->scriptStoreSize)
    {
        int i;
        // Don't allow duplicates.
        for(i = 0; i < asi->scriptStoreSize; ++i)
        {
            store = &asi->scriptStore[i];
            if(store->scriptId == scriptId && store->map == map)
                return false;
        }

        asi->scriptStore = realloc(asi->scriptStore, ++asi->scriptStoreSize * sizeof(script_store_t));
    }
    else
    {
        asi->scriptStore = malloc(sizeof(script_store_t));
        asi->scriptStoreSize = 1;
    }

    store = &asi->scriptStore[asi->scriptStoreSize-1];

    store->map = map;
    store->scriptId = scriptId;
    store->args[0] = args[0];
    store->args[1] = args[1];
    store->args[2] = args[2];
    store->args[3] = args[3];

    return true;
}

boolean ActionScriptInterpreter_Stop(actionscriptinterpreter_t* asi,
    actionscriptid_t scriptId, uint map)
{
    assert(asi);
    {
    script_info_t* info;
    int infoIndex;

    infoIndex = indexForScriptId(asi, scriptId);
    if(infoIndex == -1)
    {   // Script not found.
        return false;
    }

    info = &asi->bytecode.scriptInfo[infoIndex];

    if(info->scriptState == SS_INACTIVE || info->scriptState == SS_TERMINATING)
    {   // States that disallow termination.
        return false;
    }

    info->scriptState = SS_TERMINATING;
    return true;
    }
}

boolean ActionScriptInterpreter_Suspend(actionscriptinterpreter_t* asi,
    actionscriptid_t scriptId, uint map)
{
    assert(asi);
    {
    script_info_t* info;
    int infoIndex;

    infoIndex = indexForScriptId(asi, scriptId);
    if(infoIndex == -1)
    {   // Script not found.
        return false;
    }

    info = &asi->bytecode.scriptInfo[infoIndex];

    if(info->scriptState == SS_INACTIVE ||
       info->scriptState == SS_SUSPENDED ||
       info->scriptState == SS_TERMINATING)
    {   // States that disallow suspension.
        return false;
    }

    info->scriptState = SS_SUSPENDED;
    return true;
    }
}

void ActionScriptThinker_Think(thinker_t* th)
{
    assert(th);
    {
    actionscript_thinker_t* script = (actionscript_thinker_t*) th;
    actionscriptinterpreter_t* asi = ActionScriptInterpreter;
    script_info_t* info = &asi->bytecode.scriptInfo[script->infoIndex];
    script_action_t action;

    if(info->scriptState == SS_TERMINATING)
    {
        info->scriptState = SS_INACTIVE;
        scriptFinished(asi, script->scriptId);
        Map_RemoveThinker(Thinker_Map(th), th);
        return;
    }

    if(info->scriptState != SS_RUNNING)
    {
        return;
    }

    if(script->delayCount)
    {
        script->delayCount--;
        return;
    }

    do
    {
        script_bytecode_cmdinterpreter_t cmd = bytecodeCommandInterpreters[LONG(*script->bytecodePos++)];
        action = cmd(script);
    } while(action == SA_CONTINUE);

    if(action == SA_TERMINATE)
    {
        info->scriptState = SS_INACTIVE;
        scriptFinished(asi, script->scriptId);
        Map_RemoveThinker(Thinker_Map(th), th);
    }
    }
}

void ActionScriptInterpreter_TagFinished(actionscriptinterpreter_t* asi, int tag)
{
    assert(asi);
    if(!tagBusy(tag))
    {
        int i;
        for(i = 0; i < asi->bytecode.numScripts; ++i)
        {
            script_info_t* info = &asi->bytecode.scriptInfo[i];

            if(info->scriptState == SS_WAITING_FOR_TAG && info->waitValue == tag)
            {
                info->scriptState = SS_RUNNING;
            }
        }
    }
}

void ActionScriptInterpreter_PolyobjFinished(actionscriptinterpreter_t* asi, int po)
{
    assert(asi);
    if(!PO_Busy(P_CurrentMap(), po))
    {
        int i;
        for(i = 0; i < asi->bytecode.numScripts; ++i)
        {
            script_info_t* info = &asi->bytecode.scriptInfo[i];

            if(info->scriptState == SS_WAITING_FOR_POLY && info->waitValue == po)
            {
                info->scriptState = SS_RUNNING;
            }
        }
    }
}

static void scriptFinished(actionscriptinterpreter_t* asi, actionscriptid_t scriptId)
{
    int i;

    for(i = 0; i < asi->bytecode.numScripts; ++i)
    {
        script_info_t* info = &asi->bytecode.scriptInfo[i];

        if(info->scriptState == SS_WAITING_FOR_SCRIPT && info->waitValue == scriptId)
        {
            info->scriptState = SS_RUNNING;
        }
    }
}

static boolean tagBusy(int tag)
{
    map_t* map = P_CurrentMap();
    uint k;

    // @note We can't use the sector tag lists here as we might already be in an
    // iteration at a higher level.
    for(k = 0; k < Map_NumSectors(map); ++k)
    {
        sector_t* sec = DMU_ToPtr(DMU_SECTOR, k);
        xsector_t* xsec = P_ToXSector(sec);

        if(xsec->tag != tag)
            continue;

        if(xsec->specialData)
            return true;
    }

    return false;
}

/**
 * @return              The index of the script with the specified id else @c -1,.
 */
static int indexForScriptId(actionscriptinterpreter_t* asi, actionscriptid_t scriptId)
{
    int i;
    for(i = 0; i < asi->bytecode.numScripts; ++i)
        if(asi->bytecode.scriptInfo[i].scriptId == scriptId)
            return i;
    return -1;
}

static D_ASCMD(NOP)
{
    return SA_CONTINUE;
}

static D_ASCMD(Terminate)
{
    return SA_TERMINATE;
}

static D_ASCMD(Suspend)
{
    ActionScriptInterpreter->bytecode.scriptInfo[script->infoIndex].scriptState = SS_SUSPENDED;
    return SA_STOP;
}

static D_ASCMD(PushNumber)
{
    Push(script, LONG(*script->bytecodePos++));
    return SA_CONTINUE;
}

static D_ASCMD(LSpec1)
{
    int special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[0] = Pop(script);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

static D_ASCMD(LSpec2)
{
    int special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[1] = Pop(script);
    ActionScriptInterpreter->specArgs[0] = Pop(script);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

static D_ASCMD(LSpec3)
{
    int special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[2] = Pop(script);
    ActionScriptInterpreter->specArgs[1] = Pop(script);
    ActionScriptInterpreter->specArgs[0] = Pop(script);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

static D_ASCMD(LSpec4)
{
    int special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[3] = Pop(script);
    ActionScriptInterpreter->specArgs[2] = Pop(script);
    ActionScriptInterpreter->specArgs[1] = Pop(script);
    ActionScriptInterpreter->specArgs[0] = Pop(script);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

static D_ASCMD(LSpec5)
{
    int special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[4] = Pop(script);
    ActionScriptInterpreter->specArgs[3] = Pop(script);
    ActionScriptInterpreter->specArgs[2] = Pop(script);
    ActionScriptInterpreter->specArgs[1] = Pop(script);
    ActionScriptInterpreter->specArgs[0] = Pop(script);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

static D_ASCMD(LSpec1Direct)
{
    int special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[0] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

static D_ASCMD(LSpec2Direct)
{
    int special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[0] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[1] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

static D_ASCMD(LSpec3Direct)
{
    int special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[0] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[1] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[2] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

static D_ASCMD(LSpec4Direct)
{
    int special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[0] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[1] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[2] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[3] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

static D_ASCMD(LSpec5Direct)
{
    int special = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[0] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[1] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[2] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[3] = LONG(*script->bytecodePos++);
    ActionScriptInterpreter->specArgs[4] = LONG(*script->bytecodePos++);
    P_ExecuteLineSpecial(special, ActionScriptInterpreter->specArgs, script->lineDef, script->lineSide, script->activator);
    return SA_CONTINUE;
}

static D_ASCMD(Add)
{
    Push(script, Pop(script) + Pop(script));
    return SA_CONTINUE;
}

static D_ASCMD(Subtract)
{
    int operand2 = Pop(script);
    Push(script, Pop(script) - operand2);
    return SA_CONTINUE;
}

static D_ASCMD(Multiply)
{
    Push(script, Pop(script) * Pop(script));
    return SA_CONTINUE;
}

static D_ASCMD(Divide)
{
    int operand2 = Pop(script);
    Push(script, Pop(script) / operand2);
    return SA_CONTINUE;
}

static D_ASCMD(Modulus)
{
    int operand2 = Pop(script);
    Push(script, Pop(script) % operand2);
    return SA_CONTINUE;
}

static D_ASCMD(EQ)
{
    Push(script, Pop(script) == Pop(script));
    return SA_CONTINUE;
}

static D_ASCMD(NE)
{
    Push(script, Pop(script) != Pop(script));
    return SA_CONTINUE;
}

static D_ASCMD(LT)
{
    int operand2 = Pop(script);
    Push(script, Pop(script) < operand2);
    return SA_CONTINUE;
}

static D_ASCMD(GT)
{
    int operand2 = Pop(script);
    Push(script, Pop(script) > operand2);
    return SA_CONTINUE;
}

static D_ASCMD(LE)
{
    int operand2 = Pop(script);
    Push(script, Pop(script) <= operand2);
    return SA_CONTINUE;
}

static D_ASCMD(GE)
{
    int operand2 = Pop(script);
    Push(script, Pop(script) >= operand2);
    return SA_CONTINUE;
}

static D_ASCMD(AssignScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] = Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(AssignMapVar)
{
    ActionScriptInterpreter->mapVars[LONG(*script->bytecodePos++)] = Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(AssignWorldVar)
{
    ActionScriptInterpreter->worldVars[LONG(*script->bytecodePos++)] = Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(PushScriptVar)
{
    Push(script, script->vars[LONG(*script->bytecodePos++)]);
    return SA_CONTINUE;
}

static D_ASCMD(PushMapVar)
{
    Push(script, ActionScriptInterpreter->mapVars[LONG(*script->bytecodePos++)]);
    return SA_CONTINUE;
}

static D_ASCMD(PushWorldVar)
{
    Push(script, ActionScriptInterpreter->worldVars[LONG(*script->bytecodePos++)]);
    return SA_CONTINUE;
}

static D_ASCMD(AddScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] += Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(AddMapVar)
{
    ActionScriptInterpreter->mapVars[LONG(*script->bytecodePos++)] += Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(AddWorldVar)
{
    ActionScriptInterpreter->worldVars[LONG(*script->bytecodePos++)] += Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(SubScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] -= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(SubMapVar)
{
    ActionScriptInterpreter->mapVars[LONG(*script->bytecodePos++)] -= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(SubWorldVar)
{
    ActionScriptInterpreter->worldVars[LONG(*script->bytecodePos++)] -= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(MulScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] *= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(MulMapVar)
{
    ActionScriptInterpreter->mapVars[LONG(*script->bytecodePos++)] *= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(MulWorldVar)
{
    ActionScriptInterpreter->worldVars[LONG(*script->bytecodePos++)] *= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(DivScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] /= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(DivMapVar)
{
    ActionScriptInterpreter->mapVars[LONG(*script->bytecodePos++)] /= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(DivWorldVar)
{
    ActionScriptInterpreter->worldVars[LONG(*script->bytecodePos++)] /= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(ModScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)] %= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(ModMapVar)
{
    ActionScriptInterpreter->mapVars[LONG(*script->bytecodePos++)] %= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(ModWorldVar)
{
    ActionScriptInterpreter->worldVars[LONG(*script->bytecodePos++)] %= Pop(script);
    return SA_CONTINUE;
}

static D_ASCMD(IncScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)]++;
    return SA_CONTINUE;
}

static D_ASCMD(IncMapVar)
{
    ActionScriptInterpreter->mapVars[LONG(*script->bytecodePos++)]++;
    return SA_CONTINUE;
}

static D_ASCMD(IncWorldVar)
{
    ActionScriptInterpreter->worldVars[LONG(*script->bytecodePos++)]++;
    return SA_CONTINUE;
}

static D_ASCMD(DecScriptVar)
{
    script->vars[LONG(*script->bytecodePos++)]--;
    return SA_CONTINUE;
}

static D_ASCMD(DecMapVar)
{
    ActionScriptInterpreter->mapVars[LONG(*script->bytecodePos++)]--;
    return SA_CONTINUE;
}

static D_ASCMD(DecWorldVar)
{
    ActionScriptInterpreter->worldVars[LONG(*script->bytecodePos++)]--;
    return SA_CONTINUE;
}

static D_ASCMD(Goto)
{
    script->bytecodePos = (int*) (ActionScriptInterpreter->bytecode.base + LONG(*script->bytecodePos));
    return SA_CONTINUE;
}

static D_ASCMD(IfGoto)
{
    if(Pop(script))
    {
        script->bytecodePos = (int*) (ActionScriptInterpreter->bytecode.base + LONG(*script->bytecodePos));
    }
    else
    {
        script->bytecodePos++;
    }
    return SA_CONTINUE;
}

static D_ASCMD(Drop)
{
    Drop(script);
    return SA_CONTINUE;
}

static D_ASCMD(Delay)
{
    script->delayCount = Pop(script);
    return SA_STOP;
}

static D_ASCMD(DelayDirect)
{
    script->delayCount = LONG(*script->bytecodePos++);
    return SA_STOP;
}

static D_ASCMD(Random)
{
    int low, high;
    high = Pop(script);
    low = Pop(script);
    Push(script, low + (P_Random() % (high - low + 1)));
    return SA_CONTINUE;
}

static D_ASCMD(RandomDirect)
{
    int low, high;
    low = LONG(*script->bytecodePos++);
    high = LONG(*script->bytecodePos++);
    Push(script, low + (P_Random() % (high - low + 1)));
    return SA_CONTINUE;
}

static D_ASCMD(MobjCount)
{
    int tid;
    tid = Pop(script);
    Push(script, countMobjsOfType(P_CurrentMap(), Pop(script), tid));
    return SA_CONTINUE;
}

static D_ASCMD(MobjCountDirect)
{
    int type;
    type = LONG(*script->bytecodePos++);
    Push(script, countMobjsOfType(P_CurrentMap(), type, LONG(*script->bytecodePos++)));
    return SA_CONTINUE;
}

typedef struct {
    mobjtype_t      type;
    int             count;
} countmobjoftypeparams_t;

static int countMobjOfType(void* p, void* context)
{
    countmobjoftypeparams_t* params = (countmobjoftypeparams_t*) context;
    mobj_t* mo = (mobj_t*) p;

    // Does the type match?
    if(mo->type != params->type)
        return true; // Continue iteration.

    // Minimum health requirement?
    if((mo->flags & MF_COUNTKILL) && mo->health <= 0)
        return true; // Continue iteration.

    params->count++;

    return true; // Continue iteration.
}

static int countMobjsOfType(map_t* map, int type, int tid)
{
    mobjtype_t moType;
    int count;

    if(!(type + tid))
    {   // Nothing to count.
        return 0;
    }

    moType = P_MapScriptThingIdToMobjType(type);
    count = 0;

    if(tid)
    {   // Count TID things.
        mobj_t* mo;
        int searcher = -1;

        while((mo = P_FindMobjFromTID(map, tid, &searcher)) != NULL)
        {
            if(type == 0)
            {   // Just count TIDs.
                count++;
            }
            else if(moType == mo->type)
            {
                if((mo->flags & MF_COUNTKILL) && mo->health <= 0)
                {   // Don't count dead monsters.
                    continue;
                }

                count++;
            }
        }
    }
    else
    {   // Count only types.
        countmobjoftypeparams_t params;

        params.type = moType;
        params.count = 0;
        Map_IterateThinkers(map, P_MobjThinker, countMobjOfType, &params);

        count = params.count;
    }

    return count;
}

static D_ASCMD(TagWait)
{
    script_info_t* info = &ActionScriptInterpreter->bytecode.scriptInfo[script->infoIndex];
    info->waitValue = Pop(script);
    info->scriptState = SS_WAITING_FOR_TAG;
    return SA_STOP;
}

static D_ASCMD(TagWaitDirect)
{
    script_info_t* info = &ActionScriptInterpreter->bytecode.scriptInfo[script->infoIndex];
    info->waitValue = LONG(*script->bytecodePos++);
    info->scriptState = SS_WAITING_FOR_TAG;
    return SA_STOP;
}

static D_ASCMD(PolyWait)
{
    script_info_t* info = &ActionScriptInterpreter->bytecode.scriptInfo[script->infoIndex];
    info->waitValue = Pop(script);
    info->scriptState = SS_WAITING_FOR_POLY;
    return SA_STOP;
}

static D_ASCMD(PolyWaitDirect)
{
    script_info_t* info = &ActionScriptInterpreter->bytecode.scriptInfo[script->infoIndex];
    info->waitValue = LONG(*script->bytecodePos++);
    info->scriptState = SS_WAITING_FOR_POLY;
    return SA_STOP;
}

static D_ASCMD(ChangeFloor)
{
    const char* flatName = getString(&ActionScriptInterpreter->bytecode, (script_bytecode_stringid_t) Pop(script));
    int tag = Pop(script);
    iterlist_t* list;

    if((list = GameMap_SectorIterListForTag(P_CurrentMap(), tag, false)))
    {
        material_t* mat = P_MaterialForName(MN_FLATS, flatName);
        sector_t* sec = NULL;

        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_FLOOR_MATERIAL, mat);
        }
    }

    return SA_CONTINUE;
}

static D_ASCMD(ChangeFloorDirect)
{
    int tag = LONG(*script->bytecodePos++);
    const char* flatName = getString(&ActionScriptInterpreter->bytecode, (script_bytecode_stringid_t) LONG(*script->bytecodePos++));
    iterlist_t* list;

    if((list = GameMap_SectorIterListForTag(P_CurrentMap(), tag, false)))
    {
        material_t* mat = P_MaterialForName(MN_FLATS, flatName);
        sector_t* sec = NULL;

        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_FLOOR_MATERIAL, mat);
        }
    }

    return SA_CONTINUE;
}

static D_ASCMD(ChangeCeiling)
{
    const char* flatName = getString(&ActionScriptInterpreter->bytecode, (script_bytecode_stringid_t) Pop(script));
    int tag = Pop(script);
    iterlist_t* list;

    if((list = GameMap_SectorIterListForTag(P_CurrentMap(), tag, false)))
    {
        material_t* mat = P_MaterialForName(MN_FLATS, flatName);
        sector_t* sec = NULL;

        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_CEILING_MATERIAL, mat);
        }
    }

    return SA_CONTINUE;
}

static D_ASCMD(ChangeCeilingDirect)
{
    int tag = LONG(*script->bytecodePos++);
    const char* flatName = getString(&ActionScriptInterpreter->bytecode, (script_bytecode_stringid_t) LONG(*script->bytecodePos++));
    iterlist_t* list;

    if((list = GameMap_SectorIterListForTag(P_CurrentMap(), tag, false)))
    {
        material_t* mat = P_MaterialForName(MN_FLATS, flatName);
        sector_t* sec = NULL;

        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_CEILING_MATERIAL, mat);
        }
    }

    return SA_CONTINUE;
}

static D_ASCMD(Restart)
{
    script->bytecodePos = ActionScriptInterpreter->bytecode.scriptInfo[script->infoIndex].entryPoint;
    return SA_CONTINUE;
}

static D_ASCMD(AndLogical)
{
    Push(script, Pop(script) && Pop(script));
    return SA_CONTINUE;
}

static D_ASCMD(OrLogical)
{
    Push(script, Pop(script) || Pop(script));
    return SA_CONTINUE;
}

static D_ASCMD(AndBitwise)
{
    Push(script, Pop(script) & Pop(script));
    return SA_CONTINUE;
}

static D_ASCMD(OrBitwise)
{
    Push(script, Pop(script) | Pop(script));
    return SA_CONTINUE;
}

static D_ASCMD(EorBitwise)
{
    Push(script, Pop(script) ^ Pop(script));
    return SA_CONTINUE;
}

static D_ASCMD(NegateLogical)
{
    Push(script, !Pop(script));
    return SA_CONTINUE;
}

static D_ASCMD(LShift)
{
    int operand2 = Pop(script);
    Push(script, Pop(script) << operand2);
    return SA_CONTINUE;
}

static D_ASCMD(RShift)
{
    int operand2 = Pop(script);
    Push(script, Pop(script) >> operand2);
    return SA_CONTINUE;
}

static D_ASCMD(UnaryMinus)
{
    Push(script, -Pop(script));
    return SA_CONTINUE;
}

static D_ASCMD(IfNotGoto)
{
    if(Pop(script))
    {
        script->bytecodePos++;
    }
    else
    {
            script->bytecodePos = (int*) (ActionScriptInterpreter->bytecode.base + LONG(*script->bytecodePos));
    }
    return SA_CONTINUE;
}

static D_ASCMD(LineDefSide)
{
    Push(script, script->lineSide);
    return SA_CONTINUE;
}

static D_ASCMD(ScriptWait)
{
    script_info_t* info = &ActionScriptInterpreter->bytecode.scriptInfo[script->infoIndex];
    info->waitValue = Pop(script);
    info->scriptState = SS_WAITING_FOR_SCRIPT;
    return SA_STOP;
}

static D_ASCMD(ScriptWaitDirect)
{
    script_info_t* info = &ActionScriptInterpreter->bytecode.scriptInfo[script->infoIndex];
    info->waitValue = LONG(*script->bytecodePos++);
    info->scriptState = SS_WAITING_FOR_SCRIPT;
    return SA_STOP;
}

static D_ASCMD(ClearLineDefSpecial)
{
    if(script->lineDef)
    {
        P_ToXLine(script->lineDef)->special = 0;
    }
    return SA_CONTINUE;
}

static D_ASCMD(CaseGoto)
{
    if(Top(script) == LONG(*script->bytecodePos++))
    {
            script->bytecodePos = (int*) (ActionScriptInterpreter->bytecode.base + LONG(*script->bytecodePos));
        Drop(script);
    }
    else
    {
        script->bytecodePos++;
    }
    return SA_CONTINUE;
}

static D_ASCMD(BeginPrint)
{
    ActionScriptInterpreter->printBuffer[0] = 0;
    return SA_CONTINUE;
}

static D_ASCMD(EndPrint)
{
    if(script->activator && script->activator->player)
    {
        P_SetMessage(script->activator->player, ActionScriptInterpreter->printBuffer, false);
    }
    else
    {
        int i;

        // Send to everybody.
        for(i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame)
                P_SetMessage(&players[i], ActionScriptInterpreter->printBuffer, false);
    }

    return SA_CONTINUE;
}

static D_ASCMD(EndPrintBold)
{
    int i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            P_SetYellowMessage(&players[i], ActionScriptInterpreter->printBuffer, false);
        }
    }

    return SA_CONTINUE;
}

static D_ASCMD(PrintString)
{
    strcat(ActionScriptInterpreter->printBuffer, getString(&ActionScriptInterpreter->bytecode, (script_bytecode_stringid_t) Pop(script)));
    return SA_CONTINUE;
}

static D_ASCMD(PrintNumber)
{
    char tempStr[16];
    sprintf(tempStr, "%d", Pop(script));
    strcat(ActionScriptInterpreter->printBuffer, tempStr);
    return SA_CONTINUE;
}

static D_ASCMD(PrintCharacter)
{
    char* bufferEnd;
    bufferEnd = ActionScriptInterpreter->printBuffer + strlen(ActionScriptInterpreter->printBuffer);
    *bufferEnd++ = Pop(script);
    *bufferEnd = 0;
    return SA_CONTINUE;
}

static D_ASCMD(PlayerCount)
{
    int i, count;

    count = 0;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        count += players[i].plr->inGame;
    }
    Push(script, count);

    return SA_CONTINUE;
}

static D_ASCMD(GameType)
{
    int gameType;

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

static D_ASCMD(GameSkill)
{
    Push(script, gameSkill);
    return SA_CONTINUE;
}

static D_ASCMD(Timer)
{
    map_t* map = P_CurrentMap();
    Push(script, map->time);
    return SA_CONTINUE;
}

static D_ASCMD(SectorSound)
{
    int volume;
    mobj_t* mobj;

    mobj = NULL;
    if(script->lineDef)
    {
        mobj = DMU_GetPtrp(DMU_GetPtrp(script->lineDef, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
    }
    volume = Pop(script);

#if _DEBUG
Con_Printf("CmdSectorSound: volume=%i\n", volume);
#endif

    S_StartSoundAtVolume(S_GetSoundID(getString(&ActionScriptInterpreter->bytecode, (script_bytecode_stringid_t) Pop(script))), mobj, volume / 127.0f);
    return SA_CONTINUE;
}

static D_ASCMD(MobjSound)
{
    int tid, sound, volume, searcher;
    mobj_t* mo;

    volume = Pop(script);
    sound = S_GetSoundID(getString(&ActionScriptInterpreter->bytecode, (script_bytecode_stringid_t) Pop(script)));
    tid = Pop(script);
    searcher = -1;
    while(sound && (mo = P_FindMobjFromTID(P_CurrentMap(), tid, &searcher)) != NULL)
    {
        S_StartSoundAtVolume(sound, mo, volume / 127.0f);
    }

    return SA_CONTINUE;
}

static D_ASCMD(AmbientSound)
{
    int volume, sound;
    mobj_t* mo = NULL; // For 3D positioning.
    mobj_t* plrmo = players[DISPLAYPLAYER].plr->mo;

    volume = Pop(script);
    // If we are playing 3D sounds, create a temporary source mobj
    // for the sound.
    if(cfg.snd3D && plrmo)
    {
        map_t* map = Thinker_Map((thinker_t*) plrmo);
        float pos[3];

        pos[VX] = plrmo->pos[VX] + (((M_Random() - 127) * 2) << FRACBITS);
        pos[VY] = plrmo->pos[VY] + (((M_Random() - 127) * 2) << FRACBITS);
        pos[VZ] = plrmo->pos[VZ] + (((M_Random() - 127) * 2) << FRACBITS);

        if((mo = GameMap_SpawnMobj3fv(map, MT_CAMERA, pos, 0, 0))) // A camera is a good temporary source.
            mo->tics = 5 * TICSPERSEC; // Five seconds should be enough.
    }

    sound = S_GetSoundID(getString(&ActionScriptInterpreter->bytecode, (script_bytecode_stringid_t) Pop(script)));
    S_StartSoundAtVolume(sound, mo, volume / 127.0f);

    return SA_CONTINUE;
}

static D_ASCMD(SoundSequence)
{
    mobj_t* mo = NULL;
    if(script->lineDef)
        mo = DMU_GetPtrp(DMU_GetPtrp(script->lineDef, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
    SN_StartSequenceName(mo, getString(&ActionScriptInterpreter->bytecode, (script_bytecode_stringid_t) Pop(script)));
    return SA_CONTINUE;
}

static D_ASCMD(SetSideDefMaterial)
{
    map_t* map = P_CurrentMap();
    int lineTag, side, position;
    material_t* mat;
    linedef_t* line;
    iterlist_t* list;

    mat = P_MaterialForName(MN_TEXTURES, getString(&ActionScriptInterpreter->bytecode, (script_bytecode_stringid_t) Pop(script)));
    position = Pop(script);
    side = Pop(script);
    lineTag = Pop(script);

    list = GameMap_IterListForTag(map, lineTag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((line = P_IterListIterator(list)) != NULL)
        {
            sidedef_t* sdef = DMU_GetPtrp(line, (side == 0? DMU_SIDEDEF0 : DMU_SIDEDEF1));

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

static D_ASCMD(SetLineDefBlocking)
{
    map_t* map = P_CurrentMap();
    linedef_t* line;
    int lineTag;
    boolean blocking;
    iterlist_t* list;

    blocking = Pop(script)? DDLF_BLOCKING : 0;
    lineTag = Pop(script);

    list = GameMap_IterListForTag(map, lineTag, false);
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

static D_ASCMD(SetLineDefSpecial)
{
    map_t* map = P_CurrentMap();
    linedef_t* line;
    int lineTag, special, arg1, arg2, arg3, arg4, arg5;
    iterlist_t* list;

    arg5 = Pop(script);
    arg4 = Pop(script);
    arg3 = Pop(script);
    arg2 = Pop(script);
    arg1 = Pop(script);
    special = Pop(script);
    lineTag = Pop(script);

    list = GameMap_IterListForTag(map, lineTag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((line = P_IterListIterator(list)) != NULL)
        {
            xlinedef_t* xline = P_ToXLine(line);
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

static void printScriptInfo(actionscriptinterpreter_t* asi, actionscriptid_t scriptId)
{
    static const char* scriptStateDescriptions[] = {
        "Inactive",
        "Running",
        "Suspended",
        "Waiting for tag",
        "Waiting for poly",
        "Waiting for script",
        "Terminating"
    };

    int i;

    for(i = 0; i < asi->bytecode.numScripts; ++i)
    {
        const script_info_t* info = &asi->bytecode.scriptInfo[i];

        if(scriptId != -1 && scriptId != info->scriptId)
            continue;

        Con_Printf("%d %s (a: %d, w: %d)\n", (int) info->scriptId,
                   scriptStateDescriptions[info->scriptState], info->argCount,
                   info->waitValue);
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
