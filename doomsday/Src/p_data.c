/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/

 */

/*
 * p_data.c: Playsim Data Structures, Macros and Constants
 *
 * The Map Update API is implemented here.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_system.h"

#include "rend_bias.h"
#include "m_bams.h"
#include "m_misc.h"

#include <math.h>
#include <stddef.h>

// MACROS ------------------------------------------------------------------

#define MAPDATA_FORMATS 2

#define GLNODE_FORMATS  5

// glNode lump offsets
#define GLVERT_OFFSET   1
#define GLSEG_OFFSET    2
#define GLSSECT_OFFSET  3
#define GLNODE_OFFSET   4

// number of map data lumps for a level
#define NUM_MAPLUMPS 11

// well, there is GL_PVIS too but we arn't interested in that
#define NUM_GLLUMPS 4

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       32*FRACUNIT

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum {
    ML_LABEL,                 // A separator, name, ExMx or MAPxx
    ML_THINGS,                     // Monsters, items..
    ML_LINEDEFS,                   // LineDefs, from editing
    ML_SIDEDEFS,                   // SideDefs, from editing
    ML_VERTEXES,                   // Vertices, edited and BSP splits generated
    ML_SEGS,                       // LineSegs, from LineDefs split by BSP
    ML_SSECTORS,                   // SubSectors, list of LineSegs
    ML_NODES,                      // BSP nodes
    ML_SECTORS,                    // Sectors, from editing
    ML_REJECT,                     // LUT, sector-sector visibility
    ML_BLOCKMAP,                       // LUT, motion clipping, walls/grid element
    ML_BEHAVIOR        // ACS Scripts (not supported currently)
};

// map data type flags
#define DT_UNSIGNED 0x01
#define DT_FRACBITS 0x02
#define DT_FLAT     0x04
#define DT_TEXTURE  0x08
#define DT_NOINDEX  0x10

// Common map format properties
enum {
    DAM_VERTEX = 1,
    DAM_LINE,
    DAM_SIDE,
    DAM_SECTOR,
    DAM_SEG,
    DAM_SUBSECTOR,
    DAM_NODE,

    DAM_X,
    DAM_Y,
    DAM_DX,
    DAM_DY,

    DAM_VERTEX1,
    DAM_VERTEX2,
    DAM_FLAGS,
    DAM_SIDE0,
    DAM_SIDE1,

    DAM_TEXTURE_OFFSET_X,
    DAM_TEXTURE_OFFSET_Y,
    DAM_TOP_TEXTURE,
    DAM_MIDDLE_TEXTURE,
    DAM_BOTTOM_TEXTURE,
    DAM_FRONT_SECTOR,

    DAM_FLOOR_HEIGHT,
    DAM_CEILING_HEIGHT,
    DAM_FLOOR_TEXTURE,
    DAM_CEILING_TEXTURE,
    DAM_LIGHT_LEVEL,

    DAM_ANGLE,
    DAM_OFFSET,

    DAM_LINE_COUNT,
    DAM_LINE_FIRST,

    DAM_BBOX_RIGHT_TOP_Y,
    DAM_BBOX_RIGHT_LOW_Y,
    DAM_BBOX_RIGHT_LOW_X,
    DAM_BBOX_RIGHT_TOP_X,
    DAM_BBOX_LEFT_TOP_Y,
    DAM_BBOX_LEFT_LOW_Y,
    DAM_BBOX_LEFT_LOW_X,
    DAM_BBOX_LEFT_TOP_X,
    DAM_CHILD_RIGHT,
    DAM_CHILD_LEFT
};

// Game specific map format properties
enum {
    DAM_LINE_TAG,
    DAM_LINE_SPECIAL,
    DAM_LINE_ARG1,
    DAM_LINE_ARG2,
    DAM_LINE_ARG3,
    DAM_LINE_ARG4,
    DAM_LINE_ARG5,
    DAM_SECTOR_SPECIAL,
    DAM_SECTOR_TAG,
    DAM_THING_TID,
    DAM_THING_X,
    DAM_THING_Y,
    DAM_THING_HEIGHT,
    DAM_THING_ANGLE,
    DAM_THING_TYPE,
    DAM_THING_OPTIONS,
    DAM_THING_SPECIAL,
    DAM_THING_ARG1,
    DAM_THING_ARG2,
    DAM_THING_ARG3,
    DAM_THING_ARG4,
    DAM_THING_ARG5,
    DAM_PROPERTY_COUNT
};

// internal blockmap stuff
#define BLKSHIFT 7                // places to shift rel position for cell num
#define BLKMASK ((1<<BLKSHIFT)-1) // mask for rel position within cell
#define BLKMARGIN 0               // size guardband around map used

// TYPES -------------------------------------------------------------------

// THING formats
// These formats are FYI
/*
typedef struct {
    short           x;
    short           y;
    short           angle;
    short           type;
    short           options;
} mapthing_t;       // (DOOM format)

typedef struct {
    short           tid;
    short           x;
    short           y;
    short           height;
    short           angle;
    short           type;
    short           options;
    byte            special;
    byte            arg1;
    byte            arg2;
    byte            arg3;
    byte            arg4;
    byte            arg5;
} mapthinghex_t;    // (HEXEN format)

// LINEDEF formats
*/
// TODO: we still use these structs for byte offsets
typedef struct {
    short           v1;
    short           v2;
    short           flags;
    short           special;
    short           tag;
    short           sidenum[2];
} maplinedef_t;     // (DOOM format)

typedef struct {
    short           v1;
    short           v2;
    short           flags;
    byte            special;
    byte            arg1;
    byte            arg2;
    byte            arg3;
    byte            arg4;
    byte            arg5;
    short           sidenum[2];
} maplinedefhex_t;  // (HEXEN format)

/*
// VERTEX formats
typedef struct {
    short           x;
    short           y;
} mapvertex_t;      // (DOOM format)

typedef struct glvert2_s {
    fixed_t  x, y;
} glvert2_t;        // (gl vertex ver2)

// SIDEDEF formats
*/
// TODO: we still use this struct for byte offsets
typedef struct {
    short           textureoffset;
    short           rowoffset;
    char            toptexture[8];
    char            bottomtexture[8];
    char            midtexture[8];
    // Front sector, towards viewer.
    short           sector;
} mapsidedef_t;

/*
// SECTOR formats
typedef struct {
    short           floorheight;
    short           ceilingheight;
    char            floorpic[8];
    char            ceilingpic[8];
    short           lightlevel;
    short           special;
    short           tag;
} mapsector_t;

// SUBSECTOR formats
typedef struct {
    short           numSegs;
    short           firstseg;
} mapsubsector_t;   // (DOOM format)

typedef struct {
    unsigned int numSegs;
    unsigned int firstseg;
} glsubsect3_t;     // (gl subsector ver 3)

// SEG formats
typedef struct {
    short           v1;
    short           v2;
    short           angle;
    short           linedef;
    short           side;
    short           offset;
} mapseg_t;         // (DOOM format)

typedef struct {
    unsigned short v1, v2;
    unsigned short linedef, side;
    unsigned short partner;
} glseg_t;          // (gl seg ver 1)

typedef struct {
    unsigned int v1, v2;
    unsigned short linedef, side;
    unsigned int partner;
} glseg3_t;         // (gl seg ver 3)

// NODE formats
typedef struct {
    // Partition line from (x,y) to x+dx,y+dy)
    short           x;
    short           y;
    short           dx;
    short           dy;

    // Bounding box for each child,
    // clip against view frustum.
    short           bbox[2][4];

    // If NF_SUBSECTOR its a subsector,
    // else it's a node of another subtree.
    unsigned short  children[2];

} mapnode_t;        // (DOOM format)

typedef struct {
    short x, y;
    short dx, dy;
    short bbox[2][4];
    unsigned int children[2];
} glnode4_t;        // (gl node ver 4)
*/

// temporary format
typedef struct {
    int           v1;
    int           v2;
    int           angle;
    int           linedef;
    int           side;
    int           offset;
} mapseg_t;

//
// Types used in map data handling
//
typedef struct {
    int property; // id number of the internal struct property to map the data to
    int gameprop; // if > 0 is a specific data item (passed to the game)
    int flags;
    int size;   // num of bytes
    int offset;
} datatype_t;

typedef struct {
    char *lumpname;
    void (*func) (byte *structure, int dataType, byte *buffer, size_t elmSize, int elements, int version, int values, const datatype_t *types);
    int  mdLump;
    int  glLump;
    int  dataType;
    int  lumpclass;
    int  offset;
    boolean required;
    boolean precache;
    int  length;
} maplump_t;

// Internal data types
#define idtThings 0
#define NUM_INTERNAL_DATA_STRUCTS 8

enum {
    glVerts,
    glSegs,
    glSSects,
    glNodes,
    mlThings,
    mlLineDefs,
    mlSideDefs,
    mlVertexes,
    mlSegs,
    mlSSectors,
    mlNodes,
    mlSectors,
    mlReject,
    mlBlockMap,
    mlBehavior,
    NUM_LUMPCLASSES
};

typedef struct mldataver_s {
    char *vername;
    struct {
        int   version;
        char *magicid;
        size_t elmSize;
        int   numValues;
        datatype_t *values;
    } verInfo[NUM_MAPLUMPS];
    boolean supported;
} mldataver_t;

typedef struct {
    char *vername;
    struct {
        int   version;
        char *magicid;
        size_t elmSize;
        int   numValues;
        datatype_t *values;
    } verInfo[NUM_GLLUMPS];
    boolean supported;
} glnodever_t;

// Bad texture record
typedef struct {
    char *name;
    boolean planeTex;
    int count;
} badtex_t;

// used to list lines in each block
typedef struct linelist_t {
    long num;
    struct linelist_t *next;
} linelist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

float AccurateDistance(fixed_t dx, fixed_t dy);
int P_CheckTexture(char *name, boolean planeTex, int dataType,
                   int element, int property);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_InitMapDataFormats(void);

static boolean P_FindMapLumps(int lumps, int gllumps, char *levelID);
static boolean P_GetMapLumpFormats(int lumps, int gllumps, char *levelID);

static void P_LoadMapData(int type, int lumps, int gllumps);
static void P_CheckLevel(char *levelID, boolean silent);
static void P_GroupLines(void);
static void P_ProcessSegs(int version);

static void P_ReadBinaryMapData(byte *structure, int dataType, byte *buffer, size_t elmsize, int elements, int version, int values, const datatype_t *types);
static void P_ReadSideDefs(byte *structure, int dataType, byte *buffer, size_t elmsize, int elements, int version, int values, const datatype_t *types);
static void P_ReadLineDefs(byte *structure, int dataType, byte *buffer, size_t elmsize, int elements, int version, int values, const datatype_t *types);
static void P_ReadSideDefTextures(int lump);
static void P_FinishLineDefs(void);

static void P_CompileSideSpecialsList(void);

static void P_SetLineSideNum(int *side, unsigned short num);

