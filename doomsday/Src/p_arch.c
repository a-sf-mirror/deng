/* DE1: $Id$
 * Copyright (C) 2006 Jaakko Keränen <jaakko.keranen@iki.fi>
 *                    Daniel Swanson <danij@users.sourceforge.net>
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
 * p_arch.c: Doomsday Archived Map (DAM) reader.
 *
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_system.h"

#include "p_arch.h"
#include "m_bams.h"
#include <math.h>

// MACROS ------------------------------------------------------------------

// map data type flags
#define DT_UNSIGNED 0x01
#define DT_FRACBITS 0x02
#define DT_FLAT     0x04
#define DT_TEXTURE  0x08
#define DT_NOINDEX  0x10

// Internal data types
#define MAPDATA_FORMATS 2

// GL Node versions
#define GLNODE_FORMATS  5

#define ML_SIDEDEFS     3 // TODO: read sidedefs using the generic data code

// TYPES -------------------------------------------------------------------

// Common map format properties
enum {
    DAM_UNKNOWN = -1,
    DAM_NONE,

    // Object/Data types
    DAM_THING,
    DAM_VERTEX,
    DAM_LINE,
    DAM_SIDE,
    DAM_SECTOR,
    DAM_SEG,
    DAM_SUBSECTOR,
    DAM_NODE,
    DAM_MAPBLOCK,
    DAM_SECREJECT,
    DAM_ACSSCRIPT,

    // Object properties
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
    DAM_FLOOR_TEXTURE,
    DAM_CEILING_HEIGHT,
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

typedef struct {
    char* level;
    char* builder;
    char* time;
    char* checksum;
} glbuildinfo_t;

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
// TODO: we still use this struct for texture byte offsets
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

// temporary formats
typedef struct {
    int           v1;
    int           v2;
    int           angle;
    int           linedef;
    int           side;
    int           offset;
} mapseg_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

float AccurateDistance(fixed_t dx, fixed_t dy);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean  ReadMapData(int doClass);
static boolean  DetermineMapDataFormat(void);

static void     P_ReadBinaryMapData(unsigned int startIndex, int dataType, const byte *buffer,
                                size_t elmsize, unsigned int elements,
                                unsigned int values, const datatype_t *types);

static void     P_ReadSideDefTextures(int lump);
static void     P_FinishLineDefs(void);
static void     P_ProcessSegs(int version);

#if _DEBUG
static void     P_PrintDebugMapData(void);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

unsigned int     mapFormat;

// gl nodes
unsigned int     glNodeFormat;
int     firstGLvertex = 0;

// Set to true if glNodeData exists for the level.
boolean glNodeData = false;

enum {
    NO,        // Not required.
    BSPBUILD,  // If we can build nodes we don't require it.
    YES        // MUST be present
};

// types of MAP data structure
// These arrays are temporary. Some of the data will be provided via DED definitions.
maplumpinfo_t mapLumpInfo[] = {
//   lumpname    MD  GL  datatype      lumpclass     required?  precache?
    {NULL,       0, -1,  DAM_UNKNOWN,   LCM_LABEL,      NO,    false},
    {"THINGS",   1, -1,  DAM_THING,     LCM_THINGS,     YES,   false},
    {"LINEDEFS", 2, -1,  DAM_LINE,      LCM_LINEDEFS,   YES,   false},
    {"SIDEDEFS", 3, -1,  DAM_SIDE,      LCM_SIDEDEFS,   YES,   false},
    {"VERTEXES", 4, -1,  DAM_VERTEX,    LCM_VERTEXES,   YES,   false},
    {"SEGS",     5, -1,  DAM_SEG,       LCM_SEGS,       BSPBUILD, false},
    {"SSECTORS", 6, -1,  DAM_SUBSECTOR, LCM_SUBSECTORS,   BSPBUILD, false},
    {"NODES",    7, -1,  DAM_NODE,      LCM_NODES,      BSPBUILD, false},
    {"SECTORS",  8, -1,  DAM_SECTOR,    LCM_SECTORS,    YES,   false},
    {"REJECT",   9, -1,  DAM_SECREJECT, LCM_REJECT,     NO,    false},
    {"BLOCKMAP", 10, -1, DAM_MAPBLOCK,  LCM_BLOCKMAP,   NO,    false},
    {"BEHAVIOR", 11,-1,  DAM_ACSSCRIPT, LCM_BEHAVIOR,   NO,    false},
    {NULL,       -1, 0,  DAM_UNKNOWN,   LCG_LABEL,      NO,    false},
    {"GL_VERT",  -1, 1,  DAM_VERTEX,    LCG_VERTEXES,      NO,    false},
    {"GL_SEGS",  -1, 2,  DAM_SEG,       LCG_SEGS,       NO,    false},
    {"GL_SSECT", -1, 3,  DAM_SUBSECTOR, LCG_SUBSECTORS,     NO,    false},
    {"GL_NODES", -1, 4,  DAM_NODE,      LCG_NODES,      NO,    false},
    {NULL}
};

// Versions of map data structures
mapdataformat_t mapDataFormats[] = {
    {"DOOM", {{1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL},
              {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {-1, NULL}}, true},
    {"HEXEN",{{1, NULL}, {2, NULL}, {2, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL},
              {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}}, true},
    {NULL}
};

// Versions of GL node data structures
glnodeformat_t glNodeFormats[] = {
// Ver String   Label       Verts       Segs        SSects       Nodes     Supported?
    {"V1", {{1, NULL},   {1, NULL},   {2, NULL},   {1, NULL},   {1, NULL}}, true},
    {"V2", {{1, NULL},   {2, "gNd2"}, {2, NULL},   {1, NULL},   {1, NULL}}, true},
    {"V3", {{1, NULL},   {2, "gNd2"}, {3, "gNd3"}, {3, "gNd3"}, {1, NULL}}, false},
    {"V4", {{1, NULL},   {4, "gNd4"}, {4, NULL},   {4, NULL},   {4, NULL}}, false},
    {"V5", {{1, NULL},   {5, "gNd5"}, {5, NULL},   {3, NULL},   {4, NULL}}, true},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mapdatalumpinfo_t* mapDataLumps;
static int numMapDataLumps;

static glbuildinfo_t *glBuilderInfo;

static mapseg_t *segstemp;

// CODE --------------------------------------------------------------------

/*
 * Convert enum constant into a string for error/debug messages.
 */
