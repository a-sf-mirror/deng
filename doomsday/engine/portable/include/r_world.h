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
 * r_world.h: World Setup and Refresh
 */

#ifndef __DOOMSDAY_REFRESH_WORLD_H__
#define __DOOMSDAY_REFRESH_WORLD_H__

#include "r_data.h"

#if _DEBUG
D_CMD(UpdateSurfaces);
#endif

// Used for vertex sector owners, side line owners and reverb subsectors.
typedef struct ownernode_s {
    void*           data;
    struct ownernode_s* next;
} ownernode_t;

typedef struct {
    ownernode_t*    head;
    uint            count;
} ownerlist_t;

extern int rendSkyLight; // cvar
extern float rendMaterialFadeSeconds; // cvar
extern boolean ddMapSetup;
extern int rAmbient, ambientLight;
extern float rendLightDistanceAttentuation, lightRangeCompression;
extern float lightModRange[255];

// Sky flags.
#define SIF_DRAW_SPHERE     0x1 // Always draw the sky sphere.

void            R_SetupMap(int mode, int flags);
void            R_InitLinks(gamemap_t* map);
void            R_BuildSectorLinks(gamemap_t* map);
void            R_UpdatePlanes(void);
void            R_ClearSectorFlags(gamemap_t* map);
void            R_InitSkyFix(gamemap_t* map);
void            R_SetupFog(float start, float end, float density, float* rgb);
void            R_SetupFogDefaults(void);

const float*    R_GetSectorLightColor(const sector_t* sector);
float           R_DistAttenuateLightLevel(float distToViewer, float lightLevel);
float           R_ExtraLightDelta(void);
void            R_CalcLightModRange(struct cvar_s* unused);
float           R_CheckSectorLight(float lightlevel, float min, float max);

void            R_PickSubsectorFanBase(subsector_t* subsector);
boolean         R_SectorContainsSkySurfaces(const sector_t* sec);

float           R_WallAngleLightLevelDelta(const linedef_t* l, byte side);
void            R_MarkLineDefAsDrawnForViewer(linedef_t* lineDef, int pid);

void            R_UpdateSkyFixForSec(gamemap_t* map, uint secIDX);
void            R_OrderVertices(const linedef_t* line, const sector_t* sector,
                                vertex_t* verts[2]);
plane_t*        R_NewPlaneForSector(gamemap_t* map, sector_t* sec);
void            R_DestroyPlaneOfSector(gamemap_t* map, uint id, sector_t* sec);

surfacedecor_t* R_CreateSurfaceDecoration(decortype_t type, surface_t* suf);
void            R_ClearSurfaceDecorations(surface_t* suf);

void            R_CreateBiasSurfacesInSubsector(subsector_t* subsector);

void            R_UpdateWatchedPlanes(gamemap_t* map);
void            R_InterpolateWatchedPlanes(gamemap_t* map, boolean resetNextViewer);

void            R_UpdateMovingSurfaces(gamemap_t* map);
void            R_InterpolateMovingSurfaces(gamemap_t* map, boolean resetNextViewer);

void            PlaneList_Add(planelist_t* pl, plane_t* pln);
boolean         PlaneList_Remove(planelist_t* pl, const plane_t* pln);
void            PlaneList_Empty(planelist_t* pl);
boolean         PlaneList_Iterate(planelist_t* pl,
                                  boolean (*callback) (plane_t*, void*),
                                  void* context);

void            SurfaceList_Add(surfacelist_t* sl, surface_t* suf);
boolean         SurfaceList_Remove(surfacelist_t* sl, const surface_t* suf);
void            SurfaceList_Empty(surfacelist_t* sl);
boolean         SurfaceList_Iterate(surfacelist_t* sl,
                                    boolean (*callback) (surface_t*, void*),
                                    void* context);

void            R_MarkDependantSurfacesForDecorationUpdate(plane_t* pln);
boolean         R_IsGlowingPlane(const plane_t* pln);

boolean         R_SideDefIsSoftSurface(sidedef_t* sideDef, segsection_t section);
float           R_ApplySoftSurfaceDeltaToAlpha(float bottom, float top, sidedef_t* sideDef, float alpha);

boolean         R_DoesMiddleMaterialFillGap(linedef_t* line, int backside);
int             R_MiddleMaterialPosition(float* bottomleft, float* bottomright,
                                    float* topleft, float* topright,
                                    float* texoffy, float tcyoff, float texHeight,
                                    boolean lower_unpeg, boolean clipTop,
                                    boolean clipBottom);

boolean         R_FindBottomTopOfHEdgeSection(hedge_t* hEdge, segsection_t section,
                                              const plane_t* ffloor, const plane_t* fceil,
                                              const plane_t* bfloor, const plane_t* bceil,
                                              float* bottom, float* top, float texOffset[2],
                                              float texScale[2]);
void            R_PickPlanesForSegExtrusion(hedge_t* hEdge,
                                            boolean useSectorsFromFrontSideDef,
                                            plane_t** ffloor, plane_t** fceil,
                                            plane_t** bfloor, plane_t** bceil);
boolean         R_UseSectorsFromFrontSideDef(hedge_t* hEdge, segsection_t section);
boolean         R_ConsiderOneSided(hedge_t* hEdge);

void            R_FindSegSectionDivisions(walldiv_t* wdivs, const hedge_t* hEdge,
                                          const sector_t* frontsec, float low, float hi);

lineowner_t*    R_GetVtxLineOwner(const vertex_t* vtx, const linedef_t* line);
linedef_t*      R_FindLineNeighbor(const sector_t* sector,
                                   const linedef_t* line,
                                   const lineowner_t* own,
                                   boolean antiClockwise, binangle_t* diff);
linedef_t*      R_FindSolidLineNeighbor(const sector_t* sector,
                                        const linedef_t* line,
                                        const lineowner_t* own,
                                        boolean antiClockwise,
                                        binangle_t* diff);
linedef_t*      R_FindLineBackNeighbor(const sector_t* sector,
                                       const linedef_t* line,
                                       const lineowner_t* own,
                                       boolean antiClockwise,
                                       binangle_t* diff);

void            R_MatFaderThinker(matfader_t* fader);
void            R_StopMatFader(matfader_t* fader);
int             RIT_StopMatFader(void* p, void* context);
#endif