static void P_FreeBadTexList(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean levelSetup;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     numvertexes;
byte   *vertexes;

int     numsegs;
byte   *segs;

int     numsectors;
byte   *sectors;

int     numsubsectors;
byte   *subsectors;

int     numnodes;
byte   *nodes;

int     numlines;
byte   *lines;

int     numsides;
byte   *sides;

// Should we generate new blockmap data if its invalid?
// 0: error out
// 1: generate new
// 2: Always generate new
int     createBMap = 1;

int     createReject = 0;

// mapthings are actually stored & handled game-side
int     numthings;

long  *blockmaplump;           // offsets in blockmap are from here
long  *blockmap;

int     bmapwidth, bmapheight;  // in mapblocks
fixed_t bmaporgx, bmaporgy;     // origin of block map
linkmobj_t *blockrings;         // for thing rings

byte   *rejectmatrix;           // for fast sight rejection

polyblock_t **polyblockmap;     // polyobj blockmap

nodepile_t thingnodes, linenodes;   // all kinds of wacky links

ded_mapinfo_t *mapinfo = 0;     // Current mapinfo.
fixed_t mapgravity;             // Gravity for the current map.

/*
 * Value types for the DMU constants.
 *
 * Each DMU constant (declared in dd_share.h) requires an
 * entry in this table.
 * The existing indices MUST NOT change and MUST match their
 * DMU constant indices. Add new DMU constants to the end.
 *
 * Not all DMU constants have a value type, in which case
 * they should be entered using the special case: "DDVT_NONE"
 */

// FIXME: This way of initializing the array is unsafe considering changes
// to the DMU constants. Maybe use a similar type of array as in p_dmu.c for
// DMU_Str, but used for initializing propertyTypes[] in P_Init.

const valuetype_t propertyTypes[] = {
    DDVT_NONE,        // DMU_NONE = 0

    DDVT_PTR,         // DMU_VERTEX = 1
    DDVT_PTR,         // DMU_SEG
    DDVT_PTR,         // DMU_LINE
    DDVT_PTR,         // DMU_SIDE
    DDVT_PTR,         // DMU_NODE
    DDVT_PTR,         // DMU_SUBSECTOR
    DDVT_PTR,         // DMU_SECTOR
    DDVT_NONE,        // DMU_BLOCKMAP
    DDVT_NONE,        // DMU_REJEC
    DDVT_NONE,        // DMU_POLYBLOCKMAP
    DDVT_PTR,         // DMU_POLYOBJ

    DDVT_NONE,        // DMU_LINE_BY_TAG
    DDVT_NONE,        // DMU_SECTOR_BY_TAG

    DDVT_NONE,        // DMU_LINE_BY_ACT_TAG
    DDVT_NONE,        // DMU_SECTOR_BY_ACT_TAG

    DDVT_NONE,        // unused1
    DDVT_NONE,        // unused2
    DDVT_NONE,        // unused2

    DDVT_FIXED,       // DMU_X
    DDVT_FIXED,       // DMU_Y
    DDVT_FIXED,       // DMU_XY

    DDVT_PTR,         // DMU_VERTEX1
    DDVT_PTR,         // DMU_VERTEX2
    DDVT_FIXED,       // DMU_VERTEX1_X
    DDVT_FIXED,       // DMU_VERTEX1_Y
    DDVT_FIXED,       // DMU_VERTEX1_XY
    DDVT_FIXED,       // DMU_VERTEX2_X
    DDVT_FIXED,       // DMU_VERTEX2_Y
    DDVT_FIXED,       // DMU_VERTEX2_XY

    DDVT_PTR,         // DMU_FRONT_SECTOR
    DDVT_PTR,         // DMU_BACK_SECTOR
    DDVT_PTR,         // DMU_SIDE0
    DDVT_PTR,         // DMU_SIDE1
    DDVT_INT,         // DMU_FLAGS
    DDVT_FIXED,       // DMU_DX
    DDVT_FIXED,       // DMU_DY
    DDVT_FIXED,       // DMU_LENGTH
    DDVT_INT,         // DMU_SLOPE_TYPE
    DDVT_ANGLE,       // DMU_ANGLE
    DDVT_FIXED,       // DMU_OFFSET
    DDVT_FLAT_INDEX,  // DMU_TOP_TEXTURE
    DDVT_BYTE,        // DMU_TOP_COLOR
    DDVT_BYTE,        // DMU_TOP_COLOR_RED
    DDVT_BYTE,        // DMU_TOP_COLOR_GREEN
    DDVT_BYTE,        // DMU_TOP_COLOR_BLUE
    DDVT_FLAT_INDEX,  // DMU_MIDDLE_TEXTURE
    DDVT_BYTE,        // DMU_MIDDLE_COLOR
    DDVT_BYTE,        // DMU_MIDDLE_COLOR_RED
    DDVT_BYTE,        // DMU_MIDDLE_COLOR_GREEN
    DDVT_BYTE,        // DMU_MIDDLE_COLOR_BLUE
    DDVT_BYTE,        // DMU_MIDDLE_COLOR_ALPHA
    DDVT_BLENDMODE,   // DMU_MIDDLE_BLENDMODE
    DDVT_FLAT_INDEX,  // DMU_BOTTOM_TEXTURE
    DDVT_BYTE,        // DMU_BOTTOM_COLOR
    DDVT_BYTE,        // DMU_BOTTOM_COLOR_RED
    DDVT_BYTE,        // DMU_BOTTOM_COLOR_GREEN
    DDVT_BYTE,        // DMU_BOTTOM_COLOR_BLUE
    DDVT_FIXED,       // DMU_TEXTURE_OFFSET_X
    DDVT_FIXED,       // DMU_TEXTURE_OFFSET_Y
    DDVT_FIXED,       // DMU_TEXTURE_OFFSET_XY
    DDVT_INT,         // DMU_VALID_COUNT

    DDVT_INT,         // DMU_LINE_COUNT
    DDVT_BYTE,        // DMU_COLOR
    DDVT_BYTE,        // DMU_COLOR_RED
    DDVT_BYTE,        // DMU_COLOR_GREEN
    DDVT_BYTE,        // DMU_COLOR_BLUE
    DDVT_SHORT,       // DMU_LIGHT_LEVEL
    DDVT_NONE,        // DMU_THINGS
    DDVT_FIXED,       // DMU_BOUNDING_BOX
    DDVT_PTR,         // DMU_SOUND_ORIGIN

    DDVT_FIXED,       // DMU_FLOOR_HEIGHT
    DDVT_FLAT_INDEX,  // DMU_FLOOR_TEXTURE
    DDVT_FLOAT,       // DMU_FLOOR_OFFSET_X
    DDVT_FLOAT,       // DMU_FLOOR_OFFSET_Y
    DDVT_FLOAT,       // DMU_FLOOR_OFFSET_XY
    DDVT_INT,         // DMU_FLOOR_TARGET
    DDVT_INT,         // DMU_FLOOR_SPEED
    DDVT_BYTE,        // DMU_FLOOR_COLOR
    DDVT_BYTE,        // DMU_FLOOR_COLOR_RED
    DDVT_BYTE,        // DMU_FLOOR_COLOR_GREEN
    DDVT_BYTE,        // DMU_FLOOR_COLOR_BLUE
    DDVT_INT,         // DMU_FLOOR_TEXTURE_MOVE_X
    DDVT_INT,         // DMU_FLOOR_TEXTURE_MOVE_Y
    DDVT_INT,         // DMU_FLOOR_TEXTURE_MOVE_XY

    DDVT_FIXED,       // DMU_CEILING_HEIGHT
    DDVT_FLAT_INDEX,  // DMU_CEILING_TEXTURE
    DDVT_FLOAT,       // DMU_CEILING_OFFSET_X
    DDVT_FLOAT,       // DMU_CEILING_OFFSET_Y
    DDVT_FLOAT,       // DMU_CEILING_OFFSET_XY
    DDVT_INT,         // DMU_CEILING_TARGET
    DDVT_INT,         // DMU_CEILING_SPEED
    DDVT_BYTE,        // DMU_CEILING_COLOR
    DDVT_BYTE,        // DMU_CEILING_COLOR_RED
    DDVT_BYTE,        // DMU_CEILING_COLOR_GREEN
    DDVT_BYTE,        // DMU_CEILING_COLOR_BLUE
    DDVT_INT,         // DMU_CEILING_TEXTURE_MOVE_X
    DDVT_INT,         // DMU_CEILING_TEXTURE_MOVE_Y
    DDVT_INT,         // DMU_CEILING_TEXTURE_MOVE_XY

    DDVT_NONE,        // DMU_SEG_LIST
    DDVT_INT,         // DMU_SEG_COUNT
    DDVT_INT,         // DMU_TAG
    DDVT_PTR,         // DMU_ORIGINAL_POINTS
    DDVT_PTR,         // DMU_PREVIOUS_POINTS
    DDVT_PTR,         // DMU_START_SPOT
    DDVT_FIXED,       // DMU_START_SPOT_X
    DDVT_FIXED,       // DMU_START_SPOT_Y
    DDVT_FIXED,       // DMU_START_SPOT_XY
    DDVT_FIXED,       // DMU_DESTINATION_X
    DDVT_FIXED,       // DMU_DESTINATION_Y
    DDVT_FIXED,       // DMU_DESTINATION_XY
    DDVT_ANGLE,       // DMU_DESTINATION_ANGLE
    DDVT_INT,         // DMU_SPEED
    DDVT_ANGLE,       // DMU_ANGLE_SPEED
    DDVT_INT,         // DMU_SEQUENCE_TYPE
    DDVT_BOOL,        // DMU_CRUSH
    DDVT_PTR          // DMU_SPECIAL_DATA
};

// --------------------------------------
int     mapFormat;

// gl nodes
int     glNodeFormat;
int     firstGLvertex = 0;

// types of MAP data structure
// These arrays are temporary. Data will be provided via DED definitions.
maplump_t LumpInfo[] = {
//   lumpname    readfunc        MD  GL  internaltype   lumpclass     offset     required?  precache?
    {"THINGS",   P_ReadBinaryMapData, 0, -1,  idtThings,     mlThings,     ML_THINGS,  true,     false},
    {"LINEDEFS", P_ReadLineDefs,      1, -1,  DAM_LINE,      mlLineDefs,   ML_LINEDEFS,true,     false},
    {"SIDEDEFS", P_ReadSideDefs,      2, -1,  DAM_SIDE,      mlSideDefs,   ML_SIDEDEFS,true,     false},
    {"VERTEXES", P_ReadBinaryMapData, 3, -1,  DAM_VERTEX,    mlVertexes,   ML_VERTEXES,true,     false},
    {"SSECTORS", P_ReadBinaryMapData, 5, -1,  DAM_SUBSECTOR, mlSSectors,   ML_SSECTORS,true,     false},
    {"SEGS",     P_ReadBinaryMapData, 4, -1,  DAM_SEG,       mlSegs,       ML_SEGS,    true,     false},
    {"NODES",    P_ReadBinaryMapData, 6, -1,  DAM_NODE,      mlNodes,      ML_NODES,   true,     false},
    {"SECTORS",  P_ReadBinaryMapData, 7, -1,  DAM_SECTOR,    mlSectors,    ML_SECTORS, true,     false},
    {"REJECT",   NULL,                8, -1,  -1,            mlReject,     ML_REJECT,  false,    false},
    {"BLOCKMAP", NULL,                9, -1,  -1,            mlBlockMap,   ML_BLOCKMAP,true,     false},
    {"BEHAVIOR", NULL,                10,-1,  -1,            mlBehavior,   ML_BEHAVIOR,false,    false},
    {"GL_VERT",  P_ReadBinaryMapData, -1, 0,  DAM_VERTEX,    glVerts,      1,          false,    false},
    {"GL_SEGS",  P_ReadBinaryMapData, -1, 1,  DAM_SEG,       glSegs,       2,          false,    false},
    {"GL_SSECT", P_ReadBinaryMapData, -1, 2,  DAM_SUBSECTOR, glSSects,     3,          false,    false},
    {"GL_NODES", P_ReadBinaryMapData, -1, 3,  DAM_NODE,      glNodes,      4,          false,    false},
    {NULL}
};

// Versions of map data structures
mldataver_t mlDataVer[] = {
    {"DOOM", {{1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {0, NULL}, {1, NULL},
              {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {-1, NULL}}, true},
    {"HEXEN",{{2, NULL}, {2, NULL}, {1, NULL}, {1, NULL}, {0, NULL}, {1, NULL},
              {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}}, true},
    {NULL}
};

// Versions of GL node data structures
glnodever_t glNodeVer[] = {
// Ver String   Verts       Segs        SSects       Nodes     Supported?
    {"V1", {{1, NULL},   {1, NULL},   {1, NULL},   {1, NULL}}, true},
    {"V2", {{2, "gNd2"}, {1, NULL},   {1, NULL},   {1, NULL}}, true},
    {"V3", {{2, "gNd2"}, {3, "gNd3"}, {3, "gNd3"}, {1, NULL}}, false},
    {"V4", {{2, "gNd4"}, {4, NULL},   {4, NULL},   {4, NULL}}, false},
    {"V5", {{2, "gNd5"}, {5, NULL},   {3, NULL},   {4, NULL}}, true},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// ptrs to the mapLumps lumps
static byte   *mapLumps[NUM_LUMPCLASSES];

static mapseg_t *segstemp;

// for sector->linecount
static int numUniqueLines;

// For BOOM style texture name overloading (TEMP)
static int *sidespecials;

// The following is used in error fixing/detection/reporting:
// missing sidedefs
static int numMissingFronts = 0;
static int *missingFronts = NULL;

// bad texture list
static int numBadTexNames = 0;
static int maxBadTexNames = 0;
static badtex_t *badTexNames = NULL;

// CODE --------------------------------------------------------------------

void P_Init(void)
{
    P_InitMapDataFormats();
    P_InitMapUpdate();
}

//===========================================================================
// P_ValidateLevel
//  Make sure all texture references in the level data are good.
//===========================================================================
void P_ValidateLevel(void)
{
    int     i;

    for(i = 0; i < numsides; i++)
    {
        side_t *side = SIDE_PTR(i);

        if(side->toptexture > numtextures - 1)
            side->toptexture = 0;
        if(side->midtexture > numtextures - 1)
            side->midtexture = 0;
        if(side->bottomtexture > numtextures - 1)
            side->bottomtexture = 0;
    }
}

/*
 * Generate valid blockmap data from the already loaded level data.
 * Adapted from algorithm used in prBoom 2.2.6 -DJS
 *
 * Algorithm is order of nlines*(ncols+nrows) not nlines*ncols*nrows
 */

/*
 * Subroutine to add a line number to a block list
 * It simply returns if the line is already in the block
 */
static void AddBlockLine(linelist_t **lists, int *count, int *done,
                         int blockno, long lineno)
{
    linelist_t *l;

    if(done[blockno])
        return;

    l = malloc(sizeof(linelist_t));
    l->num = lineno;
    l->next = lists[blockno];

    lists[blockno] = l;

    count[blockno]++;

    done[blockno] = 1;
}

/*
 * Actually construct the blockmap lump from the level data
 *
 * This finds the intersection of each linedef with the column and
 * row lines at the left and bottom of each blockmap cell. It then
 * adds the line to all block lists touching the intersection.
 */
void P_CreateBlockMap(void)
{
    int i, j;
    int bMapWidth, bMapHeight;      // blockmap dimensions
    static vec2_t bMapOrigin;       // blockmap origin (lower left)
    static vec2_t blockSize;        // size of the blocks
    int *blockcount = NULL;          // array of counters of line lists
    int *blockdone = NULL;           // array keeping track of blocks/line
    int numBlocks;                     // number of cells = nrows*ncols

    linelist_t **blocklists = NULL;  // array of pointers to lists of lines
    long linetotal = 0;              // total length of all blocklists

    vec2_t  bounds[2], point, dims;
    vertex_t *vtx;

    // scan for map limits, which the blockmap must enclose
    for(i = 0; i < numvertexes; i++)
    {
        vtx = VERTEX_PTR(i);
        V2_Set(point, FIX2FLT(vtx->x), FIX2FLT(vtx->y));
        if(!i)
            V2_InitBox(bounds, point);
        else
            V2_AddToBox(bounds, point);
    }

    // Setup the blockmap area to enclose the whole map,
    // plus a margin (margin is needed for a map that fits
    // entirely inside one blockmap cell).
    V2_Set(bounds[0], bounds[0][VX] - BLKMARGIN, bounds[0][VY] - BLKMARGIN);
    V2_Set(bounds[1], bounds[1][VX] + BLKMARGIN + 1, bounds[1][VY] + BLKMARGIN + 1);

    // Select a good size for the blocks.
    V2_Set(blockSize, 128, 128);
    V2_Copy(bMapOrigin, bounds[0]);   // min point
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    bMapWidth = ceil(dims[VX] / blockSize[VX]) + 1;
    bMapHeight = ceil(dims[VY] / blockSize[VY]) + 1;
    numBlocks = bMapWidth * bMapHeight;

    // Create the array of pointers on NBlocks to blocklists,
    // create an array of linelist counts on NBlocks, then finally,
    // make an array in which we can mark blocks done per line
    blocklists = calloc(numBlocks, sizeof(linelist_t *));
    blockcount = calloc(numBlocks, sizeof(int));
    blockdone = malloc(numBlocks * sizeof(int));

    // Initialize each blocklist, and enter the trailing -1 in all blocklists.
    // NOTE: the linked list of lines grows backwards.
    for(i = 0; i < numBlocks; i++)
    {
        blocklists[i] = malloc(sizeof(linelist_t));
        blocklists[i]->num = -1;
        blocklists[i]->next = NULL;
        blockcount[i]++;
    }

    // For each linedef in the wad, determine all blockmap blocks it touches
    // and add the linedef number to the blocklists for those blocks.
    {
    int xorg = FLT2FIX(bMapOrigin[VX]) >> FRACBITS;
    int yorg = FLT2FIX(bMapOrigin[VY]) >> FRACBITS;

    for(i = 0; i < numlines; i++)
    {
        line_t *line = LINE_PTR(i);
        int x1 = line->v1->x >> FRACBITS;
        int y1 = line->v1->y >> FRACBITS;
        int x2 = line->v2->x >> FRACBITS;
        int y2 = line->v2->y >> FRACBITS;
        int dx = x2 - x1;
        int dy = y2 - y1;
        int vert = !dx;
        int horiz = !dy;
        int spos = (dx ^ dy) > 0;
        int sneg = (dx ^ dy) < 0;
        int bx, by;
        // extremal lines[i] coords
        int minx = x1 > x2? x2 : x1;
        int maxx = x1 > x2? x1 : x2;
        int miny = y1 > y2? y2 : y1;
        int maxy = y1 > y2? y1 : y2;

        // no blocks done for this linedef yet
        memset(blockdone, 0, numBlocks * sizeof(int));

        // The line always belongs to the blocks containing its endpoints
        bx = (x1 - xorg) >> BLKSHIFT;
        by = (y1 - yorg) >> BLKSHIFT;
        AddBlockLine(blocklists, blockcount, blockdone, by * bMapWidth + bx, i);

        bx = (x2 - xorg) >> BLKSHIFT;
        by = (y2 - yorg) >> BLKSHIFT;
        AddBlockLine(blocklists, blockcount, blockdone, by * bMapWidth + bx, i);

        // For each column, see where the line along its left edge, which
        // it contains, intersects the Linedef i. Add i to each corresponding
        // blocklist.
        // We don't want to interesect vertical lines with columns.
        if(!vert)
        {
            for(j = 0; j < bMapWidth; j++)
            {
                // intersection of Linedef with x=xorg+(j<<BLKSHIFT)
                // (y-y1)*dx = dy*(x-x1)
                // y = dy*(x-x1)+y1*dx;
                int x = xorg + (j << BLKSHIFT);       // (x,y) is intersection
                int y = (dy * (x - x1)) / dx + y1;
                int yb = (y - yorg) >> BLKSHIFT;      // block row number
                int yp = (y - yorg) & BLKMASK;        // y position within block

                // Already outside the blockmap?
                if(yb < 0 || yb > (bMapHeight - 1))
                    continue;

                // Does the line touch this column at all?
                if(x < minx || x > maxx)
                    continue;

                // The cell that contains the intersection point is always added
                AddBlockLine(blocklists, blockcount, blockdone, bMapWidth * yb + j, i);

                // If the intersection is at a corner it depends on the slope
                // (and whether the line extends past the intersection) which
                // blocks are hit.

                // Where does the intersection occur?
                if(yp == 0)
                {
                    // Intersection occured at a corner
                    if(sneg) //   \ - blocks x,y-, x-,y
                    {
                        if(yb > 0 && miny < y)
                            AddBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * (yb - 1) + j, i);

                        if(j > 0 && minx < x)
                            AddBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * yb + j - 1, i);
                    }
                    else if(spos) //   / - block x-,y-
                    {
                        if(yb > 0 && j > 0 && minx < x)
                            AddBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * (yb - 1) + j - 1, i);
                    }
                    else if(horiz) //   - - block x-,y
                    {
                        if(j > 0 && minx < x)
                            AddBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * yb + j - 1, i);
                    }
                }
                else if(j > 0 && minx < x)
                {
                    // Else not at corner: x-,y
                    AddBlockLine(blocklists, blockcount,
                                 blockdone, bMapWidth * yb + j - 1, i);
                }
            }
        }

        // For each row, see where the line along its bottom edge, which
        // it contains, intersects the Linedef i. Add i to all the corresponding
        // blocklists.
        if(!horiz)
        {
            for(j = 0; j < bMapHeight; j++)
            {
                // intersection of Linedef with y=yorg+(j<<BLKSHIFT)
                // (x,y) on Linedef i satisfies: (y-y1)*dx = dy*(x-x1)
                // x = dx*(y-y1)/dy+x1;
                int y = yorg + (j << BLKSHIFT);       // (x,y) is intersection
                int x = (dx * (y - y1)) / dy + x1;
                int xb = (x - xorg) >> BLKSHIFT;      // block column number
                int xp = (x - xorg) & BLKMASK;        // x position within block

                // Outside the blockmap?
                if(xb < 0 || xb > bMapWidth - 1)
                    continue;

                // Touches this row?
                if(y < miny || y > maxy)
                    continue;

                // The cell that contains the intersection point is always added
                AddBlockLine(blocklists, blockcount, blockdone, bMapWidth * j + xb, i);

                // If the intersection is at a corner it depends on the slope
                // (and whether the line extends past the intersection) which
                // blocks are hit

                // Where does the intersection occur?
                if(xp == 0)
                {
                    // Intersection occured at a corner
                    if(sneg) //   \ - blocks x,y-, x-,y
                    {
                        if(j > 0 && miny < y)
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * (j - 1) + xb, i);
                        if(xb > 0 && minx < x)
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * j + xb - 1, i);
                    }
                    else if(vert) //   | - block x,y-
                    {
                        if(j > 0 && miny < y)
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * (j - 1) + xb, i);
                    }
                    else if(spos) //   / - block x-,y-
                    {
                        if(xb > 0 && j > 0 && miny < y)
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * (j - 1) + xb -1, i);
                    }
                }
                else if(j > 0 && miny < y)
                {
                    // Else not on a corner: x, y-
                    AddBlockLine(blocklists, blockcount, blockdone,
                                 bMapWidth * (j - 1) + xb, i);
                }
            }
        }
    }
    }

    // Add initial 0 to all blocklists
    // count the total number of lines (and 0's and -1's)
    memset(blockdone, 0, numBlocks * sizeof(int));
    for(i = 0, linetotal = 0; i < numBlocks; i++)
    {
        AddBlockLine(blocklists, blockcount, blockdone, i, 0);
        linetotal += blockcount[i];
    }

    // Create the blockmap lump
    blockmaplump = Z_Malloc(sizeof(*blockmaplump) * (4 + numBlocks + linetotal),
                            PU_LEVEL, 0);
    // blockmap header
    blockmaplump[0] = bmaporgx = FLT2FIX(bMapOrigin[VX]);
    blockmaplump[1] = bmaporgy = FLT2FIX(bMapOrigin[VY]);
    blockmaplump[2] = bmapwidth  = bMapWidth;
    blockmaplump[3] = bmapheight = bMapHeight;

    // offsets to lists and block lists
    for(i = 0; i < numBlocks; i++)
    {
        linelist_t *bl = blocklists[i];
        long offs = blockmaplump[4 + i] =   // set offset to block's list
        (i? blockmaplump[4 + i - 1] : 4 + numBlocks) + (i? blockcount[i - 1] : 0);

        // add the lines in each block's list to the blockmaplump
        // delete each list node as we go
        while (bl)
        {
            linelist_t *tmp = bl->next;

            blockmaplump[offs++] = bl->num;
            free(bl);
            bl = tmp;
        }
    }

    blockmap = blockmaplump + 4;

    // free all temporary storage
    free(blocklists);
    free(blockcount);
    free(blockdone);
}

