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
 * def_read.c: Doomsday Engine Definition File Reader
 *
 * A GHASTLY MESS!!! This should be rewritten.
 *
 * You have been warned!
 *
 * At the moment the idea is that a lot of macros are used to read a more
 * or less fixed structure of definitions, and if an error occurs then
 * "goto out_of_here;". It leads to a lot more code than an elegant parser
 * would require.
 *
 * The current implementation of the reader is a "structural" approach:
 * the definition file is parsed based on the structure implied by
 * the read tokens. A true parser would have syntax definitions for
 * a bunch of tokens, and the same parsing rules would be applied for
 * everything.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_refresh.h"
#include "de_defs.h"

// XGClass.h is actually a part of the engine.
#include "../../../plugins/common/include/xgclass.h"

// MACROS ------------------------------------------------------------------

#define MAX_RECUR_DEPTH     30
#define MAX_TOKEN_LEN       128

// Some macros.
#define STOPCHAR(x) (isspace(x) || x == ';' || x == '#' || x == '{' \
                    || x == '}' || x == '=' || x == '"' || x == '*' \
                    || x == '|')

#define ISTOKEN(X)  (!stricmp(token, X))

#define READSTR(X)  if(!ReadString(X, sizeof(X))) { \
                    SetError("Syntax error in string value."); \
                    retVal = false; goto ded_end_read; }

#define MISSING_SC_ERROR    SetError("Missing semicolon."); \
                            retVal = false; goto ded_end_read;

#define CHECKSC     if(source->version <= 5) { ReadToken(); if(!ISTOKEN(";")) { MISSING_SC_ERROR; } }

#define FINDBEGIN   while(!ISTOKEN("{") && !source->atEnd) ReadToken();
#define FINDEND     while(!ISTOKEN("}") && !source->atEnd) ReadToken();

#define ISLABEL(X)  (!stricmp(label, X))

#define READLABEL   if(!ReadLabel(label)) { retVal = false; goto ded_end_read; } \
                    if(ISLABEL("}")) break;

#define READLABEL_NOBREAK   if(!ReadLabel(label)) { retVal = false; goto ded_end_read; }

#define FAILURE     retVal = false; goto ded_end_read;
#define READBYTE(X) if(!ReadByte(&X)) { FAILURE }
#define READINT(X)  if(!ReadInt(&X, false)) { FAILURE }
#define READUINT(X) if(!ReadInt(&X, true)) { FAILURE }
#define READFLT(X)  if(!ReadFloat(&X)) { FAILURE }
#define READNBYTEVEC(X,N) if(!ReadNByteVector(X, N)) { FAILURE }
#define READFLAGS(X,P) if(!ReadFlags(&X, P)) { FAILURE }
#define READBLENDMODE(X) if(!ReadBlendmode(&X)) { FAILURE }

#define RV_BYTE(lab, X) if(ISLABEL(lab)) { READBYTE(X); } else
#define RV_INT(lab, X)  if(ISLABEL(lab)) { READINT(X); } else
#define RV_UINT(lab, X) if(ISLABEL(lab)) { READUINT(X); } else
#define RV_FLT(lab, X)  if(ISLABEL(lab)) { READFLT(X); } else
#define RV_VEC(lab, X, N)   if(ISLABEL(lab)) { int b; FINDBEGIN; \
                        for(b=0; b<N; ++b) {READFLT(X[b])} ReadToken(); } else
#define RV_IVEC(lab, X, N)  if(ISLABEL(lab)) { int b; FINDBEGIN; \
                        for(b=0; b<N; ++b) {READINT(X[b])} ReadToken(); } else
#define RV_NBVEC(lab, X, N) if(ISLABEL(lab)) { READNBYTEVEC(X,N); } else
#define RV_STR(lab, X)  if(ISLABEL(lab)) { READSTR(X); } else
#define RV_STR_INT(lab, S, I)   if(ISLABEL(lab)) { if(!ReadString(S,sizeof(S))) \
                                I = strtol(token,0,0); } else
#define RV_FLAGS(lab, X, P) if(ISLABEL(lab)) { READFLAGS(X, P); } else
#define RV_BLENDMODE(lab, X) if(ISLABEL(lab)) { READBLENDMODE(X); } else
#define RV_ANYSTR(lab, X)   if(ISLABEL(lab)) { if(!ReadAnyString(&X)) { FAILURE } } else
#define RV_END          { SetError2("Unknown label.", label); retVal = false; goto ded_end_read; }

#define RV_XGIPARM(lab, S, I, IP) if(ISLABEL(lab)) { \
                   if(xgClassLinks[l->lineClass].iparm[IP].flagPrefix && \
                      xgClassLinks[l->lineClass].iparm[IP].flagPrefix[0]) { \
                   if(!ReadFlags(&I, xgClassLinks[l->lineClass].iparm[IP].flagPrefix)) { \
                     FAILURE } } \
                   else { if(!ReadString(S,sizeof(S))) { \
                     I = strtol(token,0,0); } } } else

// TYPES -------------------------------------------------------------------

typedef struct dedsource_s {
    char*           buffer;
    char*           pos;
    boolean         atEnd;
    int             lineNumber;
    const char*     fileName;
    int             version; // v6 does not require semicolons.
} dedsource_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// the actual classes are game-side
extern xgclass_t* xgClassLinks;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char dedReadError[512];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static dedsource_t sourceStack[MAX_RECUR_DEPTH];
static dedsource_t* source; // Points to the current source.

static char token[MAX_TOKEN_LEN+1];
static char unreadToken[MAX_TOKEN_LEN+1];

// CODE --------------------------------------------------------------------

static char* sdup(const char* str)
{
    char*               newstr;

    if(!str)
        return NULL;

    newstr = M_Malloc(strlen(str) + 1);
    strcpy(newstr, str);
    return newstr;
}

static void SetError(char* str)
{
    sprintf(dedReadError, "Error in %s:\n  Line %i: %s",
            source ? source->fileName : "?", source ? source->lineNumber : 0,
            str);
}

static void SetError2(char* str, char* more)
{
    sprintf(dedReadError, "Error in %s:\n  Line %i: %s (%s)",
            source ? source->fileName : "?", source ? source->lineNumber : 0,
            str, more);
}

/**
 * Reads a single character from the input file. Increments the line
 * number counter if necessary.
 */
static int FGetC(void)
{
    int                 ch = (unsigned char) *source->pos;

    if(ch)
        source->pos++;
    else
        source->atEnd = true;
    if(ch == '\n')
        source->lineNumber++;
    if(ch == '\r')
        return FGetC();

    return ch;
}

/**
 * Undoes an FGetC.
 */
static int FUngetC(int ch)
{
    if(source->atEnd)
        return 0;
    if(ch == '\n')
        source->lineNumber--;
    if(source->pos > source->buffer)
        source->pos--;

    return ch;
}

/**
 * Reads stuff until a newline is found.
 */
static void SkipComment(void)
{
    int                 ch = FGetC();
    boolean             seq = false;

    if(ch == '\n')
        return; // Comment ends right away.

    if(ch != '>') // Single-line comment?
    {
        while(FGetC() != '\n' && !source->atEnd);
    }
    else // Multiline comment?
    {
        while(!source->atEnd)
        {
            ch = FGetC();
            if(seq)
            {
                if(ch == '#')
                    break;
                seq = false;
            }

            if(ch == '<')
                seq = true;
        }
    }
}

static int ReadToken(void)
{
    int                 ch;
    char*               out = token;

    // Has a token been unread?
    if(unreadToken[0])
    {
        strcpy(token, unreadToken);
        strcpy(unreadToken, "");
        return true;
    }

    ch = FGetC();
    if(source->atEnd)
        return false;

    // Skip whitespace and comments in the beginning.
    while((ch == '#' || isspace(ch)))
    {
        if(ch == '#')
            SkipComment();
        ch = FGetC();
        if(source->atEnd)
            return false;
    }

    // Always store the first character.
    *out++ = ch;
    if(STOPCHAR(ch))
    {
        // Stop here.
        *out = 0;
        return true;
    }

    while(!STOPCHAR(ch) && !source->atEnd)
    {
        // Store the character in the buffer.
        ch = FGetC();
        *out++ = ch;
    }
    *(out - 1) = 0; // End token.

    // Put the last read character back in the stream.
    FUngetC(ch);
    return true;
}

static void UnreadToken(const char* token)
{
    // The next time ReadToken() is called, this is returned.
    strcpy(unreadToken, token);
}

/**
 * Current pos in the file is at the first ".
 * Does not expand escape sequences, only checks for \".
 *
 * @return              @c true, if successful.
 */
