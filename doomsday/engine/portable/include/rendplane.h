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

#ifndef DOOMSDAY_RENDPLANE_H
#define DOOMSDAY_RENDPLANE_H

// temporary includes:
#include "rend_list.h"
#include "rend_fakeradio.h"
#include "p_material.h"

/**
 * @defGroup rendPlaneFlags Rend Plane Flags.
 */
/*@{*/
#define RPF_SKYMASK         (0x1)
#define RPF_GLOW            (0x2)
#define RPF_NO_RADIO        (0x4)
#define RPF_NO_REFLECTION   (0x8)
/*@}*/

typedef struct rendplane_s {
    byte                flags; // @see rendPlaneFlags.

    face_t*             face;
    float               height;
    float               midPoint[2];
    uint                numVertices;

    pvec3_t             normal;

    float               texQuadTopLeft[3], texQuadBottomRight[3];

    float               sectorLightLevel;
    const float*        sectorLightColor;

    float               surfaceLightLevelDelta;
    const float*        surfaceColorTint; //, *surfaceColorTint2;
    float               surfaceMaterialOffset[2];
    float               surfaceMaterialScale[2];
    biassurface_t*      biasSurface;
    float               alpha; // @c < 0 signifies "force opaque"
    blendmode_t         blendMode;
    uint                dynlistID;

    /*rendseg_shadow_t    radioConfig[4];

    // @todo get rid of divs in their current form.
    // Use a list of "plane interfaces" attached to each world vertex
    // and sorted by visheight.
    walldiv_t           divs[2];*/

    // @todo members not wanted in this class!
    struct {
        material_snapshot_t snapshotA, snapshotB;
        float           inter;
        boolean         blend;
    } materials;
} rendplane_t;

// Constructors:
rendplane_t*    RendPlane_staticConstructFromFacePlane(rendplane_t* newRendPlane,
    face_t* face, uint planeId, const float from[2], const float to[2], float height,
    const float materialOffset[2], const float materialScale[2]);

// Public methods:
boolean         RendPlane_SkyMasked(rendplane_t* rplane);
uint            RendPlane_NumRequiredVertices(rendplane_t* rplane);
uint            RendPlane_DynlistID(rendplane_t* rplane);

// @todo once the material snapshots are owned by the Materials class, these
// become accessors.
void            RendPlane_TakeBlendedMaterialSnapshots(rendplane_t* rplane, material_t* materialA, float blendFactor, material_t* materialB);
void            RendPlane_TakeMaterialSnapshots(rendplane_t* rplane, material_t* material);

#endif /* DOOMSDAY_RENDPLANE_H */
