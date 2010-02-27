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

#include <de/String>
#include <de/Lex>

#include "wadconverter.h"

namespace wadconverter
{
    /**
     * Lexical analyser tailored to reading Hexen text definitions such as
     * MAPINFO and ANIMDEFS.
     *
     * Although implemented using de::Lex, the "loose" analysis behaviour of
     * the parser in the original game means we need to alter Lex to fit our
     * needs, hence HexLex :-)
     */
    class HexLex : de::Lex
    {
    public:
        HexLex(const de::String& input = "") : de::Lex(input, ';') {}

    public:
        /// Determines whether a character is whitespace.
        /// @param c Character to check.
        static bool isWhite(de::duchar c) { return c <= ' '; }
    };

    /**
     * Reads Hexen text definition scripts and translates them into a set of
     * Doomsday script sources.
     */
    class HexenDefParser
    {
    public:
        /// A syntax error is detected during the parsing. Note that the HexLex classes 
        /// also define syntax errors. @ingroup errors
        DEFINE_ERROR(SyntaxError);
        
        /// A token is encountered where we don't know what to do with it. @ingroup errors
        DEFINE_SUB_ERROR(SyntaxError, UnexpectedTokenError);

        static const char QUOTE = '"';

    public:
        HexenDefParser();
        ~HexenDefParser();

        void parse(const de::String& input);

        bool getString(void);
        bool getNumber(void);

        void mustGetString(void);
        void mustGetStringName(char* name);
        void mustGetNumber(void);

        /**
         * Assumes there is a valid string in sc_String.
         */
        void unGet(void);

        void skipToNextLine();

        void scriptError(char* message);

    private:
        HexLex _analyzer;

        de::String _token;

        /**
         * @return              Index of the first match to sc_String from the
         *                      passed array of strings, ELSE @c -1,.
         */
        int matchString(char** strings);

        int mustMatchString(char** strings);
    };
}

#endif /* LIBWADCONVERTER_HEXENDEFPARSER_H */