static int ReadStringEx(char* dest, int maxlen, boolean inside,
                        boolean doubleq)
{
    char*               ptr = dest;
    int                 ch, esc = false, newl = false;

    if(!inside)
    {
        ReadToken();
        if(!ISTOKEN("\""))
            return false;
    }

    // Start reading the characters.
    ch = FGetC();
    while(esc || ch != '"') // The string-end-character.
    {
        if(source->atEnd)
            return false;

        // If a newline is found, skip all whitespace that follows.
        if(newl)
        {
            if(isspace(ch))
            {
                ch = FGetC();
                continue;
            }
            else
            {
                // End the skip.
                newl = false;
            }
        }

        // An escape character?
        if(!esc && ch == '\\')
            esc = true;
        else
        {
            // In case it's something other than \" or \\, just insert
            // the whole sequence as-is.
            if(esc && ch != '"' && ch != '\\')
                *ptr++ = '\\';
            esc = false;
        }
        if(ch == '\n')
            newl = true;

        // Store the character in the buffer.
        if(ptr - dest < maxlen && !esc && !newl)
        {
            *ptr++ = ch;
            if(doubleq && ch == '"')
                *ptr++ = '"';
        }

        // Read the next character, please.
        ch = FGetC();
    }

    // End the string in a null.
    *ptr = 0;

    return true;
}

static int ReadString(char* dest, int maxlen)
{
    return ReadStringEx(dest, maxlen, false, false);
}

/**
 * Read a string of (pretty much) any length.
 */
static int ReadAnyString(char** dest)
{
    char                buffer[0x20000];

    if(!ReadString(buffer, sizeof(buffer)))
        return false;

    // Get rid of the old string.
    if(*dest)
        M_Free(*dest);

    // Make sure it doesn't overflow.
    buffer[sizeof(buffer) - 1] = 0;

    // Make a copy.
    *dest = M_Malloc(strlen(buffer) + 1);
    strcpy(*dest, buffer);

    return true;
}

static int ReadNByteVector(unsigned char* dest, int max)
{
    int                 i;

    FINDBEGIN;
    for(i = 0; i < max; ++i)
    {
        ReadToken();
        if(ISTOKEN("}"))
            return true;
        dest[i] = strtoul(token, 0, 0);
    }
    FINDEND;
    return true;
}

/**
 * @return              @c true, if successful.
 */
static int ReadByte(unsigned char* dest)
{
    ReadToken();
    if(ISTOKEN(";"))
    {
        SetError("Missing integer value.");
        return false;
    }

    *dest = strtoul(token, 0, 0);
    return true;
}

/**
 * @return              @c true, if successful.
 */
static int ReadInt(int* dest, int unsign)
{
    ReadToken();
    if(ISTOKEN(";"))
    {
        SetError("Missing integer value.");
        return false;
    }

    *dest = unsign ? strtoul(token, 0, 0) : strtol(token, 0, 0);
    return true;
}

/**
 * @return              @c true, if successful.
 */
static int ReadFloat(float* dest)
{
    ReadToken();
    if(ISTOKEN(";"))
    {
        SetError("Missing float value.");
        return false;
    }

    *dest = (float) strtod(token, 0);
    return true;
}

static int ReadFlags(int* dest, const char* prefix)
{
    char                flag[1024];

    // By default, no flags are set.
    *dest = 0;

    ReadToken();
    UnreadToken(token);
    if(ISTOKEN("\""))
    {
        // The old format.
        if(!ReadString(flag, sizeof(flag)))
            return false;

        *dest = Def_EvalFlags(flag);
        return true;
    }

    for(;;)
    {
        // Read the flag.
        ReadToken();
        if(prefix)
        {
            strcpy(flag, prefix);
            strcat(flag, token);
        }
        else
        {
            strcpy(flag, token);
        }

        *dest |= Def_EvalFlags(flag);

        if(!ReadToken())
            break;

        if(!ISTOKEN("|")) // | is required for multiple flags.
        {
            UnreadToken(token);
            break;
        }
    }

    return true;
}

static int ReadBlendmode(blendmode_t* dest)
{
    char                flag[1024];
    blendmode_t         bm;

    // By default, the blendmode is "normal".
    *dest = BM_NORMAL;

    ReadToken();
    UnreadToken(token);
    if(ISTOKEN("\""))
    {
        // The old format.
        if(!ReadString(flag, sizeof(flag)))
            return false;

        bm = (blendmode_t) Def_EvalFlags(flag);
        if(bm != BM_NORMAL)
            *dest = bm;

        return true;
    }

    // Read the blendmode.
    ReadToken();

    strcpy(flag, "bm_");
    strcat(flag, token);

    bm = (blendmode_t) Def_EvalFlags(flag);
    if(bm != BM_NORMAL)
        *dest = bm;

    return true;
}

/**
 * @return              @c true, if successful.
 */
static int ReadLabel(char* label)
{
    strcpy(label, "");
    for(;;)
    {
        ReadToken();
        if(source->atEnd)
        {
            SetError("Unexpected end of file.");
            return false;
        }
        if(ISTOKEN("}")) // End block.
        {
            strcpy(label, token);
            return true;
        }
        if(ISTOKEN(";"))
        {
            if(source->version <= 5)
            {
                SetError("Label without value.");
                return false;
            }
            continue; // Semicolons are optional in v6.
        }
        if(ISTOKEN("=") || ISTOKEN("{"))
            break;

        if(label[0])
            strcat(label, " ");
        strcat(label, token);
    }

    return true;
}

static void DED_Include(const char* fileName, directory_t* dir)
{
    filename_t          tmp, path;

    M_TranslatePath(tmp, fileName, FILENAME_T_MAXLEN);
    if(!Dir_IsAbsolute(tmp))
    {
        dd_snprintf(path, FILENAME_T_MAXLEN, "%s%s", dir->path, tmp);
    }
    else
    {
        strncpy(path, tmp, FILENAME_T_MAXLEN);
    }
    Def_ReadProcessDED(path);
    strncpy(token, "", MAX_TOKEN_LEN);
}

static void DED_InitReader(char* buffer, const char* fileName)
{
    if(source && source - sourceStack >= MAX_RECUR_DEPTH)
    {
        Con_Error("DED_InitReader: Include recursion is too deep.\n");
    }

    if(!source)
    {
        // This'll be the first source.
        source = sourceStack;
    }
    else
    {
        // Take the next entry in the stack.
        source++;
    }

    source->pos = source->buffer = buffer;
    source->atEnd = false;
    source->lineNumber = 1;
    source->fileName = fileName;
    source->version = DED_VERSION;
}

static void DED_CloseReader(void)
{
    if(source == sourceStack)
    {
        source = NULL;
    }
    else
    {
        memset(source, 0, sizeof(*source));
        source--;
    }
}

/**
 * @param cond          Condition token. Can be a command line option
 *                      or a game mode.
 * @return              @c true if the condition passes.
 */
static boolean DED_CheckCondition(const char* cond, boolean expected)
{
    boolean             value = false;

    if(cond[0] == '-')
    {
        // It's a command line option.
        value = (ArgCheck(token) != 0);
    }
    else if(isalnum(cond[0]))
    {
        // Then it must be a game mode.
        value = !stricmp(cond, (char *) gx.GetVariable(DD_GAME_MODE));
    }

    return value == expected;
}

/**
 * Reads definitions from the given buffer.
 * The definition is being loaded from @sourcefile (DED or WAD).
 *
 * @param buffer        The data to be read, must be null-terminated.
 * @param sourceFile    Just FYI.
 */
