/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * dmu_lib.c: Helper routines for accessing the DMU API
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "dmu_lib.h"
#include "gamemap.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

linedef_t* P_AllocDummyLine(void)
{
    xlinedef_t* extra = Z_Calloc(sizeof(xlinedef_t), PU_STATIC, 0);
    return DMU_AllocDummy(DMU_LINEDEF, extra);
}

void P_FreeDummyLine(linedef_t* line)
{
    Z_Free(DMU_DummyExtraData(line));
    DMU_FreeDummy(line);
}

/**
 * Copies all (changeable) properties from one line to another including the
 * extended properties.
 */
void P_CopyLine(linedef_t* dest, linedef_t* src)
{
    int i, sidx;
    sidedef_t* sidefrom, *sideto;
    xlinedef_t* xsrc = P_ToXLine(src);
    xlinedef_t* xdest = P_ToXLine(dest);

    if(src == dest)
        return; // no point copying self

    // Copy the built-in properties
    for(i = 0; i < 2; ++i) // For each side
    {
        sidx = (i==0? DMU_SIDEDEF0 : DMU_SIDEDEF1);

        sidefrom = DMU_GetPtrp(src, sidx);
        sideto = DMU_GetPtrp(dest, sidx);

        if(!sidefrom || !sideto)
            continue;

#if 0
        // P_Copyp is not implemented in Doomsday yet.
        P_Copyp(DMU_TOP_MATERIAL_OFFSET_XY, sidefrom, sideto);
        P_Copyp(DMU_TOP_MATERIAL, sidefrom, sideto);
        P_Copyp(DMU_TOP_COLOR, sidefrom, sideto);

        P_Copyp(DMU_MIDDLE_MATERIAL, sidefrom, sideto);
        P_Copyp(DMU_MIDDLE_COLOR, sidefrom, sideto);
        P_Copyp(DMU_MIDDLE_BLENDMODE, sidefrom, sideto);

        P_Copyp(DMU_BOTTOM_MATERIAL, sidefrom, sideto);
        P_Copyp(DMU_BOTTOM_COLOR, sidefrom, sideto);
#else
        {
        float temp[4];
        float itemp[2];

        DMU_SetPtrp(sideto, DMU_TOP_MATERIAL, DMU_GetPtrp(sidefrom, DMU_TOP_MATERIAL));
        DMU_GetFloatpv(sidefrom, DMU_TOP_MATERIAL_OFFSET_XY, itemp);
        DMU_SetFloatpv(sideto, DMU_TOP_MATERIAL_OFFSET_XY, itemp);
        DMU_GetFloatpv(sidefrom, DMU_TOP_COLOR, temp);
        DMU_SetFloatpv(sideto, DMU_TOP_COLOR, temp);

        DMU_SetPtrp(sideto, DMU_MIDDLE_MATERIAL, DMU_GetPtrp(sidefrom, DMU_MIDDLE_MATERIAL));
        DMU_GetFloatpv(sidefrom, DMU_MIDDLE_COLOR, temp);
        DMU_GetFloatpv(sidefrom, DMU_MIDDLE_MATERIAL_OFFSET_XY, itemp);
        DMU_SetFloatpv(sideto, DMU_MIDDLE_MATERIAL_OFFSET_XY, itemp);
        DMU_SetFloatpv(sideto, DMU_MIDDLE_COLOR, temp);
        DMU_SetIntp(sideto, DMU_MIDDLE_BLENDMODE, DMU_GetIntp(sidefrom, DMU_MIDDLE_BLENDMODE));

        DMU_SetPtrp(sideto, DMU_BOTTOM_MATERIAL, DMU_GetPtrp(sidefrom, DMU_BOTTOM_MATERIAL));
        DMU_GetFloatpv(sidefrom, DMU_BOTTOM_MATERIAL_OFFSET_XY, itemp);
        DMU_SetFloatpv(sideto, DMU_BOTTOM_MATERIAL_OFFSET_XY, itemp);
        DMU_GetFloatpv(sidefrom, DMU_BOTTOM_COLOR, temp);
        DMU_SetFloatpv(sideto, DMU_BOTTOM_COLOR, temp);
        }
#endif
    }

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    xdest->special = xsrc->special;
    if(xsrc->xg && xdest->xg)
        memcpy(xdest->xg, xsrc->xg, sizeof(*xdest->xg));
    else
        xdest->xg = NULL;
#else
    xdest->special = xsrc->special;
    xdest->arg1 = xsrc->arg1;
    xdest->arg2 = xsrc->arg2;
    xdest->arg3 = xsrc->arg3;
    xdest->arg4 = xsrc->arg4;
    xdest->arg5 = xsrc->arg5;
#endif
}

/**
 * Copies all (changeable) properties from one sector to another including
 * the extended properties.
 */
