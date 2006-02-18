/* DE1: $Id$
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
 * rend_main.c: Rendering Subsystem
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_graphics.h"

#include "de_misc.h"
#include "de_ui.h"
#include "de_system.h"
#include "net_main.h"

// MACROS ------------------------------------------------------------------

// FakeRadio should not be rendered.
#define RWSF_NO_RADIO 0x100

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void    Rend_ProjectionMatrix();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int useDynLights, translucentIceCorpse;
extern int skyhemispheres, haloMode;
extern int dlMaxRad;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean useFog = false;         // Is the fog in use?
byte    fogColor[4];
float   fieldOfView = 95.0f;
float   maxLightDist = 1024;
boolean smoothTexAnim = true;
int     useShinySurfaces = true;

float   vx, vy, vz, vang, vpitch;
float   viewsidex, viewsidey;

boolean willRenderSprites = true, freezeRLs = false;
int     missileBlend = 1;
int     litSprites = 1;
int     r_ambient = 0;

int     viewpw, viewph;         // Viewport size, in pixels.
int     viewpx, viewpy;         // Viewpoint top left corner, in pixels.

float   yfov;

int    gamedrawhud = 1;    // Set to zero when we advise that the HUD should not be drawn

extern DGLuint dd_textures[];

int     playerLightRange[MAXPLAYERS];

typedef struct {
    int value;
    int currentlight;
    sector_t *sector;
    unsigned int updateTime;
} lightsample_t;

lightsample_t playerLastLightSample[MAXPLAYERS];

float   r_lightAdapt = 0.2f; // Amount of light adaption

int     r_lightAdaptDarkTime = 80;
int     r_lightAdaptBrightTime = 10;

float   r_lightAdaptRamp = 0.004f;
float   r_lightAdaptMul = 0.2f;

int     r_lightrangeshift = 0;
float   r_lightcompression = 80; //80%

signed short     lightRangeModMatrix[MOD_RANGE][255];

int debugLightModMatrix = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstsubsector;  // No range checking for the first one.

// CODE --------------------------------------------------------------------

void Rend_Register(void)
{
    C_VAR_INT("rend-dev-freeze", &freezeRLs, 0, 0, 1,
              "1=Stop updating rendering lists.");

    C_VAR_FLOAT("rend-camera-fov", &fieldOfView, 0, 1, 179, "Field of view.");

    C_VAR_BYTE("rend-tex-anim-smooth", &smoothTexAnim, 0, 0, 1,
               "1=Enable interpolated texture animation.");

    C_VAR_INT("rend-tex-shiny", &useShinySurfaces, 0, 0, 1,
              "1=Enable shiny textures on surfaces of the map.");

    C_VAR_FLOAT2("rend-light-compression", &r_lightcompression, 0, -100, 100,
                 "Sector light range compression (brighten dark areas / darken light areas).",
                 Rend_CalcLightRangeModMatrix);


    C_VAR_FLOAT("rend-light-adaptation", &r_lightAdapt, 0, 0, 1,
                "Controls the overall strength of light adaptation");

    C_VAR_FLOAT2("rend-light-adaptation-mul", &r_lightAdaptMul, CVF_PROTECTED, 0, 1,
                 "Light adaptation range multiplier.", Rend_CalcLightRangeModMatrix);

    C_VAR_FLOAT2("rend-light-adaptation-ramp", &r_lightAdaptRamp, CVF_PROTECTED, 0, 1,
                 "Light adaptation range ramp multiplier.", Rend_CalcLightRangeModMatrix);

    C_VAR_INT("rend-light-adaptation-darktime", &r_lightAdaptDarkTime, 0, 0, 200,
              "Time it takes to adapt to dark lighting conditions in seconds.");

    C_VAR_INT("rend-light-adaptation-brighttime", &r_lightAdaptBrightTime, 0, 0, 200,
              "Time it takes to adapt to bright lighting conditions in seconds.");


    C_VAR_INT("rend-dev-light-modmatrix", &debugLightModMatrix, 0, 0, 1,
              "Show light modification matrix.");

    RL_Register();
    SB_Register();
    LG_Register();
    Rend_ModelRegister();
    Rend_RadioRegister();
}

float Rend_SignedPointDist2D(float c[2])
{
    /*          (YA-YC)(XB-XA)-(XA-XC)(YB-YA)
       s = -----------------------------
       L**2
       Luckily, L**2 is one. dist = s*L. Even more luckily, L is also one.
     */
    return (vz - c[VY]) * viewsidex - (vx - c[VX]) * viewsidey;
}

/*
 * Approximated! The Z axis aspect ratio is corrected.
 */
float Rend_PointDist3D(float c[3])
{
    return M_ApproxDistance3f(vx - c[VX], vz - c[VY], 1.2f * (vy - c[VZ]));
}

void Rend_Init(void)
{
    C_Init();                   // Clipper.
    RL_Init();                  // Rendering lists.
    Rend_InitSky();             // The sky.
    Rend_CalcLightRangeModMatrix(NULL);
}

/*
 * Used to be called before starting a new level.
 */
void Rend_Reset(void)
{
    // Textures are deleted (at least skies need this???).
    GL_ClearRuntimeTextures();
    DL_Clear();
}

void Rend_ModelViewMatrix(boolean use_angles)
{
    vx = FIX2FLT(viewx);
    vy = FIX2FLT(viewz);
    vz = FIX2FLT(viewy);
    vang = viewangle / (float) ANGLE_MAX *360 - 90;

    gl.MatrixMode(DGL_MODELVIEW);
    gl.LoadIdentity();
    if(use_angles)
    {
        gl.Rotatef(vpitch = viewpitch * 85.0 / 110.0, 1, 0, 0);
        gl.Rotatef(vang, 0, 1, 0);
    }
    gl.Scalef(1, 1.2f, 1);      // This is the aspect correction.
    gl.Translatef(-vx, -vy, -vz);
}

#if 0
/*
 * Models the effect of distance to the light level. Extralight (torch)
 * is also noted. This is meant to be used for white light only
 * (a light level).
 */
int R_AttenuateLevel(int lightlevel, float distance)
{
    float   light = lightlevel / 255.0f, real, minimum;
    float   d, newmin;
    int     i;

    //boolean usewhite = false;

    real = light - (distance - 32) / maxLightDist * (1 - light);
    minimum = light * light + (light - .63f) / 2;
    if(real < minimum)
        real = minimum;         // Clamp it.

    // Add extra light.
    real += extralight / 16.0f;

    // Check for torch.
    if(viewplayer->fixedcolormap)
    {
        // Colormap 1 is the brightest. I'm guessing 16 would be the darkest.
        newmin =
            ((1024 - distance) / 512) * (16 -
                                         viewplayer->fixedcolormap) / 15.0f;
        if(real < newmin)
        {
            real = newmin;
            //usewhite = true; // FIXME : Do some linear blending.
        }
    }

    real *= 256;

    // Clamp the final light.
    if(real < 0)
        real = 0;
    if(real > 255)
        real = 255;

    /*for(i = 0; i < 3; i++)
       vtx->color.rgb[i] = (DGLubyte) ((usewhite? 0xff : rgb[c]) * real); */
    return real;
}
#endif

int Rend_SegFacingDir(float v1[2], float v2[2])
{
    // The dot product. (1 if facing front.)
    return (v1[VY] - v2[VY]) * (v1[VX] - vx) + (v2[VX] - v1[VX]) * (v1[VY] -
                                                                    vz) > 0;
}

int Rend_FixedSegFacingDir(seg_t *seg)
{
    // The dot product. (1 if facing front.)
    return FIX2FLT(seg->v1->y - seg->v2->y) * FIX2FLT(seg->v1->x - viewx) +
        FIX2FLT(seg->v2->x - seg->v1->x) * FIX2FLT(seg->v1->y - viewy) > 0;
}

int Rend_SegFacingPoint(float v1[2], float v2[2], float pnt[2])
{
    float   nx = v1[VY] - v2[VY], ny = v2[VX] - v1[VX];
    float   vvx = v1[VX] - pnt[VX], vvy = v1[VY] - pnt[VY];

    // The dot product.
    if(nx * vvx + ny * vvy > 0)
        return 1;               // Facing front.
    return 0;                   // Facing away.
}

