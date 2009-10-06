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
 * rend_bias.h: Light/Shadow Bias
 */

#ifndef __DOOMSDAY_RENDER_SHADOW_BIAS_H__
#define __DOOMSDAY_RENDER_SHADOW_BIAS_H__

#include "m_vector.h"

#define MAX_BIAS_LIGHTS   (8 * 32) // Hard limit due to change tracking.
#define MAX_BIAS_AFFECTED 6

typedef struct vilight_s {
    short           source;
    float           color[3];     // Light from an affecting source.
} vilight_t;

// Vertex illumination flags.
#define VIF_LERP         0x1      // Interpolation is in progress.
#define VIF_STILL_UNSEEN 0x2      // The color of the vertex is still unknown.

typedef struct vertexillum_s {
    float           color[3];     // Current color of the vertex.
    float           dest[3];      // Destination color of the vertex.
    unsigned int    updatetime;   // When the value was calculated.
    short           flags;
    vilight_t       casted[MAX_BIAS_AFFECTED];
} vertexillum_t;

typedef struct biasaffection_s {
    short           source;       // Index of light source.
} biasaffection_t;

// Bias light source macros.
#define BLF_COLOR_OVERRIDE   0x00000001
#define BLF_LOCKED           0x40000000
#define BLF_CHANGED          0x80000000

typedef struct source_s {
    int             flags;
    float           pos[3];
    float           color[3];
    float           intensity;
    float           primaryIntensity;
    float           sectorLevel[2];
    unsigned int    lastUpdateTime;
} source_t;

typedef struct biastracker_s {
    unsigned int changes[MAX_BIAS_LIGHTS / 32];
} biastracker_t;

struct rendpoly_s;
struct rvertex_s;
struct rcolor_s;

extern int      useBias; // Bias lighting enabled.
extern unsigned int currentTimeSB;

void            SB_Register(void);

void            SB_InitForMap(struct gamemap_s* map);
void            SB_BeginFrame(struct gamemap_s* map);
void            SB_EndFrame(struct gamemap_s* map);
void            SB_DestroySurfaces(struct gamemap_s* map);

struct biassurface_s* SB_CreateSurface(struct gamemap_s* map);
void            SB_DestroySurface(struct gamemap_s* map, struct biassurface_s* bsuf);
void            SB_SurfaceMoved(struct gamemap_s* map, struct biassurface_s* bsuf);

int             SB_NewSourceAt(struct gamemap_s* map, float x, float y,
                               float z, float size, float minLight,
                               float maxLight, float* rgb);
void            SB_DeleteSource(struct gamemap_s* map, int which);
void            SB_ClearSources(struct gamemap_s* map);

source_t*       SB_GetSource(struct gamemap_s* map, int which);
int             SB_ToIndex(struct gamemap_s* map, source_t* source);

void            SB_SourceUpdate(source_t* source, float x, float y, float z,
                                float size, float minLight, float maxLight,
                                float* rgb);
void            SB_SourceSetColor(source_t* src, float* rgb);

void SB_RendSeg(struct gamemap_s* map, struct rcolor_s* rcolors, struct biassurface_s* bsuf,
                const struct rvertex_s* rvertices, size_t numVertices,
                const vectorcomp_t* normal, float sectorLightLevel,
                const struct fvertex_s* from, const struct fvertex_s* to);

void SB_RendPlane(struct gamemap_s* map, struct rcolor_s* rcolors, struct biassurface_s* bsuf,
                  const struct rvertex_s* rvertices, size_t numVertices,
                  const vectorcomp_t* normal, float sectorLightLevel,
                  struct face_s* face, uint plane);

void            SB_InitVertexIllum(vertexillum_t* villum);
#endif
