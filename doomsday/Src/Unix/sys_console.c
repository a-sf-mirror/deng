
//**************************************************************************
//**
//** SYS_CONSOLE.C
//**
//** Standalone console window handling.
//** Used in dedicated mode.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Sys_ConInit(void)
{
}

void Sys_ConShutdown(void)
{
}

void Sys_ConPostEvents(void)
{
/*	event_t ev; */
}

void Sys_ConSetCursor(int x, int y)
{
}

void Sys_ConScrollLine(void)
{
}

void Sys_ConSetAttrib(int flags)
{
}

/*
// Writes the text at the (cx,cy).
void Sys_ConWriteText(CHAR_INFO *line, int len)
{
}
*/

void Sys_ConPrint(int clflags, char *text)
{
}

void Sys_ConUpdateCmdLine(char *text)
{
}	
