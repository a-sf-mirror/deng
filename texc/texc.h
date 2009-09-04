#ifndef __DDTEXCOMPILER_H__
#define __DDTEXCOMPILER_H__

#define VERSION_STR	"1.0"
#define MAX_TOKEN	256
#define NUM_GROUPS	2
#define MAX_PATCHES	256

enum texsyntax_e
{
	STX_SIMPLE
};

typedef struct
{
	char identification[4];
	int numlumps;
	int infotableofs;
} wadinfo_t;

typedef struct
{
	int filepos;
	int size;
	char name[8];
} lumpinfo_t;

typedef struct
{
	short		originX;
	short		originY;
	short		patch;
	short		reserved1;
	short		reserved2;
} mappatch_t;

typedef struct
{
	char		name[9];
	int			flags;	
	short		width;
	short		height;
	int			reserved;	
	short		patchCount;
	mappatch_t	patches[MAX_PATCHES];
} maptexture_t;

typedef struct def_s
{
	def_s		*next, *prev;
	maptexture_t tex;
} def_t;

typedef struct patch_s
{
	patch_s		*next, *prev;
	char		name[9];
} patch_t;

// Global variables.
extern bool fullImport;

#endif