void P_CopySector(sector_t* dest, sector_t* src)
{
    xsector_t*          xsrc = P_ToXSector(src);
    xsector_t*          xdest = P_ToXSector(dest);

    if(src == dest)
        return; // no point copying self.

    // Copy the built-in properties.
#if 0
    // P_Copyp is not implemented in Doomsday yet.
    P_Copyp(DMU_LIGHT_LEVEL, src, dest);
    P_Copyp(DMU_COLOR, src, dest);

    P_Copyp(DMU_FLOOR_HEIGHT, src, dest);
    P_Copyp(DMU_FLOOR_MATERIAL, src, dest);
    P_Copyp(DMU_FLOOR_COLOR, src, dest);
    P_Copyp(DMU_FLOOR_MATERIAL_OFFSET_XY, src, dest);
    P_Copyp(DMU_FLOOR_SPEED, src, dest);
    P_Copyp(DMU_FLOOR_TARGET_HEIGHT, src, dest);

    P_Copyp(DMU_CEILING_HEIGHT, src, dest);
    P_Copyp(DMU_CEILING_MATERIAL, src, dest);
    P_Copyp(DMU_CEILING_COLOR, src, dest);
    P_Copyp(DMU_CEILING_MATERIAL_OFFSET_XY, src, dest);
    P_Copyp(DMU_CEILING_SPEED, src, dest);
    P_Copyp(DMU_CEILING_TARGET_HEIGHT, src, dest);
#else
    {
    float ftemp[4];

    DMU_SetFloatp(dest, DMU_LIGHT_LEVEL, DMU_GetFloatp(src, DMU_LIGHT_LEVEL));
    DMU_GetFloatpv(src, DMU_COLOR, ftemp);
    DMU_SetFloatpv(dest, DMU_COLOR, ftemp);

    DMU_SetFloatp(dest, DMU_FLOOR_HEIGHT, DMU_GetFloatp(src, DMU_FLOOR_HEIGHT));
    DMU_SetPtrp(dest, DMU_FLOOR_MATERIAL, DMU_GetPtrp(src, DMU_FLOOR_MATERIAL));
    DMU_GetFloatpv(src, DMU_FLOOR_COLOR, ftemp);
    DMU_SetFloatpv(dest, DMU_FLOOR_COLOR, ftemp);
    DMU_GetFloatpv(src, DMU_FLOOR_MATERIAL_OFFSET_XY, ftemp);
    DMU_SetFloatpv(dest, DMU_FLOOR_MATERIAL_OFFSET_XY, ftemp);
    DMU_SetIntp(dest, DMU_FLOOR_SPEED, DMU_GetIntp(src, DMU_FLOOR_SPEED));
    DMU_SetFloatp(dest, DMU_FLOOR_TARGET_HEIGHT, DMU_GetFloatp(src, DMU_FLOOR_TARGET_HEIGHT));

    DMU_SetFloatp(dest, DMU_CEILING_HEIGHT, DMU_GetFloatp(src, DMU_CEILING_HEIGHT));
    DMU_SetPtrp(dest, DMU_CEILING_MATERIAL, DMU_GetPtrp(src, DMU_CEILING_MATERIAL));
    DMU_GetFloatpv(src, DMU_CEILING_COLOR, ftemp);
    DMU_SetFloatpv(dest, DMU_CEILING_COLOR, ftemp);
    DMU_GetFloatpv(src, DMU_CEILING_MATERIAL_OFFSET_XY, ftemp);
    DMU_SetFloatpv(dest, DMU_CEILING_MATERIAL_OFFSET_XY, ftemp);
    DMU_SetIntp(dest, DMU_CEILING_SPEED, DMU_GetIntp(src, DMU_CEILING_SPEED));
    DMU_SetFloatp(dest, DMU_CEILING_TARGET_HEIGHT, DMU_GetFloatp(src, DMU_CEILING_TARGET_HEIGHT));
    }
#endif

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    xdest->special = xsrc->special;
    xdest->soundTraversed = xsrc->soundTraversed;
    xdest->soundTarget = xsrc->soundTarget;
#if __JHERETIC__
    xdest->seqType = xsrc->seqType;
#endif
    xdest->SP_floororigheight = xsrc->SP_floororigheight;
    xdest->SP_ceilorigheight = xsrc->SP_ceilorigheight;
    xdest->origLight = xsrc->origLight;
    memcpy(xdest->origRGB, xsrc->origRGB, sizeof(float) * 3);
    if(xsrc->xg && xdest->xg)
        memcpy(xdest->xg, xsrc->xg, sizeof(*xdest->xg));
    else
        xdest->xg = NULL;
#else
    xdest->special = xsrc->special;
    xdest->soundTraversed = xsrc->soundTraversed;
    xdest->soundTarget = xsrc->soundTarget;
    xdest->seqType = xsrc->seqType;
#endif
}

