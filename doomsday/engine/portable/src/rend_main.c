/**\file rend_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include "texturevariant.h"
#include "materialvariant.h"
#include "blockmapvisual.h"

void Rend_RenderBoundingBox(coord_t const pos[3], coord_t w, coord_t l, coord_t h, float a,
    float const color[3], float alpha, float br, boolean alignToBase);

void Rend_RenderArrow(coord_t const pos[3], float a, float s, float const color3f[3], float alpha);

static DGLuint constructBBox(DGLuint name, float br);
static material_t* chooseHEdgeMaterial(HEdge* hedge, SideDefSection section);
static void wallEdgeLightLevelDeltas(HEdge* hedge, SideDefSection section, float* deltaL, float* deltaR);
static uint Rend_BuildBspLeafPlaneGeometry(BspLeaf* leaf, boolean antiClockwise,
    coord_t height, rvertex_t** verts, uint* vertsSize);

boolean usingFog = false; // Is the fog in use?
float fogColor[4];
float fieldOfView = 95.0f;
byte smoothTexAnim = true;
int useShinySurfaces = true;

int useDynLights = true;
float dynlightFactor = .5f;
float dynlightFogBright = .15f;

int useWallGlow = true;
float glowFactor = .5f;
float glowHeightFactor = 3; // Glow height as a multiplier.
int glowHeightMax = 100; // 100 is the default (0-1024).

int useShadows = true;
float shadowFactor = 1.2f;
int shadowMaxRadius = 80;
int shadowMaxDistance = 1000;

coord_t vOrigin[3];
float vang, vpitch;
float viewsidex, viewsidey;

byte freezeRLs = false;
byte devRendSkyMode = false;
byte devRendSkyAlways = false;

// Ambient lighting, rAmbient is used within the renderer, ambientLight is
// used to store the value of the ambient light cvar.
// The value chosen for rAmbient occurs in Rend_CalcLightModRange
// for convenience (since we would have to recalculate the matrix anyway).
int rAmbient = 0, ambientLight = 0;

int viewpw, viewph; // Viewport size, in pixels.
int viewpx, viewpy; // Viewpoint top left corner, in pixels.

float yfov;

int gameDrawHUD = 1; // Set to zero when we advise that the HUD should not be drawn

/**
 * Implements a pre-calculated LUT for light level limiting and range
 * compression offsets, arranged such that it may be indexed with a
 * light level value. Return value is an appropriate delta (considering
 * all applicable renderer properties) which has been pre-clamped such
 * that when summed with the original light value the result remains in
 * the normalized range [0..1].
 */
float lightRangeCompression = 0;
float lightModRange[255];
byte devLightModRange = 0;

float rendLightDistanceAttentuation = 1024;

byte devMobjVLights = 0; // @c 1= Draw mobj vertex lighting vector.
byte devMobjBBox = 0; // 1 = Draw mobj bounding boxes (for debug).
byte devPolyobjBBox = 0; // 1 = Draw polyobj bounding boxes (for debug).
byte devVertexIndices = 0; // @c 1= Draw world vertex indices (for debug).
byte devVertexBars = 0; // @c 1= Draw world vertex position bars.
byte devSoundOrigins = 0; ///< cvar @c 1= Draw sound origin debug display.
byte devSurfaceVectors = 0;
byte devNoTexFix = 0;

static BspLeaf* currentBspLeaf; // BSP leaf currently being drawn.
static boolean firstBspLeaf; // No range checking for the first one.

void Rend_Register(void)
{
    C_VAR_FLOAT("rend-camera-fov", &fieldOfView, 0, 1, 179);

    C_VAR_FLOAT("rend-glow", &glowFactor, 0, 0, 2);
    C_VAR_INT("rend-glow-height", &glowHeightMax, 0, 0, 1024);
    C_VAR_FLOAT("rend-glow-scale", &glowHeightFactor, 0, 0.1f, 10);
    C_VAR_INT("rend-glow-wall", &useWallGlow, 0, 0, 1);

    C_VAR_INT2("rend-light", &useDynLights, 0, 0, 1, LO_UnlinkMobjLumobjs);
    C_VAR_INT2("rend-light-ambient", &ambientLight, 0, 0, 255, Rend_CalcLightModRange);
    C_VAR_FLOAT("rend-light-attenuation", &rendLightDistanceAttentuation, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-light-bright", &dynlightFactor, 0, 0, 1);
    C_VAR_FLOAT2("rend-light-compression", &lightRangeCompression, 0, -1, 1, Rend_CalcLightModRange);
    C_VAR_FLOAT("rend-light-fog-bright", &dynlightFogBright, 0, 0, 1);
    C_VAR_FLOAT2("rend-light-sky", &rendSkyLight, 0, 0, 1, LG_MarkAllForUpdate);
    C_VAR_BYTE2("rend-light-sky-auto", &rendSkyLightAuto, 0, 0, 1, LG_MarkAllForUpdate);
    C_VAR_FLOAT("rend-light-wall-angle", &rendLightWallAngle, CVF_NO_MAX, 0, 0);
    C_VAR_BYTE("rend-light-wall-angle-smooth", &rendLightWallAngleSmooth, 0, 0, 1);

    C_VAR_BYTE("rend-map-material-precache", &precacheMapMaterials, 0, 0, 1);

    C_VAR_INT("rend-shadow", &useShadows, 0, 0, 1);
    C_VAR_FLOAT("rend-shadow-darkness", &shadowFactor, 0, 0, 2);
    C_VAR_INT("rend-shadow-far", &shadowMaxDistance, CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-shadow-radius-max", &shadowMaxRadius, CVF_NO_MAX, 0, 0);

    C_VAR_BYTE("rend-tex-anim-smooth", &smoothTexAnim, 0, 0, 1);
    C_VAR_INT("rend-tex-shiny", &useShinySurfaces, 0, 0, 1);

    C_VAR_BYTE("rend-dev-sky", &devRendSkyMode, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-sky-always", &devRendSkyAlways, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-freeze", &freezeRLs, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-cull-leafs", &devNoCulling, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-mobj-bbox", &devMobjBBox, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-mobj-show-vlights", &devMobjVLights, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-polyobj-bbox", &devPolyobjBBox, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-light-mod", &devLightModRange, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-tex-showfix", &devNoTexFix, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-blockmap-debug", &bmapShowDebug, CVF_NO_ARCHIVE, 0, 4);
    C_VAR_FLOAT("rend-dev-blockmap-debug-size", &bmapDebugSize, CVF_NO_ARCHIVE, .1f, 100);
    C_VAR_BYTE("rend-dev-vertex-show-indices", &devVertexIndices, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-vertex-show-bars", &devVertexBars, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-surface-show-vectors", &devSurfaceVectors, CVF_NO_ARCHIVE, 0, 7);
    C_VAR_BYTE("rend-dev-soundorigins", &devSoundOrigins, CVF_NO_ARCHIVE, 0, 7);

    RL_Register();
    LO_Register();
    Rend_DecorRegister();
    SB_Register();
    LG_Register();
    Rend_SkyRegister();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Rend_RadioRegister();
    Rend_SpriteRegister();
    Rend_ConsoleRegister();
}

/**
 * Approximated! The Z axis aspect ratio is corrected.
 */
coord_t Rend_PointDist3D(coord_t const point[3])
{
    return M_ApproxDistance3(vOrigin[VX] - point[VX], vOrigin[VZ] - point[VY], 1.2 * (vOrigin[VY] - point[VZ]));
}

void Rend_Init(void)
{
    C_Init();
    RL_Init();
    Rend_SkyInit();
}

void Rend_Shutdown(void)
{
    RL_Shutdown();
}

/// World/map renderer reset.
void Rend_Reset(void)
{
    LO_Clear(); // Free lumobj stuff.
    Rend_ClearBoundingBoxGLData();
}

void Rend_ModelViewMatrix(boolean useAngles)
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

    vOrigin[VX] = viewData->current.origin[VX];
    vOrigin[VY] = viewData->current.origin[VZ];
    vOrigin[VZ] = viewData->current.origin[VY];
    vang = viewData->current.angle / (float) ANGLE_MAX *360 - 90;
    vpitch = viewData->current.pitch * 85.0 / 110.0;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if(useAngles)
    {
        glRotatef(vpitch, 1, 0, 0);
        glRotatef(vang, 0, 1, 0);
    }
    glScalef(1, 1.2f, 1);      // This is the aspect correction.
    glTranslatef(-vOrigin[VX], -vOrigin[VY], -vOrigin[VZ]);
}

static __inline double viewFacingDot(coord_t v1[2], coord_t v2[2])
{
    // The dot product.
    return (v1[VY] - v2[VY]) * (v1[VX] - vOrigin[VX]) + (v2[VX] - v1[VX]) * (v1[VY] - vOrigin[VZ]);
}

static __inline boolean isOneSided(HEdge* hedge, Sector* frontSec, Sector* backSec)
{
    return !frontSec || !backSec || (hedge->twin && !HEDGE_SIDEDEF(hedge->twin)) /* front side of a "window" */;
}

static void markSideDefSectionsPVisible(HEdge* hedge)
{
    LineDef* line = hedge->lineDef;
    SideDef* frontSideDef = HEDGE_SIDEDEF(hedge);
    Sector* frontSec, *backSec;

    if(!line || !frontSideDef) return;

    frontSec = hedge->lineDef->L_sector(hedge->side); /// @todo frontSec should be derived from hedge not linedef
    backSec  = HEDGE_BACK_SECTOR(hedge);

    // Init flags.
    frontSideDef->SW_middleinflags &= ~SUIF_PVIS;
    frontSideDef->SW_topinflags    &= ~SUIF_PVIS;
    frontSideDef->SW_bottominflags &= ~SUIF_PVIS;

    // Middle surface:
    if(Surface_IsDrawable(&frontSideDef->SW_middlesurface))
    {
        frontSideDef->SW_middleinflags |= SUIF_PVIS;
    }

    // If "one-sided" only the middle is potentially visible.
    if(!isOneSided(hedge, frontSec, backSec))
    {
        const Plane* ffloor = frontSec->SP_floor;
        const Plane* fceil  = frontSec->SP_ceil;
        const Plane* bfloor = backSec->SP_floor;
        const Plane* bceil  = backSec->SP_ceil;

        // Top surface:
        if(!(!devRendSkyMode && Surface_IsSkyMasked(&fceil->surface) && Surface_IsSkyMasked(&bceil->surface)) &&
        // !(!devRendSkyMode && Surface_IsSkyMasked(&bceil->surface) && (side->SW_topsurface.inFlags & SUIF_FIX_MISSING_MATERIAL)) &&
           fceil->visHeight > bceil->visHeight &&
           Surface_IsDrawable(&frontSideDef->SW_topsurface))
        {
            frontSideDef->SW_topsurface.inFlags |= SUIF_PVIS;
        }

        // Bottom surface:
        if(!(!devRendSkyMode && Surface_IsSkyMasked(&ffloor->surface) && Surface_IsSkyMasked(&bfloor->surface)) &&
        // !(!devRendSkyMode && Surface_IsSkyMasked(&bfloor->surface) && (side->SW_bottomsurface.inFlags & SUIF_FIX_MISSING_MATERIAL)) ||
           ffloor->visHeight < bfloor->visHeight &&
           Surface_IsDrawable(&frontSideDef->SW_bottomsurface))
        {
            frontSideDef->SW_bottomsurface.inFlags |= SUIF_PVIS;
        }
    }
}

static void Rend_MarkSideDefSectionsPVisible(HEdge* hedge)
{
    assert(hedge);
    /// @todo Do we really need to do this for both sides? Surely not.
    markSideDefSectionsPVisible(hedge);
    if(hedge->twin)
        markSideDefSectionsPVisible(hedge->twin);
}

int RIT_FirstDynlightIterator(const dynlight_t* dyn, void* paramaters)
{
    const dynlight_t** ptr = (const dynlight_t**)paramaters;
    *ptr = dyn;
    return 1; // Stop iteration.
}

static __inline const materialvariantspecification_t*
mapSurfaceMaterialSpec(int wrapS, int wrapT)
{
    return Materials_VariantSpecificationForContext(MC_MAPSURFACE, 0, 0, 0, 0,
                                                    wrapS, wrapT, -1, -1, -1,
                                                    true, true, false, false);
}

/**
 * This doesn't create a rendering primitive but a vissprite! The vissprite
 * represents the masked poly and will be rendered during the rendering
 * of sprites. This is necessary because all masked polygons must be
 * rendered back-to-front, or there will be alpha artifacts along edges.
 */
static void Rend_WriteMaskedPoly(const rvertex_t* rvertices, const ColorRawf* rcolors,
    coord_t wallLength, materialvariant_t* material, float const texOffset[2],
    blendmode_t blendMode, uint lightListIdx, float glow)
{
    vissprite_t* vis = R_NewVisSprite();
    int i, c;

    vis->type = VSPR_MASKED_WALL;
    vis->origin[VX] = (rvertices[0].pos[VX] + rvertices[3].pos[VX]) / 2;
    vis->origin[VY] = (rvertices[0].pos[VY] + rvertices[3].pos[VY]) / 2;
    vis->origin[VZ] = (rvertices[0].pos[VZ] + rvertices[3].pos[VZ]) / 2;
    vis->distance = Rend_PointDist2D(vis->origin);

    if(texOffset)
    {
        VS_WALL(vis)->texOffset.x = texOffset[VX];
        VS_WALL(vis)->texOffset.y = texOffset[VY];
    }

    // Masked walls are sometimes used for special effects like arcs,
    // cobwebs and bottoms of sails. In order for them to look right,
    // we need to disable texture wrapping on the horizontal axis (S).
    // Most masked walls need wrapping, though. What we need to do is
    // look at the texture coordinates and see if they require texture
    // wrapping.
    if(renderTextures)
    {
        const materialsnapshot_t* ms = Materials_PrepareVariant(material);
        int wrapS = GL_REPEAT, wrapT = GL_REPEAT;

        VS_WALL(vis)->texCoord[0][VX] = VS_WALL(vis)->texOffset.x / ms->size.width;
        VS_WALL(vis)->texCoord[1][VX] = VS_WALL(vis)->texCoord[0][VX] + wallLength / ms->size.width;
        VS_WALL(vis)->texCoord[0][VY] = VS_WALL(vis)->texOffset.y / ms->size.height;
        VS_WALL(vis)->texCoord[1][VY] = VS_WALL(vis)->texCoord[0][VY] +
                (rvertices[3].pos[VZ] - rvertices[0].pos[VZ]) / ms->size.height;

        if(ms && !ms->isOpaque)
        {
            if(!(VS_WALL(vis)->texCoord[0][VX] < 0 || VS_WALL(vis)->texCoord[0][VX] > 1 ||
                 VS_WALL(vis)->texCoord[1][VX] < 0 || VS_WALL(vis)->texCoord[1][VX] > 1))
            {
                // Visible portion is within the actual [0..1] range.
                wrapS = GL_CLAMP_TO_EDGE;
            }

            // Clamp on the vertical axis if the coords are in the normal [0..1] range.
            if(!(VS_WALL(vis)->texCoord[0][VY] < 0 || VS_WALL(vis)->texCoord[0][VY] > 1 ||
                 VS_WALL(vis)->texCoord[1][VY] < 0 || VS_WALL(vis)->texCoord[1][VY] > 1))
            {
                wrapT = GL_CLAMP_TO_EDGE;
            }
        }

        // Choose an appropriate variant.
        /// @todo Can result in multiple variants being prepared.
        ///       This decision should be made earlier (in rendHEdgeSection()).
        material = Materials_ChooseVariant(MaterialVariant_GeneralCase(material),
                                           mapSurfaceMaterialSpec(wrapS, wrapT), true, true);
    }

    VS_WALL(vis)->material = material;
    VS_WALL(vis)->blendMode = blendMode;

    for(i = 0; i < 4; ++i)
    {
        VS_WALL(vis)->vertices[i].pos[VX] = rvertices[i].pos[VX];
        VS_WALL(vis)->vertices[i].pos[VY] = rvertices[i].pos[VY];
        VS_WALL(vis)->vertices[i].pos[VZ] = rvertices[i].pos[VZ];

        for(c = 0; c < 4; ++c)
        {
            /// @todo Do not clamp here.
            VS_WALL(vis)->vertices[i].color[c] = MINMAX_OF(0, rcolors[i].rgba[c], 1);
        }
    }

    /// @todo Semitransparent masked polys arn't lit atm
    if(glow < 1 && lightListIdx && numTexUnits > 1 && envModAdd &&
       !(rcolors[0].rgba[CA] < 1))
    {
        const dynlight_t* dyn = NULL;

        /**
         * The dynlights will have already been sorted so that the brightest
         * and largest of them is first in the list. So grab that one.
         */
        LO_IterateProjections2(lightListIdx, RIT_FirstDynlightIterator, (void*)&dyn);

        VS_WALL(vis)->modTex = dyn->texture;
        VS_WALL(vis)->modTexCoord[0][0] = dyn->s[0];
        VS_WALL(vis)->modTexCoord[0][1] = dyn->s[1];
        VS_WALL(vis)->modTexCoord[1][0] = dyn->t[0];
        VS_WALL(vis)->modTexCoord[1][1] = dyn->t[1];
        for(c = 0; c < 4; ++c)
            VS_WALL(vis)->modColor[c] = dyn->color.rgba[c];
    }
    else
    {
        VS_WALL(vis)->modTex = 0;
    }
}

static float getMaterialSnapshots(const materialsnapshot_t** msA,
    const materialsnapshot_t** msB, material_t* mat)
{
    const materialvariantspecification_t* spec = mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT);
    float interPos = 0;
    assert(msA);

    *msA = Materials_Prepare(mat, spec, true);

    // Smooth Texture Animation?
    if(msB)
    {
        materialvariant_t* variant = Materials_ChooseVariant(mat, spec, false, false);
        if(MaterialVariant_TranslationCurrent(variant) != MaterialVariant_TranslationNext(variant))
        {
            materialvariant_t* matB = MaterialVariant_TranslationNext(variant);

            // Prepare the inter texture.
            *msB = Materials_PrepareVariant(matB);

            // If fog is active, inter=0 is accepted as well. Otherwise
            // flickering may occur if the rendering passes don't match for
            // blended and unblended surfaces.
            if(!(!usingFog && MaterialVariant_TranslationPoint(matB) < 0))
            {
                interPos = MaterialVariant_TranslationPoint(matB);
            }
        }
        else
        {
            *msB = NULL;
        }
    }

    return interPos;
}

static void quadTexCoords(rtexcoord_t* tc, const rvertex_t* rverts,
    coord_t wallLength, coord_t const topLeft[3])
{
    tc[0].st[0] = tc[1].st[0] =
        rverts[0].pos[VX] - topLeft[VX];
    tc[3].st[1] = tc[1].st[1] =
        rverts[0].pos[VY] - topLeft[VY];
    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]);
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]);
}