/*
 * Attempts to load the BLOCKMAP data resource.
 *
 * If the level is too large (would overflow the size limit of
 * the BLOCKMAP lump in a WAD therefore it will have been truncated),
 * it's zero length or we are forcing a rebuild - we'll have to
 * generate the blockmap data ourselves.
 */
void P_LoadBlockMap(int lump)
{
    long i, count;
    boolean generateBMap = (createBMap == 2)? true : false;

    count = W_LumpLength(lump) / 2;

    // Is there valid BLOCKMAP data?
    if(count >= 0x10000)
    {
        // No
        Con_Message("P_LoadBlockMap: Map exceeds limits of +/- 32767 map units.\n");

        // Are we allowed to generate new blockmap data?
        if(createBMap == 0)
            Con_Error("P_LoadBlockMap: Map has invalid BLOCKMAP resource.\n"
                      "You can circumvent this error by allowing Doomsday to\n"
                      "generate this resource when needed by setting the CVAR:"
                      "blockmap-build 1");
        else
            generateBMap = true;
    }

    // Are we generating new blockmap data?
    if(generateBMap)
    {
        uint startTime;

        Con_Message("P_LoadBlockMap: Generating blockmap...\n");

        startTime = Sys_GetRealTime();

        P_CreateBlockMap();

        Con_Message("P_LoadBlockMap: Done in %.4f seconds.\n",
                    (Sys_GetRealTime() - startTime) / 1000.0f);
    }
    else
    {
        // No, the existing data is valid - so load it in.
        // Data in PWAD is little endian.
        short *wadBlockMapLump;

        Con_Message("P_LoadBlockMap: Loading BLOCKMAP...\n");

        wadBlockMapLump = W_CacheLumpNum(lump, PU_STATIC);
        blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);

        // Expand WAD blockmap into larger internal one, by treating all
        // offsets except -1 as unsigned and zero-extending them. This
        // potentially doubles the size of blockmaps allowed because DOOM
        // originally considered the offsets as always signed.
        blockmaplump[0] = SHORT(wadBlockMapLump[0]);
        blockmaplump[1] = SHORT(wadBlockMapLump[1]);
        blockmaplump[2] = (long)(SHORT(wadBlockMapLump[2])) & 0xffff;
        blockmaplump[3] = (long)(SHORT(wadBlockMapLump[3])) & 0xffff;

        for(i = 4; i < count; i++)
        {
            short t = SHORT(wadBlockMapLump[i]);
            blockmaplump[i] = t == -1? -1 : (long) t & 0xffff;
        }

        bmaporgx = blockmaplump[0] << FRACBITS;
        bmaporgy = blockmaplump[1] << FRACBITS;
        bmapwidth = blockmaplump[2];
        bmapheight = blockmaplump[3];

        Z_Free(wadBlockMapLump);
        blockmap = blockmaplump + 4;
    }

    // Clear out mobj rings.
    count = sizeof(*blockrings) * bmapwidth * bmapheight;
    blockrings = Z_Malloc(count, PU_LEVEL, 0);
    memset(blockrings, 0, count);

    for(i = 0; i < bmapwidth * bmapheight; i++)
        blockrings[i].next = blockrings[i].prev = (mobj_t *) &blockrings[i];
}

/*
 * Attempt to load the REJECT.
 *
 * If a lump is not found, we'll generate an empty REJECT LUT.
 * TODO: We could generate a proper table if a suitable one is
 *       not made available to us once we've loaded the map.
 *
 * The REJECT resource is a LUT that provides the results of
 * trivial line-of-sight tests between sectors. This is done
 * with a matrix of sector pairs ie if a monster in sector 4
 * can see the player in sector 2 - the inverse should be true.
 *
 * NOTE: Some PWADS have carefully constructed REJECT data to
 *       create special effects. For example it is possible to
 *       make a player completely invissible in certain sectors.
 *
 * The format of the table is a simple matrix of boolean values,
 * a (true) value indicates that it is impossible for mobjs in
 * sector A to see mobjs in sector B (and vice-versa). A (false)
 * value indicates that a line-of-sight MIGHT be possible an a
 * more accurate (thus more expensive) calculation will have to
 * be made.
 *
 * The table itself is constructed as follows:
 *
 *  X = sector num player is in
 *  Y = sector num monster is in
 *
 *         X
 *
 *       0 1 2 3 4 ->
 *     0 1 - 1 - -
 *  Y  1 - - 1 - -
 *     2 1 1 - - 1
 *     3 - - - 1 -
 *    \|/
 *
 * These results are read left-to-right, top-to-bottom and are
 * packed into bytes (each byte represents eight results).
 * As are all lumps in WAD the data is in little-endian order.
 *
 * Thus the size of a valid REJECT lump can be calculated as:
 *
 *   ceiling(numsectors^2)
 */
void P_LoadReject(int lump)
{
    int rejectLength = W_LumpLength(lump);
    int requiredLength = (((numsectors*numsectors) + 7) & ~7) /8;

    // If no reject matrix is found, issue a warning.
    if(rejectLength < requiredLength)
    {
        Con_Message("P_LoadReject: Valid REJECT data could not be found.\n");

        if(createReject)
        {
            // Simply generate an empty REJECT LUT for now.
            rejectmatrix = Z_Malloc(requiredLength, PU_LEVEL, 0);
            memset(rejectmatrix, 0, requiredLength);
            // TODO: Generate valid REJECT for the map.
        }
        else
            rejectmatrix = NULL;
    }
    else
    {
        // Load the REJECT
        rejectmatrix = W_CacheLumpNum(lump, PU_LEVEL);
    }
}

/*
 * TODO: Consolidate with R_UpdatePlanes?
 */
void P_PlaneChanged(sector_t *sector, boolean theCeiling)
{
    int i;
    int k;
    subsector_t *sub;
    seg_t *seg;

    // FIXME: Find a better way to find the subsectors of a sector.

    for(i = 0; i < numsubsectors; ++i)
    {
        sub = SUBSECTOR_PTR(i);

        // Only the subsectors of the changed sector.
        if(sub->sector != sector)
            continue;

        for(k = 0; k < sub->linecount; ++k)
        {
            seg = SEG_PTR(k + sub->firstline);

            // Inform the shadow bias of changed geometry.
            SB_SegHasMoved(seg);
        }

        // Inform the shadow bias of changed geometry.
        SB_PlaneHasMoved(sub, theCeiling);
    }
}

/*
 * When a change is made, this must be called to inform the engine of
 * it.  Repercussions include notifications to the renderer, network...
 */
void P_FloorChanged(sector_t *sector)
{
    P_PlaneChanged(sector, false);
}

void P_CeilingChanged(sector_t *sector)
{
    P_PlaneChanged(sector, true);
}

void P_PolyobjChanged(polyobj_t *po)
{
    seg_t **seg = po->segs;
    int i;

    for(i = 0; i < po->numsegs; ++i, ++seg)
    {
        // Shadow bias must be told.
        SB_SegHasMoved(*seg);
    }
}

//#ifdef _DEBUG
void P_PrintDebugMapData(void)
{
    int i;
    vertex_t *vtx;
    sector_t *sec;
    subsector_t *ss;
    line_t  *li;
    seg_t   *seg;
    side_t  *si;
    node_t  *no;

    Con_Printf("VERTEXES:\n");
    for(i = 0; i < numvertexes; i++)
    {
        vtx = VERTEX_PTR(i);
        Con_Printf("x=%i y=%i\n", vtx->x >> FRACBITS, vtx->y >> FRACBITS);
    }

    Con_Printf("SEGS:\n");
    for(i = 0; i < numsegs; i++)
    {
        seg = SEG_PTR(i);
        Con_Printf("v1=%i v2=%i angle=%i line=%i side=%i offset=%i\n",
                    GET_VERTEX_IDX(seg->v1), GET_VERTEX_IDX(seg->v2),
                    seg->angle >> FRACBITS,
                    (seg->linedef != NULL)? GET_LINE_IDX(seg->linedef) : -1,
                    GET_SIDE_IDX(seg->sidedef), seg->offset >> FRACBITS);
    }

    Con_Printf("SECTORS:\n");
    for(i = 0; i < numsectors; i++)
    {
        sec = SECTOR_PTR(i);
        Con_Printf("floor=%i ceiling=%i floorpic(%i)=\"%s\" ",
                     sec->floorheight >> FRACBITS, sec->ceilingheight >> FRACBITS,
                     sec->floorpic, W_CacheLumpNum(sec->floorpic, PU_GETNAME));
        Con_Printf("ceilingpic(%i)=\"%s\" light=%i\n",
                     sec->ceilingpic, W_CacheLumpNum(sec->ceilingpic, PU_GETNAME),
                     sec->lightlevel);
    }

    Con_Printf("SSECTORS:\n");
    for(i = 0; i < numsubsectors; i++)
    {
        ss = SUBSECTOR_PTR(i);
        Con_Printf("numlines=%i firstline=%i\n", ss->linecount, ss->firstline);
    }

    Con_Printf("NODES:\n");
    for(i = 0; i < numnodes; i++)
    {
        no = NODE_PTR(i);
        Con_Printf("x=%i y=%i dx=%i dy=%i bb[0][0]=%i bb[0][1]=%i bb[0][2]=%i bb[0][3]=%i " \
                   "bb[1][0]=%i bb[1][1]=%i bb[1][2]=%i bb[1][3]=%i child[0]=%i child[1]=%i\n",
                    no->x >> FRACBITS, no->y >> FRACBITS, no->dx >> FRACBITS, no->dy >>FRACBITS,
                    no->bbox[0][0] >> FRACBITS, no->bbox[0][1] >> FRACBITS,
                    no->bbox[0][2] >> FRACBITS, no->bbox[0][3] >> FRACBITS,
                    no->bbox[1][0] >> FRACBITS, no->bbox[1][1] >> FRACBITS,
                    no->bbox[1][2] >> FRACBITS, no->bbox[1][3] >> FRACBITS,
                    no->children[0], no->children[1]);
    }

    Con_Printf("LINEDEFS:\n");
    for(i = 0; i < numlines; i++)
    {
        li = LINE_PTR(i);
        Con_Printf("v1=%i v2=%i flags=%i frontside=%i backside=%i\n",
                     GET_VERTEX_IDX(li->v1), GET_VERTEX_IDX(li->v2),
                     li->flags, li->sidenum[0],
                     (SHORT(li->sidenum[1]) == NO_INDEX)? -1 : li->sidenum[1]);
    }

    Con_Printf("SIDEDEFS:\n");
    for(i = 0; i < numsides; i++)
    {
        si = SIDE_PTR(i);
        Con_Printf("xoff=%i yoff=%i toptex\"%s\" bottomtex\"%s\" midtex=\"%s\" sec=%i\n",
                    si->textureoffset >> FRACBITS, si->rowoffset >> FRACBITS,
                    si->toptexture? R_TextureNameForNum(si->toptexture) : "-",
                    si->bottomtexture? R_TextureNameForNum(si->bottomtexture) : "-",
                    si->midtexture? R_TextureNameForNum(si->midtexture) : "-",
                    GET_SECTOR_IDX(si->sector));
    }
}
//#endif

void P_LoadMap(int mapLumpStartNum, int glLumpStartNum, char *levelId)
{
    numvertexes = 0;
    numsubsectors = 0;
    numsectors = 0;
    numnodes = 0;
    numsides = 0;
    numlines = 0;
    numsegs = 0;
    numthings = 0;

    // note: most of this ordering is important
    // DJS 01/10/05 - revised load order to allow for cross-referencing
    // data during loading (detect + fix trivial errors), in preperation
    // for commonization and to support various format enhancements.
    // Make sure we have all the lumps we need to load this map
    P_FindMapLumps(mapLumpStartNum, glLumpStartNum, levelId);

    if(!P_GetMapLumpFormats(mapLumpStartNum, glLumpStartNum, levelId))
        Con_Error("P_SetupLevel: Sorry, %s gl Nodes arn't supported",
                   glNodeVer[glNodeFormat].vername);

    if(glLumpStartNum > mapLumpStartNum)
        // no GL nodes :-(
        Con_Message("Begin loading map lumps\n");
    else
        glNodeFormat = -1;

    P_LoadMapData(mlVertexes, mapLumpStartNum, glLumpStartNum);
    P_LoadMapData(glVerts, mapLumpStartNum, glLumpStartNum);
    P_LoadMapData(mlSectors, mapLumpStartNum, glLumpStartNum);
    P_LoadMapData(mlSideDefs, mapLumpStartNum, glLumpStartNum);
    P_LoadMapData(mlLineDefs, mapLumpStartNum, glLumpStartNum);

    P_ReadSideDefTextures(mapLumpStartNum + ML_SIDEDEFS);
    P_FinishLineDefs();

    // Load/Generate the blockmap
    P_LoadBlockMap(mapLumpStartNum + ML_BLOCKMAP);

    P_LoadMapData(mlThings, mapLumpStartNum, glLumpStartNum);

    P_LoadMapData(mlSegs, mapLumpStartNum, glLumpStartNum);
    P_LoadMapData(mlSSectors, mapLumpStartNum, glLumpStartNum);
    P_LoadMapData(mlNodes, mapLumpStartNum, glLumpStartNum);


    P_PrintDebugMapData();

    // Must be called before we go any further
    P_CheckLevel(levelId, false);

    // Dont need this stuff anymore
    free(missingFronts);
    P_FreeBadTexList();

    // Must be called before any mobjs are spawned.
    Con_Message("Init links\n");
    R_SetupLevel(levelId, DDSLF_INIT_LINKS);

    // Load the Reject LUT
    P_LoadReject(mapLumpStartNum + ML_REJECT);

    Con_Message("Group lines\n");
    P_GroupLines();
}

