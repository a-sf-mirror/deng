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
 * Handle jDoom specific map data properties.
 */

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "d_config.h"
#include "p_local.h"

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
 * @parm id: int, index of the current element being read.
 * @parm dtype: int, lump type class id this value is for.
 * @parm prop: int, propertyid of the game-specific variable (as declared via DED).
 * @parm type: int, data type id of the value pointed to by *data.
 * @parm *data: void ptr, to the data value (has already been expanded, size
 *              converted and endian converted where necessary).
 */
int P_HandleMapDataProperty(int id, int dtype, int prop, int type, void *data)
{
#ifdef TODO_MAP_UPDATE
    switch(type)
    {
        case VT_BOOL:
            boolean* d = data;
            break;

        case VT_BYTE:
            byte* d = data;
            break;

        case VT_INT:
            int* d = data;
            break;

        case VT_FIXED:
            fixed_t* d = data;
            break;

        case VT_FLOAT:
            float* d = data;
            break;

        default:
            Con_Error("P_HandleMapDataElement: Unknown value type id %i.\n",type);
    }
    return 1;
#endif
}

/*
 * Initializes various playsim related data
 */
void P_Init(void)
{
    P_InitSwitchList();
    P_InitPicAnims();

    // Maximum health and armor points.
    maxhealth = 100;
    healthlimit = 200;
    godmodehealth = 100;

    megaspherehealth = 200;

    soulspherehealth = 100;
    soulspherelimit = 200;

    armorpoints[0] = 100;
    armorpoints[1] = armorpoints[2] = armorpoints[3] = 200;
    armorclass[0] = 1;
    armorclass[1] = armorclass[2] = armorclass[3] = 2;

    GetDefInt("Player|Max Health", &maxhealth);
    GetDefInt("Player|Health Limit", &healthlimit);
    GetDefInt("Player|God Health", &godmodehealth);

    GetDefInt("Player|Green Armor", &armorpoints[0]);
    GetDefInt("Player|Blue Armor", &armorpoints[1]);
    GetDefInt("Player|IDFA Armor", &armorpoints[2]);
    GetDefInt("Player|IDKFA Armor", &armorpoints[3]);

    GetDefInt("Player|Green Armor Class", &armorclass[0]);
    GetDefInt("Player|Blue Armor Class", &armorclass[1]);
    GetDefInt("Player|IDFA Armor Class", &armorclass[2]);
    GetDefInt("Player|IDKFA Armor Class", &armorclass[3]);

    GetDefInt("MegaSphere|Give|Health", &megaspherehealth);

    GetDefInt("SoulSphere|Give|Health", &soulspherehealth);
    GetDefInt("SoulSphere|Give|Health Limit", &soulspherelimit);
}