static void quadLightCoords(rtexcoord_t* tc, float const s[2], float const t[2])
{
    tc[1].st[0] = tc[0].st[0] = s[0];
    tc[1].st[1] = tc[3].st[1] = t[0];
    tc[3].st[0] = tc[2].st[0] = s[1];
    tc[2].st[1] = tc[0].st[1] = t[1];
}

static float shinyVertical(float dy, float dx)
{
    return ((atan(dy/dx) / (PI/2)) + 1) / 2;
}

static void quadShinyTexCoords(rtexcoord_t* tc, const rvertex_t* topLeft,
    const rvertex_t* bottomRight, coord_t wallLength)
{
    vec2f_t surface, normal, projected, s, reflected, view;
    float distance, angle, prevAngle = 0;
    uint i;

    // Quad surface vector.
    V2f_Set(surface, (bottomRight->pos[VX] - topLeft->pos[VX]) / wallLength,
                     (bottomRight->pos[VY] - topLeft->pos[VY]) / wallLength);

    V2f_Set(normal, surface[VY], -surface[VX]);

    // Calculate coordinates based on viewpoint and surface normal.
    for(i = 0; i < 2; ++i)
    {
        // View vector.
        V2f_Set(view, vOrigin[VX] - (i == 0? topLeft->pos[VX] : bottomRight->pos[VX]),
                      vOrigin[VZ] - (i == 0? topLeft->pos[VY] : bottomRight->pos[VY]));

        distance = V2f_Normalize(view);

        V2f_Project(projected, view, normal);
        V2f_Subtract(s, projected, view);
        V2f_Scale(s, 2);
        V2f_Sum(reflected, view, s);

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
            angle + .3f; /*acos(-dot)/PI*/

        tc[ (i == 0 ? 0 : 2) ].st[1] =
            shinyVertical(vOrigin[VY] - bottomRight->pos[VZ], distance);

        // Vertical coordinates.
        tc[ (i == 0 ? 1 : 3) ].st[1] =
            shinyVertical(vOrigin[VY] - topLeft->pos[VZ], distance);
    }
}

static void flatShinyTexCoords(rtexcoord_t* tc, const float xyz[3])
{
    float distance, offset;
    vec2f_t view, start;

    // View vector.
    V2f_Set(view, vOrigin[VX] - xyz[VX], vOrigin[VZ] - xyz[VY]);

    distance = V2f_Normalize(view);
    if(distance < 10)
    {
        // Too small distances cause an ugly 'crunch' below and above
        // the viewpoint.
        distance = 10;
    }

    // Offset from the normal view plane.
    V2f_Set(start, vOrigin[VX], vOrigin[VZ]);

    offset = ((start[VY] - xyz[VY]) * sin(.4f)/*viewFrontVec[VX]*/ -
              (start[VX] - xyz[VX]) * cos(.4f)/*viewFrontVec[VZ]*/);

    tc->st[0] = ((shinyVertical(offset, distance) - .5f) * 2) + .5f;
    tc->st[1] = shinyVertical(vOrigin[VY] - xyz[VZ], distance);
}

static void buildWallSectionCoords(rtexcoord_t* primaryCoords, rtexcoord_t* interCoords,
    rtexcoord_t* modCoords, rtexcoord_t* shinyCoords, rvertex_t* verts, uint numVerts,
    coord_t const* texTL, coord_t const* texBR, float modTexTC[2][2],
    coord_t hedgeLength)
{
    DENG_UNUSED(numVerts);
    DENG_UNUSED(texBR);

    // Primary texture coordinates.
    if(primaryCoords)
    {
        quadTexCoords(primaryCoords, verts, hedgeLength, texTL);
    }

    // Blend texture coordinates.
    if(interCoords)
    {
        quadTexCoords(interCoords, verts, hedgeLength, texTL);
    }

    // Modulation texture coordinates.
    if(modCoords)
    {
        quadLightCoords(modCoords, modTexTC[0], modTexTC[1]);
    }

    // Shiny texture coordinates.
    if(shinyCoords)
    {
        quadShinyTexCoords(shinyCoords, &verts[1], &verts[2], hedgeLength);
    }
}

static void buildPlaneCoords(rtexcoord_t* primaryCoords, rtexcoord_t* interCoords,
    rtexcoord_t* modCoords, rtexcoord_t* shinyCoords, rvertex_t* verts, uint numVerts,
    coord_t const* texTL, coord_t const* texBR, float modTexTC[2][2])
{
    uint i;
    for(i = 0; i < numVerts; ++i)
    {
        const rvertex_t* vtx = &verts[i];
        float xyz[3];

        xyz[VX] = vtx->pos[VX] - texTL[VX];
        xyz[VY] = vtx->pos[VY] - texTL[VY];
        xyz[VZ] = vtx->pos[VZ] - texTL[VZ];

        // Primary texture coordinates.
        if(primaryCoords)
        {
            rtexcoord_t* tc = &primaryCoords[i];
            tc->st[0] =  xyz[VX];
            tc->st[1] = -xyz[VY];
        }

        // Blend primary texture coordinates.
        if(interCoords)
        {
            rtexcoord_t* tc = &interCoords[i];
            tc->st[0] =  xyz[VX];
            tc->st[1] = -xyz[VY];
        }

        // Modulation texture coordinates.
        if(modCoords)
        {
            rtexcoord_t* tc = &modCoords[i];
            const float width  = texBR[VX] - texTL[VX];
            const float height = texBR[VY] - texTL[VY];

            tc->st[0] = ((texBR[VX] - vtx->pos[VX]) / width  * modTexTC[0][0]) + (xyz[VX] / width  * modTexTC[0][1]);
            tc->st[1] = ((texBR[VY] - vtx->pos[VY]) / height * modTexTC[1][0]) + (xyz[VY] / height * modTexTC[1][1]);
        }

        // Shiny texture coordinates.
        if(shinyCoords)
        {
            flatShinyTexCoords(&shinyCoords[i], vtx->pos);
        }
    }
}

/// @note Logic here should match that of buildPlaneColors
static void buildWallSectionColors(ColorRawf* rcolors, uint numVertices, rvertex_t* rvertices,
    const float* sectorLightColor, float sectorLightLevel, float surfaceLightLevelDL, float surfaceLightLevelDR,
    const float* surfaceColor, const float* surfaceColor2, float glowing, float alpha,
    void* mapObject, uint subelementIndex)
{
    if(levelFullBright || !(glowing < 1))
    {
        // Uniform color. Apply to all vertices.
        Rend_SetUniformColor(rcolors, numVertices, sectorLightLevel + (levelFullBright? 1 : glowing));
    }
    else
    {
        // Non-uniform color.
        if(useBias)
        {
            // Do BIAS lighting for this poly.
            SB_LightVertices(rcolors, rvertices, numVertices, sectorLightLevel, mapObject, subelementIndex);
            if(glowing > 0)
            {
                uint i;
                for(i = 0; i < numVertices; ++i)
                {
                    rcolors[i].rgba[CR] = MINMAX_OF(0, rcolors[i].rgba[CR] + glowing, 1);
                    rcolors[i].rgba[CG] = MINMAX_OF(0, rcolors[i].rgba[CG] + glowing, 1);
                    rcolors[i].rgba[CB] = MINMAX_OF(0, rcolors[i].rgba[CB] + glowing, 1);
                }
            }
        }
        else
        {
            float llL = MINMAX_OF(0, sectorLightLevel + surfaceLightLevelDL + glowing, 1);
            float llR = MINMAX_OF(0, sectorLightLevel + surfaceLightLevelDR + glowing, 1);

            // Calculate the color for each vertex, blended with plane color?
            if(surfaceColor[0] < 1 || surfaceColor[1] < 1 || surfaceColor[2] < 1)
            {
                float vColor[4];

                // Blend sector light+color+surfacecolor
                vColor[CR] = surfaceColor[CR] * sectorLightColor[CR];
                vColor[CG] = surfaceColor[CG] * sectorLightColor[CG];
                vColor[CB] = surfaceColor[CB] * sectorLightColor[CB];
                vColor[CA] = 1;

                if(llL != llR)
                {
                    Rend_LightVertex(&rcolors[0], &rvertices[0], llL, vColor);
                    Rend_LightVertex(&rcolors[1], &rvertices[1], llL, vColor);
                    Rend_LightVertex(&rcolors[2], &rvertices[2], llR, vColor);
                    Rend_LightVertex(&rcolors[3], &rvertices[3], llR, vColor);
                }
                else
                {
                    Rend_LightVertices(numVertices, rcolors, rvertices, llL, vColor);
                }
            }
            else
            {
                // Use sector light+color only.
                if(llL != llR)
                {
                    Rend_LightVertex(&rcolors[0], &rvertices[0], llL, sectorLightColor);
                    Rend_LightVertex(&rcolors[1], &rvertices[1], llL, sectorLightColor);
                    Rend_LightVertex(&rcolors[2], &rvertices[2], llR, sectorLightColor);
                    Rend_LightVertex(&rcolors[3], &rvertices[3], llR, sectorLightColor);
                }
                else
                {
                    Rend_LightVertices(numVertices, rcolors, rvertices, llL, sectorLightColor);
                }
            }

            // Bottom color (if different from top)?
            if(surfaceColor2)
            {
                float vColor[4];

                // Blend sector light+color+surfacecolor
                vColor[CR] = surfaceColor2[CR] * sectorLightColor[CR];
                vColor[CG] = surfaceColor2[CG] * sectorLightColor[CG];
                vColor[CB] = surfaceColor2[CB] * sectorLightColor[CB];
                vColor[CA] = 1;

                Rend_LightVertex(&rcolors[0], &rvertices[0], llL, vColor);
                Rend_LightVertex(&rcolors[2], &rvertices[2], llR, vColor);
            }
        }

        Rend_VertexColorsApplyTorchLight(rcolors, rvertices, numVertices);
    }

    // Apply uniform alpha.
    Rend_SetAlpha(rcolors, numVertices, alpha);
}

/// @note Logic here should match that of buildWallSectionColors
static void buildPlaneColors(ColorRawf* rcolors, uint numVertices, rvertex_t* rvertices,
    const float* sectorLightColor, float sectorLightLevel,
    const float* surfaceColor, float glowing, float alpha,
    void* mapObject, uint subelementIndex)
{
    if(levelFullBright || !(glowing < 1))
    {
        // Uniform color. Apply to all vertices.
        Rend_SetUniformColor(rcolors, numVertices, sectorLightLevel + (levelFullBright? 1 : glowing));
    }
    else
    {
        // Non-uniform color.
        if(useBias)
        {
            // Do BIAS lighting for this poly.
            SB_LightVertices(rcolors, rvertices, numVertices, sectorLightLevel, mapObject, subelementIndex);
            if(glowing > 0)
            {
                uint i;
                for(i = 0; i < numVertices; ++i)
                {
                    rcolors[i].rgba[CR] = MINMAX_OF(0, rcolors[i].rgba[CR] + glowing, 1);
                    rcolors[i].rgba[CG] = MINMAX_OF(0, rcolors[i].rgba[CG] + glowing, 1);
                    rcolors[i].rgba[CB] = MINMAX_OF(0, rcolors[i].rgba[CB] + glowing, 1);
                }
            }
        }
        else
        {
            float ll = MINMAX_OF(0, sectorLightLevel + glowing, 1);

            // Calculate the color for each vertex, blended with plane color?
            if(surfaceColor[0] < 1 || surfaceColor[1] < 1 || surfaceColor[2] < 1)
            {
                float vColor[4];

                // Blend sector light+color+surfacecolor
                vColor[CR] = surfaceColor[CR] * sectorLightColor[CR];
                vColor[CG] = surfaceColor[CG] * sectorLightColor[CG];
                vColor[CB] = surfaceColor[CB] * sectorLightColor[CB];
                vColor[CA] = 1;

                Rend_LightVertices(numVertices, rcolors, rvertices, ll, vColor);
            }
            else
            {
                // Use sector light+color only.
                Rend_LightVertices(numVertices, rcolors, rvertices, ll, sectorLightColor);
            }
        }

        Rend_VertexColorsApplyTorchLight(rcolors, rvertices, numVertices);
    }

    // Apply uniform alpha.
    Rend_SetAlpha(rcolors, numVertices, alpha);
}

/**
 * Map RTU configuration from prepared MaterialSnapshot(s).
 *
 * @param msA  Main material snapshot.
 * @param inter  Blend factor used for smoothing between materials.
 * @param msB  Inter material snapshot. Can be @c NULL.
 */
static void mapRTUStateFromMaterialSnapshots(const materialsnapshot_t* msA,
    float inter, const materialsnapshot_t* msB,
    const float texOffset[2], const float texScale[2])
{
    const rtexmapunit_t* primaryRTU       = &MSU(msA, MTU_PRIMARY);
    const rtexmapunit_t* primaryDetailRTU = (r_detail && Rtu_HasTexture(&MSU(msA, MTU_DETAIL)))? &MSU(msA, MTU_DETAIL) : NULL;
    const rtexmapunit_t* interRTU         = (msB && Rtu_HasTexture(&MSU(msB, MTU_PRIMARY)))? &MSU(msB, MTU_PRIMARY) : NULL;
    const rtexmapunit_t* interDetailRTU   = (r_detail && msB && Rtu_HasTexture(&MSU(msB, MTU_DETAIL)))? &MSU(msB, MTU_DETAIL) : NULL;
    const rtexmapunit_t* shinyRTU         = (useShinySurfaces && Rtu_HasTexture(&MSU(msA, MTU_REFLECTION)))? &MSU(msA, MTU_REFLECTION) : NULL;
    const rtexmapunit_t* shinyMaskRTU     = (useShinySurfaces && Rtu_HasTexture(&MSU(msA, MTU_REFLECTION)) && Rtu_HasTexture(&MSU(msA, MTU_REFLECTION_MASK)))? &MSU(msA, MTU_REFLECTION_MASK) : NULL;

    RL_MapRtu(RTU_PRIMARY, primaryRTU);
    if(texOffset) RL_Rtu_TranslateOffsetv(RTU_PRIMARY, texOffset);
    if(texScale)  RL_Rtu_ScaleST(RTU_PRIMARY, texScale);

    RL_MapRtu(RTU_PRIMARY_DETAIL, primaryDetailRTU);
    if(primaryDetailRTU)
    {
        if(texOffset) RL_Rtu_TranslateOffsetv(RTU_PRIMARY_DETAIL, texOffset);
    }

    if(interRTU)
    {
        // Blend between the primary and inter textures.
        rtexmapunit_t rtu;

        memcpy(&rtu, interRTU, sizeof rtu);
        rtu.opacity = inter;
        if(texOffset) Rtu_TranslateOffsetv(&rtu, texOffset);
        if(texScale)  Rtu_ScaleST(&rtu, texScale);
        RL_CopyRtu(RTU_INTER, &rtu);

        if(interDetailRTU)
        {
            memcpy(&rtu, interDetailRTU, sizeof rtu);
            rtu.opacity = inter;
            if(texOffset) Rtu_TranslateOffsetv(&rtu, texOffset);
            RL_CopyRtu(RTU_INTER_DETAIL, &rtu);
        }
    }

    RL_MapRtu(RTU_REFLECTION, shinyRTU);

    RL_MapRtu(RTU_REFLECTION_MASK, shinyMaskRTU);
    if(shinyMaskRTU)
    {
        if(texOffset) RL_Rtu_TranslateOffsetv(RTU_REFLECTION_MASK, texOffset);
        if(texScale)  RL_Rtu_ScaleST(RTU_REFLECTION_MASK, texScale);
    }
}