float P_SectorLight(sector_t* sector)
{
    return DMU_GetFloatp(sector, DMU_LIGHT_LEVEL);
}

void P_SectorSetLight(sector_t* sector, float level)
{
    DMU_SetFloatp(sector, DMU_LIGHT_LEVEL, level);
}

void P_SectorModifyLight(sector_t* sector, float value)
{
    float               level =
        MINMAX_OF(0.f, P_SectorLight(sector) + value, 1.f);

    P_SectorSetLight(sector, level);
}

void P_SectorModifyLightx(sector_t* sector, fixed_t value)
{
    DMU_SetFloatp(sector, DMU_LIGHT_LEVEL,
                P_SectorLight(sector) + FIX2FLT(value) / 255.0f);
}

void* P_SectorSoundOrigin(sector_t* sec)
{
    return DMU_GetPtrp(sec, DMU_SOUND_ORIGIN);
}

unsigned int DMU_ToIndex(const void* ptr)
{
    return P_ToIndex(ptr);
}

void* DMU_ToPtr(int type, uint index)
{
    return P_ToPtr(type, index);
}

int DMU_Callback(int type, uint index, int (*callback)(void* p, void* ctx),
                 void* context)
{
    return P_Callback(type, index, callback, context);
}

int DMU_Callbackp(int type, void* ptr, int (*callback)(void* p, void* ctx),
                  void* context)
{
    return P_Callbackp(type, ptr, callback, context);
}

int DMU_Iteratep(void* ptr, uint prop, int (*callback) (void* p, void* ctx),
                 void* context)
{
    return P_Iteratep(ptr, prop, callback, context);
}

void* DMU_AllocDummy(int type, void* extraData)
{
    return P_AllocDummy(type, extraData);
}

void DMU_FreeDummy(void* dummy)
{
    P_FreeDummy(dummy);
}

int DMU_DummyType(void* dummy)
{
    return P_DummyType(dummy);
}

boolean DMU_IsDummy(void* dummy)
{
    return P_IsDummy(dummy);
}

void* DMU_DummyExtraData(void* dummy)
{
    return P_DummyExtraData(dummy);
}

void DMU_SetBool(int type, uint index, uint prop, boolean param)
{
    P_SetBool(type, index, prop, param);
}

void DMU_SetByte(int type, uint index, uint prop, byte param)
{
    P_SetByte(type, index, prop, param);
}

void DMU_SetInt(int type, uint index, uint prop, int param)
{
    P_SetInt(type, index, prop, param);
}

void DMU_SetFixed(int type, uint index, uint prop, fixed_t param)
{
    P_SetFixed(type, index, prop, param);
}

void DMU_SetAngle(int type, uint index, uint prop, angle_t param)
{
    P_SetAngle(type, index, prop, param);
}

void DMU_SetFloat(int type, uint index, uint prop, float param)
{
    P_SetFloat(type, index, prop, param);
}

void DMU_SetPtr(int type, uint index, uint prop, void* param)
{
    P_SetPtr(type, index, prop, param);
}

void DMU_SetBoolv(int type, uint index, uint prop, boolean* params)
{
    P_SetBoolv(type, index, prop, params);
}

void DMU_SetBytev(int type, uint index, uint prop, byte* params)
{
    P_SetBytev(type, index, prop, params);
}

void DMU_SetIntv(int type, uint index, uint prop, int* params)
{
    P_SetIntv(type, index, prop, params);
}

void DMU_SetFixedv(int type, uint index, uint prop, fixed_t* params)
{
    P_SetFixedv(type, index, prop, params);
}

void DMU_SetAnglev(int type, uint index, uint prop, angle_t* params)
{
    P_SetAnglev(type, index, prop, params);
}

void DMU_SetFloatv(int type, uint index, uint prop, float* params)
{
    P_SetFloatv(type, index, prop, params);
}

void DMU_SetPtrv(int type, uint index, uint prop, void* params)
{
    P_SetPtrv(type, index, prop, params);
}

void DMU_SetBoolp(void* ptr, uint prop, boolean param)
{
    P_SetBoolp(ptr, prop, param);
}

void DMU_SetBytep(void* ptr, uint prop, byte param)
{
    P_SetBytep(ptr, prop, param);
}

void DMU_SetIntp(void* ptr, uint prop, int param)
{
    P_SetIntp(ptr, prop, param);
}

void DMU_SetFixedp(void* ptr, uint prop, fixed_t param)
{
    P_SetFixedp(ptr, prop, param);
}

void DMU_SetAnglep(void* ptr, uint prop, angle_t param)
{
    P_SetAnglep(ptr, prop, param);
}

