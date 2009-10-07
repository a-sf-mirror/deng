/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * rend_bias.c: Light/Shadow Bias
 *
 * Calculating macro-scale lighting on the fly.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_edit.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_defs.h"
#include "de_misc.h"
#include "p_sight.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
  PROF_BIAS_UPDATE
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

typedef struct affection_s {
    float           intensities[MAX_BIAS_AFFECTED];
    int             numFound;
    biasaffection_t* affected;
} affection_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void evalPoint(gamemap_t* map, float light[4], vertexillum_t* illum,
                      biasaffection_t* affectedSources, const float* point,
                      const float* normal);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int useBias = false;
unsigned int currentTimeSB;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int useSightCheck = true;
static int doUpdateAffected = true;
static float biasIgnoreLimit = .005f;
static int lightSpeed = 130;

/**
 * BS_EvalPoint uses these, so they must be set before it is called.
 */
static biastracker_t trackChanged;
static biastracker_t trackApplied;

// CODE --------------------------------------------------------------------

/**
 * Register console variables for Shadow Bias.
 */
void SB_Register(void)
{
    C_VAR_INT("rend-bias", &useBias, 0, 0, 1);
    C_VAR_INT("rend-bias-lightspeed", &lightSpeed, 0, 0, 5000);

    // Development variables.
    C_VAR_INT("rend-dev-bias-sight", &useSightCheck, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_INT("rend-dev-bias-affected", &doUpdateAffected, CVF_NO_ARCHIVE, 0, 1);
}

static void linkSurfaceToList(biassurface_t** list, biassurface_t* bsuf)
{
    bsuf->next = (*list);
    (*list) = bsuf;
}

static void unlinkSurfaceFromList(biassurface_t** list, biassurface_t* bsuf)
{
    if((*list) == NULL)
        return;

    if(bsuf == *list)
    {
        *list = (*list)->next;
    }
    else if((*list)->next)
    {
        biassurface_t*      p = (*list)->next, *last = (*list);

        do
        {
            if(p == bsuf)
            {
                last->next = p->next;
                break;
            }

            last = p;
            p = p->next;
        } while(p);
    }
}

static __inline biassurface_t* allocBiasSurface(void)
{
    return Z_Calloc(sizeof(biassurface_t), PU_STATIC, 0);
}

static __inline void freeBiasSurface(biassurface_t* bsuf)
{
    Z_Free(bsuf);
}

static biassurface_t* createSurface(gamemap_t* map)
{
    biassurface_t*      bsuf = allocBiasSurface();

    // Link it to the map's surface list.
    linkSurfaceToList(&map->bias.surfaces, bsuf);
    return bsuf;
}

static void destroySurface(gamemap_t* map, biassurface_t* bsuf)
{
    // Unlink this surface from map's surface list.
    unlinkSurfaceFromList(&map->bias.surfaces, bsuf);

    freeBiasSurface(bsuf);
}

static void destroyAllSurfaces(gamemap_t* map)
{
    biassurface_t*      bsuf;

    while((bsuf = map->bias.surfaces))
    {
        destroySurface(map, bsuf);
    }
}

static int newSourceAt(gamemap_t* map, float x, float y, float z, float size,
                       float minLight, float maxLight, float* rgb)
{
    source_t*           src;

    if(map->bias.numSources == MAX_BIAS_LIGHTS)
        return -1;

    src = &map->bias.sources[map->bias.numSources++];

    // New lights are automatically locked.
    src->flags = BLF_CHANGED | BLF_LOCKED;

    src->pos[VX] = x;
    src->pos[VY] = y;
    src->pos[VZ] = z;

    SB_SourceSetColor(src, rgb);

    src->primaryIntensity = src->intensity = size;

    src->sectorLevel[0] = minLight;
    src->sectorLevel[1] = maxLight;

    // This'll enforce an update (although the vertices are also
    // STILL_UNSEEN).
    src->lastUpdateTime = 0;

    return map->bias.numSources; // == index + 1;
}

static void deleteSource(gamemap_t* map, int which)
{
    int                 i;

    // Do a memory move.
    for(i = which; i < map->bias.numSources; ++i)
        map->bias.sources[i].flags |= BLF_CHANGED;

    if(which < map->bias.numSources)
        memmove(&map->bias.sources[which], &map->bias.sources[which + 1],
                sizeof(source_t) * (map->bias.numSources - which - 1));

    map->bias.sources[map->bias.numSources - 1].intensity = 0;

    // Will be one fewer very soon.
    map->bias.numSourceDelta--;
}

static void clearSources(gamemap_t* map)
{
    while(map->bias.numSources-- > 0)
        map->bias.sources[map->bias.numSources].flags |= BLF_CHANGED;
    map->bias.numSources = 0;
}

static void addAffected(affection_t* aff, uint k, float intensity)
{
    uint                i, worst;

    if(aff->numFound < MAX_BIAS_AFFECTED)
    {
        aff->affected[aff->numFound].source = k;
        aff->intensities[aff->numFound] = intensity;
        aff->numFound++;
    }
    else
    {
        // Drop the weakest.
        worst = 0;
        for(i = 1; i < MAX_BIAS_AFFECTED; ++i)
        {
            if(aff->intensities[i] < aff->intensities[worst])
                worst = i;
        }

        aff->affected[worst].source = k;
        aff->intensities[worst] = intensity;
    }
}

/**
 * Sets/clears a bit in the tracker for the given index.
 */
static void trackerMark(biastracker_t* tracker, int index)
{
    if(index >= 0)
    {
        tracker->changes[index >> 5] |= (1 << (index & 0x1f));
    }
}

/**
 * Checks if the given index bit is set in the tracker.
 */
static int trackerCheck(biastracker_t* tracker, int index)
{
    return (tracker->changes[index >> 5] & (1 << (index & 0x1f))) != 0;
}

/**
 * Copies changes from src to dest.
 */
static void trackerApply(biastracker_t* dest, const biastracker_t* src)
{
    unsigned int        i;

    for(i = 0; i < sizeof(dest->changes)/sizeof(dest->changes[0]); ++i)
    {
        dest->changes[i] |= src->changes[i];
    }
}

/**
 * Clears changes of src from dest.
 */
static void trackerClear(biastracker_t* dest, const biastracker_t* src)
{
    unsigned int        i;

    for(i = 0; i < sizeof(dest->changes)/sizeof(dest->changes[0]); ++i)
    {
        dest->changes[i] &= ~src->changes[i];
    }
}

static void addLight(float dest[4], const float* color, float howMuch)
{
    int                 i;
    float               newval, amplified[3], largest = 0;

    if(color == NULL)
    {
        for(i = 0; i < 3; ++i)
        {
            amplified[i] = dest[i];
            if(i == 0 || amplified[i] > largest)
                largest = amplified[i];
        }

        if(largest == 0) // Black!
        {
            amplified[0] = amplified[1] = amplified[2] = 1;
        }
        else
        {
            for(i = 0; i < 3; ++i)
            {
                amplified[i] = amplified[i] / largest;
            }
        }
    }

    for(i = 0; i < 3; ++i)
    {
        newval = dest[i] + ((color ? color : amplified)[i] * howMuch);

        if(newval > 1)
            newval = 1;

        dest[i] = newval;
    }
}

/**
 * Add ambient light.
 */
static void applyAmbientLight(const float* point, float* light)
{
    // Add grid light (represents ambient lighting).
    float               color[3];

    LG_Evaluate(point, color);
    addLight(light, color, 1.0f);
}

/**
 * Interpolate between current and destination.
 */
static void lerpIllumination(vertexillum_t* illum, float* result)
{
    uint                i;

    if(!(illum->flags & VIF_LERP))
    {
        // We're done with the interpolation, just use the
        // destination color.
        result[CR] = illum->color[CR];
        result[CG] = illum->color[CG];
        result[CB] = illum->color[CB];
    }
    else
    {
        float               inter =
            (currentTimeSB - illum->updatetime) / (float)lightSpeed;

        if(inter > 1)
        {
            illum->flags &= ~VIF_LERP;

            illum->color[CR] = illum->dest[CR];
            illum->color[CG] = illum->dest[CG];
            illum->color[CB] = illum->dest[CB];

            result[CR] = illum->color[CR];
            result[CG] = illum->color[CG];
            result[CB] = illum->color[CB];
        }
        else
        {
            for(i = 0; i < 3; ++i)
            {
                result[i] = (illum->color[i] +
                     (illum->dest[i] - illum->color[i]) * inter);
            }
        }
    }
}

/**
 * @return              Light contributed by the specified source.
 */
static float* getCasted(vertexillum_t* illum, int sourceIndex,
                        biasaffection_t* affectedSources)
{
    int                 i, k;
    boolean             inUse;

    for(i = 0; i < MAX_BIAS_AFFECTED; ++i)
        if(illum->casted[i].source == sourceIndex)
            return illum->casted[i].color;

    // Choose an array element not used by the affectedSources.
    for(i = 0; i < MAX_BIAS_AFFECTED; ++i)
    {
        inUse = false;
        for(k = 0; k < MAX_BIAS_AFFECTED; ++k)
        {
            if(affectedSources[k].source < 0)
                break;

            if(affectedSources[k].source == illum->casted[i].source)
            {
                inUse = true;
                break;
            }
        }

        if(!inUse)
        {
            illum->casted[i].source = sourceIndex;
            illum->casted[i].color[CR] =
                illum->casted[i].color[CG] =
                    illum->casted[i].color[CB] = 0;

            return illum->casted[i].color;
        }
    }

    Con_Error("getCasted: No light casted by source %i.\n", sourceIndex);
    return NULL;
}

/**
 * Applies shadow bias to the given point.  If 'forced' is true, new
 * lighting is calculated regardless of whether the lights affecting the
 * point have changed.  This is needed when there has been world geometry
 * changes. 'illum' is allowed to be NULL.
 *
 * \fixme Only recalculate the changed lights.  The colors contributed
 * by the others can be saved with the 'affected' array.
 */
static void evalPoint(gamemap_t* map, float light[4], vertexillum_t* illum,
                      biasaffection_t* affectedSources, const float* point,
                      const float* normal)
{
#define COLOR_CHANGE_THRESHOLD  0.1f

    float               newColor[3];
    float               dot;
    vec3_t              delta, surfacePoint;
    float               distance;
    float               level;
    uint                i;
    int                 idx;
    boolean             illuminationChanged = false;
    unsigned int        latestSourceUpdate = 0;
    source_t*           s;
    float*              casted;
    struct {
        int              index;
        //uint           affNum; // Index in affectedSources.
        source_t*        source;
        biasaffection_t* affection;
        boolean          changed;
        boolean          overrider;
    } affecting[MAX_BIAS_AFFECTED + 1], *aff;

    // Vertices that are rendered for the first time need to be fully
    // evaluated.
    if(illum->flags & VIF_STILL_UNSEEN)
    {
        illuminationChanged = true;
        illum->flags &= ~VIF_STILL_UNSEEN;
    }

    // Determine if any of the affecting lights have changed since
    // last frame.
    aff = affecting;
    if(map->bias.numSources > 0)
    {
        for(i = 0;
            affectedSources[i].source >= 0 && i < MAX_BIAS_AFFECTED; ++i)
        {
            idx = affectedSources[i].source;

            // Is this a valid index?
            if(idx < 0 || idx >= map->bias.numSources)
                continue;

            aff->index = idx;
            //aff->affNum = i;
            aff->source = &map->bias.sources[idx];
            aff->affection = &affectedSources[i];
            aff->overrider = (aff->source->flags & BLF_COLOR_OVERRIDE) != 0;

            if(trackerCheck(&trackChanged, idx))
            {
                aff->changed = true;
                illuminationChanged = true;
                trackerMark(&trackApplied, idx);

                // Keep track of the earliest time when an affected source
                // was changed.
                if(latestSourceUpdate < map->bias.sources[idx].lastUpdateTime)
                {
                    latestSourceUpdate = map->bias.sources[idx].lastUpdateTime;
                }
            }
            else
            {
                aff->changed = false;
            }

            // Move to the next.
            aff++;
        }
    }
    aff->source = NULL;

    if(!illuminationChanged && illum != NULL)
    {
        // Reuse the previous value.
        lerpIllumination(illum, light);
        applyAmbientLight(point, light);
        return;
    }

    // Init to black.
    newColor[CR] = newColor[CG] = newColor[CB] = 0;

    // Calculate the contribution from each light.
    for(aff = affecting; aff->source; aff++)
    {
        if(illum && !aff->changed)
        {
            // We can reuse the previously calculated value.  This can
            // only be done if this particular light source hasn't
            // changed.
            continue;
        }

        s = aff->source;
        if(illum)
            casted = getCasted(illum, aff->index, affectedSources);
        else
            casted = NULL;

        V3_Subtract(delta, s->pos, point);
        V3_Copy(surfacePoint, delta);
        V3_Scale(surfacePoint, 1.f / 100);
        V3_Sum(surfacePoint, surfacePoint, point);

        if(useSightCheck &&
           !P_CheckLineSight(s->pos, surfacePoint, -1, 1,
                             LS_PASSOVER_SKY | LS_PASSUNDER_SKY))
        {
            // LOS fail.
            if(casted)
            {
                // This affecting source does not contribute any light.
                casted[CR] = casted[CG] = casted[CB] = 0;
            }

            continue;
        }
        else
        {
            distance = V3_Normalize(delta);
            dot = V3_DotProduct(delta, normal);

            // The surface faces away from the light.
            if(dot <= 0)
            {
                if(casted)
                {
                    casted[CR] = casted[CG] = casted[CB] = 0;
                }

                continue;
            }

            level = dot * s->intensity / distance;
        }

        if(level > 1)
            level = 1;

        for(i = 0; i < 3; ++i)
        {   // The light casted from this source.
            casted[i] =  s->color[i] * level;
        }
    }

    if(illum)
    {
        boolean             willOverride = false;

        // Combine the casted light from each source.
        for(aff = affecting; aff->source; aff++)
        {
            float*              casted =
                getCasted(illum, aff->index, affectedSources);

            if(aff->overrider &&
               (casted[CR] > 0 || casted[CG] > 0 || casted[CB] > 0))
                willOverride = true;

            for(i = 0; i < 3; ++i)
            {
                newColor[i] = MINMAX_OF(0, newColor[i] + casted[i], 1);
            }
        }

        // Is there a new destination?
        if(!(illum->dest[CR] < newColor[CR] + COLOR_CHANGE_THRESHOLD &&
             illum->dest[CR] > newColor[CR] - COLOR_CHANGE_THRESHOLD) ||
           !(illum->dest[CG] < newColor[CG] + COLOR_CHANGE_THRESHOLD &&
             illum->dest[CG] > newColor[CG] - COLOR_CHANGE_THRESHOLD) ||
           !(illum->dest[CB] < newColor[CB] + COLOR_CHANGE_THRESHOLD &&
             illum->dest[CB] > newColor[CB] - COLOR_CHANGE_THRESHOLD))
        {
            if(illum->flags & VIF_LERP)
            {
                // Must not lose the half-way interpolation.
                float           mid[3];

                lerpIllumination(illum, mid);

                // This is current color at this very moment.
                illum->color[CR] = mid[CR];
                illum->color[CG] = mid[CG];
                illum->color[CB] = mid[CB];
            }

            // This is what we will be interpolating to.
            illum->dest[CR] = newColor[CR];
            illum->dest[CG] = newColor[CG];
            illum->dest[CB] = newColor[CB];

            illum->flags |= VIF_LERP;
            illum->updatetime = latestSourceUpdate;
        }

        lerpIllumination(illum, light);
    }
    else
    {
        light[CR] = newColor[CR];
        light[CG] = newColor[CG];
        light[CB] = newColor[CB];
    }

    applyAmbientLight(point, light);

#undef COLOR_CHANGE_THRESHOLD
}

static float biasDot(source_t* src, const vectorcomp_t* point,
                     const vectorcomp_t* normal)
{
    float               delta[3];

    // Delta vector between source and given point.
    V3_Subtract(delta, src->pos, point);

    // Calculate the distance.
    V3_Normalize(delta);

    return V3_DotProduct(delta, normal);
}

#if 0
/**
 * Color override forces the bias light color to override biased
 * sectorlight.
 */
static boolean checkColorOverride(biasaffection_t *affected)
{
    int                 i;

    for(i = 0; affected[i].source >= 0 && i < MAX_BIAS_AFFECTED; ++i)
    {
        // If the color is completely black, it means no light was
        // reached from this affected source.
        if(!(affected[i].rgb[0] | affected[i].rgb[1] | affected[i].rgb[2]))
            continue;

        if(sources[affected[i].source].flags & BLF_COLOR_OVERRIDE)
            return true;
    }

    return false;
}
#endif

static void updateAffected(gamemap_t* map, biassurface_t* bsuf,
                           const fvertex_t* from, const fvertex_t* to,
                           const vectorcomp_t* normal)
{
    int                 i, k;
    vec2_t              delta;
    source_t*           src;
    float               distance = 0, len;
    float               intensity;
    affection_t         aff;

    // If the data is already up to date, nothing needs to be done.
    if(bsuf->updated == map->bias.lastChangeOnFrame)
        return;

    bsuf->updated = map->bias.lastChangeOnFrame;
    aff.affected = bsuf->affected;
    aff.numFound = 0;
    memset(aff.affected, -1, sizeof(bsuf->affected));

    for(k = 0, src = map->bias.sources; k < map->bias.numSources; ++k, ++src)
    {
        if(src->intensity <= 0)
            continue;

        // Calculate minimum 2D distance to the seg.
        for(i = 0; i < 2; ++i)
        {
            if(!i)
                V2_Set(delta, from->pos[VX] - src->pos[VX],
                       from->pos[VY] - src->pos[VY]);
            else
                V2_Set(delta, to->pos[VX] - src->pos[VX],
                       to->pos[VY] - src->pos[VY]);
            len = V2_Normalize(delta);

            if(i == 0 || len < distance)
                distance = len;
        }

        if(M_DotProduct(delta, normal) >= 0)
            continue;

        if(distance < 1)
            distance = 1;

        intensity = src->intensity / distance;

        // Is the source is too weak, ignore it entirely.
        if(intensity < biasIgnoreLimit)
            continue;

        addAffected(&aff, k, intensity);
    }
}

static void updateAffected2(gamemap_t* map, biassurface_t* bsuf,
                            const struct rvertex_s* rvertices,
                            size_t numVertices, const vectorcomp_t* point,
                            const vectorcomp_t* normal)
{
    int                 i;
    uint                k;
    vec2_t              delta;
    source_t*           src;
    float               distance = 0, len, dot;
    float               intensity;
    affection_t         aff;

    // If the data is already up to date, nothing needs to be done.
    if(bsuf->updated == map->bias.lastChangeOnFrame)
        return;

    bsuf->updated = map->bias.lastChangeOnFrame;
    aff.affected = bsuf->affected;
    aff.numFound = 0;
    memset(aff.affected, -1, sizeof(aff.affected));

    for(i = 0, src = map->bias.sources; i < map->bias.numSources; ++i, ++src)
    {
        if(src->intensity <= 0)
            continue;

        // Calculate minimum 2D distance to the subSector.
        // \fixme This is probably too accurate an estimate.
        for(k = 0; k < bsuf->size; ++k)
        {
            V2_Set(delta,
                   rvertices[k].pos[VX] - src->pos[VX],
                   rvertices[k].pos[VY] - src->pos[VY]);
            len = V2_Length(delta);

            if(k == 0 || len < distance)
                distance = len;
        }
        if(distance < 1)
            distance = 1;

        // Estimate the effect on this surface.
        dot = biasDot(src, point, normal);
        if(dot <= 0)
            continue;

        intensity = /*dot * */ src->intensity / distance;

        // Is the source is too weak, ignore it entirely.
        if(intensity < biasIgnoreLimit)
            continue;

        addAffected(&aff, i, intensity);
    }
}

/**
 * Tests against trackChanged.
 */
static boolean changeInAffected(biasaffection_t* affected,
                                biastracker_t* changed)
{
    uint                i;

    for(i = 0; i < MAX_BIAS_AFFECTED; ++i)
    {
        if(affected[i].source < 0)
            break;

        if(trackerCheck(changed, affected[i].source))
            return true;
    }
    return false;
}

static void markSurfaceSourcesChanged(gamemap_t* map, biassurface_t* bsuf)
{
    int                 i;

    for(i = 0; i < MAX_BIAS_AFFECTED && bsuf->affected[i].source >= 0; ++i)
    {
        map->bias.sources[bsuf->affected[i].source].flags |= BLF_CHANGED;
    }
}

static void newSourcesFromLightDefs(gamemap_t* map)
{
    int                 i;
    ded_light_t*        def;
    const char*         uniqueID = P_GetUniqueMapID(map);

    // Check all the loaded Light definitions for any matches.
    for(i = 0; i < defs.count.lights.num; ++i)
    {
        def = &defs.lights[i];

        if(def->state[0] || stricmp(uniqueID, def->uniqueMapID))
            continue;

        if(newSourceAt(map, def->offset[VX], def->offset[VY], def->offset[VZ],
                       def->size, def->lightLevel[0], def->lightLevel[1],
                       def->color) == -1)
            break;
    }
}

static void init(gamemap_t* map)
{
    destroyAllSurfaces(map);

    // Start with no sources whatsoever.
    map->bias.numSources = 0;
    map->bias.numSourceDelta = 0;
    map->bias.lastChangeOnFrame = 0;

    newSourcesFromLightDefs(map);
}

/**
 * Initializes the bias lights according to the loaded Light
 * definitions.
 */
void SB_InitForMap(gamemap_t* map)
{
    uint                startTime;

    if(!map)
        return;

    startTime = Sys_GetRealTime();

    init(map);

    // How much time did we spend?
    VERBOSE(Con_Message("SB_InitForMap: Done in %.2f seconds.\n",
                        (Sys_GetRealTime() - startTime) / 1000.0f));
}

void SB_DestroySurfaces(gamemap_t* map)
{
    if(!map)
        return;

    destroyAllSurfaces(map);
}

/**
 * Do initial processing that needs to be done before rendering a frame.
 * Changed sources cause the tracker bits to the set for all bias surfaces.
 */
void SB_BeginFrame(gamemap_t* map)
{
    int                 l;
    source_t*           s;
    biastracker_t       allChanges;

#ifdef DD_PROFILE
    static int          i;

    if(++i > 40)
    {
        i = 0;
        PRINT_PROF( PROF_BIAS_UPDATE );
    }
#endif

    if(!map)
        return; 

    if(!useBias)
        return;

BEGIN_PROF( PROF_BIAS_UPDATE );

    // The time that applies on this frame.
    currentTimeSB = Sys_GetRealTime();

    // Check which sources have changed.
    memset(&allChanges, 0, sizeof(allChanges));
    for(l = 0; l < map->bias.numSources; ++l)
    {
        s = &map->bias.sources[l];

        if(s->sectorLevel[1] > 0 || s->sectorLevel[0] > 0)
        {
            float               minLevel = s->sectorLevel[0];
            float               maxLevel = s->sectorLevel[1];
            subsector_t*        subSector = (subsector_t*)
                R_PointInSubSector(s->pos[VX], s->pos[VY])->data;
            sector_t*           sector;
            float               oldIntensity = s->intensity;

            sector = subSector->sector;

            // The lower intensities are useless for light emission.
            if(sector->lightLevel >= maxLevel)
            {
                s->intensity = s->primaryIntensity;
            }

            if(sector->lightLevel >= minLevel && minLevel != maxLevel)
            {
                s->intensity = s->primaryIntensity *
                    (sector->lightLevel - minLevel) / (maxLevel - minLevel);
            }
            else
            {
                s->intensity = 0;
            }

            if(s->intensity != oldIntensity)
                s->flags |= BLF_CHANGED;
        }

        if(s->flags & BLF_CHANGED)
        {
            trackerMark(&allChanges, l);
            s->flags &= ~BLF_CHANGED;

            // This is used for interpolation.
            s->lastUpdateTime = currentTimeSB;

            // Recalculate which sources affect which surfaces.
            map->bias.lastChangeOnFrame = frameCount;
        }
    }

    // Apply to all surfaces.
    {
    biassurface_t*      bsuf;

    for(bsuf = map->bias.surfaces; bsuf; bsuf = bsuf->next)
    {
        trackerApply(&bsuf->tracker, &allChanges);

        // Everything that is affected by the changed lights will need an
        // update.
        if(changeInAffected(bsuf->affected, &allChanges))
        {
            uint                i;

            // Mark the illumination unseen to force an update.
            for(i = 0; i < bsuf->size; ++i)
                bsuf->illum[i].flags |= VIF_STILL_UNSEEN;
        }
    }
    }

END_PROF( PROF_BIAS_UPDATE );
}

void SB_EndFrame(gamemap_t* map)
{
    if(!map)
        return;
    
    if(map->bias.numSourceDelta != 0)
    {
        map->bias.numSources += map->bias.numSourceDelta;
        map->bias.numSourceDelta = 0;
    }
}

biassurface_t* SB_CreateSurface(gamemap_t* map)
{
    if(!map)
        return NULL;

    return createSurface(map);
}

void SB_DestroySurface(gamemap_t* map, biassurface_t* bsuf)
{
    if(!map || !bsuf)
        return;

    destroySurface(map, bsuf);
}

void SB_InitVertexIllum(vertexillum_t* villum)
{
    int                 i;

    villum->flags |= VIF_STILL_UNSEEN;

    for(i = 0; i < MAX_BIAS_AFFECTED; ++i)
        villum->casted[i].source = -1;
}

void SB_SurfaceInit(biassurface_t* bsuf)
{
    uint                i;

    for(i = 0; i < bsuf->size; ++i)
    {
        SB_InitVertexIllum(&bsuf->illum[i]);
    }
}

void SB_SurfaceMoved(gamemap_t* map, biassurface_t* bsuf)
{
    if(!map || !bsuf)
        return;

    markSurfaceSourcesChanged(map, bsuf);
}

/**
 * @return              Ptr to the bias light source for the given id.
 */
source_t* SB_GetSource(gamemap_t* map, int which)
{
    if(!map || which < 0)
        return NULL;

    return &map->bias.sources[which];
}

/**
 * Convert bias light source ptr to index.
 *
 * @param source        Must be linked to the specified map!
 */
int SB_ToIndex(gamemap_t* map, source_t* source)
{
    if(!map || !source)
        return -1;

    return (source - map->bias.sources);
}

/**
 * Creates a new bias light source and sets the appropriate properties to
 * the values of the passed parameters. The id of the new light source is
 * returned unless there are no free sources available.
 *
 * @param map           The map the new source is to be placed in.
 * @param x             X coordinate of the new light.
 * @param y             Y coordinate of the new light.
 * @param z             Z coordinate of the new light.
 * @param size          Size (strength) of the new light.
 * @param minLight      Lower sector light level limit.
 * @param maxLight      Upper sector light level limit.
 * @param rgb           Ptr to float[3], the color for the new light.
 *
 * @return              The id of the newly created bias light source else -1.
 */
int SB_NewSourceAt(gamemap_t* map, float x, float y, float z, float size,
                   float minLight, float maxLight, float* rgb)
{
    if(!map)
        return 0;

    return newSourceAt(map, x, y, z, size, minLight, maxLight, rgb);
}

/**
 * Same as above really but for updating an existing source.
 */
void SB_SourceUpdate(source_t* src, float x, float y, float z,
                     float size, float minLight, float maxLight, float* rgb)
{
    if(!src)
        return;

    // Position change?
    src->pos[VX] = x;
    src->pos[VY] = y;
    src->pos[VZ] = z;

    if(rgb)
        SB_SourceSetColor(src, rgb);

    src->primaryIntensity = src->intensity = size;

    src->sectorLevel[0] = minLight;
    src->sectorLevel[1] = maxLight;
}

void SB_SourceSetColor(source_t* src, float* rgb)
{
    int                 i;
    float               largest = 0;

    if(!src || !rgb)
        return;

    // Amplify the color.
    for(i = 0; i < 3; ++i)
    {
        src->color[i] = rgb[i];
        if(largest < src->color[i])
            largest = src->color[i];
    }

    if(largest > 0)
    {
        for(i = 0; i < 3; ++i)
            src->color[i] /= largest;
    }
    else
    {
        // Replace black with white.
        src->color[0] = src->color[1] = src->color[2] = 1;
    }
}

/**
 * Removes the specified bias light source from the map.
 *
 * @param which         The id of the bias light source to be deleted.
 */
void SB_DeleteSource(gamemap_t* map, int which)
{
    if(!map || which < 0 || which >= map->bias.numSources)
        return; // Very odd...

    deleteSource(map, which);
}

/**
 * Removes ALL bias light sources on the map.
 */
void SB_ClearSources(gamemap_t* map)
{
    if(!map)
        return;

    clearSources(map);
}

/**
 * Surface can be a either a wall or a plane (ceiling or a floor).
 *
 * @param colors        Array of colors to be written to.
 * @param bsuf          Bias data for this surface.
 * @param vertices      Array of vertices to be lit.
 * @param numVertices   Number of vertices (in the array) to be lit.
 * @param normal        Surface normal.
 * @param sectorLightLevel Sector light level.
 */
void SB_RendSeg(gamemap_t* map, rcolor_t* rcolors, biassurface_t* bsuf,
                const rvertex_t* rvertices, size_t numVertices,
                const vectorcomp_t* normal, float sectorLightLevel,
                const fvertex_t* from, const fvertex_t* to)
{
    uint                i;
    boolean             forced;

    memcpy(&trackChanged, &bsuf->tracker, sizeof(trackChanged));
    memset(&trackApplied, 0, sizeof(trackApplied));

    // Has any of the old affected lights changed?
    forced = false;

    if(doUpdateAffected)
    {
        /**
         * \todo This could be enhanced so that only the lights on the
         * right side of the surface are taken into consideration.
         */
        updateAffected(map, bsuf, from, to, normal);
    }

    for(i = 0; i < numVertices; ++i)
        evalPoint(map, rcolors[i].rgba, &bsuf->illum[i], bsuf->affected,
                  rvertices[i].pos, normal);

    trackerClear(&bsuf->tracker, &trackApplied);
}

/**
 * Surface can be a either a wall or a plane (ceiling or a floor).
 *
 * @param colors        Array of colors to be written to.
 * @param bsuf          Bias data for this surface.
 * @param vertices      Array of vertices to be lit.
 * @param numVertices   Number of vertices (in the array) to be lit.
 * @param normal        Surface normal.
 * @param sectorLightLevel Sector light level.
 * @param face          Ptr to a face.
 * @param plane         Plane number.
 */
void SB_RendPlane(gamemap_t* map, rcolor_t* rcolors, biassurface_t* bsuf,
                  const rvertex_t* rvertices, size_t numVertices,
                  const vectorcomp_t* normal, float sectorLightLevel,
                  face_t* face, uint plane)
{
    uint                i;
    boolean             forced;

    memcpy(&trackChanged, &bsuf->tracker, sizeof(trackChanged));
    memset(&trackApplied, 0, sizeof(trackApplied));

    // Has any of the old affected lights changed?
    forced = false;

    if(doUpdateAffected)
    {
        /**
         * \todo This could be enhanced so that only the lights on the
         * right side of the surface are taken into consideration.
         */

        const subsector_t*  subSector = (subsector_t*) face->data;
        vec3_t              point;

        V3_Set(point, subSector->midPoint.pos[VX], subSector->midPoint.pos[VY],
               subSector->sector->planes[plane]->height);

        updateAffected2(map, bsuf, rvertices, numVertices, point, normal);
    }

    for(i = 0; i < numVertices; ++i)
        evalPoint(map, rcolors[i].rgba, &bsuf->illum[i], bsuf->affected,
                  rvertices[i].pos, normal);

    trackerClear(&bsuf->tracker, &trackApplied);
}
