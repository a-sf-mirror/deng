/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * con_main.c: Console Subsystem
 *
 * Should be completely redesigned.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>

#include "de_platform.h"

#ifdef WIN32
#   include <process.h>
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_edit.h"
#include "de_play.h"
#include "de_audio.h"
#include "de_ui.h"
#include "de_misc.h"

#ifdef TextOut
// Windows has its own TextOut.
#  undef TextOut
#endif

// MACROS ------------------------------------------------------------------

#define SC_EMPTY_QUOTE  -1

// Length of the print buffer. Used in conPrintf and in Console_Message.
// If console messages are longer than this, an error will occur.
// Needed because we can't sizeof a malloc'd block.
#define PRBUFF_SIZE 655365

// Operators for the "if" command.
enum {
    IF_EQUAL,
    IF_NOT_EQUAL,
    IF_GREATER,
    IF_LESS,
    IF_GEQUAL,
    IF_LEQUAL
};

#define CMDTYPESTR(src) (src == CMDS_DDAY? "a direct call" \
        : src == CMDS_GAME? "a dll call" \
        : src == CMDS_CONSOLE? "the console" \
        : src == CMDS_BIND? "a binding" \
        : src == CMDS_CONFIG? "a cfg file" \
        : src == CMDS_PROFILE? "a player profile" \
        : src == CMDS_CMDLINE? "the command line" \
        : src == CMDS_DED? "an action command" : "???")

// TYPES -------------------------------------------------------------------

typedef struct execbuff_s {
    boolean used;               // Is this in use?
    timespan_t when;            // System time when to execute the command.
    byte    source;             // Where the command came from
                                // (console input, a cfg file etc..)
    boolean isNetCmd;           // Command was sent over the net to us.
    char    subCmd[1024];       // A single command w/args.
} execbuff_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(AddSub);
D_CMD(IncDec);
D_CMD(Alias);
D_CMD(Clear);
D_CMD(Echo);
D_CMD(Font);
D_CMD(Help);
D_CMD(If);
D_CMD(OpenClose);
D_CMD(Parse);
D_CMD(Quit);
D_CMD(Repeat);
D_CMD(Toggle);
D_CMD(Version);
D_CMD(Wait);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void Con_Register(void);
static int executeSubCmd(const char *subCmd, byte src, boolean isNetCmd);
static void Con_SplitIntoSubCommands(const char *command,
                                     timespan_t markerOffset, byte src,
                                     boolean isNetCmd);

static void Con_ClearExecBuffer(void);
static void Con_DestroyMatchedWordList(void);

static void updateDedicatedConsoleCmdLine(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     CmdReturnValue = 0;

ddfont_t Cfont;                 // The console font.

byte    ConsoleSilent = false;

int     conCompMode = 0;        // Completion mode.
byte    conSilentCVars = 1;
byte    consoleDump = true;
int     consoleActiveKey = '`'; // Tilde.
byte    consoleSnapBackOnPrint = false;

char    trimmedFloatBuffer[32]; // Returned by TrimmedFloat().
char   *prbuff = NULL;          // Print buffer, used by conPrintf.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static cbuffer_t *histBuf = NULL; // The console history buffer (log).
uint bLineOff; // How many lines from the last in the histBuf?

static cbuffer_t *oldCmds = NULL; // The old commands buffer.
uint ocPos;    // How many cmds from the last in the oldCmds buffer.

static boolean ConsoleInited;   // Has Con_Init() been called?
static boolean ConsoleActive;   // Is the console active?
static timespan_t ConsoleTime;  // How many seconds has the console been open?

static char cmdLine[CMDLINE_SIZE]; // The command line.
static uint cmdCursor;          // Position of the cursor on the command line.
static boolean cmdInsMode;      // Are we in insert input mode.
static boolean conInputLock;    // While locked, most user input is disabled.

static execbuff_t *exBuff;
static int exBuffSize;
static execbuff_t *curExec;

static uint complPos;            // Where is the completion cursor?

static knownword_t **matchedWords;
static unsigned int matchedWordCount;
static unsigned int lastCompletion;  // The last completed known word match.
static boolean matchedWordListGood;

static boolean finishCompletion; // An autocomplete has taken place that must
                                 // be completed ASAP.

// CODE --------------------------------------------------------------------

static void Con_Register(void)
{
    C_CMD("add",            NULL,   AddSub);
    C_CMD("after",          "is",   Wait);
    C_CMD("alias",          NULL,   Alias);
    C_CMD("clear",          "",     Clear);
    C_CMD_FLAGS("conclose",       "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD_FLAGS("conopen",        "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD_FLAGS("contoggle",      "",     OpenClose,    CMDF_NO_DEDICATED);
    C_CMD("dec",            NULL,   IncDec);
    C_CMD("echo",           "s*",   Echo);
    C_CMD("print",          "s*",   Echo);
    C_CMD("exec",           "s*",   Parse);
    C_CMD("font",           NULL,   Font);
    C_CMD("help",           "",     Help);
    C_CMD("if",             NULL,   If);
    C_CMD("inc",            NULL,   IncDec);
    C_CMD("listmobjtypes",  "",     ListMobjs);
    C_CMD("quit!",          "",     Quit);
    C_CMD("repeat",         "ifs",  Repeat);
    C_CMD("sub",            NULL,   AddSub);
    C_CMD("toggle",         "s",    Toggle);
    C_CMD("version",        "",     Version);
    C_CMD("write",          "s",    WriteConsole);

    // Console
    C_VAR_INT("con-completion", &conCompMode, 0, 0, 1);
    C_VAR_BYTE("con-dump", &consoleDump, 0, 0, 1);
    C_VAR_INT("con-key-activate", &consoleActiveKey, 0, 0, 255);
    C_VAR_BYTE("con-var-silent", &conSilentCVars, 0, 0, 1);
    C_VAR_BYTE("con-snapback", &consoleSnapBackOnPrint, 0, 0, 1);

    // File
    C_VAR_CHARPTR("file-startup", &defaultWads, 0, 0, 0);

    C_VAR_INT("con-transition", &rTransition, 0, FIRST_TRANSITIONSTYLE, LAST_TRANSITIONSTYLE);
    C_VAR_INT("con-transition-tics", &rTransitionTics, 0, 0, 60);

    Con_DataRegister();
}

boolean Con_IsActive(void)
{
    return ConsoleActive;
}

boolean Con_IsLocked(void)
{
    return conInputLock;
}

boolean Con_InputMode(void)
{
    return cmdInsMode;
}

char *Con_GetCommandLine(void)
{
    return cmdLine;
}

cbuffer_t *Con_GetConsoleBuffer(void)
{
    return histBuf;
}

uint Con_CursorPosition(void)
{
    return cmdCursor;
}

void PrepareCmdArgs(cmdargs_t *cargs, const char *lpCmdLine)
{
#define IS_ESC_CHAR(x)  ((x) == '"' || (x) == '\\' || (x) == '{' || (x) == '}')

    size_t              i, len = strlen(lpCmdLine);

    // Prepare the data.
    memset(cargs, 0, sizeof(cmdargs_t));
    strcpy(cargs->cmdLine, lpCmdLine);

    // Prepare.
    for(i = 0; i < len; ++i)
    {
        // Whitespaces are separators.
        if(ISSPACE(cargs->cmdLine[i]))
            cargs->cmdLine[i] = 0;

        if(cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1]))
        {   // Escape sequence.
            memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
                    sizeof(cargs->cmdLine) - i - 1);
            len--;
            continue;
        }

        if(cargs->cmdLine[i] == '"')
        {   // Find the end.
            size_t              start = i;

            cargs->cmdLine[i] = 0;
            for(++i; i < len && cargs->cmdLine[i] != '"'; ++i)
            {
                if(cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1])) // Escape sequence?
                {
                    memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
                            sizeof(cargs->cmdLine) - i - 1);
                    len--;
                    continue;
                }
            }

            // Quote not terminated?
            if(i == len)
                break;

            // An empty set of quotes?
            if(i == start + 1)
                cargs->cmdLine[i] = SC_EMPTY_QUOTE;
            else
                cargs->cmdLine[i] = 0;
        }

        if(cargs->cmdLine[i] == '{')
        {   // Find matching end, braces are another notation for quotes.
            int             level = 0;
            size_t          start = i;

            cargs->cmdLine[i] = 0;
            for(++i; i < len; ++i)
            {
                if(cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1])) // Escape sequence?
                {
                    memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
                            sizeof(cargs->cmdLine) - i - 1);
                    len--;
                    i++;
                    continue;
                }

                if(cargs->cmdLine[i] == '}')
                {
                    if(!level)
                        break;

                    level--;
                }

                if(cargs->cmdLine[i] == '{')
                    level++;
            }

            // Quote not terminated?
            if(i == len)
                break;

            // An empty set of braces?
            if(i == start + 1)
                cargs->cmdLine[i] = SC_EMPTY_QUOTE;
            else
                cargs->cmdLine[i] = 0;
        }
    }

    // Scan through the cmdLine and get the beginning of each token.
    cargs->argc = 0;
    for(i = 0; i < len; ++i)
    {
        if(!cargs->cmdLine[i])
            continue;

        // Is this an empty quote?
        if(cargs->cmdLine[i] == SC_EMPTY_QUOTE)
            cargs->cmdLine[i] = 0;  // Just an empty string.

        cargs->argv[cargs->argc++] = cargs->cmdLine + i;
        i += strlen(cargs->cmdLine + i);
    }