void DMU_SetFloatp(void* ptr, uint prop, float param)
{
    P_SetFloatp(ptr, prop, param);
}

void DMU_SetPtrp(void* ptr, uint prop, void* param)
{
    P_SetPtrp(ptr, prop, param);
}

void DMU_SetBoolpv(void* ptr, uint prop, boolean* params)
{
    P_SetBoolpv(ptr, prop, params);
}

void DMU_SetBytepv(void* ptr, uint prop, byte* params)
{
    P_SetBytepv(ptr, prop, params);
}

void DMU_SetIntpv(void* ptr, uint prop, int* params)
{
    P_SetIntpv(ptr, prop, params);
}

void DMU_SetFixedpv(void* ptr, uint prop, fixed_t* params)
{
    P_SetFixedpv(ptr, prop, params);
}

void DMU_SetAnglepv(void* ptr, uint prop, angle_t* params)
{
    P_SetAnglepv(ptr, prop, params);
}

void DMU_SetFloatpv(void* ptr, uint prop, float* params)
{
    P_SetFloatpv(ptr, prop, params);
}

void DMU_SetPtrpv(void* ptr, uint prop, void* params)
{
    P_SetPtrpv(ptr, prop, params);
}

boolean DMU_GetBool(int type, uint index, uint prop)
{
    return P_GetBool(type, index, prop);
}

byte DMU_GetByte(int type, uint index, uint prop)
{
    return P_GetByte(type, index, prop);
}

int DMU_GetInt(int type, uint index, uint prop)
{
    return P_GetInt(type, index, prop);
}

fixed_t DMU_GetFixed(int type, uint index, uint prop)
{
    return P_GetFixed(type, index, prop);
}

angle_t DMU_GetAngle(int type, uint index, uint prop)
{
    return P_GetAngle(type, index, prop);
}

float DMU_GetFloat(int type, uint index, uint prop)
{
    return P_GetFloat(type, index, prop);
}

void* DMU_GetPtr(int type, uint index, uint prop)
{
    return P_GetPtr(type, index, prop);
}

void DMU_GetBoolv(int type, uint index, uint prop, boolean* params)
{
    P_GetBoolv(type, index, prop, params);
}

void DMU_GetBytev(int type, uint index, uint prop, byte* params)
{
    P_GetBytev(type, index, prop, params);
}

void DMU_GetIntv(int type, uint index, uint prop, int* params)
{
    P_GetIntv(type, index, prop, params);
}

void DMU_GetFixedv(int type, uint index, uint prop, fixed_t* params)
{
    P_GetFixedv(type, index, prop, params);
}

void DMU_GetAnglev(int type, uint index, uint prop, angle_t* params)
{
    P_GetAnglev(type, index, prop, params);
}

void DMU_GetFloatv(int type, uint index, uint prop, float* params)
{
    P_GetFloatv(type, index, prop, params);
}

void DMU_GetPtrv(int type, uint index, uint prop, void* params)
{
    P_GetPtrv(type, index, prop, params);
}

boolean DMU_GetBoolp(void* ptr, uint prop)
{
    return P_GetBoolp(ptr, prop);
}

byte DMU_GetBytep(void* ptr, uint prop)
{
    return P_GetBytep(ptr, prop);
}

int DMU_GetIntp(void* ptr, uint prop)
{
    return P_GetIntp(ptr, prop);
}

fixed_t DMU_GetFixedp(void* ptr, uint prop)
{
    return P_GetFixedp(ptr, prop);
}

angle_t DMU_GetAnglep(void* ptr, uint prop)
{
    return P_GetAnglep(ptr, prop);
}

float DMU_GetFloatp(void* ptr, uint prop)
{
    return P_GetFloatp(ptr, prop);
}

void* DMU_GetPtrp(void* ptr, uint prop)
{
    return P_GetPtrp(ptr, prop);
}

void DMU_GetBoolpv(void* ptr, uint prop, boolean* params)
{
    P_GetBoolpv(ptr, prop, params);
}

void DMU_GetBytepv(void* ptr, uint prop, byte* params)
{
    P_GetBytepv(ptr, prop, params);
}

void DMU_GetIntpv(void* ptr, uint prop, int* params)
{
    P_GetIntpv(ptr, prop, params);
}

void DMU_GetFixedpv(void* ptr, uint prop, fixed_t* params)
{
    P_GetFixedpv(ptr, prop, params);
}

void DMU_GetAnglepv(void* ptr, uint prop, angle_t* params)
{
    P_GetAnglepv(ptr, prop, params);
}

void DMU_GetFloatpv(void* ptr, uint prop, float* params)
{
    P_GetFloatpv(ptr, prop, params);
}

void DMU_GetPtrpv(void* ptr, uint prop, void* params)
{
    P_GetPtrpv(ptr, prop, params);
}
