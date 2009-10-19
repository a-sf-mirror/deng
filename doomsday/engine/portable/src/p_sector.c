/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * r_sector.c: World sectors.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

float Sector_LightLevel(sector_t* sec)
{
    assert(sec);

    if(mapFullBright)
        return 1.0f;

    return sec->lightLevel;
}

/**
 * @pre Sector bounds must be setup before this is called!
 */
void Sector_Bounds(sector_t* sec, float* min, float* max)
{
    assert(sec);

    if(min)
    {
        min[VX] = sec->bBox[BOXLEFT];
        min[VY] = sec->bBox[BOXBOTTOM];
    }
    if(max)
    {
        max[VX] = sec->bBox[BOXRIGHT];
        max[VY] = sec->bBox[BOXTOP];
    }
}

/**
 * @pre Lines in sector must be setup before this is called!
 */
void Sector_UpdateBounds(sector_t* sec)
{
    uint i;
    float* bbox;
    vertex_t* vtx;

    assert(sec);

    bbox = sec->bBox;

    if(!(sec->lineDefCount > 0))
    {
        memset(sec->bBox, 0, sizeof(sec->bBox));
        return;
    }

    bbox[BOXLEFT] = DDMAXFLOAT;
    bbox[BOXRIGHT] = DDMINFLOAT;
    bbox[BOXBOTTOM] = DDMAXFLOAT;
    bbox[BOXTOP] = DDMINFLOAT;

    for(i = 1; i < sec->lineDefCount; ++i)
    {
        linedef_t* li = sec->lineDefs[i];

        if(li->inFlags & LF_POLYOBJ)
            continue;

        vtx = li->L_v1;

        if(vtx->pos[VX] < bbox[BOXLEFT])
            bbox[BOXLEFT]   = vtx->pos[VX];
        if(vtx->pos[VX] > bbox[BOXRIGHT])
            bbox[BOXRIGHT]  = vtx->pos[VX];
        if(vtx->pos[VY] < bbox[BOXBOTTOM])
            bbox[BOXBOTTOM] = vtx->pos[VY];
        if(vtx->pos[VY] > bbox[BOXTOP])
            bbox[BOXTOP]    = vtx->pos[VY];
    }

    // This is very rough estimate of sector area.
    sec->approxArea = ((bbox[BOXRIGHT] - bbox[BOXLEFT]) / 128) *
        ((bbox[BOXTOP] - bbox[BOXBOTTOM]) / 128);
}

/**
 * Update the sector, property is selected by DMU_* name.
 */
boolean Sector_SetProperty(sector_t *sec, const setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_COLOR:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 1);
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 0);
        break;
    case DMU_LIGHT_LEVEL:
        DMU_SetValue(DMT_SECTOR_LIGHTLEVEL, &sec->lightLevel, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_SECTOR_VALIDCOUNT, &sec->validCount, args, 0);
        break;
    default:
        Con_Error("Sector_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a sector property, selected by DMU_* name.
 */
boolean Sector_GetProperty(const sector_t *sec, setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_LIGHT_LEVEL:
        DMU_GetValue(DMT_SECTOR_LIGHTLEVEL, &sec->lightLevel, args, 0);
        break;
    case DMU_COLOR:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 1);
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 0);
        break;
    case DMU_SOUND_ORIGIN:
    {
        const ddmobj_base_t* dmo = &sec->soundOrg;
        DMU_GetValue(DMT_SECTOR_SOUNDORG, &dmo, args, 0);
        break;
    }
    case DMU_LINEDEF_COUNT:
    {
        int val = (int) sec->lineDefCount;
        DMU_GetValue(DDVT_INT, &val, args, 0);
        break;
    }
    case DMU_MOBJS:
        DMU_GetValue(DMT_SECTOR_MOBJLIST, &sec->mobjList, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_SECTOR_VALIDCOUNT, &sec->validCount, args, 0);
        break;
    default:
        Con_Error("Sector_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
