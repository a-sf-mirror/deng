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
 *
 * \bug Not 64bit clean: In function 'P_LoadACScripts': cast from pointer to integer of different size, cast to pointer from integer of different size
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "dmu_lib.h"
#include "p_player.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

#define SCRIPT_CONTINUE 0
#define SCRIPT_STOP 1
#define SCRIPT_TERMINATE 2
#define OPEN_SCRIPTS_BASE 1000
#define PRINT_BUFFER_SIZE 256
#define GAME_SINGLE_PLAYER 0
#define GAME_NET_COOPERATIVE 1
#define GAME_NET_DEATHMATCH 2
#define TEXTURE_TOP 0
#define TEXTURE_MIDDLE 1
#define TEXTURE_BOTTOM 2
#define S_DROP ACScript->stackPtr--
#define S_POP ACScript->stack[--ACScript->stackPtr]
#define S_PUSH(x) ACScript->stack[ACScript->stackPtr++] = x

// TYPES -------------------------------------------------------------------

typedef struct acsheader_s {
    int     marker;
    int     infoOffset;
    int     code;
} acsheader_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void StartOpenACS(int number, int infoIndex, const int* address);
static void ScriptFinished(int number);
static boolean TagBusy(int tag);
static boolean AddToACSStore(int map, int number, byte *args);
static int GetACSIndex(int number);
static void Push(int value);
static int Pop(void);
static int Top(void);
static void Drop(void);

static int CmdNOP(void);
static int CmdTerminate(void);
static int CmdSuspend(void);
static int CmdPushNumber(void);
static int CmdLSpec1(void);
static int CmdLSpec2(void);
static int CmdLSpec3(void);
static int CmdLSpec4(void);
static int CmdLSpec5(void);
static int CmdLSpec1Direct(void);
static int CmdLSpec2Direct(void);
static int CmdLSpec3Direct(void);
static int CmdLSpec4Direct(void);
static int CmdLSpec5Direct(void);
static int CmdAdd(void);
static int CmdSubtract(void);
static int CmdMultiply(void);
static int CmdDivide(void);
static int CmdModulus(void);
static int CmdEQ(void);
static int CmdNE(void);
static int CmdLT(void);
static int CmdGT(void);
static int CmdLE(void);
static int CmdGE(void);
static int CmdAssignScriptVar(void);
static int CmdAssignMapVar(void);
static int CmdAssignWorldVar(void);
static int CmdPushScriptVar(void);
static int CmdPushMapVar(void);
static int CmdPushWorldVar(void);
static int CmdAddScriptVar(void);
static int CmdAddMapVar(void);
static int CmdAddWorldVar(void);
static int CmdSubScriptVar(void);
static int CmdSubMapVar(void);
static int CmdSubWorldVar(void);
static int CmdMulScriptVar(void);
static int CmdMulMapVar(void);
static int CmdMulWorldVar(void);
static int CmdDivScriptVar(void);
static int CmdDivMapVar(void);
static int CmdDivWorldVar(void);
static int CmdModScriptVar(void);
static int CmdModMapVar(void);
static int CmdModWorldVar(void);
static int CmdIncScriptVar(void);
static int CmdIncMapVar(void);
static int CmdIncWorldVar(void);
static int CmdDecScriptVar(void);
static int CmdDecMapVar(void);
static int CmdDecWorldVar(void);
static int CmdGoto(void);
static int CmdIfGoto(void);
static int CmdDrop(void);
static int CmdDelay(void);
static int CmdDelayDirect(void);
static int CmdRandom(void);
static int CmdRandomDirect(void);
static int CmdThingCount(void);
static int CmdThingCountDirect(void);
static int CmdTagWait(void);
static int CmdTagWaitDirect(void);
static int CmdPolyWait(void);
static int CmdPolyWaitDirect(void);
static int CmdChangeFloor(void);
static int CmdChangeFloorDirect(void);
static int CmdChangeCeiling(void);
static int CmdChangeCeilingDirect(void);
static int CmdRestart(void);
static int CmdAndLogical(void);
static int CmdOrLogical(void);
static int CmdAndBitwise(void);
static int CmdOrBitwise(void);
static int CmdEorBitwise(void);
static int CmdNegateLogical(void);
static int CmdLShift(void);
static int CmdRShift(void);
static int CmdUnaryMinus(void);
static int CmdIfNotGoto(void);
static int CmdLineSide(void);
static int CmdScriptWait(void);
static int CmdScriptWaitDirect(void);
static int CmdClearLineSpecial(void);
static int CmdCaseGoto(void);
static int CmdBeginPrint(void);
static int CmdEndPrint(void);
static int CmdPrintString(void);
static int CmdPrintNumber(void);
static int CmdPrintCharacter(void);
static int CmdPlayerCount(void);
static int CmdGameType(void);
static int CmdGameSkill(void);
static int CmdTimer(void);
static int CmdSectorSound(void);
static int CmdAmbientSound(void);
static int CmdSoundSequence(void);
static int CmdSetLineTexture(void);
static int CmdSetLineBlocking(void);
static int CmdSetLineSpecial(void);
static int CmdThingSound(void);
static int CmdEndPrintBold(void);

