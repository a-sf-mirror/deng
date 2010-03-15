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
    static const de::dint MAX_VARS = 10;
    static const de::dint STACK_DEPTH = 32;

public:
    enum ProcessAction {
        CONTINUE,
        STOP,
        TERMINATE
    };

    struct Process {
        de::dint _stack[STACK_DEPTH];
        de::dint _stackDepth;

        /// Local context - set of variables that exist for the life of this process.
        de::dint _context[MAX_VARS];

        /// Callers.
        de::Thing* _activator;
        de::LineDef* _lineDef;
        de::dint _lineSide;

        Process(de::dint numArguments, const de::dbyte* arguments, de::Thing* activator,
                de::LineDef* lineDef, de::dint lineSide)
          : _activator(activator),
            _lineDef(lineDef),
            _lineSide(lineSide),
            _stackDepth(0)
        {
            if(arguments)
            {
                /// Create local variables for the arguments in this context.
                for(de::dint i = 0; i < numArguments; ++i)
                    _context[i] = arguments[i];
            }
        };

        void drop() {
            _stackDepth--;
        }

        de::dint pop() {
            return _stack[--_stackDepth];
        }

        void push(de::dint value) {
            _stack[_stackDepth++] = value;
        }

        de::dint top() const {
            return _stack[_stackDepth - 1];
        }
    } process;

public:
    FunctionName name;

    const de::dint* bytecodePos;
    de::dint delayCount;

public:
    ActionScriptThinker(FunctionName name, const de::dint* bytecodePos,
        de::dint numArguments, const de::dbyte* arguments,
        de::Thing* activator, de::LineDef* lineDef, de::dint lineSide,
        de::dint delayCount = 0)
      : de::Thinker(SID_ACTIONSCRIPT_THINKER),
        name(name),
        bytecodePos(bytecodePos),
        delayCount(delayCount),
        process(numArguments, arguments, activator, lineDef, lineSide)
    {}

    void think(const de::Time::Delta& elapsed);

    // Implements ISerializable.
    void operator >> (de::Writer& to) const;
    void operator << (de::Reader& from);
};

#endif /* LIBCOMMON_ACTIONSCRIPT_THINKER_H */
