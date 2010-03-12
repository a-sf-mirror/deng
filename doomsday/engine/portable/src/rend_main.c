/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * rend_main.c: Rendering Subsystem
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_edit.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_graphics.h"

#include "de_misc.h"
#include "de_ui.h"
#include "de_system.h"
#include "net_main.h"

#include "rendseg.h"
#include "rendplane.h"
#include "blockmapvisual.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

enum {
    COORD_PRIMARY,
    COORD_MODULATE,
    COORD_SHINY,
    NUM_COORD_INDICES
};

enum {
    COLOR_PRIMARY,
    COLOR_SHINY,
    COLOR_RADIO_TOP,
    COLOR_RADIO_BOTTOM,
    COLOR_RADIO_LEFT,
    COLOR_RADIO_RIGHT,
    NUM_COLOR_INDICES
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Rend_DrawBBox(const float pos3f[3], float w, float l, float h,
                   const float color3f[3], float alpha, float br,
                   boolean alignToBase);
void Rend_DrawArrow(const float pos3f[3], angle_t a, float s,
                    const float color3f[3], float alpha);

void Rend_RenderNormals(map_t* map);
void Rend_Vertexes(map_t* map);
void Rend_RenderBoundingBoxes(map_t* map);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static DGLuint constructBBox(DGLuint name, float br);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int useDynlights, translucentIceCorpse;
extern int loMaxRadius;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean usingFog = false; // Is the fog in use?
float fogColor[4];
byte smoothTexAnim = true;
int useShinySurfaces = true;

byte freezeRLs = false;
int devRendSkyMode = false;
byte devRendSkyAlways = false;

int missileBlend = 1;
int gameDrawHUD = 1; // Set to zero when we advise that the HUD should not be drawn

int devMobjBBox = 0; // 1 = Draw mobj bounding boxes (for debug)
DGLuint dlBBox = 0; // Display list: active-textured bbox model.

byte devBlockmap = 0; // 1 = mobjs, 2 = linedefs, 3 = subSectors.
float devBlockmapSize = 1.5f;

byte devVertexIndices = 0; // @c 1= Draw world vertex indices (for debug).
byte devVertexBars = 0; // @c 1= Draw world vertex position bars.
byte devSurfaceNormals = 0; // @c 1= Draw world surface normal tails.
byte devNoTexFix = 0;
byte devLightModRange = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstsubsector; // No range checking for the first one.

// CODE --------------------------------------------------------------------

void Rend_Register(void)
{
    C_VAR_FLOAT("rend-camera-fov", &fieldOfView, 0, 1, 179);
    C_VAR_BYTE("rend-tex-anim-smooth", &smoothTexAnim, 0, 0, 1);
    C_VAR_INT("rend-tex-shiny", &useShinySurfaces, 0, 0, 1);
    C_VAR_FLOAT2("rend-light-compression", &lightRangeCompression, 0, -1, 1, R_CalcLightModRange);
    C_VAR_INT2("rend-light-ambient", &ambientLight, 0, 0, 255, R_CalcLightModRange);
    C_VAR_INT2("rend-light-sky", &rendSkyLight, 0, 0, 1, R_MarkAllSectorsForLightGridUpdate);
    C_VAR_FLOAT("rend-light-wall-angle", &rendLightWallAngle, CVF_NO_MAX, 0, 0);
    C_VAR_BYTE("rend-light-wall-angle-smooth", &rendLightWallAngleSmooth, 0, 0, 1);
    C_VAR_FLOAT("rend-light-attenuation", &rendLightDistanceAttentuation, CVF_NO_MAX, 0, 0);

    C_VAR_INT("rend-dev-sky", &devRendSkyMode, CVF_NO_ARCHIVE, 0, 2);
    C_VAR_BYTE("rend-dev-sky-always", &devRendSkyAlways, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-freeze", &freezeRLs, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-cull-subsectors", &devNoCulling,CVF_NO_ARCHIVE,0,1);
    C_VAR_INT("rend-dev-mobj-bbox", &devMobjBBox, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-light-mod", &devLightModRange, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-tex-showfix", &devNoTexFix, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-blockmap-debug", &devBlockmap, CVF_NO_ARCHIVE, 0, 3);
    C_VAR_FLOAT("rend-dev-blockmap-debug-size", &devBlockmapSize, CVF_NO_ARCHIVE, .1f, 100);
    C_VAR_BYTE("rend-dev-vertex-show-indices", &devVertexIndices, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-vertex-show-bars", &devVertexBars, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-surface-show-normals", &devSurfaceNormals, CVF_NO_ARCHIVE, 0, 1);

    RL_Register();
    DL_Register();
    SB_Register();
    R_RegisterLightGrid();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Rend_RadioRegister();
    Rend_ShadowRegister();
    Rend_SkyRegister();
    Rend_SpriteRegister();
    Rend_ConsoleRegister();
}

void Rend_Init(void)
{
    C_Init(); // Clipper.
    RL_Init(); // Rendering lists.
}

void Rend_Reset(void)
{
    GL_ClearRuntimeTextures();
    LO_Clear(); // Lumobj stuff.

    if(dlBBox)
        GL_DeleteLists(dlBBox, 1);
    dlBBox = 0;
}

void Rend_ModelViewMatrix(boolean useAngles)
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

    vx = viewData->current.pos[VX];
    vy = viewData->current.pos[VZ];
    vz = viewData->current.pos[VY];
    vang = viewData->current.angle / (float) ANGLE_MAX *360 - 90;
    vpitch = viewData->current.pitch * 85.0 / 110.0;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if(useAngles)
    {
        glRotatef(vpitch, 1, 0, 0);
        glRotatef(vang, 0, 1, 0);
    }
    glScalef(1, 1.2f, 1); // This is the aspect correction.
    glTranslatef(-vx, -vy, -vz);
}

static void markSegSectionsPVisible(hedge_t* hEdge)
{
    sidedef_t*          sideDef;

    if(!hEdge)
        return;

    sideDef = HE_FRONTSIDEDEF(hEdge);
    if(sideDef)
    {
        if(R_ConsiderOneSided(hEdge))
        {
            sideDef->SW_middleinflags |= SUIF_PVIS;
            sideDef->SW_topinflags &= ~SUIF_PVIS;
            sideDef->SW_bottominflags &= ~SUIF_PVIS;
        }
        else
        {
            sector_t*           frontSec = HE_FRONTSECTOR(hEdge);
            sector_t*           backSec = HE_BACKSECTOR(hEdge);

            sideDef->SW_middleinflags |= SUIF_PVIS;
            sideDef->SW_topinflags |= SUIF_PVIS;
            sideDef->SW_bottominflags |= SUIF_PVIS;

            // Check middle material.
            if((!sideDef->SW_middlematerial ||
                (sideDef->SW_middlematerial->flags & MATF_NO_DRAW)) ||
                sideDef->SW_middlergba[3] <= 0) // Check alpha
                sideDef->SW_middleinflags &= ~SUIF_PVIS;

            if(IS_SKYSURFACE(&backSec->SP_ceilsurface) &&
               IS_SKYSURFACE(&frontSec->SP_ceilsurface))
               sideDef->SW_topinflags &= ~SUIF_PVIS;
            else
            {
                if(frontSec->SP_ceilvisheight <=
                   backSec->SP_ceilvisheight)
                    sideDef->SW_topinflags &= ~SUIF_PVIS;
            }

            // Bottom
            if(IS_SKYSURFACE(&backSec->SP_floorsurface) &&
               IS_SKYSURFACE(&frontSec->SP_floorsurface))
               sideDef->SW_bottominflags &= ~SUIF_PVIS;
            else
            {
                if(frontSec->SP_floorvisheight >= backSec->SP_floorvisheight)
                    sideDef->SW_bottominflags &= ~SUIF_PVIS;
            }
        }
    }
    else
    {
        sideDef->SW_middleinflags &= ~SUIF_PVIS;
        sideDef->SW_topinflags &= ~SUIF_PVIS;
        sideDef->SW_bottominflags &= ~SUIF_PVIS;
    }
}

static void quadTexCoords(rtexcoord_t* tc, const rvertex_t* rverts,
                          float wallLength, float texWidth, float texHeight,
                          const float texOrigin[3], const float texOffset[2])
{
    tc[0].st[0] = tc[1].st[0] =
        rverts[0].pos[VX] - texOrigin[VX] + texOffset[VX] / texWidth;
    tc[3].st[1] = tc[1].st[1] =
        rverts[0].pos[VY] - texOrigin[VY] + texOffset[VY] / texHeight;
    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength / texWidth;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]) / texHeight;
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]) / texHeight;
}

static void quadLightCoords(rtexcoord_t* tc, const float s[2],
                            const float t[2])
{
    tc[1].st[0] = tc[0].st[0] = s[0];
    tc[1].st[1] = tc[3].st[1] = t[0];
    tc[3].st[0] = tc[2].st[0] = s[1];
    tc[2].st[1] = tc[0].st[1] = t[1];
}

#if 0
static void quadShinyMaskTexCoords(rtexcoord_t* tc, const rvertex_t* rverts,
                                   float wallLength, float texWidth,
                                   float texHeight, const pvec2_t texOrigin[2],
                                   const pvec2_t texOffset)
{
    tc[0].st[0] = tc[1].st[0] =
        rverts[0].pos[VX] - texOrigin[0][VX] + texOffset[VX] / texWidth;
    tc[3].st[1] = tc[1].st[1] =
        rverts[0].pos[VY] - texOrigin[0][VY] + texOffset[VY] / texHeight;
    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength / texWidth;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]) / texHeight;
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]) / texHeight;
}
#endif

static float shinyVertical(float dy, float dx)
{
    return ( (atan(dy/dx) / (PI/2)) + 1 ) / 2;
}

static void quadShinyTexCoords(rtexcoord_t* tc, const rvertex_t* topLeft,
                               const rvertex_t* bottomRight, float wallLength)
{
    uint                i;
    vec2_t              surface, normal, projected, s, reflected, view;
    float               distance, angle, prevAngle = 0;

    // Quad surface vector.
    V2_Set(surface,
           (bottomRight->pos[VX] - topLeft->pos[VX]) / wallLength,
           (bottomRight->pos[VY] - topLeft->pos[VY]) / wallLength);

    V2_Set(normal, surface[VY], -surface[VX]);

    // Calculate coordinates based on viewpoint and surface normal.
    for(i = 0; i < 2; ++i)
    {
        // View vector.
        V2_Set(view, vx - (i == 0? topLeft->pos[VX] : bottomRight->pos[VX]),
                vz - (i == 0? topLeft->pos[VY] : bottomRight->pos[VY]));

        distance = V2_Normalize(view);

        V2_Project(projected, view, normal);
        V2_Subtract(s, projected, view);
        V2_Scale(s, 2);
        V2_Sum(reflected, view, s);

        angle = acos(reflected[VY]) / PI;
        if(reflected[VX] < 0)
        {
            angle = 1 - angle;
        }

        if(i == 0)
        {
            prevAngle = angle;
        }
        else
        {
            if(angle > prevAngle)
                angle -= 1;
        }

        // Horizontal coordinates.
        tc[ (i == 0 ? 1 : 2) ].st[0] = tc[ (i == 0 ? 0 : 3) ].st[0] =
            angle + .3f;     /*acos(-dot)/PI*/

        tc[ (i == 0 ? 0 : 2) ].st[1] =
            shinyVertical(vy - bottomRight->pos[VZ], distance);

        // Vertical coordinates.
        tc[ (i == 0 ? 1 : 3) ].st[1] =
            shinyVertical(vy - topLeft->pos[VZ], distance);
    }
}

static void flatShinyTexCoords(rtexcoord_t* tc, const float xyz[3])
{
    vec2_t              view, start;
    float               distance;
    float               offset;

    // View vector.
    V2_Set(view, vx - xyz[VX], vz - xyz[VY]);

    distance = V2_Normalize(view);
    if(distance < 10)
    {
        // Too small distances cause an ugly 'crunch' below and above
        // the viewpoint.
        distance = 10;
    }

    // Offset from the normal view plane.
    V2_Set(start, vx, vz);

    offset = ((start[VY] - xyz[VY]) * sin(.4f)/*viewFrontVec[VX]*/ -
              (start[VX] - xyz[VX]) * cos(.4f)/*viewFrontVec[VZ]*/);

    tc->st[0] = ((shinyVertical(offset, distance) - .5f) * 2) + .5f;
    tc->st[1] = shinyVertical(vy - xyz[VZ], distance);
}

