
//**************************************************************************
//**
//** SYS_INPUT.C
//**
//** Keyboard, mouse and joystick input using SDL.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <SDL.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define EVBUFSIZE		64

#define KEYBUFSIZE		32
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
	{ "i_JoyDevice",		CVF_HIDE|CVF_NO_ARCHIVE|CVF_NO_MAX|CVF_PROTECTED, CVT_INT,	&joydevice,		0,	0,	"ID of joystick to use (if more than one)." },
	{ "i_UseJoystick",	CVF_HIDE|CVF_NO_ARCHIVE,	CVT_BYTE,	&usejoystick,	0,	1,	"1=Enable joystick input." },
//--------------------------------------------------------------------------
	{ "input-joy-device",	CVF_NO_MAX|CVF_PROTECTED,	CVT_INT,	&joydevice,		0,	0,	"ID of joystick to use (if more than one)." },
	{ "input-joy",		0,							CVT_BYTE,	&usejoystick,	0,	1,	"1=Enable joystick input." },
	{ "input-joy-x-inverse",			0,	CVT_INT,	&joyInverseAxis[0],	0, 1,	"1=Inverse joystick X axis." },
	{ "input-joy-y-inverse",			0,	CVT_INT,	&joyInverseAxis[1],	0, 1,	"1=Inverse joystick Y axis." },
	{ "input-joy-z-inverse",			0,	CVT_INT,	&joyInverseAxis[2],	0, 1,	"1=Inverse joystick Z axis." },
	{ "input-joy-rx-inverse",			0,	CVT_INT,	&joyInverseAxis[3],	0, 1,	"1=Inverse joystick RX axis." },
	{ "input-joy-ry-inverse",			0,	CVT_INT,	&joyInverseAxis[4],	0, 1,	"1=Inverse joystick RY axis." },
	{ "input-joy-rz-inverse",			0,	CVT_INT,	&joyInverseAxis[5],	0, 1,	"1=Inverse joystick RZ axis." },
	{ "input-joy-slider1-inverse",	0,	CVT_INT,	&joyInverseAxis[6], 0, 1,	"1=Inverse joystick slider 1." },
	{ "input-joy-slider2-inverse",	0,	CVT_INT,	&joyInverseAxis[7], 0, 1,	"1=Inverse joystick slider 2." },
	{ NULL }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initOk = false;
static boolean useMouse, useJoystick;

static keyevent_t keyEvents[EVBUFSIZE];
static int evHead, evTail;

// CODE --------------------------------------------------------------------

/*
 * Returns a new key event struct from the buffer.
 */
keyevent_t *I_NewKeyEvent(void)
{
	keyevent_t *ev = keyEvents + evHead;

	evHead = (evHead + 1) % EVBUFSIZE;
	memset(ev, 0, sizeof(*ev));
	return ev;
}

/*
 * Returns the oldest event from the buffer.
 */
keyevent_t *I_GetKeyEvent(void)
{
	keyevent_t *ev;
	
	if(evHead == evTail) return NULL; // No more...
	ev = keyEvents + evTail;
	evTail = (evTail + 1) % EVBUFSIZE;
	return ev;
}

/*
 * Translate the SDL symbolic key code to a DDKEY.
 */
int I_TranslateKeyCode(SDLKey sym)
{
	switch(sym)
	{
	case 167: // Tilde
		return 96;

	case '\b': // Backspace
		return DDKEY_BACKSPACE;

	case SDLK_PAUSE:
		return DDKEY_PAUSE;

	case SDLK_UP:
		return DDKEY_UPARROW;

	case SDLK_DOWN:
		return DDKEY_DOWNARROW;

	case SDLK_LEFT:
		return DDKEY_LEFTARROW;

	case SDLK_RIGHT:
		return DDKEY_RIGHTARROW;

	case SDLK_RSHIFT:
	case SDLK_LSHIFT:
		return DDKEY_RSHIFT;

	case SDLK_RALT:
	case SDLK_LALT:
		return DDKEY_RALT;

	case SDLK_RCTRL:
	case SDLK_LCTRL:
		return DDKEY_RCTRL;
		
	default:
		break;
	}
	return sym;
}

/*
 * SDL's events are all returned from the same routine.  This function
 * is called periodically, and the events we are interested in a saved
 * into our own buffer.
 */
void I_PollEvents(void)
{
	SDL_Event event;
	keyevent_t *e;
	
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			e = I_NewKeyEvent();
			e->event = (event.type == SDL_KEYDOWN? IKE_KEY_DOWN : IKE_KEY_UP);
			e->code = I_TranslateKeyCode(event.key.keysym.sym);
			printf("sdl:%i code:%i\n", event.key.keysym.sym, e->code);
			break;

		case SDL_QUIT:
			// The system wishes to close the program immediately...
			Sys_Quit();
			break;

		default:
			// The rest of the events are ignored.
			break;
		}
	}
}

//===========================================================================
// I_InitMouse
//===========================================================================
void I_InitMouse(void)
{
	if(ArgCheck("-nomouse") || novideo) return;

	// Init was successful.
	useMouse = true;
}

//===========================================================================
// I_InitJoystick
//===========================================================================
void I_InitJoystick(void)
{
	if(ArgCheck("-nojoy")) return;
//	useJoystick = true;
}

//===========================================================================
// I_Init
//	Initialize input. Returns true if successful.
//===========================================================================
int I_Init(void)
{
	if(initOk) return true;	// Already initialized.
	initOk = true;
//	SDL_WM_GrabInput(SDL_GRAB_ON);
	return true;
}

//===========================================================================
// I_Shutdown
//===========================================================================
void I_Shutdown(void)
{
	if(!initOk) return;	// Not initialized.
	initOk = false;
//	SDL_WM_GrabInput(SDL_GRAB_OFF);	
}

//===========================================================================
// I_MousePresent
//===========================================================================
boolean I_MousePresent(void)
{
	return useMouse;
}

//===========================================================================
// I_JoystickPresent
//===========================================================================
boolean I_JoystickPresent(void)
{
	return useJoystick;
}

//===========================================================================
// I_GetKeyEvents
//===========================================================================
int I_GetKeyEvents(keyevent_t *evbuf, int bufsize)
{
	keyevent_t *e;
	int i = 0;
	
	if(!initOk) return 0;

	// Get new events from SDL.
	I_PollEvents();
	
	// Get the events.
	for(i = 0; i < bufsize; i++)
	{
		e = I_GetKeyEvent();
		if(!e) break; // No more events.
		memcpy(&evbuf[i], e, sizeof(*e));
	}
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
