/* DE1: $Id$
 * Copyright (C) 2006 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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

// MACROS ------------------------------------------------------------------

// map data type flags
#define DT_UNSIGNED 0x01
#define DT_FRACBITS 0x02
#define DT_FLAT     0x04
#define DT_TEXTURE  0x08
#define DT_NOINDEX  0x10

// Internal data types
#define NUM_INTERNAL_DATA_STRUCTS 8

#define MAPDATA_FORMATS 2

#define GLNODE_FORMATS  5

// glNode lump offsets
#define GLVERT_OFFSET   1
#define GLSEG_OFFSET    2
#define GLSSECT_OFFSET  3
#define GLNODE_OFFSET   4

// TYPES -------------------------------------------------------------------

// Common map format properties
enum {
    DAM_NONE = 0,
    DAM_THING,
    DAM_VERTEX,
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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

float AccurateDistance(fixed_t dx, fixed_t dy);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void P_ReadMapData(int lumpclass, int lumps, int gllumps);

static void P_ProcessSegs(int version);

static boolean P_DetermineMapFormat(int lumps, int gllumps, char *levelID);

static void P_ReadBinaryMapData(byte *structure, int dataType, byte *buffer, size_t elmsize, int elements, int version, int values, const datatype_t *types);
static void P_ReadSideDefs(byte *structure, int dataType, byte *buffer, size_t elmsize, int elements, int version, int values, const datatype_t *types);
static void P_ReadLineDefs(byte *structure, int dataType, byte *buffer, size_t elmsize, int elements, int version, int values, const datatype_t *types);
static void P_ReadSideDefTextures(int lump);
static void P_FinishLineDefs(void);

static void P_CompileSideSpecialsList(void);

static void P_SetLineSideNum(int *side, unsigned short num);

#if _DEBUG
static void P_PrintDebugMapData(void);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     mapFormat;

// gl nodes
int     glNodeFormat;
int     firstGLvertex = 0;

// types of MAP data structure
// These arrays are temporary. Data will be provided via DED definitions.
maplump_t LumpInfo[] = {
//   lumpname    readfunc        MD  GL  internaltype   lumpclass     required?  precache?
    {"THINGS",   P_ReadBinaryMapData, 0, -1,  DAM_THING,     mlThings,     true,     false},
    {"LINEDEFS", P_ReadLineDefs,      1, -1,  DAM_LINE,      mlLineDefs,   true,     false},
    {"SIDEDEFS", P_ReadSideDefs,      2, -1,  DAM_SIDE,      mlSideDefs,   true,     false},
    {"VERTEXES", P_ReadBinaryMapData, 3, -1,  DAM_VERTEX,    mlVertexes,   true,     false},
    {"SSECTORS", P_ReadBinaryMapData, 5, -1,  DAM_SUBSECTOR, mlSSectors,   true,     false},
    {"SEGS",     P_ReadBinaryMapData, 4, -1,  DAM_SEG,       mlSegs,       true,     false},
    {"NODES",    P_ReadBinaryMapData, 6, -1,  DAM_NODE,      mlNodes,      true,     false},
    {"SECTORS",  P_ReadBinaryMapData, 7, -1,  DAM_SECTOR,    mlSectors,    true,     false},
    {"REJECT",   NULL,                8, -1,  -1,            mlReject,     false,    false},
    {"BLOCKMAP", NULL,                9, -1,  -1,            mlBlockMap,   false,    false},
    {"BEHAVIOR", NULL,                10,-1,  -1,            mlBehavior,   false,    false},
    {"GL_VERT",  P_ReadBinaryMapData, -1, 0,  DAM_VERTEX,    glVerts,      false,    false},
    {"GL_SEGS",  P_ReadBinaryMapData, -1, 1,  DAM_SEG,       glSegs,       false,    false},
    {"GL_SSECT", P_ReadBinaryMapData, -1, 2,  DAM_SUBSECTOR, glSSects,     false,    false},
    {"GL_NODES", P_ReadBinaryMapData, -1, 3,  DAM_NODE,      glNodes,      false,    false},
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

// CODE --------------------------------------------------------------------

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
static boolean P_DetermineMapFormat(int lumps, int gllumps, char *levelID)
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

boolean P_GetMapLumpFormats(int lumps, int gllumps, char *levelID)
{
    if(!P_DetermineMapFormat(lumps, gllumps, levelID))
        Con_Error("P_SetupLevel: Sorry, %s gl Nodes arn't supported",
           glNodeVer[glNodeFormat].vername);
    else
        return true;

    return false;
}

void P_LoadMapData(int mapLumpStartNum, int glLumpStartNum, char *levelId)
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

    P_GetMapLumpFormats(mapLumpStartNum, glLumpStartNum, levelId);

    if(glLumpStartNum > mapLumpStartNum)
        // no GL nodes :-(
        Con_Message("Begin loading map lumps\n");
    else
        glNodeFormat = -1;

    P_ReadMapData(mlVertexes, mapLumpStartNum, glLumpStartNum);
    P_ReadMapData(glVerts, mapLumpStartNum, glLumpStartNum);
    P_ReadMapData(mlSectors, mapLumpStartNum, glLumpStartNum);
    P_ReadMapData(mlSideDefs, mapLumpStartNum, glLumpStartNum);
    P_ReadMapData(mlLineDefs, mapLumpStartNum, glLumpStartNum);

    P_ReadSideDefTextures(mapLumpStartNum + ML_SIDEDEFS);
    P_FinishLineDefs();

    // Load/Generate the blockmap
    P_ReadBlockMap(mapLumpStartNum + ML_BLOCKMAP);

    P_ReadMapData(mlThings, mapLumpStartNum, glLumpStartNum);

    P_ReadMapData(mlSegs, mapLumpStartNum, glLumpStartNum);
    P_ReadMapData(mlSSectors, mapLumpStartNum, glLumpStartNum);
    P_ReadMapData(mlNodes, mapLumpStartNum, glLumpStartNum);

    //P_PrintDebugMapData();
}

/*
 * Scans the map data lump array and processes them
 *
 * @param: class of lumps to process
 */
void P_ReadMapData(int lumpclass, int lumps, int gllumps)
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

                case DAM_THING:
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
            Con_Message("P_ReadMapData: Done \"%s\" in %.4f seconds.\n",
                        LumpInfo[i].lumpname, (Sys_GetRealTime() - startTime) / 1000.0f);

            Z_Free(mapLumps[i]);
        }
    }
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

                    if(LONG(num) != NO_INDEX)
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

    case DAM_THING: // FIXME: This is correct for DOOM format but not Hexen!
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

#ifdef _DEBUG
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
    float   fx = FIX2FLT(dx), fy = FIX2FLT(dy);

    return (float) sqrt(fx * fx + fy * fy);
}