const char* DAM_Str(int prop)
{
    static char propStr[40];
    struct prop_s {
        int prop;
        const char* str;
    } props[] =
    {
        { DAM_UNKNOWN, "(unknown)" },
        { 0, "(invalid)" },
        { DAM_THING, "DAM_THING" },
        { DAM_VERTEX, "DAM_VERTEX" },
        { DAM_LINE, "DAM_LINE" },
        { DAM_SIDE, "DAM_SIDE" },
        { DAM_SECTOR, "DAM_SECTOR" },
        { DAM_SEG, "DAM_SEG" },
        { DAM_SUBSECTOR, "DAM_SUBSECTOR" },
        { DAM_NODE, "DAM_NODE" },
        { DAM_MAPBLOCK, "DAM_MAPBLOCK" },
        { DAM_SECREJECT, "DAM_SECREJECT" },
        { DAM_ACSSCRIPT, "DAM_ACSSCRIPT" },
        { DAM_X, "DAM_X" },
        { DAM_Y, "DAM_Y" },
        { DAM_DX, "DAM_DX" },
        { DAM_DY, "DAM_DY" },
        { DAM_VERTEX1, "DAM_VERTEX1" },
        { DAM_VERTEX2, "DAM_VERTEX2" },
        { DAM_FLAGS, "DAM_FLAGS" },
        { DAM_SIDE0, "DAM_SIDE0" },
        { DAM_SIDE1, "DAM_SIDE1" },
        { DAM_TEXTURE_OFFSET_X, "DAM_TEXTURE_OFFSET_X" },
        { DAM_TEXTURE_OFFSET_Y, "DAM_TEXTURE_OFFSET_Y" },
        { DAM_TOP_TEXTURE, "DAM_TOP_TEXTURE" },
        { DAM_MIDDLE_TEXTURE, "DAM_MIDDLE_TEXTURE" },
        { DAM_BOTTOM_TEXTURE, "DAM_BOTTOM_TEXTURE" },
        { DAM_FRONT_SECTOR, "DAM_FRONT_SECTOR" },
        { DAM_FLOOR_HEIGHT, "DAM_FLOOR_HEIGHT" },
        { DAM_FLOOR_TEXTURE, "DAM_FLOOR_TEXTURE" },
        { DAM_CEILING_HEIGHT, "DAM_CEILING_HEIGHT" },
        { DAM_CEILING_TEXTURE, "DAM_CEILING_TEXTURE" },
        { DAM_LIGHT_LEVEL, "DAM_LIGHT_LEVEL" },
        { DAM_ANGLE, "DAM_ANGLE" },
        { DAM_OFFSET, "DAM_OFFSET" },
        { DAM_LINE_COUNT, "DAM_LINE_COUNT" },
        { DAM_LINE_FIRST, "DAM_LINE_FIRST" },
        { DAM_BBOX_RIGHT_TOP_Y, "DAM_BBOX_RIGHT_TOP_Y" },
        { DAM_BBOX_RIGHT_LOW_Y, "DAM_BBOX_RIGHT_LOW_Y" },
        { DAM_BBOX_RIGHT_LOW_X, "DAM_BBOX_RIGHT_LOW_X" },
        { DAM_BBOX_RIGHT_TOP_X, "DAM_BBOX_RIGHT_TOP_X" },
        { DAM_BBOX_LEFT_TOP_Y, "DAM_BBOX_LEFT_TOP_Y" },
        { DAM_BBOX_LEFT_LOW_Y, "DAM_BBOX_LEFT_LOW_Y" },
        { DAM_BBOX_LEFT_LOW_X, "DAM_BBOX_LEFT_LOW_X" },
        { DAM_BBOX_LEFT_TOP_X, "DAM_BBOX_LEFT_TOP_X" },
        { DAM_CHILD_RIGHT, "DAM_CHILD_RIGHT" },
        { DAM_CHILD_LEFT, "DAM_CHILD_LEFT" },
        { 0, NULL }
    };
    int i;

    for(i = 0; props[i].str; ++i)
        if(props[i].prop == prop)
            return props[i].str;

    sprintf(propStr, "(unnamed %i)", prop);
    return propStr;
}

static void ParseGLBSPInf(mapdatalumpinfo_t* mapLump)
{
    int i, n, keylength;
    char* ch;
    char line[250];

    glbuildinfo_t *newInfo = malloc(sizeof(glbuildinfo_t));

    struct glbsp_keyword_s {
        const char* label;
        char** data;
    } keywords[] =
    {
        {"LEVEL", &newInfo->level},
        {"BUILDER", &newInfo->builder},
        {"TIME", &newInfo->time},
        {"CHECKSUM", &newInfo->checksum},
        {NULL}
    };

    newInfo->level = NULL;
    newInfo->builder = NULL;
    newInfo->time = NULL;
    newInfo->checksum = NULL;

    // Have we cached the lump yet?
    if(mapLump->lumpp == NULL)
        mapLump->lumpp = (byte *) W_CacheLumpNum(mapLump->lumpNum, PU_STATIC);

    ch = (char *) mapLump->lumpp;
    n = 0;
    for(;;)
    {
        // Read a line
        memset(line, 0, 250);
        for(i = 0; i < 250 - 1; ++n)    // Make the last null stay there.
        {
            if(n == mapLump->length || *ch == '\n')
                break;

            if(*ch == '=')
                keylength = i;

            line[i++] = *ch;
            ch++;
        }

        // Only one keyword per line. Is it known?
        for(i = 0; keywords[i].label; ++i)
            if(!strncmp(line, keywords[i].label, keylength))
            {
                *keywords[i].data = malloc(strlen(line) - keylength + 1);
                strncpy(*keywords[i].data, line + keylength + 1,
                        strlen(line) - keylength);
            }

        n++;

        // End of lump;
        if(n == mapLump->length)
            break;
        ch++;
    }

    glBuilderInfo = newInfo;
}

static void FreeGLBSPInf(void)
{
    // Free the glBuilder info
    if(glBuilderInfo != NULL)
    {
        if(glBuilderInfo->level)
        {
            free(glBuilderInfo->level);
            glBuilderInfo->level = NULL;
        }

        if(glBuilderInfo->builder)
        {
            free(glBuilderInfo->builder);
            glBuilderInfo->builder = NULL;
        }

        if(glBuilderInfo->time)
        {
            free(glBuilderInfo->time);
            glBuilderInfo->time = NULL;
        }

        if(glBuilderInfo->checksum)
        {
            free(glBuilderInfo->checksum);
            glBuilderInfo->checksum = NULL;
        }

        free(glBuilderInfo);
        glBuilderInfo = NULL;
    }
}

static void AddMapDataLump(int lumpNum, int lumpClass)
{
    int num = numMapDataLumps;
    mapdatalumpinfo_t* mapDataLump;

    numMapDataLumps++;

    mapDataLumps =
        realloc(mapDataLumps, sizeof(mapdatalumpinfo_t) * numMapDataLumps);

    mapDataLump = &mapDataLumps[num];
    mapDataLump->lumpNum = lumpNum;
    mapDataLump->lumpClass = lumpClass;
    mapDataLump->lumpp = NULL;
    mapDataLump->length = 0;
    mapDataLump->version = -1;
    mapDataLump->startOffset = 0;
}

static void FreeMapDataLumps(void)
{
    int k;

    // Free the map data lump array
    if(mapDataLumps != NULL)
    {
        // Free any lumps we might have pre-cached.
        for(k = 0; k < numMapDataLumps; k++)
            if(mapDataLumps[k].lumpp)
            {
                Z_Free(mapDataLumps[k].lumpp);
                mapDataLumps[k].lumpp = 0;
            }

        free(mapDataLumps);
        mapDataLumps = NULL;
        numMapDataLumps = 0;
    }
}

/*
 * Locate the lump indices where the data of the specified map
 * resides (both regular and GL Node data).
 *
 * @param levelID       Identifier of the map to be loaded (eg "E1M1").
 * @param lumpIndices   Ptr to the array to write the identifiers back to.
 *
 * @return boolean      (FALSE) If we cannot find the map data.
 */
static boolean P_LocateMapData(char *levelID, int *lumpIndices)
{
    char glLumpName[40];

    // Find map name.
    sprintf(glLumpName, "GL_%s", levelID);
    Con_Message("SetupLevel: %s\n", levelID);

    // Let's see if a plugin is available for loading the data.
    if(!Plug_DoHook(HOOK_LOAD_MAP_LUMPS, W_GetNumForName(levelID),
                    (void*) lumpIndices))
    {
        // The plugin failed.
        lumpIndices[0] = W_CheckNumForName(levelID);

        // FIXME: The latest GLBSP spec supports maps with non-standard
        // identifiers. To support these we must check the lump named
        // GL_LEVEL. In this lump will be a text string which identifies
        // the name of the lump the data is for.
        lumpIndices[1] = W_CheckNumForName(glLumpName);
    }

    if(lumpIndices[0] == -1)
        return false; // The map data cannot be found.

    // Do we have any GL Nodes?
    if(lumpIndices[1] > lumpIndices[0])
        glNodeData = true;
    else
    {
        glNodeData = false;
        glNodeFormat = -1;
    }

    return true;
}

/*
 * Find the lump offsets for this map dataset automatically.
 * Some obscure PWADs have these lumps in a non-standard order... tsk, tsk.
 *
 * @param startLump     The lump number to begin our search with.
 */