#undef IS_ESC_CHAR
}

boolean Con_Init(void)
{
    histBuf = Con_NewBuffer(512, 70, 0);
    bLineOff = 0;

    oldCmds = Con_NewBuffer(0, CMDLINE_SIZE, CBF_ALWAYSFLUSH);
    ocPos = 0; // No commands yet.

    //B_Init();

    ConsoleActive = false;
    ConsoleInited = true;
    ConsoleTime = 0;

    Rend_ConsoleInit();

    complPos = 0;
    matchedWordListGood = false;

    cmdCursor = 0;

    exBuff = NULL;
    exBuffSize = 0;

    // Register the engine commands and variables.
    DD_RegisterLoop();
    DD_RegisterInput();
    DD_RegisterVFS();
    B_Register(); // for control bindings
    Con_Register();
    DH_Register();
    R_Register();
    S_Register();
    SBE_Register(); // for bias editor
    Rend_Register();
    GL_Register();
    Net_Register();
    I_Register();
    H_Register();
    P_MapRegister();
    UI_Register();
    Demo_Register();
    P_ControlRegister();

    Con_Message("Con_Init: Initializing the console.\n");
    return true;
}

void Con_Shutdown(void)
{
    // Announce
    VERBOSE(Con_Printf("Con_Shutdown: Shuting down the console...\n"));

    Con_DestroyBuffer(histBuf); // The console history buffer.
    Con_DestroyBuffer(oldCmds); // The old commands buffer.

    if(prbuff)
        M_Free(prbuff); // Free the print buffer.

    Con_DestroyMatchedWordList();
    Con_DestroyDatabases();

    Con_ClearExecBuffer();
}

/**
 * Changes the console font.
 * Part of the Doomsday public API.
 */
void Con_SetFont(ddfont_t* cfont)
{
    if(!cfont)
        return;

    memcpy(&Cfont, cfont, sizeof(Cfont));
}

/**
 * Send a console command to the server.
 * This shouldn't be called unless we're logged in with the right password.
 */
static void Con_Send(const char *command, byte src, int silent)
{
    ushort          len = (ushort) (strlen(command) + 1);

    Msg_Begin(PKT_COMMAND2);
    // Mark high bit for silent commands.
    Msg_WriteShort(len | (silent ? 0x8000 : 0));
    Msg_WriteShort(0); // flags. Unused at present.
    Msg_WriteByte(src);
    Msg_Write(command, len);
    // Send it reliably.
    Net_SendBuffer(0, SPF_ORDERED);
}

static void Con_QueueCmd(const char *singleCmd, timespan_t atSecond,
                         byte source, boolean isNetCmd)
{
    execbuff_t *ptr = NULL;
    int     i;

    // Look for an empty spot.
    for(i = 0; i < exBuffSize; ++i)
        if(!exBuff[i].used)
        {
            ptr = exBuff + i;
            break;
        }

    if(ptr == NULL)
    {
        // No empty places, allocate a new one.
        exBuff = M_Realloc(exBuff, sizeof(execbuff_t) * ++exBuffSize);
        ptr = exBuff + exBuffSize - 1;
    }
    ptr->used = true;
    strcpy(ptr->subCmd, singleCmd);
    ptr->when = atSecond;
    ptr->source = source;
    ptr->isNetCmd = isNetCmd;
}

static void Con_ClearExecBuffer(void)
{
    M_Free(exBuff);
    exBuff = NULL;
    exBuffSize = 0;
}

/**
 * The execbuffer is used to schedule commands for later.
 *
 * @return          @c false, if an executed command fails.
 */
static boolean Con_CheckExecBuffer(void)
{
#define BUFFSIZE 256
    boolean allDone;
    boolean ret = true;
    int     i, count = 0;
    char    storage[BUFFSIZE];

    storage[255] = 0;

    do                          // We'll keep checking until all is done.
    {
        allDone = true;

        // Execute the commands marked for this or a previous tic.
        for(i = 0; i < exBuffSize; ++i)
        {
            execbuff_t *ptr = exBuff + i;

            if(!ptr->used || ptr->when > sysTime)
                continue;
            // We'll now execute this command.
            curExec = ptr;
            ptr->used = false;
            strncpy(storage, ptr->subCmd, BUFFSIZE-1);

            if(!executeSubCmd(storage, ptr->source, ptr->isNetCmd))
                ret = false;
            allDone = false;
        }

        if(count++ > 100)
        {
            Con_Message("Console execution buffer overflow! "
                        "Everything canceled.\n");
            Con_ClearExecBuffer();
            break;
        }
    } while(!allDone);

    return ret;
#undef BUFFSIZE
}

void Con_Ticker(timespan_t time)
{
    if(!conInputLock && finishCompletion)
    {
        knownword_t *word = matchedWords[lastCompletion];

        // Add a trailing space if the word is NOT a cmd or alias.
        if(matchedWordListGood &&
           !(word->type == WT_ALIAS || word->type == WT_CCMD) &&
           cmdCursor < CMDLINE_SIZE)
        {
            strcat(cmdLine, " ");
            cmdCursor++;
        }

        matchedWordListGood = false;
        finishCompletion = false;
    }

    Con_CheckExecBuffer();
    if(tickFrame)
        Con_TransitionTicker(time);
    Rend_ConsoleTicker(time);

    if(!ConsoleActive)
        return;                 // We have nothing further to do here.

    ConsoleTime += time;        // Increase the ticker.
}

void Con_SetMaxLineLength(void)
{
    int         cw = FR_TextWidth("A");
    int         length;
    float       winWidth = theWindow->width? theWindow->width : 640;

    if(!cw)
    {
        length = 70;
    }
    else
    {
        length = winWidth / cw - 2;
        if(length > 250)
            length = 250;
    }

    Con_BufferSetMaxLineLength(histBuf, length);
}

/**
 * expCommand gets reallocated in the expansion process.
 * This could be bit more clever.
 */
