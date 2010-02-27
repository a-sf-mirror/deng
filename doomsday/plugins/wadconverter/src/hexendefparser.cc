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

#include <de/String>
#include <de/Lex>

#include "wadconverter/HexenDefParser"

using namespace wadconverter;

HexenDefParser::HexenDefParser()
{}

HexenDefParser::~HexenDefParser()
{}

HexenDefParser::parse(const de::String& script)
{
    _lex = HexLex(script);

    try
    {
        while(getString())
        {
            if(!_token.compareWithCase("flat"))
            {
                parseAnimGroup(MN_FLATS);
                continue;
            }
            if(!_token.compareWithCase("texture"))
            {
                parseAnimGroup(MN_TEXTURES);
                continue;
            }

            std::ostringstream os;
            os << "Unexpected token '" << _token << "' at line #" << _analyzer.lineNumber();
            /// @throw UnexpectedTokenError An unknown/unexpected token was encountered.
            throw UnexpectedTokenError("WadConverter::LoadANIMDEFS", os.str());
        }
    }
    catch(const OutOfInputError&)
    {
        /// Inevitable, sooner or later.
    }
}

void HexenDefParser::skipToNextLine()
{
    _lex.skipToNextLine();
}

bool HexenDefParser::getString(void)
{
    _lex.skipWhite();

    char* text = string;
    if(*_scriptPtr == QUOTE)
    {   // Quoted string.
        _scriptPtr++;
        while(*_scriptPtr != QUOTE)
        {
            *text++ = *_scriptPtr++;
            if(!(_script.offset() < _script.source().size()) ||
               text == &string[MAX_STRING_SIZE - 1])
            {
                break;
            }
        }

        _scriptPtr++;
    }
    else
    {   // Normal string.
        while(!(*_scriptPtr < '!') && *_scriptPtr != BEGIN_LINE_COMMENT)
        {
            *text++ = *_scriptPtr++;
            if(!(_script.offset() < _script.source().size()) ||
               text == &string[MAX_STRING_SIZE - 1])
            {
                break;
            }
        }
    }
    *text = 0;

    return true;
}

void HexenDefParser::mustGetString(void)
{
    if(!getString())
        scriptError("Missing string.");
}

void HexenDefParser::mustGetStringName(char* name)
{
    mustGetString();
    if(!compare(name))
        scriptError(NULL);
}

bool HexenDefParser::getNumber(void)
{
    char* stopper;

    if(getString())
    {
        number = strtol(string, &stopper, 0);
        if(*stopper != 0)
        {
            Con_Error("SC_GetNumber: Bad numeric constant \"%s\".\n"
                      "Script %s, Line %d", string, scriptName, lineNumber);
        }
        return true;
    }
    return false;
}

void HexenDefParser::mustGetNumber(void)
{
    if(!getNumber())
        scriptError("Missing integer.");
}

void HexenDefParser::unGet(void)
{
    _alreadyGot = true;
}

int HexenDefParser::matchString(char** strings)
{
    assert(strings);
    for(int i = 0; *strings != NULL; ++i)
    {
        if(compare(*strings++))
        {
            return i;
        }
    }
    return -1;
}

int HexenDefParser::mustMatchString(char** strings)
{
    assert(strings);
    int i = matchString(strings);
    if(i == -1)
    {
        scriptError(NULL);
    }
    return i;
}

void HexenDefParser::scriptError(char* message)
{
    if(message == NULL)
    {
        message = "Bad syntax.";
    }
    Con_Error("Script error, \"%s\" line %d: %s", scriptName, lineNumber, message);
}
