/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * animdefs.c: Parser for ANIMDEFS lumps.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <stdlib.h>

#include "doomsday.h"
#include "dd_api.h"

#include "wadmapconverter.h"

// MACROS ------------------------------------------------------------------

#define MAX_SCRIPTNAME_LEN      (32)
#define MAX_STRING_SIZE         (64)
#define ASCII_COMMENT           (';')
#define ASCII_QUOTE             (34)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void     SC_Open(const char* name);
static void     SC_OpenLump(lumpnum_t lump);
static void     SC_OpenFile(const char* name);
static void     SC_OpenFileCLib(const char* name);
static void     SC_Close(void);
static boolean  SC_GetString(void);
static void     SC_MustGetString(void);
static void     SC_MustGetStringName(char* name);
static boolean  SC_GetNumber(void);
static void     SC_MustGetNumber(void);
static void     SC_UnGet(void);

static boolean  SC_Compare(char* text);
static int      SC_MatchString(char** strings);
static int      SC_MustMatchString(char** strings);
static void     SC_ScriptError(char* message);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char* sc_String;
static int sc_Number;
static int sc_Line;
static boolean sc_End;
static boolean sc_Crossed;
static boolean sc_FileScripts = false;
static const char* sc_ScriptsDir = "";

static char ScriptName[MAX_SCRIPTNAME_LEN+1];
static char* ScriptBuffer;
static char* ScriptPtr;
static char* ScriptEndPtr;
static char StringBuffer[MAX_STRING_SIZE];
static boolean ScriptOpen = false;
static boolean ScriptFreeCLib; // true = de-allocate using free()
static size_t ScriptSize;
static boolean AlreadyGot = false;

// CODE --------------------------------------------------------------------

static void checkOpen(void)
{
    if(ScriptOpen == false)
    {
        Con_Error("SC_ call before SC_Open().");
    }
}

static void openScriptLump(lumpnum_t lump)
{
    SC_Close();

    strcpy(ScriptName, W_LumpName(lump));

    ScriptBuffer = (char *) W_CacheLumpNum(lump, PU_STATIC);
    ScriptSize = W_LumpLength(lump);

    ScriptFreeCLib = false; // De-allocate using Z_Free()

    ScriptPtr = ScriptBuffer;
    ScriptEndPtr = ScriptPtr + ScriptSize;
    sc_Line = 1;
    sc_End = false;
    ScriptOpen = true;
    sc_String = StringBuffer;
    AlreadyGot = false;
}

static void openScriptFile(const char* name)
{
    SC_Close();

    ScriptSize = M_ReadFile(name, (byte **) &ScriptBuffer);
    M_ExtractFileBase(ScriptName, name, MAX_SCRIPTNAME_LEN);
    ScriptFreeCLib = false; // De-allocate using Z_Free()

    ScriptPtr = ScriptBuffer;
    ScriptEndPtr = ScriptPtr + ScriptSize;
    sc_Line = 1;
    sc_End = false;
    ScriptOpen = true;
    sc_String = StringBuffer;
    AlreadyGot = false;
}

static void openScriptCLib(const char* name)
{
    SC_Close();

    ScriptSize = M_ReadFileCLib(name, (byte **) &ScriptBuffer);
    M_ExtractFileBase(ScriptName, name, MAX_SCRIPTNAME_LEN);
    ScriptFreeCLib = true;  // De-allocate using free()

    ScriptPtr = ScriptBuffer;
    ScriptEndPtr = ScriptPtr + ScriptSize;
    sc_Line = 1;
    sc_End = false;
    ScriptOpen = true;
    sc_String = StringBuffer;
    AlreadyGot = false;
}

static void SC_Open(const char* name)
{
    char                fileName[128];

    if(sc_FileScripts == true)
    {
        dd_snprintf(fileName, 128, "%s%s.txt", sc_ScriptsDir, name);
        SC_OpenFile(fileName);
    }
    else
    {
        lumpnum_t           lump = W_CheckNumForName(name);

        if(lump == -1)
            Con_Error("SC_Open: Failed opening lump %s.\n", name);

        SC_OpenLump(lump);
    }
}

/**
 * Loads a script (from the WAD files) and prepares it for parsing.
 */
static void SC_OpenLump(lumpnum_t lump)
{
    openScriptLump(lump);
}

/**
 * Loads a script (from a file) and prepares it for parsing.  Uses the
 * zone memory allocator for memory allocation and de-allocation.
 */
static void SC_OpenFile(const char* name)
{
    openScriptFile(name);
}

/**
 * Loads a script (from a file) and prepares it for parsing.  Uses C
 * library function calls for memory allocation and de-allocation.
 */
static void SC_OpenFileCLib(const char* name)
{
    openScriptCLib(name);
}

static void SC_Close(void)
{
    if(ScriptOpen)
    {
        if(ScriptFreeCLib == true)
        {
            free(ScriptBuffer);
        }
        else
        {
            Z_Free(ScriptBuffer);
        }

        ScriptOpen = false;
    }
}