void Rend_SetupRTU(rtexmapunit_t rTU[NUM_TEXMAP_UNITS],
                   rtexmapunit_t rTUs[NUM_TEXMAP_UNITS],
                   const material_snapshot_t* msA, float inter,
                   const material_snapshot_t* msB)
{
    rTU[TU_PRIMARY].tex = msA->units[MTU_PRIMARY].texInst->id;
    rTU[TU_PRIMARY].magMode = msA->units[MTU_PRIMARY].magMode;
    rTU[TU_PRIMARY].scale[0] = msA->units[MTU_PRIMARY].scale[0];
    rTU[TU_PRIMARY].scale[1] = msA->units[MTU_PRIMARY].scale[1];
    rTU[TU_PRIMARY].offset[0] = msA->units[MTU_PRIMARY].offset[0];
    rTU[TU_PRIMARY].offset[1] = msA->units[MTU_PRIMARY].offset[1];
    rTU[TU_PRIMARY].blendMode = msA->units[MTU_PRIMARY].blendMode;
    rTU[TU_PRIMARY].blend = msA->units[MTU_PRIMARY].alpha;

    if(msA->units[MTU_DETAIL].texInst)
    {
        rTU[TU_PRIMARY_DETAIL].tex = msA->units[MTU_DETAIL].texInst->id;
        rTU[TU_PRIMARY_DETAIL].magMode = msA->units[MTU_DETAIL].magMode;
        rTU[TU_PRIMARY_DETAIL].scale[0] = msA->units[MTU_DETAIL].scale[0];
        rTU[TU_PRIMARY_DETAIL].scale[1] = msA->units[MTU_DETAIL].scale[1];
        rTU[TU_PRIMARY_DETAIL].offset[0] = msA->units[MTU_DETAIL].offset[0];
        rTU[TU_PRIMARY_DETAIL].offset[1] = msA->units[MTU_DETAIL].offset[1];
        rTU[TU_PRIMARY_DETAIL].blendMode = msA->units[MTU_DETAIL].blendMode;
        rTU[TU_PRIMARY_DETAIL].blend = msA->units[MTU_DETAIL].alpha;
    }

    if(msB && msB->units[MTU_PRIMARY].texInst)
    {
        rTU[TU_INTER].tex = msB->units[MTU_PRIMARY].texInst->id;
        rTU[TU_INTER].magMode = msB->units[MTU_PRIMARY].magMode;
        rTU[TU_INTER].scale[0] = msB->units[MTU_PRIMARY].scale[0];
        rTU[TU_INTER].scale[1] = msB->units[MTU_PRIMARY].scale[1];
        rTU[TU_INTER].offset[0] = msB->units[MTU_PRIMARY].offset[0];
        rTU[TU_INTER].offset[1] = msB->units[MTU_PRIMARY].offset[1];
        rTU[TU_INTER].blendMode = msB->units[MTU_PRIMARY].blendMode;
        rTU[TU_INTER].blend = msB->units[MTU_PRIMARY].alpha;

        // Blend between the primary and inter textures.
        rTU[TU_INTER].blend = inter;
    }

    if(msB && msB->units[MTU_DETAIL].texInst)
    {
        rTU[TU_INTER_DETAIL].tex = msB->units[MTU_DETAIL].texInst->id;
        rTU[TU_INTER_DETAIL].magMode = msB->units[MTU_DETAIL].magMode;
        rTU[TU_INTER_DETAIL].scale[0] = msB->units[MTU_DETAIL].scale[0];
        rTU[TU_INTER_DETAIL].scale[1] = msB->units[MTU_DETAIL].scale[1];
        rTU[TU_INTER_DETAIL].offset[0] = msB->units[MTU_DETAIL].offset[0];
        rTU[TU_INTER_DETAIL].offset[1] = msB->units[MTU_DETAIL].offset[1];
        rTU[TU_INTER_DETAIL].blendMode = msB->units[MTU_DETAIL].blendMode;
        rTU[TU_INTER_DETAIL].blend = msB->units[MTU_DETAIL].alpha;

        // Blend between the primary and inter detail textures.
        rTU[TU_INTER_DETAIL].blend = inter;
    }

    if(msA->units[MTU_REFLECTION].texInst)
    {
        rTUs[TU_PRIMARY].tex = msA->units[MTU_REFLECTION].texInst->id;
        rTUs[TU_PRIMARY].magMode = msA->units[MTU_REFLECTION].magMode;
        rTUs[TU_PRIMARY].scale[0] = msA->units[MTU_REFLECTION].scale[0];
        rTUs[TU_PRIMARY].scale[1] = msA->units[MTU_REFLECTION].scale[1];
        rTUs[TU_PRIMARY].offset[0] = msA->units[MTU_REFLECTION].offset[0];
        rTUs[TU_PRIMARY].offset[1] = msA->units[MTU_REFLECTION].offset[1];
        rTUs[TU_PRIMARY].blendMode = msA->units[MTU_REFLECTION].blendMode;
        rTUs[TU_PRIMARY].blend = msA->units[MTU_REFLECTION].alpha;

        if(msA->units[MTU_REFLECTION_MASK].texInst)
        {
            rTUs[TU_INTER].tex = msA->units[MTU_REFLECTION_MASK].texInst->id;
            rTUs[TU_INTER].magMode = msA->units[MTU_REFLECTION_MASK].magMode;
            rTUs[TU_INTER].scale[0] = msA->units[MTU_REFLECTION_MASK].scale[0];
            rTUs[TU_INTER].scale[1] = msA->units[MTU_REFLECTION_MASK].scale[1];
            rTUs[TU_INTER].offset[0] = msA->units[MTU_REFLECTION_MASK].offset[0];
            rTUs[TU_INTER].offset[1] = msA->units[MTU_REFLECTION_MASK].offset[1];
            rTUs[TU_INTER].blendMode = msA->units[MTU_REFLECTION_MASK].blendMode;
            rTUs[TU_INTER].blend = msA->units[MTU_REFLECTION_MASK].alpha;
        }
    }
}

/**
 * Apply primitive-specific manipulations.
 */
void Rend_SetupRTU2(rtexmapunit_t rTU[NUM_TEXMAP_UNITS],
                    rtexmapunit_t rTUs[NUM_TEXMAP_UNITS],
                      boolean isWall, const float texOffset[2],
                      const float texScale[2],
                      const material_snapshot_t* msA,
                      const material_snapshot_t* msB)
{
    if(texScale)
    {
        rTU[TU_PRIMARY].scale[0] *= texScale[0];
        rTU[TU_PRIMARY].scale[1] *= texScale[1];
    }
    if(texOffset)
    {
        rTU[TU_PRIMARY].offset[0] += texOffset[0] / msA->width;
        rTU[TU_PRIMARY].offset[1] +=
            (isWall? texOffset[1] : -texOffset[1]) / msA->height;
    }

    if(msA->units[MTU_DETAIL].texInst && texOffset)
    {
        rTU[TU_PRIMARY_DETAIL].offset[0] +=
            texOffset[0] * rTU[TU_PRIMARY_DETAIL].scale[0];
        rTU[TU_PRIMARY_DETAIL].offset[1] +=
            (isWall? texOffset[1] : -texOffset[1]) *
                rTU[TU_PRIMARY_DETAIL].scale[1];
    }

    if(msB && msB->units[MTU_PRIMARY].texInst)
    {
        if(texScale)
        {
            rTU[TU_INTER].scale[0] *= texScale[0];
            rTU[TU_INTER].scale[1] *= texScale[1];
        }
        if(texOffset)
        {
            rTU[TU_INTER].offset[0] += texOffset[0] / msB->width;
            rTU[TU_INTER].offset[1] +=
                (isWall? texOffset[1] : -texOffset[1]) / msB->height;
        }
    }

    if(msB && msB->units[MTU_DETAIL].texInst && texOffset)
    {
        rTU[TU_INTER_DETAIL].offset[0] +=
            texOffset[0] * rTU[TU_INTER_DETAIL].scale[0];
        rTU[TU_INTER_DETAIL].offset[1] +=
            (isWall? texOffset[1] : -texOffset[1]) *
                rTU[TU_INTER_DETAIL].scale[1];
    }

    if(msA->units[MTU_REFLECTION].texInst)
    {
        if(texScale)
        {
            rTUs[TU_INTER].scale[0] *= texScale[0];
            rTUs[TU_INTER].scale[1] *= texScale[1];
        }
        if(texOffset)
        {
            rTUs[TU_INTER].offset[0] += texOffset[0] / msA->width;
            rTUs[TU_INTER].offset[1] += (isWall? texOffset[1] : -texOffset[1]) /
                msA->height;
        }
    }
}

static void lightPolygon(rcolor_t* rcolors, size_t numVertices,
    const rvertex_t* rvertices, float sectorLightLevel,
    const float* sectorLightColor, biassurface_t* biasSurface,
    float surfaceLightLevelDeltaL, float surfaceLightLevelDeltaR,
    const float* surfaceColorTint, const float* surfaceColorTint2,
    float alpha, boolean isSkyMasked, boolean isGlowing,
    const float* surfaceNormal, const float* pointInPlane,
    const float* from, const float* to)
{
    boolean isPlane = (pointInPlane != NULL? true : false);

    if(isSkyMasked)
    {
        memset(rcolors, 0, sizeof(rcolor_t) * numVertices);
        return;
    }

    if(isGlowing || mapFullBright)
    {   // Uniform colour. Apply to all vertices.
        R_VertexColorsGlow(rcolors, numVertices);
    }
    else
    {   // Non-uniform color.
        if(useBias && biasSurface)
        {
            if(isPlane)
            {
                // Do BIAS lighting for this poly.
                SB_RendPlane(P_CurrentMap(), rcolors, biasSurface,
                             rvertices, numVertices, surfaceNormal, sectorLightLevel,
                             pointInPlane);
            }
            else
            {
                // Do BIAS lighting for this poly.
                SB_RendSeg(P_CurrentMap(), rcolors, biasSurface,
                           rvertices, numVertices, surfaceNormal, sectorLightLevel,
                           from, to);
            }
        }
        else
        {
            float blendedColor[4];
            const float* finalColor;

            // Blended with surface color tint?
            if(surfaceColorTint &&
               (surfaceColorTint[CR] < 1 || surfaceColorTint[CG] < 1 || surfaceColorTint[CB] < 1))
            {   // Blend sector light+color+surfacecolor
                blendedColor[CR] = surfaceColorTint[CR] * sectorLightColor[CR];
                blendedColor[CG] = surfaceColorTint[CG] * sectorLightColor[CG];
                blendedColor[CB] = surfaceColorTint[CB] * sectorLightColor[CB];
                blendedColor[CA] = 1;
                finalColor = &blendedColor[0];
            }
            else
            {   // Use sector light+color only
                finalColor = sectorLightColor;
            }

            if(isPlane)
            {
                float ll = sectorLightLevel + surfaceLightLevelDeltaL;
                uint i;

                // Use sector light+color only
                for(i = 0; i < numVertices; ++i)
                    R_VertexColorsApplyAmbientLight(&rcolors[i], &rvertices[i], ll, finalColor);
            }
            else
            {
                float lightLevelL = sectorLightLevel + surfaceLightLevelDeltaL;
                float lightLevelR = sectorLightLevel + surfaceLightLevelDeltaR;

                R_VertexColorsApplyAmbientLight(&rcolors[0], &rvertices[0], lightLevelL, finalColor);
                R_VertexColorsApplyAmbientLight(&rcolors[1], &rvertices[1], lightLevelL, finalColor);
                R_VertexColorsApplyAmbientLight(&rcolors[2], &rvertices[2], lightLevelR, finalColor);
                R_VertexColorsApplyAmbientLight(&rcolors[3], &rvertices[3], lightLevelR, finalColor);

                // Bottom color (if different from top)?
                if(surfaceColorTint2)
                {
                    float vColor[3];

                    // Blend sector light+color+surfacecolor
                    vColor[CR] = surfaceColorTint2[CR] * sectorLightColor[CR];
                    vColor[CG] = surfaceColorTint2[CG] * sectorLightColor[CG];
                    vColor[CB] = surfaceColorTint2[CB] * sectorLightColor[CB];

                    R_VertexColorsApplyAmbientLight(&rcolors[0], &rvertices[0], lightLevelL, vColor);
                    R_VertexColorsApplyAmbientLight(&rcolors[2], &rvertices[2], lightLevelR, vColor);
                }
            }
        }

        R_VertexColorsApplyTorchLight(rcolors, rvertices, numVertices);
    }

    // Apply uniform alpha.
    R_VertexColorsAlpha(rcolors, numVertices, alpha);
}

static void lightShinyPolygon(rcolor_t* rcolors, const rcolor_t* baseColors, uint numVertices,
                              const float minColor[3], float alpha)
{
    uint i;

    // Strength of the shine.
    for(i = 0; i < numVertices; ++i)
    {
        rcolors[i].rgba[CR] = MAX_OF(baseColors[i].rgba[CR], minColor[CR]);
        rcolors[i].rgba[CG] = MAX_OF(baseColors[i].rgba[CG], minColor[CG]);
        rcolors[i].rgba[CB] = MAX_OF(baseColors[i].rgba[CB], minColor[CB]);
        rcolors[i].rgba[CA] = alpha;
    }
}

static float averageLuma(const rcolor_t* rcolors, uint size)
{
    float avg = 0;
    uint i;

    for(i = 0; i < size; ++i)
    {
        avg += rcolors[i].rgba[CR];
        avg += rcolors[i].rgba[CG];
        avg += rcolors[i].rgba[CB];
    }

    avg /= (float) size * 3;

    return avg;
}

static DGLuint initModTexFromDynlight(float modColor[3], float modTexTC[2][2],
                                      const dynlight_t* dyn)
{
    modColor[CR] = dyn->color[CR];
    modColor[CG] = dyn->color[CG];
    modColor[CB] = dyn->color[CB];
    modTexTC[0][0] = dyn->s[0];
    modTexTC[0][1] = dyn->s[1];
    modTexTC[1][0] = dyn->t[0];
    modTexTC[1][1] = dyn->t[1];

    return dyn->texture;
}

static void divideVertices(rvertex_t* rvertices, const walldiv_t* divs)
{
    rvertex_t origVerts[4];
    memcpy(origVerts, rvertices, sizeof(rvertex_t) * 4);
    R_DivVerts(rvertices, origVerts, divs);
}

static void divideCoords(rtexcoord_t* rcoords, const walldiv_t* divs,
                         float bL, float tL, float bR, float tR)
{
    rtexcoord_t origCoords[4];
    memcpy(origCoords, rcoords, sizeof(rtexcoord_t) * 4);
    R_DivTexCoords(rcoords, origCoords, divs, bL, tL, bR, tR);
}

static void divideColors(rcolor_t* rcolors, const walldiv_t* divs,
                         float bL, float tL, float bR, float tR)
{
    rcolor_t origColors[4];
    memcpy(origColors, rcolors, sizeof(rcolor_t) * 4);
    R_DivVertColors(rcolors, origColors, divs, bL, tL, bR, tR);
}

