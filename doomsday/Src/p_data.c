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

typedef enum {
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

typedef struct {
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

typedef struct setargs_s {
    int type;
    int prop;
    valuetype_t valueType;
    boolean* booleanValues;
    byte* byteValues;
    int* intValues;
    fixed_t* fixedValues;
    float* floatValues;
    angle_t* angleValues;
    void** ptrValues;
} setargs_t;

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
 * Not all DMU constants have a value type, in which caase
 * they should be entered using the special case: "DDVT_NONE"
 */
const valuetype_t propertyTypes[] = {
        { DDVT_NONE },        // DMU_ALL = -1

        { DDVT_PTR },         // DMU_VERTEX = 1
        { DDVT_PTR },         // DMU_SEG
        { DDVT_PTR },         // DMU_LINE
        { DDVT_PTR },         // DMU_SIDE
        { DDVT_PTR },         // DMU_NODE
        { DDVT_PTR },         // DMU_SUBSECTOR
        { DDVT_PTR },         // DMU_SECTOR
        { DDVT_NONE },        // DMU_BLOCKMAP
        { DDVT_NONE },        // DMU_REJEC
        { DDVT_NONE },        // DMU_POLYBLOCKMAP
        { DDVT_PTR },         // DMU_POLYOBJ

        { DDVT_NONE },        // DMU_LINE_BY_TAG
        { DDVT_NONE },        // DMU_SECTOR_BY_TAG

        { DDVT_NONE },        // DMU_LINE_BY_ACT_TAG
        { DDVT_NONE },        // DMU_SECTOR_BY_ACT_TAG

        { DDVT_NONE },        // DMU_LINE_OF_SECTOR
        { DDVT_NONE },        // DMU_SECTOR_OF_SUBSECTOR
        { DDVT_NONE },        // DMU_SEG_OF_POLYOBJ

        { DDVT_FIXED },       // DMU_X
        { DDVT_FIXED },       // DMU_Y
        { DDVT_FIXED },       // DMU_XY

        { DDVT_PTR },         // DMU_VERTEX1
        { DDVT_PTR },         // DMU_VERTEX2
        { DDVT_FIXED },       // DMU_VERTEX1_X
        { DDVT_FIXED },       // DMU_VERTEX1_Y
        { DDVT_FIXED },       // DMU_VERTEX1_XY
        { DDVT_FIXED },       // DMU_VERTEX2_X
        { DDVT_FIXED },       // DMU_VERTEX2_Y
        { DDVT_FIXED },       // DMU_VERTEX2_XY

        { DDVT_PTR },         // DMU_FRONT_SECTOR
        { DDVT_PTR },         // DMU_BACK_SECTOR
        { DDVT_PTR },         // DMU_SIDE0
        { DDVT_PTR },         // DMU_SIDE1
        { DDVT_INT },         // DMU_FLAGS
        { DDVT_FIXED },       // DMU_DX
        { DDVT_FIXED },       // DMU_DY
        { DDVT_FIXED },       // DMU_LENGTH
        { DDVT_INT },         // DMU_SLOPE_TYPE
        { DDVT_ANGLE },       // DMU_ANGLE
        { DDVT_FIXED },       // DMU_OFFSET
        { DDVT_FLAT_INDEX },  // DMU_TOP_TEXTURE
        { DDVT_BYTE },        // DMU_TOP_COLOR
        { DDVT_BYTE },        // DMU_TOP_COLOR_RED
        { DDVT_BYTE },        // DMU_TOP_COLOR_GREEN
        { DDVT_BYTE },        // DMU_TOP_COLOR_BLUE
        { DDVT_FLAT_INDEX },  // DMU_MIDDLE_TEXTURE
        { DDVT_BYTE },        // DMU_MIDDLE_COLOR
        { DDVT_BYTE },        // DMU_MIDDLE_COLOR_RED
        { DDVT_BYTE },        // DMU_MIDDLE_COLOR_GREEN
        { DDVT_BYTE },        // DMU_MIDDLE_COLOR_BLUE
        { DDVT_BYTE },        // DMU_MIDDLE_COLOR_ALPHA
        { DDVT_BLENDMODE },   // DMU_MIDDLE_BLENDMODE
        { DDVT_FLAT_INDEX },  // DMU_BOTTOM_TEXTURE
        { DDVT_BYTE },        // DMU_BOTTOM_COLOR
        { DDVT_BYTE },        // DMU_BOTTOM_COLOR_RED
        { DDVT_BYTE },        // DMU_BOTTOM_COLOR_GREEN
        { DDVT_BYTE },        // DMU_BOTTOM_COLOR_BLUE
        { DDVT_FIXED },       // DMU_TEXTURE_OFFSET_X
        { DDVT_FIXED },       // DMU_TEXTURE_OFFSET_Y
        { DDVT_FIXED },       // DMU_TEXTURE_OFFSET_XY
        { DDVT_INT },         // DMU_VALID_COUNT

        { DDVT_INT },         // DMU_LINE_COUNT
        { DDVT_BYTE },        // DMU_COLOR
        { DDVT_BYTE },        // DMU_COLOR_RED
        { DDVT_BYTE },        // DMU_COLOR_GREEN
        { DDVT_BYTE },        // DMU_COLOR_BLUE
        { DDVT_SHORT },       // DMU_LIGHT_LEVEL
        { DDVT_NONE },        // DMU_THINGS
        { DDVT_FIXED },       // DMU_BOUNDING_BOX
        { DDVT_PTR },         // DMU_SOUND_ORIGIN

        { DDVT_FIXED },       // DMU_FLOOR_HEIGHT
        { DDVT_FLAT_INDEX },  // DMU_FLOOR_TEXTURE
        { DDVT_FLOAT },       // DMU_FLOOR_OFFSET_X
        { DDVT_FLOAT },       // DMU_FLOOR_OFFSET_Y
        { DDVT_FLOAT },       // DMU_FLOOR_OFFSET_XY
        { DDVT_INT },         // DMU_FLOOR_TARGET
        { DDVT_INT },         // DMU_FLOOR_SPEED
        { DDVT_BYTE },        // DMU_FLOOR_COLOR
        { DDVT_BYTE },        // DMU_FLOOR_COLOR_RED
        { DDVT_BYTE },        // DMU_FLOOR_COLOR_GREEN
        { DDVT_BYTE },        // DMU_FLOOR_COLOR_BLUE
        { DDVT_INT },         // DMU_FLOOR_TEXTURE_MOVE_X
        { DDVT_INT },         // DMU_FLOOR_TEXTURE_MOVE_Y
        { DDVT_INT },         // DMU_FLOOR_TEXTURE_MOVE_XY

        { DDVT_FIXED },       // DMU_CEILING_HEIGHT
        { DDVT_FLAT_INDEX },  // DMU_CEILING_TEXTURE
        { DDVT_FLOAT },       // DMU_CEILING_OFFSET_X
        { DDVT_FLOAT },       // DMU_CEILING_OFFSET_Y
        { DDVT_FLOAT },       // DMU_CEILING_OFFSET_XY
        { DDVT_INT },         // DMU_CEILING_TARGET
        { DDVT_INT },         // DMU_CEILING_SPEED
        { DDVT_BYTE },        // DMU_CEILING_COLOR
        { DDVT_BYTE },        // DMU_CEILING_COLOR_RED
        { DDVT_BYTE },        // DMU_CEILING_COLOR_GREEN
        { DDVT_BYTE },        // DMU_CEILING_COLOR_BLUE
        { DDVT_INT },         // DMU_CEILING_TEXTURE_MOVE_X
        { DDVT_INT },         // DMU_CEILING_TEXTURE_MOVE_Y
        { DDVT_INT },         // DMU_CEILING_TEXTURE_MOVE_XY

        { DDVT_NONE },        // DMU_SEG_LIST
        { DDVT_INT },         // DMU_SEG_COUNT
        { DDVT_INT },         // DMU_TAG
        { DDVT_PTR },         // DMU_ORIGINAL_POINTS
        { DDVT_PTR },         // DMU_PREVIOUS_POINTS
        { DDVT_PTR },         // DMU_START_SPOT
        { DDVT_FIXED },       // DMU_START_SPOT_X
        { DDVT_FIXED },       // DMU_START_SPOT_Y
        { DDVT_FIXED },       // DMU_START_SPOT_XY
        { DDVT_FIXED },       // DMU_DESTINATION_X
        { DDVT_FIXED },       // DMU_DESTINATION_Y
        { DDVT_FIXED },       // DMU_DESTINATION_XY
        { DDVT_ANGLE },       // DMU_DESTINATION_ANGLE
        { DDVT_INT },         // DMU_SPEED
        { DDVT_ANGLE },       // DMU_ANGLE_SPEED
        { DDVT_INT },         // DMU_SEQUENCE_TYPE
        { DDVT_BOOL },        // DMU_CRUSH
        { DDVT_PTR }          // DMU_SPECIAL_DATA
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
}

static const char* DMU_Str(int prop)
{
    static char propStr[40];
    struct prop_s {
        int prop;
        const char* str;
    } props[] =
    {
        { DMU_ALL, "DMU_ALL" },
        { 0, "(invalid)" },
        { DMU_VERTEX, "DMU_VERTEX" },
        { DMU_SEG, "DMU_SEG" },
        { DMU_LINE, "DMU_LINE" },
        { DMU_SIDE, "DMU_SIDE" },
        { DMU_NODE, "DMU_NODE" },
        { DMU_SUBSECTOR, "DMU_SUBSECTOR" },
        { DMU_SECTOR, "DMU_SECTOR" },
        { DMU_BLOCKMAP, "DMU_BLOCKMAP" },
        { DMU_REJECT, "DMU_REJECT" },
        { DMU_POLYBLOCKMAP, "DMU_POLYBLOCKMAP" },
        { DMU_POLYOBJ, "DMU_POLYOBJ" },
        { DMU_LINE_BY_TAG, "DMU_LINE_BY_TAG" },
        { DMU_SECTOR_BY_TAG, "DMU_SECTOR_BY_TAG" },
        { DMU_LINE_BY_ACT_TAG, "DMU_LINE_BY_ACT_TAG" },
        { DMU_SECTOR_BY_ACT_TAG, "DMU_SECTOR_BY_ACT_TAG" },
        { DMU_LINE_OF_SECTOR, "DMU_LINE_OF_SECTOR" },
        { DMU_SECTOR_OF_SUBSECTOR, "DMU_SECTOR_OF_SUBSECTOR" },
        { DMU_SEG_OF_POLYOBJ, "DMU_SEG_OF_POLYOBJ" },
        { DMU_X, "DMU_X" },
        { DMU_Y, "DMU_Y" },
        { DMU_XY, "DMU_XY" },
        { DMU_VERTEX1, "DMU_VERTEX1" },
        { DMU_VERTEX2, "DMU_VERTEX2" },
        { DMU_VERTEX1_X, "DMU_VERTEX1_X" },
        { DMU_VERTEX1_Y, "DMU_VERTEX1_Y" },
        { DMU_VERTEX1_XY, "DMU_VERTEX1_XY" },
        { DMU_VERTEX2_X, "DMU_VERTEX2_X" },
        { DMU_VERTEX2_Y, "DMU_VERTEX2_Y" },
        { DMU_VERTEX2_XY, "DMU_VERTEX2_XY" },
        { DMU_FRONT_SECTOR, "DMU_FRONT_SECTOR" },
        { DMU_BACK_SECTOR, "DMU_BACK_SECTOR" },
        { DMU_SIDE0, "DMU_SIDE0" },
        { DMU_SIDE1, "DMU_SIDE1" },
        { DMU_FLAGS, "DMU_FLAGS" },
        { DMU_DX, "DMU_DX" },
        { DMU_DY, "DMU_DY" },
        { DMU_LENGTH, "DMU_LENGTH" },
        { DMU_SLOPE_TYPE, "DMU_SLOPE_TYPE" },
        { DMU_ANGLE, "DMU_ANGLE" },
        { DMU_OFFSET, "DMU_OFFSET" },
        { DMU_TOP_TEXTURE, "DMU_TOP_TEXTURE" },
        { DMU_TOP_COLOR, "DMU_TOP_COLOR" },
        { DMU_MIDDLE_TEXTURE, "DMU_MIDDLE_TEXTURE" },
        { DMU_MIDDLE_COLOR, "DMU_MIDDLE_COLOR" },
        { DMU_MIDDLE_BLENDMODE, "DMU_MIDDLE_BLENDMODE" },
        { DMU_BOTTOM_TEXTURE, "DMU_BOTTOM_TEXTURE" },
        { DMU_BOTTOM_COLOR, "DMU_BOTTOM_COLOR" },
        { DMU_TEXTURE_OFFSET_X, "DMU_TEXTURE_OFFSET_X" },
        { DMU_TEXTURE_OFFSET_Y, "DMU_TEXTURE_OFFSET_Y" },
        { DMU_TEXTURE_OFFSET_XY, "DMU_TEXTURE_OFFSET_XY" },
        { DMU_VALID_COUNT, "DMU_VALID_COUNT" },
        { DMU_LINE_COUNT, "DMU_LINE_COUNT" },
        { DMU_COLOR, "DMU_COLOR" },
        { DMU_LIGHT_LEVEL, "DMU_LIGHT_LEVEL" },
        { DMU_THINGS, "DMU_THINGS" },
        { DMU_BOUNDING_BOX, "DMU_BOUNDING_BOX" },
        { DMU_SOUND_ORIGIN, "DMU_SOUND_ORIGIN" },
        { DMU_FLOOR_HEIGHT, "DMU_FLOOR_HEIGHT" },
        { DMU_FLOOR_TEXTURE, "DMU_FLOOR_TEXTURE" },
        { DMU_FLOOR_OFFSET_X, "DMU_FLOOR_OFFSET_X" },
        { DMU_FLOOR_OFFSET_Y, "DMU_FLOOR_OFFSET_Y" },
        { DMU_FLOOR_OFFSET_XY, "DMU_FLOOR_OFFSET_XY" },
        { DMU_FLOOR_TARGET, "DMU_FLOOR_TARGET" },
        { DMU_FLOOR_SPEED, "DMU_FLOOR_SPEED" },
        { DMU_FLOOR_COLOR, "DMU_FLOOR_COLOR" },
        { DMU_FLOOR_TEXTURE_MOVE_X, "DMU_FLOOR_TEXTURE_MOVE_X" },
        { DMU_FLOOR_TEXTURE_MOVE_Y, "DMU_FLOOR_TEXTURE_MOVE_Y" },
        { DMU_FLOOR_TEXTURE_MOVE_XY, "DMU_FLOOR_TEXTURE_MOVE_XY" },
        { DMU_CEILING_HEIGHT, "DMU_CEILING_HEIGHT" },
        { DMU_CEILING_TEXTURE, "DMU_CEILING_TEXTURE" },
        { DMU_CEILING_OFFSET_X, "DMU_CEILING_OFFSET_X" },
        { DMU_CEILING_OFFSET_Y, "DMU_CEILING_OFFSET_Y" },
        { DMU_CEILING_OFFSET_XY, "DMU_CEILING_OFFSET_XY" },
        { DMU_CEILING_TARGET, "DMU_CEILING_TARGET" },
        { DMU_CEILING_SPEED, "DMU_CEILING_SPEED" },
        { DMU_CEILING_COLOR, "DMU_CEILING_COLOR" },
        { DMU_CEILING_TEXTURE_MOVE_X, "DMU_CEILING_TEXTURE_MOVE_X" },
        { DMU_CEILING_TEXTURE_MOVE_Y, "DMU_CEILING_TEXTURE_MOVE_Y" },
        { DMU_CEILING_TEXTURE_MOVE_XY, "DMU_CEILING_TEXTURE_MOVE_XY" },
        { 0, NULL }
    };
    int i;

    for(i = 0; props[i].str; ++i)
        if(props[i].prop == prop)
            return props[i].str;

    sprintf(propStr, "(unnamed %i)", prop);
    return propStr;
}

static void InitArgs(setargs_t* args, valuetype_t type, int prop)
{
    memset(args, 0, sizeof(*args));
    args->type = type;
    args->prop = prop;
}

/*
 * Convert pointer to index.
 *
 * TODO: Think of a way to enforce type checks. At the moment it's a bit too
 *       easy to convert to the wrong kind of object.
 */
int P_ToIndex(int type, void* ptr)
{
    switch(type)
    {
    case DMU_VERTEX:
        return GET_VERTEX_IDX(ptr);

    case DMU_SEG:
        return GET_SEG_IDX(ptr);

    case DMU_LINE:
        return GET_LINE_IDX(ptr);

    case DMU_SIDE:
        return GET_SIDE_IDX(ptr);

    case DMU_SUBSECTOR:
        return GET_SUBSECTOR_IDX(ptr);

    case DMU_SECTOR:
        return GET_SECTOR_IDX(ptr);

    case DMU_SECTOR_OF_SUBSECTOR:
        return GET_SECTOR_IDX(((subsector_t*)ptr)->sector);

    case DMU_POLYOBJ:
        return GET_POLYOBJ_IDX(ptr);

    case DMU_NODE:
        return GET_NODE_IDX(ptr);

    default:
        Con_Error("P_ToIndex: unknown type %s.\n", DMU_Str(type));
    }
    return -1;
}

/*
 * Convert index to pointer.
 */
void* P_ToPtr(int type, int index)
{
    switch(type)
    {
    case DMU_VERTEX:
        return VERTEX_PTR(index);

    case DMU_SEG:
        return SEG_PTR(index);

    case DMU_LINE:
        return LINE_PTR(index);

    case DMU_SIDE:
        return SIDE_PTR(index);

    case DMU_SUBSECTOR:
        return SUBSECTOR_PTR(index);

    case DMU_SECTOR:
        return SECTOR_PTR(index);

    case DMU_POLYOBJ:
        return PO_PTR(index);

    case DMU_NODE:
        return NODE_PTR(index);

    default:
        Con_Error("P_ToPtr: unknown type %s.\n", DMU_Str(type));
    }
    return NULL;
}

/*
 * Call a callback function on a selection of map data objects. The selected
 * objects will be specified by 'type' and 'index'. 'context' is passed to the
 * callback function along with a pointer to the data object. P_Callback
 * returns true if all the calls to the callback function return true. False
 * is returned when the callback function returns false; in this case, the
 * iteration is aborted immediately when the callback function returns false.
 */
int P_Callback(int type, int index, void* context, int (*callback)(void* p, void* ctx))
{
    int i;

    switch(type)
    {
    case DMU_VERTEX:
        if(index >= 0 && index < numvertexes)
        {
            return callback(VERTEX_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numvertexes; ++i)
                if(!callback(VERTEX_PTR(i), context)) return false;
        }
        break;

    case DMU_SEG:
        if(index >= 0 && index < numsegs)
        {
            return callback(SEG_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsegs; ++i)
                if(!callback(SEG_PTR(i), context)) return false;
        }
        break;

    case DMU_LINE:
        if(index >= 0 && index < numlines)
        {
            return callback(LINE_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numlines; ++i)
                if(!callback(LINE_PTR(i), context)) return false;
        }
        break;

    case DMU_SIDE:
        if(index >= 0 && index < numsides)
        {
            return callback(SIDE_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsides; ++i)
                if(!callback(SIDE_PTR(i), context)) return false;
        }
        break;

    case DMU_NODE:
        if(index >= 0 && index < numnodes)
        {
            return callback(NODE_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numnodes; ++i)
                if(!callback(NODE_PTR(i), context)) return false;
        }
        break;

    case DMU_SUBSECTOR:
        if(index >= 0 && index < numsubsectors)
        {
            return callback(SUBSECTOR_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsubsectors; ++i)
                if(!callback(SUBSECTOR_PTR(i), context)) return false;
        }
        break;

    case DMU_SECTOR:
        if(index >= 0 && index < numsectors)
        {
            return callback(SECTOR_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsectors; ++i)
                if(!callback(SECTOR_PTR(i), context)) return false;
        }
        break;

    case DMU_LINE_BY_TAG:
    case DMU_SECTOR_BY_TAG:
    case DMU_LINE_BY_ACT_TAG:
    case DMU_SECTOR_BY_ACT_TAG:
        Con_Error("P_Callback: Type %s not implemented yet.\n", DMU_Str(type));
        /*
        for(i = 0; i < numlines; ++i)
        {
            if(!callback(LINE_PTR(i), context)) return false;
        }
        */
        break;

    default:
        Con_Error("P_Callback: Type %s unknown (index %i).\n", DMU_Str(type), index);
    }
    return true;
}

/*
 * Another version of callback iteration. The set of selected objects is
 * determined by 'type' and 'ptr'. Otherwise works like P_Callback.
 */
int P_Callbackp(int type, void* ptr, void* context, int (*callback)(void* p, void* ctx))
{
    switch(type)
    {
    case DMU_VERTEX:
    case DMU_SEG:
    case DMU_LINE:
    case DMU_SIDE:
    case DMU_NODE:
    case DMU_SUBSECTOR:
    case DMU_SECTOR:
        return callback(ptr, context);

    // TODO: If necessary, add special types for accessing multiple objects.

    default:
        Con_Error("P_Callbackp: Type %s unknown.\n", DMU_Str(type));
    }
    return true;
}

/*
 * Sets a value. Does some basic type checking so that incompatible types are
 * not assigned. Simple conversions are also done, e.g., float to fixed.
 */
static void SetValue(valuetype_t valueType, void* dst, setargs_t* args, int index)
{
    if(valueType == DDVT_FIXED)
    {
        fixed_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            *d = (args->byteValues[index] << FRACBITS);
            break;
        case DDVT_INT:
            *d = (args->intValues[index] << FRACBITS);
            break;
        case DDVT_FIXED:
            *d = args->fixedValues[index];
            break;
        case DDVT_FLOAT:
            *d = FLT2FIX(args->floatValues[index]);
            break;
        default:
            Con_Error("SetValue: DDVT_FIXED incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        float* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FIXED:
            *d = FIX2FLT(args->fixedValues[index]);
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BOOL)
    {
        boolean* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BOOL incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BYTE)
    {
        byte* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = (byte) args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BYTE incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_INT)
    {
        int* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_INT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_SHORT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_ANGLE:
            *d = args->angleValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        blendmode_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_INT:
            if(args->intValues[index] > DDNUM_BLENDMODES || args->intValues[index] < 0)
                Con_Error("SetValue: %d is not a valid value for DDVT_BLENDMODE.\n",
                          args->intValues[index]);

            *d = args->intValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BLENDMODE incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_PTR)
    {
        void** d = dst;

        switch(args->valueType)
        {
        case DDVT_PTR:
            *d = args->ptrValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_PTR incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else
    {
        Con_Error("SetValue: unknown value type %s.\n", DMU_Str(valueType));
    }
}

/*
 * Only those properties that are writable by outside parties (such as games)
 * are included here. Attempting to set a non-writable property causes a
 * fatal error.
 *
 * When a property changes, the relevant subsystems are notified of the change
 * so that they can update their state accordingly.
 */
static int SetProperty(void* ptr, void* context)
{
    setargs_t* args = (setargs_t*) context;

    switch(args->type)
    {
    case DMU_VERTEX:
        {
        // Vertices are not writable through DMU.
        Con_Error("SetProperty: DMU_VERTEX is not writable.\n");
        break;
        }

    case DMU_SEG:
        {
        seg_t* p = ptr;
        valuetype_t type = propertyTypes[args->prop];
        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(type, &p->flags, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_SEG.\n",
                      DMU_Str(args->prop));
        }
        break;
        }

    case DMU_LINE:
        {
        line_t* p = ptr;
        valuetype_t type = propertyTypes[args->prop];
        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(type, &p->flags, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_LINE.\n",
                      DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SIDE:
        {
        side_t* p = ptr;
        valuetype_t type = propertyTypes[args->prop];
        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(type, &p->flags, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_X:
            SetValue(type, &p->textureoffset, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_Y:
            SetValue(type, &p->rowoffset, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_XY:
            SetValue(type, &p->textureoffset, args, 0);
            SetValue(type, &p->rowoffset, args, 1);
            break;
        case DMU_TOP_COLOR:
            SetValue(type, &p->toprgb[0], args, 0);
            SetValue(type, &p->toprgb[1], args, 1);
            SetValue(type, &p->toprgb[2], args, 2);
            break;
        case DMU_TOP_TEXTURE:
            SetValue(type, &p->toptexture, args, 0);
            break;
        case DMU_MIDDLE_COLOR:
            SetValue(type, &p->midrgba[0], args, 0);
            SetValue(type, &p->midrgba[1], args, 1);
            SetValue(type, &p->midrgba[2], args, 2);
            SetValue(type, &p->midrgba[3], args, 3);
            break;
        case DMU_MIDDLE_BLENDMODE:
            SetValue(type, &p->blendmode, args, 0);
            break;
        case DMU_MIDDLE_TEXTURE:
            SetValue(type, &p->midtexture, args, 0);
            break;
        case DMU_BOTTOM_COLOR:
            SetValue(type, &p->bottomrgb[0], args, 0);
            SetValue(type, &p->bottomrgb[1], args, 1);
            SetValue(type, &p->bottomrgb[2], args, 2);
            break;
        case DMU_BOTTOM_TEXTURE:
            SetValue(type, &p->bottomtexture, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_SUBSECTOR.\n",
                      DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SUBSECTOR:
        {
        seg_t* p = ptr;
        valuetype_t type = propertyTypes[args->prop];
        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(type, &p->flags, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_SUBSECTOR.\n",
                      DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SECTOR:
        {
        sector_t* p = ptr;
        valuetype_t type = propertyTypes[args->prop];
        switch(args->prop)
        {
        case DMU_COLOR:
            SetValue(type, &p->rgb[0], args, 0);
            SetValue(type, &p->rgb[1], args, 1);
            SetValue(type, &p->rgb[2], args, 2);
            break;
        case DMU_LIGHT_LEVEL:
            SetValue(type, &p->lightlevel, args, 0);
            break;
        case DMU_FLOOR_COLOR:
            SetValue(type, &p->floorrgb[0], args, 0);
            SetValue(type, &p->floorrgb[1], args, 1);
            SetValue(type, &p->floorrgb[2], args, 2);
            break;
        case DMU_FLOOR_HEIGHT:
            SetValue(type, &p->floorheight, args, 0);
            break;
        case DMU_FLOOR_TEXTURE:
            SetValue(type, &p->floorpic, args, 0);
            break;
        case DMU_FLOOR_OFFSET_X:
            SetValue(type, &p->flooroffx, args, 0);
            break;
        case DMU_FLOOR_OFFSET_Y:
            SetValue(type, &p->flooroffy, args, 0);
            break;
        case DMU_FLOOR_OFFSET_XY:
            SetValue(type, &p->flooroffx, args, 0);
            SetValue(type, &p->flooroffy, args, 1);
            break;
        case DMU_FLOOR_TEXTURE_MOVE_X:
            SetValue(type, &p->planes[PLN_FLOOR].texmove[0], args, 0);
            break;
        case DMU_FLOOR_TEXTURE_MOVE_Y:
            SetValue(type, &p->planes[PLN_FLOOR].texmove[1], args, 0);
            break;
        case DMU_FLOOR_TEXTURE_MOVE_XY:
            SetValue(type, &p->planes[PLN_FLOOR].texmove[0], args, 0);
            SetValue(type, &p->planes[PLN_FLOOR].texmove[1], args, 1);
            break;
        case DMU_FLOOR_TARGET:
            SetValue(type, &p->planes[PLN_FLOOR].target, args, 0);
            break;
        case DMU_FLOOR_SPEED:
            SetValue(type, &p->planes[PLN_FLOOR].speed, args, 0);
            break;
        case DMU_CEILING_COLOR:
            SetValue(type, &p->ceilingrgb[0], args, 0);
            SetValue(type, &p->ceilingrgb[1], args, 1);
            SetValue(type, &p->ceilingrgb[2], args, 2);
            break;
        case DMU_CEILING_HEIGHT:
            SetValue(type, &p->ceilingheight, args, 0);
            break;
        case DMU_CEILING_TEXTURE:
            SetValue(type, &p->ceilingpic, args, 0);
            break;
        case DMU_CEILING_OFFSET_X:
            SetValue(type, &p->ceiloffx, args, 0);
            break;
        case DMU_CEILING_OFFSET_Y:
            SetValue(type, &p->ceiloffy, args, 0);
            break;
        case DMU_CEILING_OFFSET_XY:
            SetValue(type, &p->ceiloffx, args, 0);
            SetValue(type, &p->ceiloffy, args, 1);
            break;
        case DMU_CEILING_TEXTURE_MOVE_X:
            SetValue(type, &p->planes[PLN_CEILING].texmove[0], args, 0);
            break;
        case DMU_CEILING_TEXTURE_MOVE_Y:
            SetValue(type, &p->planes[PLN_CEILING].texmove[1], args, 0);
            break;
        case DMU_CEILING_TEXTURE_MOVE_XY:
            SetValue(type, &p->planes[PLN_CEILING].texmove[0], args, 0);
            SetValue(type, &p->planes[PLN_CEILING].texmove[1], args, 1);
            break;
        case DMU_CEILING_TARGET:
            SetValue(type, &p->planes[PLN_CEILING].target, args, 0);
            break;
        case DMU_CEILING_SPEED:
            SetValue(type, &p->planes[PLN_CEILING].speed, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_SEG.\n",
                      DMU_Str(args->prop));
        }
        // TODO: Notify the relevant subsystems.
        break;
        }

    case DMU_POLYOBJ:
        Con_Error("SetProperty: Property %s is not writable in DMU_POLYOBJ.\n",
                  DMU_Str(args->prop));
        break;

    case DMU_NODE:
        Con_Error("SetProperty: Property %s is not writable in DMU_NODE.\n",
                  DMU_Str(args->prop));
        break;

    default:
        Con_Error("SetProperty: Type %s not writable.\n", DMU_Str(args->type));
    }
    // Continue iteration.
    return true;
}

/*
 * Gets a value. Does some basic type checking so that incompatible types are
 * not assigned. Simple conversions are also done, e.g., float to fixed.
 */
static void GetValue(valuetype_t valueType, void* dst, setargs_t* args, int index)
{
    if(valueType == DDVT_FIXED)
    {
        fixed_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            args->byteValues[index] = (*d >> FRACBITS);
            break;
        case DDVT_INT:
            args->intValues[index] = (*d >> FRACBITS);
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = *d;
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = FIX2FLT(*d);
            break;
        default:
            Con_Error("GetValue: DDVT_FIXED incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        float* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            args->byteValues[index] = *d;
            break;
        case DDVT_INT:
            args->intValues[index] = (int) *d;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = FLT2FIX(*d);
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = *d;
            break;
        default:
            Con_Error("GetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BOOL)
    {
        boolean* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *d;
            break;
        default:
            Con_Error("GetValue: DDVT_BOOL incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BYTE)
    {
        byte* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *d;
            break;
        case DDVT_BYTE:
            args->byteValues[index] = *d;
            break;
        case DDVT_INT:
            args->intValues[index] = *d;
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = *d;
            break;
        default:
            Con_Error("GetValue: DDVT_BYTE incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_INT)
    {
        int* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *d;
            break;
        case DDVT_BYTE:
            args->byteValues[index] = *d;
            break;
        case DDVT_INT:
            args->intValues[index] = *d;
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = *d;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = (*d << FRACBITS);
            break;
        default:
            Con_Error("GetValue: DDVT_INT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *d;
            break;
        case DDVT_BYTE:
            args->byteValues[index] = *d;
            break;
        case DDVT_INT:
            args->intValues[index] = *d;
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = *d;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = (*d << FRACBITS);
            break;
        default:
            Con_Error("GetValue: DDVT_SHORT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_ANGLE:
            args->angleValues[index] = *d;
            break;
        default:
            Con_Error("GetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        blendmode_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_INT:
            args->intValues[index] = *d;
            break;
        default:
            Con_Error("GetValue: DDVT_BLENDMODE incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_PTR)
    {
        void** d = dst;

        switch(args->valueType)
        {
        case DDVT_PTR:
            args->ptrValues[index] = *d;
            break;
        default:
            Con_Error("GetValue: DDVT_PTR incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else
    {
        Con_Error("GetValue: unknown value type %s.\n", DMU_Str(valueType));
    }
}

static int GetProperty(void* ptr, void* context)
{
    setargs_t* args = (setargs_t*) context;

    switch(args->type)
    {
    case DMU_VERTEX:
        {
        vertex_t* p = ptr;
        valuetype_t type = propertyTypes[args->prop];
        switch(args->prop)
        {
        case DMU_X:
            GetValue(type, &p->x, args, 0);
            break;
        case DMU_Y:
            GetValue(type, &p->y, args, 0);
            break;
        case DMU_XY:
            GetValue(type, &p->x, args, 0);
            GetValue(type, &p->y, args, 1);
            break;
        default:
            Con_Error("GetProperty: DMU_VERTEX has no property %s.\n",
                      DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SEG:
        {
        seg_t* p = ptr;
        valuetype_t type = propertyTypes[args->prop];
        switch(args->prop)
        {
        case DMU_VERTEX1:
            GetValue(type, &p->v1, args, 0);
            break;
        case DMU_VERTEX2:
            GetValue(type, &p->v2, args, 0);
            break;
        case DMU_LENGTH:
            GetValue(type, &p->length, args, 0);
            break;
        case DMU_OFFSET:
            GetValue(type, &p->offset, args, 0);
            break;
        case DMU_SIDE:
            GetValue(type, &p->sidedef, args, 0);
            break;
        case DMU_LINE:
            GetValue(type, &p->linedef, args, 0);
            break;
        case DMU_FRONT_SECTOR:
            GetValue(type, &p->frontsector, args, 0);
            break;
        case DMU_BACK_SECTOR:
            GetValue(type, &p->backsector, args, 0);
            break;
        case DMU_FLAGS:
            GetValue(type, &p->flags, args, 0);
            break;
        case DMU_ANGLE:
            GetValue(type, &p->angle, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SEG has no property %s.\n", DMU_Str(args->prop));
        }
        break;
        }

    case DMU_LINE:
        {
        line_t* p = ptr;
        valuetype_t type = propertyTypes[args->prop];
        switch(args->prop)
        {
        case DMU_VERTEX1:
            GetValue(type, &p->v1, args, 0);
            break;
        case DMU_VERTEX2:
            GetValue(type, &p->v2, args, 0);
            break;
        case DMU_DX:
            GetValue(type, &p->dx, args, 0);
            break;
        case DMU_DY:
            GetValue(type, &p->dy, args, 0);
            break;
        case DMU_FRONT_SECTOR:
            GetValue(type, &p->frontsector, args, 0);
            break;
        case DMU_BACK_SECTOR:
            GetValue(type, &p->backsector, args, 0);
            break;
        case DMU_FLAGS:
            GetValue(type, &p->flags, args, 0);
            break;
        case DMU_SIDE0:
            GetValue(type, SIDE_PTR(p->sidenum[0]), args, 0);
            break;
        case DMU_SIDE1:
            GetValue(type, SIDE_PTR(p->sidenum[1]), args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_LINE has no property %s.\n", DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SIDE:
        {
        side_t* p = ptr;
        valuetype_t type = propertyTypes[args->prop];
        switch(args->prop)
        {
        case DMU_SECTOR:
            GetValue(type, &p->sector, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_X:
            GetValue(type, &p->textureoffset, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_Y:
            GetValue(type, &p->rowoffset, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_XY:
            GetValue(type, &p->textureoffset, args, 0);
            GetValue(type, &p->rowoffset, args, 1);
            break;
        case DMU_TOP_TEXTURE:
            GetValue(type, &p->toptexture, args, 0);
            break;
        case DMU_TOP_COLOR:
            GetValue(type, &p->toprgb[0], args, 0);
            GetValue(type, &p->toprgb[1], args, 1);
            GetValue(type, &p->toprgb[2], args, 2);
            break;
        case DMU_MIDDLE_TEXTURE:
            GetValue(type, &p->midtexture, args, 0);
            break;
        case DMU_MIDDLE_COLOR:
            GetValue(type, &p->midrgba[0], args, 0);
            GetValue(type, &p->midrgba[1], args, 1);
            GetValue(type, &p->midrgba[2], args, 2);
            GetValue(type, &p->midrgba[3], args, 3);
            break;
        case DMU_MIDDLE_BLENDMODE:
            GetValue(type, &p->blendmode, args, 0);
            break;
        case DMU_BOTTOM_TEXTURE:
            GetValue(type, &p->bottomtexture, args, 0);
            break;
        case DMU_BOTTOM_COLOR:
            GetValue(type, &p->bottomrgb[0], args, 0);
            GetValue(type, &p->bottomrgb[1], args, 1);
            GetValue(type, &p->bottomrgb[2], args, 2);
            break;
        case DMU_FLAGS:
            GetValue(type, &p->flags, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SIDE has no property %s.\n", DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SUBSECTOR:
        {
        subsector_t* p = ptr;
        valuetype_t type = propertyTypes[args->prop];
        switch(args->prop)
        {
        case DMU_SECTOR:
            GetValue(type, &p->sector, args, 0);
            break;
        case DMU_FLOOR_HEIGHT:
            GetValue(type, &p->sector->floorheight, args, 0);
            break;
        case DMU_FLOOR_TEXTURE:
            GetValue(type, &p->sector->floorpic, args, 0);
            break;
        case DMU_CEILING_HEIGHT:
            GetValue(type, &p->sector->ceilingheight, args, 0);
            break;
        case DMU_CEILING_TEXTURE:
            GetValue(type, &p->sector->ceilingpic, args, 0);
            break;
        case DMU_THINGS:
            GetValue(type, &p->sector->thinglist, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SUBSECTOR has no property %s.\n",
                      DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SECTOR_OF_SUBSECTOR:
    case DMU_SECTOR:
        {
        sector_t* p;
        valuetype_t type = propertyTypes[args->prop];
        if(args->type == DMU_SECTOR)
            p = ptr;
        else
            p = ((subsector_t*)ptr)->sector;

        switch(args->prop)
        {
        case DMU_LIGHT_LEVEL:
            GetValue(type, &p->lightlevel, args, 0);
            break;
        case DMU_COLOR:
            GetValue(type, &p->rgb[0], args, 0);
            GetValue(type, &p->rgb[1], args, 1);
            GetValue(type, &p->rgb[2], args, 2);
            break;
        case DMU_FLOOR_COLOR:
            GetValue(type, &p->floorrgb[0], args, 0);
            GetValue(type, &p->floorrgb[1], args, 1);
            GetValue(type, &p->floorrgb[2], args, 2);
            break;
        case DMU_FLOOR_HEIGHT:
            GetValue(type, &p->floorheight, args, 0);
            break;
        case DMU_FLOOR_TEXTURE:
            GetValue(type, &p->floorpic, args, 0);
            break;
        case DMU_CEILING_COLOR:
            GetValue(type, &p->ceilingrgb[0], args, 0);
            GetValue(type, &p->ceilingrgb[1], args, 1);
            GetValue(type, &p->ceilingrgb[2], args, 2);
            break;
        case DMU_CEILING_HEIGHT:
            GetValue(type, &p->ceilingheight, args, 0);
            break;
        case DMU_CEILING_TEXTURE:
            GetValue(type, &p->ceilingpic, args, 0);
            break;
        case DMU_LINE_COUNT:
            GetValue(type, &p->linecount, args, 0);
            break;
        case DMU_THINGS:
            GetValue(type, &p->thinglist, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SECTOR has no property %s.\n",
                       DMU_Str(args->prop));
        }
        break;
        }

    case DMU_LINE_OF_SECTOR:
        {
        sector_t* p = ptr;
        if(args->prop < 0 || args->prop >= p->linecount)
        {
            Con_Error("GetProperty: DMU_LINE_OF_SECTOR %i does not exist.\n",
                      args->prop);
        }
        GetValue(DDVT_PTR, &p->Lines[args->prop], args, 0);
        break;
        }

    default:
        Con_Error("GetProperty: Type %s not readable.\n", DMU_Str(args->type));
    }

    // Currently no aggregate values are collected.
    return false;
}

/*
 * Swaps two values. Does NOT do any type checking. Both values are
 * assumed to be of the correct (and same) type.
 *
 * TODO: How to do type checks?
 */
static void SwapValue(valuetype_t valueType, void* src, void* dst)
{
    if(valueType == DDVT_FIXED)
    {
        fixed_t tmp = *(fixed_t*) dst;

        *(fixed_t*) dst = *(fixed_t*) src;
        *(fixed_t*) src = tmp;
    }
    else if(valueType == DDVT_FLOAT)
    {
        float tmp = *(float*) dst;

        *(float*) dst = *(float*) src;
        *(float*) src = tmp;
    }
    else if(valueType == DDVT_BOOL)
    {
        boolean tmp = *(boolean*) dst;

        *(boolean*) dst = *(boolean*) src;
        *(boolean*) src = tmp;
    }
    else if(valueType == DDVT_BYTE)
    {
        byte tmp = *(byte*) dst;

        *(byte*) dst = *(byte*) src;
        *(byte*) src = tmp;
    }
    else if(valueType == DDVT_INT)
    {
        int tmp = *(int*) dst;

        *(int*) dst = *(int*) src;
        *(int*) src = tmp;
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short tmp = *(short*) dst;

        *(short*) dst = *(short*) src;
        *(short*) src = tmp;
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t tmp = *(angle_t*) dst;

        *(angle_t*) dst = *(angle_t*) src;
        *(angle_t*) src = tmp;
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        blendmode_t tmp = *(blendmode_t*) dst;

        *(blendmode_t*) dst = *(blendmode_t*) src;
        *(blendmode_t*) src = tmp;
    }
    else if(valueType == DDVT_PTR)
    {
        void* tmp = &dst;

        dst = &src;
        src = &tmp;
    }
    else
    {
        Con_Error("SwapValue: unknown value type %s.\n", DMU_Str(valueType));
    }
}

void P_SetBool(int type, int index, int prop, boolean param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetByte(int type, int index, int prop, byte param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetInt(int type, int index, int prop, int param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFixed(int type, int index, int prop, fixed_t param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetAngle(int type, int index, int prop, angle_t param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFloat(int type, int index, int prop, float param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetPtr(int type, int index, int prop, void* param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetBoolv(int type, int index, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetBytev(int type, int index, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetIntv(int type, int index, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFixedv(int type, int index, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetAnglev(int type, int index, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFloatv(int type, int index, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetPtrv(int type, int index, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callback(type, index, &args, SetProperty);
}

/* pointer-based write functions */

void P_SetBoolp(int type, void* ptr, int prop, boolean param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetBytep(int type, void* ptr, int prop, byte param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetIntp(int type, void* ptr, int prop, int param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFixedp(int type, void* ptr, int prop, fixed_t param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetAnglep(int type, void* ptr, int prop, angle_t param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFloatp(int type, void* ptr, int prop, float param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetPtrp(int type, void* ptr, int prop, void* param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetBoolpv(int type, void* ptr, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetBytepv(int type, void* ptr, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetIntpv(int type, void* ptr, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFixedpv(int type, void* ptr, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetAnglepv(int type, void* ptr, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFloatpv(int type, void* ptr, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetPtrpv(int type, void* ptr, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

/* index-based read functions */

boolean P_GetBool(int type, int index, int prop)
{
    setargs_t args;
    boolean returnValue = false;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

byte P_GetByte(int type, int index, int prop)
{
    setargs_t args;
    byte returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

int P_GetInt(int type, int index, int prop)
{
    setargs_t args;
    int returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

fixed_t P_GetFixed(int type, int index, int prop)
{
    setargs_t args;
    fixed_t returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

angle_t P_GetAngle(int type, int index, int prop)
{
    setargs_t args;
    angle_t returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

float P_GetFloat(int type, int index, int prop)
{
    setargs_t args;
    float returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

void* P_GetPtr(int type, int index, int prop)
{
    setargs_t args;
    void* returnValue = NULL;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

void P_GetBoolv(int type, int index, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetBytev(int type, int index, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetIntv(int type, int index, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetFixedv(int type, int index, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetAnglev(int type, int index, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetFloatv(int type, int index, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetPtrv(int type, int index, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callback(type, index, &args, GetProperty);
}

/* pointer-based read functions */

boolean P_GetBoolp(int type, void* ptr, int prop)
{
    setargs_t args;
    boolean returnValue = false;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

byte P_GetBytep(int type, void* ptr, int prop)
{
    setargs_t args;
    byte returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

int P_GetIntp(int type, void* ptr, int prop)
{
    setargs_t args;
    int returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

fixed_t P_GetFixedp(int type, void* ptr, int prop)
{
    setargs_t args;
    fixed_t returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

angle_t P_GetAnglep(int type, void* ptr, int prop)
{
    setargs_t args;
    angle_t returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

float P_GetFloatp(int type, void* ptr, int prop)
{
    setargs_t args;
    float returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

void* P_GetPtrp(int type, void* ptr, int prop)
{
    setargs_t args;
    void* returnValue = NULL;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

void P_GetBoolpv(int type, void* ptr, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetBytepv(int type, void* ptr, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetIntpv(int type, void* ptr, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetFixedpv(int type, void* ptr, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetAnglepv(int type, void* ptr, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetFloatpv(int type, void* ptr, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetPtrpv(int type, void* ptr, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_Copy(int type, int prop, int fromIndex, int toIndex)
{
    setargs_t args;
    int ptype = propertyTypes[prop];

    InitArgs(&args, type, prop);

    switch(ptype)
    {
    case DDVT_BOOL:
        {
        boolean b = false;

        args.booleanValues = &b;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_BYTE:
        {
        byte b = 0;

        args.byteValues = &b;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_INT:
        {
        int i = 0;

        args.intValues = &i;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_FIXED:
        {
        fixed_t f = 0;

        args.fixedValues = &f;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_ANGLE:
        {
        angle_t a = 0;

        args.angleValues = &a;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_FLOAT:
        {
        float f = 0;

        args.floatValues = &f;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_PTR:
        {
        void *ptr = NULL;

        args.ptrValues = &ptr;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    default:
        Con_Error("P_Copy: properties of type %s cannot be copied\n",
                  DMU_Str(prop));
    }
}

void P_Copyp(int type, int prop, void* from, void* to)
{
    setargs_t args;
    int ptype = propertyTypes[prop];

    InitArgs(&args, type, prop);

    switch(ptype)
    {
    case DDVT_BOOL:
        {
        boolean b = false;

        args.booleanValues = &b;
        P_Callbackp(type, from, &args, GetProperty);
        P_Callbackp(type, to, &args, SetProperty);
        break;
        }

    case DDVT_BYTE:
        {
        byte b = 0;

        args.byteValues = &b;
        P_Callbackp(type, from, &args, GetProperty);
        P_Callbackp(type, to, &args, SetProperty);
        break;
        }

    case DDVT_INT:
        {
        int i = 0;

        args.intValues = &i;
        P_Callbackp(type, from, &args, GetProperty);
        P_Callbackp(type, to, &args, SetProperty);
        break;
        }

    case DDVT_FIXED:
        {
        fixed_t f = 0;

        args.fixedValues = &f;
        P_Callbackp(type, from, &args, GetProperty);
        P_Callbackp(type, to, &args, SetProperty);
        break;
        }

    case DDVT_ANGLE:
        {
        angle_t a = 0;

        args.angleValues = &a;
        P_Callbackp(type, from, &args, GetProperty);
        P_Callbackp(type, to, &args, SetProperty);
        break;
        }

    case DDVT_FLOAT:
        {
        float f = 0;

        args.floatValues = &f;
        P_Callbackp(type, from, &args, GetProperty);
        P_Callbackp(type, to, &args, SetProperty);
        break;
        }

    case DDVT_PTR:
        {
        void *ptr = NULL;

        args.ptrValues = &ptr;
        P_Callbackp(type, from, &args, GetProperty);
        P_Callbackp(type, to, &args, SetProperty);
        break;
        }

    default:
        Con_Error("P_Copyp: properties of type %s cannot be copied\n",
                  DMU_Str(prop));
    }
}

void P_Swap(int type, int prop, int fromIndex, int toIndex)
{
    setargs_t argsA, argsB;
    int ptype = propertyTypes[prop];

    InitArgs(&argsA, type, prop);
    InitArgs(&argsB, type, prop);

    argsA.valueType = argsB.valueType = ptype;

    switch(ptype)
    {
    case DDVT_BOOL:
        {
        boolean a = false;
        boolean b = false;

        argsA.booleanValues = &a;
        argsB.booleanValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_BYTE:
        {
        byte a = 0;
        byte b = 0;

        argsA.byteValues = &a;
        argsB.byteValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_INT:
        {
        int a = 0;
        int b = 0;

        argsA.intValues = &a;
        argsB.intValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_FIXED:
        {
        fixed_t a = 0;
        fixed_t b = 0;

        argsA.fixedValues = &a;
        argsB.fixedValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_ANGLE:
        {
        angle_t a = 0;
        angle_t b = 0

        argsA.angleValues = &a;
        argsB.angleValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_FLOAT:
        {
        float a = 0;
        float b = 0;

        argsA.floatValues = &a;
        argsB.floatValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_PTR:
        {
        void *a = NULL;
        void *b = NULL;

        argsA.ptrValues = &a;
        argsB.ptrValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    default:
        Con_Error("P_Swap: properties of type %s cannot be swapped\n",
                  DMU_Str(prop));
    }
}

void P_Swapp(int type, int prop, void* from, void* to)
{
    setargs_t argsA, argsB;
    int ptype = propertyTypes[prop];

    InitArgs(&argsA, type, prop);
    InitArgs(&argsB, type, prop);

    argsA.valueType = argsB.valueType = ptype;

    switch(ptype)
    {
    case DDVT_BOOL:
        {
        boolean a = false;
        boolean b = false;

        argsA.booleanValues = &a;
        argsB.booleanValues = &b;

        P_Callbackp(type, from, &argsA, GetProperty);
        P_Callbackp(type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_BYTE:
        {
        byte a = 0;
        byte b = 0;

        argsA.byteValues = &a;
        argsB.byteValues = &b;

        P_Callbackp(type, from, &argsA, GetProperty);
        P_Callbackp(type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_INT:
        {
        int a = 0;
        int b = 0;

        argsA.intValues = &a;
        argsB.intValues = &b;

        P_Callbackp(type, from, &argsA, GetProperty);
        P_Callbackp(type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_FIXED:
        {
        fixed_t a = 0;
        fixed_t b = 0;

        argsA.fixedValues = &a;
        argsB.fixedValues = &b;

        P_Callbackp(type, from, &argsA, GetProperty);
        P_Callbackp(type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_ANGLE:
        {
        angle_t a = 0;
        angle_t b = 0

        argsA.angleValues = &a;
        argsB.angleValues = &b;

        P_Callbackp(type, from, &argsA, GetProperty);
        P_Callbackp(type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_FLOAT:
        {
        float a = 0;
        float b = 0;

        argsA.floatValues = &a;
        argsB.floatValues = &b;

        P_Callbackp(type, from, &argsA, GetProperty);
        P_Callbackp(type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_PTR:
        {
        void *a = NULL;
        void *b = NULL;

        argsA.ptrValues = &a;
        argsB.ptrValues = &b;

        P_Callbackp(type, from, &argsA, GetProperty);
        P_Callbackp(type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    default:
        Con_Error("P_Swapp: properties of type %s cannot be swapped\n",
                  DMU_Str(prop));
    }
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