static void expandWithArguments(char **expCommand, cmdargs_t *args)
{
    int             p;
    char           *text = *expCommand;
    size_t          i, off, size = strlen(text) + 1;

    for(i = 0; text[i]; ++i)
    {
        if(text[i] == '%' && (text[i + 1] >= '1' && text[i + 1] <= '9'))
        {
            char           *substr;
            int             aidx = text[i + 1] - '1' + 1;

            // Expand! (or delete)
            if(aidx > args->argc - 1)
                substr = "";
            else
                substr = args->argv[aidx];

            // First get rid of the %n.
            memmove(text + i, text + i + 2, size - i - 2);
            // Reallocate.
            off = strlen(substr);
            text = *expCommand = M_Realloc(*expCommand, size += off - 2);
            if(off)
            {
                // Make room for the insert.
                memmove(text + i + off, text + i, size - i - off);
                memcpy(text + i, substr, off);
            }
            i += off - 1;
        }
        else if(text[i] == '%' && text[i + 1] == '0')
        {
            // First get rid of the %0.
            memmove(text + i, text + i + 2, size - i - 2);
            text = *expCommand = M_Realloc(*expCommand, size -= 2);
            for(p = args->argc - 1; p > 0; p--)
            {
                off = strlen(args->argv[p]) + 1;
                text = *expCommand = M_Realloc(*expCommand, size += off);
                memmove(text + i + off, text + i, size - i - off);
                text[i] = ' ';
                memcpy(text + i + 1, args->argv[p], off - 1);
            }
        }
    }
}

/**
 * The command is executed forthwith!!
 */
static int executeSubCmd(const char *subCmd, byte src, boolean isNetCmd)
{
    cmdargs_t   args;
    ddccmd_t   *ccmd;
    cvar_t     *cvar;
    calias_t   *cal;

    PrepareCmdArgs(&args, subCmd);
    if(!args.argc)
        return true;

    /*
    if(args.argc == 1)  // Possibly a control command?
    {
        if(P_ControlExecute(args.argv[0]))
        {
            // It was a control command.  No further processing is
            // necessary.
            return true;
        }
    }
     */

    // If logged in, send command to server at this point.
    if(!isServer && netLoggedIn)
    {
        // We have logged in on the server. Send the command there.
        Con_Send(subCmd, src, ConsoleSilent);
        return true;
    }

    // Try to find a matching console command.
    ccmd = Con_GetCommand(&args);
    if(ccmd != NULL)
    {
        // Found a match. Are we allowed to execute?
        boolean canExecute = true;

        // A dedicated server, trying to execute a ccmd not available to us?
        if(isDedicated && (ccmd->flags & CMDF_NO_DEDICATED))
        {
            Con_Printf("executeSubCmd: '%s' impossible in dedicated mode.\n",
                        ccmd->name);
            return true;
        }

        // Net commands sent to servers have extra protection.
        if(isServer && isNetCmd)
        {
            // Is the command permitted for use by clients?
            if(ccmd->flags & CMDF_CLIENT)
            {
                Con_Printf("executeSubCmd: Blocked command. A client"
                           " attempted to use '%s'.\n"
                           "This command is not permitted for use by clients\n",
                           ccmd->name);
                // \todo Tell the client!
                return true;
            }

            // Are ANY commands from this (remote) src permitted for use
            // by our clients?

            // NOTE:
            // This is an interim measure to protect against abuse of the
            // most vulnerable invocation methods.
            // Once all console commands are updated with the correct usage
            // flags we can then remove these restrictions or make them
            // optional for servers.
            //
            // The next step will then be allowing select console commands
            // to be executed by non-logged in clients.
            switch(src)
            {
            case CMDS_UNKNOWN:
            case CMDS_CONFIG:
            case CMDS_PROFILE:
            case CMDS_CMDLINE:
            case CMDS_DED:
                Con_Printf("executeSubCmd: Blocked command. A client"
                           " attempted to use '%s' via %s.\n"
                           "This invocation method is not permitted "
                           "by clients\n", ccmd->name, CMDTYPESTR(src));
                // \todo Tell the client!
                return true;

            default:
                break;
            }
        }

        // Is the src permitted for this command?
        switch(src)
        {
        case CMDS_UNKNOWN:
            canExecute = false;
            break;

        case CMDS_DDAY:
            if(ccmd->flags & CMDF_DDAY)
                canExecute = false;
            break;

        case CMDS_GAME:
            if(ccmd->flags & CMDF_GAME)
                canExecute = false;
            break;

        case CMDS_CONSOLE:
            if(ccmd->flags & CMDF_CONSOLE)
                canExecute = false;
            break;

        case CMDS_BIND:
            if(ccmd->flags & CMDF_BIND)
                canExecute = false;
            break;

        case CMDS_CONFIG:
            if(ccmd->flags & CMDF_CONFIG)
                canExecute = false;
            break;

        case CMDS_PROFILE:
            if(ccmd->flags & CMDF_PROFILE)
                canExecute = false;
            break;

        case CMDS_CMDLINE:
            if(ccmd->flags & CMDF_CMDLINE)
                canExecute = false;
            break;

        case CMDS_DED:
            if(ccmd->flags & CMDF_DED)
                canExecute = false;
            break;

        default:
            return true;
        }

        if(!canExecute)
        {
            Con_Printf("Error: '%s' cannot be executed via %s.\n",
                       ccmd->name, CMDTYPESTR(src));
            return true;
        }

        if(canExecute)
        {   // Execute the command!
            int     cret = ccmd->func(src, args.argc, args.argv);

            if(cret == false)
                Con_Printf("Error: '%s' failed.\n", ccmd->name);
            // We're quite done.
            return cret;
        }
        else
        {
            return true;
        }
    }

    // Then try the cvars?
    cvar = Con_GetVariable(args.argv[0]);
    if(cvar != NULL)
    {
        boolean out_of_range = false, setting = false;

        if(args.argc == 2 ||
           (args.argc == 3 && !stricmp(args.argv[1], "force")))
        {
            char   *argptr = args.argv[args.argc - 1];
            boolean forced = args.argc == 3;

            setting = true;
            if(cvar->flags & CVF_READ_ONLY)
            {
                Con_Printf("%s is read-only. It can't be changed (not even "
                           "with force)\n", cvar->name);
            }
            else if(cvar->flags & CVF_PROTECTED && !forced)
            {
                Con_Printf
                    ("%s is protected. You shouldn't change its value.\n",
                     cvar->name);
                Con_Printf
                    ("Use the command: '%s force %s' to modify it anyway.\n",
                     cvar->name, argptr);
            }
            else if(cvar->type == CVT_BYTE)
            {
                byte    val = (byte) strtol(argptr, NULL, 0);

                if(!forced &&
                   ((!(cvar->flags & CVF_NO_MIN) && val < cvar->min) ||
                    (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)))
                    out_of_range = true;
                else
                    Con_SetInteger(cvar->name, val, false);
            }
            else if(cvar->type == CVT_INT)
            {
                int     val = strtol(argptr, NULL, 0);

                if(!forced &&
                   ((!(cvar->flags & CVF_NO_MIN) && val < cvar->min) ||
                    (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)))
                    out_of_range = true;
                else
                    Con_SetInteger(cvar->name, val, false);
            }
            else if(cvar->type == CVT_FLOAT)
            {
                float   val = strtod(argptr, NULL);

                if(!forced &&
                   ((!(cvar->flags & CVF_NO_MIN) && val < cvar->min) ||
                    (!(cvar->flags & CVF_NO_MAX) && val > cvar->max)))
                    out_of_range = true;
                else
                    Con_SetFloat(cvar->name, val, false);
            }
            else if(cvar->type == CVT_CHARPTR)
            {
                Con_SetString(cvar->name, argptr, false);
            }
        }

        if(out_of_range)
        {
            if(!(cvar->flags & (CVF_NO_MIN | CVF_NO_MAX)))
            {
                char    temp[20];

                strcpy(temp, TrimmedFloat(cvar->min));
                Con_Printf("Error: %s <= %s <= %s\n", temp, cvar->name,
                           TrimmedFloat(cvar->max));
            }
            else if(cvar->flags & CVF_NO_MAX)
            {
                Con_Printf("Error: %s >= %s\n", cvar->name,
                           TrimmedFloat(cvar->min));
            }
            else
            {
                Con_Printf("Error: %s <= %s\n", cvar->name,
                           TrimmedFloat(cvar->max));
            }
        }
        else if(!setting || !conSilentCVars)    // Show the value.
        {
            Con_PrintCVar(cvar, "");
        }
        return true;
    }

    // How about an alias then?
    cal = Con_GetAlias(args.argv[0]);
    if(cal != NULL)
    {
        char   *expCommand;

        // Expand the command with arguments.
        expCommand = M_Malloc(strlen(cal->command) + 1);
        strcpy(expCommand, cal->command);
        expandWithArguments(&expCommand, &args);
        // Do it, man!
        Con_SplitIntoSubCommands(expCommand, 0, src, isNetCmd);
        M_Free(expCommand);
        return true;
    }

    // What *is* that?
    Con_Printf("%s: unknown identifier, or command arguments invalid.\n", args.argv[0]);
    return false;
}

