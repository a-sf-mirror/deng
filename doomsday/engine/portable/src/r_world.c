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
 * r_world.c: World Setup and Refresh
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <assert.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int rendSkyLight = 1; // cvar.
float rendLightWallAngle = 1; // Intensity of angle-based wall lighting.
float rendMaterialFadeSeconds = .6f;

// Ambient lighting, rAmbient is used within the renderer, ambientLight is
// used to store the value of the ambient light cvar.
// The value chosen for rAmbient occurs in R_CalcLightModRange
// for convenience (since we would have to recalculate the matrix anyway).
int rAmbient = 0, ambientLight = 0;

float lightRangeCompression = 0;
float lightModRange[255];
float rendLightDistanceAttentuation = 1024;

boolean firstFrameAfterLoad;
boolean ddMapSetup;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static surfacelistnode_t* unusedSurfaceListNodes = NULL;
static planelistnode_t* unusedPlaneListNodes = NULL;

// CODE --------------------------------------------------------------------

/**
 * Allocate a new surface list node.
 */
static surfacelistnode_t* allocSurfaceListNode(void)
{
    surfacelistnode_t* node = Z_Calloc(sizeof(*node), PU_STATIC, 0);
    return node;
}

/**
 * Free all memory acquired for the given surface list node.
 */
static void freeSurfaceListNode(surfacelistnode_t* node)
{
    if(node)
        Z_Free(node);
}

static surfacelistnode_t* surfaceListNodeCreate(void)
{
    surfacelistnode_t* node;

    // Is there a free node in the unused list?
    if(unusedSurfaceListNodes)
    {
        node = unusedSurfaceListNodes;
        unusedSurfaceListNodes = node->next;
    }
    else
    {
        node = allocSurfaceListNode();
    }

    node->data = NULL;
    node->next = NULL;

    return node;
}

static void surfaceListNodeDestroy(surfacelistnode_t* node)
{
    // Move it to the list of unused nodes.
    node->data = NULL;
    node->next = unusedSurfaceListNodes;
    unusedSurfaceListNodes = node;
}

/**
 * Adds the surface to the given surface list.
 *
 * @param sl            The surface list to add the surface to.
 * @param suf           The surface to add to the list.
 */
void SurfaceList_Add(surfacelist_t* sl, surface_t* suf)
{
    surfacelistnode_t*  node;

    if(!sl || !suf)
        return;

    // Check whether this surface is already in the list.
    node = sl->head;
    while(node)
    {
        if((surface_t*) node->data == suf)
            return; // Yep.
        node = node->next;
    }

    // Not found, add it to the list.
    node = surfaceListNodeCreate();
    node->data = suf;
    node->next = sl->head;

    sl->head = node;
    sl->num++;
}

boolean SurfaceList_Remove(surfacelist_t* sl, const surface_t* suf)
{
    surfacelistnode_t* last, *n;

    if(!sl || !suf)
        return false;

    last = sl->head;
    if(last)
    {
        n = last->next;
        while(n)
        {
            if((surface_t*) n->data == suf)
            {
                last->next = n->next;
                surfaceListNodeDestroy(n);
                sl->num--;
                return true;
            }

            last = n;
            n = n->next;
        }
    }

    return false;
}

void SurfaceList_Empty(surfacelist_t* sl)
{
    surfacelistnode_t* next;

    if(!sl)
        return;

    while(sl->head)
    {
        next = sl->head->next;
        surfaceListNodeDestroy(sl->head);
        sl->head = next;
    }

    sl->head = NULL;
    sl->num = 0;
}

/**
 * Iterate the list of surfaces making a callback for each.
 *
 * @param sl            The surface list to iterate.
 * @param callback      The callback to make. Iteration will continue
 *                      until a callback returns a zero value.
 * @param context       Is passed to the callback function.
 */
boolean SurfaceList_Iterate(surfacelist_t* sl,
                            boolean (*callback) (surface_t* suf, void*),
                            void* context)
{
    boolean result = true;
    surfacelistnode_t* n, *np;

    if(sl)
    {
        n = sl->head;
        while(n)
        {
            np = n->next;
            if((result = callback((surface_t*) n->data, context)) == 0)
                break;
            n = np;
        }
    }

    return result;
}

/**
 * @param fader         Material fader to be stopped.
 */
void R_StopMatFader(matfader_t* fader)
{
    if(fader)
    {
        surface_t* suf = fader->suf;

        fader->suf->materialB = NULL;
        fader->suf->matBlendFactor = 1;

        // @todo thinker should return the map it's linked to.
        Map_RemoveThinker(P_CurrentMap(), (thinker_t*) fader);
    }
}

/**
 * Thinker function. Updates the surface, blend material and blend factor to
 * animate smoothly (blend) between material changes.
 *
 * @param fader         Material fader.
 */
void R_MatFaderThinker(matfader_t* fader)
{
    if(fader)
    {
        int maxTics = rendMaterialFadeSeconds * TICSPERSEC;

        fader->tics++;
        if(!(fader->tics < maxTics))
        {
            R_StopMatFader(fader);
            return;
        }

        fader->suf->matBlendFactor = 1 - ((float) fader->tics / maxTics);
    }
}

/**
 * Spawn multiple new particles using all applicable sources.
 */
int RIT_StopMatFader(void* p, void* context)
{
    matfader_t* fader = (matfader_t*) p;

    // Surface match?
    if(fader->suf == (surface_t*) context)
    {
        R_StopMatFader(fader);
        return false; // Stop iteration.
    }

    return true; // Continue iteration.
}

/**
 * Allocate a new surface list node.
 */
static planelistnode_t* allocPlaneListNode(void)
{
    planelistnode_t* node = Z_Calloc(sizeof(*node), PU_STATIC, 0);
    return node;
}

/**
 * Free all memory acquired for the given surface list node.
 */
static void freePlaneListNode(planelistnode_t* node)
{
    if(node)
        Z_Free(node);
}

static planelistnode_t* planeListNodeCreate(void)
{
    planelistnode_t* node;

    // Is there a free node in the unused list?
    if(unusedPlaneListNodes)
    {
        node = unusedPlaneListNodes;
        unusedPlaneListNodes = node->next;
    }
    else
    {
        node = allocPlaneListNode();
    }

    node->data = NULL;
    node->next = NULL;

    return node;
}

static void planeListNodeDestroy(planelistnode_t* node)
{
    // Move it to the list of unused nodes.
    node->data = NULL;
    node->next = unusedPlaneListNodes;
    unusedPlaneListNodes = node;
}

void PlaneList_Add(planelist_t* pl, plane_t* pln)
{
    planelistnode_t*  node;

    if(!pl || !pln)
        return;

    // Check whether this surface is already in the list.
    node = pl->head;
    while(node)
    {
        if((plane_t*) node->data == pln)
            return; // Yep.
        node = node->next;
    }

    // Not found, add it to the list.
    node = planeListNodeCreate();
    node->data = pln;
    node->next = pl->head;

    pl->head = node;
    pl->num++;
}

boolean PlaneList_Remove(planelist_t* pl, const plane_t* pln)
{
    planelistnode_t* last, *n;

    if(!pl || !pln)
        return false;

    last = pl->head;
    if(last)
    {
        n = last->next;
        while(n)
        {
            if((plane_t*) n->data == pln)
            {
                last->next = n->next;
                planeListNodeDestroy(n);
                pl->num--;
                return true;
            }

            last = n;
            n = n->next;
        }
    }

    return false;
}

