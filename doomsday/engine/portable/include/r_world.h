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

#ifndef DOOMSDAY_REFRESH_WORLD_H
#define DOOMSDAY_REFRESH_WORLD_H

//#include "r_data.h"
#include "map.h"

#if _DEBUG
D_CMD(UpdateSurfaces);
#endif

extern int rendSkyLight; // cvar
extern float rendMaterialFadeSeconds; // cvar
extern boolean ddMapSetup;
extern int rAmbient, ambientLight;
extern float rendLightDistanceAttentuation, lightRangeCompression;
extern float lightModRange[255];

// Sky flags.
#define SIF_DRAW_SPHERE     0x1 // Always draw the sky sphere.

void            R_BeginSetupMap(void);
void            R_SetupMap(struct map_s* map, int mode, int flags);
void            R_UpdatePlanes(void);
void            R_SetupFog(float start, float end, float density, float* rgb);
void            R_SetupFogDefaults(void);

const float*    R_GetSectorLightColor(const sector_t* sector);
float           R_DistAttenuateLightLevel(float distToViewer, float lightLevel);
float           R_ExtraLightDelta(void);
void            R_CalcLightModRange(struct cvar_s* unused);
float           R_CheckSectorLight(float lightlevel, float min, float max);

void            R_PickSubsectorFanBase(subsector_t* subsector);
boolean         R_SectorContainsSkySurfaces(const sector_t* sec);
void            R_MarkAllSectorsForLightGridUpdate(cvar_t* unused);
float           R_WallAngleLightLevelDelta(const linedef_t* l, byte side);
void            R_MarkLineDefAsDrawnForViewer(linedef_t* lineDef, int pid);

void            R_OrderVertices(const linedef_t* line, const sector_t* sector,
                                vertex_t* verts[2]);

surfacedecor_t* R_CreateSurfaceDecoration(decortype_t type, surface_t* suf);
void            R_ClearSurfaceDecorations(surface_t* suf);

void            R_CreateBiasSurfacesInSubsector(subsector_t* subsector);

planelist_t*    P_CreatePlaneList(void);
void            P_DestroyPlaneList(planelist_t* pl);

void            PlaneList_Add(planelist_t* pl, plane_t* pln);
boolean         PlaneList_Remove(planelist_t* pl, const plane_t* pln);
void            PlaneList_Empty(planelist_t* pl);
boolean         PlaneList_Iterate(planelist_t* pl,
                                  boolean (*callback) (plane_t*, void*),
                                  void* context);

surfacelist_t*  P_CreateSurfaceList(void);
void            P_DestroySurfaceList(surfacelist_t* sl);

void            SurfaceList_Add(surfacelist_t* sl, surface_t* suf);
boolean         SurfaceList_Remove(surfacelist_t* sl, const surface_t* suf);
void            SurfaceList_Empty(surfacelist_t* sl);
boolean         SurfaceList_Iterate(surfacelist_t* sl,
                                    boolean (*callback) (surface_t*, void*),
                                    void* context);

void            R_MarkDependantSurfacesForDecorationUpdate(sector_t* sector);
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

void            R_FindSegSectionDivisions(walldiv_t* wdivs, hedge_t* hEdge,
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
#endif /* DOOMSDAY_REFRESH_WORLD_H */
