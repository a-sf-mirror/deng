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

#ifndef DOOMSDAY_RENDSEG_H
#define DOOMSDAY_RENDSEG_H

// temporary includes:
#include "rend_list.h"
#include "p_material.h"

/**
 * @defGroup rendSegFlags Rend Seg Flags.
 */
/*@{*/
#define RSF_SKYMASK         (0x1)
#define RSF_GLOW            (0x2)
#define RSF_NO_RADIO        (0x4)
#define RSF_NO_REFLECTION   (0x8)
/*@}*/

typedef struct rendseg_s {
    byte                flags; // @see rendSegFlags.

    float               from[2], to[2];
    float               bottom, top;

    pvec3_t             normal;

    float               texQuadTopLeft[3], texQuadBottomRight[3];
    float               texQuadWidth;

    float               sectorLightLevel;
    const float*        sectorLightColor;

    float               surfaceLightLevelDelta;
    const float*        surfaceColorTint, *surfaceColorTint2;
    float               surfaceMaterialOffset[2];
    float               surfaceMaterialScale[2];
    biassurface_t*      biasSurface;
    sideradioconfig_t*  radioConfig;
    float               alpha; // @c < 0 signifies "force opaque"
    blendmode_t         blendMode;
    uint                dynlistID;

    // @todo get rid of divs in their current form.
    // Use a list of "plane interfaces" attached to each world vertex
    // and sorted by visheight.
    walldiv_t           divs[2];

    // @todo members not wanted in this class!
    struct {
        material_snapshot_t snapshotA, snapshotB;
        float           inter;
        boolean         blend;
    } materials;
} rendseg_t;

// Constructors:
rendseg_t*      RendSeg_staticConstructFromHEdgeSection(rendseg_t* newRendSeg, hedge_t* hEdge, segsection_t section,
                                   float from[2], float to[2], float bottom, float top,
                                   const float materialOffset[2], const float materialScale[2]);
rendseg_t*      RendSeg_staticConstructFromPolyobjSideDef(rendseg_t* newRendSeg, sidedef_t* sideDef,
                                     float from[2], float to[2], float bottom, float top,
                                     subsector_t* subsector, poseg_t* poSeg);

// Public methods:
boolean         RendSeg_SkyMasked(rendseg_t* rseg);
boolean         RendSeg_MustUseVisSprite(rendseg_t* rseg);
uint            RendSeg_NumRequiredVertices(rendseg_t* rseg);
uint            RendSeg_DynlistID(rendseg_t* rseg);

// @todo once the material snapshots are owned by the Materials class, these
// become accessors.
void            RendSeg_TakeBlendedMaterialSnapshots(rendseg_t* rseg, material_t* materialA, float blendFactor, material_t* materialB);
void            RendSeg_TakeMaterialSnapshots(rendseg_t* rseg, material_t* material);

#endif /* DOOMSDAY_RENDSEG_H */
