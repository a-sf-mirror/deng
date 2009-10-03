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

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Rend_DrawBBox(const float pos3f[3], float w, float l, float h,
                   const float color3f[3], float alpha, float br,
                   boolean alignToBase);
void Rend_DrawArrow(const float pos3f[3], angle_t a, float s,
                    const float color3f[3], float alpha);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void Rend_RenderBoundingBoxes(void);
static DGLuint constructBBox(DGLuint name, float br);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int useDynlights, translucentIceCorpse;
extern int loMaxRadius;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean usingFog = false; // Is the fog in use?
float fogColor[4];
float fieldOfView = 95.0f;
byte smoothTexAnim = true;
int useShinySurfaces = true;

float vx, vy, vz, vang, vpitch;
float viewsidex, viewsidey;

byte freezeRLs = false;
int devSkyMode = false;

int missileBlend = 1;
// Ambient lighting, rAmbient is used within the renderer, ambientLight is
// used to store the value of the ambient light cvar.
// The value chosen for rAmbient occurs in Rend_CalcLightModRange
// for convenience (since we would have to recalculate the matrix anyway).
int rAmbient = 0, ambientLight = 0;

int viewpw, viewph; // Viewport size, in pixels.
int viewpx, viewpy; // Viewpoint top left corner, in pixels.

float yfov;

int gameDrawHUD = 1; // Set to zero when we advise that the HUD should not be drawn

float lightRangeCompression = 0;
float lightModRange[255];
byte devLightModRange = 0;

float rendLightDistanceAttentuation = 1024;

int devMobjBBox = 0; // 1 = Draw mobj bounding boxes (for debug)
DGLuint dlBBox = 0; // Display list: active-textured bbox model.

byte devVertexIndices = 0; // @c 1= Draw world vertex indices (for debug).
byte devVertexBars = 0; // @c 1= Draw world vertex position bars.
byte devSurfaceNormals = 0; // @c 1= Draw world surface normal tails.
byte devNoTexFix = 0;

// Current sector light color.
const float* sLightColor;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstsubsector; // No range checking for the first one.

// CODE --------------------------------------------------------------------

void Rend_Register(void)
{
    C_VAR_INT("rend-dev-sky", &devSkyMode, CVF_NO_ARCHIVE, 0, 2);
    C_VAR_BYTE("rend-dev-freeze", &freezeRLs, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-cull-subsectors", &devNoCulling,CVF_NO_ARCHIVE,0,1);
    C_VAR_INT("rend-dev-mobj-bbox", &devMobjBBox, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_FLOAT("rend-camera-fov", &fieldOfView, 0, 1, 179);
    C_VAR_BYTE("rend-tex-anim-smooth", &smoothTexAnim, 0, 0, 1);
    C_VAR_INT("rend-tex-shiny", &useShinySurfaces, 0, 0, 1);
    C_VAR_FLOAT2("rend-light-compression", &lightRangeCompression, 0, -1, 1,
                 Rend_CalcLightModRange);
    C_VAR_INT2("rend-light-ambient", &ambientLight, 0, 0, 255,
               Rend_CalcLightModRange);
    C_VAR_INT2("rend-light-sky", &rendSkyLight, 0, 0, 1,
               LG_MarkAllForUpdate);
    C_VAR_FLOAT("rend-light-wall-angle", &rendLightWallAngle, CVF_NO_MAX,
                0, 0);
    C_VAR_FLOAT("rend-light-attenuation", &rendLightDistanceAttentuation,
                CVF_NO_MAX, 0, 0);
    C_VAR_BYTE("rend-dev-light-mod", &devLightModRange, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-tex-showfix", &devNoTexFix, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-blockmap-debug", &bmapShowDebug, CVF_NO_ARCHIVE, 0, 3);
    C_VAR_FLOAT("rend-dev-blockmap-debug-size", &bmapDebugSize, CVF_NO_ARCHIVE, .1f, 100);
    C_VAR_BYTE("rend-dev-vertex-show-indices", &devVertexIndices, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-vertex-show-bars", &devVertexBars, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-surface-show-normals", &devSurfaceNormals, CVF_NO_ARCHIVE, 0, 1);

    RL_Register();
    DL_Register();
    SB_Register();
    LG_Register();
    Rend_ModelRegister();
    Rend_ParticleRegister();
    Rend_RadioRegister();
    Rend_ShadowRegister();
    Rend_SkyRegister();
    Rend_SpriteRegister();
    Rend_ConsoleRegister();
}

#if 0 // unused atm
float Rend_SignedPointDist2D(float c[2])
{
    /*          (YA-YC)(XB-XA)-(XA-XC)(YB-YA)
       s = -----------------------------
       L**2
       Luckily, L**2 is one. dist = s*L. Even more luckily, L is also one.
     */
    return (vz - c[VY]) * viewsidex - (vx - c[VX]) * viewsidey;
}
#endif

/**
 * Approximated! The Z axis aspect ratio is corrected.
 */
float Rend_PointDist3D(const float c[3])
{
    return M_ApproxDistance3f(vx - c[VX], vz - c[VY], 1.2f * (vy - c[VZ]));
}

void Rend_Init(void)
{
    C_Init(); // Clipper.
    RL_Init(); // Rendering lists.
}

/**
 * Used to be called before starting a new map.
 */
void Rend_Reset(void)
{
    // Textures are deleted (at least skies need this???).
    GL_ClearRuntimeTextures();
    LO_Clear(); // Free lumobj stuff.

    if(dlBBox)
        GL_DeleteLists(dlBBox, 1);
    dlBBox = 0;
}

void Rend_ModelViewMatrix(boolean useAngles)
{
    vx = viewX;
    vy = viewZ;
    vz = viewY;
    vang = viewAngle / (float) ANGLE_MAX *360 - 90;
    vpitch = viewPitch * 85.0 / 110.0;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if(useAngles)
    {
        glRotatef(vpitch, 1, 0, 0);
        glRotatef(vang, 0, 1, 0);
    }
    glScalef(1, 1.2f, 1);      // This is the aspect correction.
    glTranslatef(-vx, -vy, -vz);
}

float Rend_FacingViewerDot(float v1[2], float v2[2])
{
    // The dot product.
    return (v1[VY] - v2[VY]) * (v1[VX] - vx) + (v2[VX] - v1[VX]) * (v1[VY] - vz);
}

#if 0 // unused atm
static int Rend_FixedSegFacingDir(const hedge_t *seg)
{
    // The dot product. (1 if facing front.)
    return((seg->fv1.pos[VY] - seg->fv2.pos[VY]) * (seg->fv1.pos[VX] - viewX) +
           (seg->fv2.pos[VX] - seg->fv1.pos[VX]) * (seg->fv1.pos[VY] - viewY)) > 0;
}
#endif

#if 0 // unused atm
int Rend_SegFacingPoint(float v1[2], float v2[2], float pnt[2])
{
    float   nx = v1[VY] - v2[VY], ny = v2[VX] - v1[VX];
    float   vvx = v1[VX] - pnt[VX], vvy = v1[VY] - pnt[VY];

    // The dot product.
    if(nx * vvx + ny * vvy > 0)
        return 1;               // Facing front.
    return 0;                   // Facing away.
}
#endif

static int C_DECL DivSortAscend(const void* e1, const void* e2)
{
    float               f1 = (*((const plane_t**) e1))->visHeight;
    float               f2 = (*((const plane_t**) e2))->visHeight;

    if(f1 > f2)
        return 1;
    if(f2 > f1)
        return -1;
    return 0;
}

static int C_DECL DivSortDescend(const void* e1, const void* e2)
{
    float               f1 = (*((const plane_t**) e1))->visHeight;
    float               f2 = (*((const plane_t**) e2))->visHeight;

    if(f1 > f2)
        return -1;
    if(f2 > f1)
        return 1;
    return 0;
}

void Rend_VertexColorsGlow(rcolor_t* colors, size_t num)
{
    size_t              i;

    for(i = 0; i < num; ++i)
    {
        rcolor_t*           c = &colors[i];
        c->rgba[CR] = c->rgba[CG] = c->rgba[CB] = 1;
    }
}

void Rend_VertexColorsAlpha(rcolor_t* colors, size_t num, float alpha)
{
    size_t               i;

    for(i = 0; i < num; ++i)
    {
        colors[i].rgba[CA] = alpha;
    }
}

void Rend_ApplyTorchLight(float* color, float distance)
{
    ddplayer_t*         ddpl = &viewPlayer->shared;

    if(!ddpl->fixedColorMap)
        return;

    // Check for torch.
    if(distance < 1024)
    {
        // Colormap 1 is the brightest. I'm guessing 16 would be
        // the darkest.
        int                 ll = 16 - ddpl->fixedColorMap;
        float               d = (1024 - distance) / 1024.0f * ll / 15.0f;

        if(torchAdditive)
        {
            color[CR] += d * torchColor[CR];
            color[CG] += d * torchColor[CG];
            color[CB] += d * torchColor[CB];
        }
        else
        {
            color[CR] += d * ((color[CR] * torchColor[CR]) - color[CR]);
            color[CG] += d * ((color[CG] * torchColor[CG]) - color[CG]);
            color[CB] += d * ((color[CB] * torchColor[CB]) - color[CB]);
        }
    }
}

static void lightVertex(rcolor_t* color, const rvertex_t* vtx,
                        float lightLevel, const float* ambientColor)
{
    float               lightVal, dist;

    dist = Rend_PointDist2D(vtx->pos);

    // Apply distance attenuation.
    lightVal = R_DistAttenuateLightLevel(dist, lightLevel);

    // Add extra light.
    lightVal += R_ExtraLightDelta();

    Rend_ApplyLightAdaptation(&lightVal);

    // Mix with the surface color.
    color->rgba[CR] = lightVal * ambientColor[CR];
    color->rgba[CG] = lightVal * ambientColor[CG];
    color->rgba[CB] = lightVal * ambientColor[CB];
}

void Rend_VertexColorsApplyTorchLight(rcolor_t* colors,
                                      const rvertex_t* vertices,
                                      size_t numVertices)
{
    size_t              i;
    ddplayer_t*         ddpl = &viewPlayer->shared;

    if(!ddpl->fixedColorMap)
        return; // No need, its disabled.

    for(i = 0; i < numVertices; ++i)
    {
        const rvertex_t*    vtx = &vertices[i];
        rcolor_t*           c = &colors[i];

        Rend_ApplyTorchLight(c->rgba, Rend_PointDist2D(vtx->pos));
    }
}

static void preparePlane(rvertex_t* rvertices, const subsector_t* ssec,
                         float height, boolean antiClockwise)
{
    size_t              i = 0;
    hedge_t*            hEdge;

    if(ssec->useMidPoint)
    {
        rvertices[i].pos[VX] = ssec->midPoint.pos[VX];
        rvertices[i].pos[VY] = ssec->midPoint.pos[VY];
        rvertices[i].pos[VZ] = height;
        ++i;
    }

    // Copy the vertices in reverse order for ceilings (flip faces).
    hEdge = ssec->firstFanHEdge;
    do
    {
        rvertices[i].pos[VX] = hEdge->HE_v1pos[VX];
        rvertices[i].pos[VY] = hEdge->HE_v1pos[VY];
        rvertices[i].pos[VZ] = height;
        ++i;
    } while((hEdge = (antiClockwise? hEdge->prev : hEdge->next)) != ssec->firstFanHEdge);

    if(ssec->useMidPoint)
    {
        rvertices[i].pos[VX] = ssec->firstFanHEdge->HE_v1pos[VX];
        rvertices[i].pos[VY] = ssec->firstFanHEdge->HE_v1pos[VY];
        rvertices[i].pos[VZ] = height;
    }
}

/**
 * \fixme No need to do this each frame. Set a flag in sidedef_t->flags to
 * denote this. Is sensitive to plane heights, surface properties
 * (e.g. alpha) and surface texture properties.
 */
boolean Rend_DoesMidTextureFillGap(linedef_t *line, int backside)
{
    // Check for unmasked midtextures on twosided lines that completely
    // fill the gap between floor and ceiling (we don't want to give away
    // the location of any secret areas (false walls)).
    if(LINE_BACKSIDE(line))
    {
        sector_t*           front = LINE_SECTOR(line, backside);
        sector_t*           back  = LINE_SECTOR(line, backside^1);
        sidedef_t*          side  = LINE_SIDE(line, backside);

        if(side->SW_middlematerial)
        {
            material_t*         mat = side->SW_middlematerial;
            material_snapshot_t ms;

            // Ensure we have up to date info.
            Material_Prepare(&ms, mat, true, NULL);

            if(ms.isOpaque && !side->SW_middleblendmode &&
               side->SW_middlergba[3] >= 1)
            {
                float               openTop[2], matTop[2];
                float               openBottom[2], matBottom[2];

                if(side->flags & SDF_MIDDLE_STRETCH)
                    return true;

                openTop[0] = openTop[1] = matTop[0] = matTop[1] =
                    MIN_OF(back->SP_ceilvisheight, front->SP_ceilvisheight);
                openBottom[0] = openBottom[1] = matBottom[0] = matBottom[1] =
                    MAX_OF(back->SP_floorvisheight, front->SP_floorvisheight);

                // Could the mid texture fill enough of this gap for us
                // to consider it completely closed?
                if(ms.height >= (openTop[0] - openBottom[0]) &&
                   ms.height >= (openTop[1] - openBottom[1]))
                {
                    // Possibly. Check the placement of the mid texture.
                    if(Rend_MidMaterialPos
                       (&matBottom[0], &matBottom[1], &matTop[0], &matTop[1],
                        NULL, side->SW_middlevisoffset[VY], ms.height,
                        0 != (line->flags & DDLF_DONTPEGBOTTOM),
                        !(IS_SKYSURFACE(&front->SP_ceilsurface) &&
                          IS_SKYSURFACE(&back->SP_ceilsurface)),
                        !(IS_SKYSURFACE(&front->SP_floorsurface) &&
                          IS_SKYSURFACE(&back->SP_floorsurface))))
                    {
                        if(matTop[0] >= openTop[0] &&
                           matTop[1] >= openTop[1] &&
                           matBottom[0] <= openBottom[0] &&
                           matBottom[1] <= openBottom[1])
                            return true;
                    }
                }
            }
        }
    }

    return false;
}

static void markSideSectionsPVisible(const linedef_t* line, byte sid)
{
    byte                j;
    sidedef_t*          side;

    if(!(side = LINE_SIDE(line, sid)))
        return;

    for(j = 0; j < 3; ++j)
        side->sections[j].inFlags |= SUIF_PVIS;

    // Top
    if(!LINE_BACKSIDE(line))
    {
        side->sections[SEG_TOP].inFlags &= ~SUIF_PVIS;
        side->sections[SEG_BOTTOM].inFlags &= ~SUIF_PVIS;
    }
    else
    {
        // Check middle texture
        if((!side->SW_middlematerial ||
            (side->SW_middlematerial->flags & MATF_NO_DRAW)) ||
            side->SW_middlergba[3] <= 0) // Check alpha
            side->sections[SEG_MIDDLE].inFlags &= ~SUIF_PVIS;

        if(IS_SKYSURFACE(&LINE_BACKSECTOR(line)->SP_ceilsurface) &&
           IS_SKYSURFACE(&LINE_FRONTSECTOR(line)->SP_ceilsurface))
           side->sections[SEG_TOP].inFlags &= ~SUIF_PVIS;
        else
        {
            if(sid != 0)
            {
                if(LINE_BACKSECTOR(line)->SP_ceilvisheight <=
                   LINE_FRONTSECTOR(line)->SP_ceilvisheight)
                    side->sections[SEG_TOP].inFlags &= ~SUIF_PVIS;
            }
            else
            {
                if(LINE_FRONTSECTOR(line)->SP_ceilvisheight <=
                   LINE_BACKSECTOR(line)->SP_ceilvisheight)
                    side->sections[SEG_TOP].inFlags &= ~SUIF_PVIS;
            }
        }

        // Bottom
        if(IS_SKYSURFACE(&LINE_BACKSECTOR(line)->SP_floorsurface) &&
           IS_SKYSURFACE(&LINE_FRONTSECTOR(line)->SP_floorsurface))
           side->sections[SEG_BOTTOM].inFlags &= ~SUIF_PVIS;
        else
        {
            if(sid != 0)
            {
                if(LINE_BACKSECTOR(line)->SP_floorvisheight >=
                   LINE_FRONTSECTOR(line)->SP_floorvisheight)
                    side->sections[SEG_BOTTOM].inFlags &= ~SUIF_PVIS;
            }
            else
            {
                if(LINE_FRONTSECTOR(line)->SP_floorvisheight >=
                   LINE_BACKSECTOR(line)->SP_floorvisheight)
                    side->sections[SEG_BOTTOM].inFlags &= ~SUIF_PVIS;
            }
        }
    }
}

static void Rend_MarkSegSectionsPVisible(hedge_t* hEdge)
{
    seg_t*              seg;

    if(!hEdge || !((seg_t*) hEdge->data)->lineDef)
        return; // huh?

    seg = ((seg_t*) hEdge->data);
    markSideSectionsPVisible(seg->lineDef, seg->side);

    if(hEdge->twin)
    {
        seg = ((seg_t*) hEdge->twin->data);
        markSideSectionsPVisible(seg->lineDef, seg->side);
    }
}

static boolean testForPlaneDivision(walldiv_t* wdiv, plane_t* pln,
                                    float bottomZ, float topZ)
{
    if(pln->visHeight > bottomZ && pln->visHeight < topZ)
    {
        uint                i;

        // If there is already a division at this height, ignore this plane.
        for(i = 0; i < wdiv->num; ++i)
            if(wdiv->divs[i]->visHeight == pln->visHeight)
                return true; // Continue.

        // A new division.
        wdiv->divs[wdiv->num++] = pln;

        // Have we reached the div limit?
        if(wdiv->num == RL_MAX_DIVS)
            return false; // Stop.
    }

    return true; // Continue.
}

static void doFindSegDivisions(walldiv_t* div, const hedge_t* hEdge,
                               const sector_t* frontSec, float bottomZ,
                               float topZ, boolean doRight)
{
    linedef_t*          iter;
    lineowner_t*        base, *own;
    boolean             clockwise = !doRight;

    if(bottomZ >= topZ)
        return; // Obviously no division.

    // Retrieve the start owner node.
    {
    const seg_t*        seg = ((seg_t*) hEdge->data);
    base = own = R_GetVtxLineOwner(seg->lineDef->L_v(seg->side^doRight),
                                   seg->lineDef);
    }

    /**
     * We need to handle the special case of a sector with zero volume.
     * In this instance, the only potential divisor in the sector is the back
     * ceiling. This is because elsewhere we automatically fix the case of a
     * floor above a ceiling by lowering the floor.
     */
    while((own = own->link[clockwise]) != base)
    {
        boolean             sectorHasVolume = false;
        sidedef_t*          side;
        
        iter = own->lineDef;

        if(LINE_SELFREF(iter))
            continue;

        side = LINE_SIDE(iter,
            LINE_FRONTSECTOR(iter) == frontSec? BACK : FRONT);

        if(side)
        {
            plane_t*            pln;
            sector_t*           scanSec = side->sector;

            if(scanSec->SP_ceilvisheight - scanSec->SP_floorvisheight > 0)
                sectorHasVolume = true;

            if(sectorHasVolume)
            {
                // First, the floor.
                pln = scanSec->SP_plane(PLN_FLOOR);
                if(testForPlaneDivision(div, pln, bottomZ, topZ))
                {   // Clip a range bound to this height?
                    if(pln->visHeight > bottomZ)
                        bottomZ = pln->visHeight;

                    // All clipped away?
                    if(bottomZ >= topZ)
                        break;
                }

                // Next, every plane between floor and ceiling.
                if(scanSec->planeCount > 2)
                {
                    uint                j;
                    boolean             stop = false;

                    for(j = PLN_MID; j < scanSec->planeCount - 2 && !stop; ++j)
                    {
                        pln = scanSec->SP_plane(j);
                        if(!testForPlaneDivision(div, pln, bottomZ, topZ))
                            stop = true;
                    }

                    if(stop)
                        break;
                }
            }

            // Lastly, the ceiling.
            pln = scanSec->SP_plane(PLN_CEILING);
            if(testForPlaneDivision(div, pln, bottomZ, topZ) && sectorHasVolume)
            {   // Clip a range bound to this height?
                if(pln->visHeight < topZ)
                    topZ = pln->visHeight;

                // All clipped away?
                if(bottomZ >= topZ)
                    break;
            }
        }

        // Stop the scan when a solid neighbour is reached.
        if(!side || !sectorHasVolume)
            break;

        // Prepare for the next round.
        frontSec = side->sector;
    }
}

static void findSegDivisions(walldiv_t* div, const hedge_t* hEdge,
                             const sector_t* frontSec, float bottomZ,
                             float topZ, boolean doRight)
{
    seg_t*              seg = (seg_t*) hEdge->data;
    sidedef_t*          side = seg->sideDef;

    div->num = 0;

    if(!side)
        return;

    // Only segs at sidedef ends can/should be split.
    if(!((hEdge == side->line->hEdges[seg->side? 1 : 0] && !doRight) ||
         (hEdge == side->line->hEdges[!seg->side? 1 : 0] && doRight)))
        return;

    doFindSegDivisions(div, hEdge, frontSec, bottomZ, topZ, doRight);
}

/**
 * Division will only happen if it must be done.
 */
static void applyWallHeightDivision(walldiv_t* wdivs, const hedge_t* hEdge,
                                    const sector_t* frontsec, float low,
                                    float hi)
{
    uint                i;
    walldiv_t*          wdiv;

    if(!((seg_t*) hEdge->data)->lineDef)
        return; // Mini-segs arn't drawn.

    for(i = 0; i < 2; ++i)
    {
        wdiv = &wdivs[i];
        findSegDivisions(wdiv, hEdge, frontsec, low, hi, i);

        // We need to sort the divisions for the renderer.
        if(wdiv->num > 1)
        {
            // Sorting is required. This shouldn't take too long...
            // There seldom are more than one or two divisions.
            qsort(wdiv->divs, wdiv->num, sizeof(plane_t*),
                  i!=0 ? DivSortDescend : DivSortAscend);
        }

#ifdef RANGECHECK
{
uint        k;
for(k = 0; k < wdiv->num; ++k)
    if(wdiv->divs[k]->visHeight > hi || wdiv->divs[k]->visHeight < low)
    {
        Con_Error("DivQuad: i=%i, pos (%f), hi (%f), low (%f), num=%i\n",
                  i, wdiv->divs[k]->visHeight, hi, low, wdiv->num);
    }
}
#endif
    }
}

/**
 * Calculates the placement for a middle texture (top, bottom, offset).
 * texoffy may be NULL.
 * Returns false if the middle texture isn't visible (in the opening).
 */
int Rend_MidMaterialPos(float* bottomleft, float* bottomright,
                        float* topleft, float* topright, float* texoffy,
                        float tcyoff, float texHeight, boolean lowerUnpeg,
                        boolean clipTop, boolean clipBottom)
{
    int                 side;
    float               openingTop, openingBottom;
    boolean             visible[2] = {false, false};

    for(side = 0; side < 2; ++side)
    {
        openingTop = *(side? topright : topleft);
        openingBottom = *(side? bottomright : bottomleft);

        if(openingTop <= openingBottom)
            continue;

        // Else the mid texture is visible on this side.
        visible[side] = true;

        if(side == 0 && texoffy)
            *texoffy = 0;

        // We don't allow vertical tiling.
        if(lowerUnpeg)
        {
            *(side? bottomright : bottomleft) += tcyoff;
            *(side? topright : topleft) =
                *(side? bottomright : bottomleft) + texHeight;
        }
        else
        {
            *(side? topright : topleft) += tcyoff;
            *(side? bottomright : bottomleft) =
                *(side? topright : topleft) - texHeight;
        }

        // Clip it.
        if(clipBottom)
            if(*(side? bottomright : bottomleft) < openingBottom)
            {
                *(side? bottomright : bottomleft) = openingBottom;
            }

        if(clipTop)
            if(*(side? topright : topleft) > openingTop)
            {
                if(side == 0 && texoffy)
                    *texoffy += *(side? topright : topleft) - openingTop;
                *(side? topright : topleft) = openingTop;
            }
    }

    return (visible[0] || visible[1]);
}

static void selectSurfaceColors(const float** topColor,
                                const float** bottomColor, sidedef_t* side,
                                segsection_t section)
{
    // Select the colors for this surface.
    switch(section)
    {
    case SEG_MIDDLE:
        if(side->flags & SDF_BLENDMIDTOTOP)
        {
            *topColor = side->SW_toprgba;
            *bottomColor = side->SW_middlergba;
        }
        else if(side->flags & SDF_BLENDMIDTOBOTTOM)
        {
            *topColor = side->SW_middlergba;
            *bottomColor = side->SW_bottomrgba;
        }
        else
        {
            *topColor = side->SW_middlergba;
            *bottomColor = NULL;
        }
        break;

    case SEG_TOP:
        if(side->flags & SDF_BLENDTOPTOMID)
        {
            *topColor = side->SW_toprgba;
            *bottomColor = side->SW_middlergba;
        }
        else
        {
            *topColor = side->SW_toprgba;
            *bottomColor = NULL;
        }
        break;

    case SEG_BOTTOM:
        // Select the correct colors for this surface.
        if(side->flags & SDF_BLENDBOTTOMTOMID)
        {
            *topColor = side->SW_middlergba;
            *bottomColor = side->SW_bottomrgba;
        }
        else
        {
            *topColor = side->SW_bottomrgba;
            *bottomColor = NULL;
        }
        break;

    default:
        break;
    }
}

boolean RLIT_DynGetFirst(const dynlight_t* dyn, void* data)
{
    dynlight_t**        ptr = data;

    *ptr = (dynlight_t*) dyn;
    return false; // Stop iteration.
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
    vis->lumIdx = 0;
    vis->center[VX] = midpoint[VX];
    vis->center[VY] = midpoint[VY];
    vis->center[VZ] = midpoint[VZ];
    vis->isDecoration = false;
    vis->distance = Rend_PointDist2D(midpoint);
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
        DL_DynlistIterator(lightListIdx, RLIT_DynGetFirst, &dyn);

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

static void quadTexCoords(rtexcoord_t* tc, const rvertex_t* rverts,
                          float wallLength, const vectorcomp_t topLeft[3])
{
    tc[0].st[0] = tc[1].st[0] =
        rverts[0].pos[VX] - topLeft[VX];
    tc[3].st[1] = tc[1].st[1] =
        rverts[0].pos[VY] - topLeft[VY];
    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]);
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]);
}