static void ThingCount(int type, int tid);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int ACScriptCount;
const byte* ActionCodeBase;
acsinfo_t* ACSInfo;
int MapVars[MAX_ACS_MAP_VARS];
int WorldVars[MAX_ACS_WORLD_VARS];
acsstore_t ACSStore[MAX_ACS_STORE + 1]; // +1 for termination marker

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static acs_t* ACScript;
static const int* PCodePtr;
static byte SpecArgs[8];
static int ACStringCount;
static char const** ACStrings;
static char PrintBuffer[PRINT_BUFFER_SIZE];
static acs_t* NewScript;

static char ErrorMsg[128];

static int (*PCodeCmds[]) (void) =
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
        CmdPolyWait, CmdPolyWaitDirect, CmdChangeFloor,
        CmdChangeFloorDirect, CmdChangeCeiling, CmdChangeCeilingDirect,
        CmdRestart, CmdAndLogical, CmdOrLogical, CmdAndBitwise,
        CmdOrBitwise, CmdEorBitwise, CmdNegateLogical, CmdLShift,
        CmdRShift, CmdUnaryMinus, CmdIfNotGoto, CmdLineSide, CmdScriptWait,
        CmdScriptWaitDirect, CmdClearLineSpecial, CmdCaseGoto,
        CmdBeginPrint, CmdEndPrint, CmdPrintString, CmdPrintNumber,
        CmdPrintCharacter, CmdPlayerCount, CmdGameType, CmdGameSkill,
        CmdTimer, CmdSectorSound, CmdAmbientSound, CmdSoundSequence,
        CmdSetLineTexture, CmdSetLineBlocking, CmdSetLineSpecial,
        CmdThingSound, CmdEndPrintBold};

// CODE --------------------------------------------------------------------

const char* GetACString(int id)
{
    if(id < 0 || id > ACStringCount)
        return NULL;

    return ACStrings[id];
}

void P_LoadACScripts(int lump)
{
    int                 i;
    const int*          buffer;
    const acsheader_t*  header;
    acsinfo_t*          info;

    header = W_CacheLumpNum(lump, PU_MAP);
    ActionCodeBase = (const byte*) header;
    buffer = (int*) ((const byte*) header + LONG(header->infoOffset));
    ACScriptCount = LONG(*buffer++);
    if(ACScriptCount == 0 || IS_CLIENT)
    {                           // Empty behavior lump
        ACScriptCount = 0;
        return;
    }

    ACSInfo = Z_Malloc(ACScriptCount * sizeof(acsinfo_t), PU_MAP, 0);
    memset(ACSInfo, 0, ACScriptCount * sizeof(acsinfo_t));
    for(i = 0, info = ACSInfo; i < ACScriptCount; ++i, info++)
    {
        info->number = LONG(*buffer++);
        info->address =
            (const int*) ((const byte*) ActionCodeBase + LONG(*buffer++));
        info->argCount = LONG(*buffer++);
        if(info->number >= OPEN_SCRIPTS_BASE)
        {                       // Auto-activate
            info->number -= OPEN_SCRIPTS_BASE;
            StartOpenACS(info->number, i, info->address);
            info->state = ASTE_RUNNING;
        }
        else
        {
            info->state = ASTE_INACTIVE;
        }
    }

    ACStringCount = LONG(*buffer++);
    ACStrings = Z_Malloc(ACStringCount * sizeof(char*), PU_MAP, 0);
    for(i = 0; i < ACStringCount; ++i)
    {
        ACStrings[i] = (const char*)ActionCodeBase + LONG(*buffer++);
    }

    memset(MapVars, 0, sizeof(MapVars));
}

static void StartOpenACS(int number, int infoIndex, const int* address)
{
    acs_t*              script;

    script = Z_Calloc(sizeof(*script), PU_MAP, 0);
    script->thinker.function = T_InterpretACS;
    DD_ThinkerAdd(&script->thinker);

    // World objects are allotted 1 second for initialization.
    script->number = number;
    script->delayCount = 35;
    script->infoIndex = infoIndex;
    script->ip = address;
}

/**
 * Scans the ACS store and executes all scripts belonging to the current
 * map.
 */
void P_CheckACSStore(void)
{
    acsstore_t*         store;

    for(store = ACSStore; store->map != 0; store++)
    {
        if(store->map == gameMap)
        {
            P_StartACS(store->script, 0, store->args, NULL, NULL, 0);
            if(NewScript)
            {
                NewScript->delayCount = 35;
            }
            store->map = -1;
        }
    }
}

