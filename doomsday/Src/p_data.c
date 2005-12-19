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
 *
 * Based on Hexen by Raven Software.
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

#if !__JHEXEN__
#define SEQTYPE_STONE 0
#endif

#define myoffsetof(type,identifier,fl) (((size_t)&((type*)0)->identifier)|fl)

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       32*FRACUNIT

// internal data types
enum {
    MDT_BYTE,
    MDT_SHORT,
    MDT_USHORT,
    MDT_FIXEDT,
    MDT_INT,
    MDT_UINT,
    MDT_LONG,
    MDT_ULONG
};

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum {
    ML_LABEL,                      // A separator, name, ExMx or MAPxx
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
} segtmp_t;

//
// Types used in map data handling
//
typedef struct {
    int valueid; // id number of the internal struct member to map the data to
    int gameprop; // if > 0 is a specific data item (passed to the game)
    int flags;
    int size;   // num of bytes
    int offset;
} datatype_t;

// Internal data format info structs
typedef struct {
    int         flags;
    int         dataType;
    size_t      size;
    ptrdiff_t   offset;
} structmember_t;

typedef struct {
    int         type;  // uneeded?
    size_t      size;
    int         nummembers;
    structmember_t *members;
} structinfo_t;

typedef struct {
    char *lumpname;
    void (*func) (byte *structure, const structinfo_t *idf, byte *buffer, size_t elmSize, int elements, int version, int values, const datatype_t *types);
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
typedef enum {
    idtThings,
    idtLineDefs,
    idtSideDefs,
    idtVertexes,
    idtSegs,
    idtSSectors,
    idtNodes,
    idtSectors,
    NUM_INTERNAL_DATA_STRUCTS
};

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
} badtex_t;

enum // Value types.
{
    VT_BOOL,
    VT_BYTE,
    VT_SHORT,
    VT_INT,    // 32 or 64
    VT_FIXED,
    VT_ANGLE,
    VT_FLOAT,
    VT_PTR,
    VT_FLAT_INDEX
};

typedef struct setargs_s {
    int type;
    int prop;
    int valueType;
    boolean* booleanValues;
    byte* byteValues;
    int* intValues;
    fixed_t* fixedValues;
    float* floatValues;
    angle_t* angleValues;
    void** ptrValues;
} setargs_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

float AccurateDistance(fixed_t dx, fixed_t dy);
int P_CheckTexture(char *name, boolean planeTex);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_InitMapDataFormats(void);

static boolean P_FindMapLumps(int lumps, int gllumps, char *levelID);
static boolean P_GetMapLumpFormats(int lumps, int gllumps, char *levelID);

static void P_LoadMapData(int type, int lumps, int gllumps);  // Move to DDay + export
static void P_CheckLevel(char *levelID, boolean silent);
static void P_GroupLines(void);
static void P_ProcessSegs(int version);

static void P_ReadBinaryMapData(byte *structure, const structinfo_t *idf, byte *buffer, size_t elmsize, int elements, int version, int values, const datatype_t *types);
static void P_ReadSideDefs(byte *structure, const structinfo_t *idf, byte *buffer, size_t elmsize, int elements, int version, int values, const datatype_t *types);
static void P_ReadLineDefs(byte *structure, const structinfo_t *idf, byte *buffer, size_t elmsize, int elements, int version, int values, const datatype_t *types);
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

int     numthings;
byte   *things;

short  *blockmaplump;           // offsets in blockmap are from here
short  *blockmap;
int     bmapwidth, bmapheight;  // in mapblocks
fixed_t bmaporgx, bmaporgy;     // origin of block map
linkmobj_t *blockrings;         // for thing rings
byte   *rejectmatrix;           // for fast sight rejection
polyblock_t **polyblockmap;     // polyobj blockmap
nodepile_t thingnodes, linenodes;   // all kinds of wacky links

ded_mapinfo_t *mapinfo = 0;     // Current mapinfo.
fixed_t mapgravity;             // Gravity for the current map.

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
    {"LINEDEFS", P_ReadLineDefs,      1, -1,  idtLineDefs,   mlLineDefs,   ML_LINEDEFS,true,     false},
    {"SIDEDEFS", P_ReadSideDefs,      2, -1,  idtSideDefs,   mlSideDefs,   ML_SIDEDEFS,true,     false},
    {"VERTEXES", P_ReadBinaryMapData, 3, -1,  idtVertexes,   mlVertexes,   ML_VERTEXES,true,     false},
    {"SSECTORS", P_ReadBinaryMapData, 5, -1,  idtSSectors,   mlSSectors,   ML_SSECTORS,true,     false},
    {"SEGS",     P_ReadBinaryMapData, 4, -1,  idtSegs,       mlSegs,       ML_SEGS,    true,     false},
    {"NODES",    P_ReadBinaryMapData, 6, -1,  idtNodes,      mlNodes,      ML_NODES,   true,     false},
    {"SECTORS",  P_ReadBinaryMapData, 7, -1,  idtSectors,    mlSectors,    ML_SECTORS, true,     false},
    {"REJECT",   NULL,                8, -1,  -1,            mlReject,     ML_REJECT,  false,    false},
    {"BLOCKMAP", NULL,                9, -1,  -1,            mlBlockMap,   ML_BLOCKMAP,true,     false},
    {"BEHAVIOR", NULL,                10,-1,  -1,            mlBehavior,   ML_BEHAVIOR,false,    false},
    {"GL_VERT",  P_ReadBinaryMapData, -1, 0,  idtVertexes,   glVerts,      1,          false,    false},
    {"GL_SEGS",  P_ReadBinaryMapData, -1, 1,  idtSegs,       glSegs,       2,          false,    false},
    {"GL_SSECT", P_ReadBinaryMapData, -1, 2,  idtSSectors,   glSSects,     3,          false,    false},
    {"GL_NODES", P_ReadBinaryMapData, -1, 3,  idtNodes,      glNodes,      4,          false,    false},
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

static segtmp_t *segstemp;

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

structinfo_t internalMapDataStructInfo[NUM_INTERNAL_DATA_STRUCTS];

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
        { DMU_X, "DMU_X" },
        { DMU_Y, "DMU_Y" },
        { DMU_XY, "DMU_XY" },
        { DMU_VERTEX1, "DMU_VERTEX1" },
        { DMU_VERTEX2, "DMU_VERTEX2" },
        { DMU_FRONT_SECTOR, "DMU_FRONT_SECTOR" },
        { DMU_BACK_SECTOR, "DMU_BACK_SECTOR" },
        { DMU_FLAGS, "DMU_FLAGS" },
        { DMU_DX, "DMU_DX" },
        { DMU_DY, "DMU_DY" },
        { DMU_LENGTH, "DMU_LENGTH" },
        { DMU_ANGLE, "DMU_ANGLE" },
        { DMU_OFFSET, "DMU_OFFSET" },
        { DMU_TOP_TEXTURE, "DMU_TOP_TEXTURE" },
        { DMU_MIDDLE_TEXTURE, "DMU_MIDDLE_TEXTURE" },
        { DMU_BOTTOM_TEXTURE, "DMU_BOTTOM_TEXTURE" },
        { DMU_TEXTURE_OFFSET_X, "DMU_TEXTURE_OFFSET_X" },
        { DMU_TEXTURE_OFFSET_Y, "DMU_TEXTURE_OFFSET_Y" },
        { DMU_TEXTURE_OFFSET_XY, "DMU_TEXTURE_OFFSET_XY" },
        { DMU_LINE_COUNT, "DMU_LINE_COUNT" },
        { DMU_COLOR, "DMU_COLOR" },
        { DMU_LIGHT_LEVEL, "DMU_LIGHT_LEVEL" },
        { DMU_THINGS, "DMU_THINGS" },
        { DMU_FLOOR_HEIGHT, "DMU_FLOOR_HEIGHT" },
        { DMU_FLOOR_TEXTURE, "DMU_FLOOR_TEXTURE" },
        { DMU_FLOOR_OFFSET_X, "DMU_FLOOR_OFFSET_X" },
        { DMU_FLOOR_OFFSET_Y, "DMU_FLOOR_OFFSET_Y" },
        { DMU_FLOOR_OFFSET_XY, "DMU_FLOOR_OFFSET_XY" },
        { DMU_FLOOR_TARGET, "DMU_FLOOR_TARGET" },
        { DMU_FLOOR_SPEED, "DMU_FLOOR_SPEED" },
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

static void InitArgs(setargs_t* args, int type, int prop)
{
    memset(args, 0, sizeof(*args));
    args->type = type;
    args->prop = prop;
}

