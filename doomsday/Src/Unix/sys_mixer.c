
//**************************************************************************
//**
//** SYS_MIXER.C
//**
//** Windows Multimedia Mixer (mainly used by the Win Mus driver for 
//** setting the CD volume)
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int initOk = 0;

// CODE --------------------------------------------------------------------

//===========================================================================
// Sys_InitMixer
//	A ridiculous amount of code to do something this simple.
//	But mixers are pretty abstract a subject, I guess... 
//	(No, the API just sucks.)
//===========================================================================
int Sys_InitMixer(void)
{
	if(initOk 
		|| ArgCheck("-nomixer") 
		|| ArgCheck("-nomusic") 
		|| isDedicated) return true;

	// We're successful.
	initOk = true;
	return true;
}

//===========================================================================
// Sys_ShutdownMixer
//===========================================================================
void Sys_ShutdownMixer(void)
{
	if(!initOk) return; // Can't uninitialize if not inited.
	initOk = false;
}

//===========================================================================
// Sys_Mixer4i
//===========================================================================
int Sys_Mixer4i(int device, int action, int control, int parm)
{
	if(!initOk) return MIX_ERROR;

	// There is currently no implementation for anything.
	return MIX_ERROR;
}

//===========================================================================
// Sys_Mixer3i
//===========================================================================
int Sys_Mixer3i(int device, int action, int control)
{
	return Sys_Mixer4i(device, action, control, 0);	
}