boolean P_StartACS(int number, int map, byte* args, mobj_t* activator,
                   linedef_t* line, int side)
{
    int                 i, infoIndex;
    acs_t*              script;
    aste_t*             statePtr;

    NewScript = NULL;
    if(map && map != gameMap)
    {   // Add to the script store.
        return AddToACSStore(map, number, args);
    }

    infoIndex = GetACSIndex(number);
    if(infoIndex == -1)
    {   // Script not found.
        //Con_Error("P_StartACS: Unknown script number %d", number);
        sprintf(ErrorMsg, "P_STARTACS ERROR: UNKNOWN SCRIPT %d", number);
        P_SetMessage(&players[CONSOLEPLAYER], ErrorMsg, false);
        return false;
    }

    statePtr = &ACSInfo[infoIndex].state;
    if(*statePtr == ASTE_SUSPENDED)
    {   // Resume a suspended script.
        *statePtr = ASTE_RUNNING;
        return true;
    }

    if(*statePtr != ASTE_INACTIVE)
    {   // Script is already executing.
        return false;
    }

    script = Z_Calloc(sizeof(*script), PU_MAP, 0);
    script->thinker.function = T_InterpretACS;
    DD_ThinkerAdd(&script->thinker);

    script->number = number;
    script->infoIndex = infoIndex;
    script->activator = activator;
    script->line = line;
    script->side = side;
    script->ip = ACSInfo[infoIndex].address;

    for(i = 0; i < ACSInfo[infoIndex].argCount; ++i)
    {
        script->vars[i] = args[i];
    }

    *statePtr = ASTE_RUNNING;
    NewScript = script;
    return true;
}

static boolean AddToACSStore(int map, int number, byte *args)
{
    int         i;
    int         index;

    index = -1;
    for(i = 0; ACSStore[i].map != 0; ++i)
    {
        if(ACSStore[i].script == number && ACSStore[i].map == map)
        {   // Don't allow duplicates
            return false;
        }
        if(index == -1 && ACSStore[i].map == -1)
        {   // Remember first empty slot
            index = i;
        }
    }

    if(index == -1)
    {   // Append required
        if(i == MAX_ACS_STORE)
        {
            Con_Error("AddToACSStore: MAX_ACS_STORE (%d) exceeded.",
                      MAX_ACS_STORE);
        }
        index = i;
        ACSStore[index + 1].map = 0;
    }
    ACSStore[index].map = map;
    ACSStore[index].script = number;
    *((int *) ACSStore[index].args) = *((int *) args);
    return true;
}

boolean P_StartLockedACS(linedef_t *line, byte *args, mobj_t *mo, int side)
{
    int         i;
    int         lock;
    byte        newArgs[5];
    char        LockedBuffer[80];

    extern int TextKeyMessages[11];

    lock = args[4];
    if(!mo->player)
    {
        return false;
    }

    if(lock)
    {
        if(!(mo->player->keys & (1 << (lock - 1))))
        {
            sprintf(LockedBuffer, "YOU NEED THE %s\n",
                    GET_TXT(TextKeyMessages[lock - 1]));
            P_SetMessage(mo->player, LockedBuffer, false);
            S_StartSound(SFX_DOOR_LOCKED, mo);
            return false;
        }
    }

    for(i = 0; i < 4; ++i)
    {
        newArgs[i] = args[i];
    }
    newArgs[4] = 0;
    return P_StartACS(newArgs[0], newArgs[1], &newArgs[2], mo, line, side);
}

boolean P_TerminateACS(int number, int map)
{
    int         infoIndex;

    infoIndex = GetACSIndex(number);
    if(infoIndex == -1)
    {   // Script not found
        return false;
    }

    if(ACSInfo[infoIndex].state == ASTE_INACTIVE ||
       ACSInfo[infoIndex].state == ASTE_TERMINATING)
    {   // States that disallow termination
        return false;
    }

    ACSInfo[infoIndex].state = ASTE_TERMINATING;
    return true;
}

boolean P_SuspendACS(int number, int map)
{
    int         infoIndex;

    infoIndex = GetACSIndex(number);
    if(infoIndex == -1)
    {   // Script not found
        return false;
    }

    if(ACSInfo[infoIndex].state == ASTE_INACTIVE ||
       ACSInfo[infoIndex].state == ASTE_SUSPENDED ||
       ACSInfo[infoIndex].state == ASTE_TERMINATING)
    {   // States that disallow suspension
        return false;
    }

    ACSInfo[infoIndex].state = ASTE_SUSPENDED;
    return true;
}

void P_ACSInitNewGame(void)
{
    memset(WorldVars, 0, sizeof(WorldVars));
    memset(ACSStore, 0, sizeof(ACSStore));
}

void T_InterpretACS(acs_t* script)
{
    int             cmd, action;

    if(ACSInfo[script->infoIndex].state == ASTE_TERMINATING)
    {
        ACSInfo[script->infoIndex].state = ASTE_INACTIVE;
        ScriptFinished(ACScript->number);
        DD_ThinkerRemove(&ACScript->thinker);
        return;
    }

    if(ACSInfo[script->infoIndex].state != ASTE_RUNNING)
    {
        return;
    }

    if(script->delayCount)
    {
        script->delayCount--;
        return;
    }
    ACScript = script;

    PCodePtr = ACScript->ip;
    do
    {
        cmd = LONG(*PCodePtr++);
        action = PCodeCmds[cmd] ();
    } while(action == SCRIPT_CONTINUE);

    ACScript->ip = PCodePtr;
    if(action == SCRIPT_TERMINATE)
    {
        ACSInfo[script->infoIndex].state = ASTE_INACTIVE;
        ScriptFinished(ACScript->number);
        DD_ThinkerRemove(&ACScript->thinker);
    }
}

