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

#include "rendplane.h"

// temporary includes:
#include "rend_list.h"
#include "p_material.h"

// MACROS ------------------------------------------------------------------

#define RPLANE_MIN_ALPHA      (0.0001f)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void pickMaterialsAndGetDrawFlags(rendplane_t* rplane, plane_t* plane,
                                         uint planeId, float alpha,
                                         material_t** materialA,
                                         float* materialBlendInter,
                                         material_t** materialB)
{
    int texMode = 0;
    surface_t* surface = &plane->surface;
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
            rplane->flags |= RPF_GLOW;
        }
        else
        {   // We'll mask this.
            rplane->flags |= RPF_SKYMASK;
            *materialA = NULL;
        }
    }
    else
    {
        int surfaceFlags, surfaceInFlags;

        // Determine which texture to use.
        if(renderTextures == 2)
            texMode = 2;
        else if(!surface->material)
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
            rplane->flags |= RPF_GLOW; // Make it stand out
        }

        if(planeId <= PLN_CEILING)
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
            rplane->flags |= RPF_GLOW;

        if(surfaceInFlags & SUIF_NO_RADIO)
            rplane->flags |= RPF_NO_RADIO;
    }

    if((rplane->flags & RPF_GLOW) || (rplane->flags & RPF_SKYMASK))
        rplane->flags |= RPF_NO_RADIO;

    if(!useShinySurfaces || !(*materialA) || !(*materialA)->reflection)
        rplane->flags |= RPF_NO_REFLECTION;

    if(forceOpaque)
        rplane->alpha = -1;
    else
        rplane->alpha = alpha;
    rplane->blendMode = blendMode;
}

static void init(rendplane_t* rplane, const float from[2], const float to[2],
    face_t* face, float height, pvec3_t normal)
{
    rplane->flags = 0;
    rplane->blendMode = BM_NORMAL;

    rplane->face = face;
    rplane->height = height;

    rplane->normal = normal;

    V3_Set(rplane->texQuadTopLeft, from[VX], from[VY], height);
    V3_Set(rplane->texQuadBottomRight, to[VX], to[VY], height);

    rplane->alpha = 1;
    rplane->surfaceColorTint = NULL;
    rplane->dynlistID = 0;

    rplane->materials.blend = false;
    rplane->materials.inter = 0;
}

static void projectLumobjs(rendplane_t* rplane, subsector_t* subsector, boolean sortBrightest)
{
    rplane->dynlistID = DL_ProjectOnSurface(P_CurrentMap(), subsector, rplane->texQuadTopLeft,
                                          rplane->texQuadBottomRight, rplane->normal,
                                          sortBrightest? DLF_SORT_LUMADSC : 0);
}

static void constructor(rendplane_t* rplane, const float from[2], const float to[2],
    face_t* face, float height, pvec3_t normal, uint planeId,
    float sectorLightLevel, const float* sectorLightColor,
    float surfaceLightLevelDelta, const float* surfaceColorTint, float alpha,
    biassurface_t* biasSurface, const float materialOffset[2],
    const float materialScale[2], boolean lightWithLumobjs)
{
    float materialBlendInter;
    subsector_t* subsector = (subsector_t*) face->data;
    material_t* materialA = NULL, *materialB = NULL;

    init(rplane, from, to, face, height, normal);
    rplane->numVertices = subsector->hEdgeCount + (subsector->useMidPoint? 2 : 0);

    V2_Set(rplane->midPoint, subsector->midPoint[0], subsector->midPoint[1]);

    rplane->sectorLightLevel = sectorLightLevel;
    rplane->sectorLightColor = sectorLightColor;

    rplane->surfaceLightLevelDelta = surfaceLightLevelDelta;
    rplane->surfaceColorTint = surfaceColorTint;
    rplane->biasSurface = biasSurface;

    V2_Copy(rplane->surfaceMaterialOffset, materialOffset);
    V2_Copy(rplane->surfaceMaterialScale, materialScale);

    pickMaterialsAndGetDrawFlags(rplane, subsector->sector->planes[planeId], planeId, alpha,
                                 &materialA, &materialBlendInter, &materialB);

    if(!(rplane->flags & RPF_SKYMASK))
    {
        if(lightWithLumobjs && !(rplane->flags & RPF_GLOW))
            projectLumobjs(rplane, subsector, false);

        // @todo: defer to the Materials class.
        if(!(materialB && smoothTexAnim))
        {
            RendPlane_TakeMaterialSnapshots(rplane, materialA);
        }
        else
        {
            RendPlane_TakeBlendedMaterialSnapshots(rplane, materialA, materialBlendInter, materialB);
        }
    }
}

