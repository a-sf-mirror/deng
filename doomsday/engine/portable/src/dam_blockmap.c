/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * dam_blockmap.c: BlockMap generation.
 *
 * Generate valid blockmap data from the already loaded map data.
 * Adapted from algorithm used in prBoom 2.2.6 -DJS
 *
 * Algorithm is order of nlines*(ncols+nrows) not nlines*ncols*nrows
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_misc.h"
#include "de_play.h"

#include "p_mapdata.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

// MACROS ------------------------------------------------------------------

#define BLKMARGIN               (8) // size guardband around map
#define MAPBLOCKUNITS           128

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Construct a blockmap from the map data.
 *
 * This finds the intersection of each linedef with the column and row
 * lines at the left and bottom of each blockmap cell. It then adds the
 * line to all block lists touching the intersection.
 */
static linedefblockmap_t* buildLineDefBlockmap(gamemap_t* map)
{
    uint i;
    uint bMapWidth, bMapHeight; // Blockmap dimensions.
    vec2_t blockSize; // Size of the blocks.
    uint numBlocks; // Number of cells = nrows*ncols.
    vec2_t bounds[2], point, dims;
    vertex_t* vtx;
    linedefblockmap_t* blockmap;

    // Scan for map limits, which the blockmap must enclose.
    for(i = 0; i < Map_HalfEdgeDS(map)->numVertices; ++i)
    {
        vtx = Map_HalfEdgeDS(map)->vertices[i];

        V2_Set(point, vtx->pos[VX], vtx->pos[VY]);
        if(!i)
            V2_InitBox(bounds, point);
        else
            V2_AddToBox(bounds, point);
    }

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], bounds[0][VX] - BLKMARGIN, bounds[0][VY] - BLKMARGIN);
    V2_Set(bounds[1], bounds[1][VX] + BLKMARGIN, bounds[1][VY] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, MAPBLOCKUNITS, MAPBLOCKUNITS);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[VX] <= blockSize[VX])
        bMapWidth = 1;
    else
        bMapWidth = ceil(dims[VX] / blockSize[VX]);

    if(dims[VY] <= blockSize[VY])
        bMapHeight = 1;
    else
        bMapHeight = ceil(dims[VY] / blockSize[VY]);
    numBlocks = bMapWidth * bMapHeight;

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][VX] + bMapWidth  * blockSize[VX],
                      bounds[0][VY] + bMapHeight * blockSize[VY]);

    blockmap = P_CreateLineDefBlockmap(bounds[0], bounds[1], bMapWidth, bMapHeight);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* lineDef = map->lineDefs[i];

        LineDefBlockmap_Insert(blockmap, lineDef);
    }

    return blockmap;
}

void Map_BuildLineDefBlockmap(gamemap_t* map)
{
    map->_lineDefBlockmap = buildLineDefBlockmap(map);
}

/**
 * Construct a blockmap from the map data.
 *
 * This finds the intersection of each linedef with the column and row
 * lines at the left and bottom of each blockmap cell. It then adds the
 * line to all block lists touching the intersection.
 */
static mobjblockmap_t* buildMobjBlockmap(gamemap_t* map)
{
    uint i, bMapWidth, bMapHeight; // Blockmap dimensions.
    vec2_t blockSize; // Size of the blocks.
    vec2_t bounds[2], dims;
    mobjblockmap_t* blockmap;

    // Scan for map limits, which the blockmap must enclose.
    for(i = 0; i < Map_HalfEdgeDS(map)->numVertices; ++i)
    {
        vertex_t* vtx = Map_HalfEdgeDS(map)->vertices[i];
        vec2_t point;

        V2_Set(point, vtx->pos[VX], vtx->pos[VY]);
        if(!i)
            V2_InitBox(bounds, point);
        else
            V2_AddToBox(bounds, point);
    }

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], bounds[0][VX] - BLKMARGIN, bounds[0][VY] - BLKMARGIN);
    V2_Set(bounds[1], bounds[1][VX] + BLKMARGIN, bounds[1][VY] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, MAPBLOCKUNITS, MAPBLOCKUNITS);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[VX] <= blockSize[VX])
        bMapWidth = 1;
    else
        bMapWidth = ceil(dims[VX] / blockSize[VX]);

    if(dims[VY] <= blockSize[VY])
        bMapHeight = 1;
    else
        bMapHeight = ceil(dims[VY] / blockSize[VY]);

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][VX] + bMapWidth  * blockSize[VX],
                      bounds[0][VY] + bMapHeight * blockSize[VY]);

    blockmap = P_CreateMobjBlockmap(bounds[0], bounds[1], bMapWidth, bMapHeight);

    return blockmap;
}

void Map_BuildMobjBlockmap(gamemap_t* map)
{
    map->_mobjBlockmap = buildMobjBlockmap(map);
}
