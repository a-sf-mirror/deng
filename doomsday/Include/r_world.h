//===========================================================================
// R_WORLD.H
//===========================================================================
#ifndef __DOOMSDAY_REFRESH_WORLD_H__
#define __DOOMSDAY_REFRESH_WORLD_H__

#include "r_data.h"

extern int leveltic;				// Restarts when a new map is set up.
extern int rendSkyLight; 			// cvar

// Map Info flags.
#define MIF_FOG				0x1		// Fog is used in the level.
#define MIF_DRAW_SPHERE		0x2		// Always draw the sky sphere.

const char *R_GetCurrentLevelID(void);
const byte *R_GetSectorLightColor(sector_t *sector);
void R_SetupLevel(char *level_id, int flags);
void R_SetupFog(void);
void R_SetupSky(void);
void R_SetSectorLinks(sector_t *sec);
sector_t *R_GetLinkedSector(sector_t *startsec, boolean getfloor);
void R_UpdatePlanes(void);
void R_ClearSectorFlags(void);
void R_SkyFix(void);
void R_OrderVertices(line_t *line, sector_t *sector, vertex_t *verts[2]);

#endif 