void Rend_WriteDynlight(const dynlight_t* dyn, const rvertex_t* rvertices,
                        uint numVertices, uint numVerticesIncludingDivisions,
                        const float* texQuadBottomRight, const float* texQuadTopLeft,
                        boolean isWall, const walldiv_t* divs)
{
    rtexmapunit_t rTU[NUM_TEXMAP_UNITS];
    rvertex_t* dynVertices;
    rtexcoord_t* rtexcoords;
    rcolor_t* rcolors;
    uint i;

    memset(rTU, 0, sizeof(rTU));

    // Allocate enough for the divisions too.
    dynVertices = R_AllocRendVertices(numVerticesIncludingDivisions);
    rtexcoords = R_AllocRendTexCoords(numVerticesIncludingDivisions);
    rcolors = R_AllocRendColors(numVerticesIncludingDivisions);

    rTU[TU_PRIMARY].tex = dyn->texture;
    rTU[TU_PRIMARY].magMode = GL_LINEAR;

    rTU[TU_PRIMARY_DETAIL].tex = 0;
    rTU[TU_INTER].tex = 0;
    rTU[TU_INTER_DETAIL].tex = 0;

    for(i = 0; i < numVertices; ++i)
    {
        rcolor_t* col = &rcolors[i];
        uint c;

        // Each vertex uses the light's color.
        for(c = 0; c < 3; ++c)
            col->rgba[c] = dyn->color[c];
        col->rgba[3] = 1;
    }

    if(isWall)
    {
        rtexcoords[1].st[0] = rtexcoords[0].st[0] = dyn->s[0];
        rtexcoords[1].st[1] = rtexcoords[3].st[1] = dyn->t[0];
        rtexcoords[3].st[0] = rtexcoords[2].st[0] = dyn->s[1];
        rtexcoords[2].st[1] = rtexcoords[0].st[1] = dyn->t[1];

        if(divs && (divs[0].num || divs[1].num))
        {   // We need to subdivide the dynamic light quad.
            rvertex_t origVerts[4];
            rcolor_t origColors[4];
            rtexcoord_t origTexCoords[4];
            float bL, tL, bR, tR;

            /**
             * Need to swap indices around into fans set the position
             * of the division vertices, interpolate texcoords and
             * color.
             */

            memcpy(origVerts, rvertices, sizeof(rvertex_t) * 4);
            memcpy(origTexCoords, rtexcoords, sizeof(rtexcoord_t) * 4);
            memcpy(origColors, rcolors, sizeof(rcolor_t) * 4);

            bL = rvertices[0].pos[VZ];
            tL = rvertices[1].pos[VZ];
            bR = rvertices[2].pos[VZ];
            tR = rvertices[3].pos[VZ];

            R_DivVerts(dynVertices, origVerts, divs);
            R_DivTexCoords(rtexcoords, origTexCoords, divs, bL, tL, bR, tR);
            R_DivVertColors(rcolors, origColors, divs, bL, tL, bR, tR);
        }
        else
        {
            memcpy(dynVertices, rvertices, sizeof(rvertex_t) * numVertices);
        }
    }
    else
    {   // It's a flat.
        float width  = texQuadBottomRight[VX] - texQuadTopLeft[VX];
        float height = texQuadBottomRight[VY] - texQuadTopLeft[VY];
        uint i;

        for(i = 0; i < numVertices; ++i)
        {
            rtexcoords[i].st[0] = ((texQuadBottomRight[VX] - rvertices[i].pos[VX]) / width * dyn->s[0]) +
                ((rvertices[i].pos[VX] - texQuadTopLeft[VX]) / width * dyn->s[1]);

            rtexcoords[i].st[1] = ((texQuadBottomRight[VY] - rvertices[i].pos[VY]) / height * dyn->t[0]) +
                ((rvertices[i].pos[VY] - texQuadTopLeft[VY]) / height * dyn->t[1]);
        }

        memcpy(dynVertices, rvertices, sizeof(rvertex_t) * numVertices);
    }

    if(isWall && divs && (divs[0].num || divs[1].num))
    {
        RL_AddPoly(PT_FAN, RPT_LIGHT, 3 + divs[1].num, dynVertices + 3 + divs[0].num,
                   NULL, rtexcoords + 3 + divs[0].num, NULL,
                   rcolors + 3 + divs[0].num,
                   0, 0, NULL, rTU);
        RL_AddPoly(PT_FAN, RPT_LIGHT, 3 + divs[0].num, dynVertices,
                   NULL, rtexcoords, NULL, rcolors,
                   0, 0, NULL, rTU);
    }
    else
    {
        RL_AddPoly(isWall? PT_TRIANGLE_STRIP : PT_FAN, RPT_LIGHT,
                   numVertices, dynVertices, NULL, rtexcoords, NULL,
                   rcolors, 0,
                   0, NULL, rTU);
    }

    R_FreeRendVertices(dynVertices);
    R_FreeRendTexCoords(rtexcoords);
    R_FreeRendColors(rcolors);
}

boolean DLIT_GetFirstDynlight(const dynlight_t* dyn, void* data)
{
    *((dynlight_t**) data) = (dynlight_t*) dyn;
    return false; // Stop iteration.
}

typedef struct {
    uint            lastIdx;
    const rvertex_t* rvertices;
    uint            numVertices, realNumVertices;
    const float*    texTL, *texBR;
    boolean         isWall;
    const walldiv_t* divs;
} dynlightiterparams_t;

boolean DLIT_WriteDynlight(const dynlight_t* dyn, void* data)
{
    dynlightiterparams_t* p = data;

    // If multitexturing is in use, we skip the first light.
    if(!(RL_IsMTexLights() && p->lastIdx == 0))
    {
        Rend_WriteDynlight(dyn, p->rvertices, p->numVertices, p->realNumVertices,
                           p->texBR, p->texTL, p->isWall, p->divs);
    }

    p->lastIdx++;

    return true; // Continue iteration.
}

/**
 * Generate a dynlight primitive for each of the lights affecting the surface.
 * Multitexturing may be used for the first light, so it is skipped.
 */
static void writeDynlights(uint dynlistID, const rvertex_t* rvertices,
                           uint numVertices, uint realNumVertices,
                           boolean isWall, const walldiv_t* divs,
                           const float texQuadTopLeft[3],
                           const float texQuadBottomRight[3],
                           uint* numLights)
{
    dynlightiterparams_t dlparams;

    dlparams.rvertices = rvertices;
    dlparams.numVertices = numVertices;
    dlparams.realNumVertices = realNumVertices;
    dlparams.lastIdx = 0;
    dlparams.isWall = isWall;
    dlparams.divs = divs;
    dlparams.texTL = texQuadTopLeft;
    dlparams.texBR = texQuadBottomRight;

    DL_DynlistIterator(dynlistID, DLIT_WriteDynlight, &dlparams);

    *numLights += dlparams.lastIdx;
    if(RL_IsMTexLights())
        *numLights -= 1;
}

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
void Rend_AddMaskedPoly(const rvertex_t* rvertices,
                        const rcolor_t* rcolors, float wallLength,
                        float texWidth, float texHeight,
                        const float texOffset[2], blendmode_t blendMode,
                        uint lightListIdx, boolean glow, boolean masked,
                        const rtexmapunit_t rTU[NUM_TEXMAP_UNITS])
{
    vissprite_t*        vis = R_NewVisSprite();
    int                 i, c;
    float               midpoint[3];

    midpoint[VX] = (rvertices[0].pos[VX] + rvertices[3].pos[VX]) / 2;
    midpoint[VY] = (rvertices[0].pos[VY] + rvertices[3].pos[VY]) / 2;
    midpoint[VZ] = (rvertices[0].pos[VZ] + rvertices[3].pos[VZ]) / 2;

    vis->type = VSPR_MASKED_WALL;
    vis->center[VX] = midpoint[VX];
    vis->center[VY] = midpoint[VY];
    vis->center[VZ] = midpoint[VZ];
    vis->distance = R_PointDist2D(midpoint);
    vis->data.wall.tex = rTU[TU_PRIMARY].tex;
    vis->data.wall.magMode = rTU[TU_PRIMARY].magMode;
    vis->data.wall.masked = masked;
    for(i = 0; i < 4; ++i)
    {
        vis->data.wall.vertices[i].pos[VX] = rvertices[i].pos[VX];
        vis->data.wall.vertices[i].pos[VY] = rvertices[i].pos[VY];
        vis->data.wall.vertices[i].pos[VZ] = rvertices[i].pos[VZ];

        for(c = 0; c < 4; ++c)
        {
            vis->data.wall.vertices[i].color[c] =
                MINMAX_OF(0, rcolors[i].rgba[c], 1);
        }
    }

    vis->data.wall.texCoord[0][VX] = (texOffset? texOffset[VX] / texWidth : 0);
    vis->data.wall.texCoord[1][VX] =
        vis->data.wall.texCoord[0][VX] + wallLength / texWidth;
    vis->data.wall.texCoord[0][VY] = (texOffset? texOffset[VY] / texHeight : 0);
    vis->data.wall.texCoord[1][VY] =
        vis->data.wall.texCoord[0][VY] +
            (rvertices[3].pos[VZ] - rvertices[0].pos[VZ]) / texHeight;
    vis->data.wall.blendMode = blendMode;

    //// \fixme Semitransparent masked polys arn't lit atm
    if(!glow && lightListIdx && numTexUnits > 1 && envModAdd &&
       !(rcolors[0].rgba[CA] < 1))
    {
        dynlight_t*         dyn = NULL;

        /**
         * The dynlights will have already been sorted so that the brightest
         * and largest of them is first in the list. So grab that one.
         */
        DL_DynlistIterator(lightListIdx, DLIT_GetFirstDynlight, &dyn);

        vis->data.wall.modTex = dyn->texture;
        vis->data.wall.modTexCoord[0][0] = dyn->s[0];
        vis->data.wall.modTexCoord[0][1] = dyn->s[1];
        vis->data.wall.modTexCoord[1][0] = dyn->t[0];
        vis->data.wall.modTexCoord[1][1] = dyn->t[1];
        for(c = 0; c < 3; ++c)
            vis->data.wall.modColor[c] = dyn->color[c];
    }
    else
    {
        vis->data.wall.modTex = 0;
    }
}

static void renderWorldSegAsVisSprite(rendseg_t* rseg, rvertex_t* rvertices,
                                      const rtexmapunit_t rTU[NUM_TEXMAP_UNITS])
{
    boolean forceOpaque = rseg->alpha < 0 ? true : false;
    float alpha = forceOpaque ? 1 : rseg->alpha;
    rcolor_t* rcolors = R_AllocRendColors(4);
    float texQuadWidth = P_AccurateDistance(
        rseg->texQuadBottomRight[VX] - rseg->texQuadTopLeft[VX],
        rseg->texQuadBottomRight[VY] - rseg->texQuadTopLeft[VY]);

    lightPolygon(rcolors, 4, rvertices,
                 rseg->sectorLightLevel, rseg->sectorLightColor,
                 rseg->biasSurface, rseg->surfaceLightLevelDeltaL, rseg->surfaceLightLevelDeltaR,
                 rseg->surfaceColorTint, rseg->surfaceColorTint2, alpha,
                 RendSeg_SkyMasked(rseg), (rseg->flags & RSF_GLOW) != 0,
                 rseg->normal, NULL, rseg->from, rseg->to);

    /**
     * Masked polys (walls) get a special treatment (=> vissprite).
     * This is needed because all masked polys must be sorted (sprites
     * are masked polys). Otherwise there will be artifacts.
     */
    Rend_AddMaskedPoly(rvertices, rcolors, texQuadWidth,
                       rseg->materials.snapshotA.width,
                       rseg->materials.snapshotA.height, rseg->surfaceMaterialOffset,
                       rseg->blendMode, rseg->dynlistID,
                       (rseg->flags & RSF_GLOW) != 0,
                       !rseg->materials.snapshotA.isOpaque, rTU);

    R_FreeRendColors(rcolors);
}

dynlight_t* Rend_PickDynlightForModTex(uint dynlistID)
{
    dynlight_t* dyn = NULL;
    DL_DynlistIterator(dynlistID, DLIT_GetFirstDynlight, &dyn);
    return dyn;
}

static DGLuint pickModulationTexture(uint dynlistID, float modTexTC[2][2],
                                     float modColor[3])
{
    dynlight_t* dyn = Rend_PickDynlightForModTex(dynlistID);
    if(dyn)
        return initModTexFromDynlight(modColor, modTexTC, dyn);
    return 0;
}

/**
 * Set the vertex colors in the rendpoly.
 */
static void setShadowColor(rcolor_t* rcolors, uint num, float alpha)
{
    uint i;

    alpha = MINMAX_OF(0, alpha, 1);

    for(i = 0; i < num; ++i)
    {
        // Shadows are black.
        rcolors[i].rgba[CR] = rcolors[i].rgba[CG] = rcolors[i].rgba[CB] = 0;
        rcolors[i].rgba[CA] = alpha;
    }
}

static uint buildGeometryFromRendSeg(rendseg_t* rseg,
                                     DGLuint modTex, const float modTexTC[2][2], const float modColor[3],
                                     rvertex_t** rvertices, rcolor_t** rcolors, int rcolorIndices[NUM_COLOR_INDICES],
                                     rtexcoord_t** rcoords, int rcoordIndices[NUM_COORD_INDICES],
                                     rtexmapunit_t rTU[NUM_TEXMAP_UNITS], rtexmapunit_t rTUs[NUM_TEXMAP_UNITS],
                                     rtexmapunit_t radioTU[NUM_TEXMAP_UNITS],
                                     rtexmapunit_t radioTU2[NUM_TEXMAP_UNITS],
                                     rtexmapunit_t radioTU3[NUM_TEXMAP_UNITS],
                                     rtexmapunit_t radioTU4[NUM_TEXMAP_UNITS])
{
    uint numVertices, numCoords, numColors;
    float texQuadWidth, texOffset[2] = { 0, 0 };
    rvertex_t* vertices = NULL;
    rtexcoord_t* coords = NULL;
    rcolor_t* colors = NULL;

    R_TexmapUnitsFromRendSeg(rseg, rTU, rTUs, radioTU, radioTU2, radioTU3, radioTU4);

    numVertices = numColors = numCoords = RendSeg_NumRequiredVertices(rseg);

    memset(rcolorIndices, -1, sizeof(uint) * NUM_COLOR_INDICES);
    memset(rcoordIndices, -1, sizeof(uint) * NUM_COORD_INDICES);

    if(modTex)
    {
        rcoordIndices[COORD_MODULATE] = numCoords;
        numCoords += numVertices;
    }

    if(!RendSeg_SkyMasked(rseg))
    {
        // ShinySurface?
        if(!(rseg->flags & RSF_NO_REFLECTION) && rTUs[TU_PRIMARY].tex)
        {
            // We'll reuse the same verts but we need new colors.
            rcolorIndices[COLOR_SHINY] = numColors;
            numColors += numVertices;

            rcoordIndices[COORD_SHINY] = numCoords;
            numCoords += numVertices;
        }

        // Fakeradio?
        if(radioTU[TU_PRIMARY].tex)
        {
            rcolorIndices[COLOR_RADIO_TOP] = numColors;
            numColors += numVertices;
        }
        if(radioTU2[TU_PRIMARY].tex)
        {
            rcolorIndices[COLOR_RADIO_BOTTOM] = numColors;
            numColors += numVertices;
        }
        if(radioTU3[TU_PRIMARY].tex)
        {
            rcolorIndices[COLOR_RADIO_LEFT] = numColors;
            numColors += numVertices;
        }
        if(radioTU4[TU_PRIMARY].tex)
        {
            rcolorIndices[COLOR_RADIO_RIGHT] = numColors;
            numColors += numVertices;
        }
    }

    vertices = R_AllocRendVertices(numVertices);
    colors = R_AllocRendColors(numColors);
    coords = R_AllocRendTexCoords(numCoords);

    rcoordIndices[COORD_PRIMARY] = 0;
    rcolorIndices[COLOR_PRIMARY] = 0;

    V3_Set(vertices[0].pos, rseg->from[VX], rseg->from[VY], rseg->bottom);
    V3_Set(vertices[1].pos, rseg->from[VX], rseg->from[VY], rseg->top);
    V3_Set(vertices[2].pos, rseg->to  [VX], rseg->to  [VY], rseg->bottom);
    V3_Set(vertices[3].pos, rseg->to  [VX], rseg->to  [VY], rseg->top);

    texQuadWidth = P_AccurateDistance(
        rseg->texQuadBottomRight[VX] - rseg->texQuadTopLeft[VX],
        rseg->texQuadBottomRight[VY] - rseg->texQuadTopLeft[VY]);

    // Primary texture coordinates.
    quadTexCoords(coords + rcoordIndices[COORD_PRIMARY], vertices, texQuadWidth, 1, 1, rseg->texQuadTopLeft, texOffset);

    // Modulation texture coordinates.
    if(modTex)
        quadLightCoords(coords + rcoordIndices[COORD_MODULATE], modTexTC[0], modTexTC[1]);

    // Shiny texture coordinates.
    if(!(rseg->flags & RSF_NO_REFLECTION) && rTUs[TU_PRIMARY].tex)
        quadShinyTexCoords(coords + rcoordIndices[COORD_SHINY], &vertices[1], &vertices[2], texQuadWidth);

    // Fakeradio?
    if(radioTU[TU_PRIMARY].tex)
    {
        const rendseg_shadow_t* shadow = &rseg->radioConfig[0];
        setShadowColor(colors + rcolorIndices[COLOR_RADIO_TOP], 4, shadow->shadowDark);
    }

    if(radioTU2[TU_PRIMARY].tex)
    {
        const rendseg_shadow_t* shadow = &rseg->radioConfig[1];
        setShadowColor(colors + rcolorIndices[COLOR_RADIO_BOTTOM], 4, shadow->shadowDark);
    }

    if(radioTU3[TU_PRIMARY].tex)
    {
        const rendseg_shadow_t* shadow = &rseg->radioConfig[2];
        setShadowColor(colors + rcolorIndices[COLOR_RADIO_LEFT], 4, shadow->shadowDark);
    }

    if(radioTU4[TU_PRIMARY].tex)
    {
        const rendseg_shadow_t* shadow = &rseg->radioConfig[3];
        setShadowColor(colors + rcolorIndices[COLOR_RADIO_RIGHT], 4, shadow->shadowDark);
    }

    *rvertices = vertices;
    *rcolors = colors;
    *rcoords = coords;

    return numVertices;
}