static void P_FindMapLumps(int startLump)
{
    unsigned int k;
    unsigned int i;
    boolean scan;
    maplumpinfo_t* mapLmpInf;

    // Add the marker lump to the list (there might be useful info in it)
    if(!strncmp(W_CacheLumpNum(startLump, PU_GETNAME), "GL_", 3))
        AddMapDataLump(startLump, LCG_LABEL);
    else
        AddMapDataLump(startLump, LCM_LABEL);

    ++startLump;
    // Keep checking lumps to see if its a map data lump.
    for(i = (unsigned) startLump; ; ++i)
    {
        scan = true;
        // Compare the name of this lump with our known map data lump names
        mapLmpInf = mapLumpInfo;
        for(k = NUM_LUMPCLASSES; k-- && scan; ++mapLmpInf)
        {
            if(mapLmpInf->lumpname)
            if(!strncmp(mapLmpInf->lumpname, W_CacheLumpNum(i, PU_GETNAME), 8))
            {
                // Lump name matches a known lump name.
                // Add it to the lumps we'll process for map data.
                AddMapDataLump(i, mapLmpInf->lumpclass);
                scan = false;
            }
        }
        // We didn't find a match for this name?
        if(scan)
            break; // Stop looking, we've found them all.
    }
}

/*
 * Attempt to determine the format of this map data lump.
 *
 * This is done by checking the start bytes of this lump (the lump "header")
 * to see if its of a known special format. Special formats include Doomsday
 * custom map data formats and GL Node formats.
 *
 * @param mapLump   Ptr to the map lump struct to work with.
 */
static void DetermineMapDataLumpFormat(mapdatalumpinfo_t* mapLump)
{
    byte lumpHeader[4];

    W_ReadLumpSection(mapLump->lumpNum, lumpHeader, 0, 4);

    // Check to see if this a Doomsday, custom map data - lump format.
    if(memcmp(lumpHeader, "DDAY", 4) == 0)
    {
        // It IS a custom Doomsday format.

        // TODO:
        // Determine the "named" format to use when processing this lump.

        // Imediatetly after "DDAY" is a block of text with various info
        // about this lump. This text block begins with "[" and ends at "]".
        // TODO: Decide on specifics for this text block.
        // (a simple name=value pair delimited by " " should suffice?)

        // Search this string for known keywords (eg the name of the format).

        // Store the TOTAL number of bytes (including the magic bytes "DDAY")
        // that the header uses, into the startOffset (the offset into the
        // byte stream where the data starts) for this lump.

        // Once we know the name of the format, the lump length and the Offset
        // we can check to make sure to the lump format definition is correct
        // for this lump thus:

        // sum = (lumplength - startOffset) / (number of bytes per element)
        // If sum is not a whole integer then something is wrong with either
        // the lump data or the lump format definition.
        return;
    }
    else if(glNodeData &&
            mapLump->lumpClass >= LCG_VERTEXES &&
            mapLump->lumpClass <= LCG_NODES)
    {
        unsigned int i;
        int lumpClass = mapLumpInfo[mapLump->lumpClass].glLump;
        mapdatalumpformat_t* mapDataLumpFormat;
        glnodeformat_t* nodeFormat = glNodeFormats;

        // Perhaps its a "named" GL Node format?

        // Find out which gl node version the data uses
        // loop backwards (check for latest version first)
        for(i = GLNODE_FORMATS; i--; ++nodeFormat)
        {
            mapDataLumpFormat = &(nodeFormat->verInfo[lumpClass]);

            // Check the header against each known name for this lump class.
            if(mapDataLumpFormat->magicid != NULL)
            {
                if(memcmp(lumpHeader, mapDataLumpFormat->magicid, 4) == 0)
                {
                    // Aha! It IS a "named" format.
                    // Record the version number.
                    mapLump->version = mapDataLumpFormat->version;

                    // Set the start offset into byte stream.
                    mapLump->startOffset = 4; // num of bytes

                    return;
                }
            }
        }

        // It's not a named format.
        // Most GL Node formats don't include magic bytes in each lump.
        // Because we don't KNOW the format of this lump we should
        // ignore it when determining the GL Node format.
        return;
    }
    else if(mapLump->lumpClass == LCG_LABEL)
    {
        // It's a GL NODE identifier lump.
        // Perhaps it can tell us something useful about this map data?
        // It is a text lump that contains a simple label=value pair list.
        if(mapLump->length > 0)
            ParseGLBSPInf(mapLump);
    }

    // It isn't a (known) named special format.
    // Use the default data format for this lump (map format specific).
}

/*
 * Make sure we have (at least) one lump of each lump class that we require.
 *
 * During the process we will attempt to determine the format of an individual
 * map data lump and record various info about the lumps that will be of use
 * further down the line.
 *
 * @param   levelID     The map identifer string, used only in error messages.
 *
 * @return  boolean     (True) If the map data provides us with enough data
 *                             to proceed with loading the map.
 */
static boolean VerifyMapData(char *levelID)
{
    unsigned int i, k;
    boolean found;
    boolean required;
    mapdatalumpinfo_t* mapDataLump;
    maplumpinfo_t* mapLmpInf = mapLumpInfo;

    FreeGLBSPInf();

    // Itterate our known lump classes array.
    for(i = NUM_LUMPCLASSES; i--; ++mapLmpInf)
    {
        // Check all the registered map data lumps to make sure we have at least
        // one lump of each required lump class.
        found = false;
        mapDataLump = mapDataLumps;
        for(k = numMapDataLumps; k--; ++mapDataLump)
        {
            // Is this a lump of the class we are looking for?
            if(mapDataLump->lumpClass == mapLmpInf->lumpclass)
            {
                // Are we precaching lumps of this class?
                if(mapLmpInf->precache)
                   mapDataLump->lumpp =
                        (byte *) W_CacheLumpNum(mapDataLump->lumpNum, PU_STATIC);

                // Store the lump length.
                mapDataLump->length = W_LumpLength(mapDataLump->lumpNum);

                // If this is a BEHAVIOR lump, then this MUST be a HEXEN format map.
                if(mapDataLump->lumpClass == LCM_BEHAVIOR)
                    mapFormat = 1;

                // Attempt to determine the format of this map data lump.
                DetermineMapDataLumpFormat(mapDataLump);

                // Announce
                VERBOSE2(Con_Message("%s - %s is %d bytes.\n",
                                     W_CacheLumpNum(mapDataLump->lumpNum, PU_GETNAME),
                                     DAM_Str(mapLmpInf->dataType), mapDataLump->length));

                // We've found (at least) one lump of this class.
                found = true;
            }
        }

        // We arn't interested in indetifier lumps
        if(mapLmpInf->lumpclass == LCM_LABEL ||
           mapLmpInf->lumpclass == LCG_LABEL)
           continue;

        // We didn't find any lumps of this class?
        if(!found)
        {
            // Is it a required lump class?
            //   Is this lump that will be generated if a BSP builder is available?
            if(mapLmpInf->required == BSPBUILD &&
               Plug_CheckForHook(HOOK_LOAD_MAP_LUMPS) && bspBuild)
                required = false;
            else if(mapLmpInf->required)
                required = true;
            else
                required = false;

            if(required)
            {
                // Darn, the map data is incomplete. We arn't able to to load this map :`(
                // Inform the user.
                Con_Message("VerifyMapData: %s for \"%s\" could not be found.\n" \
                            " This lump is required in order to play this map.\n",
                            mapLmpInf->lumpname, levelID);
                return false;
            }
            else
            {
                // It's not required (we can generate it/we don't need it)
                Con_Message("VerifyMapData: %s for \"%s\" could not be found.\n" \
                            "Useable data will be generated automatically if needed.\n",
                            mapLmpInf->lumpname, levelID);
                // Add a dummy lump to the list
                AddMapDataLump(-1, mapLmpInf->lumpclass);
            }
        }
    }

    // All is well, we can attempt to load the map.
    return true;
}