static int C_DECL DivSortAscend(const void *e1, const void *e2)
{
    float   f1 = *(float *) e1, f2 = *(float *) e2;

    if(f1 > f2)
        return 1;
    if(f2 > f1)
        return -1;
    return 0;
}

static int C_DECL DivSortDescend(const void *e1, const void *e2)
{
    float   f1 = *(float *) e1, f2 = *(float *) e2;

    if(f1 > f2)
        return -1;
    if(f2 > f1)
        return 1;
    return 0;
}

static void Rend_ShinySurfaceColor(gl_rgba_t *color, ded_reflection_t *ref)
{
    int i;

    for(i = 0; i < 3; ++i)
    {
        DGLubyte minimum = (DGLubyte) (ref->min_color[i] * 255);

        if(color->rgba[i] < minimum)
        {
            color->rgba[i] = minimum;
        }
    }

    color->rgba[3] = (DGLubyte) (ref->shininess * 255);
}

/*
 * The poly that is passed as argument to this function must not be
 * modified because others may be using it afterwards.  A shiny wall
 * segment polygon is created.
 */
void Rend_AddShinyWallSeg(int texture, const rendpoly_t *poly)
{
    rendpoly_t q;
    ded_reflection_t *ref = NULL;
    texture_t *texptr = NULL;

    if(!useShinySurfaces)
    {
        // Shiny surfaces are not enabled.
        return;
    }

    // Get the surface reflection properties of this texture.
    texptr = textures[texturetranslation[texture].current];
    ref = texptr->reflection;
    if(ref == NULL)
    {
        // The surface doesn't shine.
        return;
    }

    // Make sure the texture has been loaded.
    if(!GL_LoadReflectionMap(ref))
    {
        // Can't shine because there is no shiny map.
        return;
    }

    // We're going to modify the polygon quite a bit.
    memcpy(&q, poly, sizeof(rendpoly_t));

    // Make it a shiny polygon.
    q.flags |= RPF_SHINY;
    q.blendmode = ref->blend_mode;
    q.tex.id = ref->use_shiny->shiny_tex;
    q.tex.detail = NULL;
    q.intertex.detail = NULL;
    q.interpos = 0;

    // Strength of the shine.
    Rend_ShinySurfaceColor(&q.vertices[0].color, ref);
    Rend_ShinySurfaceColor(&q.vertices[1].color, ref);

    // The mask texture is stored in the intertex.
    if(ref->use_mask && ref->use_mask->mask_tex)
    {
        q.intertex.id = ref->use_mask->mask_tex;
        q.tex.width = q.intertex.width = texptr->width * ref->mask_width;
        q.tex.height = q.intertex.height = texptr->height * ref->mask_height;
    }
    else
    {
        // No mask.
        q.intertex.id = 0;
    }

    RL_AddPoly(&q);
}

/*
 * The poly that is passed as argument to this function WILL be modified
 * during the execution of this function.
 */
void RL_AddShinyFlat(planeinfo_t *plane, rendpoly_t *poly,
                     subsector_t *subsector)
{
    int i;
    flat_t *flat = NULL;
    ded_reflection_t *ref = NULL;

    if(!useShinySurfaces)
    {
        // Shiny surfaces are not enabled.
        return;
    }

    // Figure out what kind of surface properties have been defined
    // for the texture in question.
    flat = R_GetFlat(plane->pic);
    if(flat->translation.current != plane->pic)
    {
        // The flat is currently translated to another one.
        flat = R_GetFlat(flat->translation.current);
    }
    ref = flat->reflection;
    if(ref == NULL)
    {
        // Unshiny.
        return;
    }

    // Make sure the texture has been loaded.
    if(!GL_LoadReflectionMap(ref))
    {
        // Can't shine because there is no shiny map.
        return;
    }

    // Make it a shiny polygon.
    poly->lights = NULL;
    poly->flags = RPF_SHINY;
    poly->blendmode = ref->blend_mode;
    poly->tex.id = ref->use_shiny->shiny_tex;
    poly->tex.detail = NULL;
    poly->intertex.detail = NULL;
    poly->interpos = 0;

    // Set shine strength.
    for(i = 0; i < poly->numvertices; ++i)
    {
        Rend_ShinySurfaceColor(&poly->vertices[i].color, ref);
    }

    // Set the mask texture.
    if(ref->use_mask && ref->use_mask->mask_tex)
    {
        poly->intertex.id = ref->use_mask->mask_tex;
        poly->tex.width = poly->intertex.width = 64 * ref->mask_width;
        poly->tex.height = poly->intertex.height = 64 * ref->mask_height;
    }
    else
    {
        poly->intertex.id = 0;
    }

    RL_AddPoly(poly);
}

void Rend_PolyTextureBlend(int texture, rendpoly_t *poly)
{
    translation_t *xlat = &texturetranslation[texture];

    // If fog is active, inter=0 is accepted as well. Otherwise flickering
    // may occur if the rendering passes don't match for blended and
    // unblended surfaces.
    if(!smoothTexAnim || numTexUnits < 2 || !texture ||
       xlat->current == xlat->next || (!useFog && xlat->inter <= 0))
    {
        // No blending for you, my friend.
        memset(&poly->intertex, 0, sizeof(poly->intertex));
        poly->interpos = 0;
        return;
    }

    // Get info of the blend target. The globals texw and texh are modified.
    poly->intertex.id = GL_PrepareTexture2(xlat->next, false);
    poly->intertex.width = texw;
    poly->intertex.height = texh;
    poly->intertex.detail = texdetail;
    poly->interpos = xlat->inter;
}

void Rend_PolyFlatBlend(int flat, rendpoly_t *poly)
{
    flat_t *ptr = R_GetFlat(flat);

    // If fog is active, inter=0 is accepted as well. Otherwise flickering
    // may occur if the rendering passes don't match for blended and
    // unblended surfaces.
    if(!smoothTexAnim || numTexUnits < 2 ||
       ptr->translation.current == ptr->translation.next || (!useFog &&
                                                             ptr->translation.
                                                             inter <= 0))
    {
        memset(&poly->intertex, 0, sizeof(poly->intertex));
        poly->interpos = 0;
        return;
    }

    // Get info of the blend target. The globals texw and texh are modified.
    poly->intertex.id = GL_PrepareFlat2(ptr->translation.next, false);
    poly->intertex.width = texw;
    poly->intertex.height = texh;
    poly->intertex.detail = texdetail;
    poly->interpos = ptr->translation.inter;
}

/*
 * Returns true if the quad has a division at the specified height.
 */
int Rend_CheckDiv(rendpoly_t *quad, int side, float height)
{
    int     i;

    for(i = 0; i < quad->divs[side].num; i++)
        if(quad->divs[side].pos[i] == height)
            return true;
    return false;
}

/*
 * Division will only happen if it must be done.
 * Converts quads to divquads.
 */