/*
 * Convert pointer to index.
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
static void SetValue(int valueType, void* dst, setargs_t* args, int index)
{
    if(valueType == VT_FIXED)
    {
        fixed_t* d = dst;

        switch(args->valueType)
        {
        case VT_BYTE:
            *d = (args->byteValues[index] << FRACBITS);
            break;
        case VT_INT:
            *d = (args->intValues[index] << FRACBITS);
            break;
        case VT_FIXED:
            *d = args->fixedValues[index];
            break;
        case VT_FLOAT:
            *d = FLT2FIX(args->floatValues[index]);
            break;
        default:
            Con_Error("SetValue: VT_FIXED incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_FLOAT)
    {
        float* d = dst;

        switch(args->valueType)
        {
        case VT_BYTE:
            *d = args->byteValues[index];
            break;
        case VT_INT:
            *d = args->intValues[index];
            break;
        case VT_FIXED:
            *d = FIX2FLT(args->fixedValues[index]);
            break;
        case VT_FLOAT:
            *d = args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: VT_FLOAT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_BOOL)
    {
        boolean* d = dst;

        switch(args->valueType)
        {
        case VT_BOOL:
            *d = args->booleanValues[index];
            break;
        default:
            Con_Error("SetValue: VT_BOOL incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_BYTE)
    {
        byte* d = dst;

        switch(args->valueType)
        {
        case VT_BOOL:
            *d = args->booleanValues[index];
            break;
        case VT_BYTE:
            *d = args->byteValues[index];
            break;
        case VT_INT:
            *d = args->intValues[index];
            break;
        case VT_FLOAT:
            *d = (byte) args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: VT_BYTE incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_INT)
    {
        int* d = dst;

        switch(args->valueType)
        {
        case VT_BOOL:
            *d = args->booleanValues[index];
            break;
        case VT_BYTE:
            *d = args->byteValues[index];
            break;
        case VT_INT:
            *d = args->intValues[index];
            break;
        case VT_FLOAT:
            *d = args->floatValues[index];
            break;
        case VT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: VT_INT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_SHORT || valueType == VT_FLAT_INDEX)
    {
        short* d = dst;

        switch(args->valueType)
        {
        case VT_BOOL:
            *d = args->booleanValues[index];
            break;
        case VT_BYTE:
            *d = args->byteValues[index];
            break;
        case VT_INT:
            *d = args->intValues[index];
            break;
        case VT_FLOAT:
            *d = args->floatValues[index];
            break;
        case VT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: VT_SHORT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_ANGLE)
    {
        angle_t* d = dst;

        switch(args->valueType)
        {
        case VT_ANGLE:
            *d = args->angleValues[index];
            break;
        default:
            Con_Error("SetValue: VT_ANGLE incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_PTR)
    {
        void** d = dst;

        switch(args->valueType)
        {
        case VT_PTR:
            *d = args->ptrValues[index];
            break;
        default:
            Con_Error("SetValue: VT_PTR incompatible with value type %s.\n",
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
        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(VT_BYTE, &p->flags, args, 0);
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
        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(VT_SHORT, &p->flags, args, 0);
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
        switch(args->prop)
        {
        case DMU_TEXTURE_OFFSET_X:
            SetValue(VT_FIXED, &p->textureoffset, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_Y:
            SetValue(VT_FIXED, &p->rowoffset, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_XY:
            SetValue(VT_FIXED, &p->textureoffset, args, 0);
            SetValue(VT_FIXED, &p->rowoffset, args, 1);
            break;
        case DMU_TOP_TEXTURE:
            SetValue(VT_FLAT_INDEX, &p->toptexture, args, 0);
            break;
        case DMU_MIDDLE_TEXTURE:
            SetValue(VT_FLAT_INDEX, &p->midtexture, args, 0);
            break;
        case DMU_BOTTOM_TEXTURE:
            SetValue(VT_FLAT_INDEX, &p->bottomtexture, args, 0);
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
        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(VT_BYTE, &p->flags, args, 0);
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
        switch(args->prop)
        {
        case DMU_COLOR:
            SetValue(VT_BYTE, &p->rgb[0], args, 0);
            SetValue(VT_BYTE, &p->rgb[1], args, 1);
            SetValue(VT_BYTE, &p->rgb[2], args, 2);
            break;
        case DMU_LIGHT_LEVEL:
            SetValue(VT_SHORT, &p->lightlevel, args, 0);
            break;
        case DMU_FLOOR_HEIGHT:
            SetValue(VT_FIXED, &p->floorheight, args, 0);
            break;
        case DMU_FLOOR_TEXTURE:
            SetValue(VT_FLAT_INDEX, &p->floorpic, args, 0);
            break;
        case DMU_FLOOR_OFFSET_X:
            SetValue(VT_FLOAT, &p->flooroffx, args, 0);
            break;
        case DMU_FLOOR_OFFSET_Y:
            SetValue(VT_FLOAT, &p->flooroffy, args, 0);
            break;
        case DMU_FLOOR_OFFSET_XY:
            SetValue(VT_FLOAT, &p->flooroffx, args, 0);
            SetValue(VT_FLOAT, &p->flooroffy, args, 1);
            break;
        case DMU_FLOOR_TEXTURE_MOVE_X:
            SetValue(VT_INT, &p->planes[PLN_FLOOR].texmove[0], args, 0);
            break;
        case DMU_FLOOR_TEXTURE_MOVE_Y:
            SetValue(VT_INT, &p->planes[PLN_FLOOR].texmove[1], args, 0);
            break;
        case DMU_FLOOR_TEXTURE_MOVE_XY:
            SetValue(VT_INT, &p->planes[PLN_FLOOR].texmove[0], args, 0);
            SetValue(VT_INT, &p->planes[PLN_FLOOR].texmove[1], args, 1);
            break;
        case DMU_FLOOR_TARGET:
            SetValue(VT_INT, &p->planes[PLN_FLOOR].target, args, 0);
            break;
        case DMU_FLOOR_SPEED:
            SetValue(VT_INT, &p->planes[PLN_FLOOR].speed, args, 0);
            break;
        case DMU_CEILING_HEIGHT:
            SetValue(VT_FIXED, &p->ceilingheight, args, 0);
            break;
        case DMU_CEILING_TEXTURE:
            SetValue(VT_FLAT_INDEX, &p->ceilingpic, args, 0);
            break;
        case DMU_CEILING_OFFSET_X:
            SetValue(VT_FLOAT, &p->ceiloffx, args, 0);
            break;
        case DMU_CEILING_OFFSET_Y:
            SetValue(VT_FLOAT, &p->ceiloffy, args, 0);
            break;
        case DMU_CEILING_OFFSET_XY:
            SetValue(VT_FLOAT, &p->ceiloffx, args, 0);
            SetValue(VT_FLOAT, &p->ceiloffy, args, 1);
            break;
        case DMU_CEILING_TEXTURE_MOVE_X:
            SetValue(VT_INT, &p->planes[PLN_CEILING].texmove[0], args, 0);
            break;
        case DMU_CEILING_TEXTURE_MOVE_Y:
            SetValue(VT_INT, &p->planes[PLN_CEILING].texmove[1], args, 0);
            break;
        case DMU_CEILING_TEXTURE_MOVE_XY:
            SetValue(VT_INT, &p->planes[PLN_CEILING].texmove[0], args, 0);
            SetValue(VT_INT, &p->planes[PLN_CEILING].texmove[1], args, 1);
            break;
        case DMU_CEILING_TARGET:
            SetValue(VT_INT, &p->planes[PLN_CEILING].target, args, 0);
            break;
        case DMU_CEILING_SPEED:
            SetValue(VT_INT, &p->planes[PLN_CEILING].speed, args, 0);
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
static void GetValue(int valueType, void* dst, setargs_t* args, int index)
{
    if(valueType == VT_FIXED)
    {
        fixed_t* d = dst;

        switch(args->valueType)
        {
        case VT_BYTE:
            args->byteValues[index] = (*d >> FRACBITS);
            break;
        case VT_INT:
            args->intValues[index] = (*d >> FRACBITS);
            break;
        case VT_FIXED:
            args->fixedValues[index] = *d;
            break;
        case VT_FLOAT:
            args->floatValues[index] = FIX2FLT(*d);
            break;
        default:
            Con_Error("GetValue: VT_FIXED incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_FLOAT)
    {
        float* d = dst;

        switch(args->valueType)
        {
        case VT_BYTE:
            args->byteValues[index] = *d;
            break;
        case VT_INT:
            args->intValues[index] = (int) *d;
            break;
        case VT_FIXED:
            args->fixedValues[index] = FLT2FIX(*d);
            break;
        case VT_FLOAT:
            args->floatValues[index] = *d;
            break;
        default:
            Con_Error("GetValue: VT_FLOAT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_BOOL)
    {
        boolean* d = dst;

        switch(args->valueType)
        {
        case VT_BOOL:
            args->booleanValues[index] = *d;
            break;
        default:
            Con_Error("GetValue: VT_BOOL incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_BYTE)
    {
        byte* d = dst;

        switch(args->valueType)
        {
        case VT_BOOL:
            args->booleanValues[index] = *d;
            break;
        case VT_BYTE:
            args->byteValues[index] = *d;
            break;
        case VT_INT:
            args->intValues[index] = *d;
            break;
        case VT_FLOAT:
            args->floatValues[index] = *d;
            break;
        default:
            Con_Error("GetValue: VT_BYTE incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_INT)
    {
        int* d = dst;

        switch(args->valueType)
        {
        case VT_BOOL:
            args->booleanValues[index] = *d;
            break;
        case VT_BYTE:
            args->byteValues[index] = *d;
            break;
        case VT_INT:
            args->intValues[index] = *d;
            break;
        case VT_FLOAT:
            args->floatValues[index] = *d;
            break;
        case VT_FIXED:
            args->fixedValues[index] = (*d << FRACBITS);
            break;
        default:
            Con_Error("GetValue: VT_INT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_SHORT || valueType == VT_FLAT_INDEX)
    {
        short* d = dst;

        switch(args->valueType)
        {
        case VT_BOOL:
            args->booleanValues[index] = *d;
            break;
        case VT_BYTE:
            args->byteValues[index] = *d;
            break;
        case VT_INT:
            args->intValues[index] = *d;
            break;
        case VT_FLOAT:
            args->floatValues[index] = *d;
            break;
        case VT_FIXED:
            args->fixedValues[index] = (*d << FRACBITS);
            break;
        default:
            Con_Error("GetValue: VT_SHORT incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_ANGLE)
    {
        angle_t* d = dst;

        switch(args->valueType)
        {
        case VT_ANGLE:
            args->angleValues[index] = *d;
            break;
        default:
            Con_Error("GetValue: VT_ANGLE incompatible with value type %s.\n",
                      DMU_Str(args->valueType));
        }
    }
    else if(valueType == VT_PTR)
    {
        void** d = dst;

        switch(args->valueType)
        {
        case VT_PTR:
            args->ptrValues[index] = *d;
            break;
        default:
            Con_Error("GetValue: VT_PTR incompatible with value type %s.\n",
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
        switch(args->prop)
        {
        case DMU_X:
            GetValue(VT_FIXED, &p->x, args, 0);
            break;
        case DMU_Y:
            GetValue(VT_FIXED, &p->y, args, 0);
            break;
        case DMU_XY:
            GetValue(VT_FIXED, &p->x, args, 0);
            GetValue(VT_FIXED, &p->y, args, 1);
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
        switch(args->prop)
        {
        case DMU_VERTEX1:
            GetValue(VT_PTR, &p->v1, args, 0);
            break;
        case DMU_VERTEX2:
            GetValue(VT_PTR, &p->v2, args, 0);
            break;
        case DMU_LENGTH:
            GetValue(VT_FLOAT, &p->length, args, 0);
            break;
        case DMU_OFFSET:
            GetValue(VT_FIXED, &p->offset, args, 0);
            break;
        case DMU_SIDE:
            GetValue(VT_PTR, &p->sidedef, args, 0);
            break;
        case DMU_LINE:
            GetValue(VT_PTR, &p->linedef, args, 0);
            break;
        case DMU_FRONT_SECTOR:
            GetValue(VT_PTR, &p->frontsector, args, 0);
            break;
        case DMU_BACK_SECTOR:
            GetValue(VT_PTR, &p->backsector, args, 0);
            break;
        case DMU_FLAGS:
            GetValue(VT_BYTE, &p->flags, args, 0);
            break;
        case DMU_ANGLE:
            GetValue(VT_ANGLE, &p->angle, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SEG has no property %s.\n", DMU_Str(args->prop));
        }
        break;
        }

    case DMU_LINE:
        {
        line_t* p = ptr;
        switch(args->prop)
        {
        case DMU_VERTEX1:
            GetValue(VT_PTR, &p->v1, args, 0);
            break;
        case DMU_VERTEX2:
            GetValue(VT_PTR, &p->v2, args, 0);
            break;
        case DMU_FRONT_SECTOR:
            GetValue(VT_PTR, &p->frontsector, args, 0);
            break;
        case DMU_BACK_SECTOR:
            GetValue(VT_PTR, &p->backsector, args, 0);
            break;
        case DMU_FLAGS:
            GetValue(VT_SHORT, &p->flags, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_LINE has no property %s.\n", DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SIDE:
        {
        side_t* p = ptr;
        switch(args->prop)
        {
        case DMU_SECTOR:
            GetValue(VT_PTR, &p->sector, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_X:
            GetValue(VT_FIXED, &p->textureoffset, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_Y:
            GetValue(VT_FIXED, &p->rowoffset, args, 0);
            break;
        case DMU_TEXTURE_OFFSET_XY:
            GetValue(VT_FIXED, &p->textureoffset, args, 0);
            GetValue(VT_FIXED, &p->rowoffset, args, 1);
            break;
        case DMU_TOP_TEXTURE:
            GetValue(VT_FLAT_INDEX, &p->toptexture, args, 0);
            break;
        case DMU_MIDDLE_TEXTURE:
            GetValue(VT_FLAT_INDEX, &p->midtexture, args, 0);
            break;
        case DMU_BOTTOM_TEXTURE:
            GetValue(VT_FLAT_INDEX, &p->bottomtexture, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SIDE has no property %s.\n", DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SUBSECTOR:
        {
        subsector_t* p = ptr;
        switch(args->prop)
        {
        case DMU_SECTOR:
            GetValue(VT_PTR, &p->sector, args, 0);
            break;
        case DMU_FLOOR_HEIGHT:
            GetValue(VT_FIXED, &p->sector->floorheight, args, 0);
            break;
        case DMU_CEILING_HEIGHT:
            GetValue(VT_FIXED, &p->sector->ceilingheight, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SUBSECTOR has no property %s.\n", DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SECTOR:
        {
        sector_t* p = ptr;
        switch(args->prop)
        {
        case DMU_FLOOR_HEIGHT:
            GetValue(VT_FIXED, &p->floorheight, args, 0);
            break;
        case DMU_CEILING_HEIGHT:
            GetValue(VT_FIXED, &p->ceilingheight, args, 0);
            break;
        case DMU_THINGS:
            GetValue(VT_PTR, &p->thinglist, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SECTOR has no property %s.\n", DMU_Str(args->prop));
        }
        break;
        }

    default:
        Con_Error("GetProperty: Type %s not readable.\n", DMU_Str(args->type));
    }

    // Currently no aggregate values are collected.
    return false;
}

void P_SetBool(int type, int index, int prop, boolean param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetByte(int type, int index, int prop, byte param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BYTE;
    args.byteValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetInt(int type, int index, int prop, int param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_INT;
    args.intValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFixed(int type, int index, int prop, fixed_t param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FIXED;
    args.fixedValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetAngle(int type, int index, int prop, angle_t param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_ANGLE;
    args.angleValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFloat(int type, int index, int prop, float param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FLOAT;
    args.floatValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetPtr(int type, int index, int prop, void* param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_PTR;
    args.ptrValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetBoolv(int type, int index, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetBytev(int type, int index, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetIntv(int type, int index, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFixedv(int type, int index, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetAnglev(int type, int index, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFloatv(int type, int index, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetPtrv(int type, int index, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_PTR;
    args.ptrValues = params;
    P_Callback(type, index, &args, SetProperty);
}

/* pointer-based write functions */

