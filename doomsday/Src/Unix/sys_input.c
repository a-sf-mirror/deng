
//**************************************************************************
//**
//** SYS_INPUT.C
//**
//** Keyboard, mouse and joystick input using SDL.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define KEYBUFSIZE	32
#define INV(x, axis)	(joyInverseAxis[axis]? -x : x)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean novideo;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int joydevice = 0;				// Joystick index to use.
boolean usejoystick = false;	// Joystick input enabled?
int joyInverseAxis[8];			// Axis inversion (default: all false).

cvar_t inputCVars[] =
{
	"i_JoyDevice",		CVF_HIDE|CVF_NO_ARCHIVE|CVF_NO_MAX|CVF_PROTECTED, CVT_INT,	&joydevice,		0,	0,	"ID of joystick to use (if more than one).",
	"i_UseJoystick",	CVF_HIDE|CVF_NO_ARCHIVE,	CVT_BYTE,	&usejoystick,	0,	1,	"1=Enable joystick input.",
//--------------------------------------------------------------------------
	"input-joy-device",	CVF_NO_MAX|CVF_PROTECTED,	CVT_INT,	&joydevice,		0,	0,	"ID of joystick to use (if more than one).",
	"input-joy",		0,							CVT_BYTE,	&usejoystick,	0,	1,	"1=Enable joystick input.",
	"input-joy-x-inverse",			0,	CVT_INT,	&joyInverseAxis[0],	0, 1,	"1=Inverse joystick X axis.",
	"input-joy-y-inverse",			0,	CVT_INT,	&joyInverseAxis[1],	0, 1,	"1=Inverse joystick Y axis.",
	"input-joy-z-inverse",			0,	CVT_INT,	&joyInverseAxis[2],	0, 1,	"1=Inverse joystick Z axis.",
	"input-joy-rx-inverse",			0,	CVT_INT,	&joyInverseAxis[3],	0, 1,	"1=Inverse joystick RX axis.",
	"input-joy-ry-inverse",			0,	CVT_INT,	&joyInverseAxis[4],	0, 1,	"1=Inverse joystick RY axis.",
	"input-joy-rz-inverse",			0,	CVT_INT,	&joyInverseAxis[5],	0, 1,	"1=Inverse joystick RZ axis.",
	"input-joy-slider1-inverse",	0,	CVT_INT,	&joyInverseAxis[6], 0, 1,	"1=Inverse joystick slider 1.",
	"input-joy-slider2-inverse",	0,	CVT_INT,	&joyInverseAxis[7], 0, 1,	"1=Inverse joystick slider 2.",
	NULL
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initOk = false;		

// CODE --------------------------------------------------------------------

//===========================================================================
// I_InitMouse
//===========================================================================
void I_InitMouse(void)
{
	if(ArgCheck("-nomouse") || novideo) return;

	// Init was successful.
}

//===========================================================================
// I_InitJoystick
//===========================================================================
void I_InitJoystick(void)
{
	if(ArgCheck("-nojoy")) return;
}

//===========================================================================
// I_Init
//	Initialize input. Returns true if successful.
//===========================================================================
int I_Init(void)
{
	if(initOk) return true;	// Already initialized.

	initOk = true;
	return true;
}

//===========================================================================
// I_Shutdown
//===========================================================================
void I_Shutdown(void)
{
	if(!initOk) return;	// Not initialized.
	initOk = false;
}

//===========================================================================
// I_MousePresent
//===========================================================================
boolean I_MousePresent(void)
{
	return false;
}

//===========================================================================
// I_JoystickPresent
//===========================================================================
boolean I_JoystickPresent(void)
{
	return false;
}

//===========================================================================
// I_GetKeyEvents
//===========================================================================
int I_GetKeyEvents(keyevent_t *evbuf, int bufsize)
{
	int i = 0;
	
	if(!initOk) return 0;
	
/*	// Get the events.
	for(i=0; i<num && i<bufsize; i++)
	{
		evbuf[i].event = (keyData[i].dwData & 0x80)? IKE_KEY_DOWN : IKE_KEY_UP;
		evbuf[i].code = (unsigned char) keyData[i].dwOfs;
	}*/
	return i;
}

//===========================================================================
// I_GetMouseState
//===========================================================================
void I_GetMouseState(mousestate_t *state)
{
	memset(state, 0, sizeof(*state));

	// Has the mouse been initialized?
	if(!I_MousePresent() || !initOk) return;

/*	// Fill in the state structure.
	state->x = mstate.lX;
	state->y = mstate.lY;
	state->z = mstate.lZ;
	
	// The buttons bitfield is ordered according to the numbering.
	for(i = 0; i < 8; i++)
	    if(mstate.rgbButtons[i] & 0x80) state->buttons |= 1 << i;
*/
}

//===========================================================================
// I_GetJoystickState
//===========================================================================
void I_GetJoystickState(joystate_t *state)
{
	memset(state, 0, sizeof(*state));

	// Initialization has not been done.
	if(!I_JoystickPresent() || !usejoystick || !initOk) return;

/*	state->axis[0] = INV(dijoy.lX, 0);
	state->axis[1] = INV(dijoy.lY, 1);
	state->axis[2] = INV(dijoy.lZ, 2);

	state->rotAxis[0] = INV(dijoy.lRx, 3);
	state->rotAxis[1] = INV(dijoy.lRy, 4);
	state->rotAxis[2] = INV(dijoy.lRz, 5);

	state->slider[0] = INV(dijoy.rglSlider[0], 6);
	state->slider[1] = INV(dijoy.rglSlider[1], 7);

	for(i = 0; i < IJOY_MAXBUTTONS; i++)
		state->buttons[i] = (dijoy.rgbButtons[i] & 0x80) != 0;
	pov = dijoy.rgdwPOV[0];
	if((pov & 0xffff) == 0xffff)
		state->povAngle = IJOY_POV_CENTER;
	else
	    state->povAngle = pov / 100.0f;*/
}