void P_TagFinished(int tag)
{
    int             i;

    if(TagBusy(tag) == true)
    {
        return;
    }

    for(i = 0; i < ACScriptCount; ++i)
    {
        if(ACSInfo[i].state == ASTE_WAITING_FOR_TAG &&
           ACSInfo[i].waitValue == tag)
        {
            ACSInfo[i].state = ASTE_RUNNING;
        }
    }
}

void P_PolyobjFinished(int po)
{
    int             i;

    if(PO_Busy(po) == true)
    {
        return;
    }

    for(i = 0; i < ACScriptCount; ++i)
    {
        if(ACSInfo[i].state == ASTE_WAITING_FOR_POLY &&
           ACSInfo[i].waitValue == po)
        {
            ACSInfo[i].state = ASTE_RUNNING;
        }
    }
}

static void ScriptFinished(int number)
{
    int             i;

    for(i = 0; i < ACScriptCount; ++i)
    {
        if(ACSInfo[i].state == ASTE_WAITING_FOR_SCRIPT &&
           ACSInfo[i].waitValue == number)
        {
            ACSInfo[i].state = ASTE_RUNNING;
        }
    }
}

static boolean TagBusy(int tag)
{
    uint                k;
    sector_t*           sec;
    xsector_t*          xsec;

    // NOTE: We can't use the sector tag lists here as we might already be
    // in an iteration at a higher level.
    for(k = 0; k < numsectors; ++k)
    {
        sec = DMU_ToPtr(DMU_SECTOR, k);
        xsec = P_ToXSector(sec);

        if(xsec->tag != tag)
            continue;

        if(xsec->specialData)
            return true;
    }

    return false;
}

/**
 * @return              The index of a script number else @c -1,.
 */
static int GetACSIndex(int number)
{
    int                 i;

    for(i = 0; i < ACScriptCount; ++i)
    {
        if(ACSInfo[i].number == number)
        {
            return i;
        }
    }
    return -1;
}

static void Push(int value)
{
    ACScript->stack[ACScript->stackPtr++] = value;
}

static int Pop(void)
{
    return ACScript->stack[--ACScript->stackPtr];
}

static int Top(void)
{
    return ACScript->stack[ACScript->stackPtr - 1];
}

static void Drop(void)
{
    ACScript->stackPtr--;
}

static int CmdNOP(void)
{
    return SCRIPT_CONTINUE;
}

static int CmdTerminate(void)
{
    return SCRIPT_TERMINATE;
}

static int CmdSuspend(void)
{
    ACSInfo[ACScript->infoIndex].state = ASTE_SUSPENDED;
    return SCRIPT_STOP;
}

static int CmdPushNumber(void)
{
    Push(LONG(*PCodePtr++));
    return SCRIPT_CONTINUE;
}

