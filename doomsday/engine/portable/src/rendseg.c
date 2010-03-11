/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"

#include "rendseg.h"

// temporary includes:
#include "rend_list.h"
#include "p_material.h"

// MACROS ------------------------------------------------------------------

#define RSEG_MIN_ALPHA      (0.0001f)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void pickMaterialsAndGetDrawFlags(rendseg_t* rseg, sidedef_t* sideDef,
                                         segsection_t section, float alpha,
                                         material_t** materialA,
                                         float* materialBlendInter,
                                         material_t** materialB)
{
    int texMode = 0;
    surface_t* surface = &sideDef->SW_surface(section);
    boolean forceOpaque = false;
    blendmode_t blendMode = BM_NORMAL;

    *materialBlendInter = 0;

    // Fill in the remaining params data.
    if(IS_SKYSURFACE(surface))
    {
        // In devRendSkyMode mode we render all polys destined for the skymask
        // as regular world polys (with a few obvious properties).
        if(devRendSkyMode)
        {
            *materialA = surface->material;

            forceOpaque = true;
            rseg->flags |= RSF_GLOW;
        }
        else
        {   // We'll mask this.
            rseg->flags |= RSF_SKYMASK;
            *materialA = NULL;
        }
    }
    else
    {
        int surfaceFlags, surfaceInFlags;

        // Determine which texture to use.
        if(renderTextures == 2)
            texMode = 2;
        else if(!surface->material || ((surface->inFlags & SUIF_MATERIAL_FIX) && devNoTexFix))
            texMode = 1;
        else
            texMode = 0;

        if(texMode == 0)
        {
            *materialA = surface->material;
            *materialB = surface->materialB;
            *materialBlendInter = surface->matBlendFactor;
        }
        else if(texMode == 1)
            // For debug, render the "missing" texture instead of the texture
            // chosen for surfaces to fix the HOMs.
            *materialA = Materials_ToMaterial2(MN_SYSTEM, DDT_MISSING);
        else // texMode == 2
            // For lighting debug, render all solid surfaces using the gray
            // texture.
            *materialA = Materials_ToMaterial2(MN_SYSTEM, DDT_GRAY);

        // Make any necessary adjustments to the surface flags to suit the
        // current texture mode.
        surfaceFlags = surface->flags;
        surfaceInFlags = surface->inFlags;
        if(texMode == 1)
        {
            rseg->flags |= RSF_GLOW; // Make it stand out
        }
        else if(texMode == 2)
        {
            surfaceInFlags &= ~(SUIF_MATERIAL_FIX);
        }

        if(section != SEG_MIDDLE || !LINE_BACKSIDE(sideDef->lineDef))
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
            rseg->flags |= RSF_GLOW;

        if(surfaceInFlags & SUIF_NO_RADIO)
            rseg->flags |= RSF_NO_RADIO;
    }

    if((rseg->flags & RSF_GLOW) || (rseg->flags & RSF_SKYMASK))
        rseg->flags |= RSF_NO_RADIO;

    if(!useShinySurfaces || !(*materialA) || !(*materialA)->reflection)
        rseg->flags |= RSF_NO_REFLECTION;

    if(forceOpaque)
        rseg->alpha = -1;
    else
        rseg->alpha = alpha;
    rseg->blendMode = blendMode;
}

static void init(rendseg_t* rseg, float from[2], float to[2], float bottom, float top,
                 pvec3_t normal)
{
    rseg->flags = 0;
    rseg->blendMode = BM_NORMAL;

    V2_Copy(rseg->from, from);
    V2_Copy(rseg->to, to);
    rseg->bottom = bottom;
    rseg->top = top;

    rseg->divs[0].num = rseg->divs[1].num = 0;

    rseg->normal = normal;

    V3_Set(rseg->texQuadTopLeft, from[VX], from[VY], top);
    V3_Set(rseg->texQuadBottomRight, to[VX], to[VY], bottom);

    rseg->alpha = 1;
    rseg->surfaceColorTint = NULL;
    rseg->surfaceColorTint2 = NULL;
    rseg->dynlistID = 0;

    rseg->materials.blend = false;
    rseg->materials.inter = 0;
}

static void projectLumobjs(rendseg_t* rseg, subsector_t* subsector, boolean sortBrightest)
{
    rseg->dynlistID = DL_ProjectOnSurface(P_CurrentMap(), subsector, rseg->texQuadTopLeft,
                                          rseg->texQuadBottomRight, rseg->normal,
                                          sortBrightest? DLF_SORT_LUMADSC : 0);
}