static uint buildGeometryFromRendPlane(rendplane_t* rplane,
    DGLuint modTex, const float modTexTC[2][2], const float modColor[3],
    rvertex_t** rvertices, rcolor_t** rcolors,
    int rcolorIndices[NUM_COLOR_INDICES],
    rtexcoord_t** rcoords, int rcoordIndices[NUM_COORD_INDICES],
    rtexmapunit_t rTU[NUM_TEXMAP_UNITS], rtexmapunit_t rTUs[NUM_TEXMAP_UNITS])
{
    uint i, numVertices, numCoords, numColors;
    rvertex_t* vertices = NULL;
    rtexcoord_t* coords = NULL;
    rcolor_t* colors = NULL;
    const material_snapshot_t* msA = &rplane->materials.snapshotA;
    const material_snapshot_t* msB =
        (rplane->materials.blend) ? &rplane->materials.snapshotB : NULL;

    memset(rTU, 0, sizeof(rtexmapunit_t) * NUM_TEXMAP_UNITS);
    memset(rTUs, 0, sizeof(rtexmapunit_t) * NUM_TEXMAP_UNITS);

    if(!RendPlane_SkyMasked(rplane))
    {
        Rend_SetupRTU(rTU, rTUs, msA, rplane->materials.inter, msB);
        Rend_SetupRTU2(rTU, rTUs, false, rplane->surfaceMaterialOffset, rplane->surfaceMaterialScale, msA, msB);
    }

    vertices = R_VerticesFromRendPlane(rplane, rplane->face, rplane->height, !(rplane->normal[VZ] > 0), &numVertices);
    numColors = numCoords = numVertices;

    memset(rcolorIndices, -1, sizeof(uint) * NUM_COLOR_INDICES);
    memset(rcoordIndices, -1, sizeof(uint) * NUM_COORD_INDICES);

    if(modTex)
    {
        rcoordIndices[COORD_MODULATE] = numCoords;
        numCoords += numVertices;
    }

    if(!RendPlane_SkyMasked(rplane))
    {
        // ShinySurface?
        if(!(rplane->flags & RPF_NO_REFLECTION) && rTUs[TU_PRIMARY].tex)
        {
            // We'll reuse the same verts but we need new colors.
            rcolorIndices[COLOR_SHINY] = numColors;
            numColors += numVertices;
            // The normal texcoords are used with the mask.
            // New texcoords are required for shiny texture.
            rcoordIndices[COORD_SHINY] = numCoords;
            numCoords += numVertices;
        }
    }

    colors = R_AllocRendColors(numColors);
    coords = R_AllocRendTexCoords(numCoords);

    rcoordIndices[COORD_PRIMARY] = 0;
    rcolorIndices[COLOR_PRIMARY] = 0;

    for(i = 0; i < numVertices; ++i)
    {
        const rvertex_t* vtx = &vertices[i];
        float xyz[3];

        xyz[VX] = vtx->pos[VX] - rplane->texQuadTopLeft[VX];
        xyz[VY] = vtx->pos[VY] - rplane->texQuadTopLeft[VY];
        xyz[VZ] = vtx->pos[VZ] - rplane->texQuadTopLeft[VZ];

        // Primary texture coordinates.
        if(rTU[TU_PRIMARY].tex)
        {
            rtexcoord_t* tc = coords + rcoordIndices[COORD_PRIMARY] + i;
            tc->st[0] =  xyz[VX];
            tc->st[1] = -xyz[VY];
        }

        // Shiny texture coordinates.
        if(!(rplane->flags & RPF_NO_REFLECTION) && rTUs[TU_PRIMARY].tex)
        {
            flatShinyTexCoords(coords + rcoordIndices[COORD_SHINY] + i, vtx->pos);
        }

        // First light texture coordinates.
        if(modTex)
        {
            rtexcoord_t* tc = coords + rcoordIndices[COORD_MODULATE] + i;
            float width, height;

            width  = rplane->texQuadBottomRight[VX] - rplane->texQuadTopLeft[VX];
            height = rplane->texQuadBottomRight[VY] - rplane->texQuadTopLeft[VY];

            tc->st[0] = ((rplane->texQuadBottomRight[VX] - vtx->pos[VX]) / width * modTexTC[0][0]) +
                (xyz[VX] / width * modTexTC[0][1]);
            tc->st[1] = ((rplane->texQuadBottomRight[VY] - vtx->pos[VY]) / height * modTexTC[1][0]) +
                (xyz[VY] / height * modTexTC[1][1]);
        }
    }

    *rvertices = vertices;
    *rcolors = colors;
    *rcoords = coords;

    return numVertices;
}

static __inline void writePolygon(rendpolytype_t polyType, const walldiv_t* divs,
    uint numVertices, const rvertex_t* rvertices, const rcolor_t* rcolors,
    const rtexcoord_t* rtcPrimary, const rtexcoord_t* rtcInter,
    const rtexcoord_t* rtcModTex, DGLuint modTex, const float modColor[3],
    const rtexmapunit_t rTU[NUM_TEXMAP_UNITS], uint numLights)
{
    if(divs && (divs[0].num || divs[1].num))
    {
        RL_AddPoly(PT_FAN, polyType, 3 + divs[1].num, rvertices + 3 + divs[0].num,
                   rtcModTex? rtcModTex + 3 + divs[0].num : NULL,
                   rtcPrimary + 3 + divs[0].num,
                   rtcInter? rtcInter + 3 + divs[0].num : NULL,
                   rcolors + 3 + divs[0].num,
                   numLights, modTex, modColor, rTU);

        RL_AddPoly(PT_FAN, polyType, 3 + divs[0].num, rvertices, rtcModTex,
                   rtcPrimary, rtcInter, rcolors,
                   numLights, modTex, modColor, rTU);

        return;
    }

    RL_AddPoly(PT_TRIANGLE_STRIP, polyType, numVertices, rvertices,
               rtcModTex, rtcPrimary, rtcInter, rcolors,
               numLights, modTex, modColor, rTU);
}

static __inline void writeShinyPolygon(rendpolytype_t polyType, const walldiv_t* divs,
    uint numVertices, const rvertex_t* rvertices,
    const rcolor_t* rcolors, const rtexcoord_t* rtcPrimary,
    const rtexcoord_t* rtcShiny, const rtexmapunit_t rTU[NUM_TEXMAP_UNITS])
{
    if(divs && (divs[0].num || divs[1].num))
    {
        RL_AddPoly(PT_FAN, polyType, 3 + divs[1].num, rvertices + 3 + divs[0].num,
                   NULL, rtcShiny? rtcShiny + 3 + divs[0].num : NULL,
                   rTU[TU_INTER].tex? rtcPrimary + 3 + divs[0].num : NULL,
                   rcolors + 3 + divs[0].num,
                   0, 0, NULL, rTU);
        RL_AddPoly(PT_FAN, polyType, 3 + divs[0].num, rvertices,
                   NULL, rtcShiny, rTU[TU_INTER].tex? rtcPrimary : NULL,
                   rcolors, 0, 0, NULL, rTU);

        return;
    }

    RL_AddPoly(PT_TRIANGLE_STRIP, polyType, numVertices, rvertices, NULL,
               rtcShiny, rTU[TU_INTER].tex? rtcPrimary : NULL,
               rcolors, 0, 0, NULL, rTU);
}

static void renderWorldSeg(rendseg_t* rseg, uint numVertices, rvertex_t* rvertices,
                           rcolor_t* rcolors, const int colorIndices[NUM_COLOR_INDICES],
                           rtexcoord_t* rcoords, const int coordIndices[NUM_COORD_INDICES],
                           DGLuint modTex, float modColor[3],
                           const rtexmapunit_t* rTU, const rtexmapunit_t* rTUs,
                           const rtexmapunit_t* radioTU,
                           const rtexmapunit_t* radioTU2,
                           const rtexmapunit_t* radioTU3,
                           const rtexmapunit_t* radioTU4,
                           uint numLights)
{
    rendpolytype_t rptype = RendSeg_SkyMasked(rseg)? RPT_SKY_MASK : RPT_NORMAL;

    lightPolygon(rcolors + colorIndices[COLOR_PRIMARY], 4, rvertices,
                 rseg->sectorLightLevel, rseg->sectorLightColor,
                 rseg->biasSurface, rseg->surfaceLightLevelDeltaL, rseg->surfaceLightLevelDeltaR,
                 rseg->surfaceColorTint, rseg->surfaceColorTint2, 1.0f,
                 RendSeg_SkyMasked(rseg), (rseg->flags & RSF_GLOW) != 0,
                 rseg->normal, NULL, rseg->from, rseg->to);

    if(coordIndices[COORD_SHINY] != -1)
    {
        lightShinyPolygon(rcolors + colorIndices[COLOR_SHINY], rcolors, numVertices,
                          rseg->materials.snapshotA.shinyMinColor,
                          rTUs[TU_PRIMARY].blend);
    }

    // Do we need to do any divisions?
    if(rseg->divs && (rseg->divs[0].num || rseg->divs[1].num))
    {
        float bL, tL, bR, tR;
        uint i;

        /**
         * Need to swap indices around into fans set the position
         * of the division vertices, interpolate texcoords and
         * color.
         */

        bL = rvertices[0].pos[VZ];
        tL = rvertices[1].pos[VZ];
        bR = rvertices[2].pos[VZ];
        tR = rvertices[3].pos[VZ];

        divideVertices(rvertices, rseg->divs);

        for(i = 0; i < NUM_COORD_INDICES; ++i)
            if(coordIndices[i] != -1)
                divideCoords(rcoords + coordIndices[i], rseg->divs, bL, tL, bR, tR);

        for(i = 0; i < NUM_COLOR_INDICES; ++i)
            if(colorIndices[i] != -1)
                divideColors(rcolors + colorIndices[i], rseg->divs, bL, tL, bR, tR);
    }

    writePolygon(rptype, rseg->divs, numVertices,
                 rvertices, rcolors,
                 rcoords + coordIndices[COORD_PRIMARY],
                 rTU[TU_INTER].tex ? rcoords + coordIndices[COORD_PRIMARY]: NULL,
                 coordIndices[COORD_MODULATE] != -1 ? rcoords + coordIndices[COORD_MODULATE] : NULL,
                 modTex, modColor, rTU,
                 numLights);

    if(coordIndices[COORD_SHINY] != -1)
        writeShinyPolygon(RPT_SHINY, rseg->divs, numVertices, rvertices,
                          rcolors + colorIndices[COLOR_SHINY],
                          rcoords + coordIndices[COORD_PRIMARY],
                          rcoords + coordIndices[COORD_SHINY],
                          rTUs);

    if(rendFakeRadio != 2)
    {
        if(colorIndices[COLOR_RADIO_TOP] != -1)
            writePolygon(RPT_SHADOW, rseg->divs, numVertices, rvertices,
                         rcolors + colorIndices[COLOR_RADIO_TOP],
                         rcoords + coordIndices[COORD_PRIMARY],
                         NULL, NULL, 0, NULL, radioTU, 0);

        if(colorIndices[COLOR_RADIO_BOTTOM] != -1)
            writePolygon(RPT_SHADOW, rseg->divs, numVertices, rvertices,
                         rcolors + colorIndices[COLOR_RADIO_BOTTOM],
                         rcoords + coordIndices[COORD_PRIMARY],
                         NULL, NULL, 0, NULL, radioTU2, 0);

        if(colorIndices[COLOR_RADIO_LEFT] != -1)
            writePolygon(RPT_SHADOW, rseg->divs, numVertices, rvertices,
                         rcolors + colorIndices[COLOR_RADIO_LEFT],
                         rcoords + coordIndices[COORD_PRIMARY],
                         NULL, NULL, 0, NULL, radioTU3, 0);

        if(colorIndices[COLOR_RADIO_RIGHT] != -1)
            writePolygon(RPT_SHADOW, rseg->divs, numVertices, rvertices,
                         rcolors + colorIndices[COLOR_RADIO_RIGHT],
                         rcoords + coordIndices[COORD_PRIMARY],
                         NULL, NULL, 0, NULL, radioTU4, 0);
    }
}

