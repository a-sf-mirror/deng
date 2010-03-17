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

#include <algorithm>
#include <vector>

#include <de/Reader>
#include <de/LittleEndianByteOrder>
#include <de/FixedByteArray>

#include <common/ActionScriptBytecodeInterpreter>

using namespace de;

namespace {
bool recognize(const de::File& file, duint32* numScripts, duint32* numStrings)
{
    Reader reader = de::Reader(file, de::littleEndianByteOrder);

    // Smaller than the header?
    if(reader.source().size() < 16)
        return false;

    String magicBytes;
    FixedByteArray byteSeq(magicBytes, 0, 3);
    reader >> byteSeq;
    if(magicBytes.compareWithCase("ACS"))
        return false;
    reader.seek(1);

    // Smaller than the expected size inclusive of the start of script info table?
    duint32 infoOffset;
    reader >> infoOffset;
    if(infoOffset > reader.source().size())
        return false;

    // Smaller than the expected size inclusive of the whole script info table plus
    // the string offset table size value?
    reader >> *numScripts;
    if(infoOffset + 4 + *numScripts * 12 + 4 > reader.source().size())
        return false;

    // Smaller than the expected size inclusive of the string offset table?
    reader >> *numStrings;
    if(infoOffset + 4 + *numScripts * 12 + 4 + *numStrings * 4 > reader.source().size())
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
        std::free(const_cast<IByteArray*>(base)); base = NULL;

    _functions.clear();
    _strings.clear();
}

void ActionScriptBytecodeInterpreter::load(const de::File& file)
{
#define OPEN_SCRIPTS_BASE       1000

    unload();

    duint32 numScripts, numStrings;
    if(!recognize(file, &numScripts, &numStrings))
    {
        LOG_WARNING("ActionScriptBytecodeInterpreter::load: Warning, file \"") << file.name() <<
            "\" is not valid ACS bytecode.";
        return;
    }
    if(numScripts == 0)
        return; // Empty lump.

    Reader reader = de::Reader(file, de::littleEndianByteOrder);

    // Buffer the whole file. The script run time environment still needs access to it.
    base = reinterpret_cast<IByteArray*>(std::malloc(reader.source().size()));
    FixedByteArray byteSeq(*base, 0, reader.source().size());
    reader >> byteSeq;

    // Back to the beginning to load the bytecode.
    reader.setOffset(4);

    // Read the bytecode start offset.
    duint32 infoOffset;
    reader >> infoOffset;

    // Read the bytecode.
    reader.seek(infoOffset);
    for(duint32 i = 0; i < numScripts; ++i)
    {
        duint32 scriptId, entryPointOffset, argCount;

        reader >> scriptId
               >> entryPointOffset
               >> argCount;

        FunctionName name;
        bool callOnMapStart;
        if(scriptId >= OPEN_SCRIPTS_BASE)
        {   // Auto-activate
            name = static_cast<FunctionName>(scriptId - OPEN_SCRIPTS_BASE);
            callOnMapStart = true;
        }
        else
        {
            name = static_cast<FunctionName>(scriptId);
            callOnMapStart = false;
        }

        _functions.insert(std::pair<FunctionName, Function>(name, Function(name, argCount, callOnMapStart, static_cast<IByteArray::Offset>(entryPointOffset))));
    }

    // Read the string table.
    if(numStrings > 0)
    {
        // Seek to the start of the string offset table.
        reader.seek(4);
        // Read the string offset table.
        std::vector<duint32> offsets;
        offsets.reserve(numStrings);
        for(duint32 i = 0; i < numStrings; ++i)
        {
            duint32 offset;
            reader >> offset;
            offsets.push_back(offset);
        }

        // Sort the offsets to ensure a linear seek.
        std::sort(offsets.begin(), offsets.end());

        // Read the strings.
        _strings.reserve(numStrings);
        FOR_EACH(i, offsets, std::vector<duint32>::const_iterator)
        {
            reader.setOffset(*i);
            // Clearly wrong but how do I read construct a String from a cstring of unknown length using reader?
            _strings.push_back(String("dummy"));
        }
    }

#undef OPEN_SCRIPTS_BASE
}
