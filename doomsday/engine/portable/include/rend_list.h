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
 * rend_list.h: Rendering Lists
 */

#ifndef __DOOMSDAY_REND_LIST_H__
#define __DOOMSDAY_REND_LIST_H__

#include "r_data.h"

// Multiplicative blending for dynamic lights?
#define IS_MUL              (dlBlend != 1 && !usingFog)

// Types of rendering primitives.
typedef enum primtype_e {
    PT_FIRST = 0,
    PT_TRIANGLE_STRIP = PT_FIRST, // Used for most stuff.
    PT_FAN,
    NUM_PRIM_TYPES
} primtype_t;

// Special rendpoly types.
typedef enum {
    RPT_NORMAL = 0,
    RPT_SKY_MASK, // A sky mask polygon.
    RPT_LIGHT, // A dynamic light.
    RPT_SHADOW, // An object shadow or fakeradio edge shadow.
    RPT_SHINY // A shiny polygon.
} rendpolytype_t;

extern int renderTextures;
extern int renderWireframe;
extern int useMultiTexLights;
extern int useMultiTexDetails;
extern float rendLightWallAngle;
extern float detailFactor, detailScale;

void            RL_Register(void);
void            RL_Init(void);
boolean         RL_IsMTexLights(void);
boolean         RL_IsMTexDetails(void);

void            RL_ClearLists(void);
void            RL_DeleteLists(void);

void RL_AddPoly(primtype_t type, rendpolytype_t polyType,
                uint numVertices, const rvertex_t* vertices,
                const rtexcoord_t* modCoords, const rtexcoord_t* coords,
                const rtexcoord_t* coords2,
                const rcolor_t* colors,
                uint numLights,
                DGLuint modTex, const float modColor[3],
                const rtexmapunit_t rTU[NUM_TEXMAP_UNITS]);
void            RL_RenderAllLists(void);

#endif
