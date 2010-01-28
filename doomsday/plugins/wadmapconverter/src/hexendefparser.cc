/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * hexendefparser.c: Parser for Hexen definitions (MAPINFO, ANIMDEFS...)
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#include "doomsday.h"
#include "dd_api.h"

#include "wadconverter.h"

using namespace wadconverter;

// MACROS ------------------------------------------------------------------

#define MAX_SCRIPTNAME_LEN      (32)
#define MAX_STRING_SIZE         (64)
#define ASCII_COMMENT           (';')
#define ASCII_QUOTE             (34)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int SC_MatchString(char** strings);
static int SC_MustMatchString(char** strings);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char* sc_String;
int sc_Number;
int sc_LineNumber;
char sc_ScriptName[MAX_SCRIPTNAME_LEN+1];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean reachedScriptEnd;
static boolean sc_FileScripts = false;
static const char* sc_ScriptsDir = "";

static char* scriptBuffer;
static char* scriptPtr;
static char* scriptEndPtr;
static char stringBuffer[MAX_STRING_SIZE];
static boolean scriptOpen = false;
static boolean scriptFreeCLib; // true = de-allocate using free()
static size_t scriptSize;
static boolean alreadyGot = false;
static boolean skipCurrentLine = false;

// CODE --------------------------------------------------------------------

static void checkOpen(void)
{
    if(scriptOpen == false)
    {
        Con_Error("SC_ call before SC_Open().");
    }
}

static void openScriptLump(lumpnum_t lump)
{
    SC_Close();

    strcpy(sc_ScriptName, W_LumpName(lump));

    scriptBuffer = (char *) W_CacheLumpNum(lump, PU_STATIC);
    scriptSize = W_LumpLength(lump);

    scriptFreeCLib = false; // De-allocate using Z_Free()

    scriptPtr = scriptBuffer;
    scriptEndPtr = scriptPtr + scriptSize;
    sc_LineNumber = 1;
    reachedScriptEnd = false;
    scriptOpen = true;
    sc_String = stringBuffer;
    alreadyGot = false;
}

static void openScriptFile(const char* name)
{
    SC_Close();

    scriptSize = M_ReadFile(name, (byte **) &scriptBuffer);
    M_ExtractFileBase(sc_ScriptName, name, MAX_SCRIPTNAME_LEN);
    scriptFreeCLib = false; // De-allocate using Z_Free()

    scriptPtr = scriptBuffer;
    scriptEndPtr = scriptPtr + scriptSize;
    sc_LineNumber = 1;
    reachedScriptEnd = false;
    scriptOpen = true;
    sc_String = stringBuffer;
    alreadyGot = false;
}

static void openScriptCLib(const char* name)
{
    SC_Close();

    scriptSize = M_ReadFileCLib(name, (byte **) &scriptBuffer);
    M_ExtractFileBase(sc_ScriptName, name, MAX_SCRIPTNAME_LEN);
    scriptFreeCLib = true;  // De-allocate using free()

    scriptPtr = scriptBuffer;
    scriptEndPtr = scriptPtr + scriptSize;
    sc_LineNumber = 1;
    reachedScriptEnd = false;
    scriptOpen = true;
    sc_String = stringBuffer;
    alreadyGot = false;
}

static void SC_Open(const char* name)
{
    char fileName[128];

    if(sc_FileScripts)
    {
        dd_snprintf(fileName, 128, "%s%s.txt", sc_ScriptsDir, name);
        SC_OpenFile(fileName);
    }
    else
    {
        lumpnum_t lump = W_CheckNumForName(name);

        if(lump == -1)
            Con_Error("SC_Open: Failed opening lump %s.\n", name);

        SC_OpenLump(lump);
    }
}

/**
 * Loads a script (from the WAD files) and prepares it for parsing.
 */
void wadconverter::SC_OpenLump(lumpnum_t lump)
{
    openScriptLump(lump);
}

/**
 * Loads a script (from a file) and prepares it for parsing.  Uses the
 * zone memory allocator for memory allocation and de-allocation.
 */
void wadconverter::SC_OpenFile(const char* name)
{
    openScriptFile(name);
}

/**
 * Loads a script (from a file) and prepares it for parsing.  Uses C
 * library function calls for memory allocation and de-allocation.
 */
void wadconverter::SC_OpenFileCLib(const char* name)
{
    openScriptCLib(name);
}