static boolean P_FindMapLumps(int lumps, int gllumps, char *levelID)
{
    int i, k;
    boolean missingLump;

    // Check the existance of the map data lumps
    for(i = 0; i < NUM_LUMPCLASSES; ++i)
    {
        mapLumps[i] = NULL;

        // Are we precaching this lump?
        if(LumpInfo[i].precache)
        {
           missingLump = (mapLumps[i] = (byte *) W_CacheLumpNum(((LumpInfo[i].glLump >= 0)?
                                                     gllumps : lumps) + LumpInfo[i].offset,
                                                     PU_STATIC)) == NULL;
        }
        else
        {   // Just check its actually there instead
            missingLump = (W_LumpName(((LumpInfo[i].glLump >= 0)?
                                       gllumps : lumps) + LumpInfo[i].offset) == NULL);
        }

        if(missingLump)
        {
            // missing map data lump. Is it required?
            if(LumpInfo[i].required)
            {
                // darn, free the lumps loaded already before we exit
                for(k = i; k >= 0; --k)
                {
                    if(mapLumps[k] != NULL)
                        Z_Free(mapLumps[k]);
                }
                Con_Error("P_GetMapLumpFormats: \"%s\" for %s could not be found.\n" \
                          " This lump is required in order to play this map.",
                          LumpInfo[i].lumpname, levelID);
            }
            else
                Con_Message("P_GetMapLumpFormats: \"%s\" for %s could not be found.\n",
                        LumpInfo[i].lumpname, levelID);
        }
        else
        {
            // Store the lump length
            LumpInfo[i].length = W_LumpLength(((LumpInfo[i].glLump >= 0)?
                                                  gllumps : lumps) + LumpInfo[i].offset);
            Con_Message("%s is %d bytes\n", LumpInfo[i].lumpname,
                                             LumpInfo[i].length);
        }
    }

    return true;
}

/*
 * Caches the gl Node data lumps, then attempts to determine
 * the version by checking the magic byte of each lump.
 */
static boolean P_GetMapLumpFormats(int lumps, int gllumps, char *levelID)
{
    int i, k;
    byte glLumpHeader[4];
    boolean failed;

    // FIXME: Add code to determine map format automatically
#if __JHEXEN__
    mapFormat = 1;
#else
    mapFormat = 0;
#endif

    // Do we have GL nodes?
    if(gllumps > lumps)
    {
        // Find out which gl node version the data uses
        // loop backwards (check for latest version first)
        for(i = GLNODE_FORMATS -1; i >= 0; --i)
        {
            // Check the header of each map data lump
            for(k = 0, failed = false; k < NUM_LUMPCLASSES && !failed; ++k)
            {
                if(LumpInfo[k].glLump >= 0)
                {
                    // Check the magic byte of the gl node lump to discern the version
                    if(glNodeVer[i].verInfo[LumpInfo[k].glLump].magicid != NULL)
                    {
                        W_ReadLumpSection(gllumps + LumpInfo[k].offset, glLumpHeader, 0, 4);

                        if(memcmp(glLumpHeader,
                                   glNodeVer[i].verInfo[LumpInfo[k].glLump].magicid, 4))
                            failed = true;
                    }
                }
            }

            // Did all lumps match the required format for this version?
            if(!failed)
            {
                Con_Message("(%s gl Node data found)\n",glNodeVer[i].vername);
                glNodeFormat = i;

                // but do we support this gl node version?
                if(glNodeVer[i].supported)
                    break;
                else
                {
                    // Unsupported... Free the data before we exit
                    for(k = NUM_LUMPCLASSES -1; k >= 0; --k)
                    {
                        if(mapLumps[k] != NULL)
                            Z_Free(mapLumps[k]);
                    }
                    return false;
                }
            }
        }
    }

    return true;
}

/*
 * Scans the map data lump array and processes them
 *
 * @parm: class of lumps to process
 */
static void P_LoadMapData(int lumpclass, int lumps, int gllumps)
{
    int i;
    int dataOffset, dataVersion, numValues;
    int internalType, lumpType;
    int elements;
    int oldNum, newNum;
    char *headerBytes;

    byte *structure;

    uint startTime;

    size_t elmSize;
    boolean glLump;
    datatype_t *dataTypes;

    // Are gl Nodes available?
    if(gllumps > lumps)
    {
        // Use the gl versions of the following lumps:
        if(lumpclass == mlSSectors)
            lumpclass = glSSects;
        else if(lumpclass == mlSegs)
            lumpclass = glSegs;
        else if(lumpclass == mlNodes)
            lumpclass = glNodes;
    }

    for(i = NUM_LUMPCLASSES -1; i>= 0; --i)
    {
        // Only process lumps that match the class requested
        if(lumpclass == LumpInfo[i].lumpclass && LumpInfo[i].func != NULL)
        {
            internalType = LumpInfo[i].dataType;

            // use the original map data structures by default
            lumpType = LumpInfo[i].mdLump;
            dataVersion = 1;
            dataOffset = 0;
            glLump = false;
            headerBytes = NULL;

            if(lumpType >= 0)
            {
                dataVersion = mlDataVer[mapFormat].verInfo[lumpType].version;
                headerBytes = (mlDataVer[mapFormat].verInfo[lumpType].magicid);
                elmSize = mlDataVer[mapFormat].verInfo[lumpType].elmSize;
                numValues = mlDataVer[mapFormat].verInfo[lumpType].numValues;

                if(mlDataVer[mapFormat].verInfo[lumpType].values != NULL)
                    dataTypes = mlDataVer[mapFormat].verInfo[lumpType].values;
                else
                    dataTypes = NULL;
            }
            else // its a gl node lumpclass
            {
                // cant load ANY gl node lumps if we dont have them...
                if(!(gllumps > lumps))
                    return;

                glLump = true;
                lumpType = LumpInfo[i].glLump;

                dataVersion = glNodeVer[glNodeFormat].verInfo[lumpType].version;
                headerBytes = (glNodeVer[glNodeFormat].verInfo[lumpType].magicid);
                elmSize = glNodeVer[glNodeFormat].verInfo[lumpType].elmSize;
                numValues = glNodeVer[glNodeFormat].verInfo[lumpType].numValues;

                if(glNodeVer[glNodeFormat].verInfo[lumpType].values != NULL)
                    dataTypes = glNodeVer[glNodeFormat].verInfo[lumpType].values;
                else
                    dataTypes = NULL;
            }

            Con_Message("Loading map data \"%s\"...\n",LumpInfo[i].lumpname);

            // Does the lump have a header?
            if(headerBytes != NULL)
                dataOffset = strlen(headerBytes);

            // How many elements are in the lump?
            elements = (LumpInfo[i].length - dataOffset) / elmSize;

            // Have we cached the lump yet?
            if(mapLumps[i] == NULL)
            {
                mapLumps[i] = W_CacheLumpNum(((glLump)? gllumps : lumps)
                                            + LumpInfo[i].offset, PU_STATIC);
            }

            // Allocate and init depending on the type of data
            switch(LumpInfo[i].dataType)
            {
                case DAM_VERTEX:
                    oldNum = numvertexes;
                    newNum = numvertexes+= elements;

                    if(oldNum > 0)
                        vertexes = Z_Realloc(vertexes, numvertexes * sizeof(vertex_t), PU_LEVEL);
                    else
                        vertexes = Z_Malloc(numvertexes * sizeof(vertex_t), PU_LEVEL, 0);

                    memset(vertexes + oldNum, 0, elements * sizeof(vertex_t));

                    structure = (byte *)(vertexes + oldNum);

                    // Kludge: we should do this properly...
                    if(dataVersion == 1)
                        firstGLvertex = numvertexes;
                    break;

                case idtThings:
                    // mapthings are game-side
                    oldNum = numthings;
                    newNum = numthings+= elements;

                    structure = NULL;

                    // Call the game's setup routine
                    if(gx.SetupForThings)
                        gx.SetupForThings(elements);
                    break;

                case DAM_LINE:
                    oldNum = numlines;
                    newNum = numlines+= elements;

                    if(oldNum > 0)
                        lines = Z_Realloc(lines, numlines * sizeof(line_t), PU_LEVEL);
                    else
                        lines = Z_Malloc(numlines * sizeof(line_t), PU_LEVEL, 0);

                    memset(lines + oldNum, 0, elements * sizeof(line_t));

                    // for missing front detection
                    missingFronts = malloc(numlines * sizeof(int));
                    memset(missingFronts, 0, sizeof(missingFronts));

                    structure = (byte *)(lines + oldNum);

                    // Call the game's setup routine
                    if(gx.SetupForLines)
                        gx.SetupForLines(elements);
                    break;

                case DAM_SIDE:
                    oldNum = numsides;
                    newNum = numsides+= elements;

                    if(oldNum > 0)
                        sides = Z_Realloc(sides, numsides * sizeof(side_t), PU_LEVEL);
                    else
                        sides = Z_Malloc(numsides * sizeof(side_t), PU_LEVEL, 0);

                    memset(sides + oldNum, 0, elements * sizeof(side_t));

                    // For BOOM style texture name overloading (TEMP)
                    sidespecials = Z_Malloc(numsides * sizeof(int), PU_STATIC, 0);
                    memset(sidespecials + oldNum, 0, elements * sizeof(int));

                    structure = (byte *)(sides + oldNum);

                    // Call the game's setup routine
                    if(gx.SetupForSides)
                        gx.SetupForSides(elements);
                    break;

                case DAM_SEG:
                    // Segs are read into a temporary buffer before processing
                    oldNum = numsegs;
                    newNum = numsegs+= elements;

                    if(oldNum > 0)
                    {
                        segs = Z_Realloc(segs, numsegs * sizeof(seg_t), PU_LEVEL);
                        segstemp = Z_Realloc(segstemp, numsegs * sizeof(mapseg_t), PU_STATIC);
                    }
                    else
                    {
                        segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, 0);
                        segstemp = Z_Malloc(numsegs * sizeof(mapseg_t), PU_STATIC, 0);
                    }

                    memset(segs + oldNum, 0, elements * sizeof(seg_t));
                    memset(segstemp + oldNum, 0, elements * sizeof(mapseg_t));

                    structure = (byte *)(segstemp + oldNum);
                    break;

                case DAM_SUBSECTOR:
                    oldNum = numsubsectors;
                    newNum = numsubsectors+= elements;

                    if(oldNum > 0)
                        subsectors = Z_Realloc(subsectors, numsubsectors * sizeof(subsector_t), PU_LEVEL);
                    else
                        subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_LEVEL, 0);

                    memset(subsectors + oldNum, 0, elements * sizeof(subsector_t));

                    structure = (byte *)(subsectors + oldNum);
                    break;

                case DAM_NODE:
                    oldNum = numnodes;
                    newNum = numnodes+= elements;

                    if(oldNum > 0)
                        nodes = Z_Realloc(nodes, numnodes * sizeof(node_t), PU_LEVEL);
                    else
                        nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, 0);

                    memset(nodes + oldNum, 0, elements * sizeof(node_t));

                    structure = (byte *)(nodes + oldNum);
                    break;

                case DAM_SECTOR:
                    oldNum = numsectors;
                    newNum = numsectors+= elements;

                    if(oldNum > 0)
                        sectors = Z_Realloc(sectors, numsectors * sizeof(sector_t), PU_LEVEL);
                    else
                        sectors = Z_Malloc(numsectors * sizeof(sector_t), PU_LEVEL, 0);

                    memset(sectors + oldNum, 0, elements * sizeof(sector_t));

                    structure = (byte *)(sectors + oldNum);

                    // Call the game's setup routine
                    if(gx.SetupForSectors)
                        gx.SetupForSectors(elements);
                    break;
            }

            // Read in the lump data
            startTime = Sys_GetRealTime();
            LumpInfo[i].func(structure, LumpInfo[i].dataType,
                               (mapLumps[i])+dataOffset, elmSize, elements,
                               dataVersion, numValues, dataTypes);
            // Perform any additional processing required
            switch(LumpInfo[i].dataType)
            {
                case DAM_SEG:
                    P_ProcessSegs(dataVersion);
                    break;

                case DAM_LINE:
                    // BOOM uses a system of overloaded sidedef texture names as parameters
                    // instead of textures. These depend on the special of the linedef.
                    P_CompileSideSpecialsList();
                    break;
            }

            // How much time did we spend?
            Con_Message("P_LoadMapData: Done \"%s\" in %.4f seconds.\n",
                        LumpInfo[i].lumpname, (Sys_GetRealTime() - startTime) / 1000.0f);

            Z_Free(mapLumps[i]);
        }
    }
}

/*
 * If we encountered any problems during setup - announce them
 * to the user. If the errors couldn't be repaired or we cant
 * continue safely - an error dialog is presented.
 *
 * TODO: latter on this will be expanded to check for various
 *       doom.exe renderer hacks and other stuff.
 *
 * @parm silent - dont announce non-critcal errors
 */
static void P_CheckLevel(char *levelID, boolean silent)
{
    int i, printCount;
    boolean canContinue = !numMissingFronts;
    boolean hasErrors = (numBadTexNames || numMissingFronts);

    Con_Message("P_CheckLevel: Checking %s for errors...\n", levelID);

    // If we are missing any front sidedefs announce them to the user.
    // Critical
    if(numMissingFronts)
    {
        Con_Message(" ![100] %d linedef(s) missing a front sidedef:\n",
                    numMissingFronts);

        printCount = 0;
        for(i = numlines -1; i >=0; --i)
        {
            if(missingFronts[i] == 1)
            {
                Con_Printf("%s%d,", printCount? " ": "   ", i);

                if((++printCount) > 9)
                {   // print 10 per line then wrap.
                    printCount = 0;
                    Con_Printf("\n ");
                }
            }
        }
        Con_Printf("\n");
    }

    // Announce any bad texture names we came across when loading the map.
    // Non-critical as a "MISSING" texture will be drawn in place of them.
    if(numBadTexNames && !silent)
    {
        Con_Message("  [110] %d bad texture name(s):\n", numBadTexNames);

        for(i = numBadTexNames -1; i >= 0; --i)
            Con_Message("   Found %d unknown %s \"%s\"\n", badTexNames[i].count,
                        (badTexNames[i].planeTex)? "Flat" : "Texture",
                        badTexNames[i].name);
    }

    if(!canContinue)
        Con_Error("\nP_CheckLevel: Critical errors encountered (marked with '!').\n" \
                  "You will need to fix these errors in order to play this map.\n");
}

/*
 * Builds sector line lists and subsector sector numbers.
 * Finds block bounding boxes for sectors.
 */