typedef struct {
    boolean         isWall;
    int             flags; /// @see rendpolyFlags
    blendmode_t     blendMode;
    pvec3d_t        texTL, texBR;
    const float*    texOffset;
    const float*    texScale;
    float           alpha;
    float           sectorLightLevel;
    float           surfaceLightLevelDL;
    float           surfaceLightLevelDR;
    const float*    sectorLightColor;
    const float*    surfaceColor;

    uint            lightListIdx; // List of lights that affect this poly.
    uint            shadowListIdx; // List of shadows that affect this poly.
    float           glowing;

// For bias:
    runtime_mapdata_header_t* mapObject;
    uint            subelementIndex;

// Wall only:
    struct {
        const coord_t*  segLength;
        const float*    surfaceColor2; // Secondary color.
        struct {
            walldivnode_t* firstDiv;
            uint divCount;
        } left;
        struct {
            walldivnode_t* firstDiv;
            uint divCount;
        } right;
    } wall;
} rendworldpoly_params_t;

static boolean renderWorldPoly(rvertex_t* rvertices, uint numVertices,
    const rendworldpoly_params_t* p, const materialsnapshot_t* msA, float inter,
    const materialsnapshot_t* msB)
{
    const uint realNumVertices = ((p->isWall && (p->wall.left.divCount > 2 || p->wall.right.divCount > 2))? 1 + p->wall.left.divCount + 1 + p->wall.right.divCount : numVertices);
    const boolean skyMaskedMaterial = ((p->flags & RPF_SKYMASK) || (msA && Material_IsSkyMasked(MaterialVariant_GeneralCase(msA->material))));

    boolean drawAsVisSprite = false, useLights = false, useShadows = false, hasDynlights = false;
    rtexcoord_t* primaryCoords = NULL, *interCoords = NULL, *modCoords = NULL;
    ColorRawf* rcolors = NULL;
    ColorRawf* shinyColors = NULL;
    rtexcoord_t* shinyCoords = NULL;
    float modTexTC[2][2] = {{ 0, 0 }, { 0, 0 }};
    ColorRawf modColor = { 0, 0, 0, 0 };
    DGLuint modTex = 0;

    if(!(p->flags & RPF_SKYMASK) &&
       (!msA->isOpaque || p->alpha < 1 || p->blendMode > 0))
        drawAsVisSprite = true;

    if(!skyMaskedMaterial)
        rcolors = R_AllocRendColors(realNumVertices);

    primaryCoords = R_AllocRendTexCoords(realNumVertices);

    if(!drawAsVisSprite && !skyMaskedMaterial)
    {
        if(msB && Rtu_HasTexture(&MSU(msB, MTU_PRIMARY)))
            interCoords = R_AllocRendTexCoords(realNumVertices);

        // ShinySurface?
        if(useShinySurfaces && Rtu_HasTexture(&MSU(msA, MTU_REFLECTION)))
        {
            // We'll reuse the same verts but we need new colors.
            shinyColors = R_AllocRendColors(realNumVertices);

            // The normal coords are used with the mask.
            // Another set of coords are required for the shiny texture.
            shinyCoords = R_AllocRendTexCoords(realNumVertices);
        }
    }

    if(!skyMaskedMaterial && p->glowing < 1)
    {
        useLights  = (p->lightListIdx ? true : false);
        useShadows = (p->shadowListIdx? true : false);

        /**
         * If multitexturing is enabled and there is at least one
         * dynlight affecting this surface, grab the paramaters needed
         * to draw it.
         */
        if(useLights && RL_IsMTexLights())
        {
            dynlight_t* dyn = NULL;

            LO_IterateProjections2(p->lightListIdx, RIT_FirstDynlightIterator, (void*)&dyn);

            modCoords = R_AllocRendTexCoords(realNumVertices);

            modTex = dyn->texture;
            modColor.red   = dyn->color.red;
            modColor.green = dyn->color.green;
            modColor.blue  = dyn->color.blue;
            modTexTC[0][0] = dyn->s[0];
            modTexTC[0][1] = dyn->s[1];
            modTexTC[1][0] = dyn->t[0];
            modTexTC[1][1] = dyn->t[1];
        }
    }

    if(p->isWall)
    {
        buildWallSectionCoords(primaryCoords, interCoords, modCoords, shinyCoords,
                               rvertices, numVertices, p->texTL, p->texBR, modTexTC,
                               *p->wall.segLength);
    }
    else
    {
        buildPlaneCoords(primaryCoords, interCoords, modCoords, shinyCoords,
                         rvertices, numVertices, p->texTL, p->texBR, modTexTC);
    }

    // Light this polygon.
    if(!skyMaskedMaterial)
    {
        if(p->isWall)
        {
            buildWallSectionColors(rcolors, numVertices, rvertices, p->sectorLightColor,
                                   p->sectorLightLevel, p->surfaceLightLevelDL, p->surfaceLightLevelDR,
                                   p->surfaceColor, p->wall.surfaceColor2, p->glowing, p->alpha,
                                   p->mapObject, p->subelementIndex);
        }
        else
        {
            buildPlaneColors(rcolors, numVertices, rvertices, p->sectorLightColor,
                             p->sectorLightLevel,
                             p->surfaceColor, p->glowing, p->alpha,
                             p->mapObject, p->subelementIndex);
        }

        if(shinyColors)
        {
            uint i;
            for(i = 0; i < numVertices; ++i)
            {
                shinyColors[i].rgba[CR] = MAX_OF(rcolors[i].rgba[CR], msA->shinyMinColor[CR]);
                shinyColors[i].rgba[CG] = MAX_OF(rcolors[i].rgba[CG], msA->shinyMinColor[CG]);
                shinyColors[i].rgba[CB] = MAX_OF(rcolors[i].rgba[CB], msA->shinyMinColor[CB]);
                shinyColors[i].rgba[CA] = MSU(msA, MTU_REFLECTION).opacity; // Strength of the shine.
            }
        }
    }

    if(useLights || useShadows)
    {
        // Surfaces lit by dynamic lights may need to be rendered
        // differently than non-lit surfaces.
        float avgLightlevel = 0;

        // Determine the average light level of this rend poly,
        // if too bright; do not bother with lights.
        {uint i;
        for(i = 0; i < numVertices; ++i)
        {
            avgLightlevel += rcolors[i].rgba[CR];
            avgLightlevel += rcolors[i].rgba[CG];
            avgLightlevel += rcolors[i].rgba[CB];
        }}
        avgLightlevel /= (float) numVertices * 3;

        if(avgLightlevel > 0.98f)
        {
            useLights = false;
        }
        if(avgLightlevel < 0.02f)
        {
            useShadows = false;
        }
    }

    if(drawAsVisSprite)
    {
        assert(p->isWall);

        Rend_WriteMaskedPoly(rvertices, rcolors, *p->wall.segLength, msA->material,
                           p->texOffset, p->blendMode, p->lightListIdx, p->glowing);

        R_FreeRendTexCoords(primaryCoords);
        R_FreeRendColors(rcolors);
        R_FreeRendTexCoords(interCoords);
        R_FreeRendTexCoords(modCoords);
        R_FreeRendTexCoords(shinyCoords);
        R_FreeRendColors(shinyColors);

        return false; // We HAD to use a vissprite, so it MUST not be opaque.
    }

    if(useLights)
    {
        // Render all lights projected onto this surface.
        renderlightprojectionparams_t params;

        params.rvertices = rvertices;
        params.numVertices = numVertices;
        params.realNumVertices = realNumVertices;
        params.lastIdx = 0;
        params.texTL = p->texTL;
        params.texBR = p->texBR;
        params.isWall = p->isWall;
        if(p->isWall)
        {
            params.wall.left.firstDiv = p->wall.left.firstDiv;
            params.wall.left.divCount = p->wall.left.divCount;
            params.wall.right.firstDiv = p->wall.right.firstDiv;
            params.wall.right.divCount = p->wall.right.divCount;
        }

        hasDynlights = (0 != Rend_RenderLightProjections(p->lightListIdx, &params));
    }

    if(useShadows)
    {
        // Render all shadows projected onto this surface.
        rendershadowprojectionparams_t params;

        params.rvertices = rvertices;
        params.numVertices = numVertices;
        params.realNumVertices = realNumVertices;
        params.texTL = p->texTL;
        params.texBR = p->texBR;
        params.isWall = p->isWall;
        if(p->isWall)
        {
            params.wall.left.firstDiv = p->wall.left.firstDiv;
            params.wall.left.divCount = p->wall.left.divCount;
            params.wall.right.firstDiv = p->wall.right.firstDiv;
            params.wall.right.divCount = p->wall.right.divCount;
        }

        Rend_RenderShadowProjections(p->shadowListIdx, &params);
    }

    // Setup the texture state for the render list's primitive writer.
    RL_LoadDefaultRtus();
    if(!(p->flags & RPF_SKYMASK))
    {
        mapRTUStateFromMaterialSnapshots(msA, inter, msB, p->texOffset, p->texScale);
    }

    // Write multiple polys depending on rend params.
    if(p->isWall && (p->wall.left.divCount > 2 || p->wall.right.divCount > 2))
    {
        const float bL = rvertices[0].pos[VZ];
        const float tL = rvertices[1].pos[VZ];
        const float bR = rvertices[2].pos[VZ];
        const float tR = rvertices[3].pos[VZ];
        rvertex_t quadVerts[4];
        ColorRawf quadColors[4];
        rtexcoord_t quadCoords[4];

        /**
         * Need to swap indices around into fans set the position
         * of the division vertices, interpolate texcoords and
         * color.
         */

        memcpy(quadVerts, rvertices, sizeof(rvertex_t) * 4);
        memcpy(quadCoords, primaryCoords, sizeof(rtexcoord_t) * 4);
        if(rcolors || shinyColors)
        {
            memcpy(quadColors, rcolors, sizeof(ColorRawf) * 4);
        }

        R_DivVerts(rvertices, quadVerts[0].pos, p->wall.left.firstDiv, p->wall.left.divCount, quadVerts[2].pos, p->wall.right.firstDiv, p->wall.right.divCount);
        R_DivTexCoords(primaryCoords, p->wall.left.firstDiv, p->wall.left.divCount, p->wall.right.firstDiv, p->wall.right.divCount, quadCoords, bL, tL, bR, tR);

        if(rcolors)
        {
            R_DivVertColors(rcolors, p->wall.left.firstDiv, p->wall.left.divCount, p->wall.right.firstDiv, p->wall.right.divCount, quadColors, bL, tL, bR, tR);
        }

        if(modCoords)
        {
            rtexcoord_t quadModCoords[4];
            memcpy(quadModCoords, modCoords, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(modCoords, p->wall.left.firstDiv, p->wall.left.divCount, p->wall.right.firstDiv, p->wall.right.divCount, quadModCoords, bL, tL, bR, tR);
        }

        if(interCoords)
        {
            rtexcoord_t quadCoords2[4];
            memcpy(quadCoords2, interCoords, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(interCoords, p->wall.left.firstDiv, p->wall.left.divCount, p->wall.right.firstDiv, p->wall.right.divCount, quadCoords2, bL, tL, bR, tR);
        }

        if(shinyCoords)
        {
            rtexcoord_t quadCoords3[4];
            memcpy(quadCoords3, shinyCoords, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(shinyCoords, p->wall.left.firstDiv, p->wall.left.divCount, p->wall.right.firstDiv, p->wall.right.divCount, quadCoords3, bL, tL, bR, tR);
        }

        if(shinyColors)
        {
            ColorRawf quadShinyColors[4];
            memcpy(quadShinyColors, shinyColors, sizeof(ColorRawf) * 4);
            R_DivVertColors(shinyColors, p->wall.left.firstDiv, p->wall.left.divCount, p->wall.right.firstDiv, p->wall.right.divCount, quadShinyColors, bL, tL, bR, tR);
        }

        RL_AddPolyWithCoordsModulationReflection(PT_FAN, p->flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
            1 + p->wall.right.divCount, rvertices + 1 + p->wall.left.divCount, rcolors? rcolors + 1 + p->wall.left.divCount : NULL,
            primaryCoords + 1 + p->wall.left.divCount, interCoords? interCoords + 1 + p->wall.left.divCount : NULL,
            modTex, &modColor, modCoords? modCoords + 1 + p->wall.left.divCount : NULL,
            shinyColors + 1 + p->wall.left.divCount, shinyCoords? shinyCoords + 1 + p->wall.left.divCount : NULL,
            primaryCoords + 1 + p->wall.left.divCount);

        RL_AddPolyWithCoordsModulationReflection(PT_FAN, p->flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
            1 + p->wall.left.divCount, rvertices, rcolors,
            primaryCoords, interCoords,
            modTex, &modColor, modCoords,
            shinyColors, shinyCoords, primaryCoords);
    }
    else
    {
        RL_AddPolyWithCoordsModulationReflection(p->isWall? PT_TRIANGLE_STRIP : PT_FAN, p->flags | (hasDynlights? RPF_HAS_DYNLIGHTS : 0),
            numVertices, rvertices, rcolors,
            primaryCoords, interCoords,
            modTex, &modColor, modCoords,
            shinyColors, shinyCoords, primaryCoords);
    }

    R_FreeRendTexCoords(primaryCoords);
    R_FreeRendTexCoords(interCoords);
    R_FreeRendTexCoords(modCoords);
    R_FreeRendTexCoords(shinyCoords);
    R_FreeRendColors(rcolors);
    R_FreeRendColors(shinyColors);

    return skyMaskedMaterial || !(p->alpha < 1 || !msA->isOpaque || p->blendMode > 0);
}

static void renderPlane(BspLeaf* bspLeaf, planetype_t type, coord_t height,
    vec3f_t tangent, vec3f_t bitangent, vec3f_t normal,
    material_t* inMat, const float sufColor[4], blendmode_t blendMode,
    vec3d_t texTL, vec3d_t texBR,
    float const texOffset[2], float const texScale[2],
    boolean skyMasked,
    boolean addDLights, boolean addMobjShadows,
    uint subelementIndex,
    int texMode /*tmp*/)
{
    float inter = 0;
    rendworldpoly_params_t params;
    uint numVertices;
    rvertex_t* rvertices;
    boolean blended = false;
    Sector* sec = bspLeaf->sector;
    material_t* mat = NULL;
    const materialsnapshot_t* msA = NULL, *msB = NULL;

    memset(&params, 0, sizeof(params));

    params.flags = RPF_DEFAULT;
    params.isWall = false;
    params.mapObject = (runtime_mapdata_header_t*)bspLeaf;
    params.subelementIndex = subelementIndex;
    params.texTL = texTL;
    params.texBR = texBR;
    params.sectorLightLevel = sec->lightLevel;
    params.sectorLightColor = R_GetSectorLightColor(sec);
    params.surfaceLightLevelDL = params.surfaceLightLevelDR = 0;
    params.surfaceColor = sufColor;
    params.texOffset = texOffset;
    params.texScale = texScale;

    if(skyMasked)
    {
        // In devRendSkyMode mode we render all polys destined for the
        // skymask as regular world polys (with a few obvious properties).
        if(devRendSkyMode)
        {
            params.blendMode = BM_NORMAL;
            params.glowing = 1;
            mat = inMat;
        }
        else
        {   // We'll mask this.
            params.flags |= RPF_SKYMASK;
        }
    }
    else
    {
        mat = inMat;

        if(type != PLN_MID)
        {
            params.blendMode = BM_NORMAL;
            params.alpha = 1;
        }
        else
        {
            if(blendMode == BM_NORMAL && noSpriteTrans)
                params.blendMode = BM_ZEROALPHA; // "no translucency" mode
            else
                params.blendMode = blendMode;
            params.alpha = sufColor[CA];
        }
    }

    Rend_BuildBspLeafPlaneGeometry(bspLeaf, (type == PLN_CEILING), height,
                                   &rvertices, &numVertices);

    if(!(params.flags & RPF_SKYMASK))
    {
        // Smooth Texture Animation?
        if(smoothTexAnim)
            blended = true;

        inter = getMaterialSnapshots(&msA, blended? &msB : NULL, mat);

        if(texMode != 2)
        {
            params.glowing = msA->glowing;
        }
        else
        {
            Surface* suf = &bspLeaf->sector->planes[subelementIndex]->surface;
            const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
                MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
            const materialsnapshot_t* ms = Materials_Prepare(suf->material, spec, true);
            params.glowing = ms->glowing;
        }

        // Dynamic lights?
        if(addDLights && params.glowing < 1 && !(!useDynLights && !useWallGlow))
        {
            params.lightListIdx = LO_ProjectToSurface((PLF_NO_PLANE | (type == PLN_FLOOR? PLF_TEX_FLOOR : PLF_TEX_CEILING)), bspLeaf, 1,
                params.texTL, params.texBR, tangent, bitangent, normal);
        }

        // Mobj shadows?
        if(addMobjShadows && params.glowing < 1 && Rend_MobjShadowsEnabled())
        {
            // Glowing planes inversely diminish shadow strength.
            params.shadowListIdx = R_ProjectShadowsToSurface(bspLeaf, 1 - params.glowing,
                params.texTL, params.texBR, tangent, bitangent, normal);
        }
    }

    renderWorldPoly(rvertices, numVertices, &params, msA, inter, blended? msB : NULL);

    R_FreeRendVertices(rvertices);
}

static void Rend_RenderPlane(planetype_t type, coord_t height,
    const_pvec3f_t _tangent, const_pvec3f_t _bitangent, const_pvec3f_t _normal,
    material_t* inMat, float const sufColor[4], blendmode_t blendMode,
    float const texOffset[2], float const texScale[2],
    boolean skyMasked, boolean addDLights, boolean addMobjShadows,
    uint subelementIndex,
    int texMode /*tmp*/, boolean clipBackFacing)
{
    BspLeaf* bspLeaf = currentBspLeaf;
    vec3f_t vec, tangent, bitangent, normal;

    // Must have a visible surface.
    if(!inMat || !Material_IsDrawable(inMat)) return;

    V3f_Set(vec, vOrigin[VX] - bspLeaf->midPoint[VX], vOrigin[VZ] - bspLeaf->midPoint[VY], vOrigin[VY] - height);

    /**
     * Flip surface tangent space vectors according to the z positon of the viewer relative
     * to the plane height so that the plane is always visible.
     */
    V3f_Copy(tangent, _tangent);
    V3f_Copy(bitangent, _bitangent);
    V3f_Copy(normal, _normal);

    // Don't bother with planes facing away from the camera.
    if(!(clipBackFacing && !(V3f_DotProduct(vec, normal) < 0)))
    {
        coord_t texTL[3], texBR[3];

        // Set the texture origin, Y is flipped for the ceiling.
        V3d_Set(texTL, bspLeaf->aaBox.minX,
                       bspLeaf->aaBox.arvec2[type == PLN_FLOOR? 1 : 0][VY], height);
        V3d_Set(texBR, bspLeaf->aaBox.maxX,
                       bspLeaf->aaBox.arvec2[type == PLN_FLOOR? 0 : 1][VY], height);

        renderPlane(bspLeaf, type, height, tangent, bitangent, normal, inMat,
                    sufColor, blendMode, texTL, texBR, texOffset, texScale,
                    skyMasked, addDLights, addMobjShadows, subelementIndex, texMode);
    }
}

static void nearBlend(float* alpha, HEdge* hedge,
    walldivnode_t* bottomLeft, walldivnode_t* topLeft, walldivnode_t* bottomRight, walldivnode_t* topRight,
    const coord_t* nearOrigin, coord_t nearRadius)
{
    if(!alpha) return;

    DENG_UNUSED(topLeft);
    DENG_UNUSED(bottomRight);

    if(nearOrigin[VZ] > WallDivNode_Height(bottomLeft) &&
       nearOrigin[VZ] < WallDivNode_Height(topRight))
    {
        LineDef* line = hedge->lineDef;
        vec2d_t result;
        double pos = V2d_ProjectOnLine(result, nearOrigin, line->L_v1origin, line->direction);

        if(pos > 0 && pos < 1)
        {
            coord_t distance, minDistance = nearRadius;
            coord_t delta[2];

            V2d_Subtract(delta, nearOrigin, result);
            distance = M_ApproxDistance(delta[VX], delta[VY]);

            if(distance < minDistance)
            {
                // Fade it out the closer the viewPlayer gets and clamp.
                *alpha = ((*alpha) / minDistance) * distance;
                *alpha = MINMAX_OF(0, *alpha, 1);
            }
        }
    }
}

/**
 * @defgroup rendHEdgeFlags
 * Flags for rendHEdgeSection()
 */
///@{
#define RHF_ADD_DYNLIGHTS       0x01 ///< Write geometry for dynamic lights.
#define RHF_ADD_DYNSHADOWS      0x02 ///< Write geometry for dynamic (mobj) shadows.
#define RHF_ADD_RADIO           0x04 ///< Write geometry for faked radiosity.
#define RHF_VIEWER_NEAR_BLEND   0x08 ///< Alpha-blend geometry when viewer is near.
///@}

static boolean rendHEdgeSection(HEdge* hedge, SideDefSection section, int flags,
    walldivnode_t* bottomLeft, walldivnode_t* topLeft, walldivnode_t* bottomRight, walldivnode_t* topRight,
    float lightLevel, const float* lightColor, float const matOffset[2])
{
    SideDef* frontSide = HEDGE_SIDEDEF(hedge);
    Surface* surface = &frontSide->SW_surface(section);
    boolean opaque = true;
    float alpha;

    assert(Surface_IsDrawable(surface));

    if(WallDivNode_Height(bottomLeft)  >= WallDivNode_Height(topLeft) &&
       WallDivNode_Height(bottomRight) >= WallDivNode_Height(topRight)) return true;

    alpha = (section == SS_MIDDLE? surface->rgba[3] : 1.0f);

    /**
     * Can the player walk through this surface?
     * If the player is close enough we should NOT add a solid hedge otherwise HOM
     * would occur when they are directly on top of the line (e.g., passing through
     * an opaque waterfall).
     */
    if(section == SS_MIDDLE && (flags & RHF_VIEWER_NEAR_BLEND))
    {
        const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

        nearBlend(&alpha, hedge, bottomLeft, topLeft, bottomRight, topRight,
                  viewData->current.origin, 16 *.8f);

        if(alpha < 1)
            opaque = false;
    }

    if(alpha > 0)
    {
        uint lightListIdx = 0, shadowListIdx = 0;
        vec3d_t texTL, texBR;
        float texScale[2], inter = 0;
        float llDeltaL, llDeltaR;
        material_t* mat = NULL;
        int rpFlags = RPF_DEFAULT;
        boolean isTwoSided = (hedge->lineDef && hedge->lineDef->L_frontsidedef && hedge->lineDef->L_backsidedef)? true:false;
        blendmode_t blendMode = BM_NORMAL;
        const float* color = NULL, *color2 = NULL;
        const materialsnapshot_t* msA = NULL, *msB = NULL;

        texScale[0] = ((surface->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        texScale[1] = ((surface->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        V2d_Copy(texTL,  hedge->HE_v1origin);
        texTL[VZ] =  WallDivNode_Height(topLeft);

        V2d_Copy(texBR, hedge->HE_v2origin);
        texBR[VZ] = WallDivNode_Height(bottomRight);

        // Determine which Material to use.
        mat = chooseHEdgeMaterial(hedge, section);

        if(Material_IsSkyMasked(mat) && !devRendSkyMode)
        {
            // We'll mask this.
            rpFlags |= RPF_SKYMASK;
        }

        // Fill in the remaining params data.
        if(!(rpFlags & RPF_SKYMASK))
        {
            blendMode = SideDef_SurfaceBlendMode(HEDGE_SIDEDEF(hedge), section);

            if(surface->inFlags & SUIF_NO_RADIO)
                flags &= ~RHF_ADD_RADIO;

            if(mat)
            {
                // Smooth Texture Animation?
                inter = getMaterialSnapshots(&msA, smoothTexAnim? &msB : NULL, mat);

                // Dynamic Lights?
                if((flags & RHF_ADD_DYNLIGHTS) &&
                   msA->glowing < 1 && !(!useDynLights && !useWallGlow))
                {
                    lightListIdx = LO_ProjectToSurface(((section == SS_MIDDLE && isTwoSided)? PLF_SORT_LUMINOSITY_DESC : 0), currentBspLeaf, 1,
                        texTL, texBR, HEDGE_SIDEDEF(hedge)->SW_middletangent, HEDGE_SIDEDEF(hedge)->SW_middlebitangent, HEDGE_SIDEDEF(hedge)->SW_middlenormal);
                }

                // Dynamic shadows?
                if((flags & RHF_ADD_DYNSHADOWS) &&
                   msA->glowing < 1 && Rend_MobjShadowsEnabled())
                {
                    // Glowing planes inversely diminish shadow strength.
                    shadowListIdx = R_ProjectShadowsToSurface(currentBspLeaf, 1 - msA->glowing, texTL, texBR,
                        HEDGE_SIDEDEF(hedge)->SW_middletangent, HEDGE_SIDEDEF(hedge)->SW_middlebitangent, HEDGE_SIDEDEF(hedge)->SW_middlenormal);
                }

                if(msA->glowing > 0)
                    flags &= ~RHF_ADD_RADIO;

                SideDef_SurfaceColors(HEDGE_SIDEDEF(hedge), section, &color2, &color);
            }
        }

        wallEdgeLightLevelDeltas(hedge, section, &llDeltaL, &llDeltaR);

        {
            const boolean skyMask = !!(rpFlags & RPF_SKYMASK);
            const boolean addFakeRadio = !!(flags & RHF_ADD_RADIO);
            boolean isTwosidedMiddle = (section == SS_MIDDLE && isTwoSided);
            rendworldpoly_params_t params;
            SideDef* side = (hedge->lineDef? HEDGE_SIDEDEF(hedge) : NULL);
            rvertex_t* rvertices;
            walldivs_t* leftWallDivs = WallDivNode_WallDivs(bottomLeft);
            walldivs_t* rightWallDivs = WallDivNode_WallDivs(bottomRight);

            // Init the params.
            memset(&params, 0, sizeof(params));

            params.flags = RPF_DEFAULT | (skyMask? RPF_SKYMASK : 0);
            params.isWall = true;
            params.wall.segLength = &hedge->length;
            params.alpha = (alpha < 0? 1 : alpha);
            params.mapObject = (runtime_mapdata_header_t*)hedge;
            params.subelementIndex = (uint)section;
            params.texTL = texTL;
            params.texBR = texBR;
            params.sectorLightLevel = lightLevel;
            params.surfaceLightLevelDL = llDeltaL;
            params.surfaceLightLevelDR = llDeltaR;
            params.sectorLightColor = lightColor;
            params.surfaceColor = color;
            params.wall.surfaceColor2 = color2;
            params.glowing = msA? msA->glowing : 0;
            params.blendMode = blendMode;
            params.texOffset = matOffset;
            params.texScale = texScale;
            params.lightListIdx = lightListIdx;
            params.shadowListIdx = shadowListIdx;
            params.wall.left.firstDiv = bottomLeft;
            params.wall.left.divCount = WallDivs_Size(leftWallDivs);
            params.wall.right.firstDiv = topRight;
            params.wall.right.divCount = WallDivs_Size(rightWallDivs);

            // Allocate enough vertices for the divisions too.
            if(WallDivs_Size(leftWallDivs) > 2 || WallDivs_Size(rightWallDivs) > 2)
            {
                // Use two fans.
                rvertices = R_AllocRendVertices(1 + WallDivs_Size(leftWallDivs) + 1 + WallDivs_Size(rightWallDivs));
            }
            else
            {
                // Use a quad.
                rvertices = R_AllocRendVertices(4);
            }

            // Vertex coords.
            // Bottom Left.
            V2f_Copyd(rvertices[0].pos, hedge->HE_v1origin);
            rvertices[0].pos[VZ] = (float)WallDivNode_Height(bottomLeft);

            // Top Left.
            V2f_Copyd(rvertices[1].pos, hedge->HE_v1origin);
            rvertices[1].pos[VZ] = (float)WallDivNode_Height(topLeft);

            // Bottom Right.
            V2f_Copyd(rvertices[2].pos, hedge->HE_v2origin);
            rvertices[2].pos[VZ] = (float)WallDivNode_Height(bottomRight);

            // Top Right.
            V2f_Copyd(rvertices[3].pos, hedge->HE_v2origin);
            rvertices[3].pos[VZ] = (float)WallDivNode_Height(topRight);

            // Draw this hedge.
            if(renderWorldPoly(rvertices, 4, &params, msA, inter, msB))
            {
                // Drawn poly was opaque.
                // Render Fakeradio polys for this hedge?
                if(!(params.flags & RPF_SKYMASK) && addFakeRadio)
                {
                    rendsegradio_params_t radioParams;
                    float ll;

                    Rend_RadioUpdateLinedef(hedge->lineDef, hedge->side);

                    radioParams.linedefLength = &hedge->lineDef->length;
                    radioParams.botCn = side->bottomCorners;
                    radioParams.topCn = side->topCorners;
                    radioParams.sideCn = side->sideCorners;
                    radioParams.spans = side->spans;
                    radioParams.segOffset = &hedge->offset;
                    radioParams.segLength = &hedge->length;
                    radioParams.frontSec = hedge->sector;
                    radioParams.wall.left.firstDiv = params.wall.left.firstDiv;
                    radioParams.wall.left.divCount = params.wall.left.divCount;
                    radioParams.wall.right.firstDiv = params.wall.right.firstDiv;
                    radioParams.wall.right.divCount = params.wall.right.divCount;

                    if(!isTwosidedMiddle && !(hedge->twin && !HEDGE_SIDEDEF(hedge->twin)))
                    {
                        radioParams.backSec = HEDGE_BACK_SECTOR(hedge);
                    }
                    else
                    {
                        radioParams.backSec = NULL;
                    }

                    /// @kludge Revert the vertex coords as they may have been changed
                    ///         due to height divisions.

                    // Bottom Left.
                    V2f_Copyd(rvertices[0].pos, hedge->HE_v1origin);
                    rvertices[0].pos[VZ] = (float)WallDivNode_Height(bottomLeft);

                    // Top Left.
                    V2f_Copyd(rvertices[1].pos, hedge->HE_v1origin);
                    rvertices[1].pos[VZ] = (float)WallDivNode_Height(topLeft);

                    // Bottom Right.
                    V2f_Copyd(rvertices[2].pos, hedge->HE_v2origin);
                    rvertices[2].pos[VZ] = (float)WallDivNode_Height(bottomRight);

                    // Top Right.
                    V2f_Copyd(rvertices[3].pos, hedge->HE_v2origin);
                    rvertices[3].pos[VZ] = (float)WallDivNode_Height(topRight);

                    // kludge end.

                    ll = lightLevel;
                    Rend_ApplyLightAdaptation(&ll);
                    if(ll > 0)
                    {
                        // Determine the shadow properties.
                        /// @todo Make cvars out of constants.
                        radioParams.shadowSize = 2 * (8 + 16 - ll * 16);
                        radioParams.shadowDark = Rend_RadioCalcShadowDarkness(ll);

                        if(radioParams.shadowSize > 0)
                        {
                            // Shadows are black.
                            radioParams.shadowRGB[CR] = radioParams.shadowRGB[CG] = radioParams.shadowRGB[CB] = 0;
                            Rend_RadioSegSection(rvertices, &radioParams);
                        }
                    }
                }
            }
            else
            {
                opaque = false; // Do not occlude this range.
            }

            R_FreeRendVertices(rvertices);
        }
    }

    return opaque;
}

/**
 * @param hedge  HEdge to draw wall surfaces for.
 */
static void Rend_RenderPolyobjHEdge(HEdge* hedge)
{
    BspLeaf* leaf = currentBspLeaf;
    Sector* frontSec = leaf->sector;
    Sector* backSec  = HEDGE_BACK_SECTOR(hedge);
    SideDef* frontSideDef = HEDGE_SIDEDEF(hedge);
    LineDef* line = hedge->lineDef;
    Surface* surface;

    if(!frontSideDef) return; // Huh?

    // We are only interested in the middle section.
    surface = &frontSideDef->SW_surface(SS_MIDDLE);
    if(surface->inFlags & SUIF_PVIS)
    {
        const int rhFlags = RHF_ADD_DYNLIGHTS|RHF_ADD_DYNSHADOWS|RHF_ADD_RADIO;
        walldivnode_t* bottomLeft, *topLeft, *bottomRight, *topRight;
        float matOffset[2];
        boolean opaque;

        if(!R_WallSectionEdges(hedge, SS_MIDDLE, frontSec, backSec,
                               &bottomLeft, &topLeft, &bottomRight, &topRight, matOffset)) return;

        matOffset[0] += (float)(hedge->offset);

        opaque = rendHEdgeSection(hedge, SS_MIDDLE, rhFlags,
                                  bottomLeft, topLeft, bottomRight, topRight,
                                  frontSec->lightLevel, R_GetSectorLightColor(frontSec), matOffset);

        if(opaque)
        {
            C_AddRangeFromViewRelPoints(hedge->HE_v1origin, hedge->HE_v2origin);
        }

        LineDef_ReportDrawn(line, viewPlayer - ddPlayers);
    }
}

static void Rend_RenderTwoSidedMiddle(HEdge* hedge)
{
    BspLeaf* leaf = currentBspLeaf;
    Sector* frontSec = leaf->sector;
    Sector* backSec  = HEDGE_BACK_SECTOR(hedge);
    SideDef* frontSideDef = HEDGE_SIDEDEF(hedge);
    LineDef* line = hedge->lineDef;
    Surface* surface;

    if(!frontSideDef) return; // Huh?

    // We are only interested in the middle section.
    surface = &frontSideDef->SW_surface(SS_MIDDLE);
    if(surface->inFlags & SUIF_PVIS)
    {
        int rhFlags = RHF_ADD_DYNLIGHTS|RHF_ADD_DYNSHADOWS|RHF_ADD_RADIO;

        walldivnode_t* bottomLeft, *topLeft, *bottomRight, *topRight;
        float matOffset[2];

        if(!R_WallSectionEdges(hedge, SS_MIDDLE, frontSec, backSec,
                               &bottomLeft, &topLeft, &bottomRight, &topRight, matOffset)) return;

        if((viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)) ||
           !(line->flags & DDLF_BLOCKING))
            rhFlags |= RHF_VIEWER_NEAR_BLEND;

        matOffset[0] += (float)(hedge->offset);

        rendHEdgeSection(hedge, SS_MIDDLE, rhFlags,
                         bottomLeft, topLeft, bottomRight, topRight,
                         frontSec->lightLevel, R_GetSectorLightColor(frontSec), matOffset);

        LineDef_ReportDrawn(line, viewPlayer - ddPlayers);
    }
}

static void Rend_ClearFrameFlagsForBspLeaf(BspLeaf* leaf)
{
    if(!leaf) return;

    if(leaf->hedge)
    {
        HEdge* hedge = leaf->hedge;
        do
        {
            hedge->frameFlags &= ~HEDGEINF_FACINGFRONT;
        } while((hedge = hedge->next) != leaf->hedge);
    }

    if(leaf->polyObj)
    {
        LineDef** lineIt = leaf->polyObj->lines;
        uint i;
        for(i = 0; i < leaf->polyObj->lineCount; ++i, lineIt++)
        {
            LineDef* line = *lineIt;
            HEdge* hedge = line->L_frontside.hedgeLeft;

            hedge->frameFlags &= ~HEDGEINF_FACINGFRONT;
        }
    }
}

static void Rend_MarkHEdgesFacingFront(BspLeaf* leaf)
{
    if(leaf->hedge)
    {
        HEdge* hedge = leaf->hedge;
        do
        {
            // Occlusions can only happen where two sectors contact.
            if(hedge->lineDef)
            {
                // Which way should it be facing?
                if(!(viewFacingDot(hedge->HE_v1origin, hedge->HE_v2origin) < 0))
                    hedge->frameFlags |= HEDGEINF_FACINGFRONT;

                Rend_MarkSideDefSectionsPVisible(hedge);
            }
        } while((hedge = hedge->next) != leaf->hedge);
    }

    if(leaf->polyObj)
    {
        LineDef** lineIt = leaf->polyObj->lines;
        uint i;
        for(i = 0; i < leaf->polyObj->lineCount; ++i, lineIt++)
        {
            LineDef* line = *lineIt;
            HEdge* hedge = line->L_frontside.hedgeLeft;

            // Which way should it be facing?
            if(!(viewFacingDot(hedge->HE_v1origin, hedge->HE_v2origin) < 0))
                hedge->frameFlags |= HEDGEINF_FACINGFRONT;

            Rend_MarkSideDefSectionsPVisible(hedge);
        }
    }
}

static void occludeFrontFacingSegsInBspLeaf(const BspLeaf* bspLeaf)
{
    if(bspLeaf->hedge)
    {
        HEdge* hedge = bspLeaf->hedge;
        do
        {
            if(!hedge->lineDef || !(hedge->frameFlags & HEDGEINF_FACINGFRONT)) continue;

            if(!C_CheckRangeFromViewRelPoints(hedge->HE_v1origin, hedge->HE_v2origin))
            {
                hedge->frameFlags &= ~HEDGEINF_FACINGFRONT;
            }
        } while((hedge = hedge->next) != bspLeaf->hedge);
    }

    if(bspLeaf->polyObj)
    {
        Polyobj* po = bspLeaf->polyObj;
        LineDef* line;
        HEdge* hedge;
        uint i;

        for(i = 0; i < po->lineCount; ++i)
        {
            line = po->lines[i];
            hedge = line->L_frontside.hedgeLeft;

            if(!(hedge->frameFlags & HEDGEINF_FACINGFRONT)) continue;

            if(!C_CheckRangeFromViewRelPoints(hedge->HE_v1origin, hedge->HE_v2origin))
            {
                hedge->frameFlags &= ~HEDGEINF_FACINGFRONT;
            }
        }
    }
}

static coord_t skyFixFloorZ(const Plane* frontFloor, const Plane* backFloor)
{
    DENG_UNUSED(backFloor);
    if(devRendSkyMode || P_IsInVoid(viewPlayer))
        return frontFloor->visHeight;
    return GameMap_SkyFixFloor(theMap);
}

static coord_t skyFixCeilZ(const Plane* frontCeil, const Plane* backCeil)
{
    DENG_UNUSED(backCeil);
    if(devRendSkyMode || P_IsInVoid(viewPlayer))
        return frontCeil->visHeight;
    return GameMap_SkyFixCeiling(theMap);
}

/**
 * @param hedge  HEdge from which to determine sky fix coordinates.
 * @param skyCap  Either SKYCAP_LOWER or SKYCAP_UPPER (SKYCAP_UPPER has precendence).
 * @param bottom  Z map space coordinate for the bottom of the skyfix written here.
 * @param top  Z map space coordinate for the top of the skyfix written here.
 */
static void skyFixZCoords(HEdge* hedge, int skyCap, coord_t* bottom, coord_t* top)
{
    const Sector* frontSec = hedge->sector;
    const Sector* backSec  = HEDGE_BACK_SECTOR(hedge);
    const Plane* ffloor = frontSec->SP_plane(PLN_FLOOR);
    const Plane* fceil  = frontSec->SP_plane(PLN_CEILING);
    const Plane* bceil  = backSec? backSec->SP_plane(PLN_CEILING) : NULL;
    const Plane* bfloor = backSec? backSec->SP_plane(PLN_FLOOR)   : NULL;

    if(!bottom && !top) return;
    if(bottom) *bottom = 0;
    if(top)    *top    = 0;

    if(skyCap & SKYCAP_UPPER)
    {
        if(top)    *top    = skyFixCeilZ(fceil, bceil);
        if(bottom) *bottom = MAX_OF((backSec && Surface_IsSkyMasked(&bceil->surface) )? bceil->visHeight  : fceil->visHeight,  ffloor->visHeight);
    }
    else
    {
        if(top)    *top    = MIN_OF((backSec && Surface_IsSkyMasked(&bfloor->surface))? bfloor->visHeight : ffloor->visHeight, fceil->visHeight);
        if(bottom) *bottom = skyFixFloorZ(ffloor, bfloor);
    }
}

/**
 * @param ignoreOpacity  @c true= do not consider Material opacity.
 * @return  @c true if this half-edge is considered "closed" (i.e.,
 *     there is no opening through which the back Sector can be seen).
 *     Tests consider all Planes which interface with this and the "middle"
 *     Material used on the relative front side (if any).
 */
static boolean hedgeBackClosedForSkyFix(const HEdge* hedge, boolean ignoreOpacity)
{
    LineDef* lineDef;
    Sector* frontSec;
    Sector* backSec;
    byte side;
    assert(hedge && hedge->lineDef);

    lineDef = hedge->lineDef;
    side = hedge->side;

    if(!lineDef->L_sidedef(side))   return false;
    if(!lineDef->L_sidedef(side^1)) return true;

    frontSec = lineDef->L_sector(side);
    backSec  = lineDef->L_sector(side^1);
    if(frontSec == backSec) return false; // Never.

    if(frontSec && backSec)
    {
        if(backSec->SP_floorvisheight >= backSec->SP_ceilvisheight)   return true;
        if(backSec->SP_ceilvisheight  <= frontSec->SP_floorvisheight) return true;
        if(backSec->SP_floorvisheight >= frontSec->SP_ceilvisheight)  return true;
    }

    return LineDef_MiddleMaterialCoversOpening(lineDef, side, ignoreOpacity);
}

/**
 * Determine which sky fixes are necessary for the specified @a hedge.
 * @param hedge  HEdge to be evaluated.
 * @param skyCap  The fixes we are interested in. @see skyCapFlags.
 * @return
 */
static int chooseHEdgeSkyFixes(HEdge* hedge, int skyCap)
{
    int fixes = 0;
    if(hedge && hedge->lineDef) // "minisegs" have no linedefs.
    {
        const Sector* frontSec = hedge->sector;
        const Sector* backSec  = HEDGE_BACK_SECTOR(hedge);

        if(!backSec || backSec != hedge->sector)
        {
            const boolean hasSkyFloor   = Surface_IsSkyMasked(&frontSec->SP_floorsurface);
            const boolean hasSkyCeiling = Surface_IsSkyMasked(&frontSec->SP_ceilsurface);

            if(hasSkyFloor || hasSkyCeiling)
            {
                const boolean hasClosedBack = hedgeBackClosedForSkyFix(hedge, true/*ignore opacity*/);

                // Lower fix?
                if(hasSkyFloor && (skyCap & SKYCAP_LOWER))
                {
                    const Plane* ffloor = frontSec->SP_plane(PLN_FLOOR);
                    const Plane* bfloor = backSec? backSec->SP_plane(PLN_FLOOR) : NULL;
                    const coord_t skyZ = skyFixFloorZ(ffloor, bfloor);

                    if(hasClosedBack || (!Surface_IsSkyMasked(&bfloor->surface) || devRendSkyMode || P_IsInVoid(viewPlayer)))
                    {
                        const Plane* floor = (bfloor && Surface_IsSkyMasked(&bfloor->surface) && ffloor->visHeight < bfloor->visHeight? bfloor : ffloor);
                        if(floor->visHeight > skyZ)
                            fixes |= SKYCAP_LOWER;
                    }
                }

                // Upper fix?
                if(hasSkyCeiling && (skyCap & SKYCAP_UPPER))
                {
                    const Plane* fceil = frontSec->SP_plane(PLN_CEILING);
                    const Plane* bceil = backSec? backSec->SP_plane(PLN_CEILING) : NULL;
                    const coord_t skyZ = skyFixCeilZ(fceil, bceil);

                    if(hasClosedBack || (!Surface_IsSkyMasked(&bceil->surface) || devRendSkyMode || P_IsInVoid(viewPlayer)))
                    {
                        const Plane* ceil = (bceil && Surface_IsSkyMasked(&bceil->surface) && fceil->visHeight > bceil->visHeight? bceil : fceil);
                        if(ceil->visHeight < skyZ)
                            fixes |= SKYCAP_UPPER;
                    }
                }
            }
        }
    }
    return fixes;
}

static __inline void Rend_BuildBspLeafSkyFixStripEdge(coord_t const vXY[2],
    coord_t v1Z, coord_t v2Z, float texS,
    rvertex_t* v1, rvertex_t* v2, rtexcoord_t* t1, rtexcoord_t* t2)
{
    if(v1)
    {
        assert(vXY);
        V2f_Copyd(v1->pos, vXY);
        v1->pos[VZ] = v1Z;
    }
    if(v2)
    {
        assert(vXY);
        V2f_Copyd(v2->pos, vXY);
        v2->pos[VZ] = v2Z;
    }
    if(t1)
    {
        t1->st[0] = texS;
        t1->st[1] = v2Z - v1Z;
    }
    if(t2)
    {
        t2->st[0] = texS;
        t2->st[1] = 0;
    }
}

/**
 * Vertex layout:
 *   1--3    2--0
 *   |  | or |  | if antiClockwise
 *   0--2    3--1
 */
static void Rend_BuildBspLeafSkyFixStripGeometry(BspLeaf* leaf, HEdge* startNode,
    HEdge* endNode, boolean antiClockwise, int skyCap,
    rvertex_t** verts, uint* vertsSize, rtexcoord_t** coords)
{
    HEdge* node;
    float texS;
    uint n;

    *vertsSize = 0;
    *verts = 0;

    if(!startNode || !endNode || !skyCap) return;

    // Count verts.
    if(startNode == endNode)
    {
        // Special case: the whole edge loop.
        *vertsSize += 2 * (leaf->hedgeCount + 1);
    }
    else
    {
        HEdge* afterEndNode = antiClockwise? endNode->prev : endNode->next;
        HEdge* node = startNode;
        do
        {
            *vertsSize += 2;
        } while((node = antiClockwise? node->prev : node->next) != afterEndNode);
    }

    // Build geometry.
    *verts = R_AllocRendVertices(*vertsSize);
    if(coords)
    {
        *coords = R_AllocRendTexCoords(*vertsSize);
    }

    node = startNode;
    texS = (float)(node->offset);
    n = 0;
    do
    {
        HEdge* hedge = (antiClockwise? node->prev : node);
        coord_t zBottom, zTop;

        skyFixZCoords(hedge, skyCap, &zBottom, &zTop);
        assert(zBottom < zTop);

        if(n == 0)
        {
            // Add the first edge.
            rvertex_t* v1 = &(*verts)[n + antiClockwise^0];
            rvertex_t* v2 = &(*verts)[n + antiClockwise^1];
            rtexcoord_t* t1 = coords? &(*coords)[n + antiClockwise^0] : NULL;
            rtexcoord_t* t2 = coords? &(*coords)[n + antiClockwise^1] : NULL;

            Rend_BuildBspLeafSkyFixStripEdge(node->HE_v1origin, zBottom, zTop, texS,
                                             v1, v2, t1, t2);

            if(coords)
            {
                texS += antiClockwise? -node->prev->length : hedge->length;
            }

            n += 2;
        }

        // Add the next edge.
        {
            rvertex_t* v1 = &(*verts)[n + antiClockwise^0];
            rvertex_t* v2 = &(*verts)[n + antiClockwise^1];
            rtexcoord_t* t1 = coords? &(*coords)[n + antiClockwise^0] : NULL;
            rtexcoord_t* t2 = coords? &(*coords)[n + antiClockwise^1] : NULL;

            Rend_BuildBspLeafSkyFixStripEdge((antiClockwise? node->prev : node->next)->HE_v1origin,
                                             zBottom, zTop, texS,
                                             v1, v2, t1, t2);

            if(coords)
            {
                texS += antiClockwise? -hedge->length : hedge->next->length;
            }

            n += 2;
        }
    } while((node = antiClockwise? node->prev : node->next) != endNode);
}

static void Rend_WriteBspLeafSkyFixStripGeometry(BspLeaf* leaf, HEdge* startNode,
    HEdge* endNode, boolean antiClockwise, int skyFix, material_t* material)
{
    const int rendPolyFlags = RPF_DEFAULT | (!devRendSkyMode? RPF_SKYMASK : 0);
    rtexcoord_t* coords = 0;
    rvertex_t* verts;
    uint vertsSize;

    Rend_BuildBspLeafSkyFixStripGeometry(leaf, startNode, endNode, antiClockwise, skyFix,
                                         &verts, &vertsSize, devRendSkyMode? &coords : NULL);

    if(!devRendSkyMode)
    {
        RL_AddPoly(PT_TRIANGLE_STRIP, rendPolyFlags, vertsSize, verts, NULL);
    }
    else
    {
        // Map RTU configuration from prepared MaterialSnapshot(s).
        const materialvariantspecification_t* spec = mapSurfaceMaterialSpec(GL_REPEAT, GL_REPEAT);
        const materialsnapshot_t* ms = Materials_Prepare(material, spec, true);

        RL_LoadDefaultRtus();
        RL_MapRtu(RTU_PRIMARY, &MSU(ms, MTU_PRIMARY));
        RL_AddPolyWithCoords(PT_TRIANGLE_STRIP, rendPolyFlags, vertsSize, verts, NULL, coords, NULL);
    }

    R_FreeRendVertices(verts);
    R_FreeRendTexCoords(coords);
}

/// @param skyFix  @ref skyCapFlags
static void Rend_WriteBspLeafSkyFixGeometry(BspLeaf* leaf, int skyFix)
{
    const boolean antiClockwise = false;
    HEdge* baseNode, *startNode, *node;
    coord_t startZBottom, startZTop;
    material_t* startMaterial;

    if(!leaf || !leaf->hedgeCount || !leaf->sector) return;
    if(!(skyFix & (SKYCAP_LOWER|SKYCAP_UPPER))) return;

    baseNode = leaf->hedge;

    // We may need to break the loop into multiple strips.
    startNode = 0;
    startZBottom = startZTop = 0;
    startMaterial = 0;

    for(node = baseNode;;)
    {
        HEdge* hedge = (antiClockwise? node->prev : node);
        boolean endStrip = false;
        boolean beginNewStrip = false;

        // Is a fix or two necessary for this hedge?
        if(chooseHEdgeSkyFixes(hedge, skyFix))
        {
            coord_t zBottom, zTop;
            material_t* skyMaterial = 0;

            skyFixZCoords(hedge, skyFix, &zBottom, &zTop);

            if(devRendSkyMode)
            {
                skyMaterial = hedge->sector->SP_planematerial(skyFix == SKYCAP_UPPER? PLN_CEILING : PLN_FLOOR);
            }

            if(zBottom >= zTop)
            {
                // End the current strip.
                endStrip = true;
            }
            else if(startNode && (!FEQUAL(zBottom, startZBottom) || !FEQUAL(zTop, startZTop) ||
                                  (devRendSkyMode && skyMaterial != startMaterial)))
            {
                // End the current strip and start another.
                endStrip = true;
                beginNewStrip = true;
            }
            else if(!startNode)
            {
                // A new strip begins.
                startNode = node;
                startZBottom = zBottom;
                startZTop = zTop;
                startMaterial = skyMaterial;
            }
        }
        else
        {
            // End the current strip.
            endStrip = true;
        }

        if(endStrip && startNode)
        {
            // We have complete strip; build and write it.
            Rend_WriteBspLeafSkyFixStripGeometry(leaf, startNode, node, antiClockwise,
                                                 skyFix, startMaterial);

            // End the current strip.
            startNode = 0;
        }

        // Start a new strip from this node?
        if(beginNewStrip) continue;

        // On to the next node.
        node = antiClockwise? node->prev : node->next;

        // Are we done?
        if(node == baseNode) break;
    }

    // Have we an unwritten strip? - build it.
    if(startNode)
    {
        Rend_WriteBspLeafSkyFixStripGeometry(leaf, startNode, baseNode, antiClockwise,
                                             skyFix, startMaterial);
    }
}

/**
 * Determine the HEdge from @a bspleaf whose vertex is suitable for use as the
 * center point of a trifan primitive.
 *
 * Note that we do not want any overlapping or zero-area (degenerate) triangles.
 *
 * We are assured by the node build process that BspLeaf->hedges has been ordered
 * by angle, clockwise starting from the smallest angle.
 *
 * @par Algorithm
 * <pre>For each vertex
 *    For each triangle
 *        if area is not greater than minimum bound, move to next vertex
 *    Vertex is suitable</pre>
 *
 * If a vertex exists which results in no zero-area triangles it is suitable for
 * use as the center of our trifan. If a suitable vertex is not found then the
 * center of BSP leaf should be selected instead (it will always be valid as
 * BSP leafs are convex).
 *
 * @return  The chosen node. Can be @a NULL in which case there was no suitable node.
 */
static HEdge* Rend_ChooseBspLeafFanBase(BspLeaf* leaf)
{
#define MIN_TRIANGLE_EPSILON  (0.1) ///< Area

    if(!leaf) return NULL;

    if(leaf->flags & BLF_UPDATE_FANBASE)
    {
        HEdge* firstNode = leaf->hedge;
        HEdge* fanBase = firstNode;

        if(leaf->hedgeCount > 3)
        {
            // Splines with higher vertex counts demand checking.
            coord_t const* base, *a, *b;

            // Search for a good base.
            do
            {
                HEdge* other = firstNode;

                base = fanBase->HE_v1origin;
                do
                {
                    // Test this triangle?
                    if(!(fanBase != firstNode && (other == fanBase || other == fanBase->prev)))
                    {
                        a = other->HE_v1origin;
                        b = other->next->HE_v1origin;

                        if(M_TriangleArea(base, a, b) <= MIN_TRIANGLE_EPSILON)
                        {
                            // No good. We'll move on to the next vertex.
                            base = NULL;
                        }
                    }

                    // On to the next triangle.
                } while(base && (other = other->next) != firstNode);

                if(!base)
                {
                    // No good. Select the next vertex and start over.
                    fanBase = fanBase->next;
                }
            } while(!base && fanBase != firstNode);

            // Did we find something suitable?
            if(!base) // No.
            {
                fanBase = NULL;
            }
        }
        //else Implicitly suitable (or completely degenerate...).

        leaf->fanBase = fanBase;
        leaf->flags &= ~BLF_UPDATE_FANBASE;
    }

    return leaf->fanBase;

#undef MIN_TRIANGLE_EPSILON
}

uint Rend_NumFanVerticesForBspLeaf(BspLeaf* leaf)
{
    if(!leaf) return 0;
    // Are we using a hedge vertex as the fan base?
    Rend_ChooseBspLeafFanBase(leaf);
    return leaf->hedgeCount + (leaf->fanBase? 0 : 2);
}

/**
 * Prepare the trifan rvertex_t buffer specified according to the edges of this
 * BSP leaf. If a fan base HEdge has been chosen it will be used as the center of
 * the trifan, else the mid point of this leaf will be used instead.
 *
 * @param bspLeaf  BspLeaf instance.
 * @param antiClockwise  @c true= wind vertices in anticlockwise order (else clockwise).
 * @param height  Z map space height coordinate to be set for each vertex.
 * @param verts  Built vertices are written here.
 * @param vertsSize  Number of built vertices is written here. Can be @c NULL.
 *
 * @return  Number of built vertices (same as written to @a vertsSize).
 */
static uint Rend_BuildBspLeafPlaneGeometry(BspLeaf* leaf, boolean antiClockwise,
    coord_t height, rvertex_t** verts, uint* vertsSize)
{
    HEdge* baseNode, *fanBase;
    uint n, totalVerts;

    if(!leaf || !verts) return 0;

    fanBase = Rend_ChooseBspLeafFanBase(leaf);
    baseNode = fanBase? fanBase : leaf->hedge;

    totalVerts = leaf->hedgeCount + (!fanBase? 2 : 0);
    *verts = R_AllocRendVertices(totalVerts);

    n = 0;
    if(!fanBase)
    {
        V2f_Copyd((*verts)[n].pos, leaf->midPoint);
        (*verts)[n].pos[VZ] = (float)height;
        n++;
    }

    // Add the vertices for each hedge.
    { HEdge* node = baseNode;
    do
    {
        V2f_Copyd((*verts)[n].pos, node->HE_v1origin);
        (*verts)[n].pos[VZ] = (float)height;
        n++;
    } while((node = antiClockwise? node->prev : node->next) != baseNode); }

    // The last vertex is always equal to the first.
    if(!fanBase)
    {
        V2f_Copyd((*verts)[n].pos, leaf->hedge->HE_v1origin);
        (*verts)[n].pos[VZ] = (float)height;
    }

    if(vertsSize) *vertsSize = totalVerts;
    return totalVerts;
}

/// @param skyFix  @ref skyCapFlags.
static void Rend_RenderSkyFix(int skyFix)
{
    BspLeaf* leaf = currentBspLeaf;
    //rvertex_t* verts;
    //uint numVerts;

    if(!leaf || !skyFix) return;

    Rend_WriteBspLeafSkyFixGeometry(leaf, skyFix/*, &verts, &numVerts*/);

    //RL_AddPoly(PT_TRIANGLE_STRIP, rendPolyFlags, numVerts, verts, NULL);
    //R_FreeRendVertices(verts);
}

/// @param skyCap  @ref skyCapFlags.
static void Rend_RenderSkyCap(int skyCap)
{
    BspLeaf* leaf = currentBspLeaf;
    rvertex_t* verts;
    uint numVerts;

    if(devRendSkyMode) return; // Caps are unnecessary (will be drawn as regular planes).
    if(!leaf || !skyCap) return;

    Rend_BuildBspLeafPlaneGeometry(leaf, !!(skyCap & SKYCAP_UPPER), R_SkyCapZ(leaf, skyCap),
                                   &verts, &numVerts);

    RL_AddPoly(PT_FAN, RPF_DEFAULT | RPF_SKYMASK, numVerts, verts, NULL);
    R_FreeRendVertices(verts);
}

/// @param skyCap  @ref skyCapFlags
static void Rend_RenderSkySurfaces(int skyCap)
{
    BspLeaf* leaf = currentBspLeaf;

    // Any work to do?
    if(!leaf || !leaf->hedgeCount || !leaf->sector || !R_SectorContainsSkySurfaces(leaf->sector)) return;

    // Sky caps are only necessary in sectors with sky-masked planes.
    if((skyCap & SKYCAP_LOWER) && !Surface_IsSkyMasked(&leaf->sector->SP_floorsurface))
        skyCap &= ~SKYCAP_LOWER;
    if((skyCap & SKYCAP_UPPER) && !Surface_IsSkyMasked(&leaf->sector->SP_ceilsurface))
        skyCap &= ~SKYCAP_UPPER;

    if(!skyCap) return;

    if(!devRendSkyMode)
    {
        // All geometry uses the same RTU write state.
        RL_LoadDefaultRtus();
    }

    // Lower?
    if(skyCap & SKYCAP_LOWER)
    {
        Rend_RenderSkyFix(SKYCAP_LOWER);
        Rend_RenderSkyCap(SKYCAP_LOWER);
    }

    // Upper?
    if(skyCap & SKYCAP_UPPER)
    {
        Rend_RenderSkyFix(SKYCAP_UPPER);
        Rend_RenderSkyCap(SKYCAP_UPPER);
    }
}

static material_t* chooseHEdgeMaterial(HEdge* hedge, SideDefSection section)
{
    SideDef* frontSide = HEDGE_SIDEDEF(hedge);
    Surface* surface = &frontSide->SW_surface(section);

    // Determine which Material to use.
    if(devRendSkyMode && HEDGE_BACK_SECTOR(hedge) &&
       ((section == SS_BOTTOM && Surface_IsSkyMasked(&hedge->sector->SP_floorsurface) &&
                                 Surface_IsSkyMasked(&HEDGE_BACK_SECTOR(hedge)->SP_floorsurface)) ||
        (section == SS_TOP    && Surface_IsSkyMasked(&hedge->sector->SP_ceilsurface) &&
                                 Surface_IsSkyMasked(&HEDGE_BACK_SECTOR(hedge)->SP_ceilsurface))))
    {
        // Geometry not normally rendered however we do so in dev sky mode.
        return hedge->sector->SP_planematerial(section == SS_TOP? PLN_CEILING : PLN_FLOOR);
    }

    if(renderTextures == 2)
    {
        // Lighting debug mode; render using System:gray.
        return Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":gray"));
    }

    if(!surface->material || ((surface->inFlags & SUIF_FIX_MISSING_MATERIAL) && devNoTexFix))
    {
        // Missing material debug mode; render using System:missing.
        return Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":missing"));
    }

    return surface->material;
}

static __inline void Rend_BuildBspLeafWallStripEdge(coord_t const vXY[2],
    coord_t v1Z, coord_t v2Z, float texS, float texT,
    const float* sectorLightColor, float sectorLightLevel, float surfaceLightLevelDelta,
    const float* surfaceColor, const float* surfaceColor2, float glowing, float alpha,
    void* mapObject, uint subelementIndex,
    rvertex_t* v1, rvertex_t* v2, ColorRawf* c1, ColorRawf* c2, rtexcoord_t* t1, rtexcoord_t* t2,
    rtexcoord_t* tb1, rtexcoord_t* tb2, rtexcoord_t* tc1, rtexcoord_t* tc2)
{
    if(v1)
    {
        assert(vXY);
        V2f_Copyd(v1->pos, vXY);
        v1->pos[VZ] = v1Z;
    }
    if(v2)
    {
        assert(vXY);
        V2f_Copyd(v2->pos, vXY);
        v2->pos[VZ] = v2Z;
    }

    // Vertex colors.
    if(c1 || c2)
    {
        if(levelFullBright || !(glowing < 1))
        {
            // Uniform color. Apply to all vertices.
            Rend_SetUniformColor(c1, 1, sectorLightLevel + (levelFullBright? 1 : glowing));
            Rend_SetUniformColor(c2, 1, sectorLightLevel + (levelFullBright? 1 : glowing));
        }
        else
        {
            // Non-uniform color.
            if(useBias)
            {
                // Do BIAS lighting for this poly.
                SB_LightVertices(c1, v1, 1, sectorLightLevel, mapObject, subelementIndex);
                SB_LightVertices(c2, v2, 1, sectorLightLevel, mapObject, subelementIndex);

                if(glowing > 0)
                {
                    c1->rgba[CR] = MINMAX_OF(0, c1->rgba[CR] + glowing, 1);
                    c1->rgba[CG] = MINMAX_OF(0, c1->rgba[CG] + glowing, 1);
                    c1->rgba[CB] = MINMAX_OF(0, c1->rgba[CB] + glowing, 1);

                    c2->rgba[CR] = MINMAX_OF(0, c2->rgba[CR] + glowing, 1);
                    c2->rgba[CG] = MINMAX_OF(0, c2->rgba[CG] + glowing, 1);
                    c2->rgba[CB] = MINMAX_OF(0, c2->rgba[CB] + glowing, 1);
                }
            }
            else
            {
                float ll = MINMAX_OF(0, sectorLightLevel + surfaceLightLevelDelta + glowing, 1);

                // Calculate the color for each vertex, blended with plane color?
                if(surfaceColor[0] < 1 || surfaceColor[1] < 1 || surfaceColor[2] < 1)
                {
                    float vColor[4];

                    // Blend sector light+color+surfacecolor
                    vColor[CR] = surfaceColor[CR] * sectorLightColor[CR];
                    vColor[CG] = surfaceColor[CG] * sectorLightColor[CG];
                    vColor[CB] = surfaceColor[CB] * sectorLightColor[CB];
                    vColor[CA] = 1;

                    Rend_LightVertices(1, c1, v1, ll, vColor);
                    Rend_LightVertices(1, c2, v2, ll, vColor);
                }
                else
                {
                    // Use sector light+color only.
                    Rend_LightVertices(1, c1, v1, ll, sectorLightColor);
                    Rend_LightVertices(1, c2, v2, ll, sectorLightColor);
                }

                // Bottom color (if different from top)?
                if(surfaceColor2)
                {
                    float vColor[4];

                    // Blend sector light+color+surfacecolor
                    vColor[CR] = surfaceColor2[CR] * sectorLightColor[CR];
                    vColor[CG] = surfaceColor2[CG] * sectorLightColor[CG];
                    vColor[CB] = surfaceColor2[CB] * sectorLightColor[CB];
                    vColor[CA] = 1;

                    Rend_LightVertex(c1, v1, ll, vColor);
                    Rend_LightVertex(c2, v2, ll, vColor);
                }
            }

            Rend_VertexColorsApplyTorchLight(c1, v1, 1);
            Rend_VertexColorsApplyTorchLight(c2, v2, 1);
        }

        // Apply uniform alpha.
        Rend_SetAlpha(c1, 1, alpha);
        Rend_SetAlpha(c2, 1, alpha);
    }

    // Primary texture coordinates.
    if(t1)
    {
        t1->st[0] = texS;
        t1->st[1] = texT + (v2Z - v1Z);
    }
    if(t2)
    {
        t2->st[0] = texS;
        t2->st[1] = texT;
    }

    // Blend primary texture coordinates.
    if(tb1)
    {
        tb1->st[0] = texS;
        tb1->st[1] = texT + (v2Z - v1Z);
    }
    if(tb2)
    {
        tb2->st[0] = texS;
        tb2->st[1] = texT;
    }

    // Shiny texture coordinates.
    /*if(tc1)
    {
        quadShinyTexCoords(shinyCoords, &verts[1], &verts[2], hedgeLength);
    }
    if(tc2)
    {
        quadShinyTexCoords(shinyCoords, &verts[1], &verts[2], hedgeLength);
    }*/
}

static void wallEdgeLightLevelDeltas(HEdge* hedge, SideDefSection section, float* deltaL, float* deltaR)
{
    BspLeaf* leaf = currentBspLeaf;
    Sector* frontSec = leaf->sector;
    Sector* backSec  = HEDGE_BACK_SECTOR(hedge);
    const boolean oneSided = isOneSided(hedge, frontSec, backSec);
    SideDef* frontSideDef = HEDGE_SIDEDEF(hedge);
    Surface* surface = &frontSideDef->SW_surface(section);
    float left = 0, right = 0, diff;

    // Do not apply an angle based lighting delta if this surface's material
    // has been chosen as a HOM fix (we must remain consistent with the lighting
    // applied to the back plane (on this half-edge's back side)).
    if(!(frontSideDef && !oneSided && section != SS_MIDDLE && (surface->inFlags & SUIF_FIX_MISSING_MATERIAL)))
    {
        LineDef_LightLevelDelta(hedge->lineDef, hedge->side, &left, &right);
    }

    // Linear interpolation of the linedef light deltas to the edges of the hedge.
    diff = right - left;
    right = left + ((hedge->offset + hedge->length) / hedge->lineDef->length) * diff;
    left += (hedge->offset / hedge->lineDef->length) * diff;

    if(deltaL) *deltaL = left;
    if(deltaR) *deltaR = right;
}

static boolean wallGeometryCoversOpening(HEdge* hedge)
{
    BspLeaf* leaf = currentBspLeaf;
    Sector* frontSec = leaf->sector;
    Sector* backSec  = HEDGE_BACK_SECTOR(hedge);
    const boolean oneSided = isOneSided(hedge, frontSec, backSec);
    SideDef* frontSideDef = HEDGE_SIDEDEF(hedge);
    Plane* ffloor, *fceil, *bfloor, *bceil;
    LineDef* line = hedge->lineDef;
    boolean opaque = false;

    if(!line) return false;

    if(oneSided)
    {
        return true; // Always.
    }

    if(LineDef_MiddleMaterialCoversOpening(hedge->lineDef, hedge->side,
                                           false/*do not ignore material alpha*/))
    {
        return true;
    }

    if(backSec == frontSec &&
       !frontSideDef->SW_topmaterial && !frontSideDef->SW_bottommaterial &&
       !frontSideDef->SW_middlematerial)
       return false; // Ugh... an obvious wall hedge hack. Best take no chances...

    ffloor = leaf->sector->SP_floor;
    fceil  = leaf->sector->SP_ceil;
    bfloor = backSec->SP_floor;
    bceil  = backSec->SP_ceil;

    // Can we make this a solid segment in the clipper?
    if(line->L_frontsector == line->L_backsector)
        return false; // Never.

    if(!opaque) // We'll have to determine whether we can...
    {
        if(backSec == frontSec)
        {
            // An obvious hack, what to do though??
        }
        else if((bceil->visHeight <= ffloor->visHeight &&
                    ((frontSideDef->SW_topmaterial /* && !(frontSide->flags & SDF_MIDTEXUPPER)*/) ||
                     (frontSideDef->SW_middlematerial))) ||
                (bfloor->visHeight >= fceil->visHeight &&
                    (frontSideDef->SW_bottommaterial || frontSideDef->SW_middlematerial)))
        {
            // A closed gap?
            if(FEQUAL(fceil->visHeight, bfloor->visHeight))
            {
                return (bceil->visHeight <= bfloor->visHeight) ||
                        !(Surface_IsSkyMasked(&fceil->surface) &&
                          Surface_IsSkyMasked(&bceil->surface));
            }

            if(FEQUAL(ffloor->visHeight, bceil->visHeight))
            {
                return (bfloor->visHeight >= bceil->visHeight) ||
                        !(Surface_IsSkyMasked(&ffloor->surface) &&
                          Surface_IsSkyMasked(&bfloor->surface));
            }

            return true;
        }

        /// @todo Is this still necessary?
        if(bceil->visHeight <= bfloor->visHeight ||
           (!(bceil->visHeight - bfloor->visHeight > 0) && bfloor->visHeight > ffloor->visHeight && bceil->visHeight < fceil->visHeight &&
           (frontSideDef->SW_topmaterial /*&& !(frontSide->flags & SDF_MIDTEXUPPER)*/) &&
           (frontSideDef->SW_bottommaterial)))
        {
            // A zero height back segment
            return true;
        }
    }

    return false;
}

/**
 * Vertex layout:
 *   1--3    2--0
 *   |  | or |  | if antiClockwise
 *   0--2    3--1
 */
static void Rend_BuildBspLeafWallStripGeometry(BspLeaf* leaf, HEdge* startNode,
    HEdge* endNode, boolean antiClockwise, SideDefSection section,
    const float* sectorLightColor, float sectorLightLevel, float glowing, float alpha,
    const float* surfaceColor, const float* surfaceColor2, float matOffset[2],
    uint* vertsSize, rvertex_t** verts, ColorRawf** colors, rtexcoord_t** coords, rtexcoord_t** interCoords,
    ColorRawf** shinyColors, rtexcoord_t** shinyCoords)
{
    HEdge* node;
    float texS;
    uint n;

    *vertsSize = 0;
    *verts = 0;

    if(!startNode || !endNode) return;

    // Count verts.
    if(startNode == endNode)
    {
        // Special case: the whole edge loop.
        *vertsSize += 2 * (leaf->hedgeCount + 1);
    }
    else
    {
        HEdge* afterEndNode = antiClockwise? endNode->prev : endNode->next;
        HEdge* node = startNode;
        do
        {
            *vertsSize += 2;
        } while((node = antiClockwise? node->prev : node->next) != afterEndNode);
    }

    // Build geometry.
    *verts = R_AllocRendVertices(*vertsSize);
    if(colors)
    {
        *colors = R_AllocRendColors(*vertsSize);
    }
    if(coords)
    {
        *coords = R_AllocRendTexCoords(*vertsSize);
    }
    if(interCoords)
    {
        *interCoords = R_AllocRendTexCoords(*vertsSize);
    }
    if(shinyColors)
    {
        *shinyColors = R_AllocRendColors(*vertsSize);
    }
    if(shinyCoords)
    {
        *shinyCoords = R_AllocRendTexCoords(*vertsSize);
    }

    node = startNode;
    texS = matOffset[0] + (float)(node->offset);
    n = 0;
    do
    {
        HEdge* hedge = (antiClockwise? node->prev : node);
        Sector* frontSec = leaf->sector;
        Sector* backSec  = HEDGE_BACK_SECTOR(hedge);
        walldivnode_t* bottom, *top;

        R_WallSectionEdge(hedge, section, 0/*left edge */, frontSec, backSec,
                          &bottom, &top, NULL);

        assert(WallDivNode_Height(bottom) < WallDivNode_Height(top));

        LineDef_ReportDrawn(hedge->lineDef, viewPlayer - ddPlayers);

        if(n == 0)
        {
            // Add the first edge.
            rvertex_t* v1 = &(*verts)[n + antiClockwise^0];
            rvertex_t* v2 = &(*verts)[n + antiClockwise^1];
            ColorRawf* c1 = colors? &(*colors)[n + antiClockwise^0] : NULL;
            ColorRawf* c2 = colors? &(*colors)[n + antiClockwise^1] : NULL;
            rtexcoord_t* t1 = coords? &(*coords)[n + antiClockwise^0] : NULL;
            rtexcoord_t* t2 = coords? &(*coords)[n + antiClockwise^1] : NULL;
            rtexcoord_t* tb1 = interCoords? &(*interCoords)[n + antiClockwise^0] : NULL;
            rtexcoord_t* tb2 = interCoords? &(*interCoords)[n + antiClockwise^1] : NULL;
            rtexcoord_t* tc1 = shinyCoords? &(*shinyCoords)[n + antiClockwise^0] : NULL;
            rtexcoord_t* tc2 = shinyCoords? &(*shinyCoords)[n + antiClockwise^1] : NULL;
            float deltaL, deltaR;

            wallEdgeLightLevelDeltas(hedge, section, &deltaL, &deltaR);

            Rend_BuildBspLeafWallStripEdge(node->HE_v1origin,
                                           WallDivNode_Height(bottom), WallDivNode_Height(top), texS, matOffset[1],
                                           sectorLightColor, sectorLightLevel, antiClockwise? deltaR : deltaL, surfaceColor, surfaceColor2, glowing, alpha,
                                           hedge, section,
                                           v1, v2, c1, c2, t1, t2, tb1, tb2, tc1, tc2);

            if(coords)
            {
                texS += antiClockwise? -node->prev->length : hedge->length;
            }

            n += 2;
        }

        // Add the next edge.
        {
            rvertex_t* v1 = &(*verts)[n + antiClockwise^0];
            rvertex_t* v2 = &(*verts)[n + antiClockwise^1];
            ColorRawf* c1 = colors? &(*colors)[n + antiClockwise^0] : NULL;
            ColorRawf* c2 = colors? &(*colors)[n + antiClockwise^1] : NULL;
            rtexcoord_t* t1 = coords? &(*coords)[n + antiClockwise^0] : NULL;
            rtexcoord_t* t2 = coords? &(*coords)[n + antiClockwise^1] : NULL;
            rtexcoord_t* tb1 = interCoords? &(*interCoords)[n + antiClockwise^0] : NULL;
            rtexcoord_t* tb2 = interCoords? &(*interCoords)[n + antiClockwise^1] : NULL;
            rtexcoord_t* tc1 = shinyCoords? &(*shinyCoords)[n + antiClockwise^0] : NULL;
            rtexcoord_t* tc2 = shinyCoords? &(*shinyCoords)[n + antiClockwise^1] : NULL;
            float deltaL, deltaR;

            wallEdgeLightLevelDeltas(hedge, section, &deltaL, &deltaR);

            Rend_BuildBspLeafWallStripEdge((antiClockwise? node->prev : node->next)->HE_v1origin,
                                           WallDivNode_Height(bottom), WallDivNode_Height(top), texS, matOffset[1],
                                           sectorLightColor, sectorLightLevel, antiClockwise? deltaL : deltaR, surfaceColor, surfaceColor2, glowing, alpha,
                                           hedge, section,
                                           v1, v2, c1, c2, t1, t2, tb1, tb2, tc1, tc2);

            if(coords)
            {
                texS += antiClockwise? -hedge->length : hedge->next->length;
            }

            n += 2;
        }
    } while((node = antiClockwise? node->prev : node->next) != endNode);

    /*if(shinyColors)
    {
        uint i;
        for(i = 0; i < *vertsSize; ++i)
        {
            (*shinyColors)[i].rgba[CR] = MAX_OF(colors[i].rgba[CR], msA->shinyMinColor[CR]);
            (*shinyColors)[i].rgba[CG] = MAX_OF(colors[i].rgba[CG], msA->shinyMinColor[CG]);
            (*shinyColors)[i].rgba[CB] = MAX_OF(colors[i].rgba[CB], msA->shinyMinColor[CB]);
            (*shinyColors)[i].rgba[CA] = MSU(msA, MTU_REFLECTION).opacity; // Strength of the shine.
        }
    }*/
}

static void Rend_WriteBspLeafWallStripGeometry(BspLeaf* leaf, SideDefSection section,
    HEdge* startNode, HEdge* endNode, boolean antiClockwise,
    int surfaceFlags, material_t* material, float matOffset[2])
{
    HEdge* hedge = startNode;
    Sector* frontSec = leaf->sector;
    SideDef* frontSideDef = HEDGE_SIDEDEF(hedge);
    Surface* surface = &frontSideDef->SW_surface(section);

    int rpFlags = RPF_DEFAULT;
    const float* sectorLightColor = R_GetSectorLightColor(frontSec);
    float sectorLightLevel = frontSec->lightLevel;
    float alpha = (section == SS_MIDDLE? surface->rgba[3] : 1.0f);
    const float* surfaceColor = 0, *surfaceColor2 = 0;

    boolean skyMaskedMaterial;
    rtexcoord_t* coords = 0, *interCoords = 0, *shinyCoords = 0;
    ColorRawf* colors = 0, *shinyColors = 0;
    rvertex_t* verts;
    uint vertsSize;
    boolean blended = false;
    const materialsnapshot_t* msA = 0, *msB = 0;
    float inter = 0;
    float matScale[2];
    float glowing;

    matScale[0] = ((surfaceFlags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
    matScale[1] = ((surfaceFlags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

    // Smooth Texture Animation?
    if(smoothTexAnim)
        blended = true;

    if(Material_IsSkyMasked(material) && !devRendSkyMode)
    {
        // We'll mask this.
        rpFlags |= RPF_SKYMASK;
    }
    skyMaskedMaterial = !!(rpFlags & RPF_SKYMASK);

    if(!skyMaskedMaterial)
    {
        inter = getMaterialSnapshots(&msA, blended? &msB : NULL, material);

        SideDef_SurfaceColors(frontSideDef, section, &surfaceColor2, &surfaceColor);
    }

    glowing = msA? msA->glowing : 0;

    Rend_BuildBspLeafWallStripGeometry(leaf, startNode, endNode, antiClockwise, section,
                                       sectorLightColor, sectorLightLevel, glowing, (alpha < 0? 1 : alpha),
                                       surfaceColor, surfaceColor2, matOffset,
                                       &vertsSize, &verts,
                                       (!skyMaskedMaterial)? &colors : 0,
                                       (!skyMaskedMaterial)? &coords : 0,
                                       (!skyMaskedMaterial && msB && Rtu_HasTexture(&MSU(msB, MTU_PRIMARY)))? &interCoords : 0,
                                       (!skyMaskedMaterial && useShinySurfaces && Rtu_HasTexture(&MSU(msA, MTU_REFLECTION)))? &shinyColors : 0,
                                       (!skyMaskedMaterial && useShinySurfaces && Rtu_HasTexture(&MSU(msA, MTU_REFLECTION)))? &shinyCoords : 0);

    // Map RTU configuration from prepared MaterialSnapshot(s).
    RL_LoadDefaultRtus();
    if(!skyMaskedMaterial)
    {
        mapRTUStateFromMaterialSnapshots(msA, inter, msB, NULL/*no offset; already applied to coords*/, matScale);
    }

    RL_AddPolyWithCoordsModulationReflection(PT_TRIANGLE_STRIP, rpFlags,
                                             vertsSize, verts,
                                             (!skyMaskedMaterial)? colors : 0,
                                             (!skyMaskedMaterial)? coords : 0,
                                             (!skyMaskedMaterial && msB && Rtu_HasTexture(&MSU(msB, MTU_PRIMARY)))? interCoords : 0,
                                             0, 0, 0,
                                             (!skyMaskedMaterial && useShinySurfaces && Rtu_HasTexture(&MSU(msA, MTU_REFLECTION)))? shinyColors : 0,
                                             (!skyMaskedMaterial && useShinySurfaces && Rtu_HasTexture(&MSU(msA, MTU_REFLECTION)))? shinyCoords : 0,
                                             coords);

    R_FreeRendVertices(verts);
    R_FreeRendColors(colors);
    R_FreeRendTexCoords(coords);
    R_FreeRendTexCoords(interCoords);
    R_FreeRendTexCoords(shinyCoords);
    R_FreeRendColors(shinyColors);
}

static void Rend_RenderWallStrips(SideDefSection section, boolean oneSided)
{
    BspLeaf* leaf = currentBspLeaf;
    const boolean antiClockwise = false;
    HEdge* baseNode, *startNode, *node;
    walldivnode_t* stripBottom, *stripTop;
    material_t* stripMaterial;
    int stripSurfaceFlags;
    float stripMaterialOffset[2], stripSurfaceLightLevelDelta;

    if(!leaf || !leaf->hedge) return;

    baseNode = leaf->hedge;

    // We may need to break the loop into multiple strips.
    startNode = 0;
    stripBottom = stripTop = 0;
    stripSurfaceFlags = 0;
    stripSurfaceLightLevelDelta = 0;
    stripMaterial = 0;
    stripMaterialOffset[0] = stripMaterialOffset[1] = 0;

    for(node = baseNode;;)
    {
        HEdge* hedge = (antiClockwise? node->prev : node);
        boolean endStrip = false;
        boolean beginNewStrip = false;

        if((hedge->frameFlags & HEDGEINF_FACINGFRONT) &&
           HEdge_HasDrawableSurfaces(hedge) &&
           oneSided == isOneSided(hedge, hedge->sector, HEDGE_BACK_SECTOR(hedge)))
        {
            Sector* frontSec = leaf->sector;
            Sector* backSec  = HEDGE_BACK_SECTOR(hedge);
            SideDef* frontSideDef = HEDGE_SIDEDEF(hedge);
            Surface* surface = &frontSideDef->SW_surface(section);
            walldivnode_t* bottom, *top;
            material_t* material = 0;
            float matOffset[2], llDeltaL, llDeltaR;

            if((surface->inFlags & SUIF_PVIS) &&
               R_WallSectionEdge(hedge, section, 0/*left edge */, frontSec, backSec,
                                 &bottom, &top, matOffset))
            {
                material = chooseHEdgeMaterial(hedge, section);
                wallEdgeLightLevelDeltas(hedge, section, &llDeltaL, &llDeltaR);

                if(WallDivNode_Height(bottom) >= WallDivNode_Height(top))
                {
                    // End the current strip.
                    endStrip = true;
                }
                /// @todo Also end the strip if:
                ///
                ///   a) texture coords are non-contiguous.
                ///   b) surface colors are not equal.
                ///   c) surface alpha is not equal (or edge-contiguous?).
                ///
                else if(startNode && (!FEQUAL(WallDivNode_Height(bottom), WallDivNode_Height(stripBottom)) ||
                                      !FEQUAL(WallDivNode_Height(top),    WallDivNode_Height(stripTop)) ||
                                      material != stripMaterial ||
                                      surface->flags != stripSurfaceFlags ||
                                      !FEQUAL((antiClockwise? llDeltaR : llDeltaL), stripSurfaceLightLevelDelta))/*non-contiguous surface lightlevel deltas?*/)
                {
                    // End the current strip and start another.
                    endStrip = true;
                    beginNewStrip = true;
                }
                else if(!startNode)
                {
                    // A new strip begins.
                    startNode = node;

                    stripBottom = bottom;
                    stripTop = top;
                    stripSurfaceFlags = surface->flags;
                    stripMaterial = material;
                    stripMaterialOffset[0] = matOffset[0];
                    stripMaterialOffset[1] = matOffset[1];
                }

                if(!endStrip)
                {
                    stripSurfaceLightLevelDelta = (antiClockwise? llDeltaL : llDeltaR);
                }
            }
            else
            {
                // End the current strip.
                endStrip = true;
            }
        }
        else
        {
            // End the current strip.
            endStrip = true;
        }

        if(endStrip && startNode)
        {
            // We have complete strip; build and write it.
            Rend_WriteBspLeafWallStripGeometry(leaf, section, startNode, node, antiClockwise,
                                               stripSurfaceFlags, stripMaterial, stripMaterialOffset);

            // End the current strip.
            startNode = 0;
        }

        // Start a new strip from this node?
        if(beginNewStrip) continue;

        // On to the next node.
        node = antiClockwise? node->prev : node->next;

        // Are we done?
        if(node == baseNode) break;
    }

    // Have we an unwritten strip? - build it.
    if(startNode)
    {
        Rend_WriteBspLeafWallStripGeometry(leaf, section, startNode, baseNode, antiClockwise,
                                           stripSurfaceFlags, stripMaterial, stripMaterialOffset);
    }
}

static void Rend_RenderWalls(void)
{
    BspLeaf* leaf = currentBspLeaf;

    if(!leaf || !leaf->hedge) return;

    Rend_RenderWallStrips(SS_MIDDLE, true  /* "one-sided" walls */);
    Rend_RenderWallStrips(SS_TOP,    false /* "two-sided" walls */);
    Rend_RenderWallStrips(SS_BOTTOM, false /* "two-sided" walls */);

    /**
     * Finally the masked/translucent "two-sided" middle wall sections,
     * for which we generate vissprites (so that they may be sorted along
     * with other non-opaque objects, like sprites).
     */
    {
        HEdge* hedge = leaf->hedge;
        do
        {
            if((hedge->frameFlags & HEDGEINF_FACINGFRONT) &&
               HEdge_HasDrawableSurfaces(hedge) &&
               !isOneSided(hedge, hedge->sector, HEDGE_BACK_SECTOR(hedge)))
            {
                Rend_RenderTwoSidedMiddle(hedge);
            }
        } while((hedge = hedge->next) != leaf->hedge);
    }

    // Add occludable ranges to the angle clipper.
    // (Except when the viewer is in the void).
    if(!P_IsInVoid(viewPlayer))
    {
        HEdge* hedge = leaf->hedge;
        do
        {
            if((hedge->frameFlags & HEDGEINF_FACINGFRONT) &&
               HEdge_HasDrawableSurfaces(hedge) &&
               wallGeometryCoversOpening(hedge))
            {
                C_AddRangeFromViewRelPoints(hedge->HE_v1origin, hedge->HE_v2origin);
            }
        } while((hedge = hedge->next) != leaf->hedge);
    }
}

static void Rend_RenderPolyobjs(void)
{
    BspLeaf* leaf = currentBspLeaf;
    LineDef* line;
    HEdge* hedge;
    uint i;

    if(!leaf || !leaf->polyObj) return;

    for(i = 0; i < leaf->polyObj->lineCount; ++i)
    {
        line = leaf->polyObj->lines[i];
        hedge = line->L_frontside.hedgeLeft;

        // Let's first check which way this hedge is facing.
        if(!(hedge->frameFlags & HEDGEINF_FACINGFRONT)) continue;

        Rend_RenderPolyobjHEdge(hedge);
    }
}

static void Rend_RenderPlanes(void)
{
    BspLeaf* leaf = currentBspLeaf;
    Sector* sect;
    uint i;

    if(!leaf || !leaf->sector) return; // An orphan BSP leaf?
    sect = leaf->sector;

    // Render all planes of this sector.
    for(i = 0; i < sect->planeCount; ++i)
    {
        const Plane* plane = sect->planes[i];
        const Surface* suf = &plane->surface;
        boolean isSkyMasked = false;
        boolean addDynLights = !devRendSkyMode;
        boolean clipBackFacing = false;
        float texOffset[2];
        float texScale[2];
        material_t* mat;
        int texMode;

        isSkyMasked = Surface_IsSkyMasked(suf);
        if(isSkyMasked && plane->type != PLN_MID)
        {
            if(!devRendSkyMode) continue; // Not handled here.
            isSkyMasked = false;
        }

        if(renderTextures == 2)
            texMode = 2;
        else if(!suf->material || (devNoTexFix && (suf->inFlags & SUIF_FIX_MISSING_MATERIAL)))
            texMode = 1;
        else
            texMode = 0;

        if(texMode == 0)
            mat = suf->material;
        else if(texMode == 1)
            // For debug, render the "missing" texture instead of the texture
            // chosen for surfaces to fix the HOMs.
            mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":missing"));
        else
            // For lighting debug, render all solid surfaces using the gray texture.
            mat = Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":gray"));

        V2f_Copy(texOffset, suf->visOffset);
        // Add the Y offset to orient the Y flipped texture.
        if(plane->type == PLN_CEILING)
            texOffset[VY] -= leaf->aaBox.maxY - leaf->aaBox.minY;
        // Add the additional offset to align with the worldwide grid.
        texOffset[VX] += (float)leaf->worldGridOffset[VX];
        texOffset[VY] += (float)leaf->worldGridOffset[VY];
        // Inverted.
        texOffset[VY] = -texOffset[VY];

        texScale[VX] = ((suf->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        texScale[VY] = ((suf->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        Rend_RenderPlane(plane->type, plane->visHeight, suf->tangent, suf->bitangent, suf->normal,
            mat, suf->rgba, suf->blendMode, texOffset, texScale,
            isSkyMasked, addDynLights, (!devRendSkyMode && i == PLN_FLOOR), plane->planeID,
            texMode, clipBackFacing);
    }
}

/**
 * Creates new occlusion planes from the BspLeaf's edges.
 * Before testing, occlude the BspLeaf's backfaces. After testing occlude
 * the remaining faces, i.e. the forward facing edges. This is done before
 * rendering edges, so solid segments cut out all unnecessary oranges.
 */
static void occludeBspLeaf(const BspLeaf* bspLeaf, boolean forwardFacing)
{
    coord_t fFloor, fCeil, bFloor, bCeil;
    const coord_t* startv, *endv;
    Sector* front, *back;
    HEdge* hedge;

    if(devNoCulling || !bspLeaf || !bspLeaf->hedge || P_IsInVoid(viewPlayer)) return;

    front = bspLeaf->sector;
    fFloor = front->SP_floorheight;
    fCeil = front->SP_ceilheight;

    hedge = bspLeaf->hedge;
    do
    {
        // Occlusions can only happen where two sectors contact.
        if(hedge->lineDef && HEDGE_BACK_SECTOR(hedge) &&
           (forwardFacing == ((hedge->frameFlags & HEDGEINF_FACINGFRONT)? true : false)))
        {
            back = HEDGE_BACK_SECTOR(hedge);
            bFloor = back->SP_floorheight;
            bCeil = back->SP_ceilheight;

            // Choose start and end vertices so that it's facing forward.
            if(forwardFacing)
            {
                startv = hedge->HE_v1origin;
                endv   = hedge->HE_v2origin;
            }
            else
            {
                startv = hedge->HE_v2origin;
                endv   = hedge->HE_v1origin;
            }

            // Do not create an occlusion for sky floors.
            if(!Surface_IsSkyMasked(&back->SP_floorsurface) ||
               !Surface_IsSkyMasked(&front->SP_floorsurface))
            {
                // Do the floors create an occlusion?
                if((bFloor > fFloor && vOrigin[VY] <= bFloor) ||
                   (bFloor < fFloor && vOrigin[VY] >= fFloor))
                {
                    // Occlude down.
                    C_AddViewRelOcclusion(startv, endv, MAX_OF(fFloor, bFloor), false);
                }
            }

            // Do not create an occlusion for sky ceilings.
            if(!Surface_IsSkyMasked(&back->SP_ceilsurface) ||
               !Surface_IsSkyMasked(&front->SP_ceilsurface))
            {
                // Do the ceilings create an occlusion?
                if((bCeil < fCeil && vOrigin[VY] >= bCeil) ||
                   (bCeil > fCeil && vOrigin[VY] <= fCeil))
                {
                    // Occlude up.
                    C_AddViewRelOcclusion(startv, endv, MIN_OF(fCeil, bCeil), true);
                }
            }
        }
    } while((hedge = hedge->next) != bspLeaf->hedge);
}

static __inline boolean leafHasZeroVolume(BspLeaf* leaf)
{
    Sector* sec = leaf? leaf->sector : NULL;
    if(!sec) return true; // An orphan leaf?
    if(sec->SP_ceilvisheight - sec->SP_floorvisheight <= 0) return true;
    if(leaf->hedgeCount < 3) return true;
    return false;
}

static void renderBspLeaf(BspLeaf* leaf)
{
    uint bspLeafIdx;
    Sector* sec;

    if(leafHasZeroVolume(leaf))
    {
        // Skip this, neighbors handle adding the solid clipper segments.
        return;
    }

    // This is now the current leaf.
    currentBspLeaf = leaf;

    if(!firstBspLeaf)
    {
        if(!C_CheckBspLeaf(leaf))
            return; // This isn't visible.
    }
    else
    {
        firstBspLeaf = false;
    }

    // Mark the sector visible for this frame.
    sec = leaf->sector;
    sec->frameFlags |= SIF_VISIBLE;

    Rend_ClearFrameFlagsForBspLeaf(leaf);
    Rend_MarkHEdgesFacingFront(leaf);

    R_InitForBspLeaf(leaf);
    Rend_RadioBspLeafEdges(leaf);

    bspLeafIdx = GET_BSPLEAF_IDX(leaf);
    occludeBspLeaf(leaf, false);
    LO_ClipInBspLeaf(bspLeafIdx);
    occludeBspLeaf(leaf, true);

    occludeFrontFacingSegsInBspLeaf(leaf);

    if(leaf->polyObj)
    {
        // Polyobjs don't obstruct, do clip lights with another algorithm.
        LO_ClipInBspLeafBySight(bspLeafIdx);
    }

    // Mark the particle generators in the sector visible.
    Rend_ParticleMarkInSectorVisible(sec);

    /**
     * Sprites for this BSP leaf have to be drawn.
     * @note
     * Must be done BEFORE the segments of this BspLeaf are added to the
     * clipper. Otherwise the sprites would get clipped by them, and that
     * wouldn't be right.
     * Must be done AFTER the lumobjs have been clipped as this affects the
     * projection of flares.
     */
    R_AddSprites(leaf);

    // Write sky-surface geometry.
    Rend_RenderSkySurfaces(SKYCAP_LOWER|SKYCAP_UPPER);

    // Write wall geometry.
    Rend_RenderWalls();

    // Write polyobj geometry.
    Rend_RenderPolyobjs();

    // Write plane geometry.
    Rend_RenderPlanes();
}

static void renderNode(runtime_mapdata_header_t* bspPtr)
{
    // If the clipper is full we're pretty much done. This means no geometry
    // will be visible in the distance because every direction has already been
    // fully covered by geometry.
    if(C_IsFull()) return;

    if(bspPtr->type == DMU_BSPLEAF)
    {
        // We've arrived at a leaf. Render it.
        renderBspLeaf((BspLeaf*)bspPtr);
    }
    else
    {
        // Descend deeper into the nodes.
        const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
        BspNode* node = (BspNode*)bspPtr;
        byte side;

        // Decide which side the view point is on.
        side = Partition_PointOnSide(&node->partition, viewData->current.origin);

        renderNode(node->children[side]); // Recursively divide front space.
        renderNode(node->children[side ^ 1]); // ...and back space.
    }
}

void Rend_RenderMap(void)
{
    binangle_t viewside;

    if(!theMap) return;

    // Set to true if dynlights are inited for this frame.
    loInited = false;

    GL_SetMultisample(true);

    // Setup the modelview matrix.
    Rend_ModelViewMatrix(true);

    if(!freezeRLs)
    {
        const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

        // Prepare for rendering.
        RL_ClearLists(); // Clear the lists for new quads.
        C_ClearRanges(); // Clear the clipper.

        // Recycle the vlight lists. Currently done here as the lists are
        // not shared by all viewports.
        VL_InitForNewFrame();

        // Make vissprites of all the visible decorations.
        Rend_ProjectDecorations();

        LO_BeginFrame();

        // Clear particle generator visibilty info.
        Rend_ParticleInitForNewFrame();

        if(Rend_MobjShadowsEnabled())
        {
            R_InitShadowProjectionListsForNewFrame();
        }

        // Add the backside clipping range (if vpitch allows).
        if(vpitch <= 90 - yfov / 2 && vpitch >= -90 + yfov / 2)
        {

            float a = fabs(vpitch) / (90 - yfov / 2);
            binangle_t startAngle =
                (binangle_t) (BANG_45 * fieldOfView / 90) * (1 + a);
            binangle_t angLen = BANG_180 - startAngle;

            viewside = (viewData->current.angle >> (32 - BAMS_BITS)) + startAngle;
            C_SafeAddRange(viewside, viewside + angLen);
            C_SafeAddRange(viewside + angLen, viewside + 2 * angLen);
        }

        // The viewside line for the depth cue.
        viewsidex = -viewData->viewSin;
        viewsidey = viewData->viewCos;

        // We don't want BSP clip checking for the first BSP leaf.
        firstBspLeaf = true;

        if(theMap->bsp->type == DMU_BSPNODE)
        {
            renderNode(theMap->bsp);
        }
        else
        {
            // A single leaf is a special case.
            renderBspLeaf((BspLeaf*)theMap->bsp);
        }

        if(Rend_MobjShadowsEnabled())
        {
            Rend_RenderMobjShadows();
        }
    }
    RL_RenderAllLists();

    // Draw various debugging displays:
    Rend_RenderSurfaceVectors();
    LO_DrawLumobjs(); // Lumobjs.
    Rend_RenderBoundingBoxes(); // Mobj bounding boxes.
    Rend_RenderVertexes();
    Rend_RenderSoundOrigins();
    Rend_RenderGenerators();

    // Draw the Source Bias Editor's draw that identifies the current light.
    SBE_DrawCursor();

    GL_SetMultisample(false);
}
