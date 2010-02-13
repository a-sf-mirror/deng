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

#include <cstring>

#include "wadconverter/HexenDefParser"

using namespace wadconverter;

HexenDefParser::HexenDefParser(const char* name, const char* script, unsigned int size)
 : _scriptSize(size), _alreadyGot(false), _skipCurrentLine(false),
   _reachedScriptEnd(false)
{
    strncpy(scriptName, name, MAX_SCRIPTNAME_LEN);

    _script = script;
    _scriptPtr = _script;
    _scriptEndPtr = _scriptPtr + _scriptSize;
    lineNumber = 1;
    string = _stringBuffer;
}

void HexenDefParser::skipToStartOfNextLine(void)
{
    _skipCurrentLine = true;
}

bool HexenDefParser::getString(void)
{
    if(_alreadyGot)
    {
        _alreadyGot = false;
        return true;
    }

    if(_scriptPtr >= _scriptEndPtr)
    {
        _reachedScriptEnd = true;
        return false;
    }

    bool foundToken = false;
    while(foundToken == false)
    {
        while(*_scriptPtr <= 32)
        {
            if(_scriptPtr >= _scriptEndPtr)
            {
                _reachedScriptEnd = true;
                return false;
            }

            if(*_scriptPtr++ == NEWLINE)
            {
                lineNumber++;
                if(_skipCurrentLine)
                    _skipCurrentLine = false;
            }
        }

        if(_scriptPtr >= _scriptEndPtr)
        {
            _reachedScriptEnd = true;
            return false;
        }

        if(_skipCurrentLine || *_scriptPtr == BEGIN_LINE_COMMENT)
        {
            while(*_scriptPtr++ != NEWLINE)
            {
                if(_scriptPtr >= _scriptEndPtr)
                {
                    _reachedScriptEnd = true;
                    return false;

                }
            }

            lineNumber++;
            if(_skipCurrentLine)
                _skipCurrentLine = false;
        }
        else
        {   // Found a token
            foundToken = true;
        }
    }

    char* text = string;
    if(*_scriptPtr == QUOTE)
    {   // Quoted string.
        _scriptPtr++;
        while(*_scriptPtr != QUOTE)
        {
            *text++ = *_scriptPtr++;
            if(_scriptPtr == _scriptEndPtr ||
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
            if(_scriptPtr == _scriptEndPtr ||
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

bool HexenDefParser::compare(char* text)
{
    assert(text);
    if(strcasecmp(text, string) == 0)
    {
        return true;
    }
    return false;
}

void HexenDefParser::scriptError(char* message)
{
    if(message == NULL)
    {
        message = "Bad syntax.";
    }

    Con_Error("Script error, \"%s\" line %d: %s", scriptName, lineNumber, message);
}