/*
 * Determines the format of the map by comparing the (already determined)
 * lump formats against the known map formats.
 *
 * Map data lumps can be in any mixed format but GL Node data cannot so
 * we only check those atm.
 *
 * @return boolean     (True) If its a format we support.
 */
static boolean DetermineMapDataFormat(void)
{
    unsigned int i;
    int lumpClass;
    mapdatalumpinfo_t* mapLump;

    // Now that we know the data format of the lumps we need to update the
    // internal version number for any lumps that don't declare a version (-1).
    // Taken from the version stipulated in the map format.
    mapLump = mapDataLumps;
    for(i = numMapDataLumps; i--; ++mapLump)
    {
        lumpClass = mapLumpInfo[mapLump->lumpClass].mdLump;

        // Is it a map data lump class?
        if(mapLump->lumpClass >= LCM_THINGS &&
           mapLump->lumpClass <= LCM_BEHAVIOR)
        {
            // Set the lump version number for this format.
            if(mapLump->version == -1)
                mapLump->version = mapDataFormats[mapFormat].verInfo[lumpClass].version;
        }
    }

    // Do we have GL nodes?
    if(glNodeData)
    {
        unsigned int k;
        boolean failed;
        glnodeformat_t* nodeFormat = &glNodeFormats[GLNODE_FORMATS];

        // Find out which GL Node version the data is in.
        // Loop backwards (check for latest version first).
        for(i = GLNODE_FORMATS; i--; --nodeFormat)
        {
            // Check the version number of each map data lump
            mapLump = mapDataLumps;
            for(k = numMapDataLumps, failed = false; k-- && !failed; ++mapLump)
            {
                // Is it a GL Node data lump class?
                if(mapLump->lumpClass >= LCG_VERTEXES &&
                   mapLump->lumpClass <= LCG_NODES)
                {
                    lumpClass = mapLumpInfo[mapLump->lumpClass].glLump;

                    /*Con_Message("Check lump %s | %s n%d ver %d lump ver %d\n",
                                mapLumpInfo[mapLump->lumpClass].lumpname,
                                W_CacheLumpNum(mapLump->lumpNum, PU_GETNAME),
                                mapLumpInfo[mapLump->lumpClass].glLump,
                                glNodeFormats[i].verInfo[lumpClass].version,
                                mapLump->version);*/

                    // SHOULD this lump format declare a version (magic bytes)?
                    if(mapLump->version == -1)
                    {
                        if(nodeFormat->verInfo[lumpClass].magicid != NULL)
                            failed = true;
                    }
                    else
                    {
                        // Compare the versions.
                        if(mapLump->version != nodeFormat->verInfo[lumpClass].version)
                            failed = true;
                    }
                }
            }

            // Did all lumps match the required format for this version?
            if(!failed)
            {
                // We know the GL Node format
                glNodeFormat = i;

                Con_Message("DetermineMapDataFormat: (%s GL Node Data)\n",
                            nodeFormat->vername);

                // Did we find any glbuild info?
                if(glBuilderInfo)
                {
                    Con_Message("(");
                    if(glBuilderInfo->level)
                        Con_Message("%s | ", glBuilderInfo->level);

                    if(glBuilderInfo->builder)
                        Con_Message("%s | ", glBuilderInfo->builder);

                    if(glBuilderInfo->time)
                        Con_Message("%s | ", glBuilderInfo->time);

                    if(glBuilderInfo->checksum)
                        Con_Message("%s", glBuilderInfo->checksum);
                    Con_Message(")\n");
                }

                // Do we support this GL Node format?
                if(nodeFormat->supported)
                {
                    // Now that we know the GL Node format we need to update the internal
                    // version number for any lumps that don't declare a version (-1).
                    // Taken from the version stipulated in the Node format.
                    mapLump = mapDataLumps;
                    for(k = numMapDataLumps; k--; ++mapLump)
                    {
                        lumpClass = mapLumpInfo[mapLump->lumpClass].glLump;

                        // Is it a GL Node data lump class?
                        if(mapLump->lumpClass >= LCG_VERTEXES &&
                           mapLump->lumpClass <= LCG_NODES)
                        {
                            // Set the lump version number for this format.
                            if(mapLump->version == -1)
                                mapLump->version = nodeFormat->verInfo[lumpClass].version;
                        }
                    }
                    return true;
                }
                else
                {
                    // Unsupported GL Node format.
                    Con_Message("DetermineMapDataFormat: Sorry, %s GL Nodes arn't supported\n",
                                nodeFormat->vername);
                    return false;
                }
            }
        }
        Con_Message("DetermineMapDataFormat: Could not determine GL Node format\n");
        return false;
    }

    // We support this map data format.
    return true;
}

boolean P_GetMapFormat(void)
{
    if(DetermineMapDataFormat())
    {
        // We support the map data format.
        return true;
    }
    else
    {
        // Darn, we can't load this map.
        // Free any lumps we may have already precached in the process.
        FreeMapDataLumps();
        FreeGLBSPInf();
        return false;
    }
}

/*
 * Do any final initialization of map data members.
 *
 * Configure the map data objects so they can be accessed by the
 * games, using the DMU functions of the Doomsday public API.
 */
static void FinalizeMapData(void)
{
    int i;
    sector_t* sec;
    side_t* side;

    for(i = 0; i < numvertexes; ++i)
        vertexes[i].header.type = DMU_VERTEX;

    for(i = 0; i < numsegs; ++i)
        segs[i].header.type = DMU_SEG;

    for(i = 0; i < numlines; ++i)
        lines[i].header.type = DMU_LINE;

    for(i = 0; i < numsides; ++i)
    {
        side = &sides[i];

        side->header.type = DMU_SIDE;

        memset(side->toprgb, 0xff, 3);
        memset(side->midrgba, 0xff, 4);
        memset(side->bottomrgb, 0xff, 3);
        side->blendmode = BM_NORMAL;

        // Make sure the texture references are good.
        if(side->toptexture > numtextures - 1)
            side->toptexture = 0;
        if(side->midtexture > numtextures - 1)
            side->midtexture = 0;
        if(side->bottomtexture > numtextures - 1)
            side->bottomtexture = 0;
    }

    for(i = 0; i < numsubsectors; ++i)
        subsectors[i].header.type = DMU_SUBSECTOR;

    for(i = 0; i < numsectors; ++i)
    {
        sec = &sectors[i];
        sec->header.type = DMU_SECTOR;

        sec->thinglist = NULL;
        memset(sec->rgb, 0xff, 3);
        memset(sec->floorrgb, 0xff, 3);
        memset(sec->ceilingrgb, 0xff, 3);
    }

    for(i = 0; i < po_NumPolyobjs; ++i)
        polyobjs[i].header.type = DMU_POLYOBJ;

    for(i = 0; i < numnodes; ++i)
        nodes[i].header.type = DMU_NODE;
}

static boolean P_ReadMapData(int doClass)
{
    // Cant load GL NODE data if we don't have it.
    if(!glNodeData && (doClass >= LCG_VERTEXES && doClass <= LCG_NODES))
        // Not having the data is considered a success.
        // This is due to us invoking the dpMapLoader plugin at an awkward
        // point in the map loading process (at the start).
        return true;

    if(!ReadMapData(doClass))
    {
        FreeMapDataLumps();
        FreeGLBSPInf();
        return false;
    }

    return true;
}

/*
 * Loads the map data structures for a level.
 *
 * @param levelId   Identifier of the map to be loaded (eg "E1M1").
 *
 * @return boolean  (True) If we loaded/generated all map data successfully.
 */
