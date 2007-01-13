/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 * rend_dyn.h: Dynamic Lights
 */

#ifndef __DOOMSDAY_DYNLIGHTS_H__
#define __DOOMSDAY_DYNLIGHTS_H__

#include "p_object.h"
#include "rend_list.h"

#define DYN_ASPECT      1.1f       // 1.2f is just too round for Doom.

#define MAX_GLOWHEIGHT  1024.0f    // Absolute max glow height

// Lumobj Flags.
#define LUMF_USED       0x1
#define LUMF_RENDERED   0x2
#define LUMF_CLIPPED    0x4        // Hidden by world geometry.
#define LUMF_NOHALO     0x100
#define LUMF_DONTTURNHALO 0x200

typedef struct lumobj_s            // For dynamic lighting.
{
    struct lumobj_s *next;         // Next in the same DL block, or NULL.
    struct lumobj_s *ssNext;       // Next in the same subsector, or NULL.

    int             flags;
    mobj_t         *thing;
    float           center;        // Offset to center from mobj Z.
    int             radius, patch, distance;    // Radius: lights are spheres.
    int             flareSize;     // Radius for this light source.
    byte            rgb[3];        // The color.
    float           xOff;
    float           xyScale;       // 1.0 if there's no modeldef.
    DGLuint         tex;           // Lightmap texture.
    DGLuint         floorTex, ceilTex;  // Lightmaps for floor/ceil.
    DGLuint         decorMap;      // Decoration lightmap.
    DGLuint         flareTex;      // Flaremap if flareCustom ELSE (flaretexName id.
                                   // Zero = automatical)
    boolean         flareCustom;   // True id flareTex is a custom flare graphic
    float           flareMul;      // Flare brightness factor.
} lumobj_t;

/*
 * The data of a projected dynamic light is stored in this structure.
 * A list of these is associated with all the lit segs/planes in a frame.
 */
typedef struct dynlight_s {
    struct dynlight_s *next, *nextUsed;

    int             flags;
    float           s[2], t[2];
    byte            color[3];
    DGLuint         texture;
} dynlight_t;

// Flags for projected dynamic lights.
#define DYNF_PREGEN_DECOR   0x1    // Pregen RGB lightmap for a light decoration.

extern boolean  dlInited;
extern int      useDynLights;
extern int      dlBlend, dlMaxRad;
extern float    dlRadFactor, dlFactor;
extern int      useWallGlow, glowHeightMax;
extern float    glowHeightFactor;
extern float    glowFogBright;
extern byte     rendInfoLums;
extern int      dlMinRadForBias;

// Initialization
void            DL_Register(void);

// Setup.
void            DL_InitForMap(void);
void            DL_Clear(void);        // 'Physically' destroy the tables.

// Action.
void            DL_ClearForFrame(void);
void            DL_InitForNewFrame(void);
unsigned int    DL_NewLuminous(void);
lumobj_t*       DL_GetLuminous(unsigned int index);
unsigned int    DL_GetNumLuminous(void);
void            DL_PlanesChanged(sector_t *sect);
void            DL_ProcessSubsector(subsector_t *ssec);
dynlight_t*     DL_GetSegSectionLightLinks(uint segidx, segsection_t section);
dynlight_t*     DL_GetSubSecPlaneLightLinks(uint ssecidx, uint plane);

// Helpers.
boolean         DL_RadiusIterator(subsector_t *subsector, fixed_t x, fixed_t y,
                                  fixed_t radius, void *data,
                                  boolean (*func) (lumobj_t *, fixed_t, void *data));

void            DL_ClipInSubsector(uint ssecidx);
void            DL_ClipBySight(uint ssecidx);

#endif