/**
 * Splits the command into subcommands and queues them into the
 * execution buffer.
 */
static void Con_SplitIntoSubCommands(const char *command,
                                     timespan_t markerOffset, byte src,
                                     boolean isNetCmd)
{
#define BUFFSIZE        2048

    char            subCmd[BUFFSIZE];
    int             inQuotes = false, escape = false;
    size_t          gPos = 0, scPos = 0, len;

    // Is there a command to execute?
    if(!command || command[0] == 0)
        return;

    // Jump over initial semicolons.
    len = strlen(command);
    while(gPos < len && command[gPos] == ';' && command[gPos] != 0)
        gPos++;

    subCmd[0] = subCmd[BUFFSIZE-1] = 0;

    // The command may actually contain many commands, separated
    // with semicolons. This isn't a very clear algorithm...
    for(; command[gPos];)
    {
        escape = false;
        if(inQuotes && command[gPos] == '\\')   // Escape sequence?
        {
            subCmd[scPos++] = command[gPos++];
            escape = true;
        }
        if(command[gPos] == '"' && !escape)
            inQuotes = !inQuotes;

        // Collect characters.
        subCmd[scPos++] = command[gPos++];
        if(subCmd[0] == ' ')
            scPos = 0;          // No spaces in the beginning.

        if((command[gPos] == ';' && !inQuotes) || command[gPos] == 0)
        {
            while(gPos < len && command[gPos] == ';' && command[gPos] != 0)
                gPos++;
            // The subcommand ends.
            subCmd[scPos] = 0;
        }
        else
        {
            continue;
        }

        // Queue it.
        Con_QueueCmd(subCmd, sysTime + markerOffset, src, isNetCmd);

        scPos = 0;
    }

#undef BUFFSIZE
}

static void Con_DestroyMatchedWordList(void)
{
    // Free the matched words array.
    if(matchedWordCount)
        M_Free(matchedWords);
}

/**
 * Ambiguous string check. 'amb' is cut at the first character that
 * differs when compared to 'str' (case ignored).
 */
static void stramb(char *amb, const char *str)
{
    while(*str && tolower((unsigned) *amb) == tolower((unsigned) *str))
    {
        amb++;
        str++;
    }
    *amb = 0;
}

/**
 * Look at the last word and try to complete it. If there are several
 * possibilities, print them.
 *
 * @return          Number of possible completions.
 */
static int completeWord(int mode)
{
    int             cp = (int) strlen(cmdLine) - 1;
    char            word[100], *wordBegin;
    char            unambiguous[256];
    char           *completion = 0; // Pointer to the completed word.
    cvar_t         *cvar;
    boolean         justUpdated;

    if(mode == 1)
        cp = complPos - 1;
    memset(unambiguous, 0, sizeof(unambiguous));

    if(cp < 0)
        return 0;

    // Skip over any whitespace behind the cursor.
    while(cp > 0 && cmdLine[cp] == ' ')
        cp--;

    // Rewind the word pointer until space or a semicolon is found.
    while(cp > 0 &&
          cmdLine[cp - 1] != ' ' &&
          cmdLine[cp - 1] != ';' &&
          cmdLine[cp - 1] != '"')
        cp--;

    // Now cp is at the beginning of the word that needs completing.
    strcpy(word, wordBegin = cmdLine + cp);

    if(mode == 1)
        word[complPos - cp] = 0;    // Only check a partial word.

    // Can we use the previous valid matched word list?
    // If yes we will use it, else perform a new search.
    justUpdated = false;
    if(!matchedWordListGood)
    {
        Con_DestroyMatchedWordList();

        matchedWords =
            Con_CollectKnownWordsMatchingWord(word, &matchedWordCount);

        matchedWordListGood = true;
        lastCompletion = 0;
        justUpdated = true;
    }

    if(!matchedWordCount)
        return 0;

    // At this point we have at least one completion for the word.
    // What happens next depends on the completion mode.
    if(mode == 1)
    {
        // Completion Mode 1: Cycle through the possible completions.
        uint            idx;

        // fix me: signed/unsigned mismatch
        // use another var to note when lastCompletion is not valid.
        if(justUpdated || lastCompletion + 1 >= matchedWordCount)
            idx = 0;
        else
            idx = lastCompletion + 1;

        completion = matchedWords[idx]->word;
        lastCompletion = idx;
    }
    else
    {
        // Completion Mode 2: Print the possible completions
        knownword_t **foundWord;
        boolean updated = false;
        boolean printCompletions = (matchedWordCount > 1? true : false);

        if(printCompletions)
            Con_Printf("Completions:\n");

        // Look through the known words.
        for(foundWord = matchedWords; *foundWord; foundWord++)
        {
            if(printCompletions)
            {
                switch((*foundWord)->type)
                {
                case WT_CVAR:
                    if((cvar = Con_GetVariable((*foundWord)->word)) != NULL)
                        Con_PrintCVar(cvar, "  ");
                    break;

                case WT_CCMD:
                case WT_ALIAS:
                    Con_FPrintf(CBLF_LIGHT | CBLF_YELLOW, "  %s\n",
                                (*foundWord)->word);
                    break;

                default:
                    Con_Printf("  %s\n", (*foundWord)->word);
                    break;
                }
            }

            // This matches!
            if(!unambiguous[0])
                strcpy(unambiguous, (*foundWord)->word);
            else
                stramb(unambiguous, (*foundWord)->word);

            if(!updated)
            {
                // Update completion.
                completion = (*foundWord)->word;
                lastCompletion = foundWord - matchedWords;

                updated = true;
            }
        }
    }

    // Was a single completion found?
    if(matchedWordCount == 1 || (mode == 1 && matchedWordCount > 1))
    {
        if(wordBegin - cmdLine + strlen(completion) < CMDLINE_SIZE)
        {
            strcpy(wordBegin, completion);
            cmdCursor = (uint) strlen(cmdLine);
        }
    }
    else if(matchedWordCount > 1)
    {
        // More than one completion; only complete the unambiguous part.
        if(wordBegin - cmdLine + strlen(unambiguous) < CMDLINE_SIZE)
        {
            strcpy(wordBegin, unambiguous);
            cmdCursor = (uint) strlen(cmdLine);
        }
    }

    return matchedWordCount;
}