void Rend_WallHeightDivision(rendpoly_t *quad, seg_t *seg, sector_t *frontsec,
                             int mode)
{
    int     i, k, vtx[2];       // Vertex indices.
    vertexowner_t *own;
    sector_t *sec;
    float   hi, low;
    float   sceil, sfloor;

    switch (mode)
    {
    case SEG_MIDDLE:
        hi = SECT_CEIL(frontsec);
        low = SECT_FLOOR(frontsec);
        break;

    case SEG_TOP:
        hi = SECT_CEIL(frontsec);
        low = SECT_CEIL(seg->backsector);
        if(SECT_FLOOR(frontsec) > low)
            low = SECT_FLOOR(frontsec);
        break;

    case SEG_BOTTOM:
        hi = SECT_FLOOR(seg->backsector);
        low = SECT_FLOOR(frontsec);
        if(SECT_CEIL(frontsec) < hi)
            hi = SECT_CEIL(frontsec);
        break;

    default:
        return;
    }

    vtx[0] = GET_VERTEX_IDX(seg->v1);
    vtx[1] = GET_VERTEX_IDX(seg->v2);
    quad->divs[0].num = 0;
    quad->divs[1].num = 0;

    // Check both ends.
    for(i = 0; i < 2; i++)
    {
        own = vertexowners + vtx[i];
        if(own->num > 1)
        {
            // More than one sectors! The checks must be made.
            for(k = 0; k < own->num; k++)
            {
                sec = SECTOR_PTR(own->list[k]);
                if(sec == frontsec)
                    continue;   // Skip this sector.
                if(sec == seg->backsector)
                    continue;

                sceil = SECT_CEIL(sec);
                sfloor = SECT_FLOOR(sec);

                // Divide at the sector's ceiling height?
                if(sceil > low && sceil < hi)
                {
                    quad->type = RP_DIVQUAD;
                    if(!Rend_CheckDiv(quad, i, sceil))
                        quad->divs[i].pos[quad->divs[i].num++] = sceil;
                }
                // Do we need to break?
                if(quad->divs[i].num == RL_MAX_DIVS)
                    break;

                // Divide at the sector's floor height?
                if(sfloor > low && sfloor < hi)
                {
                    quad->type = RP_DIVQUAD;
                    if(!Rend_CheckDiv(quad, i, sfloor))
                        quad->divs[i].pos[quad->divs[i].num++] = sfloor;
                }
                // Do we need to break?
                if(quad->divs[i].num == RL_MAX_DIVS)
                    break;
            }
            // We need to sort the divisions for the renderer.
            if(quad->divs[i].num > 1)
            {
                // Sorting is required. This shouldn't take too long...
                // There seldom are more than one or two divisions.
                qsort(quad->divs[i].pos, quad->divs[i].num, sizeof(float),
                      i ? DivSortDescend : DivSortAscend);
            }
#ifdef RANGECHECK
            for(k = 0; k < quad->divs[i].num; k++)
                if(quad->divs[i].pos[k] > hi || quad->divs[i].pos[k] < low)
                {
                    Con_Error("DivQuad: i=%i, pos (%f), hi (%f), "
                              "low (%f), num=%i\n", i, quad->divs[i].pos[k],
                              hi, low, quad->divs[i].num);
                }
#endif
        }
    }
}

/*
 * Calculates the placement for a middle texture (top, bottom, offset).
 * Texture must be prepared so texh is known.
 * texoffy may be NULL.
 * Returns false if the middle texture isn't visible (in the opening).
 */
int Rend_MidTexturePos(float *top, float *bottom, float *texoffy, float tcyoff,
                       boolean lower_unpeg)
{
    float   openingTop = *top, openingBottom = *bottom;

    if(openingTop <= openingBottom)
        return false;

    if(texoffy)
        *texoffy = 0;

    // We don't allow vertical tiling.
    if(lower_unpeg)
    {
        *bottom += tcyoff;
        *top = *bottom + texh;
    }
    else
    {
        *top += tcyoff;
        *bottom = *top - texh;
    }

    // Clip it.
    if(*bottom < openingBottom)
    {
        *bottom = openingBottom;
    }
    if(*top > openingTop)
    {
        if(texoffy)
            *texoffy += *top - openingTop;
        *top = openingTop;
    }
    return true;
}

/*
 * The sector height should've been checked by now.
 * This seriously needs to be rewritten! Witness the accumulation of hacks
 * on kludges...
 */
