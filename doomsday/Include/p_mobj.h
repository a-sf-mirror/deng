#ifndef __DOOMSDAY_MOBJ_H__
#define __DOOMSDAY_MOBJ_H__

#include "p_data.h"

// We'll use the base mobj template directly as our mobj.
typedef struct mobj_s {
	DD_BASE_MOBJ_ELEMENTS()
} mobj_t;

#define DEFAULT_FRICTION	0xe800

extern int tmfloorz, tmceilingz;
extern mobj_t *blockingMobj;
extern boolean dontHitMobjs;

#include "cl_def.h" // for playerstate_s

void P_SetState(mobj_t *mo, int statenum);
void P_XYMovement(mobj_t* mo);
void P_XYMovement2(mobj_t* mo, void *pstate);
void P_ZMovement(mobj_t* mo);
boolean P_TryMove(mobj_t* thing, fixed_t x, fixed_t y, fixed_t z);
boolean P_StepMove(mobj_t *thing, fixed_t dx, fixed_t dy, fixed_t dz);
boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y);
boolean P_CheckPosition2(mobj_t* thing, fixed_t x, fixed_t y, fixed_t z);
boolean P_ChangeSector(sector_t *sector);

#endif