/**
 * Wrapper for Con_Execute
 * Allows plugin dlls to execute a console command
 */
int DD_Execute(int silent, const char *command)
{
    return Con_Execute(CMDS_GAME, command, silent, false);
}

/**
 * Returns false if a command fails.
 *
 * @param   src     The source of the command (e.g. DDay internal, DED etc).
 * @param   command The command to be executed.
 * @param   silent  Non-zero indicates not to log execution of the command.
 * @param   netCmd  If @c true, command was sent over the net.
 *
 * @return          Non-zero if command was executed successfully.
 */
int Con_Execute(byte src, const char *command, int silent, boolean netCmd)
{
    int             ret;

    if(silent)
        ConsoleSilent = true;

    Con_SplitIntoSubCommands(command, 0, src, netCmd);
    ret = Con_CheckExecBuffer();

    if(silent)
        ConsoleSilent = false;

    return ret;
}

/**
 * Exported version of Con_Executef
 */
int DD_Executef(int silent, const char *command, ...)
{
    va_list         argptr;
    char            buffer[4096];

    va_start(argptr, command);
    vsprintf(buffer, command, argptr);
    va_end(argptr);
    return Con_Execute(CMDS_GAME, buffer, silent, false);
}

int Con_Executef(byte src, int silent, const char *command, ...)
{
    va_list         argptr;
    char            buffer[4096];

    va_start(argptr, command);
    dd_vsnprintf(buffer, sizeof(buffer), command, argptr);
    va_end(argptr);
    return Con_Execute(src, buffer, silent, false);
}

static void processCmd(byte src)
{
    DD_ClearKeyRepeaters();

    // Add the command line to the oldCmds buffer.
    if(strlen(cmdLine) > 0)
    {
        Con_BufferWrite(oldCmds, 0, cmdLine);
        ocPos = Con_BufferNumLines(oldCmds);
    }

    Con_Execute(src, cmdLine, false, false);
}

static void updateCmdLine(void)
{
    if(ocPos >= Con_BufferNumLines(oldCmds))
        memset(cmdLine, 0, sizeof(cmdLine));
    else
        strncpy(cmdLine, Con_BufferGetLine(oldCmds, ocPos)->text, sizeof(cmdLine));

    cmdCursor = complPos = (uint) strlen(cmdLine);
    matchedWordListGood = false;
}

static void updateDedicatedConsoleCmdLine(void)
{
    int         flags = 0;

    if(!isDedicated)
        return;

    if(cmdInsMode)
        flags |= CLF_CURSOR_LARGE;

    Sys_SetConWindowCmdLine(windowIDX, cmdLine, cmdCursor+1, flags);
}

void Con_Open(int yes)
{
    // The console cannot be closed in dedicated mode.
    if(isDedicated)
        yes = true;

    // Clear all action keys, keyup events won't go to bindings processing
    // when the console is open.
    //P_ControlReset(-1);
    Rend_ConsoleOpen(yes);
    if(yes)
    {
        ConsoleActive = true;
        ConsoleTime = 0;

        B_ActivateContext(B_ContextByName(CONSOLE_BINDING_CONTEXT_NAME), true);
    }
    else
    {
        memset(cmdLine, 0, sizeof(cmdLine));
        cmdCursor = 0;
        ConsoleActive = false;

        B_ActivateContext(B_ContextByName(CONSOLE_BINDING_CONTEXT_NAME), false);
    }
}

/**
 * @return      @c true, if the event is eaten.
 */