static void P_GroupLines(void)
{
    int     *linesInSector;
    int     block;
    int     i, k;
    int     secid;
    unsigned long     j;

    line_t **linebuffer;
    line_t **linebptr;
    line_t  *li;

    side_t  *sid;
    sector_t *sec;
    subsector_t *ss;
    seg_t  *seg;
    fixed_t bbox[4];

    // This doesn't belong here, belongs in a P_InitSides func
    for(i = numsides - 1; i >= 0; --i, sid++)
    {
        sid = SIDE_PTR(i);
        memset(sid->toprgb, 0xff, 3);
        memset(sid->midrgba, 0xff, 4);
        memset(sid->bottomrgb, 0xff, 3);
        sid->blendmode = BM_NORMAL;
    }

    Con_Message(" Sector look up\n");
    // look up sector number for each subsector
    ss = SUBSECTOR_PTR(0);
    for(i = numsubsectors - 1; i >= 0; --i, ss++)
    {
        seg = SEG_PTR(ss->firstline);
        ss->sector = seg->sidedef->sector;
        for(j = ss->linecount -1; j >= 0; --j, seg++)
            if(seg->sidedef)
            {
                ss->sector = seg->sidedef->sector;
                break;
            }
    }

    Con_Message(" Build line tables\n");
    // build line tables for each sector
    linebuffer = Z_Malloc(numUniqueLines * sizeof(line_t), PU_LEVEL, 0);
    linebptr = linebuffer;
    linesInSector = malloc(numsectors * sizeof(int));
    memset(linesInSector, 0, numsectors * sizeof(int));

    for(i = numsectors -1, sec = SECTOR_PTR(0); i >= 0; --i, sec++)
    {
        if(sec->linecount > 0)
        {
            sec->Lines = linebptr;
            linebptr += sec->linecount;
        }
        // These don't really belong here.
        // Should go in a P_InitSectors() func
        sec->thinglist = NULL;
        memset(sec->rgb, 0xff, 3);
        memset(sec->floorrgb, 0xff, 3);
        memset(sec->ceilingrgb, 0xff, 3);
    }

    for(k = numlines -1, li = LINE_PTR(0); k >= 0; --k, li++)
    {
        if(li->frontsector != NULL)
        {
            secid = GET_SECTOR_IDX(li->frontsector);
            li->frontsector->Lines[linesInSector[secid]++] = li;
        }

        if(li->backsector != NULL && li->backsector != li->frontsector)
        {
            secid = GET_SECTOR_IDX(li->backsector);
            li->backsector->Lines[linesInSector[secid]++] = li;
        }
    }

    for(i = numsectors, sec = SECTOR_PTR(0); i; --i, sec++)
    {
        if(linesInSector[numsectors- i] != sec->linecount)
            Con_Error("P_GroupLines: miscounted");

        if(sec->linecount != 0)
        {
            M_ClearBox(bbox);

            for(k = sec->linecount; k ; --k)
            {
                li = sec->Lines[sec->linecount - k];
                M_AddToBox(bbox, li->v1->x, li->v1->y);
                M_AddToBox(bbox, li->v2->x, li->v2->y);
            }
        }
/*
        //FIXME: jDoom would like to know this.
        else // stops any wayward line specials from misbehaving
            sec->tag = 0;
*/
        // set the degenmobj_t to the middle of the bounding box
        sec->soundorg.x = (bbox[BOXRIGHT] + bbox[BOXLEFT]) / 2;
        sec->soundorg.y = (bbox[BOXTOP] + bbox[BOXBOTTOM]) / 2;

        // adjust bounding box to map blocks
        block = (bbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block >= bmapheight ? bmapheight - 1 : block;
        sec->blockbox[BOXTOP] = block;

        block = (bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sec->blockbox[BOXBOTTOM] = block;

        block = (bbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block >= bmapwidth ? bmapwidth - 1 : block;
        sec->blockbox[BOXRIGHT] = block;

        block = (bbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sec->blockbox[BOXLEFT] = block;
    }

    free(linesInSector);
    Con_Message("Done group lines\n");
}

/*
 * Reads a value from the (little endian) source buffer. Does some basic
 * type checking so that incompatible types are not assigned.
 * Simple conversions are also done, e.g., float to fixed.
 */
static void ReadValue(void* dest, valuetype_t valueType,
                        byte *src, int size, int flags, int index)
{
    if(valueType == DDVT_BYTE)
    {
        byte* d = dest;
        switch(size)
        {
        case 1:
            *d = *src;
            break;
        case 2:
            *d = *src;
            break;
        case 4:
            *d = *src;
            break;
        default:
            Con_Error("GetValue: DDVT_BYTE incompatible with value type %s.\n",
                      DMU_Str(size));
        }
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short* d = dest;
        switch(size)
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = SHORT(*((short*)(src)));
            }
            break;
// FIXME
/*        case 8:
            if(flags & DT_TEXTURE)
            {
                *d = P_CheckTexture((char*)((long long*)(src)), false);
            }
            else if(flags & DT_FLAT)
            {
                *d = P_CheckTexture((char*)((long long*)(src)), true);
            }
            break;
*/         default:
            Con_Error("GetValue: DDVT_SHORT incompatible with value type %s.\n",
                      DMU_Str(size));
         }
    }
    else if(valueType = DDVT_FIXED)
    {
        fixed_t* d = dest;

        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = SHORT(*((short*)(src)));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                *d = ULONG(*((long*)(src)));
            else
                *d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("GetValue: DDVT_FIXED incompatible with value type %s.\n",
                      DMU_Str(size));
        }
    }
    else if(valueType = DDVT_ULONG)
    {
        unsigned long* d = dest;

        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = SHORT(*((short*)(src)));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                *d = ULONG(*((long*)(src)));
            else
                *d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("GetValue: DDVT_ULONG incompatible with value type %s.\n",
                      DMU_Str(size));
        }
    }
    else if(valueType == DDVT_INT)
    {
        int* d = dest;

        switch(size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((short*)(src)));

                    *d = NO_INDEX;

                    if(num != NO_INDEX)
                        *d = num;
                }
                else
                {
                    if(flags & DT_FRACBITS)
                        *d = SHORT(*((short*)(src))) << FRACBITS;
                    else
                        *d = SHORT(*((short*)(src)));
                }
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                *d = ULONG(*((long*)(src)));
            else
                *d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("GetValue: DDVT_INT incompatible with value type %s.\n",
                      DMU_Str(size));;
        }
    }
    else
    {
        Con_Error("GetValue: unknown value type %s.\n", DMU_Str(valueType));
    }
}

static void ReadMapProperty(byte *structure, int dataType,
                           int element, const datatype_t* prop, byte *buffer)
{
    valuetype_t destType;
    void*   dest;

    // Handle unknown (game specific) properties.
    if(prop->gameprop)
    {
        byte    tmpbyte = 0;
        short   tmpshort = 0;
        fixed_t tmpfixed = 0;
        int     tmpint = 0;
        float   tmpfloat = 0;

        switch(prop->size)
        {
        case DDVT_BYTE:
            dest = &tmpbyte;
            break;
        case DDVT_SHORT:
            dest = &tmpshort;
            break;
        case DDVT_FIXED:
            dest = &tmpfixed;
            break;
        case DDVT_INT:
            dest = &tmpint;
            break;
        case DDVT_FLOAT:
            dest = &tmpfloat;
            break;
        default:
            Con_Error("ReadMapProperty: Unsupported data type id %i.\n", prop->size);
        };

        ReadValue(dest, prop->size, buffer + prop->offset, prop->size, prop->flags, 0);

        gx.HandleMapDataProperty(element, dataType, prop->property, prop->size, dest);
    }
    else
    {
        // These are the exported map data properties that can be
        // assigned to when reading map data.
        switch(dataType)
        {
        case DAM_VERTEX:
            {
            vertex_t *p = VERTEX_PTR(element);
            switch(prop->property)
            {
            case DAM_X:
                dest = &p->x;
                destType = DDVT_FIXED;
                break;

            case DAM_Y:
                dest = &p->y;
                destType = DDVT_FIXED;
                break;

            default:
                Con_Error("ReadMapProperty: DAM_VERTEX has no property %s.\n",
                          "blah");
            }

            ReadValue(dest, destType, buffer + prop->offset, prop->size, prop->flags, 0);
            break;
            }

        case DAM_LINE:
            {
            line_t *p = LINE_PTR(element);
            switch(prop->property)
            {
            case DAM_VERTEX1:
                dest = &p->v1;
                destType = DDVT_INT;
                break;

            case DAM_VERTEX2:
                dest = &p->v2;
                destType = DDVT_INT;
                break;

            case DAM_FLAGS:
                dest = &p->flags;
                destType = DDVT_INT;
                break;

            case DAM_SIDE0:
                dest = &p->sidenum[0];
                destType = DDVT_INT;
                break;

            case DAM_SIDE1:
                dest = &p->sidenum[1];
                destType = DDVT_INT;
                break;

            default:
                Con_Error("ReadMapProperty: DAM_LINE has no property %s.\n",
                          "blah");
            }

            ReadValue(dest, destType, buffer + prop->offset, prop->size, prop->flags, 0);
            break;
            }

        case DAM_SIDE:
            {
            side_t *p = SIDE_PTR(element);
            switch(prop->property)
            {
            case DAM_TEXTURE_OFFSET_X:
                dest = &p->textureoffset;
                destType = DDVT_FIXED;
                break;

            case DAM_TEXTURE_OFFSET_Y:
                dest = &p->rowoffset;
                destType = DDVT_FIXED;
                break;

            case DAM_TOP_TEXTURE:
                dest = &p->toptexture;
                destType = DDVT_FLAT_INDEX;
                break;

            case DAM_MIDDLE_TEXTURE:
                dest = &p->midtexture;
                destType = DDVT_FLAT_INDEX;
                break;

            case DAM_BOTTOM_TEXTURE:
                dest = &p->bottomtexture;
                destType = DDVT_FLAT_INDEX;
                break;

            case DAM_FRONT_SECTOR:
                dest = &p->sector;
                destType = DDVT_INT;
                break;

            default:
                Con_Error("ReadMapProperty: DAM_SIDE has no property %s.\n",
                          "blah");
            }

            ReadValue(dest, destType, buffer + prop->offset, prop->size, prop->flags, 0);
            break;
            }

        case DAM_SECTOR:
            {
            sector_t *p = SECTOR_PTR(element);
            switch(prop->property)
            {
            case DAM_FLOOR_HEIGHT:
                dest = &p->floorheight;
                destType = DDVT_FIXED;
                break;

            case DAM_CEILING_HEIGHT:
                dest = &p->ceilingheight;
                destType = DDVT_FIXED;
                break;

            case DAM_FLOOR_TEXTURE:
                dest = &p->floorpic;
                destType = DDVT_FLAT_INDEX;
                break;

            case DAM_CEILING_TEXTURE:
                dest = &p->ceilingpic;
                destType = DDVT_FLAT_INDEX;
                break;

            case DAM_LIGHT_LEVEL:
                dest = &p->lightlevel;
                destType = DDVT_SHORT;
                break;

            default:
                Con_Error("ReadMapProperty: DAM_SECTOR has no property %s.\n",
                          "blah");
            }

            ReadValue(dest, destType, buffer + prop->offset, prop->size, prop->flags, 0);
            break;
            }

        case DAM_SEG:  // Segs are read into an interim format
            {
            mapseg_t *p = &segstemp[element];
            switch(prop->property)
            {
            case DAM_VERTEX1:
                dest = &p->v1;
                destType = DDVT_INT;
                break;

            case DAM_VERTEX2:
                dest = &p->v2;
                destType = DDVT_INT;
                break;

            case DAM_ANGLE:
                dest = &p->angle;
                destType = DDVT_INT;
                break;

            case DAM_LINE:
                dest = &p->linedef;
                destType = DDVT_INT;
                break;

            case DAM_SIDE:
                dest = &p->side;
                destType = DDVT_INT;
                break;

            case DAM_OFFSET:
                dest = &p->offset;
                destType = DDVT_INT;
                break;

            default:
                Con_Error("ReadMapProperty: DAM_SEG has no property %s.\n",
                          "blah");
            }

            ReadValue(dest, destType, buffer + prop->offset, prop->size, prop->flags, 0);
            break;
            }

        case DAM_SUBSECTOR:
            {
            subsector_t *p = SUBSECTOR_PTR(element);
            switch(prop->property)
            {
            case DAM_LINE_COUNT:
                dest = &p->linecount;
                destType = DDVT_INT;
                break;

            case DAM_LINE_FIRST:
                dest = &p->firstline;
                destType = DDVT_INT;
                break;

            default:
                Con_Error("ReadMapProperty: DAM_SUBSECTOR has no property %s.\n",
                          "blah");
            }

            ReadValue(dest, destType, buffer + prop->offset, prop->size, prop->flags, 0);
            break;
            }

        case DAM_NODE:
            {
            node_t *p = NODE_PTR(element);
            switch(prop->property)
            {
            case DAM_X:
                dest = &p->x;
                destType = DDVT_FIXED;
                break;

            case DAM_Y:
                dest = &p->y;
                destType = DDVT_FIXED;
                break;

            case DAM_DX:
                dest = &p->dx;
                destType = DDVT_FIXED;
                break;

            case DAM_DY:
                dest = &p->dy;
                destType = DDVT_FIXED;
                break;

            case DAM_BBOX_RIGHT_TOP_Y:
                dest = &p->bbox[0][0];
                destType = DDVT_FIXED;
                break;

            case DAM_BBOX_RIGHT_LOW_Y:
                dest = &p->bbox[0][1];
                destType = DDVT_FIXED;
                break;

            case DAM_BBOX_RIGHT_LOW_X:
                dest = &p->bbox[0][2];
                destType = DDVT_FIXED;
                break;

            case DAM_BBOX_RIGHT_TOP_X:
                dest = &p->bbox[0][3];
                destType = DDVT_FIXED;
                break;

            case DAM_BBOX_LEFT_TOP_Y:
                dest = &p->bbox[1][0];
                destType = DDVT_FIXED;
                break;

            case DAM_BBOX_LEFT_LOW_Y:
                dest = &p->bbox[1][1];
                destType = DDVT_FIXED;
                break;

            case DAM_BBOX_LEFT_LOW_X:
                dest = &p->bbox[1][2];
                destType = DDVT_FIXED;
                break;

            case DAM_BBOX_LEFT_TOP_X:
                dest = &p->bbox[1][3];
                destType = DDVT_FIXED;
                break;

            case DAM_CHILD_RIGHT:
                dest = &p->children[0];
                destType = DDVT_INT;
                break;

            case DAM_CHILD_LEFT:
                dest = &p->children[1];
                destType = DDVT_INT;
                break;

            default:
                Con_Error("ReadMapProperty: DAM_NODE has no property %s.\n",
                          "blah");
            }

            ReadValue(dest, destType, buffer + prop->offset, prop->size, prop->flags, 0);
            break;
            }
        default:
            Con_Error("ReadMapProperty: Type cannot be assigned to from a map format.\n");
        }
    }
}

//
// FIXME: 32bit/64bit pointer sizes
// Shame we can't do arithmetic on void pointers in C/C++
//
static void P_ReadBinaryMapData(byte *structure, int dataType, byte *buffer, size_t elmSize,
                           int elements, int version, int values, const datatype_t *types)
{
    int     i = 0, k;
    int     blocklimit;
    size_t  structSize;

    Con_Message("Loading (%i elements) ver %i vals %i...\n", elements, version, values);

    switch(dataType)
    {
    case DAM_VERTEX:
        structSize = VTXSIZE;
        break;

    case DAM_LINE:
        structSize = LINESIZE;
        break;

    case DAM_SIDE:
        structSize = SIDESIZE;
        break;

    case DAM_SECTOR:
        structSize = SECTSIZE;
        break;

    case DAM_SUBSECTOR:
        structSize = SUBSIZE;
        break;

    case DAM_SEG: // segs are read into an interim format
        structSize = sizeof(mapseg_t);
        break;

    case DAM_NODE:
        structSize = NODESIZE;
        break;

    case idtThings:
        structSize = 10;
        break;
    }

    // Not very pretty to look at but it IS pretty quick :-)
    blocklimit = (elements / 8) * 8;
    while(i < blocklimit)
    {
        {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i+1, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i+2, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i+3, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i+4, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i+5, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i+6, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i+7, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
        }
        i += 8;
    }

    // Have we got any left to do?
    if(i < elements)
    {
        // Yes, jump into the switch at the number of elements remaining
        switch(elements - i)
        {
        case 7:
            {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
            }
            ++i;
        case 6:
            {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
            }
            ++i;
        case 5:
            {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
            }
            ++i;
        case 4:
            {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
            }
            ++i;
        case 3:
            {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
            }
            ++i;
        case 2:
            {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
            }
            ++i;
        case 1:
            {
            for(k = values-1; k>= 0; k--)
                ReadMapProperty(structure, dataType, i, &types[k], buffer);

            buffer += elmSize;
            if(structure)
                structure += structSize;
            }
            ++i;
        }
    }
}