static int DED_ReadData(ded_t* ded, char* buffer, const char* sourceFile)
{
    char                dummy[128], label[128], tmp[256];
    int                 dummyInt, idx, retVal = true;
    int                 prevMobjDefIdx = -1; // For "Copy".
    int                 prevStateDefIdx = -1; // For "Copy"
    int                 prevLightDefIdx = -1; // For "Copy".
    int                 prevMaterialDefIdx = -1; // For "Copy".
    int                 prevModelDefIdx = -1; // For "Copy".
    int                 prevMapInfoDefIdx = -1; // For "Copy".
    int                 prevSkyDefIdx = -1; // For "Copy".
    int                 prevDetailDefIdx = -1; // For "Copy".
    int                 prevGenDefIdx = -1; // For "Copy".
    int                 prevDecorDefIdx = -1; // For "Copy".
    int                 prevRefDefIdx = -1; // For "Copy".
    int                 prevLineTypeDefIdx = -1; // For "Copy".
    int                 prevSectorTypeDefIdx = -1; // For "Copy".
    int                 depth;
    char*               rootStr = 0, *ptr;
    int                 bCopyNext = 0;
    directory_t         fileDir;

    memset(&fileDir, 0, sizeof(fileDir));

    // For including other files -- we must know where we are.
    Dir_FileDir(sourceFile, &fileDir);

    // Get the next entry from the source stack.
    DED_InitReader(buffer, sourceFile);

    while(ReadToken())
    {
        if(ISTOKEN("Copy") || ISTOKEN("*"))
        {
            bCopyNext = true;
            continue; // Read the next token.
        }

        if(ISTOKEN(";"))
        {
            // Unnecessary semicolon? Just skip it.
            continue;
        }

        if(ISTOKEN("SkipIf"))
        {
            boolean             expected = true;

            ReadToken();
            if(ISTOKEN("Not"))
            {
                expected = false;
                ReadToken();
            }

            if(DED_CheckCondition(token, expected))
            {
                // Ah, we're done. Get out of here.
                goto ded_end_read;
            }
            CHECKSC;
        }

        if(ISTOKEN("Include"))
        {
            // A new include.
            READSTR(tmp);
            CHECKSC;

            DED_Include(tmp, &fileDir);
            strcpy(label, "");
        }

        if(ISTOKEN("IncludeIf")) // An optional include.
        {
            boolean             expected = true;

            ReadToken();
            if(ISTOKEN("Not"))
            {
                expected = false;
                ReadToken();
            }

            if(DED_CheckCondition(token, expected))
            {
                READSTR(tmp);
                CHECKSC;

                DED_Include(tmp, &fileDir);
                strcpy(label, "");
            }
            else
            {
                // Arg not found; just skip it.
                READSTR(tmp);
                CHECKSC;
            }
        }

        if(ISTOKEN("ModelPath"))
        {
            // A new model path. Append to the list.
            READSTR(label);
            CHECKSC;

            Dir_ValidDir(label, 128);
            R_AddClassDataPath(DDRC_MODEL, label, true);
        }

        if(ISTOKEN("Header"))
        {
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                if(ISLABEL("Version"))
                {
                    READINT(ded->version);
                    source->version = ded->version;
                }
                else RV_STR("Thing prefix", dummy)
                RV_STR("State prefix", dummy)
                RV_STR("Sprite prefix", dummy)
                RV_STR("Sfx prefix", dummy)
                RV_STR("Mus prefix", dummy)
                RV_STR("Text prefix", dummy)
                RV_STR("Model path", ded->modelPath)
                RV_FLAGS("Common model flags", ded->modelFlags, "df_")
                RV_FLT("Default model scale", ded->modelScale)
                RV_FLT("Default model offset", ded->modelOffset)
                RV_END
                CHECKSC;
            }
        }

        if(ISTOKEN("Flag"))
        {
            // A new flag.
            idx = DED_AddFlag(ded, "", "", 0);
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", ded->flags[idx].id)
                RV_UINT("Value", ded->flags[idx].value)
                RV_STR("Info", ded->flags[idx].text)
                RV_END
                CHECKSC;
            }
        }

        if(ISTOKEN("Thing"))
        {
            ded_mobj_t*         mo;

            // A new thing.
            idx = DED_AddMobj(ded, "");
            mo = &ded->mobjs[idx];

            if(prevMobjDefIdx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(mo, ded->mobjs + prevMobjDefIdx, sizeof(*mo));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", mo->id)
                RV_INT("DoomEd number", mo->doomEdNum)
                RV_STR("Name", mo->name)
                RV_STR("Spawn state", mo->states[SN_SPAWN])
                RV_STR("See state", mo->states[SN_SEE])
                RV_STR("Pain state", mo->states[SN_PAIN])
                RV_STR("Melee state", mo->states[SN_MELEE])
                RV_STR("Missile state", mo->states[SN_MISSILE])
                RV_STR("Crash state", mo->states[SN_CRASH])
                RV_STR("Death state", mo->states[SN_DEATH])
                RV_STR("Xdeath state", mo->states[SN_XDEATH])
                RV_STR("Raise state", mo->states[SN_RAISE])
                RV_STR("See sound", mo->seeSound)
                RV_STR("Attack sound", mo->attackSound)
                RV_STR("Pain sound", mo->painSound)
                RV_STR("Death sound", mo->deathSound)
                RV_STR("Active sound", mo->activeSound)
                RV_INT("Reaction time", mo->reactionTime)
                RV_INT("Pain chance", mo->painChance)
                RV_INT("Spawn health", mo->spawnHealth)
                RV_FLT("Speed", mo->speed)
                RV_FLT("Radius", mo->radius)
                RV_FLT("Height", mo->height)
                RV_INT("Mass", mo->mass)
                RV_INT("Damage", mo->damage)
                RV_FLAGS("Flags", mo->flags[0], "mf_")
                RV_FLAGS("Flags2", mo->flags[1], "mf2_")
                RV_FLAGS("Flags3", mo->flags[2], "mf3_")
                RV_INT("Misc1", mo->misc[0])
                RV_INT("Misc2", mo->misc[1])
                RV_INT("Misc3", mo->misc[2])
                RV_INT("Misc4", mo->misc[3])
                RV_END
                CHECKSC;
            }
            prevMobjDefIdx = idx;
        }

        if(ISTOKEN("State"))
        {
            ded_state_t*        st;

            // A new state.
            idx = DED_AddState(ded, "");
            st = &ded->states[idx];

            if(prevStateDefIdx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(st, ded->states + prevStateDefIdx, sizeof(*st));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", st->id)
                RV_FLAGS("Flags", st->flags, "statef_")
                RV_STR("Sprite", st->sprite.id)
                RV_INT("Frame", st->frame)
                RV_INT("Tics", st->tics)
                RV_STR("Action", st->action)
                RV_STR("Next state", st->nextState)
                RV_INT("Misc1", st->misc[0])
                RV_INT("Misc2", st->misc[1])
                RV_INT("Misc3", st->misc[2])
                RV_ANYSTR("Execute", st->execute)
                RV_END
                CHECKSC;
            }
            prevStateDefIdx = idx;
        }

        if(ISTOKEN("Sprite"))
        {
            // A new sprite.
            idx = DED_AddSprite(ded, "");
            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", ded->sprites[idx].id)
                RV_END
                CHECKSC;
            }
        }

        if(ISTOKEN("Light"))
        {
            ded_light_t*        lig;

            // A new light.
            idx = DED_AddLight(ded, "");
            lig = &ded->lights[idx];

            if(prevLightDefIdx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(lig, ded->lights + prevLightDefIdx, sizeof(*lig));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("State", lig->state)
                RV_STR("Map", lig->uniqueMapID)
                RV_FLT("X Offset", lig->offset[VX])
                RV_FLT("Y Offset", lig->offset[VY])
                RV_VEC("Origin", lig->offset, 3)
                RV_FLT("Size", lig->size)
                RV_FLT("Intensity", lig->size)
                RV_FLT("Red", lig->color[0])
                RV_FLT("Green", lig->color[1])
                RV_FLT("Blue", lig->color[2])
                RV_VEC("Color", lig->color, 3)
                if(ISLABEL("Sector levels"))
                {
                    int     b;
                    FINDBEGIN;
                    for(b = 0; b < 2; ++b)
                    {
                        READFLT(lig->lightLevel[b])
                        lig->lightLevel[b] /= 255.0f;
                        if(lig->lightLevel[b] < 0)
                            lig->lightLevel[b] = 0;
                        else if(lig->lightLevel[b] > 1)
                            lig->lightLevel[b] = 1;
                    }
                    ReadToken();
                }
                else
                RV_FLAGS("Flags", lig->flags, "lgf_")
                RV_STR("Top map", lig->up.id)
                RV_STR("Bottom map", lig->down.id)
                RV_STR("Side map", lig->sides.id)
                RV_STR("Flare map", lig->flare.id)
                RV_FLT("Halo radius", lig->haloRadius)
                RV_END
                CHECKSC;
            }
            prevLightDefIdx = idx;
        }

        if(ISTOKEN("Material"))
        {
            int                 stage = 0;
            uint                layer = 0;
            ded_material_t*     mat;

            // A new material.
            idx = DED_AddMaterial(ded, "");
            mat = &ded->materials[idx];

            if(prevMaterialDefIdx >= 0 && bCopyNext)
            {
                uint                i;

                // Should we copy the previous definition?
                memcpy(mat, ded->materials + prevMaterialDefIdx, sizeof(*mat));

                // Duplicate the stage arrays.
                for(i = 0; i < DED_MAX_MATERIAL_LAYERS; ++i)
                {
                    ded_material_layer_t*   l =
                        &ded->materials[prevMaterialDefIdx].layers[i];

                    if(l->stages)
                    {
                        mat->layers[i].stages =
                            M_Malloc(sizeof(ded_material_layer_stage_t) *
                                               mat->layers[i].stageCount.max);
                        memcpy(mat->layers[i].stages, l->stages,
                               sizeof(ded_material_layer_stage_t) *
                                   mat->layers[i].stageCount.num);
                    }
                }
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", mat->id.name)
                RV_FLAGS("Namespace", mat->id.mnamespace, "mn_")
                RV_FLAGS("Flags", mat->flags, "matf_")
                RV_FLT("Width", mat->width)
                RV_FLT("Height", mat->height)
                if(ISLABEL("Layer"))
                {
                    if(layer >= DED_MAX_MATERIAL_LAYERS)
                        Con_Error("DED_ReadData: Too many material layers "
                                  "(%s).\n", mat->id.name);

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        if(ISLABEL("Stage"))
                        {
                            ded_material_layer_stage_t* st = NULL;

                            if(stage >= mat->layers[layer].stageCount.num)
                            {
                                // Allocate new stage.
                                stage = DED_AddMaterialLayerStage(&mat->layers[layer]);
                            }

                            st = &mat->layers[layer].stages[stage];

                            FINDBEGIN;
                            for(;;)
                            {
                                READLABEL;
                                if(ISLABEL("Texture") || ISLABEL("Flat") ||
                                   ISLABEL("Sprite") || ISLABEL("System"))
                                {
                                    st->type = (
                                        ISLABEL("Texture")? GLT_DOOMTEXTURE :
                                        ISLABEL("Flat")? GLT_FLAT :
                                        ISLABEL("Sprite")? GLT_SPRITE :
                                        GLT_SYSTEM);
                                    READSTR(st->name);
                                }
                                else
                                RV_FLT("X Offset", st->origin[0])
                                RV_FLT("Y Offset", st->origin[1])
                                RV_VEC("Offset", st->origin, 2)
                                RV_FLT("Move Angle", st->moveAngle)
                                RV_FLT("Move Speed", st->moveSpeed)
                                RV_END
                                CHECKSC;
                            }
                            stage++;
                        }
                        else RV_END
                        CHECKSC;
                    }
                    layer++;
                }
                else RV_END
                CHECKSC;
            }
            prevMaterialDefIdx = idx;
        }

        if(ISTOKEN("Model"))
        {
            uint                sub = 0;
            ded_model_t*        mdl, *prevModel = NULL;

            // A new model.
            idx = DED_AddModel(ded, "");
            mdl = &ded->models[idx];

            if(prevModelDefIdx >= 0)
            {
                prevModel = ded->models + prevModelDefIdx;
                // Should we copy the previous definition?
                if(bCopyNext) memcpy(mdl, prevModel, sizeof(*mdl));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", mdl->id)
                RV_STR("State", mdl->state)
                RV_INT("Off", mdl->off)
                RV_STR("Sprite", mdl->sprite.id)
                RV_INT("Sprite frame", mdl->spriteFrame)
                RV_FLAGS("Group", mdl->group, "mg_")
                RV_INT("Selector", mdl->selector)
                RV_FLAGS("Flags", mdl->flags, "df_")
                RV_FLT("Inter", mdl->interMark)
                RV_INT("Skin tics", mdl->skinTics)
                RV_FLT("Resize", mdl->resize)
                if(ISLABEL("Scale"))
                {
                    READFLT(mdl->scale[1]);
                    mdl->scale[0] = mdl->scale[2] = mdl->scale[1];
                }
                else RV_VEC("Scale XYZ", mdl->scale, 3)
                RV_FLT("Offset", mdl->offset[1])
                RV_VEC("Offset XYZ", mdl->offset, 3)
                RV_VEC("Interpolate", mdl->interRange, 2)
                RV_FLT("Shadow radius", mdl->shadowRadius)
                if(ISLABEL("Md2") || ISLABEL("Sub"))
                {
                    if(sub >= DED_MAX_SUB_MODELS)
                        Con_Error("DED_ReadData: Too many submodels (%s).\n",
                        mdl->state);

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_STR("File", mdl->sub[sub].filename.path)
                        RV_STR("Frame", mdl->sub[sub].frame)
                        RV_INT("Frame range", mdl->sub[sub].frameRange)
                        RV_BLENDMODE("Blending mode", mdl->sub[sub].blendMode)
                        RV_INT("Skin", mdl->sub[sub].skin)
                        RV_STR("Skin file", mdl->sub[sub].skinFilename.path)
                        RV_INT("Skin range", mdl->sub[sub].skinRange)
                        RV_VEC("Offset XYZ", mdl->sub[sub].offset, 3)
                        RV_FLAGS("Flags", mdl->sub[sub].flags, "df_")
                        RV_FLT("Transparent", mdl->sub[sub].alpha)
                        RV_FLT("Parm", mdl->sub[sub].parm)
                        RV_BYTE("Selskin mask", mdl->sub[sub].selSkinBits[0])
                        RV_BYTE("Selskin shift", mdl->sub[sub].selSkinBits[1])
                        RV_NBVEC("Selskins", mdl->sub[sub].selSkins, 8)
                        RV_STR("Shiny skin", mdl->sub[sub].shinySkin)
                        RV_FLT("Shiny", mdl->sub[sub].shiny)
                        RV_VEC("Shiny color", mdl->sub[sub].shinyColor, 3)
                        RV_FLT("Shiny reaction", mdl->sub[sub].shinyReact)
                        RV_END
                        CHECKSC;
                    }
                    sub++;
                }
                else RV_END
                CHECKSC;
            }

            // Some post-processing. No point in doing this in a fancy way,
            // the whole reader will be rewritten sooner or later...
            if(prevModel)
            {
                int                 i;

                if(!strcmp(mdl->state, "-"))
                    strcpy(mdl->state, prevModel->state);
                if(!strcmp(mdl->sprite.id, "-"))
                    strcpy(mdl->sprite.id, prevModel->sprite.id);
                //if(!strcmp(mdl->group, "-"))      strcpy(mdl->group,      prevModel->group);
                //if(!strcmp(mdl->flags, "-"))      strcpy(mdl->flags,      prevModel->flags);

                for(i = 0; i < DED_MAX_SUB_MODELS; ++i)
                {
                    if(!strcmp(mdl->sub[i].filename.path, "-"))
                        strcpy(mdl->sub[i].filename.path, prevModel->sub[i].filename.path);

                    if(!strcmp(mdl->sub[i].frame, "-"))
                        strcpy(mdl->sub[i].frame, prevModel->sub[i].frame);

                    //if(!strcmp(mdl->sub[i].flags, "-"))           strcpy(mdl->sub[i].flags,           prevModel->sub[i].flags);
                }
            }

            prevModelDefIdx = idx;
        }

        if(ISTOKEN("Sound"))
        {   // A new sound.
            ded_sound_t*        snd;

            idx = DED_AddSound(ded, "");
            snd = &ded->sounds[idx];

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", snd->id)
                RV_STR("Lump", snd->lumpName)
                RV_STR("Name", snd->name)
                RV_STR("Link", snd->link)
                RV_INT("Link pitch", snd->linkPitch)
                RV_INT("Link volume", snd->linkVolume)
                RV_INT("Priority", snd->priority)
                RV_INT("Max channels", snd->channels)
                RV_INT("Group", snd->group)
                RV_FLAGS("Flags", snd->flags, "sf_")
                RV_STR("Ext", snd->ext.path)
                RV_STR("File", snd->ext.path)
                RV_STR("File name", snd->ext.path)
                RV_END
                CHECKSC;
            }
        }

        if(ISTOKEN("Music"))
        {   // A new music.
            ded_music_t*        mus;

            idx = DED_AddMusic(ded, "");
            mus = &ded->music[idx];

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", mus->id)
                RV_STR("Lump", mus->lumpName)
                RV_STR("File name", mus->path.path)
                RV_STR("File", mus->path.path)
                RV_STR("Ext", mus->path.path) // Both work.
                RV_INT("CD track", mus->cdTrack)
                RV_END
                CHECKSC;
            }
        }

        if(ISTOKEN("Sky"))
        {   // A new sky definition.
            uint                sub;
            ded_sky_t*          sky;

            idx = DED_AddSky(ded, "");
            sky = &ded->skies[idx];

            if(prevSkyDefIdx >= 0 && bCopyNext)
            {
                int                 m;

                // Should we copy the previous definition?
                memcpy(sky, ded->skies + prevSkyDefIdx, sizeof(*sky));
                for(m = 0; m < NUM_SKY_MODELS; ++m)
                {
                    sky->models[m].execute
                        = sdup(sky->models[m].execute);
                }
            }
            prevSkyDefIdx = idx;
            sub = 0;

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", sky->id)
                RV_FLAGS("Flags", sky->flags, "sif_")
                RV_FLT("Height", sky->height)
                RV_FLT("Horizon offset", sky->horizonOffset)
                RV_VEC("Light color", sky->color, 3)
                if(ISLABEL("Layer 1") || ISLABEL("Layer 2"))
                {
                    ded_skylayer_t *sl = sky->layers + atoi(label+6) - 1;
                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        if(ISLABEL("Texture") || ISLABEL("Flat") ||
                           ISLABEL("Sprite") || ISLABEL("System"))
                        {
                            sl->material.mnamespace = (
                                ISLABEL("Texture")? MN_TEXTURES :
                                ISLABEL("Flat")? MN_FLATS :
                                ISLABEL("Sprite")? MN_SPRITES :
                                MN_SYSTEM);
                            READSTR(sl->material.name);
                        }
                        else
                        RV_FLAGS("Flags", sl->flags, "slf_")
                        RV_FLT("Offset", sl->offset[0])
                        RV_FLT("Y Offset", sl->offset[1])
                        RV_FLT("Color limit", sl->colorLimit)
                        RV_END
                        CHECKSC;
                    }
                }
                else if(ISLABEL("Model"))
                {
                    ded_skymodel_t *sm = &sky->models[sub];
                    if(sub == NUM_SKY_MODELS)
                    {   // Too many!
                        SetError("Too many sky models.");
                        retVal = false;
                        goto ded_end_read;
                    }
                    sub++;

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_STR("ID", sm->id)
                        RV_INT("Layer", sm->layer)
                        RV_FLT("Frame interval", sm->frameInterval)
                        RV_FLT("Yaw", sm->yaw)
                        RV_FLT("Yaw speed", sm->yawSpeed)
                        RV_VEC("Rotate", sm->rotate, 2)
                        RV_VEC("Offset factor", sm->coordFactor, 3)
                        RV_VEC("Color", sm->color, 4)
                        RV_ANYSTR("Execute", sm->execute)
                        RV_END
                        CHECKSC;
                    }
                }
                else RV_END
                CHECKSC;
            }
        }

        if(ISTOKEN("Map")) // Info
        {   // A new map info.
            uint                sub;
            ded_mapinfo_t*      mi;

            idx = DED_AddMapInfo(ded, "");
            mi = &ded->mapInfo[idx];

            if(prevMapInfoDefIdx >= 0 && bCopyNext)
            {
                int                 m;

                // Should we copy the previous definition?
                memcpy(mi, ded->mapInfo + prevMapInfoDefIdx, sizeof(*mi));
                mi->execute = sdup(mi->execute);
                for(m = 0; m < NUM_SKY_MODELS; ++m)
                {
                    mi->sky.models[m].execute
                        = sdup(mi->sky.models[m].execute);
                }
            }
            prevMapInfoDefIdx = idx;
            sub = 0;

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", mi->id)
                RV_STR("Name", mi->name)
                RV_STR("Author", mi->author)
                RV_FLAGS("Flags", mi->flags, "mif_")
                RV_STR("Music", mi->music)
                RV_FLT("Par time", mi->parTime)
                RV_FLT("Fog color R", mi->fogColor[0])
                RV_FLT("Fog color G", mi->fogColor[1])
                RV_FLT("Fog color B", mi->fogColor[2])
                RV_FLT("Fog start", mi->fogStart)
                RV_FLT("Fog end", mi->fogEnd)
                RV_FLT("Fog density", mi->fogDensity)
                RV_FLT("Ambient light", mi->ambient)
                RV_FLT("Gravity", mi->gravity)
                RV_ANYSTR("Execute", mi->execute)
                RV_STR("Sky", mi->skyID)
                RV_FLT("Sky height", mi->sky.height)
                RV_FLT("Horizon offset", mi->sky.horizonOffset)
                RV_VEC("Sky light color", mi->sky.color, 3)
                if(ISLABEL("Sky Layer 1") || ISLABEL("Sky Layer 2"))
                {
                    ded_skylayer_t *sl = mi->sky.layers + atoi(label+10) - 1;
                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        if(ISLABEL("Texture") || ISLABEL("Flat") ||
                           ISLABEL("Sprite") || ISLABEL("System"))
                        {
                            sl->material.mnamespace = (
                                ISLABEL("Texture")? MN_TEXTURES :
                                ISLABEL("Flat")? MN_FLATS :
                                ISLABEL("Sprite")? MN_SPRITES :
                                MN_SYSTEM);
                            READSTR(sl->material.name);
                        }
                        else
                        RV_FLAGS("Flags", sl->flags, "slf_")
                        RV_FLT("Offset", sl->offset[0])
                        RV_FLT("Y Offset", sl->offset[1])
                        RV_FLT("Color limit", sl->colorLimit)
                        RV_END
                        CHECKSC;
                    }
                }
                else if(ISLABEL("Sky Model"))
                {
                    ded_skymodel_t *sm = &mi->sky.models[sub];
                    if(sub == NUM_SKY_MODELS)
                    {   // Too many!
                        SetError("Too many sky models.");
                        retVal = false;
                        goto ded_end_read;
                    }
                    sub++;

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_STR("ID", sm->id)
                        RV_INT("Layer", sm->layer)
                        RV_FLT("Frame interval", sm->frameInterval)
                        RV_FLT("Yaw", sm->yaw)
                        RV_FLT("Yaw speed", sm->yawSpeed)
                        RV_VEC("Rotate", sm->rotate, 2)
                        RV_VEC("Offset factor", sm->coordFactor, 3)
                        RV_VEC("Color", sm->color, 4)
                        RV_ANYSTR("Execute", sm->execute)
                        RV_END
                        CHECKSC;
                    }
                }
                else RV_END
                CHECKSC;
            }
        }

        if(ISTOKEN("Text"))
        {   // A new text.
            ded_text_t*         txt;

            idx = DED_AddText(ded, "");
            txt = &ded->text[idx];

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", txt->id)
                if(ISLABEL("Text"))
                {
                    // Allocate a 'huge' buffer.
                    char*           temp = M_Malloc(0x10000);

                    if(ReadString(temp, 0xffff))
                    {
                        size_t          len = strlen(temp) + 1;

                        txt->text = M_Realloc(temp, len);
                        //memcpy(ded->text[idx].text, temp, len);
                    }
                    else
                    {
                        M_Free(temp);
                        SetError("Syntax error in text value.");
                        retVal = false;
                        goto ded_end_read;
                    }
                }
                else RV_END
                CHECKSC;
            }
        }

        if(ISTOKEN("Texture")) // Environment
        {   // A new texenv.
            ded_tenviron_t*     tenv;

            idx = DED_AddTextureEnv(ded, "");
            tenv = &ded->textureEnv[idx];

            FINDBEGIN;
            for(;;)
            {
                ded_materialid_t*   mn;

                READLABEL;
                RV_STR("ID", tenv->id)
                if(ISLABEL("Texture") || ISLABEL("Flat") ||
                   ISLABEL("Sprite") || ISLABEL("System"))
                {   // A new material name.
                    mn = DED_NewEntry((void**)&tenv->materials,
                                      &tenv->count, sizeof(*mn));
                    mn->mnamespace = (ISLABEL("Texture")? MN_TEXTURES :
                           ISLABEL("Flat")? MN_FLATS :
                           ISLABEL("Sprite")? MN_SPRITES : MN_SYSTEM);

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_STR("ID", mn->name)
                        RV_END
                        CHECKSC;
                    }
                }
                else RV_END
                CHECKSC;
            }
        }

        if(ISTOKEN("Values")) // String Values
        {
            depth = 0;
            rootStr = M_Calloc(1); // A null string.

            FINDBEGIN;
            for(;;)
            {
                // Get the next label but don't stop on }.
                READLABEL_NOBREAK;
                if(strchr(label, '|'))
                {
                    SetError("Value labels can't include | characters (ASCII 124).");
                    retVal = false;
                    goto ded_end_read;
                }

                if(ISTOKEN("="))
                {
                    // Define a new string.
                    char*               temp = M_Malloc(0x1000); // A 'huge' buffer.

                    if(ReadString(temp, 0xffff))
                    {
                        ded_value_t*        val;

                        // Resize the buffer down to actual string length.
                        temp = M_Realloc(temp, strlen(temp) + 1);

                        // Get a new value entry.
                        idx = DED_AddValue(ded, 0);
                        val = &ded->values[idx];
                        val->text = temp;

                        // Compose the identifier.
                        val->id = M_Malloc(strlen(rootStr) + strlen(label) + 1);
                        strcpy(val->id, rootStr);
                        strcat(val->id, label);
                    }
                    else
                    {
                        M_Free(temp);
                        SetError("Syntax error in string value.");
                        retVal = false;
                        goto ded_end_read;
                    }
                }
                else if(ISTOKEN("{"))
                {
                    // Begin a new group.
                    rootStr = M_Realloc(rootStr, strlen(rootStr)
                        + strlen(label) + 2);
                    strcat(rootStr, label);
                    strcat(rootStr, "|");   // The separator.
                    // Increase group level.
                    depth++;
                    continue;
                }
                else if(ISTOKEN("}"))
                {
                    size_t                  len;

                    // End group.
                    if(!depth)
                        break;   // End root depth.

                    // Decrease level and modify rootStr.
                    depth--;
                    len = strlen(rootStr);
                    rootStr[len-1] = 0; // Remove last |.
                    ptr = strrchr(rootStr, '|');
                    if(ptr)
                    {
                        ptr[1] = 0;
                        rootStr = M_Realloc(rootStr, strlen(rootStr) + 1);
                    }
                    else
                    {
                        // Back to level zero.
                        rootStr = M_Realloc(rootStr, 1);
                        *rootStr = 0;
                    }
                }
                else
                {
                    // Only the above characters are allowed.
                    SetError("Illegal token.");
                    retVal = false;
                    goto ded_end_read;
                }
                CHECKSC;
            }
            M_Free(rootStr);
            rootStr = 0;
        }

        if(ISTOKEN("Detail")) // Detail Texture
        {
            ded_detailtexture_t*    dtl;

            idx = DED_AddDetail(ded, "");
            dtl = &ded->details[idx];

            if(prevDetailDefIdx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(dtl, ded->details + prevDetailDefIdx, sizeof(*dtl));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_FLAGS("Flags", dtl->flags, "dtf_")
                if(ISLABEL("Texture"))
                {
                    READSTR(dtl->material1.name)
                    dtl->material1.mnamespace = MN_TEXTURES;
                }
                else if(ISLABEL("Wall")) // Alias
                {
                    READSTR(dtl->material1.name)
                    dtl->material1.mnamespace = MN_TEXTURES;
                }
                else if(ISLABEL("Flat"))
                {
                    READSTR(dtl->material2.name)
                    dtl->material2.mnamespace = MN_FLATS;
                }
                else if(ISLABEL("Lump"))
                {
                    READSTR(dtl->detailLump.path)
                    dtl->isExternal = false;
                }
                else if(ISLABEL("File"))
                {
                    READSTR(dtl->detailLump.path)
                    dtl->isExternal = true;
                }
                else
                RV_FLT("Scale", dtl->scale)
                RV_FLT("Strength", dtl->strength)
                RV_FLT("Distance", dtl->maxDist)
                RV_END
                CHECKSC;
            }
            prevDetailDefIdx = idx;
        }

        if(ISTOKEN("Reflection")) // Surface reflection
        {
            ded_reflection_t *ref = NULL;

            idx = DED_AddReflection(ded);
            ref = &ded->reflections[idx];

            if(prevRefDefIdx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(ref, ded->reflections + prevRefDefIdx, sizeof(*ref));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_FLAGS("Flags", ref->flags, "rff_")
                RV_FLT("Shininess", ref->shininess)
                RV_VEC("Min color", ref->minColor, 3)
                RV_BLENDMODE("Blending mode", ref->blendMode)
                RV_STR("Shiny map", ref->shinyMap.path)
                RV_STR("Mask map", ref->maskMap.path)
                RV_FLT("Mask width", ref->maskWidth)
                RV_FLT("Mask height", ref->maskHeight)
                if(ISLABEL("Texture"))
                {
                    READSTR(ref->material.name)
                    ref->material.mnamespace = MN_TEXTURES;
                }
                else if(ISLABEL("Flat"))
                {
                    READSTR(ref->material.name)
                    ref->material.mnamespace = MN_FLATS;
                }
                else RV_END
                CHECKSC;
            }
            prevRefDefIdx = idx;
        }

        if(ISTOKEN("Generator")) // Particle Generator
        {
            int                 sub = 0;
            ded_ptcgen_t*       gen;

            idx = DED_AddPtcGen(ded, "");
            gen = &ded->ptcGens[idx];

            if(prevGenDefIdx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(gen, &ded->ptcGens[prevGenDefIdx], sizeof(*gen));

                // Duplicate the stages array.
                if(ded->ptcGens[prevGenDefIdx].stages)
                {
                    gen->stages = M_Malloc(sizeof(ded_ptcstage_t) *
                                           gen->stageCount.max);
                    memcpy(gen->stages, ded->ptcGens[prevGenDefIdx].stages,
                           sizeof(ded_ptcstage_t) * gen->stageCount.num);
                }
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("State", gen->state)
                if(ISLABEL("Flat"))
                {
                    READSTR(gen->material.name)
                    gen->material.mnamespace = MN_FLATS;
                }
                else if(ISLABEL("Texture"))
                {
                    READSTR(gen->material.name)
                    gen->material.mnamespace = MN_TEXTURES;
                }
                else
                RV_STR("Mobj", gen->type)
                RV_STR("Alt mobj", gen->type2)
                RV_STR("Damage mobj", gen->damage)
                RV_STR("Map", gen->map)
                RV_FLAGS("Flags", gen->flags, "gnf_")
                RV_FLT("Speed", gen->speed)
                RV_FLT("Speed Rnd", gen->speedVariance)
                RV_VEC("Vector", gen->vector, 3)
                RV_FLT("Vector Rnd", gen->vectorVariance)
                RV_FLT("Init vector Rnd", gen->initVectorVariance)
                RV_VEC("Center", gen->center, 3)
                RV_INT("Submodel", gen->subModel)
                RV_FLT("Spawn radius", gen->spawnRadius)
                RV_FLT("Min spawn radius", gen->spawnRadiusMin)
                RV_FLT("Distance", gen->maxDist)
                RV_INT("Spawn age", gen->spawnAge)
                RV_INT("Max age", gen->maxAge)
                RV_INT("Particles", gen->particles)
                RV_FLT("Spawn rate", gen->spawnRate)
                RV_FLT("Spawn Rnd", gen->spawnRateVariance)
                RV_INT("Presim", gen->preSim)
                RV_INT("Alt start", gen->altStart)
                RV_FLT("Alt Rnd", gen->altStartVariance)
                RV_VEC("Force axis", gen->forceAxis, 3)
                RV_FLT("Force radius", gen->forceRadius)
                RV_FLT("Force", gen->force)
                RV_VEC("Force origin", gen->forceOrigin, 3)
                if(ISLABEL("Stage"))
                {
                    ded_ptcstage_t *st = NULL;

                    if(sub >= gen->stageCount.num)
                    {
                        // Allocate new stage.
                        sub = DED_AddPtcGenStage(gen);
                    }

                    st = &gen->stages[sub];

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_FLAGS("Type", st->type, "pt_")
                        RV_INT("Tics", st->tics)
                        RV_FLT("Rnd", st->variance)
                        RV_VEC("Color", st->color, 4)
                        RV_FLT("Radius", st->radius)
                        RV_FLT("Radius rnd", st->radiusVariance)
                        RV_FLAGS("Flags", st->flags, "ptf_")
                        RV_FLT("Bounce", st->bounce)
                        RV_FLT("Gravity", st->gravity)
                        RV_FLT("Resistance", st->resistance)
                        RV_STR("Frame", st->frameName)
                        RV_STR("End frame", st->endFrameName)
                        RV_VEC("Spin", st->spin, 2)
                        RV_VEC("Spin resistance", st->spinResistance, 2)
                        RV_STR("Sound", st->sound.name)
                        RV_FLT("Volume", st->sound.volume)
                        RV_STR("Hit sound", st->hitSound.name)
                        RV_FLT("Hit volume", st->hitSound.volume)
                        RV_VEC("Force", st->vectorForce, 3)
                        RV_END
                        CHECKSC;
                    }
                    sub++;
                }
                else RV_END
                CHECKSC;
            }
            prevGenDefIdx = idx;
        }

        if(ISTOKEN("Finale") || ISTOKEN("InFine"))
        {
            ded_finale_t*       fin;

            idx = DED_AddFinale(ded);
            fin = &ded->finales[idx];

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", fin->id)
                RV_STR("Before", fin->before)
                RV_STR("After", fin->after)
                RV_INT("Game", dummyInt)
                if(ISLABEL("Script"))
                {
                    // Allocate an "enormous" 64K buffer.
                    char *temp = M_Calloc(0x10000), *ptr;

                    if(fin->script)
                        M_Free(fin->script);

                    FINDBEGIN;
                    ptr = temp;
                    ReadToken();
                    while(!ISTOKEN("}") && !source->atEnd)
                    {
                        if(ptr != temp)
                            *ptr++ = ' ';

                        strcpy(ptr, token);
                        ptr += strlen(token);
                        if(ISTOKEN("\""))
                        {
                            ReadStringEx(ptr, 0x10000 - (ptr-temp), true, true);
                            ptr += strlen(ptr); // Continue from the null.
                            *ptr++ = '"';
                        }
                        ReadToken();
                    }
                    fin->script = M_Realloc(temp, strlen(temp) + 1);
                }
                else RV_END
                CHECKSC;
            }
        }

        if(ISTOKEN("Decoration"))
        {
            uint                    sub;
            ded_decor_t            *decor;

            idx = DED_AddDecoration(ded);
            decor = &ded->decorations[idx];
            sub = 0;
            if(prevDecorDefIdx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(decor, ded->decorations + prevDecorDefIdx,
                       sizeof(*decor));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_FLAGS("Flags", decor->flags, "dcf_")
                if(ISLABEL("Texture"))
                {
                    READSTR(decor->material.name)
                    decor->material.mnamespace = MN_TEXTURES;
                }
                else if(ISLABEL("Flat"))
                {
                    READSTR(decor->material.name)
                    decor->material.mnamespace = MN_FLATS;
                }
                else if(ISLABEL("Model"))
                {
                    ded_decormodel_t *dm = &decor->models[sub];

                    if(sub == DED_DECOR_NUM_MODELS)
                    {
                        SetError("Too many models in decoration.");
                        retVal = false;
                        goto ded_end_read;
                    }

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_VEC("Offset", dm->pos, 2)
                        RV_FLT("Distance", dm->elevation)
                        RV_IVEC("Pattern offset", dm->patternOffset, 2)
                        RV_IVEC("Pattern skip", dm->patternSkip, 2)
                        if(ISLABEL("Levels"))
                        {
                            int                     b;

                            FINDBEGIN;
                            for(b = 0; b < 2; ++b)
                            {
                                READFLT(dm->lightLevels[b])
                                dm->lightLevels[b] /= 255.0f;
                                if(dm->lightLevels[b] < 0)
                                    dm->lightLevels[b] = 0;
                                else if(dm->lightLevels[b] > 1)
                                    dm->lightLevels[b] = 1;
                            }
                            ReadToken();
                        }
                        else
                        RV_STR("ID", dm->id)
                        RV_FLT("Frame interval", dm->frameInterval)
                        RV_END
                        CHECKSC;
                    }
                    sub++;
                }
                else if(ISLABEL("Light"))
                {
                    ded_decorlight_t *dl = &decor->lights[sub];

                    if(sub == DED_DECOR_NUM_LIGHTS)
                    {
                        SetError("Too many lights in decoration.");
                        retVal = false;
                        goto ded_end_read;
                    }

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_VEC("Offset", dl->pos, 2)
                        RV_FLT("Distance", dl->elevation)
                        RV_VEC("Color", dl->color, 3)
                        RV_FLT("Radius", dl->radius)
                        RV_FLT("Halo radius", dl->haloRadius)
                        RV_IVEC("Pattern offset", dl->patternOffset, 2)
                        RV_IVEC("Pattern skip", dl->patternSkip, 2)
                        if(ISLABEL("Levels"))
                        {
                            int                     b;

                            FINDBEGIN;
                            for(b = 0; b < 2; ++b)
                            {
                                READFLT(dl->lightLevels[b])
                                dl->lightLevels[b] /= 255.0f;
                                if(dl->lightLevels[b] < 0)
                                    dl->lightLevels[b] = 0;
                                else if(dl->lightLevels[b] > 1)
                                    dl->lightLevels[b] = 1;
                            }
                            ReadToken();
                        }
                        else
                        RV_INT("Flare texture", dl->flareTexture)
                        RV_STR("Flare map", dl->flare.id)
                        RV_STR("Top map", dl->up.id)
                        RV_STR("Bottom map", dl->down.id)
                        RV_STR("Side map", dl->sides.id)
                        RV_END
                        CHECKSC;
                    }
                    sub++;
                }
                else RV_END
                CHECKSC;
            }
            prevDecorDefIdx = idx;
        }

        if(ISTOKEN("Group"))
        {
            int                 sub;
            ded_group_t*        grp;

            idx = DED_AddGroup(ded);
            grp = &ded->groups[idx];
            sub = 0;

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                if(ISLABEL("Texture") || ISLABEL("Flat") ||
                   ISLABEL("Sprite") || ISLABEL("System"))
                {
                    ded_group_member_t* memb;

                    if(sub >= grp->count.num)
                    {
                        // Allocate new stage.
                        sub = DED_AddGroupMember(grp);
                    }

                    memb = &grp->members[sub];
                    memb->material.mnamespace = (
                        ISLABEL("Texture")? MN_TEXTURES :
                        ISLABEL("Flat")? MN_FLATS :
                        ISLABEL("Sprite")? MN_SPRITES : MN_SYSTEM);

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_STR("ID", memb->material.name)
                        RV_FLT("Tics", memb->tics)
                        RV_FLT("Random", memb->randomTics)
                        RV_END
                        CHECKSC;
                    }
                    ++sub;
                }
                else RV_FLAGS("Flags", grp->flags, "tgf_")
                RV_END
                CHECKSC;
            }
        }

        /*if(ISTOKEN("XGClass"))     // XG Class
        {
            // A new XG Class definition
            idx = DED_AddXGClass(ded);
            xgc = ded->xgClasses + idx;
            sub = 0;

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_STR("ID", xgc->id)
                RV_STR("Name", xgc->name)
                if(ISLABEL("Property"))
                {
                    ded_xgclass_property_t *xgcp = NULL;

                    if(sub >= xgc->properties_count.num)
                    {
                        // Allocate new property
                        sub = DED_AddXGClassProperty(xgc);
                    }

                    xgcp = &xgc->properties[sub];

                    FINDBEGIN;
                    for(;;)
                    {
                        READLABEL;
                        RV_FLAGS("ID", xgcp->id, "xgcp_")
                        RV_FLAGS("Flags", xgcp->flags, "xgcpf_")
                        RV_STR("Name", xgcp->name)
                        RV_STR("Flag Group", xgcp->flagprefix)
                        RV_END
                        CHECKSC;
                    }
                    sub++;
                }
                else RV_END
                CHECKSC;
            }
        }*/

        if(ISTOKEN("Line")) // Line Type
        {
            ded_linetype_t*         l;

            // A new line type.
            idx = DED_AddLineType(ded, 0);
            l = &ded->lineTypes[idx];

            if(prevLineTypeDefIdx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(l, ded->lineTypes + prevLineTypeDefIdx, sizeof(*l));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_INT("ID", l->id)
                RV_STR("Comment", l->comment)
                RV_FLAGS("Flags", l->flags[0], "ltf_")
                RV_FLAGS("Flags2", l->flags[1], "ltf2_")
                RV_FLAGS("Flags3", l->flags[2], "ltf3_")
                RV_FLAGS("Class", l->lineClass, "ltc_")
                RV_FLAGS("Type", l->actType, "lat_")
                RV_INT("Count", l->actCount)
                RV_FLT("Time", l->actTime)
                RV_INT("Act tag", l->actTag)
                RV_INT("Ap0", l->aparm[0])
                RV_INT("Ap1", l->aparm[1])
                RV_INT("Ap2", l->aparm[2])
                RV_INT("Ap3", l->aparm[3])
                RV_FLAGS("Ap4", l->aparm[4], "lref_")
                RV_INT("Ap5", l->aparm[5])
                RV_FLAGS("Ap6", l->aparm[6], "lref_")
                RV_INT("Ap7", l->aparm[7])
                RV_INT("Ap8", l->aparm[8])
                RV_STR("Ap9", l->aparm9)
                RV_INT("Health above", l->aparm[0])
                RV_INT("Health below", l->aparm[1])
                RV_INT("Power above", l->aparm[2])
                RV_INT("Power below", l->aparm[3])
                RV_FLAGS("Line act lref", l->aparm[4], "lref_")
                RV_INT("Line act lrefd", l->aparm[5])
                RV_FLAGS("Line inact lref", l->aparm[6], "lref_")
                RV_INT("Line inact lrefd", l->aparm[7])
                RV_INT("Color", l->aparm[8])
                RV_STR("Thing type", l->aparm9)
                RV_FLT("Ticker start time", l->tickerStart)
                RV_FLT("Ticker end time", l->tickerEnd)
                RV_INT("Ticker tics", l->tickerInterval)
                RV_STR("Act sound", l->actSound)
                RV_STR("Deact sound", l->deactSound)
                RV_INT("Event chain", l->evChain)
                RV_INT("Act chain", l->actChain)
                RV_INT("Deact chain", l->deactChain)
                RV_FLAGS("Wall section", l->wallSection, "lws_")
                if(ISLABEL("Act texture"))
                {
                    READSTR(l->actMaterial.name)
                    l->actMaterial.mnamespace = MN_TEXTURES;
                }
                else if(ISLABEL("Act material")) // Alias
                {
                    READSTR(l->actMaterial.name)
                    l->actMaterial.mnamespace = MN_ANY;
                }
                else if(ISLABEL("Deact texture"))
                {
                    READSTR(l->deactMaterial.name)
                    l->deactMaterial.mnamespace = MN_TEXTURES;
                }
                else if(ISLABEL("Deact material")) // Alias
                {
                    READSTR(l->deactMaterial.name)
                    l->deactMaterial.mnamespace = MN_ANY;
                }
                else
                RV_INT("Act type", l->actLineType)
                RV_INT("Deact type", l->deactLineType)
                RV_STR("Act message", l->actMsg)
                RV_STR("Deact message", l->deactMsg)
                RV_FLT("Texmove angle", l->materialMoveAngle)
                RV_FLT("Materialmove angle", l->materialMoveAngle) // Alias
                RV_FLT("Texmove speed", l->materialMoveSpeed)
                RV_FLT("Materialmove speed", l->materialMoveSpeed) // Alias
                RV_STR_INT("Ip0", l->iparmStr[0], l->iparm[0])
                RV_STR_INT("Ip1", l->iparmStr[1], l->iparm[1])
                RV_STR_INT("Ip2", l->iparmStr[2], l->iparm[2])
                RV_STR_INT("Ip3", l->iparmStr[3], l->iparm[3])
                RV_STR_INT("Ip4", l->iparmStr[4], l->iparm[4])
                RV_STR_INT("Ip5", l->iparmStr[5], l->iparm[5])
                RV_STR_INT("Ip6", l->iparmStr[6], l->iparm[6])
                RV_STR_INT("Ip7", l->iparmStr[7], l->iparm[7])
                RV_STR_INT("Ip8", l->iparmStr[8], l->iparm[8])
                RV_STR_INT("Ip9", l->iparmStr[9], l->iparm[9])
                RV_STR_INT("Ip10", l->iparmStr[10], l->iparm[10])
                RV_STR_INT("Ip11", l->iparmStr[11], l->iparm[11])
                RV_STR_INT("Ip12", l->iparmStr[12], l->iparm[12])
                RV_STR_INT("Ip13", l->iparmStr[13], l->iparm[13])
                RV_STR_INT("Ip14", l->iparmStr[14], l->iparm[14])
                RV_STR_INT("Ip15", l->iparmStr[15], l->iparm[15])
                RV_STR_INT("Ip16", l->iparmStr[16], l->iparm[16])
                RV_STR_INT("Ip17", l->iparmStr[17], l->iparm[17])
                RV_STR_INT("Ip18", l->iparmStr[18], l->iparm[18])
                RV_STR_INT("Ip19", l->iparmStr[19], l->iparm[19])
                RV_FLT("Fp0", l->fparm[0])
                RV_FLT("Fp1", l->fparm[1])
                RV_FLT("Fp2", l->fparm[2])
                RV_FLT("Fp3", l->fparm[3])
                RV_FLT("Fp4", l->fparm[4])
                RV_FLT("Fp5", l->fparm[5])
                RV_FLT("Fp6", l->fparm[6])
                RV_FLT("Fp7", l->fparm[7])
                RV_FLT("Fp8", l->fparm[8])
                RV_FLT("Fp9", l->fparm[9])
                RV_FLT("Fp10", l->fparm[10])
                RV_FLT("Fp11", l->fparm[11])
                RV_FLT("Fp12", l->fparm[12])
                RV_FLT("Fp13", l->fparm[13])
                RV_FLT("Fp14", l->fparm[14])
                RV_FLT("Fp15", l->fparm[15])
                RV_FLT("Fp16", l->fparm[16])
                RV_FLT("Fp17", l->fparm[17])
                RV_FLT("Fp18", l->fparm[18])
                RV_FLT("Fp19", l->fparm[19])
                RV_STR("Sp0", l->sparm[0])
                RV_STR("Sp1", l->sparm[1])
                RV_STR("Sp2", l->sparm[2])
                RV_STR("Sp3", l->sparm[3])
                RV_STR("Sp4", l->sparm[4])
                if(l->lineClass)
                {
                    // IpX Alt names can only be used if the class is defined first!
                    // they also support the DED v6 flags format
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[0].name,
                               l->iparmStr[0], l->iparm[0], 0)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[1].name,
                               l->iparmStr[1], l->iparm[1], 1)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[2].name,
                               l->iparmStr[2], l->iparm[2], 2)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[3].name,
                               l->iparmStr[3], l->iparm[3], 3)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[4].name,
                               l->iparmStr[4], l->iparm[4], 4)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[5].name,
                               l->iparmStr[5], l->iparm[5], 5)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[6].name,
                               l->iparmStr[6], l->iparm[6], 6)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[7].name,
                               l->iparmStr[7], l->iparm[7], 7)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[8].name,
                               l->iparmStr[8], l->iparm[8], 8)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[9].name,
                               l->iparmStr[9], l->iparm[9], 9)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[10].name,
                               l->iparmStr[10], l->iparm[10], 10)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[11].name,
                               l->iparmStr[11], l->iparm[11], 11)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[12].name,
                               l->iparmStr[12], l->iparm[12], 12)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[13].name,
                               l->iparmStr[13], l->iparm[13], 13)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[14].name,
                               l->iparmStr[14], l->iparm[14], 14)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[15].name,
                               l->iparmStr[15], l->iparm[15], 15)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[16].name,
                               l->iparmStr[16], l->iparm[16], 16)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[17].name,
                               l->iparmStr[17], l->iparm[17], 17)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[18].name,
                               l->iparmStr[18], l->iparm[18], 18)
                    RV_XGIPARM(xgClassLinks[l->lineClass].iparm[19].name,
                               l->iparmStr[19], l->iparm[19], 19)
                    RV_END

                } else
                RV_END
                CHECKSC;
            }
            prevLineTypeDefIdx = idx;
        }

        if(ISTOKEN("Sector")) // Sector Type
        {
            ded_sectortype_t*   sec;

            // A new sector type.
            idx = DED_AddSectorType(ded, 0);
            sec = &ded->sectorTypes[idx];

            if(prevSectorTypeDefIdx >= 0 && bCopyNext)
            {
                // Should we copy the previous definition?
                memcpy(sec, ded->sectorTypes + prevSectorTypeDefIdx,
                       sizeof(*sec));
            }

            FINDBEGIN;
            for(;;)
            {
                READLABEL;
                RV_INT("ID", sec->id)
                RV_STR("Comment", sec->comment)
                RV_FLAGS("Flags", sec->flags, "stf_")
                RV_INT("Act tag", sec->actTag)
                RV_INT("Floor chain", sec->chain[0])
                RV_INT("Ceiling chain", sec->chain[1])
                RV_INT("Inside chain", sec->chain[2])
                RV_INT("Ticker chain", sec->chain[3])
                RV_FLAGS("Floor chain flags", sec->chainFlags[0], "scef_")
                RV_FLAGS("Ceiling chain flags", sec->chainFlags[1], "scef_")
                RV_FLAGS("Inside chain flags", sec->chainFlags[2], "scef_")
                RV_FLAGS("Ticker chain flags", sec->chainFlags[3], "scef_")
                RV_FLT("Floor chain start time", sec->start[0])
                RV_FLT("Ceiling chain start time", sec->start[1])
                RV_FLT("Inside chain start time", sec->start[2])
                RV_FLT("Ticker chain start time", sec->start[3])
                RV_FLT("Floor chain end time", sec->end[0])
                RV_FLT("Ceiling chain end time", sec->end[1])
                RV_FLT("Inside chain end time", sec->end[2])
                RV_FLT("Ticker chain end time", sec->end[3])
                RV_FLT("Floor chain min interval", sec->interval[0][0])
                RV_FLT("Ceiling chain min interval", sec->interval[1][0])
                RV_FLT("Inside chain min interval", sec->interval[2][0])
                RV_FLT("Ticker chain min interval", sec->interval[3][0])
                RV_FLT("Floor chain max interval", sec->interval[0][1])
                RV_FLT("Ceiling chain max interval", sec->interval[1][1])
                RV_FLT("Inside chain max interval", sec->interval[2][1])
                RV_FLT("Ticker chain max interval", sec->interval[3][1])
                RV_INT("Floor chain count", sec->count[0])
                RV_INT("Ceiling chain count", sec->count[1])
                RV_INT("Inside chain count", sec->count[2])
                RV_INT("Ticker chain count", sec->count[3])
                RV_STR("Ambient sound", sec->ambientSound)
                RV_FLT("Ambient min interval", sec->soundInterval[0])
                RV_FLT("Ambient max interval", sec->soundInterval[1])
                RV_FLT("Floor texmove angle", sec->materialMoveAngle[0])
                RV_FLT("Floor materialmove angle", sec->materialMoveAngle[0]) // Alias
                RV_FLT("Ceiling texmove angle", sec->materialMoveAngle[1])
                RV_FLT("Ceiling materialmove angle", sec->materialMoveAngle[1]) // Alias
                RV_FLT("Floor texmove speed", sec->materialMoveSpeed[0])
                RV_FLT("Floor materialmove speed", sec->materialMoveSpeed[0]) // Alias
                RV_FLT("Ceiling texmove speed", sec->materialMoveSpeed[1])
                RV_FLT("Ceiling materialmove speed", sec->materialMoveSpeed[1]) // Alias
                RV_FLT("Wind angle", sec->windAngle)
                RV_FLT("Wind speed", sec->windSpeed)
                RV_FLT("Vertical wind", sec->verticalWind)
                RV_FLT("Gravity", sec->gravity)
                RV_FLT("Friction", sec->friction)
                RV_STR("Light fn", sec->lightFunc)
                RV_INT("Light fn min tics", sec->lightInterval[0])
                RV_INT("Light fn max tics", sec->lightInterval[1])
                RV_STR("Red fn", sec->colFunc[0])
                RV_STR("Green fn", sec->colFunc[1])
                RV_STR("Blue fn", sec->colFunc[2])
                RV_INT("Red fn min tics", sec->colInterval[0][0])
                RV_INT("Red fn max tics", sec->colInterval[0][1])
                RV_INT("Green fn min tics", sec->colInterval[1][0])
                RV_INT("Green fn max tics", sec->colInterval[1][1])
                RV_INT("Blue fn min tics", sec->colInterval[2][0])
                RV_INT("Blue fn max tics", sec->colInterval[2][1])
                RV_STR("Floor fn", sec->floorFunc)
                RV_FLT("Floor fn scale", sec->floorMul)
                RV_FLT("Floor fn offset", sec->floorOff)
                RV_INT("Floor fn min tics", sec->floorInterval[0])
                RV_INT("Floor fn max tics", sec->floorInterval[1])
                RV_STR("Ceiling fn", sec->ceilFunc)
                RV_FLT("Ceiling fn scale", sec->ceilMul)
                RV_FLT("Ceiling fn offset", sec->ceilOff)
                RV_INT("Ceiling fn min tics", sec->ceilInterval[0])
                RV_INT("Ceiling fn max tics", sec->ceilInterval[1])
                RV_END
                CHECKSC;
            }
            prevSectorTypeDefIdx = idx;
        }
        bCopyNext = false;
    }