boolean Con_Responder(ddevent_t* ev)
{
    // The console is only interested in keyboard toggle events.
    if(!IS_KEY_TOGGLE(ev))
        return false;

    // Special console key: Shift-Escape opens the Control Panel.
    if(!conInputLock && shiftDown && IS_TOGGLE_DOWN_ID(ev, DDKEY_ESCAPE))
    {
        Con_Execute(CMDS_DDAY, "panel", true, false);
        return true;
    }

    if(!ConsoleActive)
    {
        // In this case we are only interested in the activation key.
        if(IS_TOGGLE_DOWN_ID(ev, consoleActiveKey))
        {
            Con_Open(true);
            return true;
        }
        return false;
    }

    // All keyups are eaten by the console.
    if(IS_TOGGLE_UP(ev))
    {
        if(!shiftDown && conInputLock)
            conInputLock = false; // release the lock
        return true;
    }

    // We only want keydown events.
    if(!IS_KEY_PRESS(ev))
        return false;

    // In this case the console is active and operational.
    // Check the shutdown key.
    if(!conInputLock)
    {
        if(ev->toggle.id == consoleActiveKey)
        {
            if(shiftDown) // Shift-Tilde to fullscreen and halfscreen.
                Rend_ConsoleToggleFullscreen();
            else
                Con_Open(false);
            return true;
        }
        else
        {
            switch(ev->toggle.id)
            {
            case DDKEY_ESCAPE:
                // Hitting Escape in the console closes it.
                Con_Open(false);
                return false; // Let the menu know about this.

            case DDKEY_PGUP:
                if(shiftDown)
                {
                    Rend_ConsoleMove(-3);
                    return true;
                }
                break;

            case DDKEY_PGDN:
                if(shiftDown)
                {
                    Rend_ConsoleMove(3);
                    return true;
                }
                break;

            default:
                break;
            }
        }
    }

    switch(ev->toggle.id)
    {
    case DDKEY_UPARROW:
        if(conInputLock)
            break;

        if(ocPos > 0)
            ocPos--;
        // Update the command line.
        updateCmdLine();
        updateDedicatedConsoleCmdLine();
        return true;

    case DDKEY_DOWNARROW:
    {
        uint                num;

        if(conInputLock)
            break;

        num = Con_BufferNumLines(oldCmds);
        if(ocPos < num)
            ocPos++;

        updateCmdLine();
        updateDedicatedConsoleCmdLine();
        return true;
    }
    case DDKEY_PGUP:
    {
        uint                num;

        if(conInputLock)
            break;

        num = Con_BufferNumLines(histBuf);
        if(num > 0)
        {
            bLineOff = MIN_OF(bLineOff + 3, num - 1);
        }
        return true;
    }

    case DDKEY_PGDN:
        if(conInputLock)
            break;

        bLineOff = MAX_OF((int)bLineOff - 3, 0);
        return true;

    case DDKEY_END:
        if(conInputLock)
            break;
        bLineOff = 0;
        return true;

    case DDKEY_HOME:
        if(conInputLock)
            break;
        bLineOff = Con_BufferNumLines(histBuf);
        if(bLineOff != 0)
            bLineOff--;
        return true;

    case DDKEY_RETURN:
        if(conInputLock)
            break;

        // Return to the bottom.
        bLineOff = 0;

        // Print the command line with yellow text.
        Con_FPrintf(CBLF_YELLOW, ">%s\n", cmdLine);
        // Process the command line.
        processCmd(CMDS_CONSOLE);
        // Clear it.
        memset(cmdLine, 0, sizeof(cmdLine));
        cmdCursor = 0;
        complPos = 0;
        matchedWordListGood = false;
        Rend_ConsoleCursorResetBlink();
        updateDedicatedConsoleCmdLine();
        return true;

    case DDKEY_INS:
        if(conInputLock)
            break;

        cmdInsMode = !cmdInsMode; // Toggle text insert mode.
        updateDedicatedConsoleCmdLine();
        return true;

    case DDKEY_DEL:
        if(conInputLock)
            break;

        if(cmdLine[cmdCursor] != 0)
        {
            memmove(cmdLine + cmdCursor, cmdLine + cmdCursor + 1,
                    sizeof(cmdLine) - cmdCursor + 1);
            complPos = cmdCursor;
            matchedWordListGood = false;
            Rend_ConsoleCursorResetBlink();
            updateDedicatedConsoleCmdLine();
        }
        return true;

    case DDKEY_BACKSPACE:
        if(conInputLock)
            break;

        if(cmdCursor > 0)
        {
            memmove(cmdLine + cmdCursor - 1, cmdLine + cmdCursor,
                    sizeof(cmdLine) - cmdCursor);
            cmdCursor--;
            complPos = cmdCursor;
            matchedWordListGood = false;
            Rend_ConsoleCursorResetBlink();
            updateDedicatedConsoleCmdLine();
        }
        return true;

    case DDKEY_TAB:
        {
        int completions;
        int mode;

        if(shiftDown) // one time toggle of completion mode.
        {
            mode = (conCompMode == 0)? 1 : 0;
            conInputLock = true; // prevent most user input.
        }
        else
            mode = conCompMode;

        // Attempt to complete the word and return the number of possibilites.
        completions = completeWord(mode);

        if((completions == 1 || (mode == 1 && completions >= 1)) &&
           matchedWordListGood)
        {
            // Finish the completion ASAP.
            finishCompletion = true;
        }

        Rend_ConsoleCursorResetBlink();
        updateDedicatedConsoleCmdLine();
        }
        return true;

    case DDKEY_LEFTARROW:
        if(conInputLock)
            break;

        if(cmdCursor > 0)
        {
            if(shiftDown)
                cmdCursor = 0;
            else
                cmdCursor--;
        }
        complPos = cmdCursor;
        Rend_ConsoleCursorResetBlink();
        updateDedicatedConsoleCmdLine();
        break;

    case DDKEY_RIGHTARROW:
        if(conInputLock)
            break;

        if(cmdCursor < CMDLINE_SIZE)
        {
            if(cmdLine[cmdCursor] == 0)
            {
                uint        num;
                const cbline_t   *line;

                num = Con_BufferNumLines(oldCmds);
                if(num > 0 && ocPos > 0)
                {
                    line = Con_BufferGetLine(oldCmds, ocPos - 1);
                    if(line && cmdCursor < line->len)
                    {
                        cmdLine[cmdCursor] = line->text[cmdCursor];
                        cmdCursor++;
                        matchedWordListGood = false;
                        updateDedicatedConsoleCmdLine();
                    }
                }
            }
            else
            {
                if(shiftDown)
                    cmdCursor = (uint) strlen(cmdLine);
                else
                    cmdCursor++;
            }
        }

        complPos = cmdCursor;
        Rend_ConsoleCursorResetBlink();
        updateDedicatedConsoleCmdLine();
        break;

    case DDKEY_F5:
        if(conInputLock)
            break;

        Con_Execute(CMDS_DDAY, "clear", true, false);
        break;

    default:                    // Check for a character.
    {
        byte    ch;

        if(conInputLock)
            break;

        ch = ev->toggle.id;
        ch = DD_ModKey(ch);
        if(ch < 32 || (ch > 127 && ch < DD_HIGHEST_KEYCODE))
            return true;

        if(ch == 'c' && altDown) // if alt + c clear the current cmdline
        {
            memset(cmdLine, 0, sizeof(cmdLine));
            cmdCursor = 0;
            complPos = 0;
            matchedWordListGood = false;
            Rend_ConsoleCursorResetBlink();
            updateDedicatedConsoleCmdLine();
            return true;
        }

        // If not in insert mode, push the rest of the command-line forward.
        if(!cmdInsMode)
        {
            size_t              len = strlen(cmdLine);

            if(cmdCursor < len)
            {
                if(len < CMDLINE_SIZE - 1)
                {
                    memmove(cmdLine + cmdCursor + 1, cmdLine + cmdCursor,
                            sizeof(cmdLine) - cmdCursor - 1);

                    // The last char is always zero, though.
                    cmdLine[sizeof(cmdLine) - 1] = 0;
                }
                else
                {
                    return true; // Can't place character.
                }
            }
        }

        cmdLine[cmdCursor] = ch;
        if(cmdCursor < CMDLINE_SIZE)
            cmdCursor++;
        complPos = cmdCursor;   //strlen(cmdLine);
        matchedWordListGood = false;
        Rend_ConsoleCursorResetBlink();
        updateDedicatedConsoleCmdLine();
        return true;
    }
    }
    // The console is very hungry for keys...
    return true;
}

/**
 * A ruler line will be added into the console.
 */
void Con_AddRuler(void)
{
    int         i;

    Con_BufferWrite(histBuf, CBLF_RULER, NULL);

    if(consoleDump)
    {
        // A 70 characters long line.
        for(i = 0; i < 7; ++i)
        {
            fprintf(outFile, "----------");
            if(isDedicated)
                Sys_ConPrint(windowIDX, "----------", 0);
        }
        fprintf(outFile, "\n");
        if(isDedicated)
            Sys_ConPrint(windowIDX, "\n", 0);
    }
}

void conPrintf(int flags, const char *format, va_list args)
{
    if(flags & CBLF_RULER)
    {
        Con_AddRuler();
        flags &= ~CBLF_RULER;
    }

    if(prbuff == NULL)
        prbuff = M_Malloc(PRBUFF_SIZE);
    else
        memset(prbuff, 0, PRBUFF_SIZE);

    // Format the message to prbuff.
    dd_vsnprintf(prbuff, PRBUFF_SIZE, format, args);

    if(consoleDump)
        fprintf(outFile, "%s", prbuff);

    // Servers might have to send the text to a number of clients.
    if(isServer)
    {
        if(flags & CBLF_TRANSMIT)
            Sv_SendText(NSP_BROADCAST, flags, prbuff);
        else if(netRemoteUser) // Is somebody logged in?
            Sv_SendText(netRemoteUser, flags | SV_CONSOLE_FLAGS, prbuff);
    }

    if(isDedicated)
    {
        Sys_ConPrint(windowIDX, prbuff, flags);
    }
    else
    {
        Con_BufferWrite(histBuf, flags, prbuff);

        if(consoleSnapBackOnPrint)
        {
            // Now that something new has been printed, it will be shown.
            bLineOff = 0;
        }
    }
}

/**
 * Print into the buffer.
 */
void Con_Printf(const char *format, ...)
{
    va_list args;

    if(!ConsoleInited || ConsoleSilent)
        return;

    va_start(args, format);
    conPrintf(CBLF_WHITE, format, args);
    va_end(args);
}

void Con_FPrintf(int flags, const char *format, ...)    // Flagged printf
{
    va_list args;

    if(!ConsoleInited || ConsoleSilent)
        return;

    va_start(args, format);
    conPrintf(flags, format, args);
    va_end(args);
}

/**
 * Prints a file name to the console.
 * This is a f_forall_func_t.
 */
int Con_PrintFileName(const char *fn, filetype_t type, void *dir)
{
    // Exclude the path.
    Con_Printf("  %s\n", fn + strlen(dir));

    // Continue the listing.
    return true;
}

/**
 * Print a 'global' message (to stdout and the console).
 */
void Con_Message(const char *message, ...)
{
    va_list argptr;
    char   *buffer;

    if(message[0])
    {
        buffer = M_Malloc(PRBUFF_SIZE);
        va_start(argptr, message);
        dd_vsnprintf(buffer, PRBUFF_SIZE, message, argptr);
        va_end(argptr);

#ifdef UNIX
        if(!isDedicated)
        {
            // These messages are supposed to be visible in the real console.
            fprintf(stderr, "%s", buffer);
        }
#endif

        // These messages are always dumped. If consoleDump is set,
        // Con_Printf() will dump the message for us.
        if(!consoleDump)
            printf("%s", buffer);

        // Also print in the console.
        Con_Printf("%s", buffer);

        M_Free(buffer);
    }
}