static void renderWorldPlane(rendplane_t* rplane, uint numVertices, rvertex_t* rvertices,
                             rcolor_t* rcolors, int colorIndices[NUM_COLOR_INDICES],
                             rtexcoord_t* rcoords, int coordIndices[NUM_COORD_INDICES],
                             DGLuint modTex, float modColor[3], const rtexmapunit_t* rTU, const rtexmapunit_t* rTUs,
                             uint numLights)
{
    vec3_t pointInPlane;

    V3_Set(pointInPlane, rplane->midPoint[0], rplane->midPoint[1], rplane->height);

    lightPolygon(rcolors + colorIndices[COLOR_PRIMARY], numVertices, rvertices,
                 rplane->sectorLightLevel, rplane->sectorLightColor,
                 rplane->biasSurface, rplane->surfaceLightLevelDelta, rplane->surfaceLightLevelDelta,
                 rplane->surfaceColorTint, NULL, 1.0f,
                 RendPlane_SkyMasked(rplane), (rplane->flags & RPF_GLOW) != 0,
                 rplane->normal, pointInPlane, NULL, NULL);

    if(coordIndices[COORD_SHINY] != -1)
    {
        lightShinyPolygon(rcolors + colorIndices[COLOR_SHINY],
                          rcolors + colorIndices[COLOR_PRIMARY], numVertices,
                          rplane->materials.snapshotA.shinyMinColor,
                          rTUs[TU_PRIMARY].blend);
    }

    RL_AddPoly(PT_FAN, RendPlane_SkyMasked(rplane)? RPT_SKY_MASK : RPT_NORMAL,
               numVertices, rvertices,
               rcoords + coordIndices[COORD_MODULATE],
               rcoords + coordIndices[COORD_PRIMARY],
               rcoords + coordIndices[COORD_PRIMARY],
               rcolors + colorIndices[COLOR_PRIMARY],
               numLights, modTex, modColor, rTU);

    if(coordIndices[COORD_SHINY] != -1)
        RL_AddPoly(PT_FAN, RPT_SHINY, numVertices, rvertices, NULL,
                   rcoords + coordIndices[COORD_SHINY],
                   rTUs[TU_INTER].tex? rcoords + coordIndices[COORD_PRIMARY] : NULL,
                   rcolors + colorIndices[COLOR_SHINY],
                   0, 0, NULL, rTUs);
}

void Rend_RenderSeg(rendseg_t* rendSeg)
{
    assert(rendSeg);
    {
    int rcoordIndices[NUM_COORD_INDICES], rcolorIndices[NUM_COLOR_INDICES];
    uint numVertices, numLights = 0;
    rvertex_t* rvertices = NULL;
    rcolor_t* rcolors = NULL;
    rtexcoord_t* rcoords = NULL;
    float modTexTC[2][2], modColor[3];
    rtexmapunit_t rTU[NUM_TEXMAP_UNITS], rTUs[NUM_TEXMAP_UNITS], radioTU[NUM_TEXMAP_UNITS], radioTU2[NUM_TEXMAP_UNITS], radioTU3[NUM_TEXMAP_UNITS], radioTU4[NUM_TEXMAP_UNITS];
    uint dynlistID = RendSeg_DynlistID(rendSeg);
    DGLuint modTex = 0;

    /**
     * If multi-texturing is enabled; take the first dynlight texture for
     * use in the modulation pass.
     */
    if(RL_IsMTexLights())
    {
        modTex = pickModulationTexture(dynlistID, modTexTC, modColor);
        if(modTex)
            numLights = 1;
    }

    if(RendSeg_MustUseVisSprite(rendSeg))
    {
        rvertices = R_VerticesFromRendSeg(rendSeg, &numVertices);

        R_TexmapUnitsFromRendSeg(rendSeg, rTU, rTUs, radioTU, radioTU2, radioTU3, radioTU4);

        renderWorldSegAsVisSprite(rendSeg, rvertices, rTU);
    }
    else
    {
        numVertices = buildGeometryFromRendSeg(rendSeg, modTex, modTexTC, modColor,
            &rvertices, &rcolors, rcolorIndices, &rcoords, rcoordIndices,
            rTU, rTUs, radioTU, radioTU2, radioTU3, radioTU4);

        if(dynlistID && IS_MUL && !(averageLuma(rcolors, 4) > 0.98f))
            writeDynlights(dynlistID, rvertices, 4, numVertices,
                           true, rendSeg->divs, rendSeg->texQuadTopLeft, rendSeg->texQuadBottomRight,
                           &numLights);

        renderWorldSeg(rendSeg, numVertices, rvertices, rcolors, rcolorIndices,
                       rcoords, rcoordIndices, modTex, modColor,
                       rTU, rTUs, radioTU, radioTU2, radioTU3, radioTU4,
                       numLights);
    }

    if(rvertices)
        R_FreeRendVertices(rvertices);
    if(rcoords)
        R_FreeRendTexCoords(rcoords);
    if(rcolors)
        R_FreeRendColors(rcolors);
    }
}

void Rend_RenderPlane(rendplane_t* rendPlane)
{
    assert(rendPlane);
    {
    int rcoordIndices[NUM_COORD_INDICES], rcolorIndices[NUM_COLOR_INDICES];
    uint numVertices, numLights = 0;
    rvertex_t* rvertices = NULL;
    rcolor_t* rcolors = NULL;
    rtexcoord_t* rcoords = NULL;
    float modTexTC[2][2], modColor[3];
    rtexmapunit_t rTU[NUM_TEXMAP_UNITS], rTUs[NUM_TEXMAP_UNITS];
    uint dynlistID = RendPlane_DynlistID(rendPlane);
    DGLuint modTex = 0;

    /**
     * If multi-texturing is enabled; take the first dynlight texture for
     * use in the modulation pass.
     */
    if(RL_IsMTexLights())
    {
        modTex = pickModulationTexture(dynlistID, modTexTC, modColor);
        if(modTex)
            numLights = 1;
    }

    numVertices = buildGeometryFromRendPlane(rendPlane, modTex, modTexTC, modColor,
        &rvertices, &rcolors, rcolorIndices, &rcoords, rcoordIndices,
        rTU, rTUs);

    if(dynlistID && IS_MUL && !(averageLuma(rcolors, numVertices) > 0.98f))
        writeDynlights(dynlistID, rvertices, numVertices, numVertices,
                       false, NULL, rendPlane->texQuadTopLeft, rendPlane->texQuadBottomRight,
                       &numLights);

    renderWorldPlane(rendPlane, numVertices, rvertices,
                     rcolors, rcolorIndices, rcoords, rcoordIndices,
                     modTex, modColor, rTU, rTUs, numLights);

    if(rvertices)
        R_FreeRendVertices(rvertices);
    if(rcoords)
        R_FreeRendTexCoords(rcoords);
    if(rcolors)
        R_FreeRendColors(rcolors);
    }
}

static void markSegsFacingFront(subsector_t* subsector)
{
    hedge_t* hEdge = subsector->face->hEdge;

    do
    {
        seg_t* seg = (seg_t*) hEdge->data;

        if(seg->sideDef) // Not minisegs.
        {
            const vertex_t* from = hEdge->HE_v1;
            const vertex_t* to = hEdge->HE_v2;

            seg->frameFlags &= ~SEGINF_BACKSECSKYFIX;

            // Which way should it be facing?
            if(!(R_FacingViewerDot(from->pos[VX], from->pos[VY],
                                   to->pos  [VX], to->pos  [VY]) < 0))
                seg->frameFlags |= SEGINF_FACINGFRONT;
            else
                seg->frameFlags &= ~SEGINF_FACINGFRONT;

            markSegSectionsPVisible(hEdge);
        }
    } while((hEdge = hEdge->next) != subsector->face->hEdge);
}

static void prepareSkyMaskPoly(rvertex_t verts[4], rtexcoord_t coords[4],
                               rtexmapunit_t rTU[NUM_TEXMAP_UNITS],
                               float wallLength, material_t* mat)
{
    material_snapshot_t ms;
    float texOffset[2] = { 0, 0 };
    vec3_t texOrigin[2];

    // In devRendSkyMode mode we render all polys destined for the skymask as
    // regular world polys (with a few obvious properties).

    Materials_Prepare(mat, 0, NULL, &ms);

    rTU[TU_PRIMARY].tex = ms.units[MTU_PRIMARY].texInst->id;
    rTU[TU_PRIMARY].magMode = ms.units[MTU_PRIMARY].magMode;
    rTU[TU_PRIMARY].scale[0] = ms.units[MTU_PRIMARY].scale[0];
    rTU[TU_PRIMARY].scale[1] = ms.units[MTU_PRIMARY].scale[1];
    rTU[TU_PRIMARY].offset[0] = ms.units[MTU_PRIMARY].offset[0];
    rTU[TU_PRIMARY].offset[1] = ms.units[MTU_PRIMARY].offset[1];
    rTU[TU_PRIMARY].blend = ms.units[MTU_PRIMARY].alpha;
    rTU[TU_PRIMARY].blendMode = ms.units[MTU_PRIMARY].blendMode;

    // Top left.
    V3_Set(texOrigin[0], verts[1].pos[VX], verts[1].pos[VY], verts[1].pos[VZ]);

    // Bottom right.
    V3_Set(texOrigin[1], verts[2].pos[VX], verts[2].pos[VY], verts[2].pos[VZ]);

    quadTexCoords(coords, verts, wallLength, 1, 1, texOrigin[0], texOffset);
}

