#ifndef __DOOMSDAY_MOBJ_H__
#define __DOOMSDAY_MOBJ_H__

#include "p_data.h"

// This macro can be used to calculate a thing-specific 'random' number.
#define THING_TO_ID(mo) ( (mo)->thinker.id * 26 + ((unsigned)(mo)>>8) )

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
void P_ThingMovement(mobj_t* mo);
void P_ThingMovement2(mobj_t* mo, void *pstate);
void P_ThingZMovement(mobj_t* mo);
boolean P_TryMoveXYZ(mobj_t* thing, fixed_t x, fixed_t y, fixed_t z);
boolean P_StepMove(mobj_t *thing, fixed_t dx, fixed_t dy, fixed_t dz);
boolean P_CheckPosXY(mobj_t *thing, fixed_t x, fixed_t y);
boolean P_CheckPosXYZ(mobj_t* thing, fixed_t x, fixed_t y, fixed_t z);
boolean P_SectorPlanesChanged(sector_t *sector);

#endif