/**
 * Print an error message and quit.
 */
void Con_Error(const char *error, ...)
{
    static boolean errorInProgress = false;
    int         i, numBufLines;
    char        buff[2048], err[256];
    va_list     argptr;

    // Already in an error?
    if(!ConsoleInited || errorInProgress)
    {
        fprintf(outFile, "Con_Error: Stack overflow imminent, aborting...\n");

        va_start(argptr, error);
        dd_vsnprintf(buff, sizeof(buff), error, argptr);
        va_end(argptr);
        Sys_MessageBox(buff, true);

        // Exit immediately, lest we go into an infinite loop.
        exit(1);
    }

    // We've experienced a fatal error; program will be shut down.
    errorInProgress = true;

    // Get back to the directory we started from.
    Dir_ChDir(&ddRuntimeDir);

    va_start(argptr, error);
    dd_vsnprintf(err, sizeof(err), error, argptr);
    va_end(argptr);
    fprintf(outFile, "%s\n", err);

    strcpy(buff, "");
    if(histBuf != NULL)
    {
        // Flush anything still in the write buffer.
        Con_BufferFlush(histBuf);
        numBufLines = Con_BufferNumLines(histBuf);
        for(i = 5; i > 1; i--)
        {
            const cbline_t *cbl =
                Con_BufferGetLine(histBuf, numBufLines - i);

            if(!cbl || !cbl->text)
                continue;
            strcat(buff, cbl->text);
            strcat(buff, "\n");
        }
    }
    strcat(buff, "\n");
    strcat(buff, err);

    if(Con_IsBusy())
    {
        // In busy mode, the other thread will handle this.
        Con_BusyWorkerError(buff);
        for(;;)
        {
            // We'll stop here.
            // \todo Kill this thread?
            Sys_Sleep(10000);
        }
    }
    else
    {
        Con_AbnormalShutdown(buff);
    }
}

void Con_AbnormalShutdown(const char* message)
{
    Sys_Shutdown();
    B_Shutdown();

#ifdef WIN32
    ChangeDisplaySettings(0, 0); // Restore original mode, just in case.
#endif

    // Be a bit more graphic.
    Sys_ShowCursor(true);
    Sys_ShowCursor(true);
    if(message) // Only show if a message given.
    {
        Sys_MessageBox(message, true);
    }

    DD_Shutdown();

    // Open Doomsday.out in a text editor.
    fflush(outFile); // Make sure all the buffered stuff goes into the file.
    Sys_OpenTextEditor("doomsday.out");

    // Get outta here.
    exit(1);
}

char* TrimmedFloat(float val)
{
    char*               ptr = trimmedFloatBuffer;

    sprintf(ptr, "%f", val);
    // Get rid of the extra zeros.
    for(ptr += strlen(ptr) - 1; ptr >= trimmedFloatBuffer; ptr--)
    {
        if(*ptr == '0')
            *ptr = 0;
        else if(*ptr == '.')
        {
            *ptr = 0;
            break;
        }
        else
            break;
    }
    return trimmedFloatBuffer;
}

/**
 * Create an alias.
 */
static void Con_Alias(char *aName, char *command)
{
    calias_t *cal = Con_GetAlias(aName);
    boolean remove = false;

    // Will we remove this alias?
    if(command == NULL)
        remove = true;
    else if(command[0] == 0)
        remove = true;

    if(cal && remove) // This alias will be removed.
    {
        Con_DeleteAlias(cal);
        return; // We're done.
    }

    // Does the alias already exist?
    if(cal)
    {
        cal->command = M_Realloc(cal->command, strlen(command) + 1);
        strcpy(cal->command, command);
        return;
    }

    // We need to create a new alias.
    Con_AddAlias(aName, command);

    Con_UpdateKnownWords();
}

D_CMD(Help)
{
    Con_FPrintf(CBLF_RULER | CBLF_YELLOW | CBLF_CENTER,
                "-=- Doomsday " DOOMSDAY_VERSION_TEXT " Console -=-\n");
    Con_Printf("Keys:\n");
    Con_Printf("Tilde          Open/close the console.\n");
    Con_Printf("Shift-Tilde    Switch between half and full screen mode.\n");
    Con_Printf("F5             Clear the buffer.\n");
    Con_Printf("Alt-C          Clear the command line.\n");
    Con_Printf("Insert         Switch between replace and insert modes.\n");
    Con_Printf("Shift-Left     Move cursor to the start of the command line.\n");
    Con_Printf("Shift-Right    Move cursor to the end of the command line.\n");
    Con_Printf("Shift-PgUp/Dn  Move console window up/down.\n");
    Con_Printf("Home           Jump to the beginning of the buffer.\n");
    Con_Printf("End            Jump to the end of the buffer.\n");
    Con_Printf("PageUp/Down    Scroll up/down a couple of lines.\n");
    Con_Printf("\n");
    Con_Printf("Type \"listcmds\" to see a list of available commands.\n");
    Con_Printf("Type \"help (what)\" to see information about (what).\n");
    Con_FPrintf(CBLF_RULER, "\n");
    return true;
}

D_CMD(Clear)
{
    Con_BufferClear(histBuf);
    bLineOff = 0;
    return true;
}

D_CMD(Version)
{
    Con_Printf("Doomsday Engine %s (" __TIME__ ")\n", DOOMSDAY_VERSIONTEXT);
    Con_Printf("Game DLL: %s\n", (char *) gx.GetVariable(DD_VERSION_LONG));
    Con_Printf("%s\n", DOOMSDAY_PROJECTURL);
    return true;
}

D_CMD(Quit)
{
    // No questions asked.
    Sys_Quit();
    return true;
}

D_CMD(Alias)
{
    if(argc != 3 && argc != 2)
    {
        Con_Printf("Usage: %s (alias) (cmd)\n", argv[0]);
        Con_Printf("Example: alias bigfont \"font size 3\".\n");
        Con_Printf
            ("Use %%1-%%9 to pass the alias arguments to the command.\n");
        return true;
    }

    Con_Alias(argv[1], argc == 3 ? argv[2] : NULL);
    if(argc != 3)
        Con_Printf("Alias '%s' deleted.\n", argv[1]);

    return true;
}

D_CMD(Parse)
{
    int     i;

    for(i = 1; i < argc; ++i)
    {
        Con_Printf("Parsing %s.\n", argv[i]);
        Con_ParseCommands(argv[i], false);
    }
    return true;
}

D_CMD(Wait)
{
    timespan_t offset;

    offset = strtod(argv[1], NULL) / 35;    // Offset in seconds.
    if(offset < 0)
        offset = 0;
    Con_SplitIntoSubCommands(argv[2], offset, CMDS_CONSOLE, false);
    return true;
}

D_CMD(Repeat)
{
    int     count;
    timespan_t interval, offset;

    count = atoi(argv[1]);
    interval = strtod(argv[2], NULL) / 35;  // In seconds.
    offset = 0;
    while(count-- > 0)
    {
        offset += interval;
        Con_SplitIntoSubCommands(argv[3], offset, CMDS_CONSOLE, false);
    }
    return true;
}

D_CMD(Echo)
{
    int     i;

    for(i = 1; i < argc; ++i)
    {
        Con_Printf("%s\n", argv[i]);
#if _DEBUG
        printf("%s\n", argv[i]);
#endif
    }
    return true;
}