static void constructor(rendseg_t* rseg, float from[2], float to[2],
                        float bottom, float top,
                        pvec3_t normal, subsector_t* subsector, sidedef_t* frontSideDef, segsection_t section,
                        float sectorLightLevel, const float* sectorLightColor,
                        float surfaceLightLevelDeltaL, float surfaceLightLevelDeltaR, const float* surfaceColorTint,
                        const float* surfaceColorTint2, float alpha,
                        biassurface_t* biasSurface,
                        const float materialOffset[2], const float materialScale[2],
                        boolean lightWithLumobjs)
{
    float materialBlendInter;
    material_t* materialA = NULL, *materialB = NULL;

    init(rseg, from, to, bottom, top, normal);

    rseg->sectorLightLevel = sectorLightLevel;
    rseg->sectorLightColor = sectorLightColor;

    rseg->surfaceLightLevelDeltaL = surfaceLightLevelDeltaL;
    rseg->surfaceLightLevelDeltaR = surfaceLightLevelDeltaR;
    rseg->surfaceColorTint = surfaceColorTint;
    rseg->surfaceColorTint2 = surfaceColorTint2;
    rseg->biasSurface = biasSurface;

    V2_Copy(rseg->surfaceMaterialOffset, materialOffset);
    V2_Copy(rseg->surfaceMaterialScale, materialScale);

    pickMaterialsAndGetDrawFlags(rseg, frontSideDef, section, alpha,
                                 &materialA, &materialBlendInter, &materialB);

    if(!(rseg->flags & RSF_SKYMASK))
    {
        if(lightWithLumobjs && !(rseg->flags & RSF_GLOW))
        {
            boolean sortBrightest =
                (section == SEG_MIDDLE && LINE_BACKSIDE(frontSideDef->lineDef));

            projectLumobjs(rseg, subsector, sortBrightest);
        }

        // @todo: defer to the Materials class.
        if(!(materialB && smoothTexAnim))
        {
            RendSeg_TakeMaterialSnapshots(rseg, materialA);
        }
        else
        {
            RendSeg_TakeBlendedMaterialSnapshots(rseg, materialA, materialBlendInter, materialB);
        }
    }
}

static void getShadowRendSegs(rendseg_t* rseg, float bottomLeft, float topLeft, float topRight,
                        float shadowSize, float shadowDark, sideradioconfig_t* radioConfig,
                        float segOffset, float lineDefLength,
                        const sector_t* frontSec, const sector_t* backSec)
{
    const float* fFloor, *fCeil, *bFloor, *bCeil;
    boolean bottomGlow, topGlow;

    bottomGlow = R_IsGlowingPlane(frontSec->SP_plane(PLN_FLOOR));
    topGlow = R_IsGlowingPlane(frontSec->SP_plane(PLN_CEILING));

    fFloor = &frontSec->SP_floorvisheight;
    fCeil = &frontSec->SP_ceilvisheight;
    if(backSec)
    {
        bFloor = &backSec->SP_floorvisheight;
        bCeil  = &backSec->SP_ceilvisheight;
    }
    else
        bFloor = bCeil = NULL;

    if(!topGlow)
    {
        if(topRight > *fCeil - shadowSize && bottomLeft < *fCeil)
            Rend_RadioSetupTopShadow(&rseg->radioConfig[0], shadowSize, topLeft,
                                     segOffset, fFloor, fCeil,
                                     radioConfig, shadowDark);
    }

    if(!bottomGlow)
    {
        if(bottomLeft < *fFloor + shadowSize && topRight > *fFloor)
            Rend_RadioSetupBottomShadow(&rseg->radioConfig[1], shadowSize, topLeft,
                                  segOffset, fFloor, fCeil,
                                  radioConfig, shadowDark);
    }

    // Walls with glowing floor & ceiling get no side shadows.
    // Is there anything better we can do?
    if(!(bottomGlow && topGlow))
    {
        float texQuadWidth = P_AccurateDistance(
            rseg->texQuadBottomRight[VX] - rseg->texQuadTopLeft[VX],
            rseg->texQuadBottomRight[VY] - rseg->texQuadTopLeft[VY]);

        if(radioConfig->sideCorners[0].corner > 0 && segOffset < shadowSize)
            Rend_RadioSetupSideShadow(&rseg->radioConfig[2], shadowSize, bottomLeft,
                                topLeft, false,
                                bottomGlow, topGlow, segOffset,
                                fFloor, fCeil, bFloor, bCeil, lineDefLength,
                                radioConfig->sideCorners, shadowDark);

        if(radioConfig->sideCorners[1].corner > 0 &&
           segOffset + texQuadWidth > lineDefLength - shadowSize)
            Rend_RadioSetupSideShadow(&rseg->radioConfig[3], shadowSize, bottomLeft,
                                topLeft, true,
                                bottomGlow, topGlow, segOffset,
                                fFloor, fCeil, bFloor, bCeil, lineDefLength,
                                radioConfig->sideCorners, shadowDark);
    }
}

