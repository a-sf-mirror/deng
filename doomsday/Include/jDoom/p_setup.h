/* $Id$
 * Map setup routines.
 */
#ifndef __P_SETUP__
#define __P_SETUP__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// Called by startup code.
void            P_Init(void);

boolean         P_MapExists(int episode, int map);
void            P_SetupLevel(int episode, int map, int playermask, skill_t skill);
int             P_HandleMapDataProperty(int id, int dtype, int prop, int type, void *data);
int             P_HandleMapDataPropertyValue(int id, int dtype, int prop int type, void *data);
#endif
