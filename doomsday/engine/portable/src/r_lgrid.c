/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * r_lgrid.c: Light Grid (Large-Scale FakeRadio)
 *
 * Very simple global illumination method utilizing a 2D map->lg.grid of light
 * levels.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"

#include <math.h>
#include <assert.h>

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
  PROF_GRID_UPDATE
END_PROF_TIMERS()

#define GRID_BLOCK(g, bw, x, y)        (&(g)[(y)*(bw) + (x)])

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

int             lgEnabled = false;
static int      lgMXSample = 1; // Default is mode 1 (5 samples per block)
static int      lgBlockSize = 31;

static int      lgShowDebug = false;
static float    lgDebugSize = 1.5f;

// CODE --------------------------------------------------------------------

/**
 * Registers console variables.
 */
void LG_Register(void)
{
    C_VAR_INT("rend-bias-grid", &lgEnabled, 0, 0, 1);
    C_VAR_INT("rend-bias-grid-blocksize", &lgBlockSize, 0, 8, 1024);
    C_VAR_INT("rend-bias-grid-multisample", &lgMXSample, 0, 0, 7);

    C_VAR_INT("rend-bias-grid-debug", &lgShowDebug, 0, 0, 1);
    C_VAR_FLOAT("rend-bias-grid-debug-size", &lgDebugSize, 0, .1f, 100);
}

/**
 * Determines if the index (x, y) is in the bitfield.
 */
static boolean hasIndexBit(int x, int y, uint* bitfield, int blockWidth)
{
    uint                index = x + y * blockWidth;

    // Assume 32-bit uint.
    return (bitfield[index >> 5] & (1 << (index & 0x1f))) != 0;
}

/**
 * Sets the index (x, y) in the bitfield.
 * Count is incremented when a zero bit is changed to one.
 */
static void addIndexBit(int x, int y, uint* bitfield, int* count,
                        int blockWidth)
{
    uint                index = x + y * blockWidth;

    // Assume 32-bit uint.
    if(!hasIndexBit(index, 0, bitfield, blockWidth))
    {
        (*count)++;
    }
    bitfield[index >> 5] |= (1 << (index & 0x1f));
}

/**
 * Initialize the light map->lg.grid for the current map.
 */
