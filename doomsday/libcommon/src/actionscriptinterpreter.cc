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

using namespace de;

namespace {
#define GAMETYPE_SINGLE         0
#define GAMETYPE_COOPERATIVE    1
#define GAMETYPE_DEATHMATCH     2

#define SIDEDEF_SECTION_TOP     0
#define SIDEDEF_SECTION_MIDDLE  1
#define SIDEDEF_SECTION_BOTTOM  2

typedef dint script_bytecode_stringid_t;

typedef enum {
    SS_INACTIVE,
    SS_RUNNING,
    SS_SUSPENDED,
    SS_WAITING_FOR_TAG,
    SS_WAITING_FOR_POLY,
    SS_WAITING_FOR_SCRIPT,
    SS_TERMINATING
} script_state_t;

typedef struct script_info_s {
    actionscriptid_t scriptId;
    const int*      entryPoint;
    int             argCount;
    script_state_t  scriptState;
    int             waitValue;
} script_info_t;

const dchar* getString(ActionScriptInterpreter::Bytecode* bytecode, script_bytecode_stringid_t id)
{
    assert(id >= 0 && id < bytecode->numStrings);
    return bytecode->strings[id];
}

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

ActionScriptInterpreter::ActionScriptInterpreter()
{}

ActionScriptInterpreter::~ActionScriptInterpreter()
{
    unloadBytecode();

    if(_scriptStore)
        free(_scriptStore);
}

ActionScriptThinker* ActionScriptInterpreter::createActionScriptThinker(GameMap* map,
    actionscriptid_t scriptId, const dint* bytecodePos, dint delayCount,
    dint infoIndex, Thing* activator, LineDef* lineDef, dint lineSide,
    const dbyte* args, dint numArgs)
{
    ActionScriptThinker* script = new ActionScriptThinker(scriptId, bytecodePos,
        delayCount, infoIndex, activator, lineDef, lineSide, args, numArgs);
    App::currentMap().add(script);
    return script;
}

bool ActionScriptInterpreter::startScript(actionscriptid_t scriptId, duint map,
    dbyte* args, Thing* activator, LineDef* lineDef, dint lineSide,
    ActionScriptThinker** newScript)
{
    ActionScriptThinker* script;
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

void ActionScriptInterpreter::startAll(duint map)
{
    dint i = 0, origSize = _scriptStoreSize;
    while(i < _scriptStoreSize)
    {
        script_store_t* store = &_scriptStore[i];
        ActionScriptThinker* newScript;

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
