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
 * r_sky.h: Sky Management
 */

#ifndef DOOMSDAY_REFRESH_SKY_H
#define DOOMSDAY_REFRESH_SKY_H

#include "r_model.h"

extern sky_t* theSky;

extern int skyHemispheres;

void            R_SkyPrecache(void);
void            R_SkyTicker(void);

void            R_SetupSky(sky_t* sky, const ded_sky_t* skyDef);

#include "r_data.h"
#include "p_dmu.h"

void            Sky_InitDefault(sky_t* sky);
void            Sky_InitFromDefinition(sky_t* sky, const ded_sky_t* def);
void            Sky_Precache(sky_t* sky);
void            Sky_Ticker(sky_t* sky);

boolean         Sky_ColorGiven(sky_t* sky);

void            Sky_ActivateLayer(sky_t* sky, int layer, boolean active);

void            Sky_SetHeight(sky_t* sky, float height);
void            Sky_SetHorizonOffset(sky_t* sky, float offset);

void            Sky_SetLayerMask(sky_t* sky, int layer, boolean enable);
void            Sky_SetLayerMaterial(sky_t* sky, int layer, material_t* mat);
void            Sky_SetLayerMaterialOffsetX(sky_t* sky, int layer, float offset);
void            Sky_SetLayerColorFadeLimit(sky_t* sky, int layer, float limit);

boolean         Sky_IsLayerActive(const sky_t* sky, int layer);

int             Sky_GetFirstLayer(const sky_t* sky);
float           Sky_GetHorizonOffset(sky_t* sky);
float           Sky_GetMaxSideAngle(sky_t* sky);
const float*    Sky_GetColor(sky_t* sky);

boolean         Sky_GetLayerMask(const sky_t* sky, int layer);
material_t*     Sky_GetLayerMaterial(const sky_t* sky, int layer);
float           Sky_GetLayerMaterialOffsetX(const sky_t* sky, int layer);
const fadeout_t* Sky_GetLayerFadeout(const sky_t* sky, int layer);

// DMU interface:
boolean         Sky_SetProperty(sky_t* sky, const setargs_t* args);
boolean         Sky_GetProperty(const sky_t* sec, setargs_t* args);

#endif /* DOOMSDAY_REFRESH_SKY_H */
