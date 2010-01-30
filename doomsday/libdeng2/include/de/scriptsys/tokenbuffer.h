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

#ifndef LIBDENG2_TOKENBUFFER_H
#define LIBDENG2_TOKENBUFFER_H

#include "../deng.h"

#include <vector>

namespace de
{
    class String;
    
    /** 
     * Buffer of tokens, used as an efficient way to compile and store tokens.
     * Does its own memory management: tokens are allocated out of large blocks 
     * of memory.
     *
     * @ingroup script
     */
    class TokenBuffer
    {
    public:
        /**
         * Character sequence allocated out of the token buffer.
         */
        class Token 
        {
        public:
            /// Types for tokens. This much can be analyzed without knowing
            /// anything about the context.
            enum Type {
                UNKNOWN,
                KEYWORD,
                OPERATOR,
                LITERAL_STRING_APOSTROPHE,
                LITERAL_STRING_QUOTED,
                LITERAL_STRING_LONG,
                LITERAL_NUMBER,
                IDENTIFIER
            };
            
        public:
            Token(duchar* begin = 0, duchar* end = 0, duint line = 0) 
                : _type(UNKNOWN), _begin(begin), _end(end), _line(line) {}

            void setType(Type type) { _type = type; }
            
            Type type() const { return _type; }

            /// Returns the address of the beginning of the token.
            /// @return Pointer to the first character of the token.
            const duchar* begin() const { return _begin; }

            /// Returns the address of the end of the token.
            /// @return Pointer to the character just after the last
            /// character of the token.
            const duchar* end() const { return _end; }

            /// Returns the address of the beginning of the token.
            /// @return Pointer to the first character of the token.
            duchar* begin() { return _begin; }
            
            /// Returns the address of the end of the token.
            /// @return Pointer to the character just after the last
            /// character of the token. This is where a new character is
            /// appended when the token is being compiled.
            duchar* end() { return _end; }

            /// Determines the length of the token.
            /// @return Length of the token as number of characters.
            duint size() const { 
                if(!_begin || !_end) return 0; 
                return _end - _begin; 
            }
            
            bool isEmpty() const {
                return !size();
            }
            
            void appendChar(duchar c) {
                *_end++ = c;
            }
            
            /// Determines whether the token equals @c str. Case sensitive.
            /// @return @c true if an exact match, otherwise @c false.
            bool equals(const char* str) const;
            
            /// Determines whether the token begins with @c str. Case 
            /// sensitive. @return @c true if an exact match, otherwise @c false.
            bool beginsWith(const char* str) const;
            
            /// Determines the line on which the token begins in the source.
            duint line() const { return _line; }
            
            /// Converts the token into a String. This is human-readably output, 
            /// with the line number included.
            String asText() const;
            
            /// Converts a substring of the token into a String. 
            /// This includes nothing extra but the text of the token.
            String str() const;
            
            static std::string typeToText(Type type) {
                switch(type)
                {    
                case UNKNOWN:
                    return "UNKNOWN";
                case KEYWORD:
                    return "KEYWORD";
                case OPERATOR:
                    return "OPERATOR";
                case LITERAL_STRING_APOSTROPHE:
                    return "LITERAL_STRING_APOSTROPHE";
                case LITERAL_STRING_QUOTED:
                    return "LITERAL_STRING_QUOTED";
                case LITERAL_STRING_LONG:
                    return "LITERAL_STRING_LONG";
                case LITERAL_NUMBER:
                    return "LITERAL_NUMBER";
                case IDENTIFIER:
                    return "IDENTIFIER";
                }
                return "";
            }
            
        private:
            Type _type;     ///< Type of the token.
            duchar* _begin; ///< Points to the first character.
            duchar* _end;   ///< Points to the last character + 1.
            duint _line;    ///< On which line the token begins.
        };
        
    public:
        /// Attempt to append characters while no token is being formed. @ingroup errors
        DEFINE_ERROR(TokenNotStartedError);
        
        /// Parameter was out of range. @ingroup errors
        DEFINE_ERROR(OutOfRangeError);
        
    public:
        TokenBuffer();
        virtual ~TokenBuffer();
        
        /// Deletes all Tokens, but does not free the token pools.
        void clear();
        
        /// Begins forming a new Token.
        /// @param line Input source line number for the token.
        void newToken(duint line);
        
        /// Appends a character to the Token being formed.
        /// @param c Character to append.
        void appendChar(duchar c);
        
        /// Sets the type of the token being formed.
        void setType(Token::Type type);
        
        /// Finishes the current Token.
        void endToken();
        
        /// Returns the number of tokens in the buffer.
        duint size() const;
        
        bool empty() const { return !size(); }
        
        /// Returns a specific token in the buffer.
        const Token& at(duint i) const;
        
        const TokenBuffer::Token& latest() const;
        
    private:
        /**
         * Tokens are allocated out of Pools. If a pool runs out of
         * space, a new one is created with more space. Existing pools
         * are not resized, because existing Tokens will point to their
         * memory.
         */
        struct Pool {
            duchar* chars;
            duint size;
            duint rover;
            
            Pool() : chars(0), size(0), rover(0) {}
        };
        
        duchar* advanceToPoolWithSpace(duint minimum);
                
        typedef std::vector<Pool> Pools;
        Pools _pools;
        
        typedef std::vector<Token> Tokens;
        Tokens _tokens;
        
        /// Token being currently formed.
        Token* _forming;
        
        /// Index of pool used for token forming.
        duint _formPool;
    };
}

#endif /* LIBDENG2_TOKENBUFFER_H */
