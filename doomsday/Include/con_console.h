#ifndef __H2CONSOLE_H__
#define __H2CONSOLE_H__

#include <stdio.h>
#include "dd_share.h"

#define MAX_ARGS	256

typedef struct
{
	char cmdLine[2048];
	int argc;
	char *argv[MAX_ARGS];
} cmdargs_t;

// A console buffer line.
typedef struct
{
	int len;					// This is the length of the line (no term).
	char *text;					// This is the text.
	int flags;
} cbline_t;

// Console commands can set this when they need to return a custom value
// e.g. for the game dll.
extern int		CmdReturnValue;

extern int		consoleAlpha, consoleLight;
extern boolean	consoleDump, consoleShowFPS, consoleShadowText;

void Con_Init();
void Con_Shutdown();
void Con_WriteAliasesToFile(FILE *file);
void Con_MaxLineLength(void);
void Con_Open(int yes);
void Con_AddCommand(ccmd_t *cmd);
void Con_AddVariable(cvar_t *var);
void Con_AddCommandList(ccmd_t *cmdlist);
void Con_AddVariableList(cvar_t *varlist);
ccmd_t *Con_GetCommand(const char *name);
boolean Con_IsValidCommand(const char *name);
boolean Con_IsSpecialChar(int ch);
void Con_UpdateKnownWords(void);
void Con_Ticker(void);
boolean Con_Responder(event_t *event);
void Con_Drawer(void);
void Con_DrawRuler(int y, int lineHeight, float alpha);
void Con_Printf(char *format, ...);
void Con_FPrintf(int flags, char *format, ...); // Flagged printf.
void Con_SetFont(ddfont_t *cfont);
cbline_t *Con_GetBufferLine(int num);
int Con_Execute(char *command, int silent);
int Con_Executef(int silent, char *command, ...);

void	Con_Message(char *message, ...);
void	Con_Error(char *error, ...);
cvar_t* Con_GetVariable(char *name);
int		Con_GetInteger(char *name);
float	Con_GetFloat(char *name);
byte	Con_GetByte(char *name);
char*	Con_GetString(char *name);

void	Con_SetInteger(char *name, int value);
void	Con_SetFloat(char *name, float value);
void	Con_SetString(char *name, char *text);

char *TrimmedFloat(float val);

#endif