static void quadLightCoords(rtexcoord_t* tc, const float s[2],
                            const float t[2])
{
    tc[1].st[0] = tc[0].st[0] = s[0];
    tc[1].st[1] = tc[3].st[1] = t[0];
    tc[3].st[0] = tc[2].st[0] = s[1];
    tc[2].st[1] = tc[0].st[1] = t[1];
}

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

typedef struct {
    uint            lastIdx;
    const rvertex_t* rvertices;
    uint            numVertices, realNumVertices;
    const float*    texTL, *texBR;
    boolean         isWall;
    const walldiv_t* divs;
} dynlightiterparams_t;

boolean RLIT_DynLightWrite(const dynlight_t* dyn, void* data)
{
    dynlightiterparams_t* params = data;

    // If multitexturing is in use, we skip the first light.
    if(!(RL_IsMTexLights() && params->lastIdx == 0))
    {
        uint                i;
        rvertex_t*          rvertices;
        rtexcoord_t*        rtexcoords;
        rcolor_t*           rcolors;
        rtexmapunit_t       rTU[NUM_TEXMAP_UNITS];

        memset(rTU, 0, sizeof(rTU));

        // Allocate enough for the divisions too.
        rvertices = R_AllocRendVertices(params->realNumVertices);
        rtexcoords = R_AllocRendTexCoords(params->realNumVertices);
        rcolors = R_AllocRendColors(params->realNumVertices);

        rTU[TU_PRIMARY].tex = dyn->texture;
        rTU[TU_PRIMARY].magMode = GL_LINEAR;

        rTU[TU_PRIMARY_DETAIL].tex = 0;
        rTU[TU_INTER].tex = 0;
        rTU[TU_INTER_DETAIL].tex = 0;

        for(i = 0; i < params->numVertices; ++i)
        {
            uint                c;
            rcolor_t*           col = &rcolors[i];

            // Each vertex uses the light's color.
            for(c = 0; c < 3; ++c)
                col->rgba[c] = dyn->color[c];
            col->rgba[3] = 1;
        }

        if(params->isWall)
        {
            rtexcoords[1].st[0] = rtexcoords[0].st[0] = dyn->s[0];
            rtexcoords[1].st[1] = rtexcoords[3].st[1] = dyn->t[0];
            rtexcoords[3].st[0] = rtexcoords[2].st[0] = dyn->s[1];
            rtexcoords[2].st[1] = rtexcoords[0].st[1] = dyn->t[1];

            if(params->divs && (params->divs[0].num || params->divs[1].num))
            {   // We need to subdivide the dynamic light quad.
                float               bL, tL, bR, tR;
                rvertex_t           origVerts[4];
                rcolor_t            origColors[4];
                rtexcoord_t         origTexCoords[4];

                /**
                 * Need to swap indices around into fans set the position
                 * of the division vertices, interpolate texcoords and
                 * color.
                 */

                memcpy(origVerts, params->rvertices, sizeof(rvertex_t) * 4);
                memcpy(origTexCoords, rtexcoords, sizeof(rtexcoord_t) * 4);
                memcpy(origColors, rcolors, sizeof(rcolor_t) * 4);

                bL = params->rvertices[0].pos[VZ];
                tL = params->rvertices[1].pos[VZ];
                bR = params->rvertices[2].pos[VZ];
                tR = params->rvertices[3].pos[VZ];

                R_DivVerts(rvertices, origVerts, params->divs);
                R_DivTexCoords(rtexcoords, origTexCoords, params->divs, bL, tL, bR, tR);
                R_DivVertColors(rcolors, origColors, params->divs, bL, tL, bR, tR);
            }
            else
            {
                memcpy(rvertices, params->rvertices, sizeof(rvertex_t) * params->numVertices);
            }
        }
        else
        {   // It's a flat.
            uint                i;
            float               width, height;

            width  = params->texBR[VX] - params->texTL[VX];
            height = params->texBR[VY] - params->texTL[VY];

            for(i = 0; i < params->numVertices; ++i)
            {
                rtexcoords[i].st[0] = ((params->texBR[VX] - params->rvertices[i].pos[VX]) / width * dyn->s[0]) +
                    ((params->rvertices[i].pos[VX] - params->texTL[VX]) / width * dyn->s[1]);

                rtexcoords[i].st[1] = ((params->texBR[VY] - params->rvertices[i].pos[VY]) / height * dyn->t[0]) +
                    ((params->rvertices[i].pos[VY] - params->texTL[VY]) / height * dyn->t[1]);
            }

            memcpy(rvertices, params->rvertices, sizeof(rvertex_t) * params->numVertices);
        }

        if(params->isWall &&
           params->divs && (params->divs[0].num || params->divs[1].num))
        {
            RL_AddPoly(PT_FAN, RPT_LIGHT, rvertices + 3 + params->divs[0].num,
                       rtexcoords + 3 + params->divs[0].num, NULL, NULL,
                       rcolors + 3 + params->divs[0].num,
                       3 + params->divs[1].num, 0,
                       0, NULL, rTU);
            RL_AddPoly(PT_FAN, RPT_LIGHT, rvertices, rtexcoords, NULL, NULL,
                       rcolors, 3 + params->divs[0].num, 0,
                       0, NULL, rTU);
        }
        else
        {
            RL_AddPoly(params->isWall? PT_TRIANGLE_STRIP : PT_FAN,
                       RPT_LIGHT,
                       rvertices, rtexcoords, NULL, NULL,
                       rcolors, params->numVertices, 0,
                       0, NULL, rTU);
        }

        R_FreeRendVertices(rvertices);
        R_FreeRendTexCoords(rtexcoords);
        R_FreeRendColors(rcolors);
    }
    params->lastIdx++;

    return true; // Continue iteration.
}