/**
 * RendSeg:: class static constructor helper.
 *
 * @todo Replace @a from and @a to arguments with a parametric representation.
 */
rendseg_t* RendSeg_staticConstructFromHEdgeSection(rendseg_t* newRendSeg, hedge_t* hEdge,
                                                   segsection_t section,
                                                   float from[2], float to[2], float bottom, float top,
                                                   const float materialOffset[2],
                                                   const float materialScale[2])
{
    rendseg_t* rseg = newRendSeg; // allocate.

    seg_t* seg = (seg_t*) hEdge->data;
    const surface_t* surface = &HE_FRONTSIDEDEF(hEdge)->SW_surface(section);
    float alpha, sectorLightLevel, surfaceLightLevelDeltaL, surfaceLightLevelDeltaR, diff;
    const float* sectorLightColor, *surfaceColorTint, *surfaceColorTint2;

    if(surface->material && (surface->material->flags & MATF_NO_DRAW))
        return NULL; // @todo return null_object

    alpha = (section == SEG_MIDDLE? surface->rgba[3] : 1.0f);
    if(R_SideDefIsSoftSurface(HE_FRONTSIDEDEF(hEdge), section))
        alpha = R_ApplySoftSurfaceDeltaToAlpha(bottom, top, HE_FRONTSIDEDEF(hEdge), alpha);

    if(alpha < RSEG_MIN_ALPHA)
        return NULL; // @todo return null_object

    SideDef_ColorTints(HE_FRONTSIDEDEF(hEdge), section,
                       &surfaceColorTint, &surfaceColorTint2);

    /**
     * Lighting paramaters are taken from the sector linked to the
     * sideDef this half-edge was produced from.
     */
    sectorLightLevel = HE_FRONTSIDEDEF(hEdge)->sector->lightLevel;
    sectorLightColor = R_GetSectorLightColor(HE_FRONTSIDEDEF(hEdge)->sector);

    Linedef_LightLevelDelta(HE_FRONTSIDEDEF(hEdge)->lineDef, seg->side, &surfaceLightLevelDeltaL, &surfaceLightLevelDeltaR);

    // Linear interpolation of the linedef light deltas to the edges of the seg.
    diff = surfaceLightLevelDeltaR - surfaceLightLevelDeltaL;
    surfaceLightLevelDeltaR = surfaceLightLevelDeltaL + ((seg->offset + seg->length) / HE_FRONTSIDEDEF(hEdge)->lineDef->length) * diff;
    surfaceLightLevelDeltaL += (seg->offset / HE_FRONTSIDEDEF(hEdge)->lineDef->length) * diff;

    // @todo is this still necessary at this time? try to postpone
    // until geometry creation.
    Rend_RadioUpdateLineDef(HE_FRONTSIDEDEF(hEdge)->lineDef, seg->side);

    constructor(rseg, from, to, bottom, top,
                HE_FRONTSIDEDEF(hEdge)->SW_middlenormal,
                (subsector_t*) hEdge->face->data, HE_FRONTSIDEDEF(hEdge), section,
                sectorLightLevel, sectorLightColor,
                surfaceLightLevelDeltaL, surfaceLightLevelDeltaR, surfaceColorTint, surfaceColorTint2, alpha,
                seg->bsuf[section],
                materialOffset, materialScale, true);

    // Check for neighborhood division?
    rseg->divs[0].num = rseg->divs[1].num = 0;
    if(!(section == SEG_MIDDLE &&
         HE_FRONTSIDEDEF(hEdge) && HE_BACKSIDEDEF(hEdge)))
    {
        R_FindSegSectionDivisions(rseg->divs, hEdge, HE_FRONTSECTOR(hEdge), bottom, top);
    }

    // Render Fakeradio polys for this seg?
    if(!(rseg->flags & RSF_NO_RADIO))
    {
        // Determine the shadow properties.
        R_ApplyLightAdaptation(&sectorLightLevel);

        // No point drawing shadows in a pitch black sector.
        if(sectorLightLevel > 0)
        {
            float shadowSize = Rend_RadioShadowSize(sectorLightLevel);
            float shadowDark = Rend_RadioShadowDarkness(sectorLightLevel) *.8f;

            getShadowRendSegs(rseg, bottom, top, top, shadowSize, shadowDark,
                              &HE_FRONTSIDEDEF(hEdge)->radioConfig,
                              seg->offset, HE_FRONTSIDEDEF(hEdge)->lineDef->length,
                              HE_FRONTSECTOR(hEdge), HE_BACKSECTOR(hEdge));
        }
    }

    return rseg;
}

/**
 * RendSeg:: class static constructor helper.
 *
 * @todo Replace @a from and @a to arguments with a parametric representation.
 */
