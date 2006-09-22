/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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

/*
 * p_mapspec.c:
 *
 * Line Tag handling. Line and Sector groups. Specialized iteration
 * routines, respective utility functions...
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "p_mapsetup.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct linetaglist_s {
    int         tag;
    linelist_t *list;
} linetaglist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

linelist_t  *spechit; // for crossed line specials.
linelist_t  *linespecials; // for surfaces that tick eg wall scrollers.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static linetaglist_t *lineTagLists = NULL;
static int numLineTagLists = 0;

// CODE --------------------------------------------------------------------

/**
 *
 */
void P_DestroyLineTagLists(void)
{
    int     i;

    if(numLineTagLists == 0)
        return;

    for(i = 0; i < numLineTagLists; ++i)
    {
        P_EmptyLineList(lineTagLists[i].list);
        P_DestroyLineList(lineTagLists[i].list);
    }

    free(lineTagLists);
    lineTagLists = NULL;
    numLineTagLists = 0;
}

/**
 *
 */
linelist_t *P_GetLineListForTag(int tag, boolean createNewList)
{
    int         i;
    linetaglist_t *tagList;

    // Do we have an existing list for this tag?
    for(i = 0; i < numLineTagLists; ++i)
        if(lineTagLists[i].tag == tag)
            return lineTagLists[i].list;

    if(!createNewList)
        return NULL;

    // Nope, we need to allocate another.
    numLineTagLists++;
    lineTagLists = realloc(lineTagLists, sizeof(linetaglist_t) * numLineTagLists);
    tagList = &lineTagLists[numLineTagLists - 1];
    tagList->tag = tag;

    return (tagList->list = P_CreateLineList());
}

/**
 * Iterates all sectors which tag equals <code>tag</code>.
 *
 * @param tag       The sector tag to match.
 * @param start     If <code>NULL</code> iteration will begin from the
 *                  the start ELSE iteration will continue from this sector
 *
 * @return          The next sector ELSE <code>NULL</code>;
 */
sector_t *P_IterateTaggedSectors(int tag, sector_t *start)
{
    int         i;
    sector_t   *sec;

    if(tag < 0)
        return NULL;

    if(start)
        i = P_ToIndex(start) + 1;
    else
        i = 0;

    for(; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        if(P_XSector(sec)->tag == tag)
            return sec;
    }

    return NULL;
}

/**
 * Return sector_t * of sector next to current.
 * NULL if not two-sided line
 */
sector_t *P_GetNextSector(line_t *line, sector_t *sec)
{
    if(!(P_GetIntp(line, DMU_FLAGS) & ML_TWOSIDED))
        return NULL;

    if(P_GetPtrp(line, DMU_FRONT_SECTOR) == sec)
        return P_GetPtrp(line, DMU_BACK_SECTOR);

    return P_GetPtrp(line, DMU_FRONT_SECTOR);
}

/**
 * Find lowest floor height in surrounding sectors.
 */
fixed_t P_FindLowestFloorSurrounding(sector_t *sec)
{
    int         i;
    int         lineCount;
    fixed_t     floor;
    line_t     *check;
    sector_t   *other;

    floor = P_GetFixedp(sec, DMU_FLOOR_HEIGHT);
    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = P_GetNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(other, DMU_FLOOR_HEIGHT) < floor)
            floor = P_GetFixedp(other, DMU_FLOOR_HEIGHT);
    }

    return floor;
}

/**
 * Find highest floor height in surrounding sectors.
 */
fixed_t P_FindHighestFloorSurrounding(sector_t *sec)
{
    int         i;
    int         lineCount;
    fixed_t     floor = -500 * FRACUNIT;
    line_t     *check;
    sector_t   *other;

    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = P_GetNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(other, DMU_FLOOR_HEIGHT) > floor)
            floor = P_GetFixedp(other, DMU_FLOOR_HEIGHT);
    }

    return floor;
}

/**
 * Passed a sector and a floor height, returns the fixed point value
 * of the smallest floor height in a surrounding sector larger than
 * the floor height passed. If no such height exists the floorheight
 * passed is returned.
 *
 * DJS - Rewritten using Lee Killough's algorithm for avoiding the
 *       the fixed array.
 */
fixed_t P_FindNextHighestFloor(sector_t *sec, int currentheight)
{
    int         i;
    int         lineCount;
    fixed_t     otherHeight;
    fixed_t     anotherHeight;
    line_t     *check;
    sector_t   *other;

    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = P_GetNextSector(check, sec);

        if(!other)
            continue;

        otherHeight = P_GetFixedp(other, DMU_FLOOR_HEIGHT);

        if(otherHeight > currentheight)
        {
            while(++i < lineCount)
            {
                check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
                other = P_GetNextSector(check, sec);

                if(other)
                {
                    anotherHeight = P_GetFixedp(other, DMU_FLOOR_HEIGHT);

                    if(anotherHeight < otherHeight &&
                       anotherHeight > currentheight)
                        otherHeight = anotherHeight;
                }
            }

            return otherHeight;
        }
    }

    return currentheight;
}

/**
 * Find lowest ceiling in the surrounding sector.
 */
fixed_t P_FindLowestCeilingSurrounding(sector_t *sec)
{
    int         i;
    int         lineCount;
    fixed_t     height = DDMAXINT;
    line_t     *check;
    sector_t   *other;

    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = P_GetNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(other, DMU_CEILING_HEIGHT) < height)
            height = P_GetFixedp(other, DMU_CEILING_HEIGHT);
    }
    return height;
}

/**
 * Find highest ceiling in the surrounding sectors.
 */
fixed_t P_FindHighestCeilingSurrounding(sector_t *sec)
{
    int         i;
    int         lineCount;
    fixed_t     height = 0;
    line_t     *check;
    sector_t   *other;

    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = P_GetNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(other, DMU_CEILING_HEIGHT) > height)
            height = P_GetFixedp(other, DMU_CEILING_HEIGHT);
    }
    return height;
}

/**
 * Find minimum light from an adjacent sector
 */
int P_FindMinSurroundingLight(sector_t *sec, int max)
{
    int         i;
    int         min;
    int         lineCount;
    line_t     *line;
    sector_t   *check;

    min = max;
    lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    for(i = 0; i < lineCount; ++i)
    {
        line = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        check = P_GetNextSector(line, sec);

        if(!check)
            continue;

        if(P_GetIntp(check, DMU_LIGHT_LEVEL) < min)
            min = P_GetIntp(check, DMU_LIGHT_LEVEL);
    }

    return min;
}