static void Rend_SubsectorSkyFixes(subsector_t* subsector)
{
    float ffloor, fceil, bfloor, bceil, bsh;
    rvertex_t rvertices[4];
    rcolor_t rcolors[4];
    rtexcoord_t rtexcoords[4];
    rtexmapunit_t rTU[NUM_TEXMAP_UNITS];
    float* vBL, *vBR, *vTL, *vTR;
    sector_t* frontsec, *backsec;
    hedge_t* hEdge;

    if(!subsector->face->hEdge)
        return;

    // Init the poly.
    memset(rTU, 0, sizeof(rTU));

    if(devRendSkyMode)
    {
        uint i;

        rtexcoords[0].st[0] = 0;
        rtexcoords[0].st[1] = 1;
        rtexcoords[1].st[0] = 0;
        rtexcoords[1].st[1] = 0;
        rtexcoords[2].st[0] = 1;
        rtexcoords[2].st[1] = 1;
        rtexcoords[3].st[0] = 1;
        rtexcoords[3].st[1] = 0;

        for(i = 0; i < 4; ++i)
            rcolors[i].rgba[CR] = rcolors[i].rgba[CG] =
                rcolors[i].rgba[CB] = rcolors[i].rgba[CA] = 1;
    }

    vBL = rvertices[0].pos;
    vBR = rvertices[2].pos;
    vTL = rvertices[1].pos;
    vTR = rvertices[3].pos;

    hEdge = subsector->face->hEdge;
    do
    {
        seg_t* seg = (seg_t*) hEdge->data, *backSeg = hEdge->twin->data ? ((seg_t*) hEdge->twin->data) : NULL;
        sidedef_t* side = seg->sideDef;

        if(!side)
            continue;

        // Let's first check which way this seg is facing.
        if(!(seg->frameFlags & SEGINF_FACINGFRONT))
            continue;

        backsec = HE_BACKSECTOR(hEdge);
        frontsec = HE_FRONTSECTOR(hEdge);

        if(backsec == frontsec &&
           !side->SW_topmaterial && !side->SW_bottommaterial &&
           !side->SW_middlematerial)
           continue; // Ugh... an obvious wall seg hack. Best take no chances...

        ffloor = frontsec->SP_floorvisheight;
        fceil = frontsec->SP_ceilvisheight;

        if(backsec)
        {
            bceil = backsec->SP_ceilvisheight;
            bfloor = backsec->SP_floorvisheight;
            bsh = bceil - bfloor;
        }
        else
            bsh = bceil = bfloor = 0;

        // Get the start and end vertices, left then right. Top and bottom.
        vBL[VX] = vTL[VX] = hEdge->HE_v1->pos[VX];
        vBL[VY] = vTL[VY] = hEdge->HE_v1->pos[VY];
        vBR[VX] = vTR[VX] = hEdge->HE_v2->pos[VX];
        vBR[VY] = vTR[VY] = hEdge->HE_v2->pos[VY];

        // Upper/lower normal skyfixes.
        if(!backsec || backsec != frontsec)
        {
            // Floor.
            if(IS_SKYSURFACE(&frontsec->SP_floorsurface) &&
               !(backsec && IS_SKYSURFACE(&backsec->SP_floorsurface)) &&
               ffloor > P_CurrentMap()->skyFixFloor)
            {
                vTL[VZ] = vTR[VZ] = ffloor;
                vBL[VZ] = vBR[VZ] = P_CurrentMap()->skyFixFloor;

                if(devRendSkyMode)
                    prepareSkyMaskPoly(rvertices, rtexcoords, rTU, seg->length,
                                       frontsec->SP_floormaterial);

                RL_AddPoly(PT_TRIANGLE_STRIP,
                           (devRendSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                           4, rvertices, NULL, rtexcoords, NULL,
                           rcolors, 0, 0, NULL, rTU);
            }

            // Ceiling.
            if(IS_SKYSURFACE(&frontsec->SP_ceilsurface) &&
               !(backsec && IS_SKYSURFACE(&backsec->SP_ceilsurface)) &&
               fceil < P_CurrentMap()->skyFixCeiling)
            {
                vTL[VZ] = vTR[VZ] = P_CurrentMap()->skyFixCeiling;
                vBL[VZ] = vBR[VZ] = fceil;

                if(devRendSkyMode)
                     prepareSkyMaskPoly(rvertices, rtexcoords, rTU, seg->length,
                                        frontsec->SP_ceilmaterial);

                RL_AddPoly(PT_TRIANGLE_STRIP,
                           (devRendSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                           4, rvertices, NULL, rtexcoords, NULL,
                           rcolors, 0, 0, NULL, rTU);
            }
        }

        // Upper/lower zero height backsec skyfixes.
        if(backsec && bsh <= 0)
        {
            // Floor.
            if(IS_SKYSURFACE(&frontsec->SP_floorsurface) &&
               IS_SKYSURFACE(&backsec->SP_floorsurface))
            {
                if(bfloor > P_CurrentMap()->skyFixFloor)
                {
                    vTL[VZ] = vTR[VZ] = bfloor;
                    vBL[VZ] = vBR[VZ] = P_CurrentMap()->skyFixFloor;

                    if(devRendSkyMode)
                        prepareSkyMaskPoly(rvertices, rtexcoords, rTU, seg->length,
                                           frontsec->SP_floormaterial);

                    RL_AddPoly(PT_TRIANGLE_STRIP,
                               (devRendSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                               4, rvertices, NULL, rtexcoords, NULL,
                               rcolors, 0, 0, NULL, rTU);
                }

                // Ensure we add a solid view seg.
                seg->frameFlags |= SEGINF_BACKSECSKYFIX;
            }

            // Ceiling.
            if(IS_SKYSURFACE(&frontsec->SP_ceilsurface) &&
               IS_SKYSURFACE(&backsec->SP_ceilsurface))
            {
                if(bceil < P_CurrentMap()->skyFixCeiling)
                {
                    vTL[VZ] = vTR[VZ] = P_CurrentMap()->skyFixCeiling;
                    vBL[VZ] = vBR[VZ] = bceil;

                    if(devRendSkyMode)
                        prepareSkyMaskPoly(rvertices, rtexcoords, rTU, seg->length,
                                           frontsec->SP_ceilmaterial);


                    RL_AddPoly(PT_TRIANGLE_STRIP,
                               (devRendSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                               4, rvertices, NULL, rtexcoords, NULL,
                               rcolors, 0, 0, NULL, rTU);
                }

                // Ensure we add a solid view seg.
                seg->frameFlags |= SEGINF_BACKSECSKYFIX;
            }
        }
    } while((hEdge = hEdge->next) != subsector->face->hEdge);
}

/**
 * Creates new occlusion planes from the subsector's sides.
 * Before testing, occlude subsector's backfaces. After testing occlude
 * the remaining faces, i.e. the forward facing segs. This is done before
 * rendering segs, so solid segments cut out all unnecessary oranges.
 */
static void occludeSubsector(const subsector_t* subsector, boolean forwardFacing)
{
    sector_t* front;
    hedge_t* hEdge;
    float fronth[2];

    if(devNoCulling || P_IsInVoid(viewPlayer))
        return;

    front = subsector->sector;

    fronth[0] = front->SP_floorheight;
    fronth[1] = front->SP_ceilheight;

    if((hEdge = subsector->face->hEdge))
    {
        do
        {
            seg_t* seg = (seg_t*) hEdge->data;

            // Occlusions can only happen where two sectors contact.
            if(seg->sideDef && !R_ConsiderOneSided(hEdge) &&
               (forwardFacing == ((seg->frameFlags & SEGINF_FACINGFRONT)? true : false)))
            {
                float backh[2];
                vertex_t* startv, *endv;
                sector_t* back = ((subsector_t*) hEdge->twin->face->data)->sector;

                backh[0] = back->SP_floorheight;
                backh[1] = back->SP_ceilheight;
                // Choose start and end vertices so that it's facing forward.
                if(forwardFacing)
                {
                    startv = hEdge->HE_v1;
                    endv   = hEdge->HE_v2;
                }
                else
                {
                    startv = hEdge->HE_v2;
                    endv   = hEdge->HE_v1;
                }

                // Do not create an occlusion for sky floors.
                if(!IS_SKYSURFACE(&back->SP_floorsurface) ||
                   !IS_SKYSURFACE(&front->SP_floorsurface))
                {
                    // Do the floors create an occlusion?
                    if((backh[0] > fronth[0] && vy <= backh[0]) ||
                       (backh[0] < fronth[0] && vy >= fronth[0]))
                    {
                        // Occlude down.
                        C_AddViewRelOcclusion(startv->pos[VX], startv->pos[VY],
                                              endv->pos[VX], endv->pos[VY],
                                              MAX_OF(fronth[0], backh[0]), false);
                    }
                }

                // Do not create an occlusion for sky ceilings.
                if(!IS_SKYSURFACE(&back->SP_ceilsurface) ||
                   !IS_SKYSURFACE(&front->SP_ceilsurface))
                {
                    // Do the ceilings create an occlusion?
                    if((backh[1] < fronth[1] && vy >= backh[1]) ||
                       (backh[1] > fronth[1] && vy <= fronth[1]))
                    {
                        // Occlude up.
                        C_AddViewRelOcclusion(startv->pos[VX], startv->pos[VY],
                                              endv->pos[VX], endv->pos[VY],
                                              MIN_OF(fronth[1], backh[1]), true);
                    }
                }
            }
        } while((hEdge = hEdge->next) != subsector->face->hEdge);
    }
}

static boolean isTwoSidedSolid(int solidSeg, const hedge_t* hEdge,
                               const plane_t* ffloor, const plane_t* fceil,
                               const plane_t* bfloor, const plane_t* bceil)
{
    boolean             solid = false;
    seg_t*              seg = ((seg_t*) hEdge->data);
    sidedef_t*          sideDef = seg->sideDef;
    subsector_t*        subSector = HE_FRONTSUBSECTOR(hEdge),
                       *backSubSector = HE_BACKSUBSECTOR(hEdge);

    // Can we make this a solid segment in the clipper?
    if(solidSeg)
        solid = true;
    else
    {
        if(LINE_SELFREF(sideDef->lineDef))
        {   // @fixme Under certain situations, it COULD be solid.
            solid = false;
        }
        else
        {
            if(!solidSeg) // We'll have to determine whether we can...
            {
                if(backSubSector->sector == subSector->sector)
                {
                    // An obvious hack, what to do though??
                }
                else if((bceil->visHeight <= ffloor->visHeight &&
                            ((sideDef->SW_topmaterial /* && !(sideDef->flags & SDF_MIDTEXUPPER)*/) ||
                             (sideDef->SW_middlematerial))) ||
                        (bfloor->visHeight >= fceil->visHeight &&
                            (sideDef->SW_bottommaterial || sideDef->SW_middlematerial)))
                {
                    // A closed gap.
                    solidSeg = true;
                }
                else if((seg->frameFlags & SEGINF_BACKSECSKYFIX) ||
                        (!(bceil->visHeight - bfloor->visHeight > 0) && bfloor->visHeight > ffloor->visHeight && bceil->visHeight < fceil->visHeight &&
                        (sideDef->SW_topmaterial /*&& !(sideDef->flags & SDF_MIDTEXUPPER)*/) &&
                        (sideDef->SW_bottommaterial)))
                {
                    // A zero height back segment
                    solidSeg = true;
                }
            }

            if(solidSeg)
                solid = true;
        }
    }

    return solid;
}

static boolean isSidedefSectionPotentiallyVisible(sidedef_t* sideDef, segsection_t section)
{
    return (sideDef->SW_surface(section).inFlags & SUIF_PVIS) != 0;
}

static void getRendSegsForHEdge(hedge_t* hEdge, rendseg_t* temp, rendseg_t** rsegs)
{
    plane_t* ffloor, *fceil, *bfloor, *bceil;
    float bottom, top, texOffset[2], texScale[2];

    memset(rsegs, 0, sizeof(rendseg_t*) * 3);

    R_PickPlanesForSegExtrusion(hEdge, false, &ffloor, &fceil, &bfloor, &bceil);

    if(isSidedefSectionPotentiallyVisible(HE_FRONTSIDEDEF(hEdge), SEG_BOTTOM))
        if(R_FindBottomTopOfHEdgeSection(hEdge, SEG_BOTTOM, ffloor, fceil, bfloor, bceil,
                         &bottom, &top, texOffset, texScale))
        {
            vec2_t from, to;

            V2_Set(from, hEdge->HE_v1->pos[VX], hEdge->HE_v1->pos[VY]);
            V2_Set(to,   hEdge->HE_v2->pos[VX], hEdge->HE_v2->pos[VY]);

            rsegs[SEG_BOTTOM] = RendSeg_staticConstructFromHEdgeSection(&temp[SEG_BOTTOM], hEdge, SEG_BOTTOM,
                from, to, bottom, top, texOffset, texScale);
        }

    if(isSidedefSectionPotentiallyVisible(HE_FRONTSIDEDEF(hEdge), SEG_TOP))
        if(R_FindBottomTopOfHEdgeSection(hEdge, SEG_TOP, ffloor, fceil, bfloor, bceil,
                         &bottom, &top, texOffset, texScale))
        {
            vec2_t from, to;

            V2_Set(from, hEdge->HE_v1->pos[VX], hEdge->HE_v1->pos[VY]);
            V2_Set(to,   hEdge->HE_v2->pos[VX], hEdge->HE_v2->pos[VY]);

            rsegs[SEG_TOP] = RendSeg_staticConstructFromHEdgeSection(&temp[SEG_TOP], hEdge, SEG_TOP,
                from, to, bottom, top, texOffset, texScale);
        }

    if(isSidedefSectionPotentiallyVisible(HE_FRONTSIDEDEF(hEdge), SEG_MIDDLE))
    {
        /**
         * Special case:
         * Middle sections of "Self-referencing" linedefs must select the sectors
         * linked to seg's front sidedef rather than those linked to the half-edge.
         */
        if(R_UseSectorsFromFrontSideDef(hEdge, SEG_MIDDLE))
            R_PickPlanesForSegExtrusion(hEdge, true, &ffloor, &fceil, &bfloor, &bceil);

        if(R_FindBottomTopOfHEdgeSection(hEdge, SEG_MIDDLE, ffloor, fceil, bfloor, bceil,
                         &bottom, &top, texOffset, texScale))
        {
            vec2_t from, to;

            V2_Set(from, hEdge->HE_v1->pos[VX], hEdge->HE_v1->pos[VY]);
            V2_Set(to,   hEdge->HE_v2->pos[VX], hEdge->HE_v2->pos[VY]);

            rsegs[SEG_MIDDLE] = RendSeg_staticConstructFromHEdgeSection(&temp[SEG_MIDDLE], hEdge, SEG_MIDDLE,
                from, to, bottom, top, texOffset, texScale);
        }
    }
}

static void getRendSegForPolyobjSeg(polyobj_t* po, uint segId, rendseg_t* temp,
                                    rendseg_t** rseg)
{
    seg_t* seg = &po->segs[segId];
    sector_t* frontSec = po->subsector->sector;
    float bottom = frontSec->SP_floorvisheight;
    float top = frontSec->SP_ceilvisheight;
    float from[2], to[2];

    from[VX] = seg->sideDef->lineDef->L_v1->pos[VX];
    from[VY] = seg->sideDef->lineDef->L_v1->pos[VY];

    to[VX] = seg->sideDef->lineDef->L_v2->pos[VX];
    to[VY] = seg->sideDef->lineDef->L_v2->pos[VY];

    *rseg = RendSeg_staticConstructFromPolyobjSideDef(temp, seg->sideDef,
        from, to, bottom, top, po->subsector, seg);
}

static void getRendPlaneForFacePlane(face_t* face, uint planeId, rendplane_t* temp,
                                     rendplane_t** rplane)
{
    subsector_t* subsector = (subsector_t*) face->data;
    plane_t* plane = subsector->sector->planes[planeId];
    surface_t* suf = &plane->surface;
    float height, texOffset[2], texScale[2], texTL[3], texBR[3];

    // Determine plane height.
    if(!IS_SKYSURFACE(suf))
    {
        height = plane->visHeight;
    }
    else
    {
        if(planeId == PLN_FLOOR)
            height = P_CurrentMap()->skyFixFloor;
        else if(planeId == PLN_CEILING)
            height = P_CurrentMap()->skyFixCeiling;
        else
            height = plane->visHeight;
    }

    V2_Copy(texOffset, suf->visOffset);

    // Add the Y offset to orient the Y flipped texture.
    if(planeId == PLN_CEILING)
        texOffset[VY] -= subsector->bBox[1][VY] - subsector->bBox[0][VY];

    // Add the additional offset to align with the worldwide grid.
    texOffset[VX] += subsector->worldGridOffset[VX];
    texOffset[VY] += subsector->worldGridOffset[VY];

    texScale[VX] = ((suf->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
    texScale[VY] = ((suf->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

    // Set the texture origin, Y is flipped for the ceiling.
    V3_Set(texTL, subsector->bBox[0][VX],
           subsector->bBox[planeId == PLN_FLOOR? 1 : 0][VY], height);
    V3_Set(texBR, subsector->bBox[1][VX],
           subsector->bBox[planeId == PLN_FLOOR? 0 : 1][VY], height);

    *rplane = RendPlane_staticConstructFromFacePlane(temp, face, planeId,
                texTL, texBR, height, texOffset, texScale);
}

static void Rend_RenderSubsector(face_t* face)
{
    float sceil, sfloor;
    subsector_t* subsector;
    sector_t* sect;
    uint i;

    if(!(subsector = (subsector_t*) face->data))
        return;
    if(!subsector->sector)
        return; // An orphan subsector.

    R_PickSubsectorFanBase(subsector);
    R_CreateBiasSurfacesInSubsector(subsector);

    sect = subsector->sector;
    sceil = sect->SP_ceilvisheight;
    sfloor = sect->SP_floorvisheight;

    if(sceil - sfloor <= 0 || subsector->hEdgeCount < 3)
    {
        // Skip this, it has no volume.
        // Neighbors handle adding the solid clipper segments.
        return;
    }

    if(!firstsubsector)
    {
        if(!C_CheckSubsector(subsector))
            return; // This isn't visible.
    }
    else
    {
        firstsubsector = false;
    }

    // Mark the sector visible for this frame.
    sect->frameFlags |= SIF_VISIBLE;

    markSegsFacingFront(subsector);

    Subsector_SpreadObjs(subsector);

    Rend_RadioSubsectorEdges(subsector);

    occludeSubsector(subsector, false);
    LO_ClipInSubsector(subsector);
    occludeSubsector(subsector, true);

    if(subsector->polyObj)
    {
        // Polyobjs don't obstruct, do clip lights with another algorithm.
        LO_ClipInSubsectorBySight(subsector);
    }

    /**
     * Sprites for this subsector have to be drawn.
     * @note
     * Must be done BEFORE the segments of this subsector are added to the
     * clipper. Otherwise the sprites would get clipped by them, and that
     * wouldn't be right.
     * Must be done AFTER the lumobjs have been clipped as this affects the
     * projection of flares.
     */
    R_ProjectVisSprites(subsector);

    // Draw the various skyfixes for all front facing segs in this subsector
    // (includes polyobject segs).
    if(subsector->sector->planeCount > 0)
    {
        boolean doSkyFixes;

        doSkyFixes = false;
        i = 0;
        do
        {
            if(IS_SKYSURFACE(&subsector->sector->SP_planesurface(i)))
                doSkyFixes = true;
            else
                i++;
        } while(!doSkyFixes && i < subsector->sector->planeCount);

        if(doSkyFixes)
            Rend_SubsectorSkyFixes(subsector);
    }

    /**
     * Render "walls" by extruding seg sections within subsector.
     */
    {
    boolean canAddOcclusionRanges = !P_IsInVoid(viewPlayer);

    if(subsector->face->hEdge)
    {
        hedge_t* hEdge = subsector->face->hEdge;

        do
        {
            seg_t* seg = (seg_t*) hEdge->data;
            boolean solid = false;

            if(seg->sideDef && // Exclude "minisegs"
               (seg->frameFlags & SEGINF_FACINGFRONT))
            {
                rendseg_t temp[3], *rendSegs[3];

                memset(temp, 0, sizeof(temp));

                getRendSegsForHEdge(hEdge, temp, rendSegs);

                /**
                 * Draw sections.
                 */
                for(i = 0; i < 3; ++i)
                {
                    if(rendSegs[i])
                    {
                        Rend_RenderSeg(rendSegs[i]);
                        R_MarkLineDefAsDrawnForViewer(seg->sideDef->lineDef, viewPlayer - ddPlayers);
                    }
                }

                if(!rendSegs[SEG_MIDDLE] || RendSeg_MustUseVisSprite(rendSegs[SEG_MIDDLE]))
                    solid = false;
                else
                    solid = true;

                if(!R_ConsiderOneSided(hEdge))
                {
                    plane_t* ffloor, *fceil, *bfloor, *bceil;

                    R_PickPlanesForSegExtrusion(hEdge, false, &ffloor, &fceil, &bfloor, &bceil);

                    if(solid) // An opaque middle texture was drawn.
                    {
                        float openBottom = MAX_OF(bfloor->visHeight, ffloor->visHeight);
                        float openTop = MIN_OF(bceil->visHeight, fceil->visHeight);

                        // If the middle seg fills the opening, we can clip the range
                        // defined by the half-edge vertices.
                        if(!(rendSegs[SEG_MIDDLE]->top >= openTop &&
                             rendSegs[SEG_MIDDLE]->bottom <= openBottom))
                             solid = false;
                    }

                    /**
                     * @todo I moved this nasty block of logic into a subroutine in
                     * order to improve the readability and clarity of design in this.
                     * Clean up!
                     */
                    solid = isTwoSidedSolid(solid, hEdge, ffloor, fceil, bfloor, bceil);
                }
                else
                {
                    solid = true;
                }
            }

            if(solid && canAddOcclusionRanges)
            {
                C_AddViewRelSeg(hEdge->HE_v1->pos[VX], hEdge->HE_v1->pos[VY],
                                hEdge->HE_v2->pos[VX], hEdge->HE_v2->pos[VY]);
            }
        } while((hEdge = hEdge->next) != subsector->face->hEdge);
    }

    // Is there a polyobj on board?
    if(subsector->polyObj)
    {
        polyobj_t* po = subsector->polyObj;

        for(i = 0; i < po->numSegs; ++i)
        {
            seg_t* seg = &po->segs[i];
            rendseg_t temp, *rendSeg;

            getRendSegForPolyobjSeg(po, i, &temp, &rendSeg);

            if(rendSeg)
            {
                // Front facing?
                if(R_FacingViewerDot(rendSeg->from[VX], rendSeg->from[VY],
                                     rendSeg->to  [VX], rendSeg->to  [VY]) < 0)
                    continue;

                Rend_RenderSeg(rendSeg);
                R_MarkLineDefAsDrawnForViewer(seg->sideDef->lineDef, viewPlayer - ddPlayers);

                if(canAddOcclusionRanges)
                    C_AddViewRelSeg(seg->sideDef->lineDef->L_v1->pos[VX], seg->sideDef->lineDef->L_v1->pos[VY],
                                    seg->sideDef->lineDef->L_v2->pos[VX], seg->sideDef->lineDef->L_v2->pos[VY]);
            }
        }
    }
    }

    // Render all planes of this sector.
    for(i = 0; i < sect->planeCount; ++i)
    {
        rendplane_t temp, *rplane;

        memset(&temp, 0, sizeof(temp));
        getRendPlaneForFacePlane(face, i, &temp, &rplane);

        if(rplane)
        {
            vec3_t vec;

            // Don't bother with planes facing away from the camera.
            V3_Set(vec, vx - rplane->midPoint[VX], vz - rplane->midPoint[VY], vy - rplane->height);
            if(!(V3_DotProduct(vec, rplane->normal) < 0))
                Rend_RenderPlane(rplane);
        }
    }

#if 0
    if(devRendSkyMode == 2)
    {
        /**
         * In devSky mode 2, we draw additional geometry, showing the
         * real "physical" height of any sky masked planes that are
         * drawn at a different height due to the skyFix.
         */
        if(sect->SP_floorvisheight > P_CurrentMap()->skyFixFloor &&
           IS_SKYSURFACE(&sect->SP_floorsurface))
        {
            vec3_t normal;
            const plane_t* plane = sect->SP_plane(PLN_FLOOR);
            const surface_t* suf = &plane->surface;

            /**
             * Flip the plane normal according to the z positon of the
             * viewer relative to the plane height so that the plane is
             * always visible.
             */
            V3_Copy(normal, plane->PS_normal);
            if(vy < plane->visHeight)
                normal[VZ] *= -1;

            Rend_RenderPlane(face, PLN_CEILING+1, plane->visHeight, /*normal,
                             Materials_ToMaterial2(MN_SYSTEM, DDT_GRAY), NULL, 0,
                             suf->inFlags, suf->rgba,
                             BM_NORMAL,*/ NULL, NULL/*, false,
                             (vy > plane->visHeight? true : false),
                             false, NULL, 2*/);
        }

        if(sect->SP_ceilvisheight < P_CurrentMap()->skyFixCeiling &&
           IS_SKYSURFACE(&sect->SP_ceilsurface))
        {
            vec3_t normal;
            const plane_t* plane = sect->SP_plane(PLN_CEILING);
            const surface_t* suf = &plane->surface;

            V3_Copy(normal, plane->PS_normal);
            if(vy > plane->visHeight)
                normal[VZ] *= -1;

            Rend_RenderPlane(face, PLN_CEILING+1, plane->visHeight, /*normal,
                             Materials_ToMaterial2(MN_SYSTEM, DDT_GRAY), NULL, 0,
                             suf->inFlags, suf->rgba,
                             BM_NORMAL,*/ NULL, NULL/*, false,
                             (vy < plane->visHeight? true : false),
                             false, NULL, 2*/);
        }
    }
#endif
}

static void Rend_RenderNode(map_t* map, binarytree_t* tree)
{
    // If the clipper is full we're pretty much done. This means no geometry
    // will be visible in the distance because every direction has already been
    // fully covered by geometry.
    if(C_IsFull())
        return;

    if(BinaryTree_IsLeaf(tree))
    {
        // We've arrived at a face, render the linked subsector.
        Rend_RenderSubsector((face_t*) BinaryTree_GetData(tree));
    }
    else
    {   // Descend deeper into the nodes.
        node_t* node = (node_t*) BinaryTree_GetData(tree);
        byte side = R_PointOnSide(viewX, viewY, &node->partition);

        Rend_RenderNode(map, BinaryTree_GetChild(tree, side)); // Recursively render front space.
        Rend_RenderNode(map, BinaryTree_GetChild(tree, side ^ 1)); // ...and back space.
    }
}

void Rend_RenderMap(map_t* map)
{
    binangle_t viewside;
    boolean doLums =
        (useDynlights || haloMode || spriteLight || useDecorations);

    if(!map)
        return;

    // Set to true if dynlights are inited for this frame.
    loInited = false;

    GL_SetMultisample(true);

    // Setup the modelview matrix.
    Rend_ModelViewMatrix(true);

    if(!freezeRLs)
    {
        // Prepare for rendering.
        RL_ClearLists(); // Clear the lists for new quads.
        C_ClearRanges(); // Clear the clipper.
        LO_BeginFrame();

        // Make vissprites of all the visible decorations.
        Rend_ProjectDecorations(map);

        // Recycle the vlight lists. Currently done here as the lists are
        // not shared by all viewports.
        VL_InitForNewFrame();

        if(doLums)
        {
            /**
             * Clear the projected dynlights. This is done here as the
             * projections are sensitive to distance from the viewer
             * (e.g. some may fade out when far away).
             */
            DL_DestroyDynlights(map->_dlights.linkList);
            DL_ClearDynlists();
        }

        // Add the backside clipping range (if vpitch allows).
        if(vpitch <= 90 - yfov / 2 && vpitch >= -90 + yfov / 2)
        {
            float   a = fabs(vpitch) / (90 - yfov / 2);
            binangle_t startAngle =
                (binangle_t) (BANG_45 * fieldOfView / 90) * (1 + a);
            binangle_t angLen = BANG_180 - startAngle;

            viewside = (viewAngle >> (32 - BAMS_BITS)) + startAngle;
            C_SafeAddRange(viewside, viewside + angLen);
            C_SafeAddRange(viewside + angLen, viewside + 2 * angLen);
        }

        // The viewside line for the depth cue.
        {
        const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
        viewsidex = -viewData->viewSin;
        viewsidey = viewData->viewCos;
        }

        // We don't want subsector clipchecking for the first subsector.
        firstsubsector = true;
        Rend_RenderNode(map, map->_rootNode);

        Rend_RenderShadows(map);
    }

    RL_RenderAllLists();

    // Draw masked walls, sprites, particles and models.
    Rend_DrawMasked();

    // Draw various debugging displays:
    Rend_RenderNormals(map); // World surface normals.
    LO_DrawLumobjs(); // Lumobjs.
    Rend_RenderBoundingBoxes(map); // Mobj bounding boxes.
    Rend_Vertexes(map); // World vertex positions/indices.
    Rend_RenderGenerators(map); // Particle generator origins.

    if(!freezeRLs)
    {
        // Draw the Source Bias Editor's draw.
        SBE_DrawCursor(map);
    }

    GL_SetMultisample(false);
}

static void drawNormal(vec3_t origin, vec3_t normal, float scalar)
{
    float               black[4] = { 0, 0, 0, 0 };
    float               red[4] = { 1, 0, 0, 1 };

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslatef(origin[VX], origin[VZ], origin[VY]);

    glBegin(GL_LINES);
    {
        glColor4fv(black);
        glVertex3f(scalar * normal[VX],
                     scalar * normal[VZ],
                     scalar * normal[VY]);
        glColor4fv(red);
        glVertex3f(0, 0, 0);

    }
    glEnd();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

/**
 * Draw the surface normals, primarily for debug.
 */
void Rend_RenderNormals(map_t* map)
{
#define NORM_TAIL_LENGTH    (20)

    uint i;

    if(!devSurfaceNormals)
        return;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    for(i = 0; i < map->numSegs; ++i)
    {
        seg_t* seg = map->segs[i];
        hedge_t* hEdge = seg->hEdge;
        seg_t* backSeg = hEdge->twin ? ((seg_t*) hEdge->twin->data) : NULL;
        sidedef_t* side = seg->sideDef;
        surface_t* suf;
        vec3_t origin;
        float x, y, bottom, top;
        float scale = NORM_TAIL_LENGTH;

        if(!seg->sideDef || !((subsector_t*) hEdge->face->data)->sector)
            continue;

        x = hEdge->HE_v1->pos[VX] + (hEdge->HE_v2->pos[VX] - hEdge->HE_v1->pos[VX]) / 2;
        y = hEdge->HE_v1->pos[VY] + (hEdge->HE_v2->pos[VY] - hEdge->HE_v1->pos[VY]) / 2;

        if(!backSeg)
        {
            bottom = side->sector->SP_floorvisheight;
            top = side->sector->SP_ceilvisheight;
            suf = &side->SW_middlesurface;

            V3_Set(origin, x, y, bottom + (top - bottom) / 2);
            drawNormal(origin, suf->normal, scale);
        }
        else
        {
            sector_t* frontSec = ((subsector_t*) hEdge->face->data)->sector;
            sector_t* backSec = ((subsector_t*) hEdge->twin->face->data)->sector;

            if(side->SW_middlesurface.material)
            {
                top = frontSec->SP_ceilvisheight;
                bottom = frontSec->SP_floorvisheight;
                suf = &side->SW_middlesurface;

                V3_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawNormal(origin, suf->normal, scale);
            }

            if(backSec->SP_ceilvisheight <
               frontSec->SP_ceilvisheight &&
               !(IS_SKYSURFACE(&frontSec->SP_ceilsurface) &&
                 IS_SKYSURFACE(&backSec->SP_ceilsurface)))
            {
                bottom = backSec->SP_ceilvisheight;
                top = frontSec->SP_ceilvisheight;
                suf = &side->SW_topsurface;

                V3_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawNormal(origin, suf->normal, scale);
            }

            if(backSec->SP_floorvisheight >
               frontSec->SP_floorvisheight &&
               !(IS_SKYSURFACE(&frontSec->SP_floorsurface) &&
                 IS_SKYSURFACE(&backSec->SP_floorsurface)))
            {
                bottom = frontSec->SP_floorvisheight;
                top = backSec->SP_floorvisheight;
                suf = &side->SW_bottomsurface;

                V3_Set(origin, x, y, bottom + (top - bottom) / 2);
                drawNormal(origin, suf->normal, scale);
            }
        }
    }

    for(i = 0; i < map->numSubsectors; ++i)
    {
        const subsector_t* subsector = map->subsectors[i];
        uint j;

        for(j = 0; j < subsector->sector->planeCount; ++j)
        {
            vec3_t origin;
            plane_t* pln = subsector->sector->SP_plane(j);
            float scale = NORM_TAIL_LENGTH;

            V3_Set(origin, subsector->midPoint[VX], subsector->midPoint[VY], pln->visHeight);
            if((j == PLN_FLOOR || j == PLN_CEILING) && IS_SKYSURFACE(&pln->surface))
                origin[VZ] = j == PLN_FLOOR? map->skyFixFloor : map->skyFixCeiling;

            drawNormal(origin, pln->PS_normal, scale);
        }
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);

#undef NORM_TAIL_LENGTH
}

static void getVertexPlaneMinMax(const vertex_t* vtx, float* min, float* max)
{
    const hedge_t* hEdge, *base;

    if(!vtx || (!min && !max))
        return; // Wha?

    hEdge = base = vtx->hEdge;

    do
    {
        const seg_t* seg = (seg_t*) hEdge->data;

        if(seg && seg->sideDef)
        {
            linedef_t* lineDef = seg->sideDef->lineDef;

            if(!LINE_SELFREF(lineDef))
            {
                if(LINE_FRONTSIDE(lineDef))
                {
                    const sector_t* sec = LINE_FRONTSECTOR(lineDef);

                    if(min && sec->SP_floorvisheight < *min)
                        *min = sec->SP_floorvisheight;

                    if(max && sec->SP_ceilvisheight > *max)
                        *max = sec->SP_ceilvisheight;
                }

                if(LINE_BACKSIDE(lineDef))
                {
                    const sector_t* sec = LINE_BACKSECTOR(lineDef);

                    if(min && sec->SP_floorvisheight < *min)
                        *min = sec->SP_floorvisheight;

                    if(max && sec->SP_ceilvisheight > *max)
                        *max = sec->SP_ceilvisheight;
                }
            }
        }

        hEdge = hEdge->twin->next;
    } while(hEdge != base);
}

#define MAX_VERTEX_POINT_DIST 1280

static void drawVertexPoint(const vertex_t* vtx, float z, float alpha)
{
    glBegin(GL_POINTS);
        glColor4f(.7f, .7f, .2f, alpha * 2);
        glVertex3f(vtx->pos[VX], z, vtx->pos[VY]);
    glEnd();
}

static int _drawVertexPoint(vertex_t* vertex, void* context)
{
    mvertex_t* vinfo = (mvertex_t*) vertex->data;

    if(vinfo)
    {
        float dist = M_ApproxDistancef(vx - vertex->pos[VX], vz - vertex->pos[VY]);

        if(dist < MAX_VERTEX_POINT_DIST)
        {
            float bottom = DDMAXFLOAT;
            getVertexPlaneMinMax(vertex, &bottom, NULL);

            drawVertexPoint(vertex, bottom, (1 - dist / MAX_VERTEX_POINT_DIST) * 2);
        }
    }
    return true; // Continue iteration.
}

static void drawVertexBar(const vertex_t* vtx, float bottom, float top,
                          float alpha)
{
#define EXTEND_DIST     64

    static const float black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(vtx->pos[VX], bottom - EXTEND_DIST, vtx->pos[VY]);
        glColor4f(1, 1, 1, alpha);
        glVertex3f(vtx->pos[VX], bottom, vtx->pos[VY]);
        glVertex3f(vtx->pos[VX], bottom, vtx->pos[VY]);
        glVertex3f(vtx->pos[VX], top, vtx->pos[VY]);
        glVertex3f(vtx->pos[VX], top, vtx->pos[VY]);
        glColor4fv(black);
        glVertex3f(vtx->pos[VX], top + EXTEND_DIST, vtx->pos[VY]);
    glEnd();

#undef EXTEND_DIST
}

static int _drawVertexBar(vertex_t* vertex, void* context)
{
    mvertex_t* vinfo = (mvertex_t*) vertex->data;

    if(vinfo)
    {
        float alpha = 1 - M_ApproxDistancef(vx - vertex->pos[VX], vz - vertex->pos[VY]) / MAX_VERTEX_POINT_DIST;
        alpha = MIN_OF(alpha, .15f);

        if(alpha > 0)
        {
            float bottom, top;

            bottom = DDMAXFLOAT;
            top = DDMINFLOAT;
            getVertexPlaneMinMax(vertex, &bottom, &top);

            drawVertexBar(vertex, bottom, top, alpha);
        }
    }

    return true; // Continue iteration.
}

static void drawVertexIndex(vertex_t* vtx, float z, float scale,
                            float alpha)
{
    char buf[80];

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(vtx->pos[VX], z, vtx->pos[VY]);
    glRotatef(-vang + 180, 0, 1, 0);
    glRotatef(vpitch, 1, 0, 0);
    glScalef(-scale, -scale, 1);

    sprintf(buf, "%i", (int) P_ObjectRecord(DMU_VERTEX, vtx)->id);
    UI_TextOutEx(buf, 2, 2, false, false, UI_Color(UIC_TITLE), alpha);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

static int _drawVertexIndex(vertex_t* vertex, void* context)
{
    mvertex_t* vinfo = (mvertex_t*) vertex->data;

    if(vinfo)
    {
        float* eye = (float*) context;
        float pos[3], dist;

        pos[VX] = vertex->pos[VX];
        pos[VY] = vertex->pos[VY];
        pos[VZ] = DDMAXFLOAT;
        getVertexPlaneMinMax(vertex, &pos[VZ], NULL);

        dist = M_Distance(pos, eye);

        if(dist < MAX_VERTEX_POINT_DIST)
        {
            float alpha, scale;

            alpha = 1 - dist / MAX_VERTEX_POINT_DIST;
            scale = dist / (theWindow->width / 2);

            drawVertexIndex(vertex, pos[VZ], scale, alpha);
        }
    }
    return true; // Continue iteration.
}

static boolean drawVertex1(linedef_t* li, void* context)
{
    vertex_t* vtx = li->L_v1;
    polyobj_t* po = context;
    subsector_t* subsector = ((objectrecord_t*) po->subsector)->obj;
    float dist2D =
        M_ApproxDistancef(vx - vtx->pos[VX], vz - vtx->pos[VY]);

    if(dist2D < MAX_VERTEX_POINT_DIST)
    {
        float alpha = 1 - dist2D / MAX_VERTEX_POINT_DIST;

        if(alpha > 0)
        {
            float bottom = subsector->sector->SP_floorvisheight;
            float top = subsector->sector->SP_ceilvisheight;

            glDisable(GL_TEXTURE_2D);

            if(devVertexBars)
                drawVertexBar(vtx, bottom, top, MIN_OF(alpha, .15f));

            drawVertexPoint(vtx, bottom, alpha * 2);

            glEnable(GL_TEXTURE_2D);
        }
    }

    if(devVertexIndices)
    {
        float eye[3], pos[3], dist3D;

        eye[VX] = vx;
        eye[VY] = vz;
        eye[VZ] = vy;

        pos[VX] = vtx->pos[VX];
        pos[VY] = vtx->pos[VY];
        pos[VZ] = subsector->sector->SP_floorvisheight;

        dist3D = M_Distance(pos, eye);

        if(dist3D < MAX_VERTEX_POINT_DIST)
        {
            drawVertexIndex(vtx, pos[VZ], dist3D / (theWindow->width / 2),
                            1 - dist3D / MAX_VERTEX_POINT_DIST);
        }
    }

    return true; // Continue iteration.
}

/**
 * Draw the various vertex debug aids.
 */
void Rend_Vertexes(map_t* map)
{
    float oldPointSize, oldLineWidth = 1;

    if(!devVertexBars && !devVertexIndices)
        return;

    glDisable(GL_DEPTH_TEST);

    if(devVertexBars)
    {
        glEnable(GL_LINE_SMOOTH);
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, 2);
        glDisable(GL_TEXTURE_2D);

        HalfEdgeDS_IterateVertices(Map_HalfEdgeDS(map), _drawVertexBar, NULL);

        glEnable(GL_TEXTURE_2D);
    }

    // Always draw the vertex point nodes.
    glEnable(GL_POINT_SMOOTH);
    oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);
    DGL_SetFloat(DGL_POINT_SIZE, 6);
    glDisable(GL_TEXTURE_2D);

    HalfEdgeDS_IterateVertices(Map_HalfEdgeDS(map), _drawVertexPoint, NULL);

    glEnable(GL_TEXTURE_2D);

    if(devVertexIndices)
    {
        float eye[3];

        eye[VX] = vx;
        eye[VY] = vz;
        eye[VZ] = vy;

        HalfEdgeDS_IterateVertices(Map_HalfEdgeDS(map), _drawVertexIndex, eye);
    }

    // Restore previous state.
    if(devVertexBars)
    {
        DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);
        glDisable(GL_LINE_SMOOTH);
    }
    DGL_SetFloat(DGL_POINT_SIZE, oldPointSize);
    glDisable(GL_POINT_SMOOTH);
    glEnable(GL_DEPTH_TEST);
}

/**
 * Draws the lightModRange (for debug)
 */
void Rend_RenderLightModRange(void)
{
#define bWidth (1.0f)
#define bHeight (bWidth * 255.0f)
#define BORDER (20)

    int i;
    float c, off;
    ui_color_t color;

    if(!devLightModRange)
        return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    glTranslatef(BORDER, BORDER, 0);

    color.red = 0.2f;
    color.green = 0;
    color.blue = 0.6f;

    glDisable(GL_TEXTURE_2D);

    // Draw an outside border.
    glColor4f(1, 1, 0, 1);
    glBegin(GL_LINES);
        glVertex2f(-1, -1);
        glVertex2f(255 + 1, -1);
        glVertex2f(255 + 1,  -1);
        glVertex2f(255 + 1,  bHeight + 1);
        glVertex2f(255 + 1,  bHeight + 1);
        glVertex2f(-1,  bHeight + 1);
        glVertex2f(-1,  bHeight + 1);
        glVertex2f(-1, -1);
    glEnd();

    glBegin(GL_QUADS);
    for(i = 0, c = 0; i < 255; ++i, c += (1.0f/255.0f))
    {
        // Get the result of the source light level + offset.
        off = lightModRange[i];

        glColor4f(c + off, c + off, c + off, 1);
        glVertex2f(i * bWidth, 0);
        glVertex2f(i * bWidth + bWidth, 0);
        glVertex2f(i * bWidth + bWidth,  bHeight);
        glVertex2f(i * bWidth, bHeight);
    }
    glEnd();

    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

#undef bWidth
#undef bHeight
#undef BORDER
}

static DGLuint constructBBox(DGLuint name, float br)
{
    if(GL_NewList(name, GL_COMPILE))
    {
        glBegin(GL_QUADS);
        {
            // Top
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br, 1.0f,-1.0f-br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br, 1.0f,-1.0f-br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br, 1.0f, 1.0f+br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br, 1.0f, 1.0f+br);  // BR
            // Bottom
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br,-1.0f, 1.0f+br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br,-1.0f, 1.0f+br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br,-1.0f,-1.0f-br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br,-1.0f,-1.0f-br);  // BR
            // Front
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br, 1.0f+br, 1.0f);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br, 1.0f+br, 1.0f);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br,-1.0f-br, 1.0f);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br,-1.0f-br, 1.0f);  // BR
            // Back
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f+br,-1.0f-br,-1.0f);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f-br,-1.0f-br,-1.0f);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f-br, 1.0f+br,-1.0f);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f+br, 1.0f+br,-1.0f);  // BR
            // Left
            glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, 1.0f+br, 1.0f+br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, 1.0f+br,-1.0f-br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,-1.0f-br,-1.0f-br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f,-1.0f-br, 1.0f+br);  // BR
            // Right
            glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, 1.0f+br,-1.0f-br);  // TR
            glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, 1.0f+br, 1.0f+br);  // TL
            glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f,-1.0f-br, 1.0f+br);  // BL
            glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,-1.0f-br,-1.0f-br);  // BR
        }
        glEnd();

        return GL_EndList();
    }

    return 0;
}