static boolean SC_GetString(void)
{
    char*               text;
    boolean             foundToken;

    checkOpen();
    if(AlreadyGot)
    {
        AlreadyGot = false;
        return true;
    }

    foundToken = false;
    sc_Crossed = false;

    if(ScriptPtr >= ScriptEndPtr)
    {
        sc_End = true;
        return false;
    }

    while(foundToken == false)
    {
        while(*ScriptPtr <= 32)
        {
            if(ScriptPtr >= ScriptEndPtr)
            {
                sc_End = true;
                return false;
            }

            if(*ScriptPtr++ == '\n')
            {
                sc_Line++;
                sc_Crossed = true;
            }
        }

        if(ScriptPtr >= ScriptEndPtr)
        {
            sc_End = true;
            return false;
        }

        if(*ScriptPtr != ASCII_COMMENT)
        {   // Found a token
            foundToken = true;
        }
        else
        {   // Skip comment.
            while(*ScriptPtr++ != '\n')
            {
                if(ScriptPtr >= ScriptEndPtr)
                {
                    sc_End = true;
                    return false;

                }
            }

            sc_Line++;
            sc_Crossed = true;
        }
    }

    text = sc_String;

    if(*ScriptPtr == ASCII_QUOTE)
    {   // Quoted string.
        ScriptPtr++;
        while(*ScriptPtr != ASCII_QUOTE)
        {
            *text++ = *ScriptPtr++;
            if(ScriptPtr == ScriptEndPtr ||
               text == &sc_String[MAX_STRING_SIZE - 1])
            {
                break;
            }
        }

        ScriptPtr++;
    }
    else
    {   // Normal string.
        while((*ScriptPtr > 32) && (*ScriptPtr != ASCII_COMMENT))
        {
            *text++ = *ScriptPtr++;
            if(ScriptPtr == ScriptEndPtr ||
               text == &sc_String[MAX_STRING_SIZE - 1])
            {
                break;
            }
        }
    }

    *text = 0;
    return true;
}

static void SC_MustGetString(void)
{
    if(!SC_GetString())
    {
        SC_ScriptError("Missing string.");
    }
}

static void SC_MustGetStringName(char* name)
{
    SC_MustGetString();
    if(!SC_Compare(name))
    {
        SC_ScriptError(NULL);
    }
}

static boolean SC_GetNumber(void)
{
    char*               stopper;

    checkOpen();
    if(SC_GetString())
    {
        sc_Number = strtol(sc_String, &stopper, 0);
        if(*stopper != 0)
        {
            Con_Error("SC_GetNumber: Bad numeric constant \"%s\".\n"
                      "Script %s, Line %d", sc_String, ScriptName, sc_Line);
        }

        return true;
    }

    return false;
}

static void SC_MustGetNumber(void)
{
    if(!SC_GetNumber())
    {
        SC_ScriptError("Missing integer.");
    }
}

/**
 * Assumes there is a valid string in sc_String.
 */
static void SC_UnGet(void)
{
    AlreadyGot = true;
}

/**
 * @return              Index of the first match to sc_String from the
 *                      passed array of strings, ELSE @c -1,.
 */
static int SC_MatchString(char** strings)
{
    int                 i;

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
    int                 i;

    i = SC_MatchString(strings);
    if(i == -1)
    {
        SC_ScriptError(NULL);
    }

    return i;
}

static boolean SC_Compare(char* text)
{
    if(strcasecmp(text, sc_String) == 0)
    {
        return true;
    }

    return false;
}

static void SC_ScriptError(char* message)
{
    if(message == NULL)
    {
        message = "Bad syntax.";
    }

    Con_Error("Script error, \"%s\" line %d: %s", ScriptName, sc_Line,
              message);
}

static void parseAnimGroup(material_namespace_t mnamespace)
{
    boolean             ignore;
    boolean             done;
    int                 groupNumber = 0, texNumBase = 0, flatNumBase = 0;

    if(!(mnamespace == MN_FLATS || mnamespace == MN_TEXTURES))
        Con_Error("parseAnimGroup: Internal Error, invalid namespace %i.",
                  (int) mnamespace);

    if(!SC_GetString()) // Name.
    {
        SC_ScriptError("Missing string.");
    }

    ignore = true;
    if(mnamespace == MN_TEXTURES)
    {
        if((texNumBase = R_TextureIdForName(MN_TEXTURES, sc_String)) != -1)
            ignore = false;
    }
    else
    {
        if((flatNumBase = R_TextureIdForName(MN_FLATS, sc_String)) != -1)
            ignore = false;
    }

    if(!ignore)
        groupNumber = P_NewMaterialGroup(AGF_SMOOTH | AGF_FIRST_ONLY);

    done = false;
    do
    {
        if(SC_GetString())
        {
            if(SC_Compare("pic"))
            {
                int                 picNum, min, max = 0;

                SC_MustGetNumber();
                picNum = sc_Number;

                SC_MustGetString();
                if(SC_Compare("tics"))
                {
                    SC_MustGetNumber();
                    min = sc_Number;
                }
                else if(SC_Compare("rand"))
                {
                    SC_MustGetNumber();
                    min = sc_Number;
                    SC_MustGetNumber();
                    max = sc_Number;
                }
                else
                {
                    SC_ScriptError(NULL);
                }

                if(!ignore)
                {
                    material_t*         mat = R_MaterialForTextureId(mnamespace,
                        (mnamespace == MN_TEXTURES ? texNumBase : flatNumBase) + picNum - 1);

                    if(mat)
                        P_AddMaterialToGroup(groupNumber, P_ToIndex(mat),
                                             min, (max > 0? max - min : 0));
                }
            }
            else
            {
                SC_UnGet();
                done = true;
            }
        }
        else
        {
            done = true;
        }
    } while(!done);
}

/**
 * Parse an ANIMDEFS definition for flat/texture animations.
 */
void LoadANIMDEFS(void)
{
    lumpnum_t       lump = W_CheckNumForName("ANIMDEFS");

    if(lump != -1)
    {
        SC_OpenLump(lump);

        while(SC_GetString())
        {
            if(SC_Compare("flat"))
            {
                parseAnimGroup(MN_FLATS);
            }
            else if(SC_Compare("texture"))
            {
                parseAnimGroup(MN_TEXTURES);
            }
            else
            {
                SC_ScriptError(NULL);
            }
        }

        SC_Close();
    }
}