void Rend_RenderWallSeg(seg_t *seg, sector_t *frontsec, int flags)
{
    int i;
    int     segindex, sectorlight;
    sector_t *backsec;
    side_t *sid, *backsid, *side;
    line_t *ldef;
    float   ffloor, fceil, bfloor, bceil, fsh, bsh, mceil, tcyoff;
    rendpoly_t quad;
    float  *v1, *v2;
    byte    topvis, midvis, botvis;
    boolean present;
    boolean frontSide = false, solidSeg = false;
    const byte *sLightColor;
    byte topColor[] = {0,0,0};
    byte midColor[] = {0,0,0};
    byte bottomColor[] = {0,0,0};

    // Let's first check which way this seg is facing.
    if(!Rend_FixedSegFacingDir(seg))
        return;

    segindex = GET_SEG_IDX(seg);
    backsec = seg->backsector;
    sid = SIDE_PTR(seg->linedef->sidenum[0]);
    backsid = SIDE_PTR(seg->linedef->sidenum[1]);
    ldef = seg->linedef;
    ffloor = SECT_FLOOR(frontsec);
    fceil = SECT_CEIL(frontsec);
    fsh = fceil - ffloor;

    // Which side are we using?
    if(sid == seg->sidedef)
    {
        side = sid;
        frontSide = true;
    }
    else
        side = backsid;

    // Init the quad.
    quad.type = RP_QUAD;
    quad.flags = 0;
    quad.tex.detail = NULL;
    quad.intertex.detail = NULL;
    quad.sector = frontsec;
    quad.numvertices = 2;

    v1 = quad.vertices[0].pos;
    v2 = quad.vertices[1].pos;

    // Get the start and end points.
    v1[VX] = FIX2FLT(seg->v1->x);
    v1[VY] = FIX2FLT(seg->v1->y);
    v2[VX] = FIX2FLT(seg->v2->x);
    v2[VY] = FIX2FLT(seg->v2->y);

    // Calculate the distances.
    quad.vertices[0].dist = Rend_PointDist2D(v1);
    quad.vertices[1].dist = Rend_PointDist2D(v2);

    // This line is now seen in the map.
    ldef->flags |= ML_MAPPED;

    // Some texture coordinates.
    quad.length = seg->length;
    quad.texoffx = FIX2FLT(side->textureoffset + seg->offset);
    tcyoff = FIX2FLT(side->rowoffset);

    // Calculate the color at both vertices.
    sectorlight = Rend_SectorLight(frontsec);
    sLightColor = R_GetSectorLightColor(frontsec);

    // Top wall section color offset?
    if(side->toprgb[0] < 255 || side->toprgb[1] < 255 || side->toprgb[2] < 255)
    {
        for(i=0; i < 3; i++)
            topColor[i] = (byte)(((side->toprgb[i]/ 255.0f)) * sLightColor[i]);
    }
    else
        memcpy(topColor,sLightColor, 3);

    // Mid wall section color offset?
    if(side->midrgba[0] < 255 || side->midrgba[1] < 255 || side->midrgba[2] < 255)
    {
        for(i=0; i < 3; i++)
            midColor[i] = (byte)(((side->midrgba[i]/ 255.0f)) * sLightColor[i]);
    }
    else
        memcpy(midColor,sLightColor, 3);

    // Bottom wall section color offset?
    if(side->bottomrgb[0] < 255 || side->bottomrgb[1] < 255 || side->bottomrgb[2] < 255)
    {
       for(i=0; i < 3; i++)
           bottomColor[i] = (byte)(((side->bottomrgb[i]/ 255.0f)) * sLightColor[i]);
    }
    else
        memcpy(bottomColor,sLightColor, 3);

    // The middle texture, single sided.
    if((side->midtexture || side->midtexture == -1) && !backsec)
    {
        // Fades
        if(side->flags & SDF_BLENDMIDTOTOP)
        {
            RL_VertexColors(&quad, sectorlight, topColor);
            for(i=0; i < 2; i++)
                memcpy(quad.bottomcolor[i].rgba, midColor, 3);
        }
        else if(side->flags & SDF_BLENDMIDTOBOTTOM)
        {
            RL_VertexColors(&quad, sectorlight, midColor);
            for(i=0; i < 2; i++)
                memcpy(quad.bottomcolor[i].rgba, bottomColor, 3);
        }
        else
            RL_VertexColors(&quad, sectorlight, midColor);

        if(side->midtexture == -1)
        {
            // A missing texture, draw the "missing" graphic
            quad.tex.id = curtex = (unsigned int) dd_textures[DDTEX_MISSING];
            quad.tex.width = quad.tex.height = 64;
            quad.tex.detail = NULL;
        }
        else
        {
            // Prepare the texture
            quad.tex.id = curtex = GL_PrepareTexture(side->midtexture);
            quad.tex.width = texw;
            quad.tex.height = texh;
            quad.tex.detail = texdetail;
        }

        quad.texoffy = tcyoff;
        if(ldef->flags & ML_DONTPEGBOTTOM)
            quad.texoffy += texh - fsh;

        // Fill in the remaining quad data.
        quad.flags = /*seg->flags & DDSEGF_DLIGHT? RPF_DLIT : */ 0;

        // What about glow?
        if(side->midtexture != -1)
            if(R_TextureFlags(side->midtexture) & TXF_GLOW)
                quad.flags |= RPF_GLOW;

        quad.top = fceil;
        quad.bottom = ffloor;

        // Check for neighborhood division.
        Rend_WallHeightDivision(&quad, seg, frontsec, SEG_MIDDLE);

        // Dynamic lights.
        quad.lights = DL_GetSegLightLinks(segindex, SEG_MIDDLE);

        if(side->midtexture != -1)
            Rend_PolyTextureBlend(side->midtexture, &quad);

        // Do BIAS lighting for this poly
        SB_RendPoly(&quad, false, frontsec, seginfo[segindex].illum[1],
                    &seginfo[segindex].tracker[1], segindex);

        // Add the poly to the appropriate list
        RL_AddPoly(&quad);

        // Render Fakeradio polys for this seg?
        if(!(flags & RWSF_NO_RADIO))
        {
            Rend_RadioWallSection(seg, &quad);
        }

        if(side->midtexture != -1)
        {
            // Render Shiny polys for this seg?
            Rend_AddShinyWallSeg(side->midtexture, &quad);
        }

        // This is guaranteed to be a solid segment.
        C_AddViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]);
        solidSeg = true;
    }

    // Restore original type, height division may change this.
    quad.type = RP_QUAD;

    if(backsec)
    {
        bceil = SECT_CEIL(backsec);
        bfloor = SECT_FLOOR(backsec);
    }

    // The skyfix?
    if(frontsec->skyfix)
    {
        if(!backsec ||
           (backsec && backsec != seg->frontsector &&
            (bceil + backsec->skyfix < fceil + frontsec->skyfix)))
        {
            if(backsec && !frontSide && sid->midtexture != 0)
            {
                // This traps the skyfix glitch as seen in ICARUS.wad MAP01's
                // engine tubes (scrolling mid texture doors)
            }
            else
            {
                quad.flags = RPF_SKY_MASK;
                quad.top = fceil + frontsec->skyfix;
                quad.bottom = fceil;
                quad.tex.id = 0;
                quad.lights = NULL;
                quad.intertex.id = 0;
                RL_AddPoly(&quad);
            }
        }
    }

    // If there is a back sector we may need upper and lower walls.
    if(backsec)                 // A twosided seg?
    {
        boolean mid_covers_top = false;
        boolean backsecSkyFix = false;

        bsh = bceil - bfloor;

        // Determine which parts of the segment are visible.
        midvis = (bsh > 0 && (side->midtexture != 0));
        topvis = (bceil < fceil);
        botvis = (bfloor > ffloor);

        // Needs skyfix?
        if(seg->frontsector->ceilingpic == skyflatnum &&
           fceil + frontsec->skyfix > bceil &&  side->toptexture == 0 &&
           !(backsec->ceilingpic == skyflatnum && bsh > 0) &&
            ((botvis && (side->midtexture || side->midtexture == -1) && side->bottomtexture != 0) ||
            (side->bottomtexture || side->bottomtexture == -1)))
        {
            quad.flags = RPF_SKY_MASK;
            quad.top = fceil + frontsec->skyfix;
            quad.bottom = bceil;
            quad.tex.id = 0;
            quad.lights = NULL;
            quad.intertex.id = 0;
            RL_AddPoly(&quad);
            backsecSkyFix = true;
        }

        // Missing top or bottom textures don't occlude visibility.
        if(backsec != seg->frontsector && (((bceil <= ffloor || bfloor >= fceil) &&
           !(topvis && side->toptexture == 0 && side->midtexture != 0) &&
           !(botvis && side->bottomtexture == 0 && side->midtexture != 0)) ||
           (bsh <= 0 && topvis && (side->toptexture || side->toptexture == -1 || backsecSkyFix)
            && botvis && (side->bottomtexture || side->bottomtexture == -1)))  )
        {
            // The backsector has no space. This is a solid segment.
            C_AddViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]);
            solidSeg = true;
        }

        // Quite probably a masked texture. Won't be drawn if a visible
        // top or bottom texture is missing.
        if(side->midtexture || side->midtexture == -1)
        {
            // Use actual (smoothed) sector heights (non-linked).
            sectorinfo_t *finfo = SECT_INFO(seg->frontsector),
                         *binfo = SECT_INFO(seg->backsector);
            float   rbceil = binfo->visceil,
                   rbfloor = binfo->visfloor,
                    rfceil = finfo->visceil,
                   rffloor = finfo->visfloor;
            float   gaptop, gapbottom;

            // Fades?
            if(side->flags & SDF_BLENDMIDTOTOP)
            {
                RL_VertexColors(&quad, sectorlight, topColor);
                for(i=0; i < 2; i++)
                    memcpy(quad.bottomcolor[i].rgba, midColor, 3);
            }
            else if(side->flags & SDF_BLENDMIDTOBOTTOM)
            {
                RL_VertexColors(&quad, sectorlight, midColor);
                for(i=0; i < 2; i++)
                    memcpy(quad.bottomcolor[i].rgba, bottomColor, 3);
            }
            else
                RL_VertexColors(&quad, sectorlight, midColor);

            // Mid textures have alpha
            for(i = 0; i < 2; i++)
                quad.vertices[i].color.rgba[3] = side->midrgba[3];

            // Blendmode too
            if(sid->blendmode == BM_NORMAL && noSpriteTrans)
                quad.blendmode = BM_ZEROALPHA; // "no translucency" mode
            else
                quad.blendmode = sid->blendmode;

            quad.top = gaptop = MIN_OF(rbceil, rfceil);
            quad.bottom = gapbottom = MAX_OF(rbfloor, rffloor);

            if(side->midtexture == -1)
            {
                // A missing texture, draw the "missing" graphic
                quad.tex.id = curtex = (unsigned int) dd_textures[DDTEX_MISSING];
                quad.tex.width = quad.tex.height = 64;
                quad.tex.detail = NULL;
            }
            else
            {
                quad.tex.id = curtex = GL_PrepareTexture(side->midtexture);
                quad.tex.detail = texdetail;
                quad.tex.width = texw;
                quad.tex.height = texh;
            }

            if(topvis && side->toptexture == 0)
            {
                mceil = quad.top;
                // Extend to cover missing top texture.
                quad.top = MAX_OF(bceil, fceil);
                if(texh > quad.top - quad.bottom)
                {
                    mid_covers_top = true;  // At least partially...
                    tcyoff -= quad.top - mceil;
                }
            }

            if(Rend_MidTexturePos
               (&quad.top, &quad.bottom, &quad.texoffy, tcyoff,
                0 != (ldef->flags & ML_DONTPEGBOTTOM)))
            {
                // If alpha, masked or blended we must render as a vissprite
                if(side->midrgba[3] < 255 || texmask || side->blendmode > 0)
                    quad.flags = RPF_MASKED;
                else
                    quad.flags = 0;

                // What about glow?
                if(side->midtexture != -1)
                    if(R_TextureFlags(side->midtexture) & TXF_GLOW)
                        quad.flags |= RPF_GLOW;

                // Dynamic lights.
                quad.lights = DL_GetSegLightLinks(segindex, SEG_MIDDLE);

                // Should a solid segment be added here?
                if(!texmask && side->midrgba[3] == 255 && side->blendmode == 0
                   && quad.top >= gaptop && quad.bottom <= gapbottom)
                    C_AddViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]);

                if(side->midtexture != -1)
                    Rend_PolyTextureBlend(side->midtexture, &quad);

                SB_RendPoly(&quad, false, frontsec,
                            seginfo[segindex].illum[1],
                            &seginfo[segindex].tracker[1], segindex);
                RL_AddPoly(&quad);

                if(!texmask)
                {
                    if(!(flags & RWSF_NO_RADIO) && solidSeg)
                        Rend_RadioWallSection(seg, &quad);

                    if(side->midtexture != -1)
                        Rend_AddShinyWallSeg(side->midtexture, &quad);
                }
            }
        }

        // Upper wall.
        if(topvis && backsec != seg->frontsector &&
           !(frontsec->ceilingpic == skyflatnum &&
             backsec->ceilingpic == skyflatnum) && !mid_covers_top)
        {
            float   topwh = fceil - bceil;

            present = false;
            if(side->toptexture == -1)
            {
                // A bad texture name, draw the "missing" graphic
                quad.tex.id = curtex = (unsigned int) dd_textures[DDTEX_MISSING];
                quad.tex.width = quad.tex.height = 64;
                quad.tex.detail = NULL;
                present = true;
            }
            else if(side->toptexture) // Has a texture?
            {
                quad.tex.id = curtex = GL_PrepareTexture(side->toptexture);
                quad.tex.detail = texdetail;
                quad.tex.width = texw;
                quad.tex.height = texh;
                present = true;
            }
            else
            {
                // Texture missing? Take the ceiling texture.
                int     replacement = frontsec->ceilingpic;

                if(!(botvis && side->toptexture == 0))
                {
                    if(replacement == skyflatnum)
                    {
                        // The skyflat is not an appropriate replacement!
                        // Note that both the front and back ceilings can't be
                        // sky (the upper seg is not visible in that case).
                        replacement = backsec->ceilingpic;
                    }
                    quad.tex.id = curtex = GL_PrepareFlat(replacement);
                    quad.tex.detail = texdetail;
                    quad.tex.width = texw;
                    quad.tex.height = texh;
                    present = true;
                }
            }

            if(present)
            {
                // Fades
                if(side->flags & SDF_BLENDTOPTOMID)
                {
                    RL_VertexColors(&quad, sectorlight, topColor);
                    for(i=0; i < 2; i++)
                        memcpy(quad.bottomcolor[i].rgba, midColor, 3);
                }
                else
                    RL_VertexColors(&quad, sectorlight, topColor);

                // Calculate texture coordinates.
                quad.texoffy = tcyoff;
                if(!(ldef->flags & ML_DONTPEGTOP))
                {
                    // Normal alignment to bottom.
                    quad.texoffy += texh - topwh;
                }
                quad.flags = 0;
                //if(seg->flags & DDSEGF_DLIGHT) quad.flags |= RPF_DLIT;
                // What about glow?
                if(side->toptexture != -1)
                    if(R_TextureFlags(side->toptexture) & TXF_GLOW)
                        quad.flags |= RPF_GLOW;

                quad.top = fceil;
                quad.bottom = bceil;
                if(quad.bottom < ffloor)
                    quad.bottom = ffloor;

                // Might be necessary.
                Rend_WallHeightDivision(&quad, seg, frontsec, SEG_TOP);

                // Dynamic lights.
                quad.lights = DL_GetSegLightLinks(segindex, SEG_TOP);

                if(side->toptexture != -1)
                    Rend_PolyTextureBlend(side->toptexture, &quad);

                SB_RendPoly(&quad, false, frontsec,
                            seginfo[segindex].illum[0],
                            &seginfo[segindex].tracker[0], segindex);
                RL_AddPoly(&quad);

                if(side->toptexture != -1)
                    Rend_AddShinyWallSeg(side->toptexture, &quad);

                if(!(flags & RWSF_NO_RADIO))
                {
                    Rend_RadioWallSection(seg, &quad);
                }

                // Restore original type, height division may change this.
                quad.type = RP_QUAD;
            }
        }

        // Lower wall.
        // If no textures have been assigned to the segment, we won't
        // draw anything.
        if(botvis && backsec != seg->frontsector &&
              bfloor > ffloor && !(frontsec->floorpic == skyflatnum &&
                                   backsec->floorpic == skyflatnum))
        {
            present = false;
            if(side->bottomtexture == -1)
            {
                // A bad texture name, draw the "missing" graphic
                quad.tex.id = curtex = (unsigned int) dd_textures[DDTEX_MISSING];
                quad.tex.width = quad.tex.height = 64;
                quad.tex.detail = NULL;
                present = true;
            }
            else if(side->bottomtexture)  // There is a texture?
            {
                quad.tex.id = curtex = GL_PrepareTexture(side->bottomtexture);
                quad.tex.detail = texdetail;
                quad.tex.width = texw;
                quad.tex.height = texh;
                present = true;
            }
            else
            {
                // No texture? Again!
                if(!(topvis && side->toptexture == 0))
                {
                    quad.tex.id = curtex = GL_PrepareFlat(frontsec->floorpic);
                    quad.tex.detail = texdetail;
                    quad.tex.width = texw;
                    quad.tex.height = texh;
                    present = true;
                }
            }

            if(present)
            {
                // Fades
                if(side->flags & SDF_BLENDBOTTOMTOMID)
                {
                    RL_VertexColors(&quad, sectorlight, midColor);
                    for(i=0; i < 2; i++)
                        memcpy(quad.bottomcolor[i].rgba, bottomColor, 3);
                }
                else
                    RL_VertexColors(&quad, sectorlight, bottomColor);

                // Calculate texture coordinates.
                quad.texoffy = tcyoff;
                if(ldef->flags & ML_DONTPEGBOTTOM)
                {
                    // Lower unpegged. Align with normal middle texture.
                    quad.texoffy += fceil - bfloor;
                }
                quad.flags = 0;
                //          if(seg->flags & DDSEGF_DLIGHT) quad.flags |= RPF_DLIT;
                // What about glow?
                if(side->bottomtexture != -1)
                    if(R_TextureFlags(side->bottomtexture) & TXF_GLOW)
                        quad.flags |= RPF_GLOW;

                quad.top = bfloor;
                if(quad.top > fceil)
                {
                    // Can't go over front ceiling, would induce polygon flaws.
                    quad.texoffy += quad.top - fceil;
                    quad.top = fceil;
                }
                quad.bottom = ffloor;

                // Might be necessary.
                Rend_WallHeightDivision(&quad, seg, frontsec, SEG_BOTTOM);

                // Dynamic lights.
                quad.lights = DL_GetSegLightLinks(segindex, SEG_BOTTOM);

                if(side->bottomtexture != -1)
                    Rend_PolyTextureBlend(side->bottomtexture, &quad);

                SB_RendPoly(&quad, false, frontsec,
                            seginfo[segindex].illum[2],
                            &seginfo[segindex].tracker[2], segindex);
                RL_AddPoly(&quad);

                if(!(flags & RWSF_NO_RADIO))
                {
                    Rend_RadioWallSection(seg, &quad);
                }

                if(side->bottomtexture != -1)
                    Rend_AddShinyWallSeg(side->bottomtexture, &quad);
            }
        }
    }
}