void PlaneList_Empty(planelist_t* pl)
{
    planelistnode_t* next;

    if(!pl)
        return;

    while(pl->head)
    {
        next = pl->head->next;
        planeListNodeDestroy(pl->head);
        pl->head = next;
    }

    pl->head = NULL;
    pl->num = 0;
}

boolean PlaneList_Iterate(planelist_t* pl, boolean (*callback) (plane_t*, void*), void* context)
{
    boolean result = true;
    planelistnode_t* n, *np;

    if(pl)
    {
        n = pl->head;
        while(n)
        {
            np = n->next;
            if((result = callback((plane_t*) n->data, context)) == 0)
                break;
            n = np;
        }
    }

    return result;
}

/**
 * Called when a floor or ceiling height changes to update the plotted
 * decoration origins for surfaces whose material offset is dependant upon
 * the given plane.
 */
void R_MarkDependantSurfacesForDecorationUpdate(sector_t* sector)
{
    linedef_t** linep;

    if(!sector || !sector->lineDefs)
        return;

    // Mark the decor lights on the sides of this plane as requiring
    // an update.
    linep = sector->lineDefs;

    while(*linep)
    {
        linedef_t* li = *linep;

        if(!LINE_BACKSIDE(li))
        {
            Surface_Update(&LINE_FRONTSIDE(li)->SW_surface(SEG_MIDDLE));
        }
        else if(LINE_BACKSECTOR(li) != LINE_FRONTSECTOR(li))
        {
            Surface_Update(&LINE_FRONTSIDE(li)->SW_surface(SEG_BOTTOM));
            Surface_Update(&LINE_FRONTSIDE(li)->SW_surface(SEG_TOP));
            Surface_Update(&LINE_BACKSIDE(li)->SW_surface(SEG_BOTTOM));
            Surface_Update(&LINE_BACKSIDE(li)->SW_surface(SEG_TOP));
        }

        *linep++;
    }
}

void R_CreateBiasSurfacesForPlanesInSubsector(subsector_t* subsector)
{
    uint i;
    map_t* map;

    if(!subsector->sector)
        return;

    map = P_CurrentMap();

    subsector->bsuf = Z_Calloc(subsector->sector->planeCount * sizeof(biassurface_t*),
                          PU_STATIC, NULL);

    for(i = 0; i < subsector->sector->planeCount; ++i)
    {
        biassurface_t* bsuf = SB_CreateSurface(map);
        uint j;

        bsuf->size = subsector->hEdgeCount + (subsector->useMidPoint? 2 : 0);
        bsuf->illum = Z_Calloc(sizeof(vertexillum_t) * bsuf->size, PU_STATIC, 0);

        for(j = 0; j < bsuf->size; ++j)
            SB_InitVertexIllum(&bsuf->illum[j]);

        subsector->bsuf[i] = bsuf;
    }
}

void R_DestroyBiasSurfacesForPlanesInSubSector(subsector_t* subsector)
{
    if(subsector->sector && subsector->bsuf)
    {
        uint i;
        map_t* map = P_CurrentMap();

        for(i = 0; i < subsector->sector->planeCount; ++i)
        {
            biassurface_t* bsuf = subsector->bsuf[i];

            if(bsuf->illum)
                Z_Free(bsuf->illum);

            SB_DestroySurface(map, bsuf);
        }

        Z_Free(subsector->bsuf);
    }

    subsector->bsuf = NULL;
}

/**
 * Allocate bias surfaces and attach vertexillums for all renderable surfaces
 * in the specified subsector. If already present, this is a null-op.
 */
void R_CreateBiasSurfacesInSubsector(subsector_t* subsector)
{
    map_t* map;

    if(!subsector->sector)
        return;

    map = P_CurrentMap();

    {
    hedge_t* hEdge;
    if((hEdge = subsector->face->hEdge))
    {
        do
        {
            seg_t* seg = (seg_t*) hEdge->data;

            if(seg->sideDef) // "minisegs" have no sidedefs (or linedefs).
            {
                if(!seg->bsuf[0])
                {
                    uint i;

                    for(i = 0; i < 3; ++i)
                    {
                        uint j;
                        biassurface_t* bsuf = SB_CreateSurface(map);

                        bsuf->size = 4;
                        bsuf->illum = Z_Calloc(sizeof(vertexillum_t) * bsuf->size,
                            PU_STATIC, 0);
                        for(j = 0; j < bsuf->size; ++j)
                            SB_InitVertexIllum(&bsuf->illum[j]);

                        seg->bsuf[i] = bsuf;
                    }
                }
            }
        } while((hEdge = hEdge->next) != subsector->face->hEdge);
    }
    }

    if(!subsector->bsuf)
        R_CreateBiasSurfacesForPlanesInSubsector(subsector);
}

/**
 * Permanently destroys the specified plane of the given sector.
 * The sector's plane list is updated accordingly.
 *
 * @param id            The sector, plane id to be destroyed.
 * @param sec           Ptr to sector for which a plane will be destroyed.
 */
void R_DestroyPlaneOfSector(map_t* map, uint id, sector_t* sec)
{
    uint i;
    plane_t* plane, **newList = NULL;

    if(!map)
        return;
    if(!sec)
        return; // Do wha?

    if(id >= sec->planeCount)
        Con_Error("P_DestroyPlaneOfSector: Plane id #%i is not valid for "
                  "sector #%u", id, (uint) P_ObjectRecord(DMU_SECTOR, sec)->id);

    plane = sec->planes[id];

    // Create a new plane list?
    if(sec->planeCount > 1)
    {
        uint n;

        newList = Z_Malloc(sizeof(plane_t**) * sec->planeCount, PU_STATIC, 0);

        // Copy ptrs to the planes.
        n = 0;
        for(i = 0; i < sec->planeCount; ++i)
        {
            if(i == id)
                continue;
            newList[n++] = sec->planes[i];
        }
        newList[n] = NULL; // Terminate.
    }

    // If this plane is currently being watched, remove it.
    PlaneList_Remove(&map->watchedPlaneList, plane);

    // If this plane's surface is in the moving list, remove it.
    SurfaceList_Remove(&map->movingSurfaceList, &plane->surface);
    // If this plane's surface is in the deocrated list, remove it.
    SurfaceList_Remove(&map->decoratedSurfaceList, &plane->surface);
    if(plane->surface.decorations)
        Z_Free(plane->surface.decorations);

    // Stop active material fade on this surface.
    Map_IterateThinkers(map, R_MatFaderThinker, ITF_PRIVATE, // Always non-public
                        RIT_StopMatFader, &plane->surface);

    /**
     * Destroy biassurfaces for planes in all sector subsectors if present
     * (they will be created if needed when next drawn).
     */
    {
    subsector_t** subsectorPtr = sec->subsectors;
    while(*subsectorPtr)
    {
        R_DestroyBiasSurfacesForPlanesInSubSector(*subsectorPtr);
        *subsectorPtr++;
    }
    }

    // Destroy the specified plane.
    Z_Free(plane);
    sec->planeCount--;

    // Link the new list to the sector.
    Z_Free(sec->planes);
    sec->planes = newList;
}

surfacedecor_t* R_CreateSurfaceDecoration(decortype_t type, surface_t* suf)
{
    uint i;
    surfacedecor_t* d, *s, *decorations;

    if(!suf)
        return NULL;

    decorations = Z_Malloc(sizeof(*decorations) * (++suf->numDecorations), PU_STATIC, 0);

    if(suf->numDecorations > 1)
    {   // Copy the existing decorations.
        for(i = 0; i < suf->numDecorations - 1; ++i)
        {
            d = &decorations[i];
            s = &suf->decorations[i];

            memcpy(d, s, sizeof(*d));
        }

        Z_Free(suf->decorations);
    }

    // Add the new decoration.
    d = &decorations[suf->numDecorations - 1];
    d->type = type;

    suf->decorations = decorations;

    return d;
}