static int CmdLSpec1(void)
{
    int                 special;

    special = LONG(*PCodePtr++);
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec2(void)
{
    int                 special;

    special = LONG(*PCodePtr++);
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec3(void)
{
    int                 special;

    special = LONG(*PCodePtr++);
    SpecArgs[2] = Pop();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec4(void)
{
    int                 special;

    special = LONG(*PCodePtr++);
    SpecArgs[3] = Pop();
    SpecArgs[2] = Pop();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec5(void)
{
    int                 special;

    special = LONG(*PCodePtr++);
    SpecArgs[4] = Pop();
    SpecArgs[3] = Pop();
    SpecArgs[2] = Pop();
    SpecArgs[1] = Pop();
    SpecArgs[0] = Pop();
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec1Direct(void)
{
    int                 special;

    special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec2Direct(void)
{
    int                 special;

    special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec3Direct(void)
{
    int                 special;

    special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec4Direct(void)
{
    int                 special;

    special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    SpecArgs[3] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdLSpec5Direct(void)
{
    int                 special;

    special = LONG(*PCodePtr++);
    SpecArgs[0] = LONG(*PCodePtr++);
    SpecArgs[1] = LONG(*PCodePtr++);
    SpecArgs[2] = LONG(*PCodePtr++);
    SpecArgs[3] = LONG(*PCodePtr++);
    SpecArgs[4] = LONG(*PCodePtr++);
    P_ExecuteLineSpecial(special, SpecArgs, ACScript->line, ACScript->side,
                         ACScript->activator);

    return SCRIPT_CONTINUE;
}

static int CmdAdd(void)
{
    Push(Pop() + Pop());
    return SCRIPT_CONTINUE;
}

static int CmdSubtract(void)
{
    int                 operand2;

    operand2 = Pop();
    Push(Pop() - operand2);

    return SCRIPT_CONTINUE;
}

static int CmdMultiply(void)
{
    Push(Pop() * Pop());
    return SCRIPT_CONTINUE;
}

static int CmdDivide(void)
{
    int                 operand2;

    operand2 = Pop();
    Push(Pop() / operand2);
    return SCRIPT_CONTINUE;
}

static int CmdModulus(void)
{
    int                 operand2;

    operand2 = Pop();
    Push(Pop() % operand2);
    return SCRIPT_CONTINUE;
}

static int CmdEQ(void)
{
    Push(Pop() == Pop());
    return SCRIPT_CONTINUE;
}

static int CmdNE(void)
{
    Push(Pop() != Pop());
    return SCRIPT_CONTINUE;
}

static int CmdLT(void)
{
    int                 operand2;

    operand2 = Pop();
    Push(Pop() < operand2);
    return SCRIPT_CONTINUE;
}

static int CmdGT(void)
{
    int                 operand2;

    operand2 = Pop();
    Push(Pop() > operand2);
    return SCRIPT_CONTINUE;
}

static int CmdLE(void)
{
    int                 operand2;

    operand2 = Pop();
    Push(Pop() <= operand2);
    return SCRIPT_CONTINUE;
}

static int CmdGE(void)
{
    int                 operand2;

    operand2 = Pop();
    Push(Pop() >= operand2);
    return SCRIPT_CONTINUE;
}

static int CmdAssignScriptVar(void)
{
    ACScript->vars[LONG(*PCodePtr++)] = Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAssignMapVar(void)
{
    MapVars[LONG(*PCodePtr++)] = Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAssignWorldVar(void)
{
    WorldVars[LONG(*PCodePtr++)] = Pop();
    return SCRIPT_CONTINUE;
}

static int CmdPushScriptVar(void)
{
    Push(ACScript->vars[LONG(*PCodePtr++)]);
    return SCRIPT_CONTINUE;
}

static int CmdPushMapVar(void)
{
    Push(MapVars[LONG(*PCodePtr++)]);
    return SCRIPT_CONTINUE;
}

static int CmdPushWorldVar(void)
{
    Push(WorldVars[LONG(*PCodePtr++)]);
    return SCRIPT_CONTINUE;
}

static int CmdAddScriptVar(void)
{
    ACScript->vars[LONG(*PCodePtr++)] += Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAddMapVar(void)
{
    MapVars[LONG(*PCodePtr++)] += Pop();
    return SCRIPT_CONTINUE;
}

static int CmdAddWorldVar(void)
{
    WorldVars[LONG(*PCodePtr++)] += Pop();
    return SCRIPT_CONTINUE;
}

static int CmdSubScriptVar(void)
{
    ACScript->vars[LONG(*PCodePtr++)] -= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdSubMapVar(void)
{
    MapVars[LONG(*PCodePtr++)] -= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdSubWorldVar(void)
{
    WorldVars[LONG(*PCodePtr++)] -= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdMulScriptVar(void)
{
    ACScript->vars[LONG(*PCodePtr++)] *= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdMulMapVar(void)
{
    MapVars[LONG(*PCodePtr++)] *= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdMulWorldVar(void)
{
    WorldVars[LONG(*PCodePtr++)] *= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdDivScriptVar(void)
{
    ACScript->vars[LONG(*PCodePtr++)] /= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdDivMapVar(void)
{
    MapVars[LONG(*PCodePtr++)] /= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdDivWorldVar(void)
{
    WorldVars[LONG(*PCodePtr++)] /= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdModScriptVar(void)
{
    ACScript->vars[LONG(*PCodePtr++)] %= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdModMapVar(void)
{
    MapVars[LONG(*PCodePtr++)] %= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdModWorldVar(void)
{
    WorldVars[LONG(*PCodePtr++)] %= Pop();
    return SCRIPT_CONTINUE;
}

static int CmdIncScriptVar(void)
{
    ACScript->vars[LONG(*PCodePtr++)]++;
    return SCRIPT_CONTINUE;
}

static int CmdIncMapVar(void)
{
    MapVars[LONG(*PCodePtr++)]++;
    return SCRIPT_CONTINUE;
}

static int CmdIncWorldVar(void)
{
    WorldVars[LONG(*PCodePtr++)]++;
    return SCRIPT_CONTINUE;
}

static int CmdDecScriptVar(void)
{
    ACScript->vars[LONG(*PCodePtr++)]--;
    return SCRIPT_CONTINUE;
}

static int CmdDecMapVar(void)
{
    MapVars[LONG(*PCodePtr++)]--;
    return SCRIPT_CONTINUE;
}

static int CmdDecWorldVar(void)
{
    WorldVars[LONG(*PCodePtr++)]--;
    return SCRIPT_CONTINUE;
}

static int CmdGoto(void)
{
    PCodePtr = (int *) (ActionCodeBase + LONG(*PCodePtr));
    return SCRIPT_CONTINUE;
}

static int CmdIfGoto(void)
{
    if(Pop())
    {
        PCodePtr = (int *) (ActionCodeBase + LONG(*PCodePtr));
    }
    else
    {
        PCodePtr++;
    }
    return SCRIPT_CONTINUE;
}

static int CmdDrop(void)
{
    Drop();
    return SCRIPT_CONTINUE;
}

static int CmdDelay(void)
{
    ACScript->delayCount = Pop();
    return SCRIPT_STOP;
}

static int CmdDelayDirect(void)
{
    ACScript->delayCount = LONG(*PCodePtr++);
    return SCRIPT_STOP;
}

static int CmdRandom(void)
{
    int                 low, high;

    high = Pop();
    low = Pop();
    Push(low + (P_Random() % (high - low + 1)));
    return SCRIPT_CONTINUE;
}

static int CmdRandomDirect(void)
{
    int                 low, high;

    low = LONG(*PCodePtr++);
    high = LONG(*PCodePtr++);
    Push(low + (P_Random() % (high - low + 1)));
    return SCRIPT_CONTINUE;
}

static int CmdThingCount(void)
{
    int                 tid;

    tid = Pop();
    ThingCount(Pop(), tid);
    return SCRIPT_CONTINUE;
}

static int CmdThingCountDirect(void)
{
    int                 type;

    type = LONG(*PCodePtr++);
    ThingCount(type, LONG(*PCodePtr++));
    return SCRIPT_CONTINUE;
}

typedef struct {
    mobjtype_t          type;
    int                 count;
} countmobjoftypeparams_t;

static int countMobjOfType(void* p, void* context)
{
    countmobjoftypeparams_t* params = (countmobjoftypeparams_t*) context;
    mobj_t*             mo = (mobj_t*) p;

    // Does the type match?
    if(mo->type != params->type)
        return true; // Continue iteration.

    // Minimum health requirement?
    if((mo->flags & MF_COUNTKILL) && mo->health <= 0)
        return true; // Continue iteration.

    params->count++;

    return true; // Continue iteration.
}

static void ThingCount(int type, int tid)
{
    int                 count;
    mobjtype_t          moType;

    if(!(type + tid))
    {   // Nothing to count.
        return;
    }

    moType = TranslateThingType[type];
    count = 0;

    if(tid)
    {   // Count TID things.
        mobj_t*             mo;
        int                 searcher = -1;

        while((mo = P_FindMobjFromTID(tid, &searcher)) != NULL)
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
        DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

        count = params.count;
    }

    Push(count);
}

static int CmdTagWait(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = Pop();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_TAG;
    return SCRIPT_STOP;
}

static int CmdTagWaitDirect(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = LONG(*PCodePtr++);
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_TAG;
    return SCRIPT_STOP;
}

static int CmdPolyWait(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = Pop();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_POLY;
    return SCRIPT_STOP;
}

static int CmdPolyWaitDirect(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = LONG(*PCodePtr++);
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_POLY;
    return SCRIPT_STOP;
}

static int CmdChangeFloor(void)
{
    int                 tag;
    material_t*         mat;
    sector_t*           sec = NULL;
    iterlist_t*         list;

    mat = P_MaterialForName(MN_FLATS, GetACString(Pop()));
    tag = Pop();

    list = P_GetSectorIterListForTag(tag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_FLOOR_MATERIAL, mat);
        }
    }

    return SCRIPT_CONTINUE;
}

static int CmdChangeFloorDirect(void)
{
    int                 tag;
    material_t*         mat;
    sector_t*           sec = NULL;
    iterlist_t*         list;

    tag = LONG(*PCodePtr++);
    mat = P_MaterialForName(MN_FLATS, GetACString(LONG(*PCodePtr++)));

    list = P_GetSectorIterListForTag(tag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_FLOOR_MATERIAL, mat);
        }
    }

    return SCRIPT_CONTINUE;
}

static int CmdChangeCeiling(void)
{
    int                 tag;
    material_t*         mat;
    sector_t*           sec = NULL;
    iterlist_t*         list;

    mat = P_MaterialForName(MN_FLATS, GetACString(Pop()));
    tag = Pop();

    list = P_GetSectorIterListForTag(tag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_CEILING_MATERIAL, mat);
        }
    }

    return SCRIPT_CONTINUE;
}

static int CmdChangeCeilingDirect(void)
{
    int                 tag;
    material_t*         mat;
    sector_t*           sec = NULL;
    iterlist_t*         list;

    tag = LONG(*PCodePtr++);
    mat = P_MaterialForName(MN_FLATS, GetACString(LONG(*PCodePtr++)));

    list = P_GetSectorIterListForTag(tag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((sec = P_IterListIterator(list)) != NULL)
        {
            DMU_SetPtrp(sec, DMU_CEILING_MATERIAL, mat);
        }
    }

    return SCRIPT_CONTINUE;
}

static int CmdRestart(void)
{
    PCodePtr = ACSInfo[ACScript->infoIndex].address;
    return SCRIPT_CONTINUE;
}

static int CmdAndLogical(void)
{
    Push(Pop() && Pop());
    return SCRIPT_CONTINUE;
}

static int CmdOrLogical(void)
{
    Push(Pop() || Pop());
    return SCRIPT_CONTINUE;
}

static int CmdAndBitwise(void)
{
    Push(Pop() & Pop());
    return SCRIPT_CONTINUE;
}

static int CmdOrBitwise(void)
{
    Push(Pop() | Pop());
    return SCRIPT_CONTINUE;
}

static int CmdEorBitwise(void)
{
    Push(Pop() ^ Pop());
    return SCRIPT_CONTINUE;
}

static int CmdNegateLogical(void)
{
    Push(!Pop());
    return SCRIPT_CONTINUE;
}

static int CmdLShift(void)
{
    int                 operand2;

    operand2 = Pop();
    Push(Pop() << operand2);
    return SCRIPT_CONTINUE;
}

static int CmdRShift(void)
{
    int                 operand2;

    operand2 = Pop();
    Push(Pop() >> operand2);
    return SCRIPT_CONTINUE;
}

static int CmdUnaryMinus(void)
{
    Push(-Pop());
    return SCRIPT_CONTINUE;
}

static int CmdIfNotGoto(void)
{
    if(Pop())
    {
        PCodePtr++;
    }
    else
    {
        PCodePtr = (int *) (ActionCodeBase + LONG(*PCodePtr));
    }
    return SCRIPT_CONTINUE;
}

static int CmdLineSide(void)
{
    Push(ACScript->side);
    return SCRIPT_CONTINUE;
}

static int CmdScriptWait(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = Pop();
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_SCRIPT;
    return SCRIPT_STOP;
}

static int CmdScriptWaitDirect(void)
{
    ACSInfo[ACScript->infoIndex].waitValue = LONG(*PCodePtr++);
    ACSInfo[ACScript->infoIndex].state = ASTE_WAITING_FOR_SCRIPT;
    return SCRIPT_STOP;
}

static int CmdClearLineSpecial(void)
{
    if(ACScript->line)
    {
        P_ToXLine(ACScript->line)->special = 0;
    }
    return SCRIPT_CONTINUE;
}

static int CmdCaseGoto(void)
{
    if(Top() == LONG(*PCodePtr++))
    {
        PCodePtr = (int *) (ActionCodeBase + LONG(*PCodePtr));
        Drop();
    }
    else
    {
        PCodePtr++;
    }
    return SCRIPT_CONTINUE;
}

static int CmdBeginPrint(void)
{
    *PrintBuffer = 0;
    return SCRIPT_CONTINUE;
}

static int CmdEndPrint(void)
{
    if(ACScript->activator && ACScript->activator->player)
    {
        P_SetMessage(ACScript->activator->player, PrintBuffer, false);
    }
    else
    {
        int                 i;

        // Send to everybody.
        for(i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame)
                P_SetMessage(&players[i], PrintBuffer, false);
    }

    return SCRIPT_CONTINUE;
}

static int CmdEndPrintBold(void)
{
    int                 i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            P_SetYellowMessage(&players[i], PrintBuffer, false);
        }
    }

    return SCRIPT_CONTINUE;
}

static int CmdPrintString(void)
{
    strcat(PrintBuffer, GetACString(Pop()));
    return SCRIPT_CONTINUE;
}

static int CmdPrintNumber(void)
{
    char                tempStr[16];

    sprintf(tempStr, "%d", Pop());
    strcat(PrintBuffer, tempStr);

    return SCRIPT_CONTINUE;
}

static int CmdPrintCharacter(void)
{
    char*               bufferEnd;

    bufferEnd = PrintBuffer + strlen(PrintBuffer);
    *bufferEnd++ = Pop();
    *bufferEnd = 0;

    return SCRIPT_CONTINUE;
}

static int CmdPlayerCount(void)
{
    int                 i, count;

    count = 0;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        count += players[i].plr->inGame;
    }
    Push(count);

    return SCRIPT_CONTINUE;
}

static int CmdGameType(void)
{
    int                 gametype;

    if(IS_NETGAME == false)
    {
        gametype = GAME_SINGLE_PLAYER;
    }
    else if(deathmatch)
    {
        gametype = GAME_NET_DEATHMATCH;
    }
    else
    {
        gametype = GAME_NET_COOPERATIVE;
    }
    Push(gametype);

    return SCRIPT_CONTINUE;
}

static int CmdGameSkill(void)
{
    Push(gameSkill);
    return SCRIPT_CONTINUE;
}

static int CmdTimer(void)
{
    Push(mapTime);
    return SCRIPT_CONTINUE;
}

static int CmdSectorSound(void)
{
    int                 volume;
    mobj_t*             mobj;

    mobj = NULL;
    if(ACScript->line)
    {
        sector_t*           front =
            DMU_GetPtrp(ACScript->line, DMU_FRONT_SECTOR);

        mobj = DMU_GetPtrp(front, DMU_SOUND_ORIGIN);
    }
    volume = Pop();

#if _DEBUG
Con_Printf("CmdSectorSound: volume=%i\n", volume);
#endif

    S_StartSoundAtVolume(S_GetSoundID(GetACString(Pop())), mobj,
                         volume / 127.0f);
    return SCRIPT_CONTINUE;
}

static int CmdThingSound(void)
{
    int                 tid, sound, volume, searcher;
    mobj_t*             mobj;

    volume = Pop();
    sound = S_GetSoundID(GetACString(Pop()));
    tid = Pop();
    searcher = -1;
    while(sound && (mobj = P_FindMobjFromTID(tid, &searcher)) != NULL)
    {
        S_StartSoundAtVolume(sound, mobj, volume / 127.0f);
    }

    return SCRIPT_CONTINUE;
}

static int CmdAmbientSound(void)
{
    int                 volume, sound;
    mobj_t*             mobj = NULL; // For 3D positioning.
    mobj_t*             plrmo = players[DISPLAYPLAYER].plr->mo;

    volume = Pop();
    // If we are playing 3D sounds, create a temporary source mobj
    // for the sound.
    if(cfg.snd3D && plrmo)
    {
        // SpawnMobj calls P_Random. We don't want that
        // the random generator gets out of sync.
        if((mobj = P_SpawnMobj3f(MT_CAMERA,
                                 plrmo->pos[VX] + (((M_Random() - 127) * 2) << FRACBITS),
                                 plrmo->pos[VY] + (((M_Random() - 127) * 2) << FRACBITS),
                                 plrmo->pos[VZ] + (((M_Random() - 127) * 2) << FRACBITS),
                                 0, 0))) // A camera's a good temporary source.
            mobj->tics = 5 * TICSPERSEC; // Five seconds should be enough.
    }

    sound = S_GetSoundID(GetACString(Pop()));
    S_StartSoundAtVolume(sound, mobj, volume / 127.0f);

    return SCRIPT_CONTINUE;
}

static int CmdSoundSequence(void)
{
    mobj_t*             mobj;

    mobj = NULL;
    if(ACScript->line)
    {
        sector_t*           front =
            DMU_GetPtrp(ACScript->line, DMU_FRONT_SECTOR);

        mobj = DMU_GetPtrp(front, DMU_SOUND_ORIGIN);
    }
    SN_StartSequenceName(mobj, GetACString(Pop()));

    return SCRIPT_CONTINUE;
}

static int CmdSetLineTexture(void)
{
    int                 lineTag, side, position;
    material_t*         mat;
    linedef_t*          line;
    iterlist_t*         list;

    mat = P_MaterialForName(MN_TEXTURES, GetACString(Pop()));
    position = Pop();
    side = Pop();
    lineTag = Pop();

    list = P_GetLineIterListForTag(lineTag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((line = P_IterListIterator(list)) != NULL)
        {
            sidedef_t*          sdef =
                DMU_GetPtrp(line, (side == 0? DMU_SIDEDEF0 : DMU_SIDEDEF1));

            if(position == TEXTURE_MIDDLE)
            {
                DMU_SetPtrp(sdef, DMU_MIDDLE_MATERIAL, mat);
            }
            else if(position == TEXTURE_BOTTOM)
            {
                DMU_SetPtrp(sdef, DMU_BOTTOM_MATERIAL, mat);
            }
            else
            {                       // TEXTURE_TOP
                DMU_SetPtrp(sdef, DMU_TOP_MATERIAL, mat);
            }
        }
    }

    return SCRIPT_CONTINUE;
}

static int CmdSetLineBlocking(void)
{
    linedef_t*          line;
    int                 lineTag;
    boolean             blocking;
    iterlist_t*         list;

    blocking = Pop()? DDLF_BLOCKING : 0;
    lineTag = Pop();

    list = P_GetLineIterListForTag(lineTag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((line = P_IterListIterator(list)) != NULL)
        {
            DMU_SetIntp(line, DMU_FLAGS,
                (DMU_GetIntp(line, DMU_FLAGS) & ~DDLF_BLOCKING) | blocking);
        }
    }

    return SCRIPT_CONTINUE;
}

static int CmdSetLineSpecial(void)
{
    linedef_t*          line;
    int                 lineTag;
    int                 special, arg1, arg2, arg3, arg4, arg5;
    iterlist_t*         list;

    arg5 = Pop();
    arg4 = Pop();
    arg3 = Pop();
    arg2 = Pop();
    arg1 = Pop();
    special = Pop();
    lineTag = Pop();

    list = P_GetLineIterListForTag(lineTag, false);
    if(list)
    {
        P_IterListResetIterator(list, true);
        while((line = P_IterListIterator(list)) != NULL)
        {
            xline_t* xline = P_ToXLine(line);
            xline->special = special;
            xline->arg1 = arg1;
            xline->arg2 = arg2;
            xline->arg3 = arg3;
            xline->arg4 = arg4;
            xline->arg5 = arg5;
        }
    }

    return SCRIPT_CONTINUE;
}

// Console commands.
DEFCC(CCmdScriptInfo)
{
    int                 i, whichOne = -1;
    char*               scriptStates[] = {
        "Inactive", "Running", "Suspended", "Waiting for tag",
        "Waiting for poly", "Waiting for script", "Terminating"
    };

    if(argc == 2)
        whichOne = atoi(argv[1]);

    for(i = 0; i < ACScriptCount; ++i)
    {
        acsinfo_t*          aptr = ACSInfo + i;

        if(whichOne != -1 && whichOne != aptr->number)
            continue;

        Con_Printf("%d %s (a: %d, w: %d)\n", aptr->number,
                   scriptStates[aptr->state], aptr->argCount, aptr->waitValue);
    }

    return true;
}