void P_SetBoolp(int type, void* ptr, int prop, boolean param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetBytep(int type, void* ptr, int prop, byte param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BYTE;
    args.byteValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetIntp(int type, void* ptr, int prop, int param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_INT;
    args.intValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFixedp(int type, void* ptr, int prop, fixed_t param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FIXED;
    args.fixedValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetAnglep(int type, void* ptr, int prop, angle_t param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_ANGLE;
    args.angleValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFloatp(int type, void* ptr, int prop, float param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FLOAT;
    args.floatValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetPtrp(int type, void* ptr, int prop, void* param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_PTR;
    args.ptrValues = &param;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetBoolpv(int type, void* ptr, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BOOL;
    args.booleanValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetBytepv(int type, void* ptr, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BYTE;
    args.byteValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetIntpv(int type, void* ptr, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_INT;
    args.intValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFixedpv(int type, void* ptr, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FIXED;
    args.fixedValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetAnglepv(int type, void* ptr, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_ANGLE;
    args.angleValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetFloatpv(int type, void* ptr, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FLOAT;
    args.floatValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

void P_SetPtrpv(int type, void* ptr, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_PTR;
    args.ptrValues = params;
    P_Callbackp(type, ptr, &args, SetProperty);
}

/* index-based read functions */

boolean P_GetBool(int type, int index, int prop)
{
    setargs_t args;
    boolean returnValue = false;

    InitArgs(&args, type, prop);
    args.valueType = VT_BOOL;
    args.booleanValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

byte P_GetByte(int type, int index, int prop)
{
    setargs_t args;
    byte returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = VT_BYTE;
    args.byteValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

int P_GetInt(int type, int index, int prop)
{
    setargs_t args;
    int returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = VT_INT;
    args.intValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

fixed_t P_GetFixed(int type, int index, int prop)
{
    setargs_t args;
    fixed_t returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = VT_FIXED;
    args.fixedValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

angle_t P_GetAngle(int type, int index, int prop)
{
    setargs_t args;
    angle_t returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = VT_ANGLE;
    args.angleValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

float P_GetFloat(int type, int index, int prop)
{
    setargs_t args;
    float returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = VT_FLOAT;
    args.floatValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

void* P_GetPtr(int type, int index, int prop)
{
    setargs_t args;
    void* returnValue = NULL;

    InitArgs(&args, type, prop);
    args.valueType = VT_PTR;
    args.ptrValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

void P_GetBoolv(int type, int index, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetBytev(int type, int index, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetIntv(int type, int index, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetFixedv(int type, int index, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetAnglev(int type, int index, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetFloatv(int type, int index, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetPtrv(int type, int index, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_PTR;
    args.ptrValues = params;
    P_Callback(type, index, &args, GetProperty);
}

/* pointer-based read functions */

boolean P_GetBoolp(int type, void* ptr, int prop)
{
    setargs_t args;
    boolean returnValue = false;

    InitArgs(&args, type, prop);
    args.valueType = VT_BOOL;
    args.booleanValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

byte P_GetBytep(int type, void* ptr, int prop)
{
    setargs_t args;
    byte returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = VT_BYTE;
    args.byteValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

int P_GetIntp(int type, void* ptr, int prop)
{
    setargs_t args;
    int returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = VT_INT;
    args.intValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

fixed_t P_GetFixedp(int type, void* ptr, int prop)
{
    setargs_t args;
    fixed_t returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = VT_FIXED;
    args.fixedValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

angle_t P_GetAnglep(int type, void* ptr, int prop)
{
    setargs_t args;
    angle_t returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = VT_ANGLE;
    args.angleValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

float P_GetFloatp(int type, void* ptr, int prop)
{
    setargs_t args;
    float returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = VT_FLOAT;
    args.floatValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

void* P_GetPtrp(int type, void* ptr, int prop)
{
    setargs_t args;
    void* returnValue = NULL;

    InitArgs(&args, type, prop);
    args.valueType = VT_PTR;
    args.ptrValues = &returnValue;
    P_Callbackp(type, ptr, &args, GetProperty);
    return returnValue;
}

void P_GetBoolpv(int type, void* ptr, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BOOL;
    args.booleanValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetBytepv(int type, void* ptr, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_BYTE;
    args.byteValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetIntpv(int type, void* ptr, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_INT;
    args.intValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetFixedpv(int type, void* ptr, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FIXED;
    args.fixedValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetAnglepv(int type, void* ptr, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_ANGLE;
    args.angleValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetFloatpv(int type, void* ptr, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_FLOAT;
    args.floatValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
}

void P_GetPtrpv(int type, void* ptr, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = VT_PTR;
    args.ptrValues = params;
    P_Callbackp(type, ptr, &args, GetProperty);
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

//==========================================================================
// P_LoadBlockMap
//==========================================================================
void P_LoadBlockMap(int lump)
{
    int     i, count;

    /*
       // Disabled for now: Plutonia MAP28.
       if(W_LumpLength(lump) > 0x7fff)  // From GMJ.
       Con_Error("Invalid map - Try using this with Boomsday.\n");
     */

    blockmaplump = W_CacheLumpNum(lump, PU_LEVEL);
    blockmap = blockmaplump + 4;
    count = W_LumpLength(lump) / 2;
    for(i = 0; i < count; i++)
        blockmaplump[i] = SHORT(blockmaplump[i]);

    bmaporgx = blockmaplump[0] << FRACBITS;
    bmaporgy = blockmaplump[1] << FRACBITS;
    bmapwidth = blockmaplump[2];
    bmapheight = blockmaplump[3];

    // Clear out mobj rings.
    count = sizeof(*blockrings) * bmapwidth * bmapheight;
    blockrings = Z_Malloc(count, PU_LEVEL, 0);
    memset(blockrings, 0, count);
    for(i = 0; i < bmapwidth * bmapheight; i++)
        blockrings[i].next = blockrings[i].prev = (mobj_t *) &blockrings[i];
}

/*
   //==========================================================================
   // P_LoadBlockMap
   //   Modified for long blockmap/blockmaplump defs - GMJ Sep 2001
   //   I want to think about this a bit more... -jk
   //==========================================================================
   void P_LoadBlockMap(int lump)
   {
   long i, count = W_LumpLength(lump)/2;
   short *wadblockmaplump;

   if (count >= 0x10000)
   Con_Error("Map exceeds limits of +/- 32767 map units");

   wadblockmaplump = W_CacheLumpNum (lump, PU_LEVEL);
   blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, PU_LEVEL, 0);

   // killough 3/1/98: Expand wad blockmap into larger internal one,
   // by treating all offsets except -1 as unsigned and zero-extending
   // them. This potentially doubles the size of blockmaps allowed,
   // because Doom originally considered the offsets as always signed.

   blockmaplump[0] = SHORT(wadblockmaplump[0]);
   blockmaplump[1] = SHORT(wadblockmaplump[1]);
   blockmaplump[2] = (long)(SHORT(wadblockmaplump[2])) & 0xffff;
   blockmaplump[3] = (long)(SHORT(wadblockmaplump[3])) & 0xffff;

   for (i=4 ; i<count ; i++)
   {
   short t = SHORT(wadblockmaplump[i]);          // killough 3/1/98
   blockmaplump[i] = t == -1 ? -1l : (long) t & 0xffff;
   }

   Z_Free(wadblockmaplump);

   bmaporgx = blockmaplump[0]<<FRACBITS;
   bmaporgy = blockmaplump[1]<<FRACBITS;
   bmapwidth = blockmaplump[2];
   bmapheight = blockmaplump[3];

   // Clear out mobj rings.
   count = sizeof(*blockrings) * bmapwidth * bmapheight;
   blockrings = Z_Malloc(count, PU_LEVEL, 0);
   memset(blockrings, 0, count);
   blockmap = blockmaplump + 4;
   for(i = 0; i < bmapwidth * bmapheight; i++)
   blockrings[i].next = blockrings[i].prev = (mobj_t*) &blockrings[i];
   } */

/*
 * Load the REJECT data lump.
 */
void P_LoadReject(int lump)
{
    rejectmatrix = W_CacheLumpNum(lump, PU_LEVEL);

    // If no reject matrix is found, issue a warning.
    if(rejectmatrix == NULL)
    {
        Con_Message("P_LoadReject: No REJECT data found.\n");
    }
}

void P_PlaneChanged(sector_t *sector, boolean theCeiling)
{
    int i;
    unsigned long k;
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

#ifdef _DEBUG
void P_PrintDebugMapData(void)
{
    int i;
    vertex_t *vtx;
    sector_t *sec;
    subsector_t *ss;
    line_t  *li;
    seg_t   *seg;
    side_t  *si;
    thing_t  *th;
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
        Con_Printf("ceilingpic(%i)=\"%s\" light=%i type=%i tag=%i\n",
                     sec->ceilingpic, W_CacheLumpNum(sec->ceilingpic, PU_GETNAME),
                     sec->lightlevel, sec->special, sec->tag);
    }

    Con_Printf("SSECTORS:\n");
    for(i = 0; i < numsubsectors; i++)
    {
        ss = SUBSECTOR_PTR(i);
        Con_Printf("numlines=%i firstline%i\n", ss->linecount, ss->firstline);
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
        Con_Printf("v1=%i v2=%i flags=%i type=%i tag=%i frontside=%i backside=%i\n",
                     GET_VERTEX_IDX(li->v1), GET_VERTEX_IDX(li->v2),
                     li->flags, li->special, li->tag, li->sidenum[0],
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

    Con_Printf("THINGS:\n");
    for(i = 0; i < numthings; i++)
    {
        th = THING_PTR(i);
        Con_Printf("x=%i y=%i height=%i angle=%i type=%i options=%i\n",
                    th->x, th->y, th->height, th->angle, th->type, th->options);
    }
}
#endif

void P_LoadMap(int mapLumpStartNum, int glLumpStartNum, char *levelId)
{
    numvertexes = 0;
    numsubsectors = 0;
    numthings = 0;
    numsectors = 0;
    numnodes = 0;
    numsides = 0;
    numlines = 0;
    numsegs = 0;

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

    P_LoadMapData(mlThings, mapLumpStartNum, glLumpStartNum);

    P_LoadMapData(mlSegs, mapLumpStartNum, glLumpStartNum);
    P_LoadMapData(mlSSectors, mapLumpStartNum, glLumpStartNum);
    P_LoadMapData(mlNodes, mapLumpStartNum, glLumpStartNum);

    //P_PrintDebugMapData();

    // Must be called before we go any further
    P_CheckLevel(levelId, false);

    // Dont need this stuff anymore
    free(missingFronts);
    P_FreeBadTexList();

    // Must be called before any mobjs are spawned.
    Con_Message("Init links\n");
    R_SetupLevel(levelId, DDSLF_INIT_LINKS);

    Con_Message("LoadReject\n");
    P_LoadReject(mapLumpStartNum + ML_REJECT);
    Con_Message("Reject Loaded\n");

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
                case idtVertexes:
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
                    oldNum = numthings;
                    newNum = numthings+= elements;

                    if(oldNum > 0)
                        things = Z_Realloc(things, numthings * sizeof(thing_t), PU_LEVEL);
                    else
                        things = Z_Malloc(numthings * sizeof(thing_t), PU_LEVEL, 0);

                    memset(things + oldNum, 0, elements * sizeof(thing_t));

                    structure = (byte *)(things + oldNum);

                    // Call the game's setup routine
                    if(gx.SetupForThings)
                        gx.SetupForThings(elements);
                    break;

                case idtLineDefs:
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

                case idtSideDefs:
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

                case idtSegs:
                    // Segs are read into a temporary buffer before processing
                    oldNum = numsegs;
                    newNum = numsegs+= elements;

                    if(oldNum > 0)
                    {
                        segs = Z_Realloc(segs, numsegs * sizeof(seg_t), PU_LEVEL);
                        segstemp = Z_Realloc(segstemp, numsegs * sizeof(segtmp_t), PU_STATIC);
                    }
                    else
                    {
                        segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, 0);
                        segstemp = Z_Malloc(numsegs * sizeof(seg_t), PU_STATIC, 0);
                    }

                    memset(segs + oldNum, 0, elements * sizeof(seg_t));
                    memset(segstemp + oldNum, 0, elements * sizeof(segtmp_t));

                    structure = (byte *)(segstemp + oldNum);
                    break;

                case idtSSectors:
                    oldNum = numsubsectors;
                    newNum = numsubsectors+= elements;

                    if(oldNum > 0)
                        subsectors = Z_Realloc(subsectors, numsubsectors * sizeof(subsector_t), PU_LEVEL);
                    else
                        subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_LEVEL, 0);

                    memset(subsectors + oldNum, 0, elements * sizeof(subsector_t));

                    structure = (byte *)(subsectors + oldNum);
                    break;

                case idtNodes:
                    oldNum = numnodes;
                    newNum = numnodes+= elements;

                    if(oldNum > 0)
                        nodes = Z_Realloc(nodes, numnodes * sizeof(node_t), PU_LEVEL);
                    else
                        nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, 0);

                    memset(nodes + oldNum, 0, elements * sizeof(node_t));

                    structure = (byte *)(nodes + oldNum);
                    break;

                case idtSectors:
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
            LumpInfo[i].func(structure, &(internalMapDataStructInfo[internalType]),
                               (mapLumps[i])+dataOffset, elmSize, elements,
                               dataVersion, numValues, dataTypes);
            // Perform any additional processing required
            switch(LumpInfo[i].dataType)
            {
                case idtSegs:
                    P_ProcessSegs(dataVersion);
                    break;

                case idtLineDefs:
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
            Con_Message("   Unknown %s \"%s\"\n", (badTexNames[i].planeTex)?
                        "Flat" : "Texture", badTexNames[i].name);
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

    sector_t *sec;
    subsector_t *ss;
    seg_t  *seg;
    fixed_t bbox[4];

    Con_Message(" Sector look up\n");
    // look up sector number for each subsector
    ss = SUBSECTOR_PTR(0);
    for(i = numsubsectors -1; i >= 0; --i, ss++)
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
        sec->seqType = SEQTYPE_STONE;
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

    for(i = numsectors, sec = SECTOR_PTR(0); i ; --i, sec++)
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
        else // stops any wayward line specials from misbehaving
            sec->tag = 0;

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


void P_ReadValue(const void *structure, const structmember_t *member,
                        byte *src, int size, int flags)
{
    void    *dest = (void *) (((byte *)structure) + member->offset);

    switch(member->dataType)
    {
        case MDT_BYTE:
            *(byte *) dest = *src;
            break;

        case MDT_SHORT:
            switch(size)
            {
                case 2:
                    if(flags & DT_UNSIGNED)
                    {
                        if(flags & DT_FRACBITS)
                            *(short *) dest = USHORT(*((short*)(src))) << FRACBITS;
                        else
                            *(short *) dest = USHORT(*((short*)(src)));
                    }
                    else
                    {
                        if(flags & DT_FRACBITS)
                            *(short *) dest = SHORT(*((short*)(src))) << FRACBITS;
                        else
                            *(short *) dest = SHORT(*((short*)(src)));
                    }
                    break;

                case 8:
                    if(flags & DT_TEXTURE)
                    {
                        *(short *) dest = P_CheckTexture((char*)((long long*)(src)), false);
                    }
                    else if(flags & DT_FLAT)
                    {
                        *(short *) dest = P_CheckTexture((char*)((long long*)(src)), true);
                    }
                    break;
             }
             break;

        case MDT_FIXEDT:
            switch(size) // Number of src bytes
            {
                case 2:
                    if(flags & DT_UNSIGNED)
                    {
                        if(flags & DT_FRACBITS)
                            *(fixed_t *) dest = USHORT(*((short*)(src))) << FRACBITS;
                        else
                            *(fixed_t *) dest = USHORT(*((short*)(src)));
                    }
                    else
                    {
                        if(flags & DT_FRACBITS)
                            *(fixed_t *) dest = SHORT(*((short*)(src))) << FRACBITS;
                        else
                            *(fixed_t *) dest = SHORT(*((short*)(src)));
                    }
                    break;

                case 4:
                    if(flags & DT_UNSIGNED)
                        *(fixed_t *) dest = ULONG(*((long*)(src)));
                    else
                        *(fixed_t *) dest = LONG(*((long*)(src)));
                    break;

                default:
                    break;
            }
            break;

        case MDT_ULONG:
            switch(size) // Number of src bytes
            {
                case 2:
                    if(flags & DT_UNSIGNED)
                    {
                        if(flags & DT_FRACBITS)
                            *(unsigned long *) dest = USHORT(*((short*)(src))) << FRACBITS;
                        else
                            *(unsigned long *) dest = USHORT(*((short*)(src)));
                    }
                    else
                    {
                        if(flags & DT_FRACBITS)
                            *(unsigned long *) dest = SHORT(*((short*)(src))) << FRACBITS;
                        else
                            *(unsigned long *) dest = SHORT(*((short*)(src)));
                    }
                    break;

                case 4:
                    if(flags & DT_UNSIGNED)
                        *(unsigned long *) dest = ULONG(*((long*)(src)));
                    else
                        *(unsigned long *) dest = LONG(*((long*)(src)));
                    break;

                default:
                    break;
            }
            break;

        case MDT_INT:
            switch(size) // Number of src bytes
            {
                case 2:
                    if(flags & DT_UNSIGNED)
                    {
                        if(flags & DT_FRACBITS)
                            *(int *) dest = USHORT(*((short*)(src))) << FRACBITS;
                        else
                            *(int *) dest = USHORT(*((short*)(src)));
                    }
                    else
                    {
                        if(flags & DT_NOINDEX)
                        {
                            unsigned short num = SHORT(*((short*)(src)));

                            *(int *) dest = NO_INDEX;

                            if(num != NO_INDEX)
                                *(int *) dest = num;
                        }
                        else
                        {
                            if(flags & DT_FRACBITS)
                                *(int *) dest = SHORT(*((short*)(src))) << FRACBITS;
                            else
                                *(int *) dest = SHORT(*((short*)(src)));
                        }
                    }
                    break;

                case 4:
                    if(flags & DT_UNSIGNED)
                        *(int *) dest = ULONG(*((long*)(src)));
                    else
                        *(int *) dest = LONG(*((long*)(src)));
                    break;

                default:
                    break;
            }
            break;
        default:
            Con_Error("Look stupid - define the data type first!");
            break;
    }
}

//
// FIXME: 32bit/64bit pointer sizes
// Shame we can't do arithmetic on void pointers in C/C++
//
static void P_ReadBinaryMapData(byte *structure, const structinfo_t *idf, byte *buffer, size_t elmSize,
                           int elements, int version, int values, const datatype_t *types)
{
    int     i = 0, k;
    int     blocklimit;
    size_t  structSize = idf->size;

    Con_Message("Loading %d (%i elements) ver %i vals %i...\n", sizeof(fixed_t), elements, version, values);

    // Not very pretty to look at but it IS pretty quick :-)
    blocklimit = (elements / 8) * 8;
    while(i < blocklimit)
    {
        {
            for(k = values-1; k>= 0; k--)
                P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                            buffer + types[k].offset, types[k].size, types[k].flags);
            buffer += elmSize;
            structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                            buffer + types[k].offset, types[k].size, types[k].flags);
            buffer += elmSize;
            structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                            buffer + types[k].offset, types[k].size, types[k].flags);
            buffer += elmSize;
            structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                            buffer + types[k].offset, types[k].size, types[k].flags);
            buffer += elmSize;
            structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                            buffer + types[k].offset, types[k].size, types[k].flags);
            buffer += elmSize;
            structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                            buffer + types[k].offset, types[k].size, types[k].flags);
            buffer += elmSize;
            structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                            buffer + types[k].offset, types[k].size, types[k].flags);
            buffer += elmSize;
            structure += structSize;
        }
        {
            for(k = values-1; k>= 0; k--)
                P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                            buffer + types[k].offset, types[k].size, types[k].flags);
            buffer += elmSize;
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
                        P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                                    buffer + types[k].offset, types[k].size, types[k].flags);
                    buffer += elmSize;
                    structure += structSize;
                }
                ++i;
            case 6:
                {
                    for(k = values-1; k>= 0; k--)
                        P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                                    buffer + types[k].offset, types[k].size, types[k].flags);
                    buffer += elmSize;
                    structure += structSize;
                }
                ++i;
            case 5:
                {
                    for(k = values-1; k>= 0; k--)
                        P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                                    buffer + types[k].offset, types[k].size, types[k].flags);
                    buffer += elmSize;
                    structure += structSize;
                }
                ++i;
            case 4:
                {
                    for(k = values-1; k>= 0; k--)
                        P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                                    buffer + types[k].offset, types[k].size, types[k].flags);
                    buffer += elmSize;
                    structure += structSize;
                }
                ++i;
            case 3:
                {
                    for(k = values-1; k>= 0; k--)
                        P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                                    buffer + types[k].offset, types[k].size, types[k].flags);
                    buffer += elmSize;
                    structure += structSize;
                }
                ++i;
            case 2:
                {
                    for(k = values-1; k>= 0; k--)
                        P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                                    buffer + types[k].offset, types[k].size, types[k].flags);
                    buffer += elmSize;
                    structure += structSize;
                }
                ++i;
            case 1:
                {
                    for(k = values-1; k>= 0; k--)
                        P_ReadValue(structure, &((idf)->members[types[k].valueid]),
                                    buffer + types[k].offset, types[k].size, types[k].flags);
                    buffer += elmSize;
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

    segtmp_t *ml;

    li = SEG_PTR(0);
    ml = segstemp;

    for(i = 0; i < numsegs; i++, li++, ml++)
    {
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
static void P_ReadLineDefs(byte *structure, const structinfo_t *idf, byte *buffer, size_t elmsize,
                           int elements, int version, int values, const datatype_t *types)
{
    int     i;
    maplinedef_t *mld;
    maplinedefhex_t *mldhex;
    line_t *ld;

    Con_Message("Loading Linedefs (%i) ver %i...\n", elements, version);

    ld = LINE_PTR(0);
    mld = (maplinedef_t *) buffer;
    mldhex = (maplinedefhex_t *) buffer;
    switch(version)
    {
        case 1: // DOOM format
            for(i = 0; i < numlines; i++, mld++, mldhex++, ld++)
            {
                ld->flags = SHORT(mld->flags);
                ld->special = SHORT(mld->special);
                ld->tag = SHORT(mld->tag);

                ld->v1 = VERTEX_PTR(SHORT(mld->v1));
                ld->v2 = VERTEX_PTR(SHORT(mld->v2));

                P_SetLineSideNum(&ld->sidenum[0], SHORT(mld->sidenum[0]));
                P_SetLineSideNum(&ld->sidenum[1], SHORT(mld->sidenum[1]));
            }
            break;

        case 2: // HEXEN format
            for(i = 0; i < numlines; i++, mld++, mldhex++, ld++)
            {
                ld->flags = SHORT(mldhex->flags);

                ld->special = mldhex->special;
                ld->arg1 = mldhex->arg1;
                ld->arg2 = mldhex->arg2;
                ld->arg3 = mldhex->arg3;
                ld->arg4 = mldhex->arg4;
                ld->arg5 = mldhex->arg5;

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
static void P_CompileSideSpecialsList(void)
{
    int i;
    line_t *ld;

    ld = LINE_PTR(0);
    for(i = numlines; i ; i--, ld++)
    {
        if(ld->sidenum[0] != NO_INDEX && ld->special)
            sidespecials[ld->sidenum[0]] = ld->special;
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

    ld = LINE_PTR(0);
    numUniqueLines = 0;
    for(i = numlines -1; i >= 0; --i, ld++)
    {
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
static void P_ReadSideDefs(byte *structure, const structinfo_t *idf, byte *buffer, size_t elmsize,
                           int elements, int version, int values, const datatype_t *types)
{
    int     i, index;
    mapsidedef_t *msd;
    side_t *sd;

    Con_Message("Loading Sidedefs (%i) ver %i...\n", elements, version);

    sd = SIDE_PTR(0);
    msd = (mapsidedef_t *) buffer;
 /*   for(i = numsides -1; i >= 0; --i, msd++, sd++)
    {
        // There may be bogus sector indices here.
        index = SHORT(msd->sector);
        if(index >= 0 && index < numsectors)
            sd->sector = &sectors[index];
    }*/
}

/*
 * MUST be called after Linedefs are loaded.
 * This allows the texture names to be overloaded depending
 * on the special of the linedef (BOOM support)
 *
 * TODO: Remove game specifc stuff. The games must be allowed
 *       to handle what happens with overloaded texture names.
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
    sd = SIDE_PTR(0);
    for(i = numsides -1; i >= 0; --i, msd++, sd++)
    {
        index = SHORT(msd->sector);
        if(index >= 0 && index < numsectors)
            sd->sector = SECTOR_PTR(index);
        sd->textureoffset = SHORT(msd->textureoffset) << FRACBITS;
        sd->rowoffset = SHORT(msd->rowoffset) << FRACBITS;
#if __JDOOM__
        switch(sidespecials[i])
        {
            case 242:
            case 260:
                // A BOOM, sidedef texture overload which we don't support yet.
                // Will be resolved in P_CheckTexture() when implemented
                // and we WONT use a special dependant block to determine them.
                // Once this stuff is all in and working I'll restructure the
                // map loading code once and for all.
                sd->toptexture = -1;
                sd->bottomtexture = -1;
                sd->midtexture = -1;
                Con_Message("%d BOOM overloaded sidedef (unsupported)\n",i);
                break;

            default:
#endif
                sd->toptexture = P_CheckTexture(msd->toptexture, false);
                sd->bottomtexture = P_CheckTexture(msd->bottomtexture, false);
                sd->midtexture = P_CheckTexture(msd->midtexture, false);
#if __JDOOM__
                break;
        }
#endif
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
int P_CheckTexture(char *name, boolean planeTex)
{
    int i;
    int id;
    char namet[9];
    boolean known = false;

    if(planeTex)
        id = R_FlatNumForName(name);
    else
        id = R_TextureNumForName(name);

    // A bad texture name?
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
                    known = true;
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
 * the data item is etc. Most of this can be moved to a DED
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
                    stiptr->verInfo[index].values[0].valueid = 1;
                    stiptr->verInfo[index].values[0].flags = 0;
                    stiptr->verInfo[index].values[0].size =  2;
                    stiptr->verInfo[index].values[0].offset = 0;
                    // y
                    stiptr->verInfo[index].values[1].valueid = 2;
                    stiptr->verInfo[index].values[1].flags = 0;
                    stiptr->verInfo[index].values[1].size =  2;
                    stiptr->verInfo[index].values[1].offset = 2;
                    // angle
                    stiptr->verInfo[index].values[2].valueid = 4;
                    stiptr->verInfo[index].values[2].flags = 0;
                    stiptr->verInfo[index].values[2].size =  2;
                    stiptr->verInfo[index].values[2].offset = 4;
                    // type
                    stiptr->verInfo[index].values[3].valueid = 5;
                    stiptr->verInfo[index].values[3].flags = 0;
                    stiptr->verInfo[index].values[3].size =  2;
                    stiptr->verInfo[index].values[3].offset = 6;
                    // options
                    stiptr->verInfo[index].values[4].valueid = 6;
                    stiptr->verInfo[index].values[4].flags = 0;
                    stiptr->verInfo[index].values[4].size =  2;
                    stiptr->verInfo[index].values[4].offset = 8;
                }
                else            // HEXEN format
                {
                    *mlptr = 20;
                    stiptr->verInfo[index].numValues = 13;
                    stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 13, PU_STATIC, 0);
                    // tid
                    stiptr->verInfo[index].values[0].valueid = 0;
                    stiptr->verInfo[index].values[0].flags = 0;
                    stiptr->verInfo[index].values[0].size =  2;
                    stiptr->verInfo[index].values[0].offset = 0;
                    // x
                    stiptr->verInfo[index].values[1].valueid = 1;
                    stiptr->verInfo[index].values[1].flags = 0;
                    stiptr->verInfo[index].values[1].size =  2;
                    stiptr->verInfo[index].values[1].offset = 2;
                    // y
                    stiptr->verInfo[index].values[2].valueid = 2;
                    stiptr->verInfo[index].values[2].flags = 0;
                    stiptr->verInfo[index].values[2].size =  2;
                    stiptr->verInfo[index].values[2].offset = 4;
                    // height
                    stiptr->verInfo[index].values[3].valueid = 3;
                    stiptr->verInfo[index].values[3].flags = 0;
                    stiptr->verInfo[index].values[3].size =  2;
                    stiptr->verInfo[index].values[3].offset = 6;
                    // angle
                    stiptr->verInfo[index].values[4].valueid = 4;
                    stiptr->verInfo[index].values[4].flags = 0;
                    stiptr->verInfo[index].values[4].size =  2;
                    stiptr->verInfo[index].values[4].offset = 8;
                    // type
                    stiptr->verInfo[index].values[5].valueid = 5;
                    stiptr->verInfo[index].values[5].flags = 0;
                    stiptr->verInfo[index].values[5].size =  2;
                    stiptr->verInfo[index].values[5].offset = 10;
                    // options
                    stiptr->verInfo[index].values[6].valueid = 6;
                    stiptr->verInfo[index].values[6].flags = 0;
                    stiptr->verInfo[index].values[6].size =  2;
                    stiptr->verInfo[index].values[6].offset = 12;
                    // special
                    stiptr->verInfo[index].values[7].valueid = 7;
                    stiptr->verInfo[index].values[7].flags = 0;
                    stiptr->verInfo[index].values[7].size =  1;
                    stiptr->verInfo[index].values[7].offset = 14;
                    // arg1
                    stiptr->verInfo[index].values[8].valueid = 8;
                    stiptr->verInfo[index].values[8].flags = 0;
                    stiptr->verInfo[index].values[8].size =  1;
                    stiptr->verInfo[index].values[8].offset = 15;
                    // arg2
                    stiptr->verInfo[index].values[9].valueid = 9;
                    stiptr->verInfo[index].values[9].flags = 0;
                    stiptr->verInfo[index].values[9].size =  1;
                    stiptr->verInfo[index].values[9].offset = 16;
                    // arg3
                    stiptr->verInfo[index].values[10].valueid = 10;
                    stiptr->verInfo[index].values[10].flags = 0;
                    stiptr->verInfo[index].values[10].size =  1;
                    stiptr->verInfo[index].values[10].offset = 17;
                    // arg4
                    stiptr->verInfo[index].values[11].valueid = 11;
                    stiptr->verInfo[index].values[11].flags = 0;
                    stiptr->verInfo[index].values[11].size =  1;
                    stiptr->verInfo[index].values[11].offset = 18;
                    // arg5
                    stiptr->verInfo[index].values[12].valueid = 12;
                    stiptr->verInfo[index].values[12].flags = 0;
                    stiptr->verInfo[index].values[12].size =  1;
                    stiptr->verInfo[index].values[12].offset = 19;
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
                stiptr->verInfo[index].values[0].valueid = 0;
                stiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[0].size =  2;
                stiptr->verInfo[index].values[0].offset = 0;
                // y
                stiptr->verInfo[index].values[1].valueid = 1;
                stiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[1].size =  2;
                stiptr->verInfo[index].values[1].offset = 2;
            }
            else if(lumpClass == mlSegs)
            {
                *mlptr = 12;
                stiptr->verInfo[index].numValues = 6;
                stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 6, PU_STATIC, 0);
                // v1
                stiptr->verInfo[index].values[0].valueid = 0;
                stiptr->verInfo[index].values[0].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[0].size =  2;
                stiptr->verInfo[index].values[0].offset = 0;
                // v2
                stiptr->verInfo[index].values[1].valueid = 1;
                stiptr->verInfo[index].values[1].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[1].size =  2;
                stiptr->verInfo[index].values[1].offset = 2;
                // angle
                stiptr->verInfo[index].values[2].valueid = 2;
                stiptr->verInfo[index].values[2].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[2].size =  2;
                stiptr->verInfo[index].values[2].offset = 4;
                // linedef
                stiptr->verInfo[index].values[3].valueid = 3;
                stiptr->verInfo[index].values[3].flags = 0;
                stiptr->verInfo[index].values[3].size =  2;
                stiptr->verInfo[index].values[3].offset = 6;
                // side
                stiptr->verInfo[index].values[4].valueid = 4;
                stiptr->verInfo[index].values[4].flags = 0;
                stiptr->verInfo[index].values[4].size =  2;
                stiptr->verInfo[index].values[4].offset = 8;
                // offset
                stiptr->verInfo[index].values[5].valueid = 5;
                stiptr->verInfo[index].values[5].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[5].size =  2;
                stiptr->verInfo[index].values[5].offset = 10;
            }
            else if(lumpClass == mlSSectors)
            {
                *mlptr = 4;
                stiptr->verInfo[index].numValues = 2;
                stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);

                stiptr->verInfo[index].values[0].valueid = 0;
                stiptr->verInfo[index].values[0].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[0].size =  2;
                stiptr->verInfo[index].values[0].offset = 0;

                stiptr->verInfo[index].values[1].valueid = 1;
                stiptr->verInfo[index].values[1].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[1].size =  2;
                stiptr->verInfo[index].values[1].offset = 2;
            }
            else if(lumpClass == mlNodes)
            {
                *mlptr = 28;
                stiptr->verInfo[index].numValues = 14;
                stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 14, PU_STATIC, 0);
                // x
                stiptr->verInfo[index].values[0].valueid = 0;
                stiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[0].size =  2;
                stiptr->verInfo[index].values[0].offset = 0;
                // y
                stiptr->verInfo[index].values[1].valueid = 1;
                stiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[1].size =  2;
                stiptr->verInfo[index].values[1].offset = 2;
                // dx
                stiptr->verInfo[index].values[2].valueid = 2;
                stiptr->verInfo[index].values[2].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[2].size =  2;
                stiptr->verInfo[index].values[2].offset = 4;
                // dy
                stiptr->verInfo[index].values[3].valueid = 3;
                stiptr->verInfo[index].values[3].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[3].size =  2;
                stiptr->verInfo[index].values[3].offset = 6;
                // bbox[0][0]
                stiptr->verInfo[index].values[4].valueid = 4;
                stiptr->verInfo[index].values[4].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[4].size =  2;
                stiptr->verInfo[index].values[4].offset = 8;
                // bbox[0][1]
                stiptr->verInfo[index].values[5].valueid = 5;
                stiptr->verInfo[index].values[5].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[5].size =  2;
                stiptr->verInfo[index].values[5].offset = 10;
                // bbox[0][2]
                stiptr->verInfo[index].values[6].valueid = 6;
                stiptr->verInfo[index].values[6].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[6].size =  2;
                stiptr->verInfo[index].values[6].offset = 12;
                // bbox[0][3]
                stiptr->verInfo[index].values[7].valueid = 7;
                stiptr->verInfo[index].values[7].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[7].size =  2;
                stiptr->verInfo[index].values[7].offset = 14;
                // bbox[1][0]
                stiptr->verInfo[index].values[8].valueid = 8;
                stiptr->verInfo[index].values[8].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[8].size =  2;
                stiptr->verInfo[index].values[8].offset = 16;
                // bbox[1][1]
                stiptr->verInfo[index].values[9].valueid = 9;
                stiptr->verInfo[index].values[9].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[9].size =  2;
                stiptr->verInfo[index].values[9].offset = 18;
                // bbox[1][2]
                stiptr->verInfo[index].values[10].valueid = 10;
                stiptr->verInfo[index].values[10].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[10].size =  2;
                stiptr->verInfo[index].values[10].offset = 20;
                // bbox[1][3]
                stiptr->verInfo[index].values[11].valueid = 11;
                stiptr->verInfo[index].values[11].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[11].size =  2;
                stiptr->verInfo[index].values[11].offset = 22;
                // children[0]
                stiptr->verInfo[index].values[12].valueid = 12;
                stiptr->verInfo[index].values[12].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[12].size =  2;
                stiptr->verInfo[index].values[12].offset = 24;
                // children[1]
                stiptr->verInfo[index].values[13].valueid = 13;
                stiptr->verInfo[index].values[13].flags = DT_UNSIGNED;
                stiptr->verInfo[index].values[13].size =  2;
                stiptr->verInfo[index].values[13].offset = 26;
            }
            else if(lumpClass == mlSectors)
            {
                *mlptr = 26;
                stiptr->verInfo[index].numValues = 7;
                stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 7, PU_STATIC, 0);
                // floor height
                stiptr->verInfo[index].values[0].valueid = 0;
                stiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[0].size =  2;
                stiptr->verInfo[index].values[0].offset = 0;
                // ceiling height
                stiptr->verInfo[index].values[1].valueid = 1;
                stiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[1].size =  2;
                stiptr->verInfo[index].values[1].offset = 2;
                // floor pic
                stiptr->verInfo[index].values[2].valueid = 2;
                stiptr->verInfo[index].values[2].flags = DT_FLAT;
                stiptr->verInfo[index].values[2].size =  8;
                stiptr->verInfo[index].values[2].offset = 4;
                // ceiling pic
                stiptr->verInfo[index].values[3].valueid = 3;
                stiptr->verInfo[index].values[3].flags = DT_FLAT;
                stiptr->verInfo[index].values[3].size =  8;
                stiptr->verInfo[index].values[3].offset = 12;
                // light level
                stiptr->verInfo[index].values[4].valueid = 4;
                stiptr->verInfo[index].values[4].flags = 0;
                stiptr->verInfo[index].values[4].size =  2;
                stiptr->verInfo[index].values[4].offset = 20;
                // special
                stiptr->verInfo[index].values[5].valueid = 5;
                stiptr->verInfo[index].values[5].flags = 0;
                stiptr->verInfo[index].values[5].size =  2;
                stiptr->verInfo[index].values[5].offset = 22;
                // tag
                stiptr->verInfo[index].values[6].valueid = 6;
                stiptr->verInfo[index].values[6].flags = 0;
                stiptr->verInfo[index].values[6].size =  2;
                stiptr->verInfo[index].values[6].offset = 24;
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
                    glstiptr->verInfo[index].values[0].valueid = 0;
                    glstiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[0].size =  2;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    // y
                    glstiptr->verInfo[index].values[1].valueid = 1;
                    glstiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[1].size =  2;
                    glstiptr->verInfo[index].values[1].offset = 2;
                }
                else
                {
                    *glptr = 8;
                    glstiptr->verInfo[index].numValues = 2;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);
                    // x
                    glstiptr->verInfo[index].values[0].valueid = 0;
                    glstiptr->verInfo[index].values[0].flags = 0;
                    glstiptr->verInfo[index].values[0].size =  4;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    // y
                    glstiptr->verInfo[index].values[1].valueid = 1;
                    glstiptr->verInfo[index].values[1].flags = 0;
                    glstiptr->verInfo[index].values[1].size =  4;
                    glstiptr->verInfo[index].values[1].offset = 4;
                }
            }
            else if(lumpClass == glSegs)
            {
                if(glver == 1)
                {
                    *glptr = 8;
                    glstiptr->verInfo[index].numValues = 4;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 4, PU_STATIC, 0);
                    // v1
                    glstiptr->verInfo[index].values[0].valueid = 0;
                    glstiptr->verInfo[index].values[0].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[0].size =  2;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    // v2
                    glstiptr->verInfo[index].values[1].valueid = 1;
                    glstiptr->verInfo[index].values[1].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[1].size =  2;
                    glstiptr->verInfo[index].values[1].offset = 2;
                    // linedef
                    glstiptr->verInfo[index].values[2].valueid = 3;
                    glstiptr->verInfo[index].values[2].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[2].size =  2;
                    glstiptr->verInfo[index].values[2].offset = 4;
                    // side
                    glstiptr->verInfo[index].values[3].valueid = 4;
                    glstiptr->verInfo[index].values[3].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[3].size =  2;
                    glstiptr->verInfo[index].values[3].offset = 6;
                }
                else if(glver == 4)
                {
                    *glptr = 0;        // Unsupported atm
                }
                else
                {
                    *glptr = 12;
                    glstiptr->verInfo[index].numValues = 4;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 4, PU_STATIC, 0);
                    // v1
                    glstiptr->verInfo[index].values[0].valueid = 0;
                    glstiptr->verInfo[index].values[0].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[0].size =  4;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    // v2
                    glstiptr->verInfo[index].values[1].valueid = 1;
                    glstiptr->verInfo[index].values[1].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[1].size =  4;
                    glstiptr->verInfo[index].values[1].offset = 4;
                    // linedef
                    glstiptr->verInfo[index].values[2].valueid = 3;
                    glstiptr->verInfo[index].values[2].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[2].size =  2;
                    glstiptr->verInfo[index].values[2].offset = 8;
                    // side
                    glstiptr->verInfo[index].values[3].valueid = 4;
                    glstiptr->verInfo[index].values[3].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[3].size =  2;
                    glstiptr->verInfo[index].values[3].offset = 10;
                }
            }
            else if(lumpClass == glSSects)
            {
                if(glver == 1)
                {
                    *glptr = 4;
                    glstiptr->verInfo[index].numValues = 2;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);

                    glstiptr->verInfo[index].values[0].valueid = 0;
                    glstiptr->verInfo[index].values[0].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[0].size =  2;
                    glstiptr->verInfo[index].values[0].offset = 0;

                    glstiptr->verInfo[index].values[1].valueid = 1;
                    glstiptr->verInfo[index].values[1].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[1].size =  2;
                    glstiptr->verInfo[index].values[1].offset = 2;
                }
                else
                {
                    *glptr = 8;
                    glstiptr->verInfo[index].numValues = 2;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);

                    glstiptr->verInfo[index].values[0].valueid = 0;
                    glstiptr->verInfo[index].values[0].flags = 0;
                    glstiptr->verInfo[index].values[0].size =  4;
                    glstiptr->verInfo[index].values[0].offset = 0;

                    glstiptr->verInfo[index].values[1].valueid = 1;
                    glstiptr->verInfo[index].values[1].flags = 0;
                    glstiptr->verInfo[index].values[1].size =  4;
                    glstiptr->verInfo[index].values[1].offset = 4;
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
                    glstiptr->verInfo[index].values[0].valueid = 0;
                    glstiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[0].size =  2;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    // y
                    glstiptr->verInfo[index].values[1].valueid = 1;
                    glstiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[1].size =  2;
                    glstiptr->verInfo[index].values[1].offset = 2;
                    // dx
                    glstiptr->verInfo[index].values[2].valueid = 2;
                    glstiptr->verInfo[index].values[2].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[2].size =  2;
                    glstiptr->verInfo[index].values[2].offset = 4;
                    // dy
                    glstiptr->verInfo[index].values[3].valueid = 3;
                    glstiptr->verInfo[index].values[3].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[3].size =  2;
                    glstiptr->verInfo[index].values[3].offset = 6;
                    // bbox[0][0]
                    glstiptr->verInfo[index].values[4].valueid = 4;
                    glstiptr->verInfo[index].values[4].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[4].size =  2;
                    glstiptr->verInfo[index].values[4].offset = 8;
                    // bbox[0][1]
                    glstiptr->verInfo[index].values[5].valueid = 5;
                    glstiptr->verInfo[index].values[5].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[5].size =  2;
                    glstiptr->verInfo[index].values[5].offset = 10;
                    // bbox[0][2]
                    glstiptr->verInfo[index].values[6].valueid = 6;
                    glstiptr->verInfo[index].values[6].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[6].size =  2;
                    glstiptr->verInfo[index].values[6].offset = 12;
                    // bbox[0][3]
                    glstiptr->verInfo[index].values[7].valueid = 7;
                    glstiptr->verInfo[index].values[7].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[7].size =  2;
                    glstiptr->verInfo[index].values[7].offset = 14;
                    // bbox[1][0]
                    glstiptr->verInfo[index].values[8].valueid = 8;
                    glstiptr->verInfo[index].values[8].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[8].size =  2;
                    glstiptr->verInfo[index].values[8].offset = 16;
                    // bbox[1][1]
                    glstiptr->verInfo[index].values[9].valueid = 9;
                    glstiptr->verInfo[index].values[9].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[9].size =  2;
                    glstiptr->verInfo[index].values[9].offset = 18;
                    // bbox[1][2]
                    glstiptr->verInfo[index].values[10].valueid = 10;
                    glstiptr->verInfo[index].values[10].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[10].size =  2;
                    glstiptr->verInfo[index].values[10].offset = 20;
                    // bbox[1][3]
                    glstiptr->verInfo[index].values[11].valueid = 11;
                    glstiptr->verInfo[index].values[11].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[11].size =  2;
                    glstiptr->verInfo[index].values[11].offset = 22;
                    // children[0]
                    glstiptr->verInfo[index].values[12].valueid = 12;
                    glstiptr->verInfo[index].values[12].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[12].size =  2;
                    glstiptr->verInfo[index].values[12].offset = 24;
                    // children[1]
                    glstiptr->verInfo[index].values[13].valueid = 13;
                    glstiptr->verInfo[index].values[13].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[13].size =  2;
                    glstiptr->verInfo[index].values[13].offset = 26;
                }
                else
                {
                    *glptr = 32;
                    glstiptr->verInfo[index].numValues = 14;
                    glstiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 14, PU_STATIC, 0);
                    // x
                    glstiptr->verInfo[index].values[0].valueid = 0;
                    glstiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[0].size =  2;
                    glstiptr->verInfo[index].values[0].offset = 0;
                    // y
                    glstiptr->verInfo[index].values[1].valueid = 1;
                    glstiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[1].size =  2;
                    glstiptr->verInfo[index].values[1].offset = 2;
                    // dx
                    glstiptr->verInfo[index].values[2].valueid = 2;
                    glstiptr->verInfo[index].values[2].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[2].size =  2;
                    glstiptr->verInfo[index].values[2].offset = 4;
                    // dy
                    glstiptr->verInfo[index].values[3].valueid = 3;
                    glstiptr->verInfo[index].values[3].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[3].size =  2;
                    glstiptr->verInfo[index].values[3].offset = 6;
                    // bbox[0][0]
                    glstiptr->verInfo[index].values[4].valueid = 4;
                    glstiptr->verInfo[index].values[4].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[4].size =  2;
                    glstiptr->verInfo[index].values[4].offset = 8;
                    // bbox[0][1]
                    glstiptr->verInfo[index].values[5].valueid = 5;
                    glstiptr->verInfo[index].values[5].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[5].size =  2;
                    glstiptr->verInfo[index].values[5].offset = 10;
                    // bbox[0][2]
                    glstiptr->verInfo[index].values[6].valueid = 6;
                    glstiptr->verInfo[index].values[6].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[6].size =  2;
                    glstiptr->verInfo[index].values[6].offset = 12;
                    // bbox[0][3]
                    glstiptr->verInfo[index].values[7].valueid = 7;
                    glstiptr->verInfo[index].values[7].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[7].size =  2;
                    glstiptr->verInfo[index].values[7].offset = 14;
                    // bbox[1][0]
                    glstiptr->verInfo[index].values[8].valueid = 8;
                    glstiptr->verInfo[index].values[8].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[8].size =  2;
                    glstiptr->verInfo[index].values[8].offset = 16;
                    // bbox[1][1]
                    glstiptr->verInfo[index].values[9].valueid = 9;
                    glstiptr->verInfo[index].values[9].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[9].size =  2;
                    glstiptr->verInfo[index].values[9].offset = 18;
                    // bbox[1][2]
                    glstiptr->verInfo[index].values[10].valueid = 10;
                    glstiptr->verInfo[index].values[10].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[10].size =  2;
                    glstiptr->verInfo[index].values[10].offset = 20;
                    // bbox[1][3]
                    glstiptr->verInfo[index].values[11].valueid = 11;
                    glstiptr->verInfo[index].values[11].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].values[11].size =  2;
                    glstiptr->verInfo[index].values[11].offset = 22;
                    // children[0]
                    glstiptr->verInfo[index].values[12].valueid = 12;
                    glstiptr->verInfo[index].values[12].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[12].size =  4;
                    glstiptr->verInfo[index].values[12].offset = 24;
                    // children[1]
                    glstiptr->verInfo[index].values[13].valueid = 13;
                    glstiptr->verInfo[index].values[13].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].values[13].size =  4;
                    glstiptr->verInfo[index].values[13].offset = 28;
                }
            }
        }
    }

// vertex_t
    internalMapDataStructInfo[idtVertexes].type = idtVertexes;
    internalMapDataStructInfo[idtVertexes].size = sizeof(vertex_t);
    internalMapDataStructInfo[idtVertexes].nummembers = 2;
    (internalMapDataStructInfo[idtVertexes]).members = Z_Malloc(sizeof(structmember_t) * 2, PU_STATIC, 0);
    // X
    (internalMapDataStructInfo[idtVertexes]).members[0].flags = 0;
    (internalMapDataStructInfo[idtVertexes]).members[0].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtVertexes]).members[0].size = sizeof(((vertex_t*)0)->x);
    (internalMapDataStructInfo[idtVertexes]).members[0].offset = myoffsetof(vertex_t, x,0);
    // Y
    (internalMapDataStructInfo[idtVertexes]).members[1].flags = 0;
    (internalMapDataStructInfo[idtVertexes]).members[1].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtVertexes]).members[1].size = sizeof(((vertex_t*)0)->y);
    (internalMapDataStructInfo[idtVertexes]).members[1].offset = myoffsetof(vertex_t, y,0);

// subsector_t
    internalMapDataStructInfo[idtSSectors].type = idtSSectors;
    internalMapDataStructInfo[idtSSectors].size = sizeof(subsector_t);
    internalMapDataStructInfo[idtSSectors].nummembers = 2;
    (internalMapDataStructInfo[idtSSectors]).members = Z_Malloc(sizeof(structmember_t) * 2, PU_STATIC, 0);
    // linecount
    (internalMapDataStructInfo[idtSSectors]).members[0].flags = 0;
    (internalMapDataStructInfo[idtSSectors]).members[0].dataType = MDT_ULONG;
    (internalMapDataStructInfo[idtSSectors]).members[0].size = sizeof(((subsector_t*)0)->linecount);
    (internalMapDataStructInfo[idtSSectors]).members[0].offset = myoffsetof(subsector_t, linecount,0);
    // firstline
    (internalMapDataStructInfo[idtSSectors]).members[1].flags = 0;
    (internalMapDataStructInfo[idtSSectors]).members[1].dataType = MDT_ULONG;
    (internalMapDataStructInfo[idtSSectors]).members[1].size = sizeof(((subsector_t*)0)->firstline);
    (internalMapDataStructInfo[idtSSectors]).members[1].offset = myoffsetof(subsector_t, firstline,0);

// thing_t
    internalMapDataStructInfo[idtThings].type = idtThings;
    internalMapDataStructInfo[idtThings].size = sizeof(thing_t);
    internalMapDataStructInfo[idtThings].nummembers = 13;
    (internalMapDataStructInfo[idtThings]).members = Z_Malloc(sizeof(structmember_t) * 13, PU_STATIC, 0);
    // tid
    (internalMapDataStructInfo[idtThings]).members[0].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[0].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtThings]).members[0].size = sizeof(((thing_t*)0)->tid);
    (internalMapDataStructInfo[idtThings]).members[0].offset = myoffsetof(thing_t, tid,0);
    // x
    (internalMapDataStructInfo[idtThings]).members[1].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[1].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtThings]).members[1].size = sizeof(((thing_t*)0)->x);
    (internalMapDataStructInfo[idtThings]).members[1].offset = myoffsetof(thing_t, x,0);
    // y
    (internalMapDataStructInfo[idtThings]).members[2].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[2].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtThings]).members[2].size = sizeof(((thing_t*)0)->y);
    (internalMapDataStructInfo[idtThings]).members[2].offset = myoffsetof(thing_t, y,0);
    // height
    (internalMapDataStructInfo[idtThings]).members[3].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[3].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtThings]).members[3].size = sizeof(((thing_t*)0)->height);
    (internalMapDataStructInfo[idtThings]).members[3].offset = myoffsetof(thing_t, height,0);
    // angle
    (internalMapDataStructInfo[idtThings]).members[4].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[4].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtThings]).members[4].size = sizeof(((thing_t*)0)->angle);
    (internalMapDataStructInfo[idtThings]).members[4].offset = myoffsetof(thing_t, angle,0);
    // type
    (internalMapDataStructInfo[idtThings]).members[5].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[5].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtThings]).members[5].size = sizeof(((thing_t*)0)->type);
    (internalMapDataStructInfo[idtThings]).members[5].offset = myoffsetof(thing_t, type,0);
    // options
    (internalMapDataStructInfo[idtThings]).members[6].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[6].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtThings]).members[6].size = sizeof(((thing_t*)0)->options);
    (internalMapDataStructInfo[idtThings]).members[6].offset = myoffsetof(thing_t, options,0);
    // special
    (internalMapDataStructInfo[idtThings]).members[7].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[7].dataType = MDT_BYTE;
    (internalMapDataStructInfo[idtThings]).members[7].size = sizeof(((thing_t*)0)->special);
    (internalMapDataStructInfo[idtThings]).members[7].offset = myoffsetof(thing_t, special,0);
    // arg1
    (internalMapDataStructInfo[idtThings]).members[8].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[8].dataType = MDT_BYTE;
    (internalMapDataStructInfo[idtThings]).members[8].size = sizeof(((thing_t*)0)->arg1);
    (internalMapDataStructInfo[idtThings]).members[8].offset = myoffsetof(thing_t, arg1,0);
    // arg2
    (internalMapDataStructInfo[idtThings]).members[9].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[9].dataType = MDT_BYTE;
    (internalMapDataStructInfo[idtThings]).members[9].size = sizeof(((thing_t*)0)->arg2);
    (internalMapDataStructInfo[idtThings]).members[9].offset = myoffsetof(thing_t, arg2,0);
    // arg3
    (internalMapDataStructInfo[idtThings]).members[10].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[10].dataType = MDT_BYTE;
    (internalMapDataStructInfo[idtThings]).members[10].size = sizeof(((thing_t*)0)->arg3);
    (internalMapDataStructInfo[idtThings]).members[10].offset = myoffsetof(thing_t, arg3,0);
    // arg4
    (internalMapDataStructInfo[idtThings]).members[11].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[11].dataType = MDT_BYTE;
    (internalMapDataStructInfo[idtThings]).members[11].size = sizeof(((thing_t*)0)->arg4);
    (internalMapDataStructInfo[idtThings]).members[11].offset = myoffsetof(thing_t, arg4,0);
    // arg5
    (internalMapDataStructInfo[idtThings]).members[12].flags = 0;
    (internalMapDataStructInfo[idtThings]).members[12].dataType = MDT_BYTE;
    (internalMapDataStructInfo[idtThings]).members[12].size = sizeof(((thing_t*)0)->arg5);
    (internalMapDataStructInfo[idtThings]).members[12].offset = myoffsetof(thing_t, arg5,0);

// sector_t
    internalMapDataStructInfo[idtSectors].type = idtSectors;
    internalMapDataStructInfo[idtSectors].size = sizeof(sector_t);
    internalMapDataStructInfo[idtSectors].nummembers = 7;
    (internalMapDataStructInfo[idtSectors]).members = Z_Malloc(sizeof(structmember_t) * 7, PU_STATIC, 0);
    // floor height
    (internalMapDataStructInfo[idtSectors]).members[0].flags = 0;
    (internalMapDataStructInfo[idtSectors]).members[0].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtSectors]).members[0].size = sizeof(((sector_t*)0)->floorheight);
    (internalMapDataStructInfo[idtSectors]).members[0].offset = myoffsetof(sector_t, floorheight,0);
    // ceiling height
    (internalMapDataStructInfo[idtSectors]).members[1].flags = 0;
    (internalMapDataStructInfo[idtSectors]).members[1].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtSectors]).members[1].size = sizeof(((sector_t*)0)->ceilingheight);
    (internalMapDataStructInfo[idtSectors]).members[1].offset = myoffsetof(sector_t, ceilingheight,0);
    // floor pic
    (internalMapDataStructInfo[idtSectors]).members[2].flags = 0;
    (internalMapDataStructInfo[idtSectors]).members[2].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtSectors]).members[2].size = sizeof(((sector_t*)0)->floorpic);
    (internalMapDataStructInfo[idtSectors]).members[2].offset = myoffsetof(sector_t, floorpic,0);
    // ceiling pic
    (internalMapDataStructInfo[idtSectors]).members[3].flags = 0;
    (internalMapDataStructInfo[idtSectors]).members[3].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtSectors]).members[3].size = sizeof(((sector_t*)0)->ceilingpic);
    (internalMapDataStructInfo[idtSectors]).members[3].offset = myoffsetof(sector_t, ceilingpic,0);
    // light level
    (internalMapDataStructInfo[idtSectors]).members[4].flags = 0;
    (internalMapDataStructInfo[idtSectors]).members[4].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtSectors]).members[4].size = sizeof(((sector_t*)0)->lightlevel);
    (internalMapDataStructInfo[idtSectors]).members[4].offset = myoffsetof(sector_t, lightlevel,0);
    // special
    (internalMapDataStructInfo[idtSectors]).members[5].flags = 0;
    (internalMapDataStructInfo[idtSectors]).members[5].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtSectors]).members[5].size = sizeof(((sector_t*)0)->special);
    (internalMapDataStructInfo[idtSectors]).members[5].offset = myoffsetof(sector_t, special,0);
    // tag
    (internalMapDataStructInfo[idtSectors]).members[6].flags = 0;
    (internalMapDataStructInfo[idtSectors]).members[6].dataType = MDT_SHORT;
    (internalMapDataStructInfo[idtSectors]).members[6].size = sizeof(((sector_t*)0)->tag);
    (internalMapDataStructInfo[idtSectors]).members[6].offset = myoffsetof(sector_t, tag,0);

// node_t
    internalMapDataStructInfo[idtNodes].type = idtNodes;
    internalMapDataStructInfo[idtNodes].size = sizeof(node_t);
    internalMapDataStructInfo[idtNodes].nummembers = 14;
    (internalMapDataStructInfo[idtNodes]).members = Z_Malloc(sizeof(structmember_t) * 14, PU_STATIC, 0);
    // x
    (internalMapDataStructInfo[idtNodes]).members[0].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[0].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[0].size = sizeof(((node_t*)0)->x);
    (internalMapDataStructInfo[idtNodes]).members[0].offset = myoffsetof(node_t, x,0);
    // y
    (internalMapDataStructInfo[idtNodes]).members[1].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[1].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[1].size = sizeof(((node_t*)0)->y);
    (internalMapDataStructInfo[idtNodes]).members[1].offset = myoffsetof(node_t, y,0);
    // dx
    (internalMapDataStructInfo[idtNodes]).members[2].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[2].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[2].size = sizeof(((node_t*)0)->dx);
    (internalMapDataStructInfo[idtNodes]).members[2].offset = myoffsetof(node_t, dx,0);
    // dy
    (internalMapDataStructInfo[idtNodes]).members[3].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[3].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[3].size = sizeof(((node_t*)0)->dy);
    (internalMapDataStructInfo[idtNodes]).members[3].offset = myoffsetof(node_t, dy,0);
    // bbox[0][0]
    (internalMapDataStructInfo[idtNodes]).members[4].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[4].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[4].size = sizeof(((node_t*)0)->bbox[0][0]);
    (internalMapDataStructInfo[idtNodes]).members[4].offset = myoffsetof(node_t, bbox[0][0],0);
    // bbox[0][1]
    (internalMapDataStructInfo[idtNodes]).members[5].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[5].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[5].size = sizeof(((node_t*)0)->bbox[0][1]);
    (internalMapDataStructInfo[idtNodes]).members[5].offset = myoffsetof(node_t, bbox[0][1],0);
    // bbox[0][2]
    (internalMapDataStructInfo[idtNodes]).members[6].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[6].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[6].size = sizeof(((node_t*)0)->bbox[0][2]);
    (internalMapDataStructInfo[idtNodes]).members[6].offset = myoffsetof(node_t, bbox[0][2],0);
    // bbox[0][3]
    (internalMapDataStructInfo[idtNodes]).members[7].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[7].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[7].size = sizeof(((node_t*)0)->bbox[0][3]);
    (internalMapDataStructInfo[idtNodes]).members[7].offset = myoffsetof(node_t, bbox[0][3],0);
    // bbox[1][0]
    (internalMapDataStructInfo[idtNodes]).members[8].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[8].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[8].size = sizeof(((node_t*)0)->bbox[1][0]);
    (internalMapDataStructInfo[idtNodes]).members[8].offset = myoffsetof(node_t, bbox[1][0],0);
    // bbox[1][1]
    (internalMapDataStructInfo[idtNodes]).members[9].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[9].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[9].size = sizeof(((node_t*)0)->bbox[1][1]);
    (internalMapDataStructInfo[idtNodes]).members[9].offset = myoffsetof(node_t, bbox[1][1],0);
    // bbox[1][2]
    (internalMapDataStructInfo[idtNodes]).members[10].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[10].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[10].size = sizeof(((node_t*)0)->bbox[1][2]);
    (internalMapDataStructInfo[idtNodes]).members[10].offset = myoffsetof(node_t, bbox[1][2],0);
    // bbox[1][3]
    (internalMapDataStructInfo[idtNodes]).members[11].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[11].dataType = MDT_FIXEDT;
    (internalMapDataStructInfo[idtNodes]).members[11].size = sizeof(((node_t*)0)->bbox[1][3]);
    (internalMapDataStructInfo[idtNodes]).members[11].offset = myoffsetof(node_t, bbox[1][3],0);
    // children[0]
    (internalMapDataStructInfo[idtNodes]).members[12].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[12].dataType = MDT_INT;
    (internalMapDataStructInfo[idtNodes]).members[12].size = sizeof(((node_t*)0)->children[0]);
    (internalMapDataStructInfo[idtNodes]).members[12].offset = myoffsetof(node_t, children[0],0);
    // children[1]
    (internalMapDataStructInfo[idtNodes]).members[13].flags = 0;
    (internalMapDataStructInfo[idtNodes]).members[13].dataType = MDT_INT;
    (internalMapDataStructInfo[idtNodes]).members[13].size = sizeof(((node_t*)0)->children[1]);
    (internalMapDataStructInfo[idtNodes]).members[13].offset = myoffsetof(node_t, children[1],0);

// segtmp_t
    internalMapDataStructInfo[idtSegs].type = idtSegs;
    internalMapDataStructInfo[idtSegs].size = sizeof(segtmp_t);
    internalMapDataStructInfo[idtSegs].nummembers = 6;
    (internalMapDataStructInfo[idtSegs]).members = Z_Malloc(sizeof(structmember_t) * 6, PU_STATIC, 0);
    // v1
    (internalMapDataStructInfo[idtSegs]).members[0].flags = 0;
    (internalMapDataStructInfo[idtSegs]).members[0].dataType = MDT_INT;
    (internalMapDataStructInfo[idtSegs]).members[0].size = sizeof(((segtmp_t*)0)->v1);
    (internalMapDataStructInfo[idtSegs]).members[0].offset = myoffsetof(segtmp_t, v1,0);
    // v2
    (internalMapDataStructInfo[idtSegs]).members[1].flags = 0;
    (internalMapDataStructInfo[idtSegs]).members[1].dataType = MDT_INT;
    (internalMapDataStructInfo[idtSegs]).members[1].size = sizeof(((segtmp_t*)0)->v2);
    (internalMapDataStructInfo[idtSegs]).members[1].offset = myoffsetof(segtmp_t, v2,0);
    // angle
    (internalMapDataStructInfo[idtSegs]).members[2].flags = 0;
    (internalMapDataStructInfo[idtSegs]).members[2].dataType = MDT_INT;
    (internalMapDataStructInfo[idtSegs]).members[2].size = sizeof(((segtmp_t*)0)->angle);
    (internalMapDataStructInfo[idtSegs]).members[2].offset = myoffsetof(segtmp_t, angle,0);
    // linedef
    (internalMapDataStructInfo[idtSegs]).members[3].flags = 0;
    (internalMapDataStructInfo[idtSegs]).members[3].dataType = MDT_INT;
    (internalMapDataStructInfo[idtSegs]).members[3].size = sizeof(((segtmp_t*)0)->linedef);
    (internalMapDataStructInfo[idtSegs]).members[3].offset = myoffsetof(segtmp_t, linedef,0);
    // side
    (internalMapDataStructInfo[idtSegs]).members[4].flags = 0;
    (internalMapDataStructInfo[idtSegs]).members[4].dataType = MDT_INT;
    (internalMapDataStructInfo[idtSegs]).members[4].size = sizeof(((segtmp_t*)0)->side);
    (internalMapDataStructInfo[idtSegs]).members[4].offset = myoffsetof(segtmp_t, side,0);
    // offset
    (internalMapDataStructInfo[idtSegs]).members[5].flags = 0;
    (internalMapDataStructInfo[idtSegs]).members[5].dataType = MDT_INT;
    (internalMapDataStructInfo[idtSegs]).members[5].size = sizeof(((segtmp_t*)0)->offset);
    (internalMapDataStructInfo[idtSegs]).members[5].offset = myoffsetof(segtmp_t, offset,0);
}

float AccurateDistance(fixed_t dx, fixed_t dy)
{
    float   fx = FIX2FLT(dx), fy = FIX2FLT(dy);

    return (float) sqrt(fx * fx + fy * fy);
}
