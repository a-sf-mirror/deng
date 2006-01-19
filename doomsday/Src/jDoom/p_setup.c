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

// Game specific map format properties for ALL games.
// TODO: we don't need  to know about all of them once they
// are registered via DED.
// (notice jHeretic/jHexen properties are here too temporarily).
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
    DAM_THING_ARG5
};

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
 * Converts a sector to an xsector.
 */
xsector_t* P_XSector(sector_t* sector)
{
    return &xsectors[P_ToIndex(DMU_SECTOR, sector)];
}

/*
 * Given a subsector - find its parent xsector.
 */
xsector_t* P_XSectorOfSubsector(subsector_t* sub)
{
    return &xsectors[P_ToIndex(DMU_SECTOR_OF_SUBSECTOR, sub)];
}

/*
 * Create and initialize our private thing data array
 */
void P_SetupForThings(int num)
{
    int oldNum = numthings;

    numthings += num;

    if(oldNum > 0)
        things = Z_Realloc(things, numthings * sizeof(thing_t), PU_LEVEL);
    else
        things = Z_Malloc(numthings * sizeof(thing_t), PU_LEVEL, 0);

    memset(things + oldNum, 0, num * sizeof(thing_t));
}

/*
 * Create and initialize our private line data array
 */
void P_SetupForLines(int num)
{
    int oldNum = numlines;

    numlines += num;

    if(oldNum > 0)
        xlines = Z_Realloc(xlines, numlines * sizeof(xline_t), PU_LEVEL);
    else
        xlines = Z_Malloc(numlines * sizeof(xline_t), PU_LEVEL, 0);

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
    int oldNum = numsectors;

    numsectors += num;

    if(oldNum > 0)
        xsectors = Z_Realloc(xsectors, numsectors * sizeof(xsector_t), PU_LEVEL);
    else
        xsectors = Z_Malloc(numsectors * sizeof(xsector_t), PU_LEVEL, 0);

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
    switch(prop)
    {
    case DAM_SECTOR_SPECIAL:
        xsectors[id].special = *(short *)data;
        break;
    case DAM_SECTOR_TAG:
        xsectors[id].tag = *(short *)data;
        break;
    case DAM_LINE_SPECIAL:
        xlines[id].special = *(short *)data;
        break;
    case DAM_LINE_TAG:
        xlines[id].tag = *(short *)data;
        break;
    case DAM_THING_X:
        things[id].x = *(short *)data;
        break;
    case DAM_THING_Y:
        things[id].y = *(short *)data;
        break;
    case DAM_THING_HEIGHT:
        things[id].height = *(short *)data;
        break;
    case DAM_THING_ANGLE:
        things[id].angle = *(short *)data;
        break;
    case DAM_THING_TYPE:
        things[id].type = *(short *)data;
        break;
    case DAM_THING_OPTIONS:
        things[id].options = *(short *)data;
        break;
    default:
        Con_Error("P_HandleMapDataProperty: Unknown property id %i.\n",prop);
    }

    return 1;
}

/*
 * Doomsday will call this when loading the map data if it encounters a
 * value that it doesn't understand for a property IT handles.
 *
 * Doomsday thinks we might know what to do with it...
 * If we don't know what to do we'll return -1.
 *
 * @parm id: int, index of the current element being read.
 * @parm dtype: int, lump type class id this value is for.
 * @parm prop: int, propertyid of the map structure.
 * @parm type: int, data type id of the value pointed to by *data.
 * @parm *data: void ptr, to the data value (has already been expanded, size
 *              converted and endian converted where necessary).
 */
int P_HandleMapDataPropertyValue(int id, int dtype, int prop,
                                 int type, void *data)
{
    switch(dtype)
    {
    case DAM_SIDE:
        switch(prop)
        {
        case DAM_TOP_TEXTURE:
        case DAM_MIDDLE_TEXTURE:
        case DAM_BOTTOM_TEXTURE:
            // It could be a BOOM overloaded texture name?
            // In this context Doomsday expects either -1 (a bad texture name)
            // Or the id of a wall texture to set it should set to this section.

            // TODO: Add code to determine what to do.
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }

    return -1;
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
