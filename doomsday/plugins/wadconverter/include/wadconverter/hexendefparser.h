/*
 * The Doomsday Engine Project -- wadconverter
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBWADCONVERTER_HEXENDEFPARSER_H
#define LIBWADCONVERTER_HEXENDEFPARSER_H

#include "wadconverter.h"

namespace wadconverter
{
    class HexenDefParser
    {
    public:
        static const int MAX_SCRIPTNAME_LEN = 32;
        static const int MAX_STRING_SIZE = 64;

        static const char BEGIN_LINE_COMMENT = ';';
        static const char NEWLINE = '\n';
        static const char QUOTE = '"';

    public:
        char* string;
        int number;
        int lineNumber;
        char scriptName[MAX_SCRIPTNAME_LEN+1];

        HexenDefParser(const char* name, const char* script, unsigned int size);

        bool getString(void);
        void mustGetString(void);
        void mustGetStringName(char* name);
        bool getNumber(void);
        void mustGetNumber(void);

        /**
         * Assumes there is a valid string in sc_String.
         */
        void unGet(void);

        void skipToStartOfNextLine(void);
        void scriptError(char* message);
        bool compare(char* text);

    private:
        const char* _script;
        unsigned int _scriptSize;
        const char* _scriptPtr;
        const char* _scriptEndPtr;

        char _stringBuffer[MAX_STRING_SIZE+1];
        bool _alreadyGot;
        bool _reachedScriptEnd;
        bool _skipCurrentLine;

        /**
         * @return              Index of the first match to sc_String from the
         *                      passed array of strings, ELSE @c -1,.
         */
        int matchString(char** strings);

        int mustMatchString(char** strings);
    };
}

#endif /* LIBWADCONVERTER_HEXENDEFPARSER_H */