static float getSnapshots(material_snapshot_t* msA, material_snapshot_t* msB,
                          material_t* mat)
{
    float               interPos = 0;

    Material_Prepare(msA, mat, true, NULL);

    // Smooth Texture Animation?
    if(msB)
    {
        // If fog is active, inter=0 is accepted as well. Otherwise
        // flickering may occur if the rendering passes don't match for
        // blended and unblended surfaces.
        if(numTexUnits > 1 && mat->current != mat->next &&
           !(!usingFog && mat->inter < 0))
        {
            // Prepare the inter texture.
            Material_Prepare(msB, mat->next, false, NULL);
            interPos = mat->inter;
        }
    }

    return interPos;
}

static void setupRTU(rtexmapunit_t rTU[NUM_TEXMAP_UNITS],
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
static void setupRTU2(rtexmapunit_t rTU[NUM_TEXMAP_UNITS],
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

typedef struct {
    boolean         isWall;
    rendpolytype_t  type;
    blendmode_t     blendMode;
    const float*    texTL, *texBR;
    const float*    texOffset, *texScale;
    const float*    normal; // Surface normal.
    float           alpha;
    const float*    sectorLightLevel;
    float           surfaceLightLevelDelta;
    const float*    sectorLightColor;
    const float*    surfaceColor;

    uint            lightListIdx; // List of lights that affect this poly.
    boolean         glowing;
    boolean         reflective;
    boolean         forceOpaque;

// For bias:
    face_t*         face;
    uint            plane;
    fvertex_t*      from, *to;
    biassurface_t*  bsuf;

// Wall only (todo).
    const float*    segLength;
    const float*    surfaceColor2; // Secondary color.
} rendworldpoly_params_t;

static boolean renderWorldPoly(rvertex_t* rvertices, uint numVertices,
                               const walldiv_t* divs,
                               const rendworldpoly_params_t* p,
                               const material_snapshot_t* msA, float inter,
                               const material_snapshot_t* msB)
{
    rcolor_t*           rcolors;
    rtexcoord_t*        rtexcoords = NULL, *rtexcoords2 = NULL,
                       *rtexcoords5 = NULL;
    uint                realNumVertices;
    rcolor_t*           shinyColors = NULL;
    rtexcoord_t*        shinyTexCoords = NULL;
    boolean             useLights = false;
    uint                numLights = 0;
    float               interPos = 0;
    DGLuint             modTex = 0;
    float               modTexTC[2][2];
    float               modColor[3];
    boolean             drawAsVisSprite = false, isGlowing = p->glowing;
    rtexmapunit_t       rTU[NUM_TEXMAP_UNITS], rTUs[NUM_TEXMAP_UNITS];

    if(!p->forceOpaque && p->type != RPT_SKY_MASK &&
       (!msA->isOpaque || p->alpha < 1 || p->blendMode > 0))
        drawAsVisSprite = true;

    if(p->isWall && divs && (divs[0].num || divs[1].num))
        realNumVertices = 3 + divs[0].num + 3 + divs[1].num;
    else
        realNumVertices = numVertices;

    memset(rTU, 0, sizeof(rtexmapunit_t) * NUM_TEXMAP_UNITS);
    memset(rTUs, 0, sizeof(rtexmapunit_t) * NUM_TEXMAP_UNITS);

    if(p->type != RPT_SKY_MASK)
    {
        setupRTU(rTU, rTUs, msA, inter, msB);
        setupRTU2(rTU, rTUs, p->isWall, p->texOffset, p->texScale, msA, msB);
    }

    memset(modTexTC, 0, sizeof(modTexTC));
    memset(modColor, 0, sizeof(modColor));

    rcolors = R_AllocRendColors(realNumVertices);
    rtexcoords = R_AllocRendTexCoords(realNumVertices);
    if(rTU[TU_INTER].tex)
        rtexcoords2 = R_AllocRendTexCoords(realNumVertices);

    if(p->type != RPT_SKY_MASK)
    {
        // ShinySurface?
        if(p->reflective && rTUs[TU_PRIMARY].tex && !drawAsVisSprite)
        {
            // We'll reuse the same verts but we need new colors.
            shinyColors = R_AllocRendColors(realNumVertices);
            // The normal texcoords are used with the mask.
            // New texcoords are required for shiny texture.
            shinyTexCoords = R_AllocRendTexCoords(realNumVertices);
        }

        /**
         * Dynamic lights?
         * In multiplicative mode, glowing surfaces are fullbright.
         * Rendering lights on them would be pointless.
         */
        if(!isGlowing)
        {
            useLights = (p->lightListIdx? true : false);

            /**
             * If multitexturing is enabled and there is at least one
             * dynlight affecting this surface, grab the paramaters needed
             * to draw it.
             */
            if(useLights && RL_IsMTexLights())
            {
                dynlight_t*         dyn = NULL;

                DL_DynlistIterator(p->lightListIdx, RLIT_DynGetFirst, &dyn);

                rtexcoords5 = R_AllocRendTexCoords(realNumVertices);

                modTex = dyn->texture;
                modColor[CR] = dyn->color[CR];
                modColor[CG] = dyn->color[CG];
                modColor[CB] = dyn->color[CB];
                modTexTC[0][0] = dyn->s[0];
                modTexTC[0][1] = dyn->s[1];
                modTexTC[1][0] = dyn->t[0];
                modTexTC[1][1] = dyn->t[1];

                numLights = 1;
            }
        }
    }

    if(p->isWall)
    {
        // Primary texture coordinates.
        quadTexCoords(rtexcoords, rvertices, *p->segLength, p->texTL);

        // Blend texture coordinates.
        if(rTU[TU_INTER].tex && !drawAsVisSprite)
            quadTexCoords(rtexcoords2, rvertices, *p->segLength, p->texTL);

        // Shiny texture coordinates.
        if(p->reflective && rTUs[TU_PRIMARY].tex && !drawAsVisSprite)
            quadShinyTexCoords(shinyTexCoords, &rvertices[1], &rvertices[2],
                               *p->segLength);

        // First light texture coordinates.
        if(numLights > 0 && RL_IsMTexLights())
            quadLightCoords(rtexcoords5, modTexTC[0], modTexTC[1]);
    }
    else
    {
        uint                i;

        for(i = 0; i < numVertices; ++i)
        {
            const rvertex_t*    vtx = &rvertices[i];
            float               xyz[3];

            xyz[VX] = vtx->pos[VX] - p->texTL[VX];
            xyz[VY] = vtx->pos[VY] - p->texTL[VY];
            xyz[VZ] = vtx->pos[VZ] - p->texTL[VZ];

            // Primary texture coordinates.
            if(rTU[TU_PRIMARY].tex)
            {
                rtexcoord_t*        tc = &rtexcoords[i];

                tc->st[0] =  xyz[VX];
                tc->st[1] = -xyz[VY];
            }

            // Blend primary texture coordinates.
            if(rTU[TU_INTER].tex)
            {
                rtexcoord_t*        tc = &rtexcoords2[i];

                tc->st[0] =  xyz[VX];
                tc->st[1] = -xyz[VY];
            }

            // Shiny texture coordinates.
            if(p->reflective && rTUs[TU_PRIMARY].tex)
            {
                flatShinyTexCoords(&shinyTexCoords[i], vtx->pos);
            }

            // First light texture coordinates.
            if(numLights > 0 && RL_IsMTexLights())
            {
                rtexcoord_t*        tc = &rtexcoords5[i];
                float               width, height;

                width  = p->texBR[VX] - p->texTL[VX];
                height = p->texBR[VY] - p->texTL[VY];

                tc->st[0] = ((p->texBR[VX] - vtx->pos[VX]) / width * modTexTC[0][0]) +
                    (xyz[VX] / width * modTexTC[0][1]);
                tc->st[1] = ((p->texBR[VY] - vtx->pos[VY]) / height * modTexTC[1][0]) +
                    (xyz[VY] / height * modTexTC[1][1]);
            }
        }
    }

    // Light this polygon.
    if(p->type != RPT_SKY_MASK)
    {
        if(isGlowing || levelFullBright)
        {   // Uniform colour. Apply to all vertices.
            Rend_VertexColorsGlow(rcolors, numVertices);
        }
        else
        {   // Non-uniform color.
            if(useBias && p->bsuf)
            {
                // Do BIAS lighting for this poly.
                if(p->isWall)
                    SB_RendSeg(P_GetCurrentMap(), rcolors, p->bsuf,
                               rvertices, numVertices,
                               p->normal, *p->sectorLightLevel,
                               p->from, p->to);
                else
                    SB_RendPlane(P_GetCurrentMap(), rcolors, p->bsuf,
                                 rvertices, numVertices,
                                 p->normal, *p->sectorLightLevel,
                                 p->face, p->plane);
            }
            else
            {
                uint                i;
                float               ll = *p->sectorLightLevel +
                    p->surfaceLightLevelDelta;

                // Calculate the color for each vertex, blended with plane color?
                if(p->surfaceColor[0] < 1 || p->surfaceColor[1] < 1 ||
                   p->surfaceColor[2] < 1)
                {
                    float               vColor[4];

                    // Blend sector light+color+surfacecolor
                    vColor[CR] = p->surfaceColor[CR] * p->sectorLightColor[CR];
                    vColor[CG] = p->surfaceColor[CG] * p->sectorLightColor[CG];
                    vColor[CB] = p->surfaceColor[CB] * p->sectorLightColor[CB];
                    vColor[CA] = 1;

                    for(i = 0; i < numVertices; ++i)
                        lightVertex(&rcolors[i], &rvertices[i], ll, vColor);
                }
                else
                {
                    // Use sector light+color only
                    for(i = 0; i < numVertices; ++i)
                        lightVertex(&rcolors[i], &rvertices[i], ll,
                                    p->sectorLightColor);
                }

                // Bottom color (if different from top)?
                if(p->isWall && p->surfaceColor2)
                {
                    float               vColor[4];

                    // Blend sector light+color+surfacecolor
                    vColor[CR] = p->surfaceColor2[CR] * p->sectorLightColor[CR];
                    vColor[CG] = p->surfaceColor2[CG] * p->sectorLightColor[CG];
                    vColor[CB] = p->surfaceColor2[CB] * p->sectorLightColor[CB];
                    vColor[CA] = 1;

                    lightVertex(&rcolors[0], &rvertices[0], ll, vColor);
                    lightVertex(&rcolors[2], &rvertices[2], ll, vColor);
                }
            }

            Rend_VertexColorsApplyTorchLight(rcolors, rvertices, numVertices);
        }

        if(p->reflective && rTUs[TU_PRIMARY].tex && !drawAsVisSprite)
        {
            uint                i;

            // Strength of the shine.
            for(i = 0; i < numVertices; ++i)
            {
                shinyColors[i].rgba[CR] =
                    MAX_OF(rcolors[i].rgba[CR], msA->shiny.minColor[CR]);
                shinyColors[i].rgba[CG] =
                    MAX_OF(rcolors[i].rgba[CG], msA->shiny.minColor[CG]);
                shinyColors[i].rgba[CB] =
                    MAX_OF(rcolors[i].rgba[CB], msA->shiny.minColor[CB]);
                shinyColors[i].rgba[CA] = rTUs[TU_PRIMARY].blend;
            }
        }

        // Apply uniform alpha.
        Rend_VertexColorsAlpha(rcolors, numVertices, p->alpha);
    }
    else
    {
        memset(rcolors, 0, sizeof(rcolor_t) * numVertices);
    }

    if(IS_MUL && useLights)
    {
        // Surfaces lit by dynamic lights may need to be rendered
        // differently than non-lit surfaces.
        uint                i;
        float               avglightlevel = 0;

        // Determine the average light level of this rend poly,
        // if too bright; do not bother with lights.
        for(i = 0; i < numVertices; ++i)
        {
            avglightlevel += rcolors[i].rgba[CR];
            avglightlevel += rcolors[i].rgba[CG];
            avglightlevel += rcolors[i].rgba[CB];
        }
        avglightlevel /= (float) numVertices * 3;

        if(avglightlevel > 0.98f)
        {
            useLights = false;
        }
    }

    if(drawAsVisSprite)
    {
        assert(p->isWall);

        /**
         * Masked polys (walls) get a special treatment (=> vissprite).
         * This is needed because all masked polys must be sorted (sprites
         * are masked polys). Otherwise there will be artifacts.
         */
        Rend_AddMaskedPoly(rvertices, rcolors, *p->segLength,
                           msA->width, msA->height, p->texOffset,
                           p->blendMode, p->lightListIdx, isGlowing,
                           !msA->isOpaque, rTU);
        R_FreeRendTexCoords(rtexcoords);
        if(rtexcoords2)
            R_FreeRendTexCoords(rtexcoords2);
        if(rtexcoords5)
            R_FreeRendTexCoords(rtexcoords5);
        if(shinyTexCoords)
            R_FreeRendTexCoords(shinyTexCoords);
        R_FreeRendColors(rcolors);
        if(shinyColors)
            R_FreeRendColors(shinyColors);

        return false; // We HAD to use a vissprite, so it MUST not be opaque.
    }

    if(p->type != RPT_SKY_MASK && useLights)
    {
        /**
         * Generate a dynlight primitive for each of the lights affecting
         * the surface. Multitexturing may be used for the first light, so
         * it's skipped.
         */
        dynlightiterparams_t dlparams;

        dlparams.rvertices = rvertices;
        dlparams.numVertices = numVertices;
        dlparams.realNumVertices = realNumVertices;
        dlparams.lastIdx = 0;
        dlparams.isWall = p->isWall;
        dlparams.divs = divs;
        dlparams.texTL = p->texTL;
        dlparams.texBR = p->texBR;

        DL_DynlistIterator(p->lightListIdx, RLIT_DynLightWrite, &dlparams);
        numLights += dlparams.lastIdx;
        if(RL_IsMTexLights())
            numLights -= 1;
    }

    // Write multiple polys depending on rend params.
    if(p->isWall && divs && (divs[0].num || divs[1].num))
    {
        float               bL, tL, bR, tR;
        rvertex_t           origVerts[4];
        rcolor_t            origColors[4];
        rtexcoord_t         origTexCoords[4];

        /**
         * Need to swap indices around into fans set the position
         * of the division vertices, interpolate texcoords and
         * color.
         */

        memcpy(origVerts, rvertices, sizeof(rvertex_t) * 4);
        memcpy(origTexCoords, rtexcoords, sizeof(rtexcoord_t) * 4);
        memcpy(origColors, rcolors, sizeof(rcolor_t) * 4);

        bL = origVerts[0].pos[VZ];
        tL = origVerts[1].pos[VZ];
        bR = origVerts[2].pos[VZ];
        tR = origVerts[3].pos[VZ];

        R_DivVerts(rvertices, origVerts, divs);
        R_DivTexCoords(rtexcoords, origTexCoords, divs, bL, tL, bR, tR);
        R_DivVertColors(rcolors, origColors, divs, bL, tL, bR, tR);

        if(rtexcoords2)
        {
            rtexcoord_t         origTexCoords2[4];

            memcpy(origTexCoords2, rtexcoords2, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(rtexcoords2, origTexCoords2, divs, bL, tL, bR, tR);
        }

        if(rtexcoords5)
        {
            rtexcoord_t         origTexCoords5[4];

            memcpy(origTexCoords5, rtexcoords5, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(rtexcoords5, origTexCoords5, divs, bL, tL, bR, tR);
        }

        if(shinyTexCoords)
        {
            rtexcoord_t         origShinyTexCoords[4];

            memcpy(origShinyTexCoords, shinyTexCoords, sizeof(rtexcoord_t) * 4);
            R_DivTexCoords(shinyTexCoords, origShinyTexCoords, divs, bL, tL, bR, tR);
        }

        if(shinyColors)
        {
            rcolor_t            origShinyColors[4];
            memcpy(origShinyColors, shinyColors, sizeof(rcolor_t) * 4);
            R_DivVertColors(shinyColors, origShinyColors, divs, bL, tL, bR, tR);
        }

        RL_AddPoly(PT_FAN, p->type, rvertices + 3 + divs[0].num,
                   rtexcoords + 3 + divs[0].num,
                   rtexcoords2? rtexcoords2 + 3 + divs[0].num : NULL,
                   rtexcoords5? rtexcoords5 + 3 + divs[0].num : NULL,
                   rcolors + 3 + divs[0].num, 3 + divs[1].num,
                   numLights, modTex, modColor, rTU);
        RL_AddPoly(PT_FAN, p->type, rvertices, rtexcoords, rtexcoords2,

                   rtexcoords5, rcolors, 3 + divs[0].num,
                   numLights, modTex, modColor, rTU);
        if(p->reflective && rTUs[TU_PRIMARY].tex)
        {
            RL_AddPoly(PT_FAN, RPT_SHINY, rvertices + 3 + divs[0].num,
                       shinyTexCoords? shinyTexCoords + 3 + divs[0].num : NULL,
                       rTUs[TU_INTER].tex? rtexcoords + 3 + divs[0].num : NULL,
                       NULL,
                       shinyColors + 3 + divs[0].num,
                       3 + divs[1].num, 0, 0, NULL, rTUs);
            RL_AddPoly(PT_FAN, RPT_SHINY, rvertices,
                       shinyTexCoords, rTUs[TU_INTER].tex? rtexcoords : NULL,
                       NULL, shinyColors, 3 + divs[0].num, 0, 0, NULL, rTUs);
        }
    }
    else
    {
        RL_AddPoly(p->isWall? PT_TRIANGLE_STRIP : PT_FAN, p->type, rvertices,
                   rtexcoords, rtexcoords2, rtexcoords5, rcolors,

                   numVertices, numLights, modTex, modColor, rTU);
        if(p->reflective && rTUs[TU_PRIMARY].tex)
            RL_AddPoly(p->isWall? PT_TRIANGLE_STRIP : PT_FAN, RPT_SHINY,
                       rvertices, shinyTexCoords,
                       rTUs[TU_INTER].tex? rtexcoords : NULL,
                       NULL, shinyColors, numVertices, 0, 0, NULL, rTUs);
    }

    R_FreeRendTexCoords(rtexcoords);
    if(rtexcoords2)
        R_FreeRendTexCoords(rtexcoords2);
    if(rtexcoords5)
        R_FreeRendTexCoords(rtexcoords5);
    if(shinyTexCoords)
        R_FreeRendTexCoords(shinyTexCoords);
    R_FreeRendColors(rcolors);
    if(shinyColors)
        R_FreeRendColors(shinyColors);

    return (p->forceOpaque ||
        !(p->alpha < 1 || !msA->isOpaque || p->blendMode > 0));
}

static boolean doRenderSeg(const sideradioconfig_t* radioConfig,
                           const sector_t* segFrontSec,
                           const sector_t* segBackSec,
                           const float* segLength, const float* segOffset,
                           const float* lineDefLength,
                           fvertex_t* from, fvertex_t* to,
                           float bottom, float top, const pvec3_t normal,
                           float alpha,
                           const float* lightLevel, float lightLevelDelta,
                           const float* lightColor,
                           uint lightListIdx,
                           const walldiv_t* divs,
                           boolean skyMask,
                           boolean addFakeRadio, boolean addReflection,
                           boolean isGlowing,
                           const float texTL[3], const float texBR[3],
                           const float texOffset[2],
                           const float texScale[2],
                           blendmode_t blendMode,
                           const float* color, const float* color2,
                           biassurface_t* bsuf,
                           const material_snapshot_t* msA, float inter,
                           const material_snapshot_t* msB)
{
    rendworldpoly_params_t params;
    rvertex_t*          rvertices;

    // Init the params.
    memset(&params, 0, sizeof(params));

    params.type = (skyMask? RPT_SKY_MASK : RPT_NORMAL);
    params.isWall = true;
    params.segLength = segLength;
    params.forceOpaque = (alpha < 0? true : false);
    params.alpha = (alpha < 0? 1 : alpha);
    params.from = from;
    params.to = to;
    params.bsuf = bsuf;
    params.normal = normal;
    params.texTL = texTL;
    params.texBR = texBR;
    params.sectorLightLevel = lightLevel;
    params.surfaceLightLevelDelta = lightLevelDelta;
    params.sectorLightColor = lightColor;
    params.surfaceColor = color;
    params.surfaceColor2 = color2;
    params.blendMode = blendMode;
    params.texOffset = texOffset;
    params.texScale = texScale;
    params.lightListIdx = lightListIdx;

    if(divs && (divs[0].num || divs[1].num))
    {
        // Allocate enough vertices for the divisions too.
        rvertices = R_AllocRendVertices(3 + divs[0].num + 3 + divs[1].num);
    }
    else
    {
        rvertices = R_AllocRendVertices(4);
    }

    // Vertex coords.
    // Bottom Left.
    V3_Set(rvertices[0].pos, from->pos[VX], from->pos[VY], bottom);

    // Top Left.
    V3_Set(rvertices[1].pos, from->pos[VX], from->pos[VY], top);

    // Bottom Right.
    V3_Set(rvertices[2].pos, to->pos[VX], to->pos[VY], bottom);

    // Top Right.
    V3_Set(rvertices[3].pos, to->pos[VX], to->pos[VY], top);

    // Is reflective?
    if(addReflection)
    {
        params.reflective = true;
    }

    // Make it fullbright?
    if(isGlowing)
        params.glowing = true;

    // Draw this seg.
    if(renderWorldPoly(rvertices, 4, divs, &params, msA, inter, msB))
    {   // Drawn poly was opaque.
        // Render Fakeradio polys for this seg?
        if(params.type != RPT_SKY_MASK && addFakeRadio)
        {
            rendsegradio_params_t radioParams;

            radioParams.sectorLightLevel = lightLevel;
            radioParams.linedefLength = lineDefLength;
            radioParams.botCn = radioConfig->bottomCorners;
            radioParams.topCn = radioConfig->topCorners;
            radioParams.sideCn = radioConfig->sideCorners;
            radioParams.spans = radioConfig->spans;
            radioParams.segOffset = segOffset;
            radioParams.segLength = segLength;
            radioParams.frontSec = segFrontSec;
            radioParams.backSec = segBackSec;

            /**
             * \kludge Revert the vertex coords as they may have been
             * changed due to height divisions.
             */

            // Bottom Left.
            V3_Set(rvertices[0].pos, from->pos[VX], from->pos[VY], bottom);

            // Top Left.
            V3_Set(rvertices[1].pos, from->pos[VX], from->pos[VY], top);

            // Bottom Right.
            V3_Set(rvertices[2].pos, to->pos[VX], to->pos[VY], bottom);

            // Top Right.
            V3_Set(rvertices[3].pos, to->pos[VX], to->pos[VY], top);

            Rend_RadioSegSection(rvertices, divs, &radioParams);
        }

        R_FreeRendVertices(rvertices);
        return true; // Clip with this solid seg.
    }

    R_FreeRendVertices(rvertices);

    return false; // Do not clip with this.
}

static void renderPlane(face_t* face, planetype_t type,
                        float height, const vectorcomp_t normal[3],
                        material_t* inMat, material_t* inMatB,
                        float matBlendFactor, short sufInFlags,
                        const float sufColor[4], blendmode_t blendMode,
                        const float texTL[3], const float texBR[3],
                        const float texOffset[2], const float texScale[2],
                        boolean skyMasked,
                        boolean addDLights, boolean isGlowing,
                        biassurface_t* bsuf, uint plane /*tmp*/,
                        int texMode /*tmp*/)
{
    float               inter = 0;
    rendworldpoly_params_t params;
    subsector_t*        ssec = (subsector_t*) face->data;
    uint                numVertices;
    rvertex_t*          rvertices;
    boolean             blended = false;
    sector_t*           sec = ssec->sector;
    material_t*         mat = NULL;
    material_snapshot_t msA, msB;

    memset(&msA, 0, sizeof(msA));
    memset(&msB, 0, sizeof(msB));

    memset(&params, 0, sizeof(params));

    params.isWall = false;
    params.face = face;
    params.plane = plane;
    params.bsuf = bsuf;
    params.normal = normal;
    params.texTL = texTL;
    params.texBR = texBR;
    params.sectorLightLevel = &sec->lightLevel;
    params.sectorLightColor = R_GetSectorLightColor(sec);
    params.surfaceLightLevelDelta = 0;
    params.surfaceColor = sufColor;
    params.texOffset = texOffset;
    params.texScale = texScale;

    if(skyMasked)
    {
        skyHemispheres |=
            (type == PLN_FLOOR? SKYHEMI_LOWER : SKYHEMI_UPPER);

        // In devSkyMode mode we render all polys destined for the
        // skymask as regular world polys (with a few obvious properties).
        if(devSkyMode)
        {
            params.type = RPT_NORMAL;
            params.blendMode = BM_NORMAL;
            params.glowing = true;
            params.forceOpaque = true;
            mat = inMat;
        }
        else
        {   // We'll mask this.
            params.type = RPT_SKY_MASK;
        }
    }
    else
    {
        params.type = RPT_NORMAL;
        mat = inMat;

        if(isGlowing)
        {
            params.glowing = true; // Make it stand out
        }

        if(type != PLN_MID)
        {
            params.blendMode = BM_NORMAL;
            params.alpha = 1;
            params.forceOpaque = true;
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

    /**
     * If this poly is destined for the skymask, we don't need to
     * do any further processing.
     */
    if(params.type != RPT_SKY_MASK)
    {
        // Smooth Texture Animation?
        if(smoothTexAnim && texMode != 1)
            blended = true;

        // Dynamic lights.
        if(addDLights && !params.glowing)
        {
            params.lightListIdx =
                DL_ProjectOnSurface(P_GetCurrentMap(), face, params.texTL, params.texBR,
                                    normal,
                                    (DLF_NO_PLANAR |
                                     (type == PLN_FLOOR? DLF_TEX_FLOOR : DLF_TEX_CEILING)));
        }

        // Render Shiny polys?
        if(useShinySurfaces && mat && mat->reflection)
        {
            params.reflective = true;
        }
    }

    if(params.type != RPT_SKY_MASK)
    {
        if(blended && inMatB)
        {
            getSnapshots(&msB, NULL, inMatB);
            getSnapshots(&msA, NULL, mat);
            inter = matBlendFactor;
        }
        else
            inter = getSnapshots(&msA, blended? &msB : NULL, mat);
    }

    numVertices = ssec->hEdgeCount + (ssec->useMidPoint? 2 : 0);
    rvertices = R_AllocRendVertices(numVertices);
    preparePlane(rvertices, ssec, height, !(normal[VZ] > 0));

    renderWorldPoly(rvertices, numVertices, NULL, &params,
                    &msA, inter, (blended || inMatB)? &msB : NULL);

    R_FreeRendVertices(rvertices);
}

static void Rend_RenderPlane(face_t* face, planetype_t type,
                             float height, const vectorcomp_t normal[3],
                             material_t* inMat, material_t* inMatB,
                             float matBlendFactor,
                             short sufInFlags,
                             const float sufColor[4], blendmode_t blendMode,
                             const float texOffset[2], const float texScale[2],
                             boolean skyMasked,
                             boolean addDLights, boolean isGlowing,
                             biassurface_t* bsuf, uint elmIdx /*tmp*/,
                             int texMode /*tmp*/)
{
    subsector_t*        ssec;
    sector_t*           sec;
    vec3_t              vec;

    // Must have a visible surface.
    if(!inMat || (inMat->flags & MATF_NO_DRAW))
        return;

    ssec = (subsector_t*) face->data;
    sec = ssec->sector;

    V3_Set(vec, vx - ssec->midPoint.pos[VX], vz - ssec->midPoint.pos[VY],
           vy - height);

    // Don't bother with planes facing away from the camera.
    if(!(V3_DotProduct(vec, normal) < 0))
    {
        float               texTL[3], texBR[3];

        // Set the texture origin, Y is flipped for the ceiling.
        V3_Set(texTL, ssec->bBox[0].pos[VX],
               ssec->bBox[type == PLN_FLOOR? 1 : 0].pos[VY], height);
        V3_Set(texBR, ssec->bBox[1].pos[VX],
            ssec->bBox[type == PLN_FLOOR? 0 : 1].pos[VY], height);

        renderPlane(face, type, height, normal, inMat, inMatB, matBlendFactor,
                    sufInFlags, sufColor, blendMode, texTL, texBR, texOffset,
                    texScale, skyMasked, addDLights, isGlowing, bsuf, elmIdx,
                    texMode);
    }
}

static boolean isVisible(surface_t* surface, sector_t* frontsec,
                         boolean canMask, boolean* skyMask)
{
    if(!(surface->material && (surface->material->flags & MATF_NO_DRAW)))
    {
        *skyMask = false;
        return true;
    }
    else if(canMask)
    {   // Perhaps add this section to the sky mask?
        if(IS_SKYSURFACE(&frontsec->SP_ceilsurface) &&
           IS_SKYSURFACE(&frontsec->SP_floorsurface))
        {
           *skyMask = true;
           return true;
        }
    }

    return false;
}

static boolean rendSegSection(face_t* ssec, void* dummy, linedef_t* lineDef,
                              sidedef_t* sideDef, byte segSide,
                              const sector_t* segFrontSec,
                              const sector_t* segBackSec, const float* segLength,
                              const float* segOffset, segsection_t section,
                              surface_t* surface, fvertex_t* from, fvertex_t* to,
                              float bottom, float top,
                              const float texOffset[2],
                              sector_t* frontsec, boolean softSurface,
                              boolean addDLights, short sideFlags,
                              biassurface_t* bsuf,
                              hedge_t* hEdge /* temp */)
{
    float               alpha;
    boolean             skyMask, solidSeg = true;

    if(!isVisible(surface, frontsec, false, &skyMask))
        return false;

    if(bottom >= top)
        return true;

    alpha = (section == SEG_MIDDLE? surface->rgba[3] : 1.0f);

    if(section == SEG_MIDDLE && softSurface)
    {
        mobj_t*             mo = viewPlayer->shared.mo;

        /**
         * Can the player walk through this surface?
         * If the player is close enough we should NOT add a solid seg
         * otherwise they'd get HOM when they are directly on top of the
         * lineDef (eg passing through an opaque waterfall).
         */

        if(viewZ > bottom && viewZ < top)
        {
            float               delta[2], pos, result[2];

            delta[0] = lineDef->dX;
            delta[1] = lineDef->dY;

            pos = M_ProjectPointOnLine(mo->pos, lineDef->L_v1pos, delta, 0,
                                       result);
            if(pos > 0 && pos < 1)
            {
                float               distance;
                float               minDistance = mo->radius * .8f;

                delta[VX] = mo->pos[VX] - result[VX];
                delta[VY] = mo->pos[VY] - result[VY];

                distance = M_ApproxDistancef(delta[VX], delta[VY]);

                if(distance < minDistance)
                {
                    // Fade it out the closer the viewPlayer gets and clamp.
                    alpha = (alpha / minDistance) * distance;
                    alpha = MINMAX_OF(0, alpha, 1);
                }

                if(alpha < 1)
                    solidSeg = false;
            }
        }
    }

    if(alpha > 0)
    {
        int                 texMode = 0;
        short               tempflags = 0;
        uint                lightListIdx = 0;
        float               texTL[3], texBR[3], texScale[2],
                            inter = 0, matBlendFactor = 0;
        walldiv_t           divs[2];
        boolean             forceOpaque = false;
        material_t*         mat = NULL, *matB = NULL;
        rendpolytype_t      type = RPT_NORMAL;
        boolean             isTwoSided = (lineDef &&
            LINE_FRONTSIDE(lineDef) && LINE_BACKSIDE(lineDef))? true:false;
        blendmode_t         blendMode = BM_NORMAL;
        boolean             addFakeRadio = false, addReflection = false,
                            isGlowing = false, blended = false;
        const float*        color, *color2;
        material_snapshot_t msA, msB;

        memset(&msA, 0, sizeof(msA));
        memset(&msB, 0, sizeof(msB));

        texScale[0] = ((surface->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        texScale[1] = ((surface->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        V3_Set(texTL, from->pos[VX], from->pos[VY], top);
        V3_Set(texBR, to->pos[VX], to->pos[VY], bottom);

        // Fill in the remaining params data.
        if(skyMask || IS_SKYSURFACE(surface))
        {
            // In devSkyMode mode we render all polys destined for the skymask
            // as regular world polys (with a few obvious properties).
            if(devSkyMode)
            {
                mat = surface->material;
                forceOpaque = true;
                isGlowing = true;
            }
            else
            {   // We'll mask this.
                type = RPT_SKY_MASK;
            }
        }
        else
        {
            int                 surfaceFlags, surfaceInFlags;

            // Determine which texture to use.
            if(renderTextures == 2)
                texMode = 2;
            else if(!surface->material ||
                    ((surface->inFlags & SUIF_MATERIAL_FIX) && devNoTexFix))
                texMode = 1;
            else
                texMode = 0;

            if(texMode == 0)
            {
                mat = surface->material;
                matB = surface->materialB;
                matBlendFactor = surface->matBlendFactor;
            }
            else if(texMode == 1)
                // For debug, render the "missing" texture instead of the texture
                // chosen for surfaces to fix the HOMs.
                mat = P_GetMaterial(DDT_MISSING, MN_SYSTEM);
            else // texMode == 2
                // For lighting debug, render all solid surfaces using the gray
                // texture.
                mat = P_GetMaterial(DDT_GRAY, MN_SYSTEM);

            // Make any necessary adjustments to the surface flags to suit the
            // current texture mode.
            surfaceFlags = surface->flags;
            surfaceInFlags = surface->inFlags;
            if(texMode == 1)
            {
                isGlowing = true; // Make it stand out
            }
            else if(texMode == 2)
            {
                surfaceInFlags &= ~(SUIF_MATERIAL_FIX);
            }

            if(section != SEG_MIDDLE || (section == SEG_MIDDLE && !isTwoSided))
            {
                forceOpaque = true;
                blendMode = BM_NORMAL;
            }
            else
            {
                if(surface->blendMode == BM_NORMAL && noSpriteTrans)
                    blendMode = BM_ZEROALPHA; // "no translucency" mode
                else
                    blendMode = surface->blendMode;
            }

            if(glowingTextures &&
               ((surfaceFlags & DDSUF_GLOW) ||
               (surface->material && (surface->material->flags & MATF_GLOW))))
                isGlowing = true;

            addFakeRadio = !(surfaceInFlags & SUIF_NO_RADIO);
        }

        if(type != RPT_SKY_MASK)
        {
            // Smooth Texture Animation?
            if(smoothTexAnim && texMode != 1)
                blended = true;
        }

        if(type != RPT_SKY_MASK)
        {
            if(blended && matB)
            {
                getSnapshots(&msB, NULL, matB);
                getSnapshots(&msA, NULL, mat);
                inter = matBlendFactor;
            }
            else
                inter = getSnapshots(&msA, blended? &msB : NULL, mat);
        }

        if(addDLights && !isGlowing)
            lightListIdx = DL_ProjectOnSurface(P_GetCurrentMap(), ssec, texTL, texBR,
                                               sideDef->SW_middlenormal,
                                    ((section == SEG_MIDDLE && isTwoSided)? DLF_SORT_LUMADSC : 0));

        addFakeRadio = ((addFakeRadio && !isGlowing)? true : false);
        addReflection = (useShinySurfaces && mat && mat->reflection? true : false);

        selectSurfaceColors(&color, &color2, sideDef, section);

        // Check for neighborhood division?
        divs[0].num = divs[1].num = 0;
        if(!(section == SEG_MIDDLE && isTwoSided) &&
           !(lineDef->inFlags & LF_POLYOBJ))
        {
            applyWallHeightDivision(divs, hEdge, frontsec, bottom, top);
        }

        solidSeg = doRenderSeg(&sideDef->radioConfig, segFrontSec, segBackSec,
                               segLength, segOffset, &lineDef->length,
                               from, to, bottom, top,
                               surface->normal, (forceOpaque? -1 : alpha),
                               &frontsec->lightLevel,
                               R_WallAngleLightLevelDelta(lineDef, segSide),
                               R_GetSectorLightColor(frontsec),
                               lightListIdx,
                               divs,
                               skyMask,
                               addFakeRadio,
                               addReflection,
                               isGlowing,
                               texTL, texBR, texOffset, texScale, blendMode,
                               color, color2, bsuf,
                               &msA, inter, (blended || matB) ? &msB : NULL);
    }

    return solidSeg;
}

/**
 * Renders the given single-sided seg into the world.
 */
static boolean Rend_RenderSSWallSeg(hedge_t* hEdge, fvertex_t* from,
                                    fvertex_t* to)
{
    int                 pid;
    float               ffloor, fceil;
    subsector_t*        ssec;
    sector_t*           frontsec;
    boolean             backSide, solidSeg = true;
    seg_t*              seg = (seg_t*) hEdge->data;
    sidedef_t*          side = seg->sideDef;
    linedef_t*          ldef;

    if(!side)
    {   // A one-way window.
        return false;
    }

    ssec = (subsector_t*) hEdge->face->data;

    solidSeg = true;
    frontsec = ssec->sector;
    backSide = seg->side;
    ldef = seg->lineDef;

    pid = viewPlayer - ddPlayers;
    if(!ldef->mapped[pid])
    {
        ldef->mapped[pid] = true; // This line is now seen in the map.

        // Send a status report.
        if(gx.HandleMapObjectStatusReport)
            gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED,
                                           GET_LINE_IDX(ldef),
                                           DMU_LINEDEF, &pid);
    }

    ffloor = ssec->sector->SP_floorvisheight;
    fceil = ssec->sector->SP_ceilvisheight;

    // Create the wall sections.

    // Middle section.
    if(side->SW_middleinflags & SUIF_PVIS)
    {
        float               texOffset[2];
        surface_t*          surface = &side->SW_middlesurface;

        texOffset[0] = surface->visOffset[0] + seg->offset;
        texOffset[1] = surface->visOffset[1];

        if(ldef->flags & DDLF_DONTPEGBOTTOM)
            texOffset[1] += -(fceil - ffloor);

        Rend_RadioUpdateLinedef(seg->lineDef, seg->side);

        rendSegSection(hEdge->face, seg, seg->lineDef, seg->sideDef, seg->side,
                       ((subsector_t*) hEdge->face->data)->sector,
                       hEdge->twin ? ((subsector_t*) hEdge->twin->face->data)->sector : NULL,
                       &seg->length, &seg->offset, SEG_MIDDLE,
                       &side->SW_middlesurface,
                       from, to, ffloor, fceil,
                       texOffset, /*temp >*/ frontsec, /*< temp*/
                       false, true, side->flags, seg->bsuf[SEG_MIDDLE], hEdge);
    }

    if(P_IsInVoid(viewPlayer))
        solidSeg = false;

    return solidSeg;
}

/**
 * Renders the given polyobj seg into the world
 */
static boolean Rend_RenderPolyobjSeg(face_t* face, poseg_t* seg)
{
    int                 pid = viewPlayer - ddPlayers;
    subsector_t*        ssec = (subsector_t*) face->data;
    linedef_t*          ldef = seg->lineDef;
    sidedef_t*          side = LINE_FRONTSIDE(ldef);

    if(!ldef->mapped[pid])
    {
        ldef->mapped[pid] = true; // This line is now seen in the map.

        // Send a status report.
        if(gx.HandleMapObjectStatusReport)
            gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED,
                                           GET_LINE_IDX(ldef),
                                           DMU_LINEDEF, &pid);
    }

    if(side->SW_middleinflags & SUIF_PVIS)
    {
        float               texOffset[2], offset = 0;
        sector_t*           frontSec = ssec->sector;
        float               ffloor = frontSec->SP_floorvisheight,
                            fceil = frontSec->SP_ceilvisheight;
        surface_t*          surface = &side->SW_middlesurface;

        texOffset[0] = surface->visOffset[0];
        texOffset[1] = surface->visOffset[1];

        if(ldef->flags & DDLF_DONTPEGBOTTOM)
            texOffset[1] += -(fceil - ffloor);

        rendSegSection(face, seg, ldef, side, FRONT,
                       frontSec, NULL,
                       &ldef->length, &offset, SEG_MIDDLE,
                       &side->SW_middlesurface,
                       &ldef->L_v1->v, &ldef->L_v2->v, ffloor, fceil,
                       texOffset, /*temp >*/ frontSec, /*< temp*/
                       false, true, side->flags, seg->bsuf, NULL);
        
        return P_IsInVoid(viewPlayer)? false : true;
    }

    return false;
}

static boolean findBottomTop(segsection_t section, float segOffset,
                             const surface_t* suf,
                             const plane_t* ffloor, const plane_t* fceil,
                             const plane_t* bfloor, const plane_t* bceil,
                             boolean unpegBottom, boolean unpegTop,
                             boolean stretchMiddle, boolean isSelfRef,
                             float* bottom, float* top, float texOffset[2])
{
    switch(section)
    {
    case SEG_TOP:
        *top = fceil->visHeight;
        // Can't go over front ceiling, would induce polygon flaws.
        if(bceil->visHeight < ffloor->visHeight)
            *bottom = ffloor->visHeight;
        else
            *bottom = bceil->visHeight;
        if(*top > *bottom)
        {
            texOffset[VX] = suf->visOffset[VX] + segOffset;
            texOffset[VY] = suf->visOffset[VY];

            // Align with normal middle texture?
            if(!unpegTop)
                texOffset[VY] += -(fceil->visHeight - bceil->visHeight);

            return true;
        }
        break;

    case SEG_BOTTOM:
        {
        float               t = bfloor->visHeight;

        *bottom = ffloor->visHeight;
        // Can't go over the back ceiling, would induce polygon flaws.
        if(bfloor->visHeight > bceil->visHeight)
            t = bceil->visHeight;

        // Can't go over front ceiling, would induce polygon flaws.
        if(t > fceil->visHeight)
            t = fceil->visHeight;
        *top = t;

        if(*top > *bottom)
        {
            texOffset[VX] = suf->visOffset[VX] + segOffset;
            texOffset[VY] = suf->visOffset[VY];

            if(bfloor->visHeight > fceil->visHeight)
                texOffset[VY] += bfloor->visHeight - bceil->visHeight;

            // Align with normal middle texture?
            if(unpegBottom)
                texOffset[VY] += (fceil->visHeight - bfloor->visHeight);

            return true;
        }
        break;
        }
    case SEG_MIDDLE:
        {
        float               ftop, fbottom, vR_ZBottom, vR_ZTop;
        material_t*         mat = suf->material->current;

        if(isSelfRef)
        {
            fbottom = MIN_OF(bfloor->visHeight, ffloor->visHeight);
            ftop    = MAX_OF(bceil->visHeight, fceil->visHeight);
        }
        else
        {
            fbottom = MAX_OF(bfloor->visHeight, ffloor->visHeight);
            ftop    = MIN_OF(bceil->visHeight, fceil->visHeight);
        }

        fbottom += suf->visOffset[2];
        ftop    += suf->visOffset[2];

        *bottom = vR_ZBottom = fbottom;
        *top    = vR_ZTop    = ftop;

        if(stretchMiddle)
        {
            if(*top > *bottom)
            {
                texOffset[VX] = suf->visOffset[VX] + segOffset;
                texOffset[VY] = 0;

                return true;
            }
        }
        else
        {
            boolean             clipBottom =
                !(IS_SKYSURFACE(&ffloor->surface) &&
                  IS_SKYSURFACE(&bfloor->surface));
            boolean             clipTop =
                !(IS_SKYSURFACE(&fceil->surface) &&
                  IS_SKYSURFACE(&bceil->surface));

            if(Rend_MidMaterialPos(bottom, &vR_ZBottom, top, &vR_ZTop,
                    NULL, suf->visOffset[VY], mat->height, unpegBottom,
                    clipTop, clipBottom))
            {
                texOffset[VX] = suf->visOffset[VX] + segOffset;
                texOffset[VY] = 0;

                return true;
            }
        }
        }
        break;
    }

    return false;
}

/**
 * Renders wall sections for given two-sided seg.
 */
static boolean Rend_RenderWallSeg(hedge_t* hEdge, fvertex_t* from,
                                  fvertex_t* to)
{
    int                 pid = viewPlayer - ddPlayers;
    float               bottom, top, texOffset[2];
    sector_t*           frontSec, *backSec;
    sidedef_t*          frontSide, *backSide;
    plane_t*            ffloor, *fceil, *bfloor, *bceil;
    linedef_t*          line;
    int                 solidSeg = false;
    subsector_t*        ssec = (subsector_t*) hEdge->face->data;
    seg_t*              seg = (seg_t*) hEdge->data,
                       *backSeg = hEdge->twin ? ((seg_t*) hEdge->twin->data) : NULL;

    if(!seg->sideDef)
        return false;

    frontSide = seg->sideDef;
    backSide = backSeg->sideDef;
    frontSec = frontSide->sector;
    backSec = backSide->sector;
    line = seg->lineDef;

    if(!line->mapped[pid])
    {
        line->mapped[pid] = true; // This line is now seen in the map.

        // Send a status report.
        if(gx.HandleMapObjectStatusReport)
            gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED,
                                           GET_LINE_IDX(line),
                                           DMU_LINEDEF, &pid);
    }

    if(backSec == frontSec &&
       !frontSide->SW_topmaterial && !frontSide->SW_bottommaterial &&
       !frontSide->SW_middlematerial)
       return false; // Ugh... an obvious wall seg hack. Best take no chances...

    ffloor = ssec->sector->SP_plane(PLN_FLOOR);
    fceil  = ssec->sector->SP_plane(PLN_CEILING);
    bfloor = backSec->SP_plane(PLN_FLOOR);
    bceil  = backSec->SP_plane(PLN_CEILING);

    if((frontSide->SW_middleinflags & SUIF_PVIS) ||
       (frontSide->SW_topinflags & SUIF_PVIS) ||
       (frontSide->SW_bottominflags & SUIF_PVIS))
        Rend_RadioUpdateLinedef(seg->lineDef, seg->side);

    /**
     * Create the wall sections.
     *
     * We may need multiple wall sections.
     * Determine which parts of the segment are really visible.
     */

    // Middle section.
    if(frontSide->SW_middleinflags & SUIF_PVIS)
    {
        surface_t*          suf = &frontSide->SW_middlesurface;

        if(findBottomTop(SEG_MIDDLE, seg->offset, suf,
                         ffloor, fceil, bfloor, bceil,
                         (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
                         (line->flags & DDLF_DONTPEGTOP)? true : false,
                         (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
                         LINE_SELFREF(line)? true : false,
                         &bottom, &top, texOffset))
        {
            solidSeg = rendSegSection(hEdge->face, seg, seg->lineDef, seg->sideDef, seg->side,
                                      ((subsector_t*) hEdge->face->data)->sector,
                                      ((subsector_t*) hEdge->twin->face->data)->sector,
                                      &seg->length, &seg->offset, SEG_MIDDLE,
                                      suf, from, to, bottom, top, texOffset,
                                      frontSec,
                                      (((viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)) ||
                                        !(line->flags & DDLF_BLOCKING))? true : false),
                                      true, frontSide->flags,
                                      seg->bsuf[SEG_MIDDLE], hEdge);
            if(solidSeg)
            {
                float               xbottom, xtop;
                surface_t*          suf = &frontSide->SW_middlesurface;

                if(LINE_SELFREF(line))
                {
                    xbottom = MIN_OF(bfloor->visHeight, ffloor->visHeight);
                    xtop    = MAX_OF(bceil->visHeight, fceil->visHeight);
                }
                else
                {
                    xbottom = MAX_OF(bfloor->visHeight, ffloor->visHeight);
                    xtop    = MIN_OF(bceil->visHeight, fceil->visHeight);
                }

                xbottom += suf->visOffset[2];
                xtop    += suf->visOffset[2];

                // Can we make this a solid segment?
                if(!(top >= xtop && bottom <= xbottom))
                     solidSeg = false;
            }
        }
    }

    // Upper section.
    if(frontSide->SW_topinflags & SUIF_PVIS)
    {
        surface_t*      suf = &frontSide->SW_topsurface;

        if(findBottomTop(SEG_TOP, seg->offset, suf,
                         ffloor, fceil, bfloor, bceil,
                         (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
                         (line->flags & DDLF_DONTPEGTOP)? true : false,
                         (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
                         LINE_SELFREF(line)? true : false,
                         &bottom, &top, texOffset))
        {
            rendSegSection(hEdge->face, seg, seg->lineDef, seg->sideDef, seg->side,
                           ((subsector_t*) hEdge->face->data)->sector,
                           ((subsector_t*) hEdge->twin->face->data)->sector,
                           &seg->length, &seg->offset, SEG_TOP, suf,
                           from, to, bottom, top,
                           texOffset, frontSec, false, true,
                           frontSide->flags, seg->bsuf[SEG_TOP], hEdge);
        }
    }

    // Lower section.
    if(frontSide->SW_bottominflags & SUIF_PVIS)
    {
        surface_t*          suf = &frontSide->SW_bottomsurface;

        if(findBottomTop(SEG_BOTTOM, seg->offset, suf,
                         ffloor, fceil, bfloor, bceil,
                         (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
                         (line->flags & DDLF_DONTPEGTOP)? true : false,
                         (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
                         LINE_SELFREF(line)? true : false,
                         &bottom, &top, texOffset))
        {
            rendSegSection(hEdge->face, seg, seg->lineDef, seg->sideDef, seg->side,
                           ((subsector_t*) hEdge->face->data)->sector,
                           ((subsector_t*) hEdge->twin->face->data)->sector,
                           &seg->length, &seg->offset, SEG_BOTTOM, suf,
                           from, to, bottom, top,
                           texOffset, frontSec, false, true,
                           frontSide->flags, seg->bsuf[SEG_BOTTOM], hEdge);
        }
    }

    // Can we make this a solid segment in the clipper?
    if(solidSeg == -1)
        return false; // NEVER (we have a hole we couldn't fix).

    if(LINE_SELFREF(line))
       return false;

    if(!solidSeg) // We'll have to determine whether we can...
    {
        if(backSec == frontSec)
        {
            // An obvious hack, what to do though??
        }
        else if((bceil->visHeight <= ffloor->visHeight &&
                    ((frontSide->SW_topmaterial /* && !(frontSide->flags & SDF_MIDTEXUPPER)*/) ||
                     (frontSide->SW_middlematerial))) ||
                (bfloor->visHeight >= fceil->visHeight &&
                    (frontSide->SW_bottommaterial || frontSide->SW_middlematerial)))
        {
            // A closed gap.
            solidSeg = true;
        }
        else if((seg->frameFlags & SEGINF_BACKSECSKYFIX) ||
                (!(bceil->visHeight - bfloor->visHeight > 0) && bfloor->visHeight > ffloor->visHeight && bceil->visHeight < fceil->visHeight &&
                (frontSide->SW_topmaterial /*&& !(frontSide->flags & SDF_MIDTEXUPPER)*/) &&
                (frontSide->SW_bottommaterial)))
        {
            // A zero height back segment
            solidSeg = true;
        }
    }

    if(solidSeg && !P_IsInVoid(viewPlayer))
        return true;

    return false;
}

float Rend_SectorLight(sector_t* sec)
{
    if(levelFullBright)
        return 1.0f;

    return sec->lightLevel;
}

static void Rend_MarkSegsFacingFront(face_t* face)
{
    hedge_t*            hEdge;

    if((hEdge = face->hEdge))
    {
        do
        {
            seg_t*              seg = (seg_t*) hEdge->data;

            // Occlusions can only happen where two sectors contact.
            if(seg->lineDef)
            {
                seg->frameFlags &= ~SEGINF_BACKSECSKYFIX;

                // Which way should it be facing?
                if(!(Rend_FacingViewerDot(hEdge->HE_v1pos, hEdge->HE_v2pos) < 0))
                    seg->frameFlags |= SEGINF_FACINGFRONT;
                else
                    seg->frameFlags &= ~SEGINF_FACINGFRONT;

                Rend_MarkSegSectionsPVisible(hEdge);
            }
        } while((hEdge = hEdge->next) != face->hEdge);
    }
}

static void prepareSkyMaskPoly(rvertex_t verts[4], rtexcoord_t coords[4],
                               rtexmapunit_t rTU[NUM_TEXMAP_UNITS],
                               float wallLength, material_t* mat)
{
    material_snapshot_t ms;
    vec3_t              texOrigin[2];

    // In devSkyMode mode we render all polys destined for the skymask as
    // regular world polys (with a few obvious properties).

    Material_Prepare(&ms, mat, true, NULL);

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

    quadTexCoords(coords, verts, wallLength, texOrigin[0]);
}

static void Rend_SSectSkyFixes(face_t* face)
{
    float               ffloor, fceil, bfloor, bceil, bsh;
    rvertex_t           rvertices[4];
    rcolor_t            rcolors[4];
    rtexcoord_t         rtexcoords[4];
    rtexmapunit_t       rTU[NUM_TEXMAP_UNITS];
    float*              vBL, *vBR, *vTL, *vTR;
    sector_t*           frontsec, *backsec;
    hedge_t*            hEdge;
    subsector_t*        ssec = (subsector_t*) face->data;

    if(!face->hEdge)
        return;

    // Init the poly.
    memset(rTU, 0, sizeof(rTU));

    if(devSkyMode)
    {
        uint                i;

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

    hEdge = face->hEdge;
    do
    {
        seg_t*              seg = (seg_t*) hEdge->data,
                           *backSeg = hEdge->twin ? ((seg_t*) hEdge->twin->data) : NULL;
        sidedef_t*          side = seg->sideDef;

        if(!side)
            continue;

        // Let's first check which way this seg is facing.
        if(!(seg->frameFlags & SEGINF_FACINGFRONT))
            continue;

        backsec = hEdge->twin? ((subsector_t*) hEdge->twin->face->data)->sector : NULL;
        frontsec = ((subsector_t*) hEdge->face->data)->sector;

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
        vBL[VX] = vTL[VX] = hEdge->HE_v1pos[VX];
        vBL[VY] = vTL[VY] = hEdge->HE_v1pos[VY];
        vBR[VX] = vTR[VX] = hEdge->HE_v2pos[VX];
        vBR[VY] = vTR[VY] = hEdge->HE_v2pos[VY];

        // Upper/lower normal skyfixes.
        if(!backsec || backsec != frontsec)
        {
            // Floor.
            if(IS_SKYSURFACE(&frontsec->SP_floorsurface) &&
               !(backsec && IS_SKYSURFACE(&backsec->SP_floorsurface)) &&
               ffloor > skyFix[PLN_FLOOR].height)
            {
                vTL[VZ] = vTR[VZ] = ffloor;
                vBL[VZ] = vBR[VZ] = skyFix[PLN_FLOOR].height;

                if(devSkyMode)
                    prepareSkyMaskPoly(rvertices, rtexcoords, rTU, seg->length,
                                       frontsec->SP_floormaterial);

                RL_AddPoly(PT_TRIANGLE_STRIP,
                           (devSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                           rvertices, rtexcoords, NULL, NULL,
                           rcolors, 4, 0, 0, NULL, rTU);
            }

            // Ceiling.
            if(IS_SKYSURFACE(&frontsec->SP_ceilsurface) &&
               !(backsec && IS_SKYSURFACE(&backsec->SP_ceilsurface)) &&
               fceil < skyFix[PLN_CEILING].height)
            {
                vTL[VZ] = vTR[VZ] = skyFix[PLN_CEILING].height;
                vBL[VZ] = vBR[VZ] = fceil;

                if(devSkyMode)
                     prepareSkyMaskPoly(rvertices, rtexcoords, rTU, seg->length,
                                        frontsec->SP_ceilmaterial);

                RL_AddPoly(PT_TRIANGLE_STRIP,
                           (devSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                           rvertices, rtexcoords, NULL, NULL,
                           rcolors, 4, 0, 0, NULL, rTU);
            }
        }

        // Upper/lower zero height backsec skyfixes.
        if(backsec && bsh <= 0)
        {
            // Floor.
            if(IS_SKYSURFACE(&frontsec->SP_floorsurface) &&
               IS_SKYSURFACE(&backsec->SP_floorsurface))
            {
                if(bfloor > skyFix[PLN_FLOOR].height)
                {
                    vTL[VZ] = vTR[VZ] = bfloor;
                    vBL[VZ] = vBR[VZ] = skyFix[PLN_FLOOR].height;

                    if(devSkyMode)
                        prepareSkyMaskPoly(rvertices, rtexcoords, rTU, seg->length,
                                           frontsec->SP_floormaterial);

                    RL_AddPoly(PT_TRIANGLE_STRIP,
                               (devSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                               rvertices, rtexcoords, NULL, NULL,
                               rcolors, 4, 0, 0, NULL, rTU);
                }

                // Ensure we add a solid view seg.
                seg->frameFlags |= SEGINF_BACKSECSKYFIX;
            }

            // Ceiling.
            if(IS_SKYSURFACE(&frontsec->SP_ceilsurface) &&
               IS_SKYSURFACE(&backsec->SP_ceilsurface))
            {
                if(bceil < skyFix[PLN_CEILING].height)
                {
                    vTL[VZ] = vTR[VZ] = skyFix[PLN_CEILING].height;
                    vBL[VZ] = vBR[VZ] = bceil;

                    if(devSkyMode)
                        prepareSkyMaskPoly(rvertices, rtexcoords, rTU, seg->length,
                                           frontsec->SP_ceilmaterial);


                    RL_AddPoly(PT_TRIANGLE_STRIP,
                               (devSkyMode? RPT_NORMAL : RPT_SKY_MASK),
                               rvertices, rtexcoords, NULL, NULL,
                               rcolors, 4, 0, 0, NULL, rTU);
                }

                // Ensure we add a solid view seg.
                seg->frameFlags |= SEGINF_BACKSECSKYFIX;
            }
        }
    } while((hEdge = hEdge->next) != face->hEdge);
}

/**
 * Creates new occlusion planes from the subsector's sides.
 * Before testing, occlude subsector's backfaces. After testing occlude
 * the remaining faces, i.e. the forward facing segs. This is done before
 * rendering segs, so solid segments cut out all unnecessary oranges.
 */
static void occludeSubsector(const face_t* face, boolean forwardFacing)
{
    float               fronth[2], backh[2];
    float*              startv, *endv;
    sector_t*           front, *back;
    hedge_t*            hEdge;

    if(devNoCulling || P_IsInVoid(viewPlayer))
        return;

    front = ((subsector_t*) face->data)->sector;

    fronth[0] = front->SP_floorheight;
    fronth[1] = front->SP_ceilheight;

    if((hEdge = face->hEdge))
    {
        do
        {
            seg_t*              seg = (seg_t*) hEdge->data,
                               *backSeg = hEdge->twin ? ((seg_t*) hEdge->twin->data) : NULL;

            // Occlusions can only happen where two sectors contact.
            if(seg->lineDef && backSeg &&
               (forwardFacing == ((seg->frameFlags & SEGINF_FACINGFRONT)? true : false)))
            {
                back = ((subsector_t*) hEdge->twin->face->data)->sector;

                backh[0] = back->SP_floorheight;
                backh[1] = back->SP_ceilheight;
                // Choose start and end vertices so that it's facing forward.
                if(forwardFacing)
                {
                    startv = hEdge->HE_v1pos;
                    endv   = hEdge->HE_v2pos;
                }
                else
                {
                    startv = hEdge->HE_v2pos;
                    endv   = hEdge->HE_v1pos;
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
                        C_AddViewRelOcclusion(startv, endv, MAX_OF(fronth[0], backh[0]),
                                              false);
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
                        C_AddViewRelOcclusion(startv, endv, MIN_OF(fronth[1], backh[1]),
                                              true);
                    }
                }
            }
        } while((hEdge = hEdge->next) != face->hEdge);
    }
}

static void Rend_RenderSubsector(uint faceidx)
{
    uint                i;
    face_t*             face = FACE_PTR(faceidx);
    subsector_t*        ssec = (subsector_t*) face->data;
    sector_t*           sect;
    float               sceil, sfloor;

    if(!ssec->sector)
    {   // An orphan subsector.
        return;
    }

    R_PickSubsectorFanBase(face);
    R_CreateBiasSurfacesInSubsector(face);

    sect = ssec->sector;
    sceil = sect->SP_ceilvisheight;
    sfloor = sect->SP_floorvisheight;

    if(sceil - sfloor <= 0 || ssec->hEdgeCount < 3)
    {
        // Skip this, it has no volume.
        // Neighbors handle adding the solid clipper segments.
        return;
    }

    if(!firstsubsector)
    {
        if(!C_CheckFace(face))
            return; // This isn't visible.
    }
    else
    {
        firstsubsector = false;
    }

    // Mark the sector visible for this frame.
    sect->frameFlags |= SIF_VISIBLE;

    // Retrieve the sector light color.
    sLightColor = R_GetSectorLightColor(sect);

    Rend_MarkSegsFacingFront(face);

    R_InitForSubsector(face);

    Rend_RadioSubsectorEdges(face);

    occludeSubsector(face, false);
    LO_ClipInSubsector(face);
    occludeSubsector(face, true);

    if(ssec->polyObj)
    {
        // Polyobjs don't obstruct, do clip lights with another algorithm.
        LO_ClipInSubsectorBySight(face);
    }

    // Mark the particle generators in the sector visible.
    Rend_ParticleMarkInSectorVisible(sect);

    // Sprites for this subsector have to be drawn. This must be done before
    // the segments of this subsector are added to the clipper. Otherwise
    // the sprites would get clipped by them, and that wouldn't be right.
    R_AddSprites(face);

    // Draw the various skyfixes for all front facing segs in this ssec
    // (includes polyobject segs).
    if(ssec->sector->planeCount > 0)
    {
        boolean             doSkyFixes;

        doSkyFixes = false;
        i = 0;
        do
        {
            if(IS_SKYSURFACE(&ssec->sector->SP_planesurface(i)))
                doSkyFixes = true;
            else
                i++;
        } while(!doSkyFixes && i < ssec->sector->planeCount);

        if(doSkyFixes)
            Rend_SSectSkyFixes(face);
    }

    // Draw the walls.
    {
    hedge_t*            hEdge;
    if((hEdge = face->hEdge))
    {
        do
        {
            seg_t*              seg = (seg_t*) hEdge->data;

            if(seg->lineDef && // "minisegs" have no linedefs.
               (seg->frameFlags & SEGINF_FACINGFRONT))
            {
                boolean             solid;

                if(!hEdge->twin || !((seg_t*) hEdge->twin->data)->sideDef)
                    solid = Rend_RenderSSWallSeg(hEdge, &hEdge->HE_v1->v, &hEdge->HE_v2->v);
                else
                    solid = Rend_RenderWallSeg(hEdge, &hEdge->HE_v1->v, &hEdge->HE_v2->v);

                if(solid)
                {
                    C_AddViewRelSeg(hEdge->HE_v1pos[VX], hEdge->HE_v1pos[VY],
                                    hEdge->HE_v2pos[VX], hEdge->HE_v2pos[VY]);
                }
            }
        } while((hEdge = hEdge->next) != face->hEdge);
    }
    }

    // Is there a polyobj on board?
    if(ssec->polyObj)
    {
        polyobj_t*          po = ssec->polyObj;

        for(i = 0; i < po->numSegs; ++i)
        {
            poseg_t*            seg = &po->segs[i];
            linedef_t*          line = seg->lineDef;

            // Front facing?
            if(!(Rend_FacingViewerDot(line->L_v1pos, line->L_v2pos) < 0))
            {
                boolean             solid;

                LINE_FRONTSIDE(line)->SW_middleinflags |= SUIF_PVIS;

                if((solid = Rend_RenderPolyobjSeg(face, seg)))
                {
                    C_AddViewRelSeg(line->L_v1pos[VX], line->L_v1pos[VY],
                                    line->L_v2pos[VX], line->L_v2pos[VY]);
                }
            }
        }
    }

    // Render all planes of this sector.
    for(i = 0; i < sect->planeCount; ++i)
    {
        int                 texMode;
        float               height, texOffset[2], texScale[2];
        const plane_t*      plane = sect->planes[i];
        const surface_t*    suf = &plane->surface;
        material_t*         mat;
        boolean             isGlowing = false;

        // Determine plane height.
        if(!IS_SKYSURFACE(suf))
        {
            height = plane->visHeight;
        }
        else
        {
            // We don't render planes for unclosed sectors when the polys would
            // be added to the skymask (a DOOM.EXE renderer hack).
            if(sect->flags & SECF_UNCLOSED)
                return;

            if(plane->type != PLN_MID)
                height = skyFix[plane->type].height;
            else
                height = plane->visHeight;
        }

        if(renderTextures == 2)
            texMode = 2;
        else if(!suf->material ||
                (devNoTexFix && (suf->inFlags & SUIF_MATERIAL_FIX)))
            texMode = 1;
        else
            texMode = 0;

        if(texMode == 0)
            mat = suf->material;
        else if(texMode == 1)
            // For debug, render the "missing" texture instead of the texture
            // chosen for surfaces to fix the HOMs.
            mat = P_GetMaterial(DDT_MISSING, MN_SYSTEM);
        else
            // For lighting debug, render all solid surfaces using the gray texture.
            mat = P_GetMaterial(DDT_GRAY, MN_SYSTEM);

        V2_Copy(texOffset, suf->visOffset);

        // Add the Y offset to orient the Y flipped texture.
        if(plane->type == PLN_CEILING)
            texOffset[VY] -= ssec->bBox[1].pos[VY] - ssec->bBox[0].pos[VY];

        // Add the additional offset to align with the worldwide grid.
        texOffset[VX] += ssec->worldGridOffset[VX];
        texOffset[VY] += ssec->worldGridOffset[VY];

        texScale[VX] = ((suf->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        texScale[VY] = ((suf->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        if(glowingTextures &&
           (texMode == 1 || ((suf->flags & DDSUF_GLOW) ||
                             (mat && (mat->flags & MATF_GLOW)))))
            isGlowing = true;

        Rend_RenderPlane(face, plane->type, height, suf->normal, mat,
                         suf->materialB, suf->matBlendFactor,
                         suf->inFlags, suf->rgba,
                         suf->blendMode, texOffset, texScale,
                         IS_SKYSURFACE(suf), true, isGlowing,
                         ssec->bsuf[plane->planeID], plane->planeID,
                         texMode);
    }

    if(devSkyMode == 2)
    {
        /**
         * In devSky mode 2, we draw additional geometry, showing the
         * real "physical" height of any sky masked planes that are
         * drawn at a different height due to the skyFix.
         */
        if(sect->SP_floorvisheight > skyFix[PLN_FLOOR].height &&
           IS_SKYSURFACE(&sect->SP_floorsurface))
        {
            vec3_t              normal;
            const plane_t*      plane = sect->SP_plane(PLN_FLOOR);
            const surface_t*    suf = &plane->surface;

            /**
             * Flip the plane normal according to the z positon of the
             * viewer relative to the plane height so that the plane is
             * always visible.
             */
            V3_Copy(normal, plane->PS_normal);
            if(vy < plane->visHeight)
                normal[VZ] *= -1;

            Rend_RenderPlane(face, PLN_MID, plane->visHeight, normal,
                             P_GetMaterial(DDT_GRAY, MN_SYSTEM), NULL, 0,
                             suf->inFlags, suf->rgba,
                             BM_NORMAL, NULL, NULL, false,
                             (vy > plane->visHeight? true : false),
                             false, NULL,
                             plane->planeID, 2);
        }

        if(sect->SP_ceilvisheight < skyFix[PLN_CEILING].height &&
           IS_SKYSURFACE(&sect->SP_ceilsurface))
        {
            vec3_t              normal;
            const plane_t*      plane = sect->SP_plane(PLN_CEILING);
            const surface_t*    suf = &plane->surface;

            V3_Copy(normal, plane->PS_normal);
            if(vy > plane->visHeight)
                normal[VZ] *= -1;

            Rend_RenderPlane(face, PLN_MID, plane->visHeight, normal,
                             P_GetMaterial(DDT_GRAY, MN_SYSTEM), NULL, 0,
                             suf->inFlags, suf->rgba,
                             BM_NORMAL, NULL, NULL, false,
                             (vy < plane->visHeight? true : false),
                             false, NULL,
                             plane->planeID, 2);
        }
    }
}

static void Rend_RenderNode(uint bspnum)
{
    // If the clipper is full we're pretty much done. This means no geometry
    // will be visible in the distance because every direction has already been
    // fully covered by geometry.
    if(C_IsFull())
        return;

    if(bspnum & NF_SUBSECTOR)
    {
        // We've arrived at a subsector. Render it.
        Rend_RenderSubsector(bspnum & ~NF_SUBSECTOR);
    }
    else
    {
        node_t             *bsp;
        byte                side;

        // Descend deeper into the nodes.
        bsp = NODE_PTR(bspnum);

        // Decide which side the view point is on.
        side = R_PointOnSide(viewX, viewY, &bsp->partition);

        Rend_RenderNode(bsp->children[side]);   // Recursively divide front space.
        Rend_RenderNode(bsp->children[side ^ 1]);   // ...and back space.
    }
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
void Rend_RenderNormals(void)
{
#define NORM_TAIL_LENGTH    (20)

    uint                i;

    if(!devSurfaceNormals)
        return;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    for(i = 0; i < numHEdges; ++i)
    {
        hedge_t*            hEdge = &hEdges[i];
        seg_t*              seg = (seg_t*) hEdge->data,
                           *backSeg = hEdge->twin ? ((seg_t*) hEdge->twin->data) : NULL;
        sidedef_t*          side = seg->sideDef;
        surface_t*          suf;
        vec3_t              origin;
        float               x, y, bottom, top;
        float               scale = NORM_TAIL_LENGTH;

        if(!seg->lineDef || !((subsector_t*) hEdge->face->data)->sector)
            continue;

        x = hEdge->HE_v1pos[VX] + (hEdge->HE_v2pos[VX] - hEdge->HE_v1pos[VX]) / 2;
        y = hEdge->HE_v1pos[VY] + (hEdge->HE_v2pos[VY] - hEdge->HE_v1pos[VY]) / 2;

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
            sector_t*           frontSec = ((subsector_t*) hEdge->face->data)->sector,
                               *backSec = ((subsector_t*) hEdge->twin->face->data)->sector;

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

    for(i = 0; i < numFaces; ++i)
    {
        uint                j;
        const subsector_t*  ssec = (subsector_t*) faces[i].data;

        for(j = 0; j < ssec->sector->planeCount; ++j)
        {
            vec3_t              origin;
            plane_t*            pln = ssec->sector->SP_plane(j);
            float               scale = NORM_TAIL_LENGTH;

            V3_Set(origin, ssec->midPoint.pos[VX], ssec->midPoint.pos[VY],
                   pln->visHeight);
            if(pln->type != PLN_MID && IS_SKYSURFACE(&pln->surface))
                origin[VZ] = skyFix[pln->type].height;

            drawNormal(origin, pln->PS_normal, scale);
        }
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);

#undef NORM_TAIL_LENGTH
}

static void getVertexPlaneMinMax(const vertex_t* vtx, float* min, float* max)
{
    const lineowner_t*  vo, *base;

    if(!vtx || (!min && !max))
        return; // Wha?

    vo = base = vtx->lineOwners;

    do
    {
        const linedef_t*    li = vo->lineDef;

        if(LINE_FRONTSIDE(li))
        {
            const sector_t*     sec = LINE_FRONTSECTOR(li);

            if(min && sec->SP_floorvisheight < *min)
                *min = sec->SP_floorvisheight;

            if(max && sec->SP_ceilvisheight > *max)
                *max = sec->SP_ceilvisheight;
        }

        if(LINE_BACKSIDE(li))
        {
            const sector_t*     sec = LINE_BACKSECTOR(li);

            if(min && sec->SP_floorvisheight < *min)
                *min = sec->SP_floorvisheight;

            if(max && sec->SP_ceilvisheight > *max)
                *max = sec->SP_ceilvisheight;
        }

        vo = vo->LO_next;
    } while(vo != base);
}

static void drawVertexPoint(const vertex_t* vtx, float z, float alpha)
{
    glBegin(GL_POINTS);
        glColor4f(.7f, .7f, .2f, alpha * 2);
        glVertex3f(vtx->V_pos[VX], z, vtx->V_pos[VY]);
    glEnd();
}

static void drawVertexBar(const vertex_t* vtx, float bottom, float top,
                          float alpha)
{
#define EXTEND_DIST     64

    static const float  black[4] = { 0, 0, 0, 0 };

    glBegin(GL_LINES);
        glColor4fv(black);
        glVertex3f(vtx->V_pos[VX], bottom - EXTEND_DIST, vtx->V_pos[VY]);
        glColor4f(1, 1, 1, alpha);
        glVertex3f(vtx->V_pos[VX], bottom, vtx->V_pos[VY]);
        glVertex3f(vtx->V_pos[VX], bottom, vtx->V_pos[VY]);
        glVertex3f(vtx->V_pos[VX], top, vtx->V_pos[VY]);
        glVertex3f(vtx->V_pos[VX], top, vtx->V_pos[VY]);
        glColor4fv(black);
        glVertex3f(vtx->V_pos[VX], top + EXTEND_DIST, vtx->V_pos[VY]);
    glEnd();

#undef EXTEND_DIST
}

static void drawVertexIndex(const vertex_t* vtx, float z, float scale,
                            float alpha)
{
    char                buf[80];

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(vtx->V_pos[VX], z, vtx->V_pos[VY]);
    glRotatef(-vang + 180, 0, 1, 0);
    glRotatef(vpitch, 1, 0, 0);
    glScalef(-scale, -scale, 1);

    sprintf(buf, "%i", vtx - vertexes);
    UI_TextOutEx(buf, 2, 2, false, false, UI_Color(UIC_TITLE), alpha);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

#define MAX_VERTEX_POINT_DIST 1280

static boolean drawVertex1(linedef_t* li, void* context)
{
    vertex_t*           vtx = li->L_v1;
    polyobj_t*          po = context;
    subsector_t*        ssec = (subsector_t*) po->face->data;
    float               dist2D =
        M_ApproxDistancef(vx - vtx->V_pos[VX], vz - vtx->V_pos[VY]);

    if(dist2D < MAX_VERTEX_POINT_DIST)
    {
        float               alpha = 1 - dist2D / MAX_VERTEX_POINT_DIST;

        if(alpha > 0)
        {
            float               bottom = ssec->sector->SP_floorvisheight;
            float               top = ssec->sector->SP_ceilvisheight;

            glDisable(GL_TEXTURE_2D);

            if(devVertexBars)
                drawVertexBar(vtx, bottom, top, MIN_OF(alpha, .15f));

            drawVertexPoint(vtx, bottom, alpha * 2);

            glEnable(GL_TEXTURE_2D);
        }
    }

    if(devVertexIndices)
    {
        float               eye[3], pos[3], dist3D;

        eye[VX] = vx;
        eye[VY] = vz;
        eye[VZ] = vy;

        pos[VX] = vtx->V_pos[VX];
        pos[VY] = vtx->V_pos[VY];
        pos[VZ] = ssec->sector->SP_floorvisheight;

        dist3D = M_Distance(pos, eye);

        if(dist3D < MAX_VERTEX_POINT_DIST)
        {
            drawVertexIndex(vtx, pos[VZ], dist3D / (theWindow->width / 2),
                            1 - dist3D / MAX_VERTEX_POINT_DIST);
        }
    }

    return true; // Continue iteration.
}

boolean drawPolyObjVertexes(polyobj_t* po, void* context)
{
    return P_PolyobjLinesIterator(po, drawVertex1, po, false);
}

/**
 * Draw the various vertex debug aids.
 */
void Rend_Vertexes(void)
{
    uint                i;
    float               oldPointSize, oldLineWidth = 1, bbox[4];

    if(!devVertexBars && !devVertexIndices)
        return;

    glDisable(GL_DEPTH_TEST);

    if(devVertexBars)
    {
        glEnable(GL_LINE_SMOOTH);
        oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
        DGL_SetFloat(DGL_LINE_WIDTH, 2);
        glDisable(GL_TEXTURE_2D);

        for(i = 0; i < numVertexes; ++i)
        {
            vertex_t*           vtx = &vertexes[i];
            float               alpha;

            if(!vtx->lineOwners)
                continue; // Not a linedef vertex.
            if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ)
                continue; // A polyobj linedef vertex.

            alpha = 1 - M_ApproxDistancef(vx - vtx->V_pos[VX],
                                          vz - vtx->V_pos[VY]) / MAX_VERTEX_POINT_DIST;
            alpha = MIN_OF(alpha, .15f);

            if(alpha > 0)
            {
                float               bottom, top;

                bottom = DDMAXFLOAT;
                top = DDMINFLOAT;
                getVertexPlaneMinMax(vtx, &bottom, &top);

                drawVertexBar(vtx, bottom, top, alpha);
            }
        }

        glEnable(GL_TEXTURE_2D);
    }

    // Always draw the vertex point nodes.
    glEnable(GL_POINT_SMOOTH);
    oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);
    DGL_SetFloat(DGL_POINT_SIZE, 6);
    glDisable(GL_TEXTURE_2D);

    for(i = 0; i < numVertexes; ++i)
    {
        vertex_t*           vtx = &vertexes[i];
        float               dist;

        if(!vtx->lineOwners)
            continue; // Not a linedef vertex.
        if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ)
            continue; // A polyobj linedef vertex.

        dist = M_ApproxDistancef(vx - vtx->V_pos[VX], vz - vtx->V_pos[VY]);

        if(dist < MAX_VERTEX_POINT_DIST)
        {
            float               bottom;

            bottom = DDMAXFLOAT;
            getVertexPlaneMinMax(vtx, &bottom, NULL);

            drawVertexPoint(vtx, bottom, (1 - dist / MAX_VERTEX_POINT_DIST) * 2);
        }
    }

    glEnable(GL_TEXTURE_2D);

    if(devVertexIndices)
    {
        float               eye[3];

        eye[VX] = vx;
        eye[VY] = vz;
        eye[VZ] = vy;

        for(i = 0; i < numVertexes; ++i)
        {
            vertex_t*           vtx = &vertexes[i];
            float               pos[3], dist;

            if(!vtx->lineOwners)
                continue; // Not a linedef vertex.
            if(vtx->lineOwners[0].lineDef->inFlags & LF_POLYOBJ)
                continue; // A polyobj linedef vertex.

            pos[VX] = vtx->V_pos[VX];
            pos[VY] = vtx->V_pos[VY];
            pos[VZ] = DDMAXFLOAT;
            getVertexPlaneMinMax(vtx, &pos[VZ], NULL);

            dist = M_Distance(pos, eye);

            if(dist < MAX_VERTEX_POINT_DIST)
            {
                float               alpha, scale;

                alpha = 1 - dist / MAX_VERTEX_POINT_DIST;
                scale = dist / (theWindow->width / 2);

                drawVertexIndex(vtx, pos[VZ], scale, alpha);
            }
        }
    }

    // Next, the vertexes of all nearby polyobjs.
    bbox[BOXLEFT]   = vx - MAX_VERTEX_POINT_DIST;
    bbox[BOXRIGHT]  = vx + MAX_VERTEX_POINT_DIST;
    bbox[BOXBOTTOM] = vy - MAX_VERTEX_POINT_DIST;
    bbox[BOXTOP]    = vy + MAX_VERTEX_POINT_DIST;
    P_PolyobjsBoxIterator(bbox, drawPolyObjVertexes, NULL);

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

void Rend_RenderMap(gamemap_t* map)
{
    binangle_t          viewside;
    boolean             doLums =
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

        // Clear particle generator visibilty info.
        Rend_ParticleInitForNewFrame();

        // Make vissprites of all the visible decorations.
        Rend_ProjectDecorations();

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
            DL_DestroyDynlights(map);
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
        viewsidex = -viewSin;
        viewsidey = viewCos;

        // We don't want subsector clipchecking for the first subsector.
        firstsubsector = true;
        Rend_RenderNode(numNodes - 1);

        Rend_RenderShadows();
    }
    RL_RenderAllLists();

    // Draw various debugging displays:
    Rend_RenderNormals(); // World surface normals.
    LO_DrawLumobjs(); // Lumobjs.
    Rend_RenderBoundingBoxes(); // Mobj bounding boxes.
    Rend_Vertexes(); // World vertex positions/indices.
    Rend_RenderGenerators(); // Particle generator origins.

    if(!freezeRLs)
    {
        // Draw the Source Bias Editor's draw.
        SBE_DrawCursor(map);
    }

    GL_SetMultisample(false);
}

/**
 * Updates the lightModRange which is used to applify sector light to help
 * compensate for the differences between the OpenGL lighting equation,
 * the software Doom lighting model and the light grid (ambient lighting).
 *
 * The offsets in the lightRangeModTables are added to the sector->lightLevel
 * during rendering (both positive and negative).
 */
void Rend_CalcLightModRange(cvar_t *unused)
{
    int                 j;
    int                 mapAmbient;
    float               f;
    gamemap_t          *map = P_GetCurrentMap();

    memset(lightModRange, 0, sizeof(float) * 255);

    mapAmbient = P_GetMapAmbientLightLevel(map);
    if(mapAmbient > ambientLight)
        rAmbient = mapAmbient;
    else
        rAmbient = ambientLight;

    for(j = 0; j < 255; ++j)
    {
        // Adjust the white point/dark point?
        f = 0;
        if(lightRangeCompression != 0)
        {
            if(lightRangeCompression >= 0) // Brighten dark areas.
                f = (float) (255 - j) * lightRangeCompression;
            else // Darken bright areas.
                f = (float) -j * -lightRangeCompression;
        }

        // Lower than the ambient limit?
        if(rAmbient != 0 && j+f <= rAmbient)
            f = rAmbient - j;

        // Clamp the result as a modifier to the light value (j).
        if((j+f) >= 255)
            f = 255 - j;
        else if((j+f) <= 0)
            f = -j;

        // Insert it into the matrix
        lightModRange[j] = f / 255.0f;
    }
}

/**
 * Applies the offset from the lightModRangeto the given light value.
 *
 * The lightModRange is used to implement (precalculated) ambient light
 * limit, light range compression and light range shift.
 *
 * \note There is no need to clamp the result. Since the offset values in
 *       the lightModRange have already been clamped so that the resultant
 *       lightvalue is NEVER outside the range 0-254 when the original
 *       lightvalue is used as the index.
 *
 * @param lightvar      Ptr to the value to apply the adaptation to.
 */
void Rend_ApplyLightAdaptation(float *lightvar)
{
    int                 lightval;

    if(lightvar == NULL)
        return; // Can't apply adaptation to a NULL val ptr...

    lightval = ROUND(255.0f * *lightvar);
    if(lightval > 254)
        lightval = 254;
    else if(lightval < 0)
        lightval = 0;

    *lightvar += lightModRange[lightval];
}

/**
 * Same as above but instead of applying light adaptation to the var directly
 * it returns the light adaptation value for the passed light value.
 *
 * @param lightvalue    Light value to look up the adaptation value of.
 * @return int          Adaptation value for the passed light value.
 */
float Rend_GetLightAdaptVal(float lightvalue)
{
    int                 lightval;

    lightval = ROUND(255.0f * lightvalue);
    if(lightval > 254)
        lightval = 254;
    else if(lightval < 0)
        lightval = 0;

    return lightModRange[lightval];
}

/**
 * Draws the lightModRange (for debug)
 */
void R_DrawLightRange(void)
{
#define bWidth          1.0f
#define bHeight         (bWidth * 255.0f)
#define BORDER          20

    int                 i;
    float               c, off;
    ui_color_t          color;

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
static void Rend_RenderBoundingBoxes(void)
{
    static const float  red[3] = { 1, 0.2f, 0.2f}; // non-solid objects
    static const float  green[3] = { 0.2f, 1, 0.2f}; // solid objects
    static const float  yellow[3] = {0.7f, 0.7f, 0.2f}; // missiles

    uint                i;
    float               size, alpha, eye[3];
    mobj_t*             mo;
    sector_t*           sec;
    material_t*         mat;
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

    mat = P_GetMaterial(DDT_BBOX, MN_SYSTEM);
    Material_Prepare(&ms, mat, true, NULL);

    GL_BindTexture(ms.units[MTU_PRIMARY].texInst->id,
                   ms.units[MTU_PRIMARY].magMode);
    GL_BlendMode(BM_ADD);

    // For every sector
    for(i = 0; i < numSectors; ++i)
    {
        sec = SECTOR_PTR(i);

        // Is it vissible?
        if(!(sec->frameFlags & SIF_VISIBLE))
            continue;

        // For every mobj in the sector's mobjList
        for(mo = sec->mobjList; mo; mo = mo->sNext)
        {
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
