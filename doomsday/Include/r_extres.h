//===========================================================================
// R_EXTRES.H
//===========================================================================
#ifndef __DOOMSDAY_REFRESH_EXT_RES_H__
#define __DOOMSDAY_REFRESH_EXT_RES_H__

/*
 * Resource classes. Each has its own subdir under Data\Game\.
 */
typedef enum resourceclass_e {
	RC_TEXTURE,		// And flats.
	RC_PATCH,		// Not sprites, mind you. Names == lumpnames.
	RC_LIGHTMAP,
	RC_MUSIC,		// Names == lumpnames.
	RC_SFX,			// Names == lumpnames.
	RC_GRAPHICS,	// Doomsday graphics.
	NUM_RESOURCE_CLASSES
} resourceclass_t;

void	R_InitExternalResources(void);
void	R_SetDataPath(const char *path);
void	R_InitDataPaths(const char *path, boolean justGamePaths);
const char *R_GetDataPath(void);
void	R_PrependDataPath(const char *origPath, char *newPath);
boolean	R_FindResource(resourceclass_t resClass, const char *name, 
					   const char *optionalSuffix, char *fileName);

#endif 