int Rend_SectorLight(sector_t *sec)
{
    int     i;

    i = LevelFullBright ? 255 : sec->lightlevel;

    // Apply light adaptation
    Rend_ApplyLightAdaptation(&i);

    return i;
}

/*
 * Updates the lightRangeModMatrix which is used to applify sector light to
 * help compensate for the differences between the OpenGL lighting equation,
 * the software Doom lighting model and the light grid (ambient lighting).
 *
 * Also calculates applified values (the curve of set with r_lightAdaptMul),
 * used for emulation of the effects of light adaptation in the human eye.
 * This is not strictly accurate as the amplification happens at differing
 * rates in each color range but we'll just use one value applied uniformly
 * to each range (RGB).
 *
 * The offsets in the lightRangeModTables are added to the sector->lightlevel
 * during rendering (both positive and negative).
 */
void Rend_CalcLightRangeModMatrix(cvar_t* unused)
{
    int r, j, n;
    int mid = MOD_RANGE / 2;
    float f, mod, factor;
    double multiplier, mx;

    memset(lightRangeModMatrix, 0, (sizeof(byte) * 255) * MOD_RANGE);

    if(r_lightcompression > 0)
        factor = r_lightcompression;
    else
        factor = 1;

    multiplier = r_lightAdaptMul / MOD_RANGE;

    for(n = 0, r = -mid; r < mid; ++n, ++r)
    {
        // Calculate the light mod value for this range
        mod = (MOD_RANGE + n) / 255.0f;

        // Calculate the multiplier.
        mx = (r * multiplier) * MOD_RANGE;

        for(j = 0; j < 255; ++j)
        {
            if(r < 0)  // Dark to light range
            {
                // Apply the mod factor
                f = -((mod * j) / (n + 1));

                // Apply the multiplier
                f += -r * ((f * (mx * j)) * r_lightAdaptRamp);
            }
            else  // Light to dark range
            {
                f = ((255 - j) * mod) / (MOD_RANGE - n);
                f -= r * ((f * (mx * (255 - j))) * r_lightAdaptRamp);
            }

            // Adjust the white point/dark point
            if(r_lightcompression >= 0)
                f += (int)((255.f - j) * (r_lightcompression / 255.f));
            else
                f += (int)((j) * (r_lightcompression / 255.f));

            // Apply the linear range shift
            f += r_lightrangeshift;

            // Round to nearest (signed) whole.
            if(f >= 0)
                f += 0.5f;
            else
                f -= 0.5f;

            // Clamp the result as a modifier to the light value (j).
            if(r < 0)
            {
                if((j+f) >= 255)
                    f = 254 - j;
            }
            else
            {
                if((j+f) <= 0)
                    f = -j;
            }

            // Insert it into the matrix
            lightRangeModMatrix[n][j] = (signed short) f;
        }
    }
}