boolean P_LoadMapData(char *levelId)
{
    int lumpNumbers[2];

    numMapDataLumps = 0;

    // We'll assume we're loading a DOOM format map to begin with.
    mapFormat = 0;

    // Attempt to find the map data for this level
    if(!P_LocateMapData(levelId, lumpNumbers))
    {
        // Well that was a non-starter...
        return false;
    }

    // Find the actual map data lumps and their offsets.
    // Add them to the list of lumps to be processed.
    P_FindMapLumps(lumpNumbers[0]);

    // If we have GL Node data, find those lumps too.
    if(glNodeData)
        P_FindMapLumps(lumpNumbers[1]);

    // Make sure we have all the data we need to load this level.
    if(!VerifyMapData(levelId))
    {
        // Darn, the level data is incomplete.
        // Free any lumps we may have already precached in the process.
        FreeMapDataLumps();
        FreeGLBSPInf();

        // Sorry, but we can't continue.
        return false;
    }

    // Looking good so far.
    // Try to determine the format of this map.
    if(P_GetMapFormat())
    {
        // Excellent, its a map we can read. Load it in!
        Con_Message("P_LoadMapData: %s\n", levelId);

        // Reset the global map data struct counters.
        numvertexes = 0;
        numsubsectors = 0;
        numsectors = 0;
        numnodes = 0;
        numsides = 0;
        numlines = 0;
        numsegs = 0;
        numthings = 0;

        // Load all lumps of each class in this order.
        //
        // NOTE:
        // DJS 01/10/05 - revised load order to allow for cross-referencing
        //                data during loading (detect + fix trivial errors).
        if(!P_ReadMapData(LCM_VERTEXES))
            return false;
        if(!P_ReadMapData(LCG_VERTEXES))
            return false;
        if(!P_ReadMapData(LCM_SECTORS))
            return false;
        if(!P_ReadMapData(LCM_SIDEDEFS))
            return false;
        if(!P_ReadMapData(LCM_LINEDEFS))
            return false;

        P_ReadSideDefTextures(lumpNumbers[0] + ML_SIDEDEFS);
        P_FinishLineDefs();

        if(!P_ReadMapData(LCM_BLOCKMAP))
            return false;

        if(!P_ReadMapData(LCM_THINGS))
            return false;
        if(!P_ReadMapData(LCM_SEGS))
            return false;
        if(!P_ReadMapData(LCM_SUBSECTORS))
            return false;
        if(!P_ReadMapData(LCM_NODES))
            return false;
        if(!P_ReadMapData(LCM_REJECT))
            return false;

        //P_PrintDebugMapData();

        // We have complete level data but we're not out of the woods yet...
        FreeMapDataLumps();
        FreeGLBSPInf();

    /*    if(glNodeData)
            setupflags |= DDSLF_DONT_CLIP; */

        FinalizeMapData();

        // Do any initialization/error checking work we need to do.
        // Must be called before we go any further
        return P_CheckLevel(levelId, false);
    }
    else
    {
        // Sorry, but we can't continue.
        return false;
    }
}

/*
 * Works through the map data lump array, processing all the lumps
 * of the requested class.
 *
 * @param: doClass      The class of map data lump to process.
 *
 * @return: boolean     (True) All the lumps of the requested class
 *                      were processed successfully.
 */
static boolean ReadMapData(int doClass)
{
    int internalType;
    int lumpCount;
    unsigned int i;
    unsigned int elements;
    unsigned int oldNum, newNum;

    datatype_t *dataTypes;
    mapdatalumpinfo_t* mapLump = mapDataLumps;
    mapdatalumpformat_t* lumpFormat;
    maplumpinfo_t*  lumpInfo;

    uint startTime;

    // Are gl Nodes available?
    if(glNodeData)
    {
        // Use the gl versions of the following lumps:
        if(doClass == LCM_SUBSECTORS)
            doClass = LCG_SUBSECTORS;
        else if(doClass == LCM_SEGS)
            doClass = LCG_SEGS;
        else if(doClass == LCM_NODES)
            doClass = LCG_NODES;
    }

    lumpCount = 0;
    for(i = numMapDataLumps; i--; ++mapLump)
    {
        lumpInfo = &mapLumpInfo[mapLump->lumpClass];

        // Only process lumps that match the class requested
        if(doClass == mapLump->lumpClass)
        {
            internalType = lumpInfo->dataType;

            // Is this a "real" lump? (ie do we have to generate the data for it?)
            if(mapLump->lumpNum != -1)
            {
                // use the original map data structures by default
                if(lumpInfo->mdLump >= 0)
                {
                    lumpFormat = &mapDataFormats[mapFormat].verInfo[lumpInfo->mdLump];
                }
                else // its a gl node lumpclass
                {
                    lumpFormat = &glNodeFormats[glNodeFormat].verInfo[lumpInfo->glLump];
                }

                if(lumpFormat->values != NULL)
                    dataTypes = lumpFormat->values;
                else
                    dataTypes = NULL;

                // How many elements are in the lump?
                elements = (mapLump->length - mapLump->startOffset) / (int) lumpFormat->elmSize;

                VERBOSE(Con_Message("P_ReadMapData: Processing \"%s\" (#%d) ver %d...\n",
                                    W_CacheLumpNum(mapLump->lumpNum, PU_GETNAME), elements,
                                    mapLump->version));

                // Have we cached the lump yet?
                if(mapLump->lumpp == NULL)
                {
                    mapLump->lumpp = (byte *) W_CacheLumpNum(mapLump->lumpNum, PU_STATIC);
                }
            }
            else
            {
                // Not a problem, we'll generate useable data automatically.
                VERBOSE(Con_Message("P_ReadMapData: Generating \"%s\"\n",
                                    lumpInfo->lumpname));
            }

            // Allocate and init depending on the type of data and if this is the
            // first lump of this class being processed.
            switch(internalType)
            {
            case DAM_VERTEX:
                oldNum = numvertexes;
                newNum = numvertexes+= elements;

                if(oldNum != 0)
                    vertexes = Z_Realloc(vertexes, numvertexes * sizeof(vertex_t), PU_LEVEL);
                else
                    vertexes = Z_Malloc(numvertexes * sizeof(vertex_t), PU_LEVEL, 0);

                memset(VERTEX_PTR(oldNum), 0, elements * sizeof(vertex_t));

                if(mapLump->lumpClass == LCM_VERTEXES && oldNum == 0)
                    firstGLvertex = numvertexes;
                break;

            case DAM_THING:
                // mapthings are game-side
                oldNum = numthings;
                newNum = numthings+= elements;

                // Call the game's setup routine
                if(gx.SetupForThings)
                    gx.SetupForThings(elements);
                break;

            case DAM_LINE:
                oldNum = numlines;
                newNum = numlines+= elements;

                if(oldNum != 0)
                    lines = Z_Realloc(lines, numlines * sizeof(line_t), PU_LEVEL);
                else
                    lines = Z_Malloc(numlines * sizeof(line_t), PU_LEVEL, 0);

                memset(LINE_PTR(oldNum), 0, elements * sizeof(line_t));

                // for missing front detection
                missingFronts = malloc(numlines * sizeof(int));
                memset(missingFronts, 0, sizeof(missingFronts));

                // Call the game's setup routine
                if(gx.SetupForLines)
                    gx.SetupForLines(elements);
                break;

            case DAM_SIDE:
                oldNum = numsides;
                newNum = numsides+= elements;

                if(oldNum != 0)
                    sides = Z_Realloc(sides, numsides * sizeof(side_t), PU_LEVEL);
                else
                    sides = Z_Malloc(numsides * sizeof(side_t), PU_LEVEL, 0);

                memset(SIDE_PTR(oldNum), 0, elements * sizeof(side_t));

                // Call the game's setup routine
                if(gx.SetupForSides)
                    gx.SetupForSides(elements);
                break;

            case DAM_SEG:
                // Segs are read into a temporary buffer before processing
                oldNum = numsegs;
                newNum = numsegs+= elements;

                if(oldNum != 0)
                {
                    segs = Z_Realloc(segs, numsegs * sizeof(seg_t), PU_LEVEL);
                    segstemp = Z_Realloc(segstemp, numsegs * sizeof(mapseg_t), PU_STATIC);
                }
                else
                {
                    segs = Z_Malloc(numsegs * sizeof(seg_t), PU_LEVEL, 0);
                    segstemp = Z_Malloc(numsegs * sizeof(mapseg_t), PU_STATIC, 0);
                }

                memset(SEG_PTR(oldNum), 0, elements * sizeof(seg_t));
                memset(segstemp + oldNum, 0, elements * sizeof(mapseg_t));
                break;

            case DAM_SUBSECTOR:
                oldNum = numsubsectors;
                newNum = numsubsectors+= elements;

                if(oldNum != 0)
                    subsectors = Z_Realloc(subsectors, numsubsectors * sizeof(subsector_t), PU_LEVEL);
                else
                    subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_LEVEL, 0);

                memset(SUBSECTOR_PTR(oldNum), 0, elements * sizeof(subsector_t));
                break;

            case DAM_NODE:
                oldNum = numnodes;
                newNum = numnodes+= elements;

                if(oldNum != 0)
                    nodes = Z_Realloc(nodes, numnodes * sizeof(node_t), PU_LEVEL);
                else
                    nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, 0);

                memset(NODE_PTR(oldNum), 0, elements * sizeof(node_t));
                break;

            case DAM_SECTOR:
                oldNum = numsectors;
                newNum = numsectors+= elements;

                if(oldNum != 0)
                    sectors = Z_Realloc(sectors, numsectors * sizeof(sector_t), PU_LEVEL);
                else
                    sectors = Z_Malloc(numsectors * sizeof(sector_t), PU_LEVEL, 0);

                memset(SECTOR_PTR(oldNum), 0, elements * sizeof(sector_t));

                // Call the game's setup routine
                if(gx.SetupForSectors)
                    gx.SetupForSectors(elements);
                break;

            default:
                break;
            }

            // Read in the lump data
            startTime = Sys_GetRealTime();
            if(internalType == DAM_MAPBLOCK)
            {
                if(!P_LoadBlockMap(mapLump))
                    return false;
            }
            else if(internalType == DAM_SECREJECT)
            {
                if(!P_LoadReject(mapLump))
                    return false;
            }
            else
            {
                P_ReadBinaryMapData(oldNum, internalType, (mapLump->lumpp + mapLump->startOffset),
                                    lumpFormat->elmSize, elements, lumpFormat->numValues, dataTypes);

                // Perform any additional processing required (temporary)
                switch(internalType)
                {
                case DAM_SEG:
                    P_ProcessSegs(mapLump->version);
                    break;

                default:
                    break;
                }
            }

            // How much time did we spend?
            VERBOSE2(Con_Message("P_ReadMapData: Done in %.4f seconds.\n",
                                 (Sys_GetRealTime() - startTime) / 1000.0f));

            // We're finished with this lump.
            if(mapLump->lumpp)
            {
                Z_Free(mapLump->lumpp);
                mapLump->lumpp = 0;
            }

            // Remember how many lumps of this class we've processed
            ++lumpCount;
        }
    }

    return true;
}