/**
 * RendPlane:: class static constructor helper.
 */
rendplane_t* RendPlane_staticConstructFromFacePlane(rendplane_t* newRendPlane,
    face_t* face, uint planeId, const float from[2], const float to[2], float height,
    const float materialOffset[2], const float materialScale[2])
{
    assert(newRendPlane);
    assert(face);
    assert(from);
    assert(to);
    assert(materialOffset);
    assert(materialScale);
    {
    rendplane_t* rplane = newRendPlane; // allocate.

    subsector_t* ssec = (subsector_t*) face->data;
    surface_t* surface = &ssec->sector->planes[planeId]->surface;
    float alpha, sectorLightLevel, surfaceLightLevelDelta;
    const float* sectorLightColor, *surfaceColorTint;

    if(surface->material && (surface->material->flags & MATF_NO_DRAW))
        return NULL; // @todo return null_object

    /**
     * @note Emulate DOOM.exe renderer, map geometry construct:
     * Do not render planes for subsectors not enclosed by their sector
     * boundary if they would be added to the skymask.
     */
    if(devRendSkyMode == 0 &&
       IS_SKYSURFACE(surface) && (ssec->sector->flags & SECF_UNCLOSED))
        return NULL; // @todo return null_object

    alpha = 1;
    surfaceColorTint = surface->rgba;

    sectorLightLevel = ssec->sector->lightLevel;
    sectorLightColor = R_GetSectorLightColor(ssec->sector);
    surfaceLightLevelDelta = 0;

    constructor(rplane, from, to, face, height, surface->normal, planeId,
                sectorLightLevel, sectorLightColor,
                surfaceLightLevelDelta, surfaceColorTint, alpha,
                ssec->bsuf[planeId],
                materialOffset, materialScale, true);

    return rplane;
    }
}

boolean RendPlane_SkyMasked(rendplane_t* rplane)
{
    assert(rplane);
    {
    return (rplane->flags & RPF_SKYMASK) == 1;
    }
}

void RendPlane_TakeBlendedMaterialSnapshots(rendplane_t* rplane, material_t* materialA, float blendFactor,
                                            material_t* materialB)
{
    assert(rplane);
    assert(materialA);
    assert(materialB);
    {
    memset(&rplane->materials.snapshotA, 0, sizeof(rplane->materials.snapshotA));
    memset(&rplane->materials.snapshotB, 0, sizeof(rplane->materials.snapshotB));

    Materials_Prepare(materialA, MPF_SMOOTH, NULL, &rplane->materials.snapshotA);
    Materials_Prepare(materialB, MPF_SMOOTH, NULL, &rplane->materials.snapshotB);

    rplane->materials.inter = blendFactor;
    rplane->materials.blend = true;
    }
}

void RendPlane_TakeMaterialSnapshots(rendplane_t* rplane, material_t* material)
{
    assert(rplane);
    assert(material);
    {
    float inter = 0;
    boolean smoothed = false;

    memset(&rplane->materials.snapshotA, 0, sizeof(rplane->materials.snapshotA));
    memset(&rplane->materials.snapshotB, 0, sizeof(rplane->materials.snapshotB));

    Materials_Prepare(material, MPF_SMOOTH, NULL, &rplane->materials.snapshotA);

    if(smoothTexAnim)
    {
        // If fog is active, inter=0 is accepted as well. Otherwise
        // flickering may occur if the rendering passes don't match for
        // blended and unblended surfaces.
        if(numTexUnits > 1 && material->current != material->next &&
           !(!usingFog && material->inter < 0))
        {
            // Prepare the inter texture.
            Materials_Prepare(material->next, 0, NULL, &rplane->materials.snapshotB);
            inter = material->inter;

            smoothed = true;
        }
    }

    rplane->materials.blend = smoothed;
    rplane->materials.inter = inter;
    }
}

uint RendPlane_DynlistID(rendplane_t* rplane)
{
    assert(rplane);
    {
    return rplane->dynlistID;
    }
}

uint RendPlane_NumRequiredVertices(rendplane_t* rplane)
{
    assert(rplane);
    {
    return rplane->numVertices;
    }
}