void R_ClearSurfaceDecorations(surface_t* suf)
{
    if(!suf)
        return;

    if(suf->decorations)
        Z_Free(suf->decorations);
    suf->decorations = NULL;
    suf->numDecorations = 0;
}

/**
 * @return              Ptr to the lineowner for this line for this vertex
 *                      else @c NULL.
 */
lineowner_t* R_GetVtxLineOwner(const vertex_t* v, const linedef_t* line)
{
    if(v == line->L_v1)
        return line->L_vo1;

    if(v == line->L_v2)
        return line->L_vo2;

    return NULL;
}

void R_SetupFog(float start, float end, float density, float* rgb)
{
    Con_Execute(CMDS_DDAY, "fog on", true, false);
    Con_Executef(CMDS_DDAY, true, "fog start %f", start);
    Con_Executef(CMDS_DDAY, true, "fog end %f", end);
    Con_Executef(CMDS_DDAY, true, "fog density %f", density);
    Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                 rgb[0] * 255, rgb[1] * 255, rgb[2] * 255);
}

void R_SetupFogDefaults(void)
{
    // Go with the defaults.
    Con_Execute(CMDS_DDAY,"fog off", true, false);
}

/**
 * Returns pointers to the line's vertices in such a fashion that verts[0]
 * is the leftmost vertex and verts[1] is the rightmost vertex, when the
 * line lies at the edge of `sector.'
 */
void R_OrderVertices(const linedef_t* line, const sector_t* sector,
                     vertex_t* verts[2])
{
    byte edge;

    edge = (sector == LINE_FRONTSECTOR(line)? 0:1);
    verts[0] = LINE_VERTEX(line, edge);
    verts[1] = LINE_VERTEX(line, edge^1);
}

static int C_DECL DivSortAscend(const void* e1, const void* e2)
{
    float f1 = (*((const plane_t**) e1))->visHeight;
    float f2 = (*((const plane_t**) e2))->visHeight;

    if(f1 > f2)
        return 1;
    if(f2 > f1)
        return -1;
    return 0;
}

static int C_DECL DivSortDescend(const void* e1, const void* e2)
{
    float f1 = (*((const plane_t**) e1))->visHeight;
    float f2 = (*((const plane_t**) e2))->visHeight;

    if(f1 > f2)
        return -1;
    if(f2 > f1)
        return 1;
    return 0;
}