/*
 * Converts the temporary seg data into "real" segs.
 * If angle and offset information is not provided they are
 * calculated here.
 */
static void P_ProcessSegs(int version)
{
    int     i;
    seg_t  *li;
    line_t *ldef;

    mapseg_t *ml;

    ml = segstemp;

    for(i = 0; i < numsegs; i++, ml++)
    {
        li = SEG_PTR(i);

        // Which version?
        switch(version)
        {
            case 0:  // (mapseg_t)
                li->v1 = VERTEX_PTR(ml->v1);
                li->v2 = VERTEX_PTR(ml->v2);
                break;

            case 1:  // (glseg_t)
                li->v1 =
                    VERTEX_PTR(ml->v1 & 0x8000 ?
                              firstGLvertex + (ml->v1 & ~0x8000) :
                              ml->v1);
                li->v2 =
                    VERTEX_PTR(ml->v2 & 0x8000 ?
                              firstGLvertex + (ml->v2 & ~0x8000) :
                              ml->v2);
                break;

            case 3:
            case 5:
                li->v1 =
                    VERTEX_PTR(ml->v1 & 0xc0000000 ?
                              firstGLvertex + (ml->v1 & ~0xc0000000) :
                              ml->v1);
                li->v2 =
                    VERTEX_PTR(ml->v2 & 0xc0000000 ?
                              firstGLvertex + (ml->v2 & ~0xc0000000) :
                              ml->v2);
                break;

            default:
                Con_Error("P_ProcessSegs: Error. Unsupported Seg format.");
                break;
        }

        li->angle = ml->angle;
        li->offset = ml->offset;

        if(ml->linedef != NO_INDEX)
        {
            ldef = LINE_PTR(ml->linedef);
            li->linedef = ldef;
            li->sidedef = SIDE_PTR(ldef->sidenum[ml->side]);
            li->frontsector = SIDE_PTR(ldef->sidenum[ml->side])->sector;
            if(ldef->flags & ML_TWOSIDED &&
               ldef->sidenum[ml->side ^ 1] != NO_INDEX)
            {
                li->backsector = SIDE_PTR(ldef->sidenum[ml->side ^ 1])->sector;
            }
            else
            {
                ldef->flags &= ~ML_TWOSIDED;
                li->backsector = 0;
            }

            if(!li->offset)
            {
                if(ml->side == 0)
                    li->offset =
                        FRACUNIT * AccurateDistance(li->v1->x - ldef->v1->x,
                                                    li->v1->y - ldef->v1->y);
                else
                    li->offset =
                        FRACUNIT * AccurateDistance(li->v1->x - ldef->v2->x,
                                                    li->v1->y - ldef->v2->y);
            }

            if(!li->angle)
                li->angle =
                    bamsAtan2((li->v2->y - li->v1->y) >> FRACBITS,
                              (li->v2->x - li->v1->x) >> FRACBITS) << FRACBITS;
        }
        else
        {
            li->linedef = NULL;
            li->sidedef = NULL;
            li->frontsector = NULL;
            li->backsector = NULL;
        }

        // Calculate the length of the segment. We need this for
        // the texture coordinates. -jk
        li->length =
            AccurateDistance(li->v2->x - li->v1->x, li->v2->y - li->v1->y);
        if(version == 0 && li->length == 0)
            li->length = 0.01f; // Hmm...
    }

    // We're done with the temporary data
    Z_Free(segstemp);
}

/*
 * TODO: Remove this and use the generic P_ReadBinaryMapData
 */
static void P_ReadLineDefs(byte *structure, int dataType, byte *buffer, size_t elmsize,
                           int elements, int version, int values, const datatype_t *types)
{
    int     i;
    maplinedef_t *mld;
    maplinedefhex_t *mldhex;
    line_t *ld;
    short   tmp;
    byte    tmpb;

    Con_Message("Loading Linedefs (%i) ver %i...\n", elements, version);

    mld = (maplinedef_t *) buffer;
    mldhex = (maplinedefhex_t *) buffer;
    switch(version)
    {
        case 1: // DOOM format
            for(i = 0; i < numlines; i++, mld++, mldhex++)
            {
                ld = LINE_PTR(i);

                ld->flags = SHORT(mld->flags);

                tmp = SHORT(mld->special);
                gx.HandleMapDataProperty(i, DDVT_INT, DAM_LINE_SPECIAL, 0, &tmp);
                tmp = SHORT(mld->tag);
                gx.HandleMapDataProperty(i, DDVT_INT, DAM_LINE_TAG, 0, &tmp);

                ld->v1 = VERTEX_PTR(SHORT(mld->v1));
                ld->v2 = VERTEX_PTR(SHORT(mld->v2));

                P_SetLineSideNum(&ld->sidenum[0], SHORT(mld->sidenum[0]));
                P_SetLineSideNum(&ld->sidenum[1], SHORT(mld->sidenum[1]));
            }
            break;

        case 2: // HEXEN format
            for(i = 0; i < numlines; i++, mld++, mldhex++)
            {
                ld = LINE_PTR(i);

                ld->flags = SHORT(mldhex->flags);

                tmpb = mldhex->special;
                gx.HandleMapDataProperty(i, DDVT_BYTE, DAM_LINE_SPECIAL, 0, &tmpb);
                tmpb = mldhex->arg1;
                gx.HandleMapDataProperty(i, DDVT_BYTE, DAM_LINE_ARG1, 0, &tmpb);
                tmpb = mldhex->arg2;
                gx.HandleMapDataProperty(i, DDVT_BYTE, DAM_LINE_ARG2, 0, &tmpb);
                tmpb = mldhex->arg3;
                gx.HandleMapDataProperty(i, DDVT_BYTE, DAM_LINE_ARG3, 0, &tmpb);
                tmpb = mldhex->arg4;
                gx.HandleMapDataProperty(i, DDVT_BYTE, DAM_LINE_ARG4, 0, &tmpb);
                tmpb = mldhex->arg5;
                gx.HandleMapDataProperty(i, DDVT_BYTE, DAM_LINE_ARG5, 0, &tmpb);

                ld->v1 = VERTEX_PTR(SHORT(mldhex->v1));
                ld->v2 = VERTEX_PTR(SHORT(mldhex->v2));

                P_SetLineSideNum(&ld->sidenum[0], SHORT(mldhex->sidenum[0]));
                P_SetLineSideNum(&ld->sidenum[1], SHORT(mldhex->sidenum[1]));
            }
            break;
    }
}

// In BOOM sidedef textures can be overloaded depending on
// the special of the line that uses the sidedef.
// Hmm. How should we handle this?
static void P_CompileSideSpecialsList(void)
{
    int i;
    line_t *ld;

    ld = LINE_PTR(0);
    for(i = numlines; i ; i--, ld++)
    {
/*
        if(ld->sidenum[0] != NO_INDEX && ld->special)
            sidespecials[ld->sidenum[0]] = ld->special;
*/
    }
}

// FIXME: Could be a macro?
static void P_SetLineSideNum(int *side, unsigned short num)
{
    num = SHORT(num);

    *side = NO_INDEX;
    if(num == NO_INDEX)
    {
        // do nothing
    }
    else
        *side = num;
}

/*
 * Completes the linedef loading by resolving the front/back
 * sector ptrs which we couldn't do earlier as the sidedefs
 * hadn't been loaded at the time.
 *
 * Also increments the sector->linecount and tracks the
 * number of unqiue linedefs.
 *
 * Sidedefs MUST be loaded before this is called
 */
static void P_FinishLineDefs(void)
{
    int i;
    line_t *ld;
    vertex_t *v1;
    vertex_t *v2;

    Con_Message("Finalizing Linedefs...\n");

    numUniqueLines = 0;
    for(i = 0; i < numlines; i++)
    {
        ld = LINE_PTR(i);

        v1 = ld->v1;
        v2 = ld->v2;
        ld->dx = v2->x - v1->x;
        ld->dy = v2->y - v1->y;

        if(!ld->dx)
            ld->slopetype = ST_VERTICAL;
        else if(!ld->dy)
            ld->slopetype = ST_HORIZONTAL;
        else
        {
            if(FixedDiv(ld->dy, ld->dx) > 0)
                ld->slopetype = ST_POSITIVE;
            else
                ld->slopetype = ST_NEGATIVE;
        }

        if(v1->x < v2->x)
        {
            ld->bbox[BOXLEFT] = v1->x;
            ld->bbox[BOXRIGHT] = v2->x;
        }
        else
        {
            ld->bbox[BOXLEFT] = v2->x;
            ld->bbox[BOXRIGHT] = v1->x;
        }

        if(v1->y < v2->y)
        {
            ld->bbox[BOXBOTTOM] = v1->y;
            ld->bbox[BOXTOP] = v2->y;
        }
        else
        {
            ld->bbox[BOXBOTTOM] = v2->y;
            ld->bbox[BOXTOP] = v1->y;
        }

        if(ld->sidenum[0] != NO_INDEX)
            ld->frontsector = SIDE_PTR(ld->sidenum[0])->sector;
        else
            ld->frontsector = 0;

        if(ld->sidenum[1] != NO_INDEX)
            ld->backsector = SIDE_PTR(ld->sidenum[1])->sector;
        else
            ld->backsector = 0;

        // Increase the sector line count
        if(ld->frontsector)
        {
            ld->frontsector->linecount++;
            numUniqueLines++;
        }
        else
        {   // A missing front sidedef
            missingFronts[i] = 1;
            numMissingFronts++;
        }

        if(ld->backsector && ld->backsector != ld->frontsector)
        {
            ld->backsector->linecount++;
            numUniqueLines++;
        }
    }
}

/*
 * TODO: Remove this and use the generic P_ReadBinaryMapData
 */
static void P_ReadSideDefs(byte *structure, int dataType, byte *buffer, size_t elmsize,
                           int elements, int version, int values, const datatype_t *types)
{
    int     i, index;
    mapsidedef_t *msd;
    side_t *sd;

    Con_Message("Loading Sidedefs (%i) ver %i...\n", elements, version);

    sd = SIDE_PTR(0);
    msd = (mapsidedef_t *) buffer;
/*    for(i = numsides -1; i >= 0; --i, msd++, sd++)
    {
        // There may be bogus sector indices here.
        index = SHORT(msd->sector);
        if(index >= 0 && index < numsectors)
            sd->sector = SECTOR_PTR(index);
    }*/
}

/*
 * MUST be called after Linedefs are loaded.
 *
 * Sidedef texture fields might be overloaded with all kinds of
 * different strings.
 *
 * In BOOM for example, these fields might contain strings that
 * influence what special is assigned to the line.
 *
 * In order to allow the game to make the best decision on what
 * to do - we must provide the game with everything we know about
 * this property and the unaltered erogenous value.
 *
 * In the above example, jDoom will request various properties
 * of this side's linedef (hence why this has to wait until the
 * linedefs have been loaded).
 *
 * If the game doesn't know what the erogenous value means:
 * We'll ignore it and assign the "MISSING" texture instead.
 */
static void P_ReadSideDefTextures(int lump)
{
    byte   *data;
    int     i, index;
    mapsidedef_t *msd;
    side_t *sd;

    Con_Message("Loading Sidedef Texture IDs...\n");

    data = W_CacheLumpNum(lump, PU_STATIC);

    msd = (mapsidedef_t *) data;

    for(i = 0; i < numsides; i++, msd++)
    {
        sd = SIDE_PTR(i);
        index = SHORT(msd->sector);
        if(index >= 0 && index < numsectors)
            sd->sector = SECTOR_PTR(index);
        sd->textureoffset = SHORT(msd->textureoffset) << FRACBITS;
        sd->rowoffset = SHORT(msd->rowoffset) << FRACBITS;

        sd->toptexture = P_CheckTexture(msd->toptexture, false, DAM_SIDE,
                                        i, DAM_TOP_TEXTURE);

        sd->bottomtexture = P_CheckTexture(msd->bottomtexture, false, DAM_SIDE,
                                           i, DAM_BOTTOM_TEXTURE);

        sd->midtexture = P_CheckTexture(msd->midtexture, false, DAM_SIDE,
                                        i, DAM_MIDDLE_TEXTURE);
    }

    Z_Free(data);

    // We've finished with the sidespecials list
    Z_Free(sidespecials);
}

/*
 * Checks texture *name to see if it is a valid name.
 * Or decompose it into a colormap reference, color value etc...
 *
 * Returns the id of the texture else -1. If we are currently
 * doing level setup and the name isn't valid - add the name
 * to the "bad textures" list so it can be presented to the
 * user (in P_CheckMap()) instead of loads of duplicate missing
 * texture messages from Doomsday.
 *
 * @parm planeTex: true = *name comes from a map data field
 *                         intended for plane textures (Flats)
 */
int P_CheckTexture(char *name, boolean planeTex, int dataType,
                   int element, int property)
{
    int i;
    int id;
    char namet[9];
    boolean known = false;

    if(planeTex)
        id = R_FlatNumForName(name);
    else
        id = R_TextureNumForName(name);

    // At this point we don't know WHAT it is.
    // Perhaps the game knows what to do?
    if(gx.HandleMapDataPropertyValue)
        id = gx.HandleMapDataPropertyValue(element, dataType, property,
                                           DDVT_FLAT_INDEX, name);

    // Hmm, must be a bad texture name then...?
    // During level setup we collect this info so we can
    // present it to the user latter on.
    if(levelSetup && id == -1)
    {
        namet[8] = 0;
        memcpy(namet, name, 8);

        // Do we already know about it?
        if(numBadTexNames > 0)
        {
            for(i = numBadTexNames -1; i >= 0 && !known; --i)
            {
                if(!strcmp(badTexNames[i].name, namet) &&
                    badTexNames[i].planeTex == planeTex)
                {
                    // Yep we already know about it.
                    known = true;
                    badTexNames[i].count++;
                }
            }
        }

        if(!known)
        {   // A new unknown texture. Add it to the list
            if(++numBadTexNames > maxBadTexNames)
            {
                // Allocate more memory
                maxBadTexNames *= 2;
                if(maxBadTexNames < numBadTexNames)
                    maxBadTexNames = numBadTexNames;

                badTexNames = realloc(badTexNames, sizeof(badtex_t)
                                                    * maxBadTexNames);
            }

            badTexNames[numBadTexNames -1].name = malloc(strlen(namet) +1);
            strcpy(badTexNames[numBadTexNames -1].name, namet);

            badTexNames[numBadTexNames -1].planeTex = planeTex;
            badTexNames[numBadTexNames -1].count = 1;
        }
    }

    return id;
}

/*
 * Frees memory we allocated for bad texture name collection.
 */
static void P_FreeBadTexList(void)
{
    int i;

    if(badTexNames != NULL)
    {
        for(i= numBadTexNames -1; i >= 0; --i)
        {
            free(badTexNames[i].name);
            badTexNames[i].name = NULL;
        }

        free(badTexNames);
        badTexNames = NULL;

        numBadTexNames = maxBadTexNames = 0;
    }
}

/*
 * The DED for the game.dll should tell doomsday what
 * data maps to which internal data value, what size
 * the data item is etc.
 *
 * TODO: ALL of this can be moved to a DED
 *
 * TEMP the initialization of internal data struct info
 * is currently done here. (FIXME!!!: It isn't free'd on exit!)
 */