ded_end_read:
    M_Free(rootStr);

    // Free the source stack entry we were using.
    DED_CloseReader();

    return retVal;
}
/* *INDENT-ON* */

/**
 * @return              @c true, if the file was successfully loaded.
 */
int DED_Read(ded_t* ded, const char* sPathName)
{
    DFILE*              file;
    char*               defData;
    size_t              len;
    int                 result;
    filename_t          translated;

    M_TranslatePath(translated, sPathName, FILENAME_T_MAXLEN);

    if((file = F_Open(translated, "rb")) == NULL)
    {
        SetError("File can't be opened for reading.");
        return false;
    }

    // Allocate a large enough buffer and read the file.
    F_Seek(file, 0, SEEK_END);
    len = F_Tell(file);
    F_Rewind(file);

    defData = M_Calloc(len + 1);
    F_Read(defData, len, file);
    F_Close(file);

    // Parse the definitions.
    result = DED_ReadData(ded, defData, translated);

    // Now we can free the buffer.
    M_Free(defData);

    return result;
}

/**
 * Reads definitions from the given lump.
 */
int DED_ReadLump(ded_t* ded, lumpnum_t lump)
{
    size_t lumpLength;

    if(lump < 0 || lump >= numLumps)
    {
        SetError("Bad lump number.");
        return false;
    }

    if((lumpLength = W_LumpLength(lump)) > 0)
    {
        void* buf = M_Calloc(lumpLength + 1);
        int result = DED_ReadData(ded, buf, W_LumpSourceFile(lump));

        M_Free(buf);
        return result;
    }

    return false;
}
