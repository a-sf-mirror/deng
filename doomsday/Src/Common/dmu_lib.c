/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * dmu_lib.c: Helper routines for accessing the DMU API
 */

// HEADER FILES ------------------------------------------------------------

#ifdef __JDOOM__
# include "jDoom/r_defs.h"
#elif __JHERETIC__
# include "jHeretic/R_local.h"
#elif __JHEXEN__
# include "jHexen/r_local.h"
#endif

#include "dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

line_t *P_AllocDummyLine(void)
{
    xline_t* extra = Z_Calloc(sizeof(xline_t), PU_STATIC, 0);
    return P_AllocDummy(DMU_LINE, extra);
}

void P_FreeDummyLine(line_t* line)
{
    Z_Free(P_DummyExtraData(line));
    P_FreeDummy(line);
}

/*
 * Copies all (changeable) properties from one line
 * to another including the extended properties.
 */
void P_CopyLine(line_t* from, line_t* to)
{
    xline_t* xfrom = P_XLine(from);
    xline_t* xto = P_XLine(to);

    // Copy the built-in properties
    P_Copyp(DMU_FLAGS, from, to);
    P_Copyp(DMU_TEXTURE_OFFSET_XY, from, to);

    P_Copyp(DMU_TOP_TEXTURE, from, to);
    P_Copyp(DMU_TOP_COLOR, from, to);

    P_Copyp(DMU_MIDDLE_TEXTURE, from, to);
    P_Copyp(DMU_MIDDLE_COLOR, from, to);
    P_Copyp(DMU_MIDDLE_BLENDMODE, from, to);

    P_Copyp(DMU_BOTTOM_TEXTURE, from, to);
    P_Copyp(DMU_BOTTOM_COLOR, from, to);

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC
    xfrom->special = xto->special;
    xfrom->tag = xto->tag;
    xfrom->specialdata = xto->specialdata;
    xfrom->xg = xto->xg;
#else
    xfrom->special = to->special;
    xfrom->arg1 = xto->arg1;
    xfrom->arg2 = xto->arg2;
    xfrom->arg3 = xto->arg3;
    xfrom->arg4 = xto->arg4;
    xfrom->arg5 = xto->arg5;
    xfrom->specialdata = xto->specialdata;
#endif
}

/*
 * Copies all (changeable) properties from one sector
 * to another including the extended properties.
 */
void P_CopySector(sector_t* from, sector_t* to)
{
    xsector_t* xfrom = P_XSector(from);
    xsector_t* xto = P_XSector(to);

    // Copy the built-in properties
    P_Copyp(DMU_LIGHT_LEVEL, from, to);
    P_Copyp(DMU_COLOR, from, to);

    P_Copyp(DMU_FLOOR_HEIGHT, from, to);
    P_Copyp(DMU_FLOOR_TEXTURE, from, to);
    P_Copyp(DMU_FLOOR_COLOR, from, to);
    P_Copyp(DMU_FLOOR_OFFSET_XY, from, to);
    P_Copyp(DMU_FLOOR_TEXTURE_MOVE_XY, from, to);
    P_Copyp(DMU_FLOOR_SPEED, from, to);
    P_Copyp(DMU_FLOOR_TARGET, from, to);

    P_Copyp(DMU_CEILING_HEIGHT, from, to);
    P_Copyp(DMU_CEILING_TEXTURE, from, to);
    P_Copyp(DMU_CEILING_COLOR, from, to);
    P_Copyp(DMU_CEILING_OFFSET_XY, from, to);
    P_Copyp(DMU_CEILING_TEXTURE_MOVE_XY, from, to);
    P_Copyp(DMU_CEILING_SPEED, from, to);
    P_Copyp(DMU_CEILING_TARGET, from, to);

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC
    xfrom->special = xto->special;
    xfrom->tag = xto->tag;
    xfrom->soundtraversed = xto->soundtraversed;
    xfrom->soundtarget = xto->soundtarget;
    xfrom->specialdata = xto->specialdata;
#if __JHERETIC__
    xfrom->seqtype = xto->seqType;
#endif
    xfrom->origfloor = xto->origfloor;
    xfrom->origceiling = xto->origceiling;
    xfrom->origlight = xto->origlight;
    memcpy(xfrom->origrgb, xto->origrgb, 3);
    xfrom->xg = xto->xg;
#else
    xfrom->special = xto->special;
    xfrom->tag = xto->tag;
    xfrom->soundtraversed = xto->soundtraversed;
    xfrom->soundtarget = xto->soundtarget;
    xfrom->seqtype = xto->seqType;
    xfrom->specialdata = xto->specialdata;
#endif
}

int P_SectorLight(sector_t* sector)
{
    return P_GetIntp(sector, DMU_LIGHT_LEVEL);
}

void P_SectorSetLight(sector_t* sector, int level)
{
    P_SetIntp(sector, DMU_LIGHT_LEVEL, level);
}

void P_SectorModifyLight(sector_t* sector, int value)
{
    int level = P_SectorLight(sector);

    level += value;

    if(level < 0) level = 0;
    if(level > 255) level = 255;

    P_SectorSetLight(sector, level);
}

fixed_t P_SectorLightx(sector_t* sector)
{
    return P_GetFixedp(sector, DMU_LIGHT_LEVEL);
}

void P_SectorModifyLightx(sector_t* sector, fixed_t value)
{
    P_SetFixedp(sector, DMU_LIGHT_LEVEL,
                P_SectorLightx(sector) + value);
}

void *P_SectorSoundOrigin(sector_t *sec)
{
    return P_GetPtrp(sec, DMU_SOUND_ORIGIN);
}
