#ifndef __DOOMSDAY_INPUT_H__
#define __DOOMSDAY_INPUT_H__

// Key event types.
#define IKE_NONE		0
#define IKE_KEY_DOWN	0x1
#define IKE_KEY_UP		0x2

// Mouse buttons.
#define IMB_LEFT		0x1
#define IMB_RIGHT		0x2
#define IMB_MIDDLE		0x4
#define IMB_FOUR		0x8
#define IMB_FIVE		0x10
#define IMB_SIX			0x20
#define IMB_SEVEN		0x40
#define IMB_EIGHT		0x80

// Joystick.
#define IJOY_AXISMIN	-10000
#define IJOY_AXISMAX	10000
#define IJOY_MAXBUTTONS	32
#define IJOY_POV_CENTER	-1

typedef struct
{
	char event;			// Type of the event.		
	unsigned char code;	// The scancode (extended, corresponds DI keys).
} keyevent_t;

typedef struct
{
	int x, y, z;		// Relative X and Y mickeys since last call.
	int buttons;		// The buttons bitfield.
} mousestate_t;

typedef struct
{
	int		axis[3]; 
	int		rotAxis[3];
	int		slider[2];
	char	buttons[IJOY_MAXBUTTONS];
	float	povAngle;	// 0 - 359 degrees.
} joystate_t;

extern boolean usejoystick;

int I_Init(void);
void I_Shutdown(void);
boolean I_MousePresent(void);
boolean I_JoystickPresent(void);
int I_GetKeyEvents(keyevent_t *evbuf, int bufsize);
void I_GetMouseState(mousestate_t *state);
void I_GetJoystickState(joystate_t *state);

#endif
