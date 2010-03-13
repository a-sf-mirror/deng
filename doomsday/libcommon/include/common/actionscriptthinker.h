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

#ifndef LIBCOMMON_ACTIONSCRIPT_THINKER_H
#define LIBCOMMON_ACTIONSCRIPT_THINKER_H

#include <de/Thinker>

#include "common.h"
#include "identifiers.h"

#include "common/ActionScriptInterpreter"

/**
 * Action Script Thinker.
 */
class ActionScriptThinker : public de::Thinker
{
private:
    static const de::dint AST_MAX_VARS = 10;
    static const de::dint AST_STACK_DEPTH = 32;

public:
    de::dint delayCount;
    actionscriptid_t scriptId;
    const de::dint* bytecodePos;
    de::dint infoIndex;
    de::dint stack[AST_STACK_DEPTH];
    de::dint stackDepth;
    de::dint vars[AST_MAX_VARS];
    de::Thing* activator;
    de::LineDef* lineDef;
    de::dint lineSide;

public:
    ActionScriptThinker(actionscriptid_t scriptId, const de::dint* bytecodePos,
        de::dint delayCount, de::dint infoIndex, de::Thing* activator,
        de::LineDef* lineDef, de::dint lineSide, const de::dbyte* args = NULL,
        de::dint numArgs = 0)
      : de::Thinker(SID_ACTIONSCRIPT_THINKER),
        scriptId(scriptId),
        bytecodePos(bytecodePos),
        delayCount(delayCount),
        infoIndex(infoIndex),
        activator(activator),
        lineDef(lineDef),
        lineSide(lineSide),
        stackDepth(0),
    {
        if(args)
        {
            for(de::dint i = 0; i < numArgs; ++i)
                vars[i] = args[i];
        }
    }

    void think(const de::Time::Delta& elapsed);

    // Implements ISerializable.
    void operator >> (de::Writer& to) const;
    void operator << (de::Reader& from);

    static de::Thinker* construct() {
        return new ActionScriptThinker;
    }
};

#endif /* LIBCOMMON_ACTIONSCRIPT_THINKER_H */