/*
 * Grabs the light value of the sector each player is currently
 * in and chooses an appropriate light range.
 *
 * TODO: Interpolate between current range and last when player
 *       changes sector.
 */
void Rend_RetrieveLightSample(void)
{
    int i, diff, midpoint;
    short light;
    float range;
    float mod;
    float pos[3];
    float normal[3];
    float inter;
    ddplayer_t *player;
    subsector_t *sub;

    unsigned int currentTime = Sys_GetRealTime();

    midpoint = MOD_RANGE / 2;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].ingame)
            continue;

        player = &players[i];

        sub = R_PointInSubsector(player->mo->x, player->mo->y);

        // Evaluate the light level at the point this player is at.
        // Uses a very simply method which uses the sector in which the player
        // is at currently and uses the light level of that sector.
        // Really this is junk. We should evaluate the lighting condition using
        // a much better method (perhaps obtain an average color from all the
        // vertexes (selected lists only) rendered in the previous frame?)
     /*   if(useBias)
        {
            byte color[3];

            // use the bias lighting to evaluate the point.
            // todo this we'll create a vertex at the players position
            // so that it can be evaluated with both ambient light (light grid)
            // and with the bias light sources.

            // Use the players postion.
            pos[VX] = FIX2FLT(player->mo->x);
            pos[VY] = FIX2FLT(player->mo->y);
            pos[VZ] = FIX2FLT(player->mo->z);

            // TODO: Should be affected by BIAS sources...
            LG_Evaluate(pos, color);
            light = (int) ((color[0] + color[1] + color[2]) / 3);
        }
        else*/
        {
            // use the light level of the sector they are currently in.
            light = sub->sector->lightlevel;
        }

        // Pick the range based on the current lighting conditions compared
        // to the previous lighting conditions.
        // We adapt to predominantly bright lighting conditions (mere seconds)
        // much quicker than very dark lighting conditions (upto 30mins).
        // Obviously that would be much too slow for a fast paced action game.
        if(light < playerLastLightSample[i].value)
        {
            // Going from a bright area to a darker one
            inter = (currentTime - playerLastLightSample[i].updateTime) /
                           (float) SECONDS_TO_TICKS(r_lightAdaptDarkTime);

            if(inter > 1)
                mod = 0;
            else
            {
                diff = playerLastLightSample[i].value - light;
                mod = -(((diff - (diff * inter)) / MOD_RANGE) * 25); //midpoint
            }
        }
        else if(light > playerLastLightSample[i].value)
        {
            // Going from a dark area to a bright one
            inter = (currentTime - playerLastLightSample[i].updateTime) /
                           (float) SECONDS_TO_TICKS(r_lightAdaptBrightTime);

            if(inter > 1)
                mod = 0;
            else
            {
                diff = light - playerLastLightSample[i].value;
                mod = ((diff - (diff * inter)) / MOD_RANGE) * 25;
            }
        }
        else
            mod = 0;

        range = midpoint -
                ((/*( ((float) light / 255.f) * 10) +*/ mod) * r_lightAdapt);
        // Round to nearest whole.
        range += 0.5f;

        // Clamp the range
        if(range >= MOD_RANGE)
            range = MOD_RANGE - 1;
        else if(range < 0)
            range = 0;

        playerLightRange[i] = (int) range;

        // Have the lighting conditions changed?

        // Light differences between sectors?
        if(!(playerLastLightSample[i].sector == sub->sector))
        {
            // If this sample is different to the previous sample
            // update the last sample value;
            if(playerLastLightSample[i].sector)
                playerLastLightSample[i].value = playerLastLightSample[i].sector->lightlevel;
            else
                playerLastLightSample[i].value = light;

            playerLastLightSample[i].updateTime = currentTime;

            playerLastLightSample[i].sector = sub->sector;

            playerLastLightSample[i].currentlight = light;
        }
        else // Light changes in the same sector?
        {
            if(playerLastLightSample[i].currentlight != light)
            {
                playerLastLightSample[i].value = light;
                playerLastLightSample[i].updateTime = currentTime;
            }
        }
    }
}

/*
 * Applies the offset from the lightRangeModMatrix to the given light value.
 *
 * The range chosen is that of the current viewplayer.
 * (calculated based on the lighting conditions for that player)
 *
 * The lightRangeModMatrix is used to implement (precalculated) ambient light
 * limit, light range compression, light range shift AND light adaptation
 * response curves. By generating this precalculated matrix we save on a LOT
 * of calculations (replacing them all with one single addition) which would
 * otherwise all have to be performed each frame for each call. The values are
 * applied to sector lightlevels, lightgrid point evaluations, vissprite
 * lightlevels, fakeradio shadows etc, etc...
 *
 * NOTE: There is no need to clamp the result. Since the offset values in
 *       the lightRangeModMatrix have already been clamped so that the resultant
 *       lightvalue is NEVER outside the range 0-254 when the original lightvalue
 *       is used as the index.
 *
 * @param lightvar    Ptr to the value to apply the adaptation to.
 */
void Rend_ApplyLightAdaptation(int* lightvar)
{
    // The default range.
    int range = MOD_RANGE / 2;
    int lightval;

    if(lightvar == NULL)
        return; // Can't apply adaptation to a NULL val ptr...

    lightval = *lightvar;

    if(lightval > 255)
        lightval = 255;
    else if(lightval < 0)
        lightval = 0;

    // Apply light adaptation?
    if(r_lightAdapt)
        range = playerLightRange[viewplayer - players];

    *lightvar += lightRangeModMatrix[range][lightval];
}

/*
 * Same as above but instead of applying light adaptation to the var directly
 * it returns the light adaptation value for the passed light value.
 *
 * @param lightvalue    Light value to look up the adaptation value of.
 * @return int          Adaptation value for the passed light value.
 */
int Rend_GetLightAdaptVal(int lightvalue)
{
    int range = MOD_RANGE / 2;
    int lightval = lightvalue;

    if(lightval > 255)
        lightval = 255;
    else if(lightval < 0)
        lightval = 0;

    if(r_lightAdapt)
        range = playerLightRange[viewplayer - players];

    return lightRangeModMatrix[range][lightval];
}

/*
 * Creates new occlusion planes from the subsector's sides.
 * Before testing, occlude subsector's backfaces. After testing occlude
 * the remaining faces, i.e. the forward facing segs. This is done before
 * rendering segs, so solid segments cut out all unnecessary oranges.
 */
