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

#ifndef LIBCOMMON_ACTIONSCRIPT_STATEMENT_H
#define LIBCOMMON_ACTIONSCRIPT_STATEMENT_H

#include <de/deng.h>

#include "common/ActionScriptThinker"

/**
 * The abstract base class for all action script statements.
 *
 * @ingroup actionscript
 */
class Statement
{
public:
    /// Deserialization of a statement failed. @ingroup errors
    DEFINE_ERROR(DeserializationError);

public:
    virtual ~Statement() {}

    virtual ActionScriptThinker::ProcessAction execute(ActionScriptEnvironment& ase, ActionScriptThinker::Process* proc, ActionScriptThinker* script) const = 0;

protected:
    typedef de::duint32 SerialId;
    
    enum SerialIds {
        NOP,
        TERMINATE,
        SUSPEND,
        PUSHNUMBER,
        LSPEC1,
        LSPEC2,
        LSPEC3,
        LSPEC4,
        LSPEC5,
        LSPEC1DIRECT,
        LSPEC2DIRECT,
        LSPEC3DIRECT,
        LSPEC4DIRECT,
        LSPEC5DIRECT,
        ADD,
        SUBTRACT,
        MULTIPLY,
        DIVIDE,
        MODULUS,
        EQ,
        NE,
        LT,
        GT,
        LE,
        GE,
        ASSIGNSCRIPTVAR,
        ASSIGNMAPVAR,
        ASSIGNWORLDVAR,
        PUSHSCRIPTVAR,
        PUSHMAPVAR,
        PUSHWORLDVAR,
        ADDSCRIPTVAR,
        ADDMAPVAR,
        ADDWORLDVAR,
        SUBSCRIPTVAR,
        SUBMAPVAR,
        SUBWORLDVAR,
        MULSCRIPTVAR,
        MULMAPVAR,
        MULWORLDVAR,
        DIVSCRIPTVAR,
        DIVMAPVAR,
        DIVWORLDVAR,
        MODSCRIPTVAR,
        MODMAPVAR,
        MODWORLDVAR,
        INCSCRIPTVAR,
        INCMAPVAR,
        INCWORLDVAR,
        DECSCRIPTVAR,
        DECMAPVAR,
        DECWORLDVAR,
        GOTO,
        IFGOTO,
        DROP,
        DELAY,
        DELAYDIRECT,
        RANDOM,
        RANDOMDIRECT,
        THINGCOUNT,
        THINGCOUNTDIRECT,
        TAGWAIT,
        TAGWAITDIRECT,
        POLYOBJWAIT,
        POLYOBJWAITDIRECT,
        CHANGEFLOOR,
        CHANGEFLOORDIRECT,
        CHANGECEILING,
        CHANGECEILINGDIRECT,
        RESTART,
        ANDLOGICAL,
        ORLOGICAL,
        ANDBITWISE,
        ORBITWISE,
        EORBITWISE,
        NEGATELOGICAL,
        LSHIFT,
        RSHIFT,
        UNARYMINUS,
        IFNOTGOTO,
        LINEDEFSIDE,
        SCRIPTWAIT,
        SCRIPTWAITDIRECT,
        CLEARLINEDEFSPECIAL,
        CASEGOTO,
        BEGINPRINT,
        ENDPRINT,
        PRINTSTRING,
        PRINTNUMBER,
        PRINTCHARACTER,
        PLAYERCOUNT,
        GAMETYPE,
        GAMESKILL,
        TIMER,
        SECTORSOUND,
        AMBIENTSOUND,
        SOUNDSEQUENCE,
        SETSIDEDEFMATERIAL,
        SETLINEDEFBLOCKING,
        SETLINEDEFSPECIAL,
        THINGSOUND,
        ENDPRINTBOLD
    };

public:
    /**
     * Constructs a statement by deserializing one from a reader.
     *
     * @param from  Reader.
     *
     * @return  The deserialized statement. Caller gets ownership.
     */
    static Statement* constructFrom(de::Reader& from);
};

#endif /* LIBCOMMON_ACTIONSCRIPT_STATEMENT_H */
