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

#include "common/ActionScriptEnvironment" // temp
#include "common/ActionScriptBytecodeInterpreter"

using namespace de;

namespace {
bool recognize(const de::File& file, dint* numScripts, dint* numStrings)
{
    dsize infoOffset;
    dbyte buff[12];

    // Smaller than the bytecode header?
    if(file.size() < 12)
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

ActionScriptBytecodeInterpreter::~ActionScriptBytecodeInterpreter()
{
    unload();
}

void ActionScriptBytecodeInterpreter::unload()
{
    if(base)
        std::free(const_cast<dbyte*>(base)); base = NULL;

    _functions.clear();

    if(_strings)
        std::free(_strings); _strings = NULL;
    _numStrings = 0;
}

void ActionScriptBytecodeInterpreter::load(const de::File& file)
{
#define OPEN_SCRIPTS_BASE       1000

    unload();

    dint numScripts, numStrings;
    if(!recognize(file, &numScripts, &numStrings))
    {
        LOG_WARNING("ActionScriptBytecodeInterpreter::load: Warning, file \"") << file.name() <<
            "\" is not valid ACS bytecode.";
        return;
    }
    if(numScripts == 0)
        return; // Empty lump.

    _numStrings = numStrings;
    base = (const dbyte*) W_CacheLumpNum(lumpNum, PU_STATIC);

    // Load the script info.
    dsize infoOffset = (dsize) LONG(*((dint32*) (base + 4)));

    const dbyte* ptr = (base + infoOffset + 4);
    for(dint i = 0; i < numScripts; ++i, ptr += 12)
    {
        dint _scriptId = (dint) LONG(*((dint32*) (ptr)));
        dsize entryPointOffset = (dsize) LONG(*((dint32*) (ptr + 4)));
        dint argCount = (dint) LONG(*((dint32*) (ptr + 8)));
        const dint* entryPoint = (const dint*) (base + entryPointOffset);

        FunctionName name;
        ActionScriptEnvironment::ScriptState::Status status;
        if(_scriptId >= OPEN_SCRIPTS_BASE)
        {   // Auto-activate
            name = static_cast<FunctionName>(_scriptId - OPEN_SCRIPTS_BASE);
            status = ActionScriptEnvironment::ScriptState::RUNNING;
        }
        else
        {
            name = static_cast<FunctionName>(_scriptId);
            status = ActionScriptEnvironment::ScriptState::INACTIVE;
        }

        _functions.insert(std::pair<FunctionName, Function>(name, Function(name, entryPoint, argCount)));

        if(status == ActionScriptEnvironment::ScriptState::RUNNING)
        {
            // World scripts are allotted 1 second for initialization.
            createActionScriptThinker(P_CurrentMap(), name,
                entryPoint, TICSPERSEC, i, NULL, NULL, 0, NULL, 0);
        }

        _scriptStates[name] = ActionScriptEnvironment::ScriptState(status);
    }

    // Load the string offsets.
    if(_numStrings > 0)
    {
        _strings = reinterpret_cast<dchar const**>(std::malloc(_numStrings * sizeof(dchar*)));

        const dbyte* ptr = (base + infoOffset + 4 + numScripts * 12 + 4);
        for(dint i = 0; i < _numStrings; ++i, ptr += 4)
        {
            _strings[i] = (const dchar*) base + LONG(*((dint32*) (ptr)));
        }
    }

#undef OPEN_SCRIPTS_BASE
}