void Rend_OccludeSubsector(subsector_t *sub, boolean forward_facing)
{
    sector_t *front = sub->sector, *back;
    int     segfacing;
    int     i;
    float   v1[2], v2[2], fronth[2], backh[2];
    float  *startv, *endv;
    seg_t  *seg;

    fronth[0] = FIX2FLT(front->floorheight);
    fronth[1] = FIX2FLT(front->ceilingheight);

    for(i = 0; i < sub->linecount; i++)
    {
        seg = SEG_PTR(sub->firstline + i);
        // Occlusions can only happen where two sectors contact.
        if(!seg->linedef || !seg->backsector)
            continue;
        back = seg->backsector;
        v1[VX] = FIX2FLT(seg->v1->x);
        v1[VY] = FIX2FLT(seg->v1->y);
        v2[VX] = FIX2FLT(seg->v2->x);
        v2[VY] = FIX2FLT(seg->v2->y);
        // Which way should it be facing?
        segfacing = Rend_SegFacingDir(v1, v2);  // 1=front
        if(forward_facing != (segfacing != 0))
            continue;
        backh[0] = FIX2FLT(back->floorheight);
        backh[1] = FIX2FLT(back->ceilingheight);
        // Choose start and end vertices so that it's facing forward.
        if(forward_facing)
        {
            startv = v1;
            endv = v2;
        }
        else
        {
            startv = v2;
            endv = v1;
        }
        // Do not create an occlusion for sky floors.
        if(back->floorpic != skyflatnum || front->floorpic != skyflatnum)
        {
            // Do the floors create an occlusion?
            if((backh[0] > fronth[0] && vy <= backh[0]) ||
               (backh[0] < fronth[0] && vy >= fronth[0]))
            {
                // Occlude down.
                C_AddViewRelOcclusion(startv, endv, MAX_OF(fronth[0], backh[0]), false);
            }
        }
        // Do not create an occlusion for sky ceilings.
        if(back->ceilingpic != skyflatnum || front->ceilingpic != skyflatnum)
        {
            // Do the ceilings create an occlusion?
            if((backh[1] < fronth[1] && vy >= backh[1]) ||
               (backh[1] > fronth[1] && vy <= fronth[1]))
            {
                // Occlude up.
                C_AddViewRelOcclusion(startv, endv, MIN_OF(fronth[1], backh[1]), true);
            }
        }
    }
}

void Rend_RenderPlane(planeinfo_t *plane, dynlight_t *lights,
                      subsector_t *subsector, sectorinfo_t *sin)
{
    rendpoly_t poly;
    sector_t *sector = subsector->sector, *link = NULL;
    int     planepic;
    float   height;

    // We're creating a flat.
    poly.type = RP_FLAT;
    poly.lights = lights;

    if(plane->isfloor)
    {
        // Determine the height of the floor.
        if(sin->linkedfloor)
        {
            poly.sector = link = R_GetLinkedSector(sin->linkedfloor, true);

            // This sector has an invisible floor.
            height = SECT_FLOOR(link);
            planepic = link->floorpic;
        }
        else
        {
            poly.sector = sector;
            height = sin->visfloor;
            planepic = sector->floorpic;
        }
    }
    else
    {
        // This is a ceiling plane.
        if(sin->linkedceil)
        {
            poly.sector = link = R_GetLinkedSector(sin->linkedceil, false);

            // This sector has an invisible ceiling.
            height = SECT_CEIL(link);
            planepic = link->ceilingpic;
        }
        else
        {
            poly.sector = sector;
            height = sin->visceil + sector->skyfix;
            planepic = sector->ceilingpic;
        }
    }

    // Has the texture changed?
    if(planepic != plane->pic)
    {
        plane->pic = planepic;

        // The sky?
        if(planepic == skyflatnum)
            plane->flags |= RPF_SKY_MASK;
        else
            plane->flags &= ~RPF_SKY_MASK;
    }
    poly.flags = plane->flags;

    // Is the plane visible?
    if((plane->isfloor && vy > height) || (!plane->isfloor && vy < height))
    {
        // Check for sky.
        if(plane->pic == skyflatnum)
        {
            poly.lights = NULL;
            skyhemispheres |= (plane->isfloor ? SKYHEMI_LOWER : SKYHEMI_UPPER);
        }
        else
        {
            if(planepic == -1)
            {
                // A bad texture name, draw the "missing" graphic
                poly.tex.id = curtex = (unsigned int) dd_textures[DDTEX_MISSING];
                poly.tex.width = poly.tex.height = 64;
                poly.tex.detail = NULL;
            }
            else
            {
                poly.tex.id = curtex = GL_PrepareFlat(planepic);
                poly.tex.detail = texdetail;
                poly.tex.width = texw;
                poly.tex.height = texh;
            }

            if(plane->isfloor)
            {
                poly.texoffx = sector->flooroffx;
                poly.texoffy = sector->flooroffy;
            }
            else
            {
                poly.texoffx = sector->ceiloffx;
                poly.texoffy = sector->ceiloffy;
            }
        }
        poly.top = height;

        RL_PrepareFlat(plane, &poly, subsector);

        SB_RendPoly(&poly, plane->isfloor, sector, plane->illumination,
                    &plane->tracker, GET_SUBSECTOR_IDX(subsector));

        Rend_PolyFlatBlend(plane->pic, &poly);
        RL_AddPoly(&poly);

        // Add a shine to this flat existence.
        if(planepic)
            RL_AddShinyFlat(plane, &poly, subsector);
    }
}

void Rend_RenderSubsector(int ssecidx)
{
    subsector_t *ssec = SUBSECTOR_PTR(ssecidx);
    int     i;
    unsigned long j;
    seg_t  *seg;
    sector_t *sect = ssec->sector;
    int     sectoridx = GET_SECTOR_IDX(sect);
    sectorinfo_t *sin = secinfo + sectoridx;
    int     flags = 0;
    float   sceil = sin->visceil, sfloor = sin->visfloor;
    lumobj_t *lumi;             // Lum Iterator, or 'snow' in Finnish. :-)
    subsectorinfo_t *subin;

    if(sceil - sfloor <= 0 || ssec->numverts < 3)
    {
        // Skip this, it has no volume.
        // Neighbors handle adding the solid clipper segments.
        return;
    }

    if(!firstsubsector)
    {
        if(!C_CheckSubsector(ssec))
            return;             // This isn't visible.
    }
    else
    {
        firstsubsector = false;
    }

    // Mark the sector visible for this frame.
    sin->flags |= SIF_VISIBLE;

    // Dynamic lights.
    if(useDynLights)
        DL_ProcessSubsector(ssec);

    // Prepare for FakeRadio.
    Rend_RadioInitForSector(sect);
    Rend_RadioSubsectorEdges(ssec);

    Rend_OccludeSubsector(ssec, false);
    // Determine which dynamic light sources in the subsector get clipped.
    for(lumi = dlSubLinks[ssecidx]; lumi; lumi = lumi->ssNext)
    {
        lumi->flags &= ~LUMF_CLIPPED;
        // FIXME: Determine the exact centerpoint of the light in
        // DL_AddLuminous!
        if(!C_IsPointVisible
           (FIX2FLT(lumi->thing->x), FIX2FLT(lumi->thing->y),
            FIX2FLT(lumi->thing->z) + lumi->center))
            lumi->flags |= LUMF_CLIPPED;    // Won't have a halo.
    }
    Rend_OccludeSubsector(ssec, true);

    if(ssec->poly)
    {
        // Polyobjs don't obstruct, do clip lights with another algorithm.
        DL_ClipBySight(ssecidx);
    }

    // Mark the particle generators in the sector visible.
    PG_SectorIsVisible(sect);

    // Sprites for this sector have to be drawn. This must be done before
    // the segments of this subsector are added to the clipper. Otherwise
    // the sprites would get clipped by them, and that wouldn't be right.
    R_AddSprites(sect);

    // Draw the walls.
    for(j = ssec->linecount, seg = &segs[ssec->firstline]; j > 0;
        --j, seg++)
    {
        if(seg->linedef)    // "minisegs" have no linedefs.
            Rend_RenderWallSeg(seg, sect, flags);
    }

    // Is there a polyobj on board?
    if(ssec->poly)
    {
        for(i = 0; i < ssec->poly->numsegs; i++)
            Rend_RenderWallSeg(ssec->poly->segs[i], sect, RWSF_NO_RADIO);
    }

    subin = &subsecinfo[ssecidx];
    Rend_RenderPlane(&subin->floor, floorLightLinks[ssecidx], ssec, sin);
    Rend_RenderPlane(&subin->ceil, ceilingLightLinks[ssecidx], ssec, sin);
}

