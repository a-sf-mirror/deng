/* $Id$
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
 * Handle jHexen specific map data properties.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "h2def.h"
#include "jHexen/p_local.h"
#include "r_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void InitMapInfo(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Our private map data structures
xsector_t *xsectors;
int        numxsectors;
xline_t   *xlines;
int        numxlines;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Converts a line to an xline.
 */
xline_t* P_XLine(line_t* line)
{
    return &xlines[P_ToIndex(DMU_LINE, line)];
}

/*
 * Converts a line to an xline.
 */
xsector_t* P_XSector(sector_t* sector)
{
    return &xsectors[P_ToIndex(DMU_SECTOR, sector)];
}

xsector_t* P_XSectorOfSubsector(subsector_t* sub)
{
    return &xsectors[P_ToIndex(DMU_SECTOR_OF_SUBSECTOR, sub)];
}

void P_SetupForThings(int num)
{
    // Nothing to do
}

/*
 * Create and initialize our private line data array
 */
void P_SetupForLines(int num)
{
    int oldNum = numxlines;

    numxlines += num;

    if(oldNum > 0)
        xlines = Z_Realloc(xlines, numxlines * sizeof(xline_t), PU_LEVEL);
    else
        xlines = Z_Malloc(numxlines * sizeof(xline_t), PU_LEVEL, 0);

    memset(xlines + oldNum, 0, num * sizeof(xline_t));
}

void P_SetupForSides(int num)
{
    // Nothing to do
}

/*
 * Create and initialize our private sector data array
 */
void P_SetupForSectors(int num)
{
    int oldNum = numxsectors;

    numxsectors += num;

    if(oldNum > 0)
        xsectors = Z_Realloc(xsectors, numxsectors * sizeof(xsector_t), PU_LEVEL);
    else
        xsectors = Z_Malloc(numxsectors * sizeof(xsector_t), PU_LEVEL, 0);

    memset(xsectors + oldNum, 0, num * sizeof(xsector_t));
}

/*
 * Doomsday will call this while loading in map data when a value is read
 * that is not part of the internal data structure for the particular element.
 * This is where game specific data is added to game-side map data structures
 * (eg sector->tag, line->args etc).
 *
 * Returns true unless there is a critical problem with the data supplied.
 *
 * @param id: int, index of the current element being read.
 * @param dtype: int, lump type class id this value is for.
 * @param prop: int, propertyid of the game-specific variable (as declared via DED).
 * @param type: int, data type id of the value pointed to by *data.
 * @param *data: void ptr, to the data value (has already been expanded, size
 *              converted and endian converted where necessary).
 */
int P_HandleMapDataElement(int id, int dtype, int prop, int type, void *data)
{
#ifdef TODO_MAP_UPDATE
    switch(type)
    {
        default:
            Con_Error("P_HandleMapDataElement: Unknown value type id %i.\n",type);
    }
#endif
    return 1;
}

/*
 * Initializes various playsim related data
 */
void P_Init(void)
{
    InitMapInfo();

    P_InitSwitchList();
    P_InitPicAnims();

    P_InitTerrainTypes();
    P_InitLava();
}