static void P_InitMapDataFormats(void)
{
    int i, j, index, lumpClass;
    int mlver, glver;
    size_t *mlptr = NULL;
    size_t *glptr = NULL;
    mldataver_t *stiptr = NULL;
    glnodever_t *glstiptr = NULL;

    // Setup the map data formats for the different
    // versions of the map data structs.

    // Calculate the size of the map data structs
    for(i = MAPDATA_FORMATS -1; i >= 0; --i)
    {
        for(j = 0; j < NUM_LUMPCLASSES; j++)
        {
            lumpClass = LumpInfo[j].lumpclass;
            index = LumpInfo[j].mdLump;

            mlver = (mlDataVer[i].verInfo[index].version);
            mlptr = &(mlDataVer[i].verInfo[index].elmSize);
            stiptr = &(mlDataVer[i]);

            if(lumpClass == mlThings)
            {
                if(mlver == 1)  // DOOM Format
                {
                    *mlptr = 10;
                    stiptr->verInfo[index].numValues = 5;
                    stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 5, PU_STATIC, 0);
                    // x
                    stiptr->verInfo[index].values[0].property = DAM_THING_X;
                    stiptr->verInfo[index].values[0].flags = 0;
                    stiptr->verInfo[index].values[0].size =  2;
                    stiptr->verInfo[index].values[0].offset = 0;
                    stiptr->verInfo[index].values[0].gameprop = 1;
                    // y
                    stiptr->verInfo[index].values[1].property = DAM_THING_Y;
                    stiptr->verInfo[index].values[1].flags = 0;
                    stiptr->verInfo[index].values[1].size =  2;
                    stiptr->verInfo[index].values[1].offset = 2;
                    stiptr->verInfo[index].values[1].gameprop = 1;
                    // angle
                    stiptr->verInfo[index].values[2].property = DAM_THING_ANGLE;
                    stiptr->verInfo[index].values[2].flags = 0;
                    stiptr->verInfo[index].values[2].size =  2;
                    stiptr->verInfo[index].values[2].offset = 4;
                    stiptr->verInfo[index].values[2].gameprop = 1;
                    // type
                    stiptr->verInfo[index].values[3].property = DAM_THING_TYPE;
                    stiptr->verInfo[index].values[3].flags = 0;
                    stiptr->verInfo[index].values[3].size =  2;
                    stiptr->verInfo[index].values[3].offset = 6;
                    stiptr->verInfo[index].values[3].gameprop = 1;
                    // options
                    stiptr->verInfo[index].values[4].property = DAM_THING_OPTIONS;
                    stiptr->verInfo[index].values[4].flags = 0;
                    stiptr->verInfo[index].values[4].size =  2;
                    stiptr->verInfo[index].values[4].offset = 8;
                    stiptr->verInfo[index].values[4].gameprop = 1;
                }
                else            // HEXEN format
                {
                    *mlptr = 20;
                    stiptr->verInfo[index].numValues = 13;
                    stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 13, PU_STATIC, 0);
                    // tid
                    stiptr->verInfo[index].values[0].property = DAM_THING_TID;
                    stiptr->verInfo[index].values[0].flags = 0;
                    stiptr->verInfo[index].values[0].size =  2;
                    stiptr->verInfo[index].values[0].offset = 0;
                    stiptr->verInfo[index].values[0].gameprop = 1;
                    // x
                    stiptr->verInfo[index].values[1].property = DAM_THING_X;
                    stiptr->verInfo[index].values[1].flags = 0;
                    stiptr->verInfo[index].values[1].size =  2;
                    stiptr->verInfo[index].values[1].offset = 2;
                    stiptr->verInfo[index].values[1].gameprop = 1;
                    // y
                    stiptr->verInfo[index].values[2].property = DAM_THING_Y;
                    stiptr->verInfo[index].values[2].flags = 0;
                    stiptr->verInfo[index].values[2].size =  2;
                    stiptr->verInfo[index].values[2].offset = 4;
                    stiptr->verInfo[index].values[2].gameprop = 1;
                    // height
                    stiptr->verInfo[index].values[3].property = DAM_THING_HEIGHT;
                    stiptr->verInfo[index].values[3].flags = 0;
                    stiptr->verInfo[index].values[3].size =  2;
                    stiptr->verInfo[index].values[3].offset = 6;
                    stiptr->verInfo[index].values[3].gameprop = 1;
                    // angle
                    stiptr->verInfo[index].values[4].property = DAM_THING_ANGLE;
                    stiptr->verInfo[index].values[4].flags = 0;
                    stiptr->verInfo[index].values[4].size =  2;
                    stiptr->verInfo[index].values[4].offset = 8;
                    stiptr->verInfo[index].values[4].gameprop = 1;
                    // type
                    stiptr->verInfo[index].values[5].property = DAM_THING_TYPE;
                    stiptr->verInfo[index].values[5].flags = 0;
                    stiptr->verInfo[index].values[5].size =  2;
                    stiptr->verInfo[index].values[5].offset = 10;
                    stiptr->verInfo[index].values[5].gameprop = 1;
                    // options
                    stiptr->verInfo[index].values[6].property = DAM_THING_OPTIONS;
                    stiptr->verInfo[index].values[6].flags = 0;
                    stiptr->verInfo[index].values[6].size =  2;
                    stiptr->verInfo[index].values[6].offset = 12;
                    stiptr->verInfo[index].values[6].gameprop = 1;
                    // special
                    stiptr->verInfo[index].values[7].property = DAM_THING_SPECIAL;
                    stiptr->verInfo[index].values[7].flags = 0;
                    stiptr->verInfo[index].values[7].size =  1;
                    stiptr->verInfo[index].values[7].offset = 14;
                    stiptr->verInfo[index].values[7].gameprop = 1;
                    // arg1
                    stiptr->verInfo[index].values[8].property = DAM_THING_ARG1;
                    stiptr->verInfo[index].values[8].flags = 0;
                    stiptr->verInfo[index].values[8].size =  1;
                    stiptr->verInfo[index].values[8].offset = 15;
                    stiptr->verInfo[index].values[8].gameprop = 1;
                    // arg2
                    stiptr->verInfo[index].values[9].property = DAM_THING_ARG2;
                    stiptr->verInfo[index].values[9].flags = 0;
                    stiptr->verInfo[index].values[9].size =  1;
                    stiptr->verInfo[index].values[9].offset = 16;
                    stiptr->verInfo[index].values[9].gameprop = 1;
                    // arg3
                    stiptr->verInfo[index].values[10].property = DAM_THING_ARG3;
                    stiptr->verInfo[index].values[10].flags = 0;
                    stiptr->verInfo[index].values[10].size =  1;
                    stiptr->verInfo[index].values[10].offset = 17;
                    stiptr->verInfo[index].values[10].gameprop = 1;
                    // arg4
                    stiptr->verInfo[index].values[11].property = DAM_THING_ARG4;
                    stiptr->verInfo[index].values[11].flags = 0;
                    stiptr->verInfo[index].values[11].size =  1;
                    stiptr->verInfo[index].values[11].offset = 18;
                    stiptr->verInfo[index].values[11].gameprop = 1;
                    // arg5
                    stiptr->verInfo[index].values[12].property = DAM_THING_ARG5;
                    stiptr->verInfo[index].values[12].flags = 0;
                    stiptr->verInfo[index].values[12].size =  1;
                    stiptr->verInfo[index].values[12].offset = 19;
                    stiptr->verInfo[index].values[12].gameprop = 1;
                }
            }
            else if(lumpClass == mlLineDefs)
            {
                if(mlver == 1)  // DOOM format
                    *mlptr = sizeof(maplinedef_t);
                else            // HEXEN format
                    *mlptr = sizeof(maplinedefhex_t);
            }
            else if(lumpClass == mlSideDefs)
            {
                *mlptr = sizeof(mapsidedef_t);
            }
            else if(lumpClass == mlVertexes)
            {
                *mlptr = 4;
                stiptr->verInfo[index].numValues = 2;
                stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);
                // x
                stiptr->verInfo[index].values[0].property = DAM_X;
                stiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[0].size =  2;
                stiptr->verInfo[index].values[0].offset = 0;
                stiptr->verInfo[index].values[0].gameprop = 0;
                // y
                stiptr->verInfo[index].values[1].property = DAM_Y;
                stiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[1].size =  2;
                stiptr->verInfo[index].values[1].offset = 2;
                stiptr->verInfo[index].values[1].gameprop = 0;
            }
            else if(lumpClass == mlSegs)
            {
                *mlptr = 12;
                stiptr->verInfo[index].numValues = 6;
                stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 6, PU_STATIC, 0);
                // v1
                stiptr->verInfo[index].values[0].property = DAM_VERTEX1;
                stiptr->verInfo[index].values[0].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[0].size =  2;
                stiptr->verInfo[index].values[0].offset = 0;
                stiptr->verInfo[index].values[0].gameprop = 0;
                // v2
                stiptr->verInfo[index].values[1].property = DAM_VERTEX2;
                stiptr->verInfo[index].values[1].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[1].size =  2;
                stiptr->verInfo[index].values[1].offset = 2;
                stiptr->verInfo[index].values[1].gameprop = 0;
                // angle
                stiptr->verInfo[index].values[2].property = DAM_ANGLE;
                stiptr->verInfo[index].values[2].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[2].size =  2;
                stiptr->verInfo[index].values[2].offset = 4;
                stiptr->verInfo[index].values[2].gameprop = 0;
                // linedef
                stiptr->verInfo[index].values[3].property = DAM_LINE;
                stiptr->verInfo[index].values[3].flags = 0;
                stiptr->verInfo[index].values[3].size =  2;
                stiptr->verInfo[index].values[3].offset = 6;
                stiptr->verInfo[index].values[3].gameprop = 0;
                // side
                stiptr->verInfo[index].values[4].property = DAM_SIDE;
                stiptr->verInfo[index].values[4].flags = DT_NOINDEX;
                stiptr->verInfo[index].values[4].size =  2;
                stiptr->verInfo[index].values[4].offset = 8;
                stiptr->verInfo[index].values[4].gameprop = 0;
                // offset
                stiptr->verInfo[index].values[5].property = DAM_OFFSET;
                stiptr->verInfo[index].values[5].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[5].size =  2;
                stiptr->verInfo[index].values[5].offset = 10;
                stiptr->verInfo[index].values[5].gameprop = 0;
            }
            else if(lumpClass == mlSSectors)
            {
                *mlptr = 4;
                stiptr->verInfo[index].numValues = 2;
                stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);

                stiptr->verInfo[index].values[0].property = DAM_LINE_COUNT;
                stiptr->verInfo[index].values[0].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[0].size =  2;
                stiptr->verInfo[index].values[0].offset = 0;
                stiptr->verInfo[index].values[0].gameprop = 0;

                stiptr->verInfo[index].values[1].property = DAM_LINE_FIRST;
                stiptr->verInfo[index].values[1].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[1].size =  2;
                stiptr->verInfo[index].values[1].offset = 2;
                stiptr->verInfo[index].values[1].gameprop = 0;
            }
            else if(lumpClass == mlNodes)
            {
                *mlptr = 28;
                stiptr->verInfo[index].numValues = 14;
                stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 14, PU_STATIC, 0);
                // x
                stiptr->verInfo[index].values[0].property = DAM_X;
                stiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[0].size =  2;
                stiptr->verInfo[index].values[0].offset = 0;
                stiptr->verInfo[index].values[0].gameprop = 0;
                // y
                stiptr->verInfo[index].values[1].property = DAM_Y;
                stiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[1].size =  2;
                stiptr->verInfo[index].values[1].offset = 2;
                stiptr->verInfo[index].values[1].gameprop = 0;
                // dx
                stiptr->verInfo[index].values[2].property = DAM_DX;
                stiptr->verInfo[index].values[2].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[2].size =  2;
                stiptr->verInfo[index].values[2].offset = 4;
                stiptr->verInfo[index].values[2].gameprop = 0;
                // dy
                stiptr->verInfo[index].values[3].property = DAM_DY;
                stiptr->verInfo[index].values[3].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[3].size =  2;
                stiptr->verInfo[index].values[3].offset = 6;
                stiptr->verInfo[index].values[3].gameprop = 0;
                // bbox[0][0]
                stiptr->verInfo[index].values[4].property = DAM_BBOX_RIGHT_TOP_Y;
                stiptr->verInfo[index].values[4].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[4].size =  2;
                stiptr->verInfo[index].values[4].offset = 8;
                stiptr->verInfo[index].values[4].gameprop = 0;
                // bbox[0][1]
                stiptr->verInfo[index].values[5].property = DAM_BBOX_RIGHT_LOW_Y;
                stiptr->verInfo[index].values[5].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[5].size =  2;
                stiptr->verInfo[index].values[5].offset = 10;
                stiptr->verInfo[index].values[5].gameprop = 0;
                // bbox[0][2]
                stiptr->verInfo[index].values[6].property = DAM_BBOX_RIGHT_LOW_X;
                stiptr->verInfo[index].values[6].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[6].size =  2;
                stiptr->verInfo[index].values[6].offset = 12;
                stiptr->verInfo[index].values[6].gameprop = 0;
                // bbox[0][3]
                stiptr->verInfo[index].values[7].property = DAM_BBOX_RIGHT_TOP_X;
                stiptr->verInfo[index].values[7].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[7].size =  2;
                stiptr->verInfo[index].values[7].offset = 14;
                stiptr->verInfo[index].values[7].gameprop = 0;
                // bbox[1][0]
                stiptr->verInfo[index].values[8].property = DAM_BBOX_LEFT_TOP_Y;
                stiptr->verInfo[index].values[8].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[8].size =  2;
                stiptr->verInfo[index].values[8].offset = 16;
                stiptr->verInfo[index].values[8].gameprop = 0;
                // bbox[1][1]
                stiptr->verInfo[index].values[9].property = DAM_BBOX_LEFT_LOW_Y;
                stiptr->verInfo[index].values[9].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[9].size =  2;
                stiptr->verInfo[index].values[9].offset = 18;
                stiptr->verInfo[index].values[9].gameprop = 0;
                // bbox[1][2]
                stiptr->verInfo[index].values[10].property = DAM_BBOX_LEFT_LOW_X;
                stiptr->verInfo[index].values[10].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[10].size =  2;
                stiptr->verInfo[index].values[10].offset = 20;
                stiptr->verInfo[index].values[10].gameprop = 0;
                // bbox[1][3]
                stiptr->verInfo[index].values[11].property = DAM_BBOX_LEFT_TOP_X;
                stiptr->verInfo[index].values[11].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[11].size =  2;
                stiptr->verInfo[index].values[11].offset = 22;
                stiptr->verInfo[index].values[11].gameprop = 0;
                // children[0]
                stiptr->verInfo[index].values[12].property = DAM_CHILD_RIGHT;
                stiptr->verInfo[index].values[12].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[12].size =  2;
                stiptr->verInfo[index].values[12].offset = 24;
                stiptr->verInfo[index].values[12].gameprop = 0;
                // children[1]
                stiptr->verInfo[index].values[13].property = DAM_CHILD_LEFT;
                stiptr->verInfo[index].values[13].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[13].size =  2;
                stiptr->verInfo[index].values[13].offset = 26;
                stiptr->verInfo[index].values[13].gameprop = 0;
            }
            else if(lumpClass == mlSectors)
            {
                *mlptr = 26;
                stiptr->verInfo[index].numValues = 7;
                stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 7, PU_STATIC, 0);
                // floor height
                stiptr->verInfo[index].values[0].property = DAM_FLOOR_HEIGHT;
                stiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[0].size =  2;
                stiptr->verInfo[index].values[0].offset = 0;
                stiptr->verInfo[index].values[0].gameprop = 0;
                // ceiling height
                stiptr->verInfo[index].values[1].property = DAM_CEILING_HEIGHT;
                stiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[1].size =  2;
                stiptr->verInfo[index].values[1].offset = 2;
                stiptr->verInfo[index].values[1].gameprop = 0;
                // floor pic
                stiptr->verInfo[index].values[2].property = DAM_FLOOR_TEXTURE;
                stiptr->verInfo[index].values[2].flags = DT_FLAT;
                stiptr->verInfo[index].values[2].size =  8;
                stiptr->verInfo[index].values[2].offset = 4;
                stiptr->verInfo[index].values[2].gameprop = 0;
                // ceiling pic
                stiptr->verInfo[index].values[3].property = DAM_CEILING_TEXTURE;
                stiptr->verInfo[index].values[3].flags = DT_FLAT;
                stiptr->verInfo[index].values[3].size =  8;
                stiptr->verInfo[index].values[3].offset = 12;
                stiptr->verInfo[index].values[3].gameprop = 0;
                // light level
                stiptr->verInfo[index].values[4].property = DAM_LIGHT_LEVEL;
                stiptr->verInfo[index].values[4].flags = 0;
                stiptr->verInfo[index].values[4].size =  2;
                stiptr->verInfo[index].values[4].offset = 20;
                stiptr->verInfo[index].values[4].gameprop = 0;
                // special
                stiptr->verInfo[index].values[5].property = DAM_SECTOR_SPECIAL;
                stiptr->verInfo[index].values[5].flags = 0;
                stiptr->verInfo[index].values[5].size =  2;
                stiptr->verInfo[index].values[5].offset = 22;
                stiptr->verInfo[index].values[5].gameprop = 1;
                // tag
                stiptr->verInfo[index].values[6].property = DAM_SECTOR_TAG;
                stiptr->verInfo[index].values[6].flags = 0;
                stiptr->verInfo[index].values[6].size =  2;
                stiptr->verInfo[index].values[6].offset = 24;
                stiptr->verInfo[index].values[6].gameprop = 1;
            }
            else if(lumpClass == mlReject)
            {
                *mlptr = 0;
            }
            else if(lumpClass == mlBlockMap)
            {
                *mlptr = 0;
            }
        }
    }

    // Calculate the size of the gl node structs
    for(i = GLNODE_FORMATS -1; i >= 0; --i)
    {
        for(j = 0; j < NUM_LUMPCLASSES; ++j)
        {
            lumpClass = LumpInfo[j].lumpclass;
            index = LumpInfo[j].glLump;

            glver = (glNodeVer[i].verInfo[index].version);
            glptr = &(glNodeVer[i].verInfo[index].elmSize);
            glstiptr = &(glNodeVer[i]);

            if(lumpClass == glVerts)
            {
                if(glver == 1)
                {
                    *glptr = 4;
                    glstiptr->verInfo[index].numValues = 2;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);
                    // x
                    glstiptr->verInfo[index].values[0].property = DAM_X;
                    glstiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[0].size =  2;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    glstiptr->verInfo[index].values[0].gameprop = 0;
                    // y
                    glstiptr->verInfo[index].values[1].property = DAM_Y;
                    glstiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[1].size =  2;
                    glstiptr->verInfo[index].values[1].offset = 2;
                    glstiptr->verInfo[index].values[1].gameprop = 0;
                }
                else
                {
                    *glptr = 8;
                    glstiptr->verInfo[index].numValues = 2;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);
                    // x
                    glstiptr->verInfo[index].values[0].property = DAM_X;
                    glstiptr->verInfo[index].values[0].flags = 0;
                    glstiptr->verInfo[index].values[0].size =  4;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    glstiptr->verInfo[index].values[0].gameprop = 0;
                    // y
                    glstiptr->verInfo[index].values[1].property = DAM_Y;
                    glstiptr->verInfo[index].values[1].flags = 0;
                    glstiptr->verInfo[index].values[1].size =  4;
                    glstiptr->verInfo[index].values[1].offset = 4;
                    glstiptr->verInfo[index].values[1].gameprop = 0;
                }
            }
            else if(lumpClass == glSegs)
            {
                if(glver == 1)
                {
                    *glptr = 10;
                    glstiptr->verInfo[index].numValues = 4;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 4, PU_STATIC, 0);
                    // v1
                    glstiptr->verInfo[index].values[0].property = DAM_VERTEX1;
                    glstiptr->verInfo[index].values[0].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[0].size =  2;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    glstiptr->verInfo[index].values[0].gameprop = 0;
                    // v2
                    glstiptr->verInfo[index].values[1].property = DAM_VERTEX2;
                    glstiptr->verInfo[index].values[1].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[1].size =  2;
                    glstiptr->verInfo[index].values[1].offset = 2;
                    glstiptr->verInfo[index].values[1].gameprop = 0;
                    // linedef
                    glstiptr->verInfo[index].values[2].property = DAM_LINE;
                    glstiptr->verInfo[index].values[2].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[2].size =  2;
                    glstiptr->verInfo[index].values[2].offset = 4;
                    glstiptr->verInfo[index].values[2].gameprop = 0;
                    // side
                    glstiptr->verInfo[index].values[3].property = DAM_SIDE;
                    glstiptr->verInfo[index].values[3].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[3].size =  2;
                    glstiptr->verInfo[index].values[3].offset = 6;
                    glstiptr->verInfo[index].values[3].gameprop = 0;
                }
                else if(glver == 4)
                {
                    *glptr = 0;        // Unsupported atm
                }
                else // Ver 3/5
                {
                    *glptr = 14;
                    glstiptr->verInfo[index].numValues = 4;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 4, PU_STATIC, 0);
                    // v1
                    glstiptr->verInfo[index].values[0].property = DAM_VERTEX1;
                    glstiptr->verInfo[index].values[0].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[0].size =  4;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    glstiptr->verInfo[index].values[0].gameprop = 0;
                    // v2
                    glstiptr->verInfo[index].values[1].property = DAM_VERTEX2;
                    glstiptr->verInfo[index].values[1].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[1].size =  4;
                    glstiptr->verInfo[index].values[1].offset = 4;
                    glstiptr->verInfo[index].values[1].gameprop = 0;
                    // linedef
                    glstiptr->verInfo[index].values[2].property = DAM_LINE;
                    glstiptr->verInfo[index].values[2].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[2].size =  2;
                    glstiptr->verInfo[index].values[2].offset = 8;
                    glstiptr->verInfo[index].values[2].gameprop = 0;
                    // side
                    glstiptr->verInfo[index].values[3].property = DAM_SIDE;
                    glstiptr->verInfo[index].values[3].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[3].size =  2;
                    glstiptr->verInfo[index].values[3].offset = 10;
                    glstiptr->verInfo[index].values[3].gameprop = 0;
                }
            }
            else if(lumpClass == glSSects)
            {
                if(glver == 1)
                {
                    *glptr = 4;
                    glstiptr->verInfo[index].numValues = 2;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);

                    glstiptr->verInfo[index].values[0].property = DAM_LINE_COUNT;
                    glstiptr->verInfo[index].values[0].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[0].size =  2;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    glstiptr->verInfo[index].values[0].gameprop = 0;

                    glstiptr->verInfo[index].values[1].property = DAM_LINE_FIRST;
                    glstiptr->verInfo[index].values[1].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[1].size =  2;
                    glstiptr->verInfo[index].values[1].offset = 2;
                    glstiptr->verInfo[index].values[1].gameprop = 0;
                }
                else
                {
                    *glptr = 8;
                    glstiptr->verInfo[index].numValues = 2;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);

                    glstiptr->verInfo[index].values[0].property = DAM_LINE_COUNT;
                    glstiptr->verInfo[index].values[0].flags = 0;
                    glstiptr->verInfo[index].values[0].size =  4;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    glstiptr->verInfo[index].values[0].gameprop = 0;

                    glstiptr->verInfo[index].values[1].property = DAM_LINE_FIRST;
                    glstiptr->verInfo[index].values[1].flags = 0;
                    glstiptr->verInfo[index].values[1].size =  4;
                    glstiptr->verInfo[index].values[1].offset = 4;
                    glstiptr->verInfo[index].values[1].gameprop = 0;
                }
            }
            else if(lumpClass == glNodes)
            {
                if(glver == 1)
                {
                    *glptr = 28;
                    glstiptr->verInfo[index].numValues = 14;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 14, PU_STATIC, 0);
                    // x
                    glstiptr->verInfo[index].values[0].property = DAM_X;
                    glstiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[0].size =  2;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    glstiptr->verInfo[index].values[0].gameprop = 0;
                    // y
                    glstiptr->verInfo[index].values[1].property = DAM_Y;
                    glstiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[1].size =  2;
                    glstiptr->verInfo[index].values[1].offset = 2;
                    glstiptr->verInfo[index].values[1].gameprop = 0;
                    // dx
                    glstiptr->verInfo[index].values[2].property = DAM_DX;
                    glstiptr->verInfo[index].values[2].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[2].size =  2;
                    glstiptr->verInfo[index].values[2].offset = 4;
                    glstiptr->verInfo[index].values[2].gameprop = 0;
                    // dy
                    glstiptr->verInfo[index].values[3].property = DAM_DY;
                    glstiptr->verInfo[index].values[3].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[3].size =  2;
                    glstiptr->verInfo[index].values[3].offset = 6;
                    glstiptr->verInfo[index].values[3].gameprop = 0;
                    // bbox[0][0]
                    glstiptr->verInfo[index].values[4].property = DAM_BBOX_RIGHT_TOP_Y;
                    glstiptr->verInfo[index].values[4].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[4].size =  2;
                    glstiptr->verInfo[index].values[4].offset = 8;
                    glstiptr->verInfo[index].values[4].gameprop = 0;
                    // bbox[0][1]
                    glstiptr->verInfo[index].values[5].property = DAM_BBOX_RIGHT_LOW_Y;
                    glstiptr->verInfo[index].values[5].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[5].size =  2;
                    glstiptr->verInfo[index].values[5].offset = 10;
                    glstiptr->verInfo[index].values[5].gameprop = 0;
                    // bbox[0][2]
                    glstiptr->verInfo[index].values[6].property = DAM_BBOX_RIGHT_LOW_X;
                    glstiptr->verInfo[index].values[6].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[6].size =  2;
                    glstiptr->verInfo[index].values[6].offset = 12;
                    glstiptr->verInfo[index].values[6].gameprop = 0;
                    // bbox[0][3]
                    glstiptr->verInfo[index].values[7].property = DAM_BBOX_RIGHT_TOP_X;
                    glstiptr->verInfo[index].values[7].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[7].size =  2;
                    glstiptr->verInfo[index].values[7].offset = 14;
                    glstiptr->verInfo[index].values[7].gameprop = 0;
                    // bbox[1][0]
                    glstiptr->verInfo[index].values[8].property = DAM_BBOX_LEFT_TOP_Y;
                    glstiptr->verInfo[index].values[8].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[8].size =  2;
                    glstiptr->verInfo[index].values[8].offset = 16;
                    glstiptr->verInfo[index].values[8].gameprop = 0;
                    // bbox[1][1]
                    glstiptr->verInfo[index].values[9].property = DAM_BBOX_LEFT_LOW_Y;
                    glstiptr->verInfo[index].values[9].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[9].size =  2;
                    glstiptr->verInfo[index].values[9].offset = 18;
                    glstiptr->verInfo[index].values[9].gameprop = 0;
                    // bbox[1][2]
                    glstiptr->verInfo[index].values[10].property = DAM_BBOX_LEFT_LOW_X;
                    glstiptr->verInfo[index].values[10].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[10].size =  2;
                    glstiptr->verInfo[index].values[10].offset = 20;
                    glstiptr->verInfo[index].values[10].gameprop = 0;
                    // bbox[1][3]
                    glstiptr->verInfo[index].values[11].property = DAM_BBOX_LEFT_TOP_X;
                    glstiptr->verInfo[index].values[11].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[11].size =  2;
                    glstiptr->verInfo[index].values[11].offset = 22;
                    glstiptr->verInfo[index].values[11].gameprop = 0;
                    // children[0]
                    glstiptr->verInfo[index].values[12].property = DAM_CHILD_RIGHT;
                    glstiptr->verInfo[index].values[12].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[12].size =  2;
                    glstiptr->verInfo[index].values[12].offset = 24;
                    glstiptr->verInfo[index].values[12].gameprop = 0;
                    // children[1]
                    glstiptr->verInfo[index].values[13].property = DAM_CHILD_LEFT;
                    glstiptr->verInfo[index].values[13].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[13].size =  2;
                    glstiptr->verInfo[index].values[13].offset = 26;
                    glstiptr->verInfo[index].values[13].gameprop = 0;
                }
                else
                {
                    *glptr = 32;
                    glstiptr->verInfo[index].numValues = 14;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 14, PU_STATIC, 0);
                    // x
                    glstiptr->verInfo[index].values[0].property = DAM_X;
                    glstiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[0].size =  2;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    glstiptr->verInfo[index].values[0].gameprop = 0;
                    // y
                    glstiptr->verInfo[index].values[1].property = DAM_Y;
                    glstiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[1].size =  2;
                    glstiptr->verInfo[index].values[1].offset = 2;
                    glstiptr->verInfo[index].values[1].gameprop = 0;
                    // dx
                    glstiptr->verInfo[index].values[2].property = DAM_DX;
                    glstiptr->verInfo[index].values[2].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[2].size =  2;
                    glstiptr->verInfo[index].values[2].offset = 4;
                    glstiptr->verInfo[index].values[2].gameprop = 0;
                    // dy
                    glstiptr->verInfo[index].values[3].property = DAM_DY;
                    glstiptr->verInfo[index].values[3].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[3].size =  2;
                    glstiptr->verInfo[index].values[3].offset = 6;
                    glstiptr->verInfo[index].values[3].gameprop = 0;
                    // bbox[0][0]
                    glstiptr->verInfo[index].values[4].property = DAM_BBOX_RIGHT_TOP_Y;
                    glstiptr->verInfo[index].values[4].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[4].size =  2;
                    glstiptr->verInfo[index].values[4].offset = 8;
                    glstiptr->verInfo[index].values[4].gameprop = 0;
                    // bbox[0][1]
                    glstiptr->verInfo[index].values[5].property = DAM_BBOX_RIGHT_LOW_Y;
                    glstiptr->verInfo[index].values[5].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[5].size =  2;
                    glstiptr->verInfo[index].values[5].offset = 10;
                    glstiptr->verInfo[index].values[5].gameprop = 0;
                    // bbox[0][2]
                    glstiptr->verInfo[index].values[6].property = DAM_BBOX_RIGHT_LOW_X;
                    glstiptr->verInfo[index].values[6].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[6].size =  2;
                    glstiptr->verInfo[index].values[6].offset = 12;
                    glstiptr->verInfo[index].values[6].gameprop = 0;
                    // bbox[0][3]
                    glstiptr->verInfo[index].values[7].property = DAM_BBOX_RIGHT_TOP_X;
                    glstiptr->verInfo[index].values[7].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[7].size =  2;
                    glstiptr->verInfo[index].values[7].offset = 14;
                    glstiptr->verInfo[index].values[7].gameprop = 0;
                    // bbox[1][0]
                    glstiptr->verInfo[index].values[8].property = DAM_BBOX_LEFT_TOP_Y;
                    glstiptr->verInfo[index].values[8].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[8].size =  2;
                    glstiptr->verInfo[index].values[8].offset = 16;
                    glstiptr->verInfo[index].values[8].gameprop = 0;
                    // bbox[1][1]
                    glstiptr->verInfo[index].values[9].property = DAM_BBOX_LEFT_LOW_Y;
                    glstiptr->verInfo[index].values[9].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[9].size =  2;
                    glstiptr->verInfo[index].values[9].offset = 18;
                    glstiptr->verInfo[index].values[9].gameprop = 0;
                    // bbox[1][2]
                    glstiptr->verInfo[index].values[10].property = DAM_BBOX_LEFT_LOW_X;
                    glstiptr->verInfo[index].values[10].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[10].size =  2;
                    glstiptr->verInfo[index].values[10].offset = 20;
                    glstiptr->verInfo[index].values[10].gameprop = 0;
                    // bbox[1][3]
                    glstiptr->verInfo[index].values[11].property = DAM_BBOX_LEFT_TOP_X;
                    glstiptr->verInfo[index].values[11].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[11].size =  2;
                    glstiptr->verInfo[index].values[11].offset = 22;
                    glstiptr->verInfo[index].values[11].gameprop = 0;
                    // children[0]
                    glstiptr->verInfo[index].values[12].property = 12;
                    glstiptr->verInfo[index].values[12].flags = DAM_CHILD_RIGHT;
                    glstiptr->verInfo[index].values[12].size =  4;
                    glstiptr->verInfo[index].values[12].offset = 24;
                    glstiptr->verInfo[index].values[12].gameprop = 0;
                    // children[1]
                    glstiptr->verInfo[index].values[13].property = DAM_CHILD_LEFT;
                    glstiptr->verInfo[index].values[13].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[13].size =  4;
                    glstiptr->verInfo[index].values[13].offset = 28;
                    glstiptr->verInfo[index].values[13].gameprop = 0;
                }
            }
        }
    }
}

float AccurateDistance(fixed_t dx, fixed_t dy)
{
    float   fx = FIX2FLT(dx), fy = FIX2FLT(dy);

    return (float) sqrt(fx * fx + fy * fy);
}