/*
 * Reads a value from the (little endian) source buffer. Does some basic
 * type checking so that incompatible types are not assigned.
 * Simple conversions are also done, e.g., float to fixed.
 */
static void ReadValue(void* dest, valuetype_t valueType, const byte *src,
                      const datatype_t* prop, unsigned int index)
{
    int flags = prop->flags;

    if(valueType == DDVT_BYTE)
    {
        byte* d = dest;
        switch(prop->size)
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
            Con_Error("ReadValue: DDVT_BYTE incompatible with value type\n");
        }
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short* d = dest;
        switch(prop->size)
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

        case 8:
            if(flags & DT_TEXTURE)
            {
                *d = P_CheckTexture((char*)((long long*)(src)), false, valueType,
                                    index, prop->property);
            }
            else if(flags & DT_FLAT)
            {
                *d = P_CheckTexture((char*)((long long*)(src)), true, valueType,
                                    index, prop->property);
            }
            break;
         default:
            Con_Error("ReadValue: DDVT_SHORT incompatible with value type.\n");
         }
    }
    else if(valueType == DDVT_FIXED)
    {
        fixed_t* d = dest;

        switch(prop->size) // Number of src bytes
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
            Con_Error("ReadValue: DDVT_FIXED incompatible with value type.\n");
        }
    }
    else if(valueType == DDVT_ULONG)
    {
        unsigned long* d = dest;

        switch(prop->size) // Number of src bytes
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
            Con_Error("ReadValue: DDVT_ULONG incompatible with value type.\n");
        }
    }
    else if(valueType == DDVT_INT)
    {
        int* d = dest;

        switch(prop->size) // Number of src bytes
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

                    if(num != ((unsigned short)-1))
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
            Con_Error("ReadValue: DDVT_INT incompatible with value type.\n");
        }
    }
    else if(valueType == DDVT_SECT_PTR || valueType == DDVT_VERT_PTR)
    {
        int idx = NO_INDEX;

        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                idx = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((short*)(src)));

                    if(num != ((unsigned short)-1))
                        idx = num;
                }
                else
                {
                    idx = SHORT(*((short*)(src)));
                }
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                idx = ULONG(*((long*)(src)));
            else
                idx = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_SECT_PTR incompatible with value type.\n");
        }

        switch(valueType)
        {
        case DDVT_SECT_PTR:
            {
            sector_t** d = dest;
            if(idx >= 0 && idx < numsectors)
                *d = &sectors[idx];
            else
                *d = NULL;
            break;
            }

        case DDVT_VERT_PTR:
            {
            vertex_t** d = dest;
            if(idx >= 0 && idx < numvertexes)
                *d = &vertexes[idx];
            else
                *d = NULL;
            break;
            }
        }
    }
    else
    {
        Con_Error("ReadValue: unknown value type %s.\n", DMU_Str(valueType));
    }
}

static void ReadMapProperty(int dataType, unsigned int element, const datatype_t* prop,
                            const byte *buffer)
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

        ReadValue(dest, prop->size, buffer + prop->offset, prop, element);

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
                          DAM_Str(prop->property));
            }

            ReadValue(dest, destType, buffer + prop->offset, prop, element);
            break;
            }

        case DAM_LINE:  // Lines are read into an interim format
            {
            line_t *p = LINE_PTR(element);
            switch(prop->property)
            {
            case DAM_VERTEX1:
                dest = &p->v1;
                destType = DDVT_VERT_PTR;
                break;

            case DAM_VERTEX2:
                dest = &p->v2;
                destType = DDVT_VERT_PTR;
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
                          DAM_Str(prop->property));
            }

            ReadValue(dest, destType, buffer + prop->offset, prop, element);
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
                destType = DDVT_SECT_PTR;
                break;

            default:
                Con_Error("ReadMapProperty: DAM_SIDE has no property %s.\n",
                          DAM_Str(prop->property));
            }

            ReadValue(dest, destType, buffer + prop->offset, prop, element);
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
                          DAM_Str(prop->property));
            }

            ReadValue(dest, destType, buffer + prop->offset, prop, element);
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
                          DAM_Str(prop->property));
            }

            ReadValue(dest, destType, buffer + prop->offset, prop, element);
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
                          DAM_Str(prop->property));
            }

            ReadValue(dest, destType, buffer + prop->offset, prop, element);
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
                          DAM_Str(prop->property));
            }

            ReadValue(dest, destType, buffer + prop->offset, prop, element);
            break;
            }
        default:
            Con_Error("ReadMapProperty: Type cannot be assigned to from a map format.\n");
        }
    }
}