void LG_Init(void)
{
    uint                startTime = Sys_GetRealTime();

#define MSFACTORS 7
    typedef struct lgsamplepoint_s {
        float           pos[3];
    } lgsamplepoint_t;
    // Diagonal in maze arrangement of natural numbers.
    // Up to 65 samples per-block(!)
    static int          multisample[] = {1, 5, 9, 17, 25, 37, 49, 65};

    float               max[3], width, height;
    int                 i = 0, a, b, x, y, count, changedCount;
    size_t              bitfieldSize;
    uint*               indexBitfield = 0, *contributorBitfield = 0;
    lgridblock_t*        block;
    int*                sampleResults = 0, n, size, numSamples, center, best;
    uint                s;
    float               off[2];
    lgsamplepoint_t*    samplePoints = 0, sample;

    sector_t**          ssamples;
    sector_t**          blkSampleSectors;
    gamemap_t*          map = P_GetCurrentMap();

    if(!lgEnabled)
    {
        map->lg.inited = false;
        return;
    }

    map->lg.needsUpdate = true;

    // Allocate the map->lg.grid.
    P_GetMapBounds(map, &map->lg.origin[0], &max[0]);

    width  = max[VX] - map->lg.origin[VX];
    height = max[VY] - map->lg.origin[VY];

    map->lg.blockWidth  = ROUND(width / lgBlockSize) + 1;
    map->lg.blockHeight = ROUND(height / lgBlockSize) + 1;

    // Clamp the multisample factor.
    if(lgMXSample > MSFACTORS)
        lgMXSample = MSFACTORS;
    else if(lgMXSample < 0)
        lgMXSample = 0;

    numSamples = multisample[lgMXSample];

    // Allocate memory for sample points array.
    samplePoints = M_Malloc(sizeof(lgsamplepoint_t) * numSamples);

    /**
     * It would be possible to only allocate memory for the unique
     * sample results. And then select the appropriate sample in the loop
     * for initializing the map->lg.grid instead of copying the previous results in
     * the loop for acquiring the sample points.
     *
     * Calculate with the equation (number of unique sample points):
     *
     * ((1 + map->lg.blockHeight * lgMXSample) * (1 + map->lg.blockWidth * lgMXSample)) +
     *     (size % 2 == 0? numBlocks : 0)
     * OR
     *
     * We don't actually need to store the ENTIRE sample points array. It
     * would be sufficent to only store the results from the start of the
     * previous row to current col index. This would save a bit of memory.
     *
     * However until lightgrid init is finalized it would be rather silly
     * to optimize this much further.
     */

    // Allocate memory for all the sample results.
    ssamples = M_Malloc(sizeof(sector_t*) *
                        ((map->lg.blockWidth * map->lg.blockHeight) * numSamples));

    // Determine the size^2 of the samplePoint array plus its center.
    size = center = 0;
    if(numSamples > 1)
    {
        float       f = sqrt(numSamples);

        if(ceil(f) != floor(f))
        {
            size = sqrt(numSamples -1);
            center = 0;
        }
        else
        {
            size = (int) f;
            center = size+1;
        }
    }

    // Construct the sample point offset array.
    // This way we can use addition only during calculation of:
    // (map->lg.blockHeight*map->lg.blockWidth)*numSamples

    if(center == 0)
    {   // Zero is the center so do that first.
        samplePoints[0].pos[VX] = lgBlockSize / 2;
        samplePoints[0].pos[VY] = lgBlockSize / 2;
    }

    if(numSamples > 1)
    {
        float       bSize = (float) lgBlockSize / (size-1);

        // Is there an offset?
        if(center == 0)
            n = 1;
        else
            n = 0;

        for(y = 0; y < size; ++y)
            for(x = 0; x < size; ++x, ++n)
            {
                samplePoints[n].pos[VX] = ROUND(x * bSize);
                samplePoints[n].pos[VY] = ROUND(y * bSize);
            }
    }

/*
#if _DEBUG
    for(n = 0; n < numSamples; ++n)
        Con_Message(" %i of %i %i(%f %f)\n",
                    n, numSamples, (n == center)? 1 : 0,
                    samplePoints[n].pos[VX], samplePoints[n].pos[VY]);
#endif
*/

    // Acquire the sectors at ALL the sample points.
    for(y = 0; y < map->lg.blockHeight; ++y)
    {
        off[VY] = y * lgBlockSize;
        for(x = 0; x < map->lg.blockWidth; ++x)
        {
            int         blk = (x + y * map->lg.blockWidth);
            int         idx;

            off[VX] = x * lgBlockSize;

            n = 0;
            if(center == 0)
            {   // Center point is not considered with the term 'size'.
                // Sample this point and place at index 0 (at the start
                // of the samples for this block).
                idx = blk * (numSamples);

                sample.pos[VX] = map->lg.origin[VX] + off[VX] + samplePoints[0].pos[VX];
                sample.pos[VY] = map->lg.origin[VY] + off[VY] + samplePoints[0].pos[VY];

                ssamples[idx] = ((subsector_t*)
                    R_PointInSubSector(sample.pos[VX], sample.pos[VY])->data)->sector;
                if(!R_IsPointInSector2(sample.pos[VX], sample.pos[VY], ssamples[idx]))
                   ssamples[idx] = NULL;

                n++; // Offset the index in the samplePoints array bellow.
            }

            count = blk * size;
            for(b = 0; b < size; ++b)
            {
                i = (b + count) * size;
                for(a = 0; a < size; ++a, ++n)
                {
                    idx = a + i;

                    if(center == 0)
                        idx += blk + 1;

                    if(numSamples > 1 && ((x > 0 && a == 0) || (y > 0 && b == 0)))
                    {   // We have already sampled this point.
                        // Get the previous result.
                        int         prevX, prevY, prevA, prevB;
                        int         previdx;

                        prevX = x; prevY = y; prevA = a; prevB = b;
                        if(x > 0 && a == 0)
                        {
                            prevA = size -1;
                            prevX--;
                        }
                        if(y > 0 && b == 0)
                        {
                            prevB = size -1;
                            prevY--;
                        }

                        previdx = prevA + (prevB + (prevX + prevY * map->lg.blockWidth) * size) * size;

                        if(center == 0)
                            previdx += (prevX + prevY * map->lg.blockWidth) + 1;

                        ssamples[idx] = ssamples[previdx];
                    }
                    else
                    {   // We haven't sampled this point yet.
                        sample.pos[VX] = map->lg.origin[VX] + off[VX] + samplePoints[n].pos[VX];
                        sample.pos[VY] = map->lg.origin[VY] + off[VY] + samplePoints[n].pos[VY];

                        ssamples[idx] = ((subsector_t*)
                            R_PointInSubSector(sample.pos[VX], sample.pos[VY])->data)->sector;
                        if(!R_IsPointInSector2(sample.pos[VX], sample.pos[VY], ssamples[idx]))
                           ssamples[idx] = NULL;
                    }
                }
            }
        }
    }
    // We're done with the samplePoints block.
    M_Free(samplePoints);

    // Bitfields for marking affected blocks. Make sure each bit is in a word.
    bitfieldSize = 4 * (31 + map->lg.blockWidth * map->lg.blockHeight) / 32;
    indexBitfield = M_Calloc(bitfieldSize);
    contributorBitfield = M_Calloc(bitfieldSize);

    // \todo It would be possible to only allocate memory for the map->lg.grid
    // blocks that are going to be in use.

    // Allocate memory for the entire map->lg.grid.
    map->lg.grid = Z_Calloc(sizeof(lgridblock_t) * map->lg.blockWidth * map->lg.blockHeight,
                    PU_MAP, NULL);

    Con_Message("LG_Init: %i x %i map->lg.grid (%lu bytes).\n",
                map->lg.blockWidth, map->lg.blockHeight,
                (unsigned long) (sizeof(lgridblock_t) * map->lg.blockWidth * map->lg.blockHeight));

    // Allocate memory used for the collection of the sample results.
    blkSampleSectors = M_Malloc(sizeof(sector_t*) * numSamples);
    if(numSamples > 1)
        sampleResults = M_Calloc(sizeof(int) * numSamples);

    // Initialize the map->lg.grid.
    for(block = map->lg.grid, y = 0; y < map->lg.blockHeight; ++y)
    {
        off[VY] = y * lgBlockSize;
        for(x = 0; x < map->lg.blockWidth; ++x, ++block)
        {
            off[VX] = x * lgBlockSize;

            /**
             * Pick the sector at each of the sample points.
             * \todo We don't actually need the blkSampleSectors array
             * anymore. Now that ssamples stores the results consecutively
             * a simple index into ssamples would suffice.
             * However if the optimization to save memory is implemented as
             * described in the comments above we WOULD still require it.
             * Therefore, for now I'm making use of it to clarify the code.
             */
            n = (x + y * map->lg.blockWidth) * numSamples;
            for(i = 0; i < numSamples; ++i)
                blkSampleSectors[i] = ssamples[i + n];

            block->sector = NULL;

            if(numSamples == 1)
            {
                block->sector = blkSampleSectors[center];
            }
            else
            {   // Pick the sector which had the most hits.
                best = -1;
                memset(sampleResults, 0, sizeof(int) * numSamples);
                for(i = 0; i < numSamples; ++i)
                    if(blkSampleSectors[i])
                        for(a = 0; a < numSamples; ++a)
                            if(blkSampleSectors[a] == blkSampleSectors[i] &&
                               blkSampleSectors[a])
                            {
                                sampleResults[a]++;
                                if(sampleResults[a] > best)
                                    best = i;
                            }

                if(best != -1)
                {   // Favour the center sample if its a draw.
                    if(sampleResults[best] == sampleResults[center] &&
                       blkSampleSectors[center])
                        block->sector = blkSampleSectors[center];
                    else
                        block->sector = blkSampleSectors[best];
                }
            }
        }
    }
    // We're done with sector samples completely.
    M_Free(ssamples);
    // We're done with the sample results arrays.
    M_Free(blkSampleSectors);
    if(numSamples > 1)
        M_Free(sampleResults);

    // Find the blocks of all sectors.
    for(s = 0; s < numSectors; ++s)
    {
        sector_t*           sector = SECTOR_PTR(s);

        // Clear the bitfields.
        memset(indexBitfield, 0, bitfieldSize);
        memset(contributorBitfield, 0, bitfieldSize);
        count = changedCount = 0;

        for(block = map->lg.grid, y = 0; y < map->lg.blockHeight; ++y)
        {
            for(x = 0; x < map->lg.blockWidth; ++x, ++block)
            {
                if(block->sector == sector)
                {
                    // \todo Determine min/max a/b before going into the loop.
                    for(b = -2; b <= 2; ++b)
                    {
                        if(y + b < 0 || y + b >= map->lg.blockHeight)
                            continue;

                        for(a = -2; a <= 2; ++a)
                        {
                            if(x + a < 0 || x + a >= map->lg.blockWidth)
                                continue;

                            addIndexBit(x + a, y + b, indexBitfield,
                                        &changedCount, map->lg.blockWidth);
                        }
                    }
                }
            }
        }

        // Determine contributor blocks. Contributors are the blocks that are
        // close enough to contribute light to affected blocks.
        for(y = 0; y < map->lg.blockHeight; ++y)
        {
            for(x = 0; x < map->lg.blockWidth; ++x)
            {
                if(!hasIndexBit(x, y, indexBitfield, map->lg.blockWidth))
                    continue;

                // Add the contributor blocks.
                for(b = -2; b <= 2; ++b)
                {
                    if(y + b < 0 || y + b >= map->lg.blockHeight)
                        continue;

                    for(a = -2; a <= 2; ++a)
                    {
                        if(x + a < 0 || x + a >= map->lg.blockWidth)
                            continue;

                        if(!hasIndexBit(x + a, y + b, indexBitfield,
                                        map->lg.blockWidth))
                        {
                            addIndexBit(x + a, y + b, contributorBitfield,
                                        &count, map->lg.blockWidth);
                        }
                    }
                }
            }
        }

/*if _DEBUG
Con_Message("  Sector %i: %i / %i\n", s, changedCount, count);
#endif*/

        sector->changedBlockCount = changedCount;
        sector->blockCount = changedCount + count;

        if(sector->blockCount > 0)
        {
            sector->blocks = Z_Malloc(sizeof(unsigned short) * sector->blockCount,
                                      PU_MAPSTATIC, 0);
            for(x = 0, a = 0, b = changedCount;
                x < map->lg.blockWidth * map->lg.blockHeight;
                ++x)
            {
                if(hasIndexBit(x, 0, indexBitfield, map->lg.blockWidth))
                    sector->blocks[a++] = x;
                else if(hasIndexBit(x, 0, contributorBitfield, map->lg.blockWidth))
                    sector->blocks[b++] = x;
            }

            assert(a == changedCount);
            //assert(b == info->blockCount);
        }
        else
        {
            sector->blocks = NULL;
        }
    }

    M_Free(indexBitfield);
    M_Free(contributorBitfield);

    map->lg.inited = true;

    // How much time did we spend?
    VERBOSE(Con_Message
            ("LG_Init: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

/**
 * Apply the sector's lighting to the block.
 */
static void LG_ApplySector(lgridblock_t* block, const float* color, float level,
                           float factor, int bias)
{
    int                 i;

    // Apply a bias to the light level.
    level -= (0.95f - level);
    if(level < 0)
        level = 0;

    level *= factor;

    if(level <= 0)
        return;

    for(i = 0; i < 3; ++i)
    {
        float           c = color[i] * level;

        c = MINMAX_OF(0, c, 1);

        if(block->rgb[i] + c > 1)
        {
            block->rgb[i] = 1;
        }
        else
        {
            block->rgb[i] += c;
        }
    }

    // Influenced by the source bias.
    i = block->bias * (1 - factor) + bias * factor;
    i = MINMAX_OF(-0x80, i, 0x7f);
    block->bias = i;
}

/**
 * Called when a sector has changed its light level.
 */
void LG_SectorChanged(sector_t* sector)
{
    uint                i, j;
    unsigned short      n;
    gamemap_t*          map = P_GetCurrentMap();

    if(!map->lg.inited)
        return;

    // Mark changed blocks and contributors.
    for(i = 0; i < sector->changedBlockCount; ++i)
    {
        n = sector->blocks[i];
        // The color will be recalculated.
        if(!(map->lg.grid[n].flags & GBF_CHANGED))
            memcpy(map->lg.grid[n].oldRGB, map->lg.grid[n].rgb, sizeof(map->lg.grid[n].oldRGB));

        for(j = 0; j < 3; ++j)
            map->lg.grid[n].rgb[j] = 0;

        map->lg.grid[n].flags |= GBF_CHANGED | GBF_CONTRIBUTOR;
    }

    for(; i < sector->blockCount; ++i)
    {
        map->lg.grid[sector->blocks[i]].flags |= GBF_CONTRIBUTOR;
    }

    map->lg.needsUpdate = true;
}

/**
 * Called when a setting is changed which affects the lightgrid.
 */
void LG_MarkAllForUpdate(cvar_t* unused)
{
    uint                i;
    gamemap_t*          map = P_GetCurrentMap();

    if(!map->lg.inited)
        return;

    // Mark all blocks and contributors.
    for(i = 0; i < numSectors; ++i)
    {
        LG_SectorChanged(&sectors[i]);
    }
}

#if 0
/*
 * Determines whether it is necessary to recalculate the lighting of a
 * map->lg.grid block.  Updates are needed when there has been a light level
 * or color change in a sector that affects the block.
 */
static boolean LG_BlockNeedsUpdate(int x, int y)
{
    // First check the block itself.
    lgridblock_t*        block = GRID_BLOCK(map->lg.grid, map->lg.blockWidth, x, y);
    sector_t*           blockSector;
    int                 a, b, limitA[2], limitB;

    blockSector = block->sector;
    if(!blockSector)
    {
        // The sector needs to be determined.
        return true;
    }

    if(SECT_INFO(blockSector)->flags & SIF_LIGHT_CHANGED)
    {
        return true;
    }

    // Check neighbor blocks as well.
    // Determine neighbor boundaries.  Y coordinate.
    if(y >= 2)
    {
        b = y - 2;
    }
    else
    {
        b = 0;
    }
    if(y <= map->lg.blockHeight - 3)
    {
        limitB = y + 2;
    }
    else
    {
        limitB = map->lg.blockHeight - 1;
    }

    // X coordinate.
    if(x >= 2)
    {
        limitA[0] = x - 2;
    }
    else
    {
        limitA[0] = 0;
    }
    if(x <= map->lg.blockWidth - 3)
    {
        limitA[1] = x + 2;
    }
    else
    {
        limitA[1] = map->lg.blockWidth - 1;
    }

    // Iterate through neighbors.
    for(; b <= limitB; ++b)
    {
        a = limitA[0];
        block = GRID_BLOCK(map->lg.grid, map->lg.blockWidth, a, b);

        for(; a <= limitA[1]; ++a, ++block)
        {
            if(!a && !b) continue;

            if(block->sector == blockSector)
                continue;

            if(SECT_INFO(block->sector)->flags & SIF_LIGHT_CHANGED)
            {
                return true;
            }
        }
    }

    return false;
}
#endif

/**
 * Update the map->lg.grid by finding the strongest light source in each grid
 * block.
 */
void LG_Update(void)
{
    static const float  factors[5 * 5] =
    {
        .1f, .2f, .25f, .2f, .1f,
        .2f, .4f, .5f, .4f, .2f,
        .25f, .5f, 1.f, .5f, .25f,
        .2f, .4f, .5f, .4f, .2f,
        .1f, .2f, .25f, .2f, .1f
    };

    lgridblock_t*       block, *lastBlock, *other;
    int                 x, y, a, b;
    sector_t*           sector;
    const float*        color;
    int                 bias, height;
    gamemap_t*          map = P_GetCurrentMap();

#ifdef DD_PROFILE
    static int          i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF( PROF_GRID_UPDATE );
    }
#endif

    if(!map || !map->lg.inited || !map->lg.needsUpdate)
        return;

BEGIN_PROF( PROF_GRID_UPDATE );

#if 0
    for(block = map->lg.grid, y = 0; y < map->lg.blockHeight; ++y)
    {
        for(x = 0; x < map->lg.blockWidth; ++x, block++)
        {
            if(LG_BlockNeedsUpdate(x, y))
            {
                block->flags |= GBF_CHANGED;

                // Clear to zero (will be recalculated).
                memset(block->rgb, 0, sizeof(float) * 3);

                // Mark contributors.
                for(b = -2; b <= 2; ++b)
                {
                    if(y + b < 0 || y + b >= map->lg.blockHeight)
                        continue;

                    for(a = -2; a <= 2; ++a)
                    {
                        if(x + a < 0 || x + a >= map->lg.blockWidth)
                            continue;

                        GRID_BLOCK(map->lg.grid, map->lg.blockWidth, x + a, y + b)->flags |= GBF_CONTRIBUTOR;
                    }
                }
            }
            else
            {
                block->flags &= ~GBF_CHANGED;
            }
        }
    }
#endif

    for(block = map->lg.grid, y = 0; y < map->lg.blockHeight; ++y)
    {
        for(x = 0; x < map->lg.blockWidth; ++x, ++block)
        {
            boolean             isSkyFloor, isSkyCeil;

            // Unused blocks can't contribute.
            if(!(block->flags & GBF_CONTRIBUTOR) || !block->sector)
                continue;

            // Determine the color of the ambient light in this sector.
            sector = block->sector;
            color = R_GetSectorLightColor(sector);
            height = (int) (sector->SP_ceilheight - sector->SP_floorheight);

            isSkyFloor = IS_SKYSURFACE(&sector->SP_ceilsurface);
            isSkyCeil = IS_SKYSURFACE(&sector->SP_floorsurface);

            if(isSkyFloor && !isSkyCeil)
            {
                bias = -height / 6;
            }
            else if(!isSkyFloor && isSkyCeil)
            {
                bias = height / 6;
            }
            else if(height > 100)
            {
                bias = (height - 100) / 2;
            }
            else
            {
                bias = 0;
            }

            // \todo Calculate min/max for a and b.
            for(a = -2; a <= 2; ++a)
            {
                for(b = -2; b <= 2; ++b)
                {
                    if(x + a < 0 || y + b < 0 || x + a > map->lg.blockWidth - 1 ||
                       y + b > map->lg.blockHeight - 1) continue;

                    other = GRID_BLOCK(map->lg.grid, map->lg.blockWidth, x + a, y + b);

                    if(other->flags & GBF_CHANGED)
                    {
                        LG_ApplySector(other, color, sector->lightLevel,
                                       factors[(b + 2)*5 + a + 2]/8, bias);
                    }
                }
            }
        }
    }

    // Clear all changed and contribution flags.
    lastBlock = &map->lg.grid[map->lg.blockWidth * map->lg.blockHeight];
    for(block = map->lg.grid; block != lastBlock; ++block)
    {
        block->flags = 0;
    }

    map->lg.needsUpdate = false;

END_PROF( PROF_GRID_UPDATE );
}

/**
 * Calculate the light level for a 3D point in the world.
 *
 * @param point         3D point.
 * @param color         Evaluated color of the point (return value).
 */
void LG_Evaluate(const float* point, float* color)
{
    int                 x, y, i;
    float               dz = 0, dimming;
    lgridblock_t*        block;
    gamemap_t*          map = P_GetCurrentMap();

    if(!map->lg.inited)
    {
        memset(color, 0, sizeof(float) * 3);
        return;
    }

    x = ROUND((point[VX] - map->lg.origin[VX]) / lgBlockSize);
    y = ROUND((point[VY] - map->lg.origin[VY]) / lgBlockSize);
    x = MINMAX_OF(1, x, map->lg.blockWidth - 2);
    y = MINMAX_OF(1, y, map->lg.blockHeight - 2);

    block = &map->lg.grid[y * map->lg.blockWidth + x];

    if(block->sector)
    {
        if(block->bias < 0)
        {
            // Calculate Z difference to the ceiling.
            dz = block->sector->SP_ceilheight - point[VZ];
        }
        else if(block->bias > 0)
        {
            // Calculate Z difference to the floor.
            dz = point[VZ] - block->sector->SP_floorheight;
        }

        dz -= 50;
        if(dz < 0)
            dz = 0;

        if(block->flags & GBF_CHANGED)
        {   // We are waiting for an updated value, for now use the old.
            color[CR] = block->oldRGB[CR];
            color[CG] = block->oldRGB[CG];
            color[CB] = block->oldRGB[CB];
        }
        else
        {
            color[CR] = block->rgb[CR];
            color[CG] = block->rgb[CG];
            color[CB] = block->rgb[CB];
        }
    }
    else
    {   // The block has no sector!?
        // Must be an error in the lightgrid covering sector determination.
        // Init to black.
        color[CR] = color[CG] = color[CB] = 0;
    }

    // Biased ambient light causes a dimming in the Z direction.
    if(dz && block->bias)
    {
        if(block->bias < 0)
            dimming = 1 - (dz * (float) -block->bias) / 35000.0f;
        else
            dimming = 1 - (dz * (float) block->bias) / 35000.0f;

        if(dimming < .5f)
            dimming = .5f;

        for(i = 0; i < 3; ++i)
        {
            // Apply the dimming
            color[i] *= dimming;

            // Add the light range compression factor
            color[i] += R_LightAdaptationDelta(color[i]);
        }
    }
    else
    {
        // Just add the light range compression factor
        for(i = 0; i < 3; ++i)
            color[i] += R_LightAdaptationDelta(color[i]);
    }
}

/**
 * Draw the map->lg.grid in 2D HUD mode.
 */
void LG_Debug(void)
{
    static int          blink = 0;

    lgridblock_t*       block;
    int                 x, y;
    int                 vx, vy;
    size_t              vIdx, blockIdx;
    ddplayer_t*         ddpl = (viewPlayer? &viewPlayer->shared : NULL);
    gamemap_t*          map = P_GetCurrentMap();

    if(!map || !map->lg.inited || !lgShowDebug)
        return;

    if(ddpl)
    {
        blink++;
        vx = ROUND((ddpl->mo->pos[VX] - map->lg.origin[VX]) / lgBlockSize);
        vy = ROUND((ddpl->mo->pos[VY] - map->lg.origin[VY]) / lgBlockSize);
        vx = MINMAX_OF(1, vx, map->lg.blockWidth - 2);
        vy = MINMAX_OF(1, vy, map->lg.blockHeight - 2);
        vIdx = vy * map->lg.blockWidth + vx;
    }

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    glDisable(GL_TEXTURE_2D);

    for(y = 0; y < map->lg.blockHeight; ++y)
    {
        glBegin(GL_QUADS);
        for(x = 0; x < map->lg.blockWidth; ++x, ++block)
        {
            blockIdx = (map->lg.blockHeight - 1 - y) * map->lg.blockWidth + x;
            block = &map->lg.grid[blockIdx];

            if(ddpl && vIdx == blockIdx && (blink & 16))
            {
                glColor3f(1, 0, 0);
            }
            else
            {
                if(!block->sector)
                    continue;

                glColor3fv(block->rgb);
            }

            glVertex2f(x * lgDebugSize, y * lgDebugSize);
            glVertex2f(x * lgDebugSize + lgDebugSize, y * lgDebugSize);
            glVertex2f(x * lgDebugSize + lgDebugSize,
                       y * lgDebugSize + lgDebugSize);
            glVertex2f(x * lgDebugSize, y * lgDebugSize + lgDebugSize);
        }
        glEnd();
    }

    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