void wadconverter::SC_Close(void)
{
    if(scriptOpen)
    {
        if(scriptFreeCLib)
        {
            free(scriptBuffer);
        }
        else
        {
            Z_Free(scriptBuffer);
        }

        scriptOpen = false;
    }
}

void wadconverter::SC_SkipToStartOfNextLine(void)
{
    skipCurrentLine = true;
}

boolean wadconverter::SC_GetString(void)
{
    char* text;
    boolean foundToken;

    checkOpen();
    if(alreadyGot)
    {
        alreadyGot = false;
        return true;
    }

    foundToken = false;

    if(scriptPtr >= scriptEndPtr)
    {
        reachedScriptEnd = true;
        return false;
    }

    while(foundToken == false)
    {
        while(*scriptPtr <= 32)
        {
            if(scriptPtr >= scriptEndPtr)
            {
                reachedScriptEnd = true;
                return false;
            }

            if(*scriptPtr++ == '\n')
            {
                sc_LineNumber++;
                if(skipCurrentLine)
                    skipCurrentLine = false;
            }
        }

        if(scriptPtr >= scriptEndPtr)
        {
            reachedScriptEnd = true;
            return false;
        }

        if(skipCurrentLine || *scriptPtr == ASCII_COMMENT)
        {
            while(*scriptPtr++ != '\n')
            {
                if(scriptPtr >= scriptEndPtr)
                {
                    reachedScriptEnd = true;
                    return false;

                }
            }

            sc_LineNumber++;
            if(skipCurrentLine)
                skipCurrentLine = false;
        }
        else
        {   // Found a token
            foundToken = true;
        }
    }

    text = sc_String;

    if(*scriptPtr == ASCII_QUOTE)
    {   // Quoted string.
        scriptPtr++;
        while(*scriptPtr != ASCII_QUOTE)
        {
            *text++ = *scriptPtr++;
            if(scriptPtr == scriptEndPtr ||
               text == &sc_String[MAX_STRING_SIZE - 1])
            {
                break;
            }
        }

        scriptPtr++;
    }
    else
    {   // Normal string.
        while((*scriptPtr > 32) && (*scriptPtr != ASCII_COMMENT))
        {
            *text++ = *scriptPtr++;
            if(scriptPtr == scriptEndPtr ||
               text == &sc_String[MAX_STRING_SIZE - 1])
            {
                break;
            }
        }
    }

    *text = 0;
    return true;
}

void wadconverter::SC_MustGetString(void)
{
    if(!SC_GetString())
    {
        SC_ScriptError("Missing string.");
    }
}

void wadconverter::SC_MustGetStringName(char* name)
{
    SC_MustGetString();
    if(!SC_Compare(name))
    {
        SC_ScriptError(NULL);
    }
}

boolean wadconverter::SC_GetNumber(void)
{
    char* stopper;

    checkOpen();
    if(SC_GetString())
    {
        sc_Number = strtol(sc_String, &stopper, 0);
        if(*stopper != 0)
        {
            Con_Error("SC_GetNumber: Bad numeric constant \"%s\".\n"
                      "Script %s, Line %d", sc_String, sc_ScriptName, sc_LineNumber);
        }

        return true;
    }

    return false;
}

void wadconverter::SC_MustGetNumber(void)
{
    if(!SC_GetNumber())
    {
        SC_ScriptError("Missing integer.");
    }
}

/**
 * Assumes there is a valid string in sc_String.
 */
void wadconverter::SC_UnGet(void)
{
    alreadyGot = true;
}

/**
 * @return              Index of the first match to sc_String from the
 *                      passed array of strings, ELSE @c -1,.
 */
static int SC_MatchString(char** strings)
{
    int i;

    for(i = 0; *strings != NULL; ++i)
    {
        if(SC_Compare(*strings++))
        {
            return i;
        }
    }

    return -1;
}

static int SC_MustMatchString(char** strings)
{
    int i;

    i = SC_MatchString(strings);
    if(i == -1)
    {
        SC_ScriptError(NULL);
    }

    return i;
}

boolean wadconverter::SC_Compare(char* text)
{
    if(strcasecmp(text, sc_String) == 0)
    {
        return true;
    }

    return false;
}

void wadconverter::SC_ScriptError(char* message)
{
    if(message == NULL)
    {
        message = "Bad syntax.";
    }

    Con_Error("Script error, \"%s\" line %d: %s", sc_ScriptName, sc_LineNumber, message);
}