/*
 * Not very pretty to look at but it IS pretty quick :-)
 */
static void P_ReadBinaryMapData(unsigned int startIndex, int dataType, const byte *buffer,
                                size_t elmSize, unsigned int elements, unsigned int numValues,
                                const datatype_t *types)
{
#define NUMBLOCKS 8
#define BLOCK for(k = numValues, mytypes = types; k--; ++mytypes) \
              { \
                  ReadMapProperty(dataType, index, mytypes, buffer); \
              } \
              buffer += elmSize; \
              ++index;

    unsigned int     i = 0, k;
    unsigned int     blocklimit = (elements / NUMBLOCKS) * NUMBLOCKS;
    unsigned int     index;
    const datatype_t* mytypes;

    // Have we got enough to do some in blocks?
    if(elements >= blocklimit)
    {
        while(i < blocklimit)
        {
            index = startIndex + i;

            BLOCK BLOCK BLOCK BLOCK BLOCK BLOCK BLOCK BLOCK

            i += NUMBLOCKS;
        }
    }

    // Have we got any left to do?
    if(i < elements)
    {
        // Yes, jump into the switch at the number of elements remaining
        index = startIndex + i;
        switch(elements - i)
        {
        case 7: BLOCK
        case 6: BLOCK
        case 5: BLOCK
        case 4: BLOCK
        case 3: BLOCK
        case 2: BLOCK
        case 1:
            for(k = numValues, mytypes = types; k--; ++mytypes)
                ReadMapProperty(dataType, index, mytypes, buffer);
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
    seg_t  *seg;
    line_t *ldef;
    mapseg_t *ml;

    ml = segstemp;

    for(i = 0; i < numsegs; ++i, ++ml)
    {
        seg = SEG_PTR(i);

        // Which version?
        switch(version)
        {
        case 1:  // (mapseg_t)
            seg->v1 = VERTEX_PTR(ml->v1);
            seg->v2 = VERTEX_PTR(ml->v2);
            break;

        case 2:  // (glseg_t)
            seg->v1 =
                VERTEX_PTR(ml->v1 & 0x8000 ?
                          firstGLvertex + (ml->v1 & ~0x8000) :
                          ml->v1);
            seg->v2 =
                VERTEX_PTR(ml->v2 & 0x8000 ?
                          firstGLvertex + (ml->v2 & ~0x8000) :
                          ml->v2);
            break;

        case 3:
        case 5:
            seg->v1 =
                VERTEX_PTR(ml->v1 & 0xc0000000 ?
                          firstGLvertex + (ml->v1 & ~0xc0000000) :
                          ml->v1);
            seg->v2 =
                VERTEX_PTR(ml->v2 & 0xc0000000 ?
                          firstGLvertex + (ml->v2 & ~0xc0000000) :
                          ml->v2);
            break;

        default:
            Con_Error("P_ProcessSegs: Error. Unsupported Seg format.");
            break;
        }

        if(ml->angle != 0)
            seg->angle = ml->angle;
        else
            seg->angle = -1;

        if(ml->offset != 0)
            seg->offset = ml->offset;
        else
            seg->offset = -1;

        if(ml->linedef != NO_INDEX)
        {
            ldef = LINE_PTR(ml->linedef);
            seg->linedef = ldef;
            seg->sidedef = SIDE_PTR(ldef->sidenum[ml->side]);
            seg->frontsector = SIDE_PTR(ldef->sidenum[ml->side])->sector;

            if(ldef->flags & ML_TWOSIDED &&
               ldef->sidenum[ml->side ^ 1] != NO_INDEX)
            {
                seg->backsector = SIDE_PTR(ldef->sidenum[ml->side ^ 1])->sector;
            }
            else
            {
                ldef->flags &= ~ML_TWOSIDED;
                seg->backsector = 0;
            }

            if(seg->offset == -1)
            {
                if(ml->side == 0)
                    seg->offset =
                        FRACUNIT * AccurateDistance(seg->v1->x - ldef->v1->x,
                                                    seg->v1->y - ldef->v1->y);
                else
                    seg->offset =
                        FRACUNIT * AccurateDistance(seg->v1->x - ldef->v2->x,
                                                    seg->v1->y - ldef->v2->y);
            }

            if(seg->angle == -1)
                seg->angle =
                    bamsAtan2((seg->v2->y - seg->v1->y) >> FRACBITS,
                              (seg->v2->x - seg->v1->x) >> FRACBITS) << FRACBITS;
        }
        else
        {

            seg->linedef = NULL;
            seg->sidedef = NULL;
            seg->frontsector = NULL;
            seg->backsector = NULL;
        }

        // Calculate the length of the segment. We need this for
        // the texture coordinates. -jk
        seg->length =
            AccurateDistance(seg->v2->x - seg->v1->x, seg->v2->y - seg->v1->y);
        if(version == 0 && seg->length == 0)
            seg->length = 0.01f; // Hmm...
    }

    // We're done with the temporary data
    Z_Free(segstemp);
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

    VERBOSE2(Con_Message("Finalizing Linedefs...\n"));

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

        if(ld->sidenum[0] >= 0 && ld->sidenum[0] < numsides)
            ld->frontsector = SIDE_PTR(ld->sidenum[0])->sector;
        else
            ld->frontsector = 0;

        if(ld->sidenum[1] >= 0 && ld->sidenum[1] < numsides)
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
        sd->toptexture = P_CheckTexture(msd->toptexture, false, DAM_SIDE,
                                        i, DAM_TOP_TEXTURE);

        sd->bottomtexture = P_CheckTexture(msd->bottomtexture, false, DAM_SIDE,
                                           i, DAM_BOTTOM_TEXTURE);

        sd->midtexture = P_CheckTexture(msd->midtexture, false, DAM_SIDE,
                                        i, DAM_MIDDLE_TEXTURE);
    }

    Z_Free(data);
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
void P_InitMapDataFormats(void)
{
    int i, j, index, lumpClass;
    int mlver, glver;
    size_t *mlptr = NULL;
    size_t *glptr = NULL;
    mapdataformat_t *stiptr = NULL;
    glnodeformat_t *glstiptr = NULL;

    // Setup the map data formats for the different
    // versions of the map data structs.

    // Calculate the size of the map data structs
    for(i = MAPDATA_FORMATS -1; i >= 0; --i)
    {
        for(j = 0; j < NUM_LUMPCLASSES; j++)
        {
            lumpClass = mapLumpInfo[j].lumpclass;
            index = mapLumpInfo[j].mdLump;

            mlver = (mapDataFormats[i].verInfo[index].version);
            mlptr = &(mapDataFormats[i].verInfo[index].elmSize);
            stiptr = &(mapDataFormats[i]);

            if(lumpClass == LCM_THINGS)
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
            else if(lumpClass == LCM_LINEDEFS)
            {
                if(mlver == 1)  // DOOM format
                {
                    *mlptr = 14;
                    stiptr->verInfo[index].numValues = 7;
                    stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 7, PU_STATIC, 0);
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
                    // flags
                    stiptr->verInfo[index].values[2].property = DAM_FLAGS;
                    stiptr->verInfo[index].values[2].flags = 0;
                    stiptr->verInfo[index].values[2].size =  2;
                    stiptr->verInfo[index].values[2].offset = 4;
                    stiptr->verInfo[index].values[2].gameprop = 0;
                    // special
                    stiptr->verInfo[index].values[3].property = DAM_LINE_SPECIAL;
                    stiptr->verInfo[index].values[3].flags = 0;
                    stiptr->verInfo[index].values[3].size =  2;
                    stiptr->verInfo[index].values[3].offset = 6;
                    stiptr->verInfo[index].values[3].gameprop = 1;
                    // tag
                    stiptr->verInfo[index].values[4].property = DAM_LINE_TAG;
                    stiptr->verInfo[index].values[4].flags = 0;
                    stiptr->verInfo[index].values[4].size =  2;
                    stiptr->verInfo[index].values[4].offset = 8;
                    stiptr->verInfo[index].values[4].gameprop = 1;
                    // front side
                    stiptr->verInfo[index].values[5].property = DAM_SIDE0;
                    stiptr->verInfo[index].values[5].flags = DT_NOINDEX;
                    stiptr->verInfo[index].values[5].size =  2;
                    stiptr->verInfo[index].values[5].offset = 10;
                    stiptr->verInfo[index].values[5].gameprop = 0;
                    // back side
                    stiptr->verInfo[index].values[6].property = DAM_SIDE1;
                    stiptr->verInfo[index].values[6].flags = DT_NOINDEX;
                    stiptr->verInfo[index].values[6].size =  2;
                    stiptr->verInfo[index].values[6].offset = 12;
                    stiptr->verInfo[index].values[6].gameprop = 0;
                }
                else            // HEXEN format
                {
                    *mlptr = 16;
                    stiptr->verInfo[index].numValues = 11;
                    stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 11, PU_STATIC, 0);
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
                    // flags
                    stiptr->verInfo[index].values[2].property = DAM_FLAGS;
                    stiptr->verInfo[index].values[2].flags = 0;
                    stiptr->verInfo[index].values[2].size =  2;
                    stiptr->verInfo[index].values[2].offset = 4;
                    stiptr->verInfo[index].values[2].gameprop = 0;
                    // special
                    stiptr->verInfo[index].values[3].property = DAM_LINE_SPECIAL;
                    stiptr->verInfo[index].values[3].flags = 0;
                    stiptr->verInfo[index].values[3].size =  1;
                    stiptr->verInfo[index].values[3].offset = 6;
                    stiptr->verInfo[index].values[3].gameprop = 1;
                    // arg1
                    stiptr->verInfo[index].values[4].property = DAM_LINE_ARG1;
                    stiptr->verInfo[index].values[4].flags = 0;
                    stiptr->verInfo[index].values[4].size =  1;
                    stiptr->verInfo[index].values[4].offset = 7;
                    stiptr->verInfo[index].values[4].gameprop = 1;
                    // arg2
                    stiptr->verInfo[index].values[5].property = DAM_LINE_ARG2;
                    stiptr->verInfo[index].values[5].flags = 0;
                    stiptr->verInfo[index].values[5].size =  1;
                    stiptr->verInfo[index].values[5].offset = 8;
                    stiptr->verInfo[index].values[5].gameprop = 1;
                    // arg3
                    stiptr->verInfo[index].values[6].property = DAM_LINE_ARG3;
                    stiptr->verInfo[index].values[6].flags = 0;
                    stiptr->verInfo[index].values[6].size =  1;
                    stiptr->verInfo[index].values[6].offset = 9;
                    stiptr->verInfo[index].values[6].gameprop = 1;
                    // arg4
                    stiptr->verInfo[index].values[7].property = DAM_LINE_ARG4;
                    stiptr->verInfo[index].values[7].flags = 0;
                    stiptr->verInfo[index].values[7].size =  1;
                    stiptr->verInfo[index].values[7].offset = 10;
                    stiptr->verInfo[index].values[7].gameprop = 1;
                    // arg5
                    stiptr->verInfo[index].values[8].property = DAM_LINE_ARG5;
                    stiptr->verInfo[index].values[8].flags = 0;
                    stiptr->verInfo[index].values[8].size =  1;
                    stiptr->verInfo[index].values[8].offset = 11;
                    stiptr->verInfo[index].values[8].gameprop = 1;
                    // front side
                    stiptr->verInfo[index].values[9].property = DAM_SIDE0;
                    stiptr->verInfo[index].values[9].flags = DT_NOINDEX;
                    stiptr->verInfo[index].values[9].size =  2;
                    stiptr->verInfo[index].values[9].offset = 12;
                    stiptr->verInfo[index].values[9].gameprop = 0;
                    // back side
                    stiptr->verInfo[index].values[10].property = DAM_SIDE1;
                    stiptr->verInfo[index].values[10].flags = DT_NOINDEX;
                    stiptr->verInfo[index].values[10].size =  2;
                    stiptr->verInfo[index].values[10].offset = 14;
                    stiptr->verInfo[index].values[10].gameprop = 0;
                }
            }
            else if(lumpClass == LCM_SIDEDEFS)
            {
                *mlptr = 30;
                stiptr->verInfo[index].numValues = 3;
                stiptr->verInfo[index].values = Z_Malloc(sizeof(datatype_t) * 3, PU_STATIC, 0);
                // x offset
                stiptr->verInfo[index].values[0].property = DAM_TEXTURE_OFFSET_X;
                stiptr->verInfo[index].values[0].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[0].size =  2;
                stiptr->verInfo[index].values[0].offset = 0;
                stiptr->verInfo[index].values[0].gameprop = 0;
                // y offset
                stiptr->verInfo[index].values[1].property = DAM_TEXTURE_OFFSET_Y;
                stiptr->verInfo[index].values[1].flags = DT_FRACBITS;
                stiptr->verInfo[index].values[1].size =  2;
                stiptr->verInfo[index].values[1].offset = 2;
                stiptr->verInfo[index].values[1].gameprop = 0;
                // sector
                stiptr->verInfo[index].values[2].property = DAM_FRONT_SECTOR;
                stiptr->verInfo[index].values[2].flags = 0;
                stiptr->verInfo[index].values[2].size =  2;
                stiptr->verInfo[index].values[2].offset = 28;
                stiptr->verInfo[index].values[2].gameprop = 0;
            }
            else if(lumpClass == LCM_VERTEXES)
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
            else if(lumpClass == LCM_SEGS)
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
                stiptr->verInfo[index].values[3].flags = DT_NOINDEX;
                stiptr->verInfo[index].values[3].size =  2;
                stiptr->verInfo[index].values[3].offset = 6;
                stiptr->verInfo[index].values[3].gameprop = 0;
                // side
                stiptr->verInfo[index].values[4].property = DAM_SIDE;
                stiptr->verInfo[index].values[4].flags = 0;
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
            else if(lumpClass == LCM_SUBSECTORS)
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
            else if(lumpClass == LCM_NODES)
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
            else if(lumpClass == LCM_SECTORS)
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
            else if(lumpClass == LCM_REJECT)
            {
                *mlptr = 1;
            }
            else if(lumpClass == LCM_BLOCKMAP)
            {
                *mlptr = 1;
            }
        }
    }

    // Calculate the size of the gl node structs
    for(i = GLNODE_FORMATS -1; i >= 0; --i)
    {
        for(j = 0; j < NUM_LUMPCLASSES; ++j)
        {
            lumpClass = mapLumpInfo[j].lumpclass;
            index = mapLumpInfo[j].glLump;

            glver = (glNodeFormats[i].verInfo[index].version);
            glptr = &(glNodeFormats[i].verInfo[index].elmSize);
            glstiptr = &(glNodeFormats[i]);

            if(lumpClass == LCG_VERTEXES)
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
            else if(lumpClass == LCG_SEGS)
            {
                if(glver == 2)
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
                    glstiptr->verInfo[index].values[2].flags = DT_NOINDEX;
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
            else if(lumpClass == LCG_SUBSECTORS)
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
            else if(lumpClass == LCG_NODES)
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

#if _DEBUG
static void P_PrintDebugMapData(void)
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
                    (seg->sidedef != NULL)? GET_SIDE_IDX(seg->sidedef) : -1,
                    seg->offset >> FRACBITS);
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
                     (li->sidenum[1] == NO_INDEX)? -1 : li->sidenum[1]);
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
#endif

float AccurateDistance(fixed_t dx, fixed_t dy)
{
    float   fx = FIX2FLT(dx);
    float   fy = FIX2FLT(dy);

    return (float) sqrt(fx * fx + fy * fy);
}