static boolean cvarAddSub(const char* name, float delta, boolean force)
{
    float               val;
    cvar_t*             cvar = Con_GetVariable(name);

    if(!cvar)
        return false;

    if(cvar->flags & CVF_READ_ONLY)
    {
        Con_Printf("%s (cvar) is read-only. "
                   "It can't be changed (not even with force)\n", name);
        return false;
    }

    val = Con_GetFloat(name) + delta;

    if(!force)
    {
        if(!(cvar->flags & CVF_NO_MAX) && val > cvar->max)
            val = cvar->max;
        if(!(cvar->flags & CVF_NO_MIN) && val < cvar->min)
            val = cvar->min;
    }

    Con_SetFloat(name, val, false);
    return true;
}

/**
 * Rather messy, wouldn't you say?
 */
D_CMD(AddSub)
{
    boolean             force = false;
    float               delta = 0;

    if(argc == 2)
    {
        Con_Printf("Usage: %s (cvar) (val) (force)\n", argv[0]);
        Con_Printf("Use force to make cvars go off limits.\n");
        return true;
    }
    if(argc >= 4)
    {
        force = !stricmp(argv[3], "force");
    }

    delta = strtod(argv[2], NULL);
    if(!stricmp(argv[0], "sub"))
        delta = -delta;

    return cvarAddSub(argv[1], delta, force);
}

/**
 * Rather messy, wouldn't you say?
 */
D_CMD(IncDec)
{
    boolean             force = false;
    float               val;
    cvar_t*             cvar;

    if(argc == 1)
    {
        Con_Printf("Usage: %s (cvar) (force)\n", argv[0]);
        Con_Printf("Use force to make cvars go off limits.\n");
        return true;
    }
    if(argc >= 3)
    {
        force = !stricmp(argv[2], "force");
    }
    cvar = Con_GetVariable(argv[1]);
    if(!cvar)
        return false;

    if(cvar->flags & CVF_READ_ONLY)
    {
        Con_Printf("%s (cvar) is read-only. It can't be changed (not even with force)\n", argv[1]);
        return false;
    }

    val = Con_GetFloat(argv[1]);
    val += !stricmp(argv[0], "inc")? 1 : -1;

    if(!force)
    {
        if(!(cvar->flags & CVF_NO_MAX) && val > cvar->max)
            val = cvar->max;
        if(!(cvar->flags & CVF_NO_MIN) && val < cvar->min)
            val = cvar->min;
    }

    Con_SetFloat(argv[1], val, false);
    return true;
}

/**
 * Toggle the value of a variable between zero and nonzero.
 */
D_CMD(Toggle)
{
    Con_SetInteger(argv[1], Con_GetInteger(argv[1]) ? 0 : 1, false);
    return true;
}

/**
 * Execute a command if the condition passes.
 */
D_CMD(If)
{
    struct {
        const char *opstr;
        uint    op;
    } operators[] =
    {
        {"not", IF_NOT_EQUAL},
        {"=",   IF_EQUAL},
        {">",   IF_GREATER},
        {"<",   IF_LESS},
        {">=",  IF_GEQUAL},
        {"<=",  IF_LEQUAL},
        {NULL,  0}
    };
    uint        i, oper;
    cvar_t     *var;
    boolean     isTrue = false;

    if(argc != 5 && argc != 6)
    {
        Con_Printf("Usage: %s (cvar) (operator) (value) (cmd) (else-cmd)\n",
                   argv[0]);
        Con_Printf("Operator must be one of: not, =, >, <, >=, <=.\n");
        Con_Printf("The (else-cmd) can be omitted.\n");
        return true;
    }

    var = Con_GetVariable(argv[1]);
    if(!var)
        return false;

    // Which operator?
    for(i = 0; operators[i].opstr; ++i)
        if(!stricmp(operators[i].opstr, argv[2]))
        {
            oper = operators[i].op;
            break;
        }
    if(!operators[i].opstr)
        return false;           // Bad operator.

    // Value comparison depends on the type of the variable.
    if(var->type == CVT_BYTE || var->type == CVT_INT)
    {
        int     value = (var->type == CVT_INT ? CV_INT(var) : CV_BYTE(var));
        int     test = strtol(argv[3], 0, 0);

        isTrue =
            (oper == IF_EQUAL ? value == test : oper == IF_NOT_EQUAL ? value !=
             test : oper == IF_GREATER ? value > test : oper ==
             IF_LESS ? value < test : oper == IF_GEQUAL ? value >=
             test : value <= test);
    }
    else if(var->type == CVT_FLOAT)
    {
        float   value = CV_FLOAT(var);
        float   test = strtod(argv[3], 0);

        isTrue =
            (oper == IF_EQUAL ? value == test : oper == IF_NOT_EQUAL ? value !=
             test : oper == IF_GREATER ? value > test : oper ==
             IF_LESS ? value < test : oper == IF_GEQUAL ? value >=
             test : value <= test);
    }
    else if(var->type == CVT_CHARPTR)
    {
        int     comp = stricmp(CV_CHARPTR(var), argv[3]);

        isTrue =
            (oper == IF_EQUAL ? comp == 0 : oper == IF_NOT_EQUAL ? comp !=
             0 : oper == IF_GREATER ? comp > 0 : oper == IF_LESS ? comp <
             0 : oper == IF_GEQUAL ? comp >= 0 : comp <= 0);
    }

    // Should the command be executed?
    if(isTrue)
    {
        Con_Execute(src, argv[4], ConsoleSilent, false);
    }
    else if(argc == 6)
    {
        Con_Execute(src, argv[5], ConsoleSilent, false);
    }
    CmdReturnValue = isTrue;
    return true;
}

/**
 * Console command to open/close the console prompt.
 */
D_CMD(OpenClose)
{
    if(!stricmp(argv[0], "conopen"))
    {
        Con_Open(true);
    }
    else if(!stricmp(argv[0], "conclose"))
    {
        Con_Open(false);
    }
    else
    {
        Con_Open(!ConsoleActive);
    }
    return true;
}

D_CMD(Font)
{
    if(argc == 1 || argc > 3)
    {
        Con_Printf("Usage: %s (cmd) (args)\n", argv[0]);
        Con_Printf("Commands: default, name, size, xsize, ysize.\n");
        Con_Printf
            ("Names: Fixed, Fixed12, System, System12, Large, Small7, Small8, Small10.\n");
        Con_Printf("Size 1.0 is normal.\n");
        return true;
    }
    if(!stricmp(argv[1], "default"))
    {
        FR_SetFont(glFontFixed);
        FR_DestroyFont(FR_GetCurrent());
        FR_PrepareFont(GL_ChooseFixedFont());
        glFontFixed = FR_GetCurrent();
        Cfont.flags = DDFONT_WHITE;
        Cfont.height = FR_SingleLineHeight("Con");
        Cfont.sizeX = 1;
        Cfont.sizeY = 1;
        Cfont.drawText = FR_ShadowTextOut;
        Cfont.getWidth = FR_TextWidth;
        Cfont.filterText = NULL;
    }
    else if(!stricmp(argv[1], "name") && argc == 3)
    {
        FR_SetFont(glFontFixed);
        FR_DestroyFont(FR_GetCurrent());
        if(!FR_PrepareFont(argv[2]))
            FR_PrepareFont(GL_ChooseFixedFont());
        glFontFixed = FR_GetCurrent();
        Cfont.height = FR_SingleLineHeight("Con");
    }
    else if(argc == 3)
    {
        if(!stricmp(argv[1], "xsize") || !stricmp(argv[1], "size"))
            Cfont.sizeX = strtod(argv[2], NULL);
        if(!stricmp(argv[1], "ysize") || !stricmp(argv[1], "size"))
            Cfont.sizeY = strtod(argv[2], NULL);
        // Make sure the sizes are valid.
        if(Cfont.sizeX <= 0)
            Cfont.sizeX = 1;
        if(Cfont.sizeY <= 0)
            Cfont.sizeY = 1;
    }
    return true;
}