void Rend_RenderNode(int bspnum)
{
    node_t *bsp;
    int     side;

    // If the clipper is full we're pretty much done.
    if(C_IsFull())
        return;

    if(bspnum & NF_SUBSECTOR)
    {
        if(bspnum == -1)
            Rend_RenderSubsector(0);
        else
            Rend_RenderSubsector(bspnum & (~NF_SUBSECTOR));
        return;
    }

    bsp = NODE_PTR(bspnum);

    // Decide which side the view point is on.
    side = R_PointOnSide(viewx, viewy, bsp);

    Rend_RenderNode(bsp->children[side]);   // Recursively divide front space.
    Rend_RenderNode(bsp->children[side ^ 1]);   // ...and back space.
}

void Rend_RenderMap(void)
{
    binangle_t viewside;

    // Set to true if dynlights are inited for this frame.
    dlInited = false;

    // This is all the clearing we'll do.
    gl.Clear(DGL_DEPTH_BUFFER_BIT);

    // Setup the modelview matrix.
    Rend_ModelViewMatrix(true);

    // Calculate the light range to be used for each player
    Rend_RetrieveLightSample();

    if(!freezeRLs)
    {
        // Prepare for rendering.
        R_UpdatePlanes();       // Update all planes.
        RL_ClearLists();        // Clear the lists for new quads.
        C_ClearRanges();        // Clear the clipper.
        R_ClearSectorFlags();
        DL_ClearForFrame();     // Zeroes the links.
        LG_Update();
        SB_BeginFrame();

        // Generate surface decorations for the frame.
        Rend_InitDecorationsForFrame();

        // Maintain luminous objects.
        if(useDynLights || haloMode || litSprites || useDecorations)
        {
            DL_InitForNewFrame();
        }

        // Add the backside clipping range (if vpitch allows).
        if(vpitch <= 90 - yfov / 2 && vpitch >= -90 + yfov / 2)
        {
            float   a = fabs(vpitch) / (90 - yfov / 2);
            binangle_t startAngle =
                (binangle_t) (BANG_45 * fieldOfView / 90) * (1 + a);
            binangle_t angLen = BANG_180 - startAngle;

            viewside = (viewangle >> (32 - BAMS_BITS)) + startAngle;
            C_SafeAddRange(viewside, viewside + angLen);
            C_SafeAddRange(viewside + angLen, viewside + 2 * angLen);
        }
        // The viewside line for the depth cue.
        viewsidex = -FIX2FLT(viewsin);
        viewsidey = FIX2FLT(viewcos);

        // We don't want subsector clipchecking for the first subsector.
        firstsubsector = true;

        Rend_RenderNode(numnodes - 1);

        // Make vissprites of all the visible decorations.
        Rend_ProjectDecorations();

        Rend_RenderShadows();

        // Wrap up with Shadow Bias.
        SB_EndFrame();
    }
    RL_RenderAllLists();

    // Draw the Shadow Bias Editor's draw that identifies the current
    // light.
    SBE_DrawCursor();
}

static void DrawRangeBox(int x, int y, int w, int h, ui_color_t *c)
{
    UI_GradientEx(x, y, w, h, 6,
                  c ? c : UI_COL(UIC_BG_MEDIUM),
                  c ? c : UI_COL(UIC_BG_LIGHT),
                  1, 1);
    UI_DrawRectEx(x, y, w, h, 6, false, c ? c : UI_COL(UIC_BRD_HI),
                  NULL, 1, -1);
}

#define bWidth 1.0f
#define bHeight ((bWidth / MOD_RANGE) * 255.0f)
#define BORDER 20

/*
 * Draws the lightRangeModMatrix (for debug)
 */
void R_DrawLightRange(void)
{
    int i, r;
    int y;
    char *title = "Light Range Matrix";
    signed short v;
    float barwidth = 2.5f;
    byte a, c;
    ui_color_t color;

    if(!debugLightModMatrix)
        return;

    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

    gl.Translatef(BORDER, BORDER + BORDER, 0);

    color.red = 0.2f;
    color.green = 0;
    color.blue = 0.6f;

    DrawRangeBox(0 - (BORDER / 2), 0 - BORDER - 10,
                 bWidth * 255 + BORDER,
                 bHeight * MOD_RANGE + BORDER + BORDER, &color);

    gl.Disable(DGL_TEXTURING);

    for(r = MOD_RANGE, y = 0; r > 0; r--, y++)
    {
        gl.Begin(DGL_QUADS);
        for(i = 0; i < 255; i++)
        {
            // Get the result of the source light level + offset
            v = i + lightRangeModMatrix[r-1][i];

            c = (byte) v;

            // Show the ambient light modifier using alpha
            if(r_ambient < 0 && i > r_ambient)
                a = 128;
            else if(r_ambient > 0 && i < r_ambient)
                a = 128;
            else
                a = 255;

            // Draw in red if the range matches that of the current viewplayer
            if(r == playerLightRange[viewplayer - players])
                gl.Color4ub(255, 0, 0, 255);
            else
                gl.Color4ub(c, c, c, a);

            gl.Vertex2f(i * bWidth, y * bHeight);
            gl.Vertex2f(i * bWidth + bWidth, y * bHeight);
            gl.Vertex2f(i * bWidth + bWidth,
                        y * bHeight + bHeight);
            gl.Vertex2f(i * bWidth, y * bHeight + bHeight);
        }
        gl.End();
    }

    gl.Enable(DGL_TEXTURING);

    UI_TextOutEx(title, ((bWidth * 255) / 2),
                 -12, true, true, UI_COL(UIC_TITLE), .7f);

    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
}

// Console commands.
D_CMD(Fog)
{
    int     i;

    if(argc == 1)
    {
        Con_Printf("Usage: %s (cmd) (args)\n", argv[0]);
        Con_Printf("Commands: on, off, mode, color, start, end, density.\n");
        Con_Printf("Modes: linear, exp, exp2.\n");
        //Con_Printf( "Hints: fastest, nicest, dontcare.\n");
        Con_Printf("Color is given as RGB (0-255).\n");
        Con_Printf
            ("Start and end are for linear fog, density for exponential.\n");
        return true;
    }
    if(!stricmp(argv[1], "on"))
    {
        GL_UseFog(true);
        Con_Printf("Fog is now active.\n");
    }
    else if(!stricmp(argv[1], "off"))
    {
        GL_UseFog(false);
        Con_Printf("Fog is now disabled.\n");
    }
    else if(!stricmp(argv[1], "mode") && argc == 3)
    {
        if(!stricmp(argv[2], "linear"))
        {
            gl.Fog(DGL_FOG_MODE, DGL_LINEAR);
            Con_Printf("Fog mode set to linear.\n");
        }
        else if(!stricmp(argv[2], "exp"))
        {
            gl.Fog(DGL_FOG_MODE, DGL_EXP);
            Con_Printf("Fog mode set to exp.\n");
        }
        else if(!stricmp(argv[2], "exp2"))
        {
            gl.Fog(DGL_FOG_MODE, DGL_EXP2);
            Con_Printf("Fog mode set to exp2.\n");
        }
        else
            return false;
    }
    else if(!stricmp(argv[1], "color") && argc == 5)
    {
        for(i = 0; i < 3; i++)
            fogColor[i] = strtol(argv[2 + i], NULL, 0) /*/255.0f */ ;
        fogColor[3] = 255;
        gl.Fogv(DGL_FOG_COLOR, fogColor);
        Con_Printf("Fog color set.\n");
    }
    else if(!stricmp(argv[1], "start") && argc == 3)
    {
        gl.Fog(DGL_FOG_START, strtod(argv[2], NULL));
        Con_Printf("Fog start distance set.\n");
    }
    else if(!stricmp(argv[1], "end") && argc == 3)
    {
        gl.Fog(DGL_FOG_END, strtod(argv[2], NULL));
        Con_Printf("Fog end distance set.\n");
    }
    else if(!stricmp(argv[1], "density") && argc == 3)
    {
        gl.Fog(DGL_FOG_DENSITY, strtod(argv[2], NULL));
        Con_Printf("Fog density set.\n");
    }
    else
        return false;
    // Exit with a success.
    return true;
}