/**
 * Draws a textured cube using the currently bound gl texture.
 * Used to draw mobj bounding boxes.
 *
 * @param pos3f         Coordinates of the center of the box (in "world"
 *                      coordinates [VX, VY, VZ]).
 * @param w             Width of the box.
 * @param l             Length of the box.
 * @param h             Height of the box.
 * @param color3f       Color to make the box (uniform vertex color).
 * @param alpha         Alpha to make the box (uniform vertex color).
 * @param br            Border amount to overlap box faces.
 * @param alignToBase   If @c true, align the base of the box
 *                      to the Z coordinate.
 */
void Rend_DrawBBox(const float pos3f[3], float w, float l, float h,
                   const float color3f[3], float alpha, float br,
                   boolean alignToBase)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    if(alignToBase)
        // The Z coordinate is to the bottom of the object.
        glTranslatef(pos3f[VX], pos3f[VZ] + h, pos3f[VY]);
    else
        glTranslatef(pos3f[VX], pos3f[VZ], pos3f[VY]);

    glScalef(w - br - br, h - br - br, l - br - br);
    glColor4f(color3f[0], color3f[1], color3f[2], alpha);

    GL_CallList(dlBBox);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

/**
 * Draws a textured triangle using the currently bound gl texture.
 * Used to draw mobj angle direction arrow.
 *
 * @param pos3f         Coordinates of the center of the base of the
 *                      triangle (in "world" coordinates [VX, VY, VZ]).
 * @param a             Angle to point the triangle in.
 * @param s             Scale of the triangle.
 * @param color3f       Color to make the box (uniform vertex color).
 * @param alpha         Alpha to make the box (uniform vertex color).
 */
