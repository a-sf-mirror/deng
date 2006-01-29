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

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       32*FRACUNIT

// internal blockmap stuff
#define BLKSHIFT 7                // places to shift rel position for cell num
#define BLKMASK ((1<<BLKSHIFT)-1) // mask for rel position within cell
#define BLKMARGIN 0               // size guardband around map used

// TYPES -------------------------------------------------------------------

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
    ML_BLOCKMAP,                   // LUT, motion clipping, walls/grid element
    ML_BEHAVIOR                    // ACS Scripts (not supported currently)
};

// used to list lines in each block
typedef struct linelist_t {
    long num;
    struct linelist_t *next;
} linelist_t;

// Bad texture record
typedef struct {
    char *name;
    boolean planeTex;
    int count;
} badtex_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_CheckLevel(char *levelID, boolean silent);
static void P_GroupLines(void);
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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// for sector->linecount
int numUniqueLines;

// The following is used in error fixing/detection/reporting:
// missing sidedefs
int numMissingFronts = 0;
int *missingFronts = NULL;

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

/*
 *  Make sure all texture references in the level data are good.
 */
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

/*
 * Begin the process of loading a new map.
 * Can be accessed by the games via the public API.
 *
 * @param levelId   Identifier of the map to be loaded (eg "E1M1").
 *
 * @return boolean  (True) If we loaded the map successfully.
 */
boolean P_LoadMap(char *levelId)
{
    // It would be very cool if map loading happened in another
    // thread. That way we could be keeping ourselves busy while
    // the intermission is played...

    // We could even try to divide a HUB up into zones, so that
    // when a player enters a zone we could begin loading the map(s)
    // reachable through exits in that zone (providing they have
    // enough free memory of course) so that transitions are
    // (potentially) seamless :-)

    // Attempt to load the map
    if(P_LoadMapData(levelId))
    {
        // ALL the map data was loaded/generated successfully.
        // Do any initialization/error checking work we need to do.

        // Must be called before we go any further
        P_CheckLevel(levelId, false);

        // Must be called before any mobjs are spawned.
        Con_Message("Init links\n");
        R_SetupLevel(levelId, DDSLF_INIT_LINKS);

        Con_Message("Group lines\n");
        P_GroupLines();

        // Inform the game of our success.
        return true;
    }
    else
    {
        // Unsuccessful... inform the game.
        return false;
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
 * @param silent - don't announce non-critical errors
 */
static void P_CheckLevel(char *levelID, boolean silent)
{
    int i, printCount;
    boolean canContinue = !numMissingFronts;
    //boolean hasErrors = (numBadTexNames || numMissingFronts);

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

    // Dont need this stuff anymore
    if(missingFronts != NULL)
        free(missingFronts);

    P_FreeBadTexList();

    if(!canContinue)
    {
        Con_Error("\nP_CheckLevel: Critical errors encountered "
                  "(marked with '!').\n  You will need to fix these errors in "
                  "order to play this map.\n");
    }
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
 * Checks texture *name to see if it is a valid name.
 * Or decompose it into a colormap reference, color value etc...
 *
 * Returns the id of the texture else -1. If we are currently
 * doing level setup and the name isn't valid - add the name
 * to the "bad textures" list so it can be presented to the
 * user (in P_CheckMap()) instead of loads of duplicate missing
 * texture messages from Doomsday.
 *
 * @param planeTex: true = *name comes from a map data field
 *                         intended for plane textures (Flats)
 */
int P_CheckTexture(char *name, boolean planeTex, int dataType,
                   unsigned int element, int property)
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
    if(id == -1 && gx.HandleMapDataPropertyValue)
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