rendseg_t* RendSeg_staticConstructFromPolyobjSideDef(rendseg_t* newRendSeg,
    sidedef_t* sideDef, float from[2], float to[2], float bottom, float top,
    subsector_t* subsector, seg_t* poSeg)
{
    rendseg_t* rseg = newRendSeg; // allocate.

    float materialOffset[2], materialScale[2], offset = 0, surfaceLightLevelDeltaL, surfaceLightLevelDeltaR;
    const float* surfaceColorTint, *surfaceColorTint2;
    sector_t* frontSec = subsector->sector;
    float ffloor = frontSec->SP_floorvisheight, fceil = frontSec->SP_ceilvisheight;
    surface_t* surface = &sideDef->SW_middlesurface;

    if(surface->material && (surface->material->flags & MATF_NO_DRAW))
        return NULL; // @todo return null_object

    if(subsector->sector->SP_floorvisheight >= subsector->sector->SP_ceilvisheight)
        return NULL; // @todo return null_object

    materialOffset[0] = surface->visOffset[0];
    materialOffset[1] = surface->visOffset[1];
    if(sideDef->lineDef->flags & DDLF_DONTPEGBOTTOM)
        materialOffset[1] += -(fceil - ffloor);

    materialScale[0] = ((surface->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
    materialScale[1] = ((surface->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

    SideDef_ColorTints(sideDef, SEG_MIDDLE, &surfaceColorTint,
                       &surfaceColorTint2);

    Linedef_LightLevelDelta(sideDef->lineDef, FRONT, &surfaceLightLevelDeltaL, &surfaceLightLevelDeltaR);

    constructor(rseg, from, to, bottom, top,
                sideDef->SW_middlenormal, subsector, sideDef, SEG_MIDDLE,
                frontSec->lightLevel, R_GetSectorLightColor(frontSec),
                surfaceLightLevelDeltaL, surfaceLightLevelDeltaR, surfaceColorTint, surfaceColorTint2, 1,
                poSeg->bsuf[SEG_MIDDLE],
                materialOffset, materialScale, true);

    return rseg;
}

boolean RendSeg_SkyMasked(rendseg_t* rseg)
{
    return (rseg->flags & RSF_SKYMASK) == 1;
}

boolean RendSeg_MustUseVisSprite(rendseg_t* rseg)
{
    if(!(rseg->alpha < 0) && !(rseg->flags & RSF_SKYMASK) &&
       (rseg->alpha < 1 || !rseg->materials.snapshotA.isOpaque || rseg->blendMode > 0))
        return true;

    return false;
}

void RendSeg_TakeBlendedMaterialSnapshots(rendseg_t* rseg, material_t* materialA, float blendFactor,
                                          material_t* materialB)
{
    memset(&rseg->materials.snapshotA, 0, sizeof(rseg->materials.snapshotA));
    memset(&rseg->materials.snapshotB, 0, sizeof(rseg->materials.snapshotB));

    Materials_Prepare(materialA, MPF_SMOOTH, NULL, &rseg->materials.snapshotA);
    Materials_Prepare(materialB, MPF_SMOOTH, NULL, &rseg->materials.snapshotB);

    rseg->materials.inter = blendFactor;
    rseg->materials.blend = true;
}

void RendSeg_TakeMaterialSnapshots(rendseg_t* rseg, material_t* material)
{
    float               inter = 0;
    boolean             smoothed = false;

    memset(&rseg->materials.snapshotA, 0, sizeof(rseg->materials.snapshotA));
    memset(&rseg->materials.snapshotB, 0, sizeof(rseg->materials.snapshotB));

    Materials_Prepare(material, MPF_SMOOTH, NULL, &rseg->materials.snapshotA);

    if(smoothTexAnim)
    {
        // If fog is active, inter=0 is accepted as well. Otherwise
        // flickering may occur if the rendering passes don't match for
        // blended and unblended surfaces.
        if(numTexUnits > 1 && material->current != material->next &&
           !(!usingFog && material->inter < 0))
        {
            // Prepare the inter texture.
            Materials_Prepare(material->next, 0, NULL, &rseg->materials.snapshotB);
            inter = material->inter;

            smoothed = true;
        }
    }

    rseg->materials.blend = smoothed;
    rseg->materials.inter = inter;
}

uint RendSeg_DynlistID(rendseg_t* rseg)
{
    return rseg->dynlistID;
}

uint RendSeg_NumRequiredVertices(rendseg_t* rseg)
{
    if(!RendSeg_MustUseVisSprite(rseg) && rseg->divs && (rseg->divs[0].num || rseg->divs[1].num))
        return 3 + rseg->divs[0].num + 3 + rseg->divs[1].num;

    return 4;
}