void Rend_DrawArrow(const float pos3f[3], angle_t a, float s,
                    const float color3f[3], float alpha)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glTranslatef(pos3f[VX], pos3f[VZ], pos3f[VY]);

    glRotatef(0, 0, 0, 1);
    glRotatef(0, 1, 0, 0);
    glRotatef((a / (float) ANGLE_MAX *-360), 0, 1, 0);

    glScalef(s, 0, s);

    glBegin(GL_TRIANGLES);
    {
        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f( 1.0f, 1.0f,-1.0f);  // L

        glColor4f(color3f[0], color3f[1], color3f[2], alpha);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-1.0f, 1.0f,-1.0f);  // Point

        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-1.0f, 1.0f, 1.0f);  // R
    }
    glEnd();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

/**
 * Renders bounding boxes for all mobj's (linked in sec->mobjList, except
 * the console player) in all sectors that are currently marked as vissible.
 *
 * Depth test is disabled to show all mobjs that are being rendered, regardless
 * if they are actually vissible (hidden by previously drawn map geometry).
 */
static void Rend_RenderBoundingBoxes(map_t* map)
{
    static const float  red[3] = { 1, 0.2f, 0.2f}; // non-solid objects
    static const float  green[3] = { 0.2f, 1, 0.2f}; // solid objects
    static const float  yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    uint i;
    float eye[3];
    material_t* mat;
    material_snapshot_t ms;

    if(!devMobjBBox || netGame)
        return;

    if(!dlBBox)
        dlBBox = constructBBox(0, .08f);

    eye[VX] = vx;
    eye[VY] = vz;
    eye[VZ] = vy;

    if(usingFog)
        glDisable(GL_FOG);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    mat = Materials_ToMaterial2(MN_SYSTEM, DDT_BBOX);
    Materials_Prepare(mat, MPF_SMOOTH, NULL, &ms);

    GL_BindTexture(ms.units[MTU_PRIMARY].texInst->id,
                   ms.units[MTU_PRIMARY].magMode);
    GL_BlendMode(BM_ADD);

    // For every sector
    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sec = map->sectors[i];
        mobj_t* mo;

        // Is it vissible?
        if(!(sec->frameFlags & SIF_VISIBLE))
            continue;

        // For every mobj in the sector's mobjList
        for(mo = sec->mobjList; mo; mo = mo->sNext)
        {
            float alpha, size;

            if(mo == ddPlayers[consolePlayer].shared.mo)
                continue; // We don't want the console player.

            alpha = 1 - ((M_Distance(mo->pos, eye)/(theWindow->width/2))/4);

            if(alpha < .25f)
                alpha = .25f; // Don't make them totally invisible.

            // Draw a bounding box in an appropriate color.
            size = mo->radius;
            Rend_DrawBBox(mo->pos, size, size, mo->height/2,
                          (mo->ddFlags & DDMF_MISSILE)? yellow :
                          (mo->ddFlags & DDMF_SOLID)? green : red,
                          alpha, .08f, true);

            Rend_DrawArrow(mo->pos, mo->angle + ANG45 + ANG90 , size*1.25,
                           (mo->ddFlags & DDMF_MISSILE)? yellow :
                           (mo->ddFlags & DDMF_SOLID)? green : red, alpha);
        }
    }

    GL_BlendMode(BM_NORMAL);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    if(usingFog)
        glEnable(GL_FOG);
}