static boolean testForPlaneDivision(walldiv_t* wdiv, plane_t* pln,
                                    float bottomZ, float topZ)
{
    if(pln->visHeight > bottomZ && pln->visHeight < topZ)
    {
        uint i;

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

static hedge_t* nextHEdgeAroundVertex(hedge_t* hEdge, boolean clockwise)
{
    if(clockwise)
        return hEdge->prev->twin;
    return hEdge->next->twin;
}

static void doFindSegDivisions(walldiv_t* div, hedge_t* base,
                               const sector_t* frontSec, float bottomZ,
                               float topZ, boolean doRight)
{
    hedge_t* hEdge;
    boolean clockwise = !doRight;

    if(bottomZ >= topZ)
        return; // Obviously no division.

    /**
     * We need to handle the special case of a sector with zero volume.
     * In this instance, the only potential divisor in the sector is the back
     * ceiling. This is because elsewhere we automatically fix the case of a
     * floor above a ceiling by lowering the floor.
     */
    hEdge = base;
    while((hEdge = nextHEdgeAroundVertex(hEdge, clockwise)) != base)
    {
        seg_t* seg = (seg_t*) hEdge->data;
        plane_t* pln;
        sector_t* scanSec;
        boolean sectorHasVolume;

        if(!seg)
            break;
        if(!seg->sideDef || LINE_SELFREF(seg->sideDef->lineDef))
            continue;

        scanSec = seg->sideDef->sector;
        sectorHasVolume = (scanSec->SP_ceilvisheight - scanSec->SP_floorvisheight > 0);

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
                uint j;
                boolean stop = false;

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

        // Stop the scan when a solid neighbour is reached.
        if(!sectorHasVolume)
            break;

        // Prepare for the next round.
        frontSec = seg->sideDef->sector;
    }
}

static void findSegDivisions(walldiv_t* div, hedge_t* hEdge,
                             const sector_t* frontSec, float bottomZ,
                             float topZ, boolean doRight)
{
    seg_t* seg = (seg_t*) hEdge->data;
    sidedef_t* side = seg->sideDef;
    hedge_t* other;

    div->num = 0;

    if(!side)
        return;

    // Only segs at sidedef ends can/should be split.
    other = side->lineDef->hEdges[seg->side^doRight? 1 : 0];
    if(seg->side)
        other = other->twin;
    if(hEdge != other)
        return;

    doFindSegDivisions(div, hEdge, frontSec, bottomZ, topZ, doRight);
}

/**
 * Division will only happen if it must be done.
 */
void R_FindSegSectionDivisions(walldiv_t* wdivs, hedge_t* hEdge,
                               const sector_t* frontsec, float low, float hi)
{
    uint i;
    walldiv_t* wdiv;

    if(!((seg_t*) hEdge->data)->sideDef)
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
uint k;
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
 * \fixme No need to do this each frame. Set a flag in sidedef_t->flags to
 * denote this. Is sensitive to plane heights, surface properties
 * (e.g. alpha) and surface texture properties.
 */
boolean R_DoesMiddleMaterialFillGap(linedef_t* line, int backside)
{
    // Check for unmasked midtextures on twosided lines that completely
    // fill the gap between floor and ceiling (we don't want to give away
    // the location of any secret areas (false walls)).
    if(LINE_BACKSIDE(line))
    {
        sector_t* front = LINE_SECTOR(line, backside);
        sector_t* back  = LINE_SECTOR(line, backside^1);
        sidedef_t* side  = LINE_SIDE(line, backside);

        if(side->SW_middlematerial)
        {
            material_t* mat = side->SW_middlematerial;
            material_snapshot_t ms;

            // Ensure we have up to date info.
            Materials_Prepare(mat, MPF_SMOOTH, NULL, &ms);

            if(ms.isOpaque && !side->SW_middleblendmode &&
               side->SW_middlergba[3] >= 1)
            {
                float openTop[2], matTop[2];
                float openBottom[2], matBottom[2];

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
                    if(R_MiddleMaterialPosition
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

/**
 * Calculates the placement for a middle texture (top, bottom, offset).
 * texoffy may be NULL.
 * Returns false if the middle texture isn't visible (in the opening).
 *
 * @todo Redundant. Replace with R_FindBottomTopOfHEdgeSection.
 */
int R_MiddleMaterialPosition(float* bottomleft, float* bottomright,
                             float* topleft, float* topright, float* texoffy,
                             float tcyoff, float texHeight, boolean lowerUnpeg,
                             boolean clipTop, boolean clipBottom)
{
    int side;
    float openingTop, openingBottom;
    boolean visible[2] = {false, false};

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

boolean R_FindBottomTopOfHEdgeSection(hedge_t* hEdge, segsection_t section,
                                      const plane_t* ffloor, const plane_t* fceil,
                                      const plane_t* bfloor, const plane_t* bceil,
                                      float* bottom, float* top, float texOffset[2],
                                      float texScale[2])
{
    seg_t* seg = (seg_t*) hEdge->data;

    if(!bfloor)
    {
        surface_t* surface = &HE_FRONTSIDEDEF(hEdge)->SW_middlesurface;

        if(texOffset)
        {
            texOffset[0] = surface->visOffset[0] + seg->offset;
            texOffset[1] = surface->visOffset[1];

            if(HE_FRONTSIDEDEF(hEdge)->lineDef->flags & DDLF_DONTPEGBOTTOM)
                texOffset[1] += -(fceil->visHeight - ffloor->visHeight);
        }

        if(texScale)
        {
            texScale[0] = ((surface->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
            texScale[1] = ((surface->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);
        }

        *bottom = ffloor->visHeight;
        *top = fceil->visHeight;

        return true;
    }

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
            surface_t* surface = &HE_FRONTSIDEDEF(hEdge)->SW_topsurface;

            if(texOffset)
            {
                texOffset[0] = surface->visOffset[0] + seg->offset;
                texOffset[1] = surface->visOffset[1];

                // Align with normal middle texture?
                if(!(HE_FRONTSIDEDEF(hEdge)->lineDef->flags & DDLF_DONTPEGTOP))
                    texOffset[1] += -(fceil->visHeight - bceil->visHeight);
            }

            if(texScale)
            {
                texScale[0] = ((surface->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
                texScale[1] = ((surface->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);
            }

            return true;
        }
        break;

    case SEG_BOTTOM:
        {
        float t = bfloor->visHeight;

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
            surface_t* surface = &HE_FRONTSIDEDEF(hEdge)->SW_bottomsurface;

            if(texOffset)
            {
                texOffset[0] = surface->visOffset[0] + seg->offset;
                texOffset[1] = surface->visOffset[1];

                if(bfloor->visHeight > fceil->visHeight)
                    texOffset[1] += bfloor->visHeight - bceil->visHeight;

                // Align with normal middle texture?
                if(HE_FRONTSIDEDEF(hEdge)->lineDef->flags & DDLF_DONTPEGBOTTOM)
                    texOffset[1] += (fceil->visHeight - bfloor->visHeight);
            }

            if(texScale)
            {
                texScale[0] = ((surface->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
                texScale[1] = ((surface->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);
            }

            return true;
        }
        break;
        }
    case SEG_MIDDLE:
        {
        surface_t* surface = &HE_FRONTSIDEDEF(hEdge)->SW_middlesurface;
        const material_t* mat = surface->material->current;
        float openBottom, openTop, polyBottom, polyTop, xOffset;
        float yOffset, xScale, yScale;
        boolean visible = false;
        boolean clipBottom, clipTop;

        if(!LINE_SELFREF(HE_FRONTSIDEDEF(hEdge)->lineDef))
        {
            openBottom = MAX_OF(bfloor->visHeight, ffloor->visHeight);
            openTop = MIN_OF(bceil->visHeight, fceil->visHeight);

            clipBottom = clipTop = true;
        }
        else
        {
            clipBottom = !(IS_SKYSURFACE(&ffloor->surface) && IS_SKYSURFACE(&bfloor->surface));
            clipTop = !(IS_SKYSURFACE(&fceil->surface) && IS_SKYSURFACE(&bceil->surface));

            if(clipBottom)
                openBottom = MAX_OF(bfloor->visHeight, ffloor->visHeight);
            else
                openBottom = MIN_OF(bfloor->visHeight, ffloor->visHeight);

            if(clipTop)
                openTop = MIN_OF(bceil->visHeight, fceil->visHeight);
            else
                openTop = MAX_OF(bceil->visHeight, fceil->visHeight);
        }

        xOffset = surface->visOffset[0] + seg->offset;
        yOffset = 0;

        xScale = ((surface->flags & DDSUF_MATERIAL_FLIPH)? -1 : 1);
        yScale = ((surface->flags & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        if(openBottom < openTop)
        {
            if(HE_FRONTSIDEDEF(hEdge)->flags & SDF_MIDDLE_STRETCH)
            {
                polyTop = openTop;
                polyBottom = openBottom;
                yOffset += surface->visOffset[1];
                visible = true;
            }
            else
            {
                if(HE_FRONTSIDEDEF(hEdge)->lineDef->flags & DDLF_DONTPEGBOTTOM)
                {
                    if(LINE_SELFREF(HE_FRONTSIDEDEF(hEdge)->lineDef))
                    {
                        polyBottom = ffloor->visHeight + surface->visOffset[1];
                        polyTop = polyBottom + mat->height;
                    }
                    else
                    {
                        polyTop = openBottom + mat->height + surface->visOffset[1];
                        polyBottom = polyTop - mat->height;
                    }
                }
                else
                {
                    polyTop = openTop + surface->visOffset[1];
                    polyBottom = polyTop - mat->height;
                }

                if(clipTop && polyTop > openTop)
                {
                    yOffset += polyTop - openTop;
                    polyTop = openTop;
                }

                if(clipBottom && polyBottom < openBottom)
                {
                    polyBottom = openBottom;
                }

                if(polyTop + yOffset - mat->height < openTop)
                {
                    visible = true;
                }
            }
        }
        else
        {
            polyBottom = polyTop = openBottom;
        }

        *bottom = polyBottom;
        *top = polyTop;

        if(texOffset)
        {
            texOffset[0] = xOffset;
            texOffset[1] = yOffset;
        }

        if(texScale)
        {
            texScale[0] = xScale;
            texScale[1] = yScale;
        }

        if(visible)
            return true;

        break;
        }
    }

    return false;
}

void R_PickPlanesForSegExtrusion(hedge_t* hEdge,
                                 boolean useSectorsFromFrontSideDef,
                                 plane_t** ffloor, plane_t** fceil,
                                 plane_t** bfloor, plane_t** bceil)
{
    sector_t* frontSec;

    if(!useSectorsFromFrontSideDef)
    {
        frontSec = HE_FRONTSECTOR(hEdge);
    }
    else
    {
        frontSec = HE_FRONTSIDEDEF(hEdge)->sector;
    }

    *ffloor = frontSec->SP_plane(PLN_FLOOR);
    *fceil  = frontSec->SP_plane(PLN_CEILING);

    if(R_ConsiderOneSided(hEdge))
    {
        *bfloor = NULL;
        *bceil  = NULL;
    }
    else // Two sided.
    {
        *bfloor = HE_BACKSECTOR(hEdge)->SP_plane(PLN_FLOOR);
        *bceil  = HE_BACKSECTOR(hEdge)->SP_plane(PLN_CEILING);
    }
}

boolean R_UseSectorsFromFrontSideDef(hedge_t* hEdge, segsection_t section)
{
    return (section == SEG_MIDDLE && LINE_SELFREF(HE_FRONTSIDEDEF(hEdge)->lineDef));
}

boolean R_ConsiderOneSided(hedge_t* hEdge)
{
    return !hEdge->twin->data || !((seg_t*) hEdge->twin->data)->sideDef;
}

/**
 * A neighbour is a line that shares a vertex with 'line', and faces the
 * specified sector.
 */
linedef_t* R_FindLineNeighbor(const sector_t* sector, const linedef_t* line,
                              const lineowner_t* own, boolean antiClockwise,
                              binangle_t *diff)
{
    lineowner_t* cown = own->link[!antiClockwise];
    linedef_t* other = cown->lineDef;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? cown->angle : own->angle);

    if(!LINE_BACKSIDE(other) || LINE_FRONTSECTOR(other) != LINE_BACKSECTOR(other))
    {
        if(sector) // Must one of the sectors match?
        {
            if(LINE_FRONTSECTOR(other) == sector ||
               (LINE_BACKSIDE(other) && LINE_BACKSECTOR(other) == sector))
                return other;
        }
        else
            return other;
    }

    // Not suitable, try the next.
    return R_FindLineNeighbor(sector, line, cown, antiClockwise, diff);
}

linedef_t* R_FindSolidLineNeighbor(const sector_t* sector,
                                   const linedef_t* line,
                                   const lineowner_t* own,
                                   boolean antiClockwise, binangle_t* diff)
{
    lineowner_t* cown = own->link[!antiClockwise];
    linedef_t* other = cown->lineDef;
    int side;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? cown->angle : own->angle);

    if(!(other->buildData.windowEffect && LINE_FRONTSECTOR(other) != sector))
    {
        if(!LINE_FRONTSIDE(other) || !LINE_BACKSIDE(other))
            return other;

        if(!LINE_SELFREF(other) &&
           (LINE_FRONTSECTOR(other)->SP_floorvisheight >= sector->SP_ceilvisheight ||
            LINE_FRONTSECTOR(other)->SP_ceilvisheight <= sector->SP_floorvisheight ||
            LINE_BACKSECTOR(other)->SP_floorvisheight >= sector->SP_ceilvisheight ||
            LINE_BACKSECTOR(other)->SP_ceilvisheight <= sector->SP_floorvisheight ||
            LINE_BACKSECTOR(other)->SP_ceilvisheight <= LINE_BACKSECTOR(other)->SP_floorvisheight))
            return other;

        // Both front and back MUST be open by this point.

        // Check for mid texture which fills the gap between floor and ceiling.
        // We should not give away the location of false walls (secrets).
        side = (LINE_FRONTSECTOR(other) == sector? 0 : 1);
        if(LINE_SIDE(other, side)->SW_middlematerial)
        {
            float oFCeil  = LINE_FRONTSECTOR(other)->SP_ceilvisheight;
            float oFFloor = LINE_FRONTSECTOR(other)->SP_floorvisheight;
            float oBCeil  = LINE_BACKSECTOR(other)->SP_ceilvisheight;
            float oBFloor = LINE_BACKSECTOR(other)->SP_floorvisheight;

            if((side == 0 &&
                ((oBCeil > sector->SP_floorvisheight &&
                      oBFloor <= sector->SP_floorvisheight) ||
                 (oBFloor < sector->SP_ceilvisheight &&
                      oBCeil >= sector->SP_ceilvisheight) ||
                 (oBFloor < sector->SP_ceilvisheight &&
                      oBCeil > sector->SP_floorvisheight))) ||
               ( /* side must be 1 */
                ((oFCeil > sector->SP_floorvisheight &&
                      oFFloor <= sector->SP_floorvisheight) ||
                 (oFFloor < sector->SP_ceilvisheight &&
                      oFCeil >= sector->SP_ceilvisheight) ||
                 (oFFloor < sector->SP_ceilvisheight &&
                      oFCeil > sector->SP_floorvisheight)))  )
            {

                if(!R_DoesMiddleMaterialFillGap(other, side))
                    return 0;
            }
        }
    }

    // Not suitable, try the next.
    return R_FindSolidLineNeighbor(sector, line, cown, antiClockwise, diff);
}

/**
 * Find a backneighbour for the given line.
 * They are the neighbouring line in the backsector of the imediate line
 * neighbor.
 */
linedef_t* R_FindLineBackNeighbor(const sector_t* sector,
                                  const linedef_t* line,
                                  const lineowner_t* own,
                                  boolean antiClockwise,
                                  binangle_t* diff)
{
    lineowner_t* cown = own->link[!antiClockwise];
    linedef_t* other = cown->lineDef;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? cown->angle : own->angle);

    if(!LINE_BACKSIDE(other) || LINE_FRONTSECTOR(other) != LINE_BACKSECTOR(other) ||
       other->buildData.windowEffect)
    {
        if(!(LINE_FRONTSECTOR(other) == sector ||
             (LINE_BACKSIDE(other) && LINE_BACKSECTOR(other) == sector)))
            return other;
    }

    // Not suitable, try the next.
    return R_FindLineBackNeighbor(sector, line, cown, antiClockwise,
                                   diff);
}

/**
 * A side's alignneighbor is a line that shares a vertex with 'line' and
 * whos orientation is aligned with it (thus, making it unnecessary to have
 * a shadow between them. In practice, they would be considered a single,
 * long sidedef by the shadow generator).
 */
linedef_t* R_FindLineAlignNeighbor(const sector_t* sec,
                                   const linedef_t* line,
                                   const lineowner_t* own,
                                   boolean antiClockwise,
                                   int alignment)
{
#define SEP 10

    lineowner_t* cown = own->link[!antiClockwise];
    linedef_t* other = cown->lineDef;
    binangle_t diff;

    if(other == line)
        return NULL;

    if(!LINE_SELFREF(other))
    {
        diff = line->angle - other->angle;

        if(alignment < 0)
            diff -= BANG_180;
        if(LINE_FRONTSECTOR(other) != sec)
            diff -= BANG_180;
        if(diff < SEP || diff > BANG_360 - SEP)
            return other;
    }

    // Can't step over non-twosided lines.
    if((!LINE_BACKSIDE(other) || !LINE_FRONTSIDE(other)))
        return NULL;

    // Not suitable, try the next.
    return R_FindLineAlignNeighbor(sec, line, cown, antiClockwise, alignment);

#undef SEP
}

/**
 * Walk the half-edges of the specified subsector, looking for a half-edge
 * that is suitable for use as the base of a tri-fan.
 *
 * We do not want any overlapping tris so check that the area of each triangle
 * is > 0, if not; try the next vertice until we find a good one to use as the
 * center of the trifan. If a suitable point cannot be found use the center of
 * subsector instead (it will always be valid as subsectors are convex).
 */
void R_PickSubsectorFanBase(subsector_t* subsector)
{
#define TRIFAN_LIMIT    0.1

    hedge_t* base = NULL;

    if(subsector->firstFanHEdge)
        return; // Already chosen.

    subsector->useMidPoint = false;

    /**
     * We need to find a good tri-fan base vertex, (one that doesn't
     * generate zero-area triangles).
     */
    if(subsector->hEdgeCount > 3)
    {
        uint baseIDX;
        hedge_t* hEdge;
        vec2_t basepos, apos, bpos;

        baseIDX = 0;
        hEdge = subsector->face->hEdge;
        do
        {
            uint i;
            hedge_t* hEdge2;

            i = 0;
            base = hEdge;
            V2_Set(basepos, base->HE_v1->pos[VX], base->HE_v1->pos[VY]);
            hEdge2 = subsector->face->hEdge;
            do
            {
                if(!(baseIDX > 0 && (i == baseIDX || i == baseIDX - 1)))
                {
                    V2_Set(apos, hEdge2->HE_v1->pos[VX], hEdge2->HE_v1->pos[VY]);
                    V2_Set(bpos, hEdge2->HE_v2->pos[VX], hEdge2->HE_v2->pos[VY]);

                    if(TRIFAN_LIMIT >= M_TriangleArea(basepos, apos, bpos))
                    {
                        base = NULL;
                    }
                }

                i++;
            } while(base && (hEdge2 = hEdge2->next) != subsector->face->hEdge);

            if(!base)
                baseIDX++;
        } while(!base && (hEdge = hEdge->next) != subsector->face->hEdge);

        if(!base)
            subsector->useMidPoint = true;
    }

    if(base)
        subsector->firstFanHEdge = base;
    else
        subsector->firstFanHEdge = subsector->face->hEdge;

#undef TRIFAN_LIMIT
}

/**
 * The test is done on subsectors.
 */
#if 0 /* Currently unused. */
static sector_t* getContainingSectorOf(map_t* map, sector_t* sec)
{
    uint i;
    float cdiff = -1, diff;
    float inner[4], outer[4];
    sector_t* other, *closest = NULL;

    memcpy(inner, sec->bBox, sizeof(inner));

    // Try all sectors that fit in the bounding box.
    for(i = 0, other = map->sectors; i < map->numSectors; other++, ++i)
    {
        if(!other->lineDefCount || (other->flags & SECF_UNCLOSED))
            continue;

        if(other == sec)
            continue; // Don't try on self!

        memcpy(outer, other->bBox, sizeof(outer));
        if(inner[BOXLEFT]  >= outer[BOXLEFT] &&
           inner[BOXRIGHT] <= outer[BOXRIGHT] &&
           inner[BOXTOP]   <= outer[BOXTOP] &&
           inner[BOXBOTTOM]>= outer[BOXBOTTOM])
        {
            // Sec is totally and completely inside other!
            diff = M_BoundingBoxDiff(inner, outer);
            if(cdiff < 0 || diff <= cdiff)
            {
                closest = other;
                cdiff = diff;
            }
        }
    }
    return closest;
}
#endif

static __inline void initSurfaceMaterialOffset(surface_t* suf)
{
    if(!suf)
        return;

    suf->visOffset[VX] = suf->oldOffset[0][VX] =
        suf->oldOffset[1][VX] = suf->offset[VX];
    suf->visOffset[VY] = suf->oldOffset[0][VY] =
        suf->oldOffset[1][VY] = suf->offset[VY];
}

/**
 * Called by the game at various points in the map setup process.
 */
void R_SetupMap(int mode, int flags)
{
    uint i;

    switch(mode)
    {
    case DDSMM_INITIALIZE:
        {
        map_t* map = P_CurrentMap();

        P_DestroyMap(map);

        // @todo remove the PU_MAP and PU_MAPSTATIC zone tags.
        Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);
        }

        // Switch to fast malloc mode in the zone. This is intended for large
        // numbers of mallocs with no frees in between.
        Z_EnableFastMalloc(false);

        // A new map is about to be setup.
        ddMapSetup = true;
        return;

    case DDSMM_AFTER_LOADING:
        {
        map_t* map = P_CurrentMap();

        // Update everything again. Its possible that after loading we
        // now have more HOMs to fix, etc..

        Map_InitSkyFix(map);

        // Set intial values of various tracked and interpolated properties
        // (lighting, smoothed planes etc).
        for(i = 0; i < map->numSectors; ++i)
        {
            sector_t* sec = map->sectors[i];
            uint j;

            R_UpdateSector(sec, false);
            for(j = 0; j < sec->planeCount; ++j)
            {
                plane_t* pln = sec->SP_plane(j);

                pln->visHeight = pln->oldHeight[0] = pln->oldHeight[1] =
                    pln->height;

                initSurfaceMaterialOffset(&pln->surface);
            }
        }

        for(i = 0; i < map->numSideDefs; ++i)
        {
            sidedef_t* si = map->sideDefs[i];

            initSurfaceMaterialOffset(&si->SW_topsurface);
            initSurfaceMaterialOffset(&si->SW_middlesurface);
            initSurfaceMaterialOffset(&si->SW_bottomsurface);
        }

        P_MapInitPolyobjs(map);
        return;
        }
    case DDSMM_FINALIZE:
        {
        map_t* map = P_CurrentMap();

        // We are now finished with the game data, map object db.
        Map_DestroyGameObjectRecords(map);

        // Init server data.
        Sv_InitPools();

        // Recalculate the light range mod matrix.
        R_CalcLightModRange(NULL);

        // Update all sectors. Set intial values of various tracked
        // and interpolated properties (lighting, smoothed planes etc).
        for(i = 0; i < map->numSectors; ++i)
        {
            sector_t* sec = map->sectors[i];
            uint l;

            R_UpdateSector(sec, true);
            for(l = 0; l < sec->planeCount; ++l)
            {
                plane_t* pln = sec->SP_plane(l);

                pln->visHeight = pln->oldHeight[0] = pln->oldHeight[1] =
                    pln->height;

                initSurfaceMaterialOffset(&pln->surface);
            }
        }

        for(i = 0; i < map->numSideDefs; ++i)
        {
            sidedef_t* si = map->sideDefs[i];

            initSurfaceMaterialOffset(&si->SW_topsurface);
            initSurfaceMaterialOffset(&si->SW_middlesurface);
            initSurfaceMaterialOffset(&si->SW_bottomsurface);
        }

        P_MapInitPolyobjs(map);

        // Run any commands specified in Map Info.
        {
        ded_mapinfo_t* mapInfo = Def_GetMapInfo(Map_ID(map));

        if(mapInfo && mapInfo->execute)
            Con_Execute(CMDS_DED, mapInfo->execute, true, false);
        }

        // The map setup has been completed. Run the special map setup
        // command, which the user may alias to do something useful.
         {
        char cmd[80];

        sprintf(cmd, "init-%s", map->mapID);
        if(Con_IsValidCommand(cmd))
        {
            Con_Executef(CMDS_DED, false, cmd);
        }
        }

        // Clear any input events that might have accumulated during the
        // setup period.
        DD_ClearEvents();

        // Now that the setup is done, let's reset the tictimer so it'll
        // appear that no time has passed during the setup.
        DD_ResetTimer();

        // Kill all local commands and determine the invoid status of players.
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t* plr = &ddPlayers[i];
            ddplayer_t* ddpl = &plr->shared;

            clients[i].numTics = 0;

            // Determine if the player is in the void.
            ddpl->inVoid = true;
            if(ddpl->mo)
            {
                const subsector_t* subsector =
                    R_PointInSubSector(ddpl->mo->pos[VX], ddpl->mo->pos[VY]);

                //// \fixme $nplanes
                if(ddpl->mo->pos[VZ] >= subsector->sector->SP_floorvisheight &&
                   ddpl->mo->pos[VZ] < subsector->sector->SP_ceilvisheight - 4)
                   ddpl->inVoid = false;
            }
        }

        // Reset the map tick timer.
        ddMapTime = 0;

        // We've finished setting up the map.
        ddMapSetup = false;

        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;

        // Switch back to normal malloc mode in the zone. Z_Malloc will look
        // for free blocks in the entire zone and purge purgable blocks.
        Z_EnableFastMalloc(false);
        return;
        }
    case DDSMM_AFTER_BUSY:
        {
        map_t* map = P_CurrentMap();
        ded_mapinfo_t* mapInfo = Def_GetMapInfo(Map_ID(map));

        // Shouldn't do anything time-consuming, as we are no longer in busy mode.
        if(!mapInfo || !(mapInfo->flags & MIF_FOG))
            R_SetupFogDefaults();
        else
            R_SetupFog(mapInfo->fogStart, mapInfo->fogEnd,
                       mapInfo->fogDensity, mapInfo->fogColor);
        break;
        }
    default:
        Con_Error("R_SetupMap: Unknown setup mode %i", mode);
    }
}

void R_MarkLineDefAsDrawnForViewer(linedef_t* lineDef, int pid)
{
    int viewer = pid;

    if(!lineDef->mapped[viewer])
    {
        lineDef->mapped[viewer] = true; // This line is now seen in the map.

        // Send a status report.
        if(gx.HandleMapObjectStatusReport)
            gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED,
                                           P_ToIndex(P_ObjectRecord(DMU_LINEDEF, lineDef)),
                                           DMU_LINEDEF, &viewer);
    }
}

/**
 * Is the specified plane glowing (it glows or is a sky mask surface)?
 *
 * @return              @c true, if the specified plane is non-glowing,
 *                      i.e. not glowing or a sky.
 */
boolean R_IsGlowingPlane(const plane_t* pln)
{
    material_t* mat = pln->surface.material;

    return ((mat && (mat->flags & MATF_NO_DRAW)) || pln->glow > 0 ||
            IS_SKYSURFACE(&pln->surface));
}

/**
 * Does the specified sector contain any sky surfaces?
 *
 * @return              @c true, if one or more surfaces in the given sector
 *                      use the special sky mask material.
 */
boolean R_SectorContainsSkySurfaces(const sector_t* sec)
{
    uint i;
    boolean sectorContainsSkySurfaces;

    // Does this sector feature any sky surfaces?
    sectorContainsSkySurfaces = false;
    i = 0;
    do
    {
        if(IS_SKYSURFACE(&sec->SP_planesurface(i)))
            sectorContainsSkySurfaces = true;
        else
            i++;
    } while(!sectorContainsSkySurfaces && i < sec->planeCount);

    return sectorContainsSkySurfaces;
}

boolean R_SideDefIsSoftSurface(sidedef_t* sideDef, segsection_t section)
{
    return section == SEG_MIDDLE &&
        ((viewPlayer->shared.flags & (DDPF_NOCLIP|DDPF_CAMERA)) ||
          !(sideDef->lineDef->flags & DDLF_BLOCKING));
}

float R_ApplySoftSurfaceDeltaToAlpha(float bottom, float top, sidedef_t* sideDef, float alpha)
{
    mobj_t* mo = viewPlayer->shared.mo;
    linedef_t* lineDef = sideDef->lineDef;

    if(viewZ > bottom && viewZ < top)
    {
        float pos, result[2];

        {
        vec2_t vpos, delta;
        V2_Set(delta, lineDef->dX, lineDef->dY);
        V2_Set(vpos, lineDef->L_v1->pos[VX], lineDef->L_v1->pos[VY]);
        pos = M_ProjectPointOnLine(mo->pos, vpos, delta, 0, result);
        }

        if(pos > 0 && pos < 1)
        {
            float delta[2], distance, minDistance = mo->radius * .8f;

            V2_Subtract(delta, mo->pos, result);

            distance = M_ApproxDistancef(delta[VX], delta[VY]);

            if(distance < minDistance)
            {
                // Fade it out the closer the viewPlayer gets and clamp.
                alpha = (alpha / minDistance) * distance;
                alpha = MINMAX_OF(0, alpha, 1);
            }
        }
    }

    return alpha;
}

/**
 * Given a sidedef section, look at the neighbouring surfaces and pick the
 * best choice of material used on those surfaces to be applied to "this"
 * surface.
 */
static material_t* chooseFixMaterial(const hedge_t* hEdge, segsection_t section)
{
    material_t* choice = NULL;

    // Try the materials used on the front and back sector planes,
    // favouring non-animated materials.
    if(section == SEG_BOTTOM || section == SEG_TOP)
    {
        sector_t* backSec = HE_BACKSECTOR(hEdge);

        if(backSec)
        {
            surface_t* backSuf = &backSec->
                SP_plane(section == SEG_BOTTOM? PLN_FLOOR : PLN_CEILING)->
                    surface;

            if(!(backSuf->material && backSuf->material->inAnimGroup) &&
               !IS_SKYSURFACE(backSuf))
                choice = backSuf->material;
        }

        if(!choice)
            choice = HE_FRONTSECTOR(hEdge)->
                SP_plane(section == SEG_BOTTOM? PLN_FLOOR : PLN_CEILING)->
                    surface.material;
    }

    return choice;
}

static void updateSideDefSection(hedge_t* hEdge, segsection_t section)
{
    sidedef_t* sideDef = HE_FRONTSIDEDEF(hEdge);
    surface_t* suf = &sideDef->SW_surface(section);

    if(suf->inFlags & SUIF_MATERIAL_FIX)
    {
        Surface_SetMaterial(suf, NULL, false);
        suf->inFlags &= ~SUIF_MATERIAL_FIX;
    }

    if(!suf->material &&
       !IS_SKYSURFACE(&HE_FRONTSECTOR(hEdge)->
            SP_plane(section == SEG_BOTTOM? PLN_FLOOR : PLN_CEILING)->
                surface))
    {
        Surface_SetMaterial(suf, chooseFixMaterial(hEdge, section), false);
        suf->inFlags |= SUIF_MATERIAL_FIX;
    }
}

void R_UpdateSideDefsOfSubSector(subsector_t* subsector)
{
    hedge_t* hEdge;

    if((hEdge = subsector->face->hEdge))
    {
        do
        {
            sector_t* frontSec, *backSec;

            // Only spread to real back subsectors.
            if(!hEdge->twin->data || !HE_FRONTSIDEDEF(hEdge->twin))
                continue;

            frontSec = HE_FRONTSECTOR(hEdge);
            backSec  = HE_BACKSECTOR(hEdge);

            /**
             * Do as in the original Doom if the texture has not been defined -
             * extend the floor/ceiling to fill the space (unless it is skymasked),
             * or if there is a midtexture use that instead.
             */

            // Check for missing lowers.
            if(frontSec->SP_floorheight < backSec->SP_floorheight)
                updateSideDefSection(hEdge, SEG_BOTTOM);
            else if(frontSec->SP_floorheight > backSec->SP_floorheight)
                updateSideDefSection(hEdge->twin, SEG_BOTTOM);

            // Check for missing uppers.
            if(backSec->SP_ceilheight < frontSec->SP_ceilheight)
                updateSideDefSection(hEdge, SEG_TOP);
            else if(backSec->SP_ceilheight > frontSec->SP_ceilheight)
                updateSideDefSection(hEdge->twin, SEG_TOP);

        } while((hEdge = hEdge->next) != subsector->face->hEdge);
    }
}

boolean R_UpdatePlane(plane_t* pln, boolean forceUpdate)
{
    boolean changed = false;
    boolean hasGlow = false;
    map_t* map = P_CurrentMap();

    // Update the glow properties.
    hasGlow = false;
    if(pln->PS_material && ((pln->surface.flags & DDSUF_GLOW) ||
       (pln->PS_material->flags & MATF_GLOW)))
    {
        material_snapshot_t ms;

        Materials_Prepare(pln->PS_material, 0, NULL, &ms);
        pln->glowRGB[CR] = ms.color[CR];
        pln->glowRGB[CG] = ms.color[CG];
        pln->glowRGB[CB] = ms.color[CB];
        hasGlow = true;
        changed = true;
    }

    if(hasGlow)
    {
        pln->glow = 4; // Default height factor is 4
    }
    else
    {
        pln->glowRGB[CR] = pln->glowRGB[CG] =
            pln->glowRGB[CB] = 0;
        pln->glow = 0;
    }

    // Geometry change?
    if(forceUpdate || pln->height != pln->oldHeight[1])
    {
        uint i;
        subsector_t** subsectorPtr;
        sidedef_t* front = NULL, *back = NULL;

        for(i = 0; i < map->numSectors; ++i)
        {
            sector_t* sec = map->sectors[i];
            uint j;

            for(j = 0; j < sec->planeCount; ++j)
            {
                int k;

                if(sec->planes[j] != pln)
                    continue;

                // Check if there are any camera players in this sector. If their
                // height is now above the ceiling/below the floor they are now in
                // the void.
                for(k = 0; k < DDMAXPLAYERS; ++k)
                {
                    player_t* plr = &ddPlayers[k];
                    ddplayer_t* ddpl = &plr->shared;

                    if(!ddpl->inGame || !ddpl->mo || !ddpl->mo->subsector)
                        continue;

                    //// \fixme $nplanes
                    if((ddpl->flags & DDPF_CAMERA) &&
                       ((subsector_t*)((objectrecord_t*) ddpl->mo->subsector)->obj)->sector == sec &&
                       (ddpl->mo->pos[VZ] > sec->SP_ceilheight ||
                        ddpl->mo->pos[VZ] < sec->SP_floorheight))
                    {
                        ddpl->inVoid = true;
                    }
                }

                subsectorPtr = sec->subsectors;
                while(*subsectorPtr)
                {
                    subsector_t* subsector = *subsectorPtr;

                    R_UpdateSideDefsOfSubSector(subsector);

                    // Inform the shadow bias of changed geometry?
                    if(subsector->bsuf)
                    {
                        hedge_t* hEdge;

                        if((hEdge = subsector->face->hEdge))
                        {
                            do
                            {
                                seg_t* seg = (seg_t*) hEdge->data;

                                if(seg->sideDef)
                                {
                                    int m;
                                    for(m = 0; m < 3; ++m)
                                        SB_SurfaceMoved(map, seg->bsuf[m]);
                                }
                            } while((hEdge = hEdge->next) != subsector->face->hEdge);
                        }

                        SB_SurfaceMoved(map, subsector->bsuf[j]);
                    }

                    *subsectorPtr++;
                }

                Sector_UpdateSoundEnvironment(sec);

                // We need the decorations updated.
                Surface_Update(&pln->surface);

                changed = true;
            }
        }
    }

    return changed;
}

#if 0
/**
 * Stub.
 */
boolean R_UpdateSubSector(face_t* subSector, boolean forceUpdate)
{
    return false; // Not changed.
}
#endif

boolean R_UpdateSector(sector_t* sec, boolean forceUpdate)
{
    uint i;
    boolean changed = false, planeChanged = false;

    // Check if there are any lightlevel or color changes.
    if(forceUpdate ||
       (sec->lightLevel != sec->oldLightLevel ||
        sec->rgb[0] != sec->oldRGB[0] ||
        sec->rgb[1] != sec->oldRGB[1] ||
        sec->rgb[2] != sec->oldRGB[2]))
    {
        sec->frameFlags |= SIF_LIGHT_CHANGED;
        sec->oldLightLevel = sec->lightLevel;
        memcpy(sec->oldRGB, sec->rgb, sizeof(sec->oldRGB));

        LG_SectorChanged(sec);

        changed = true;
    }
    else
    {
        sec->frameFlags &= ~SIF_LIGHT_CHANGED;
    }

    // For each plane.
    for(i = 0; i < sec->planeCount; ++i)
    {
        if(R_UpdatePlane(sec->planes[i], forceUpdate))
        {
            planeChanged = true;
        }
    }

    return planeChanged;
}

/**
 * Stub.
 */
boolean R_UpdateLineDef(linedef_t* line, boolean forceUpdate)
{
    return false; // Not changed.
}

/**
 * Stub.
 */
boolean R_UpdateSideDef(sidedef_t* side, boolean forceUpdate)
{
    return false; // Not changed.
}

/**
 * Stub.
 */
boolean R_UpdateSurface(surface_t* suf, boolean forceUpdate)
{
    return false; // Not changed.
}

/**
 * All links will be updated every frame (sectorheights may change at
 * any time without notice).
 */
void R_UpdatePlanes(void)
{
    // Nothing to do.
}

/**
 * The DOOM lighting model applies distance attenuation to sector light
 * levels.
 *
 * @param distToViewer  Distance from the viewer to this object.
 * @param lightLevel    Sector lightLevel at this object's origin.
 * @return              The specified lightLevel plus any attentuation.
 */
float R_DistAttenuateLightLevel(float distToViewer, float lightLevel)
{
    if(distToViewer > 0 && rendLightDistanceAttentuation > 0)
    {
        float real, minimum;

        real = lightLevel -
            (distToViewer - 32) / rendLightDistanceAttentuation *
                (1 - lightLevel);

        minimum = lightLevel * lightLevel + (lightLevel - .63f) * .5f;
        if(real < minimum)
            real = minimum; // Clamp it.

        return real;
    }

    return lightLevel;
}

/**
 * The DOOM lighting model applies a sector light level delta when drawing
 * segs based on their 2D world angle.
 *
 * @param l             LineDef to calculate the delta for.
 * @param side          Side of the linedef we are interested in.
 * @return              Calculated delta.
 */
float R_WallAngleLightLevelDelta(const linedef_t* l, byte side)
{
    if(!(rendLightWallAngle > 0))
        return 0;

    // Do a lighting adjustment based on orientation.
    return LineDef_GetLightLevelDelta(l) * (side? -1 : 1) *
        rendLightWallAngle;
}

/**
 * Updates the lightModRange which is used to applify sector light to help
 * compensate for the differences between the OpenGL lighting equation,
 * the software Doom lighting model and the light grid (ambient lighting).
 *
 * The offsets in the lightRangeModTables are added to the sector->lightLevel
 * during rendering (both positive and negative).
 */
void R_CalcLightModRange(cvar_t* unused)
{
    int j, mapAmbient;
    float f;
    map_t* map = P_CurrentMap();

    memset(lightModRange, 0, sizeof(float) * 255);

    mapAmbient = Map_AmbientLightLevel(map);
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
 * The DOOM lighting model applies a light level delta to everything when
 * e.g. the player shoots.
 *
 * @return              Calculated delta.
 */
float R_ExtraLightDelta(void)
{
    return extraLightDelta;
}

/**
 * @return              @c > 0, if the sector lightlevel passes the
 *                      limit condition.
 */
float R_CheckSectorLight(float lightlevel, float min, float max)
{
    // Has a limit been set?
    if(min == max)
        return 1;

    // Apply adaptation
    R_ApplyLightAdaptation(&lightlevel);

    return MINMAX_OF(0, (lightlevel - min) / (float) (max - min), 1);
}

/**
 * Sector light color may be affected by the sky light color.
 */
const float* R_GetSectorLightColor(const sector_t *sector)
{
    const float* skyLight;

    if(!rendSkyLight ||
       !(skyLight = Sky_GetLightColor(theSky)))
        return sector->rgb; // The sector's real color.

    if(!R_SectorContainsSkySurfaces(sector))
    {
        sector_t* src;

        // A dominant light source affects this sector?
        src = sector->lightSource;
        if(src && src->lightLevel >= sector->lightLevel)
        {
            // The color shines here, too.
            return R_GetSectorLightColor(src);
        }

        return sector->rgb;
    }

    // Return the sky light color.
    return skyLight;
}

#if _DEBUG
D_CMD(UpdateSurfaces)
{
    uint i;
    map_t* map = P_CurrentMap();

    Con_Printf("Updating world surfaces...\n");

    for(i = 0; i < map->numSectors; ++i)
        R_UpdateSector(map->sectors[i], true);

    return true;
}
#endif
