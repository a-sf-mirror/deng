/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#ifndef LIBDENG2_SCRIPTLEX_H
#define LIBDENG2_SCRIPTLEX_H

#include "../Lex"
#include "../TokenBuffer"

namespace de
{    
    /**
     * Lexical analyzer specific to Doomsday scripts.
     *
     * @ingroup script
     */
    class ScriptLex : public Lex
    {        
    public:
        /// Base error for syntax errors at the level of lexical analysis (e.g., 
        /// a non-terminated string constant). @ingroup errors
        DEFINE_ERROR(SyntaxError);
        
        /// A unexpected character is encountered. @ingroup errors
        DEFINE_SUB_ERROR(SyntaxError, UnexpectedCharacterError);
        
        /// An unterminated string token is encountered. @ingroup errors
        DEFINE_SUB_ERROR(SyntaxError, UnterminatedStringError);
        
        /// The bracket level goes below zero, i.e., when more brackets are closed 
        /// than opened, or when the input ends before all brackets are closed. @ingroup errors
        DEFINE_SUB_ERROR(SyntaxError, MismatchedBracketError);
        
    public:
        ScriptLex(const String& input = "");
        
        /**
         * Analyze one complete statement from the input.
         *
         * @param output Buffer for output tokens.
         *
         * @return  The number of tokens added to the output token buffer.
         */
        duint getStatement(TokenBuffer& output);

        /** 
         * Parse a string. 
         *
         * @param startChar         Character that begun the string. This is already
         *                          in the token.
         * @param startIndentation  Indentation level for the line that starts the token.
         * @param output            Output token buffer.
         *
         * @return Type of the parsed string.
         */
        TokenBuffer::Token::Type parseString(duchar startChar, duint startIndentation, 
            TokenBuffer& output);

    public:
        /// Determines whether a character is an operator character.
        static bool isOperator(duchar c);

        /// Determines whether a token is a Haw script keyword.
        static bool isKeyword(const TokenBuffer::Token& token);
        
        /// Determines whether one character should join another to 
        /// form a longer token.
        static bool combinesWith(duchar a, duchar b);
        
        /// Unescapes a string token into a std::string.
        static String unescapeStringToken(const TokenBuffer::Token& token);
        
        /// Converts a token to a number.
        static ddouble tokenToNumber(const TokenBuffer::Token& token);
    };
}

#endif /* LIBDENG2_SCRIPTLEX_H */
