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

#ifndef DOOMSDAY_MAP_H
#define DOOMSDAY_MAP_H

#if defined(__JDOOM__) || defined(__JHERETIC__) || defined(__JHEXEN__)
#  error "Attempted to include internal Doomsday map.h from a game"
#endif

#include "dd_share.h"
#include "m_vector.h"
#include "halfedgeds.h"
#include "mobjblockmap.h"
#include "linedefblockmap.h"
#include "subsectorblockmap.h"
#include "particleblockmap.h"
#include "lumobjblockmap.h"
#include "m_binarytree.h"
#include "gameobjrecords.h"
#include "p_maptypes.h"
#include "m_nodepile.h"
#include "p_polyob.h"
#include "rend_bias.h"
#include "p_think.h"
#include "p_particle.h"
#include "r_lgrid.h"

// Return the index of plane within a sector's planes array.
#define GET_PLANE_IDX(pln)  ((pln) - (pln)->sector->planes[0])

typedef struct fvertex_s {
    float           pos[2];
} fvertex_t;

typedef struct planelistnode_s {
    void*           data;
    struct planelistnode_s* next;
} planelistnode_t;

typedef struct planelist_s {
    uint            num;
    planelistnode_t* head;
} planelist_t;

typedef struct surfacelistnode_s {
    void*           data;
    struct surfacelistnode_s* next;
} surfacelistnode_t;

typedef struct surfacelist_s {
    uint            num;
    surfacelistnode_t* head;
} surfacelist_t;

/**
 * Stores the data pertaining to vertex lighting for a worldmap, surface.
 */
typedef struct biassurface_s {
    uint            updated;
    uint            size;
    vertexillum_t*  illum; // [size]
    biastracker_t   tracker;
    biasaffection_t affected[MAX_BIAS_AFFECTED];

    struct biassurface_s* next;
} biassurface_t;

typedef struct dynlist_s {
    struct dynlistnode_s* head, *tail;
} dynlist_t;

typedef enum {
    OCT_FIRST = 0,
    OCT_MOBJ = OCT_FIRST,
    OCT_LUMOBJ,
    OCT_PARTICLE,
    NUM_OBJCONTACT_TYPES
} objcontacttype_t;

typedef struct objcontact_s {
    struct objcontact_s* next; // Next in the subsector.
    struct objcontact_s* nextUsed; // Next used contact.
    void*           obj;
} objcontact_t;

typedef struct objcontactlist_s {
    objcontact_t*   head[NUM_OBJCONTACT_TYPES];
} objcontactlist_t;

typedef struct shadowlink_s {
    struct shadowlink_s* next;
    linedef_t*      lineDef;
    byte            side;
} shadowlink_t;

// We'll use the base map template directly as our map.
typedef struct map_s {
DD_BASE_MAP_ELEMENTS()} map_t;

#define MAP_SIZE            gx.mapSize

extern int bspFactor;

void            P_MapRegister(void);

map_t*          P_CreateMap(const char* mapID);
void            P_DestroyMap(map_t* map);

boolean         Map_Load(map_t* map);
void            Map_Precache(map_t* map);

const char*     Map_ID(map_t* map);
const char*     Map_UniqueName(map_t* map);
void            Map_Bounds(map_t* map, float* min, float* max);
int             Map_AmbientLightLevel(map_t* map);

uint            Map_NumSectors(map_t* map);
uint            Map_NumLineDefs(map_t* map);
uint            Map_NumSideDefs(map_t* map);
uint            Map_NumVertexes(map_t* map);
uint            Map_NumPolyobjs(map_t* map);
uint            Map_NumSegs(map_t* map);
uint            Map_NumSubsectors(map_t* map);
uint            Map_NumNodes(map_t* map);
uint            Map_NumPlanes(map_t* map);

void            Map_BeginFrame(map_t* map, boolean resetNextViewer);
void            Map_EndFrame(map_t* map);

void            Map_Ticker(map_t* map, timespan_t time);

void            Map_Update(map_t* map);
void            Map_UpdateWatchedPlanes(map_t* map);
void            Map_UpdateMovingSurfaces(map_t* map);
void            Map_UpdateSkyFixForSector(map_t* map, uint secIDX);

void            Map_LinkMobj(map_t* map, struct mobj_s* mo, byte flags);
int             Map_UnlinkMobj(map_t* map, struct mobj_s* mo);

void            Map_AddThinker(map_t* map, thinker_t* th, boolean makePublic);
void            Map_RemoveThinker(map_t* map, thinker_t* th);
void            Map_ThinkerSetStasis(map_t* map, thinker_t* th, boolean on);

/**
 * @defgroup iterateThinkerFlags Iterate Thinker Flags
 * Used with Map_IterateThinkers2 to specify which thinkers to iterate.
 */
/*@{*/
#define ITF_PUBLIC          0x1
#define ITF_PRIVATE         0x2
/*@}*/

boolean         Map_IterateThinkers2(map_t* map, think_t func, byte flags,
                                    int (*callback) (void* p, void*),
                                    void* context);

void            Map_AddSubsectorContact(map_t* map, uint subsectorIdx, objcontacttype_t type, void* obj);
boolean         Map_IterateSubsectorContacts(map_t* map, uint subsectorIdx, objcontacttype_t type,
                                             boolean (*func) (void*, void*),
                                             void* data);
/**
 * Map Edit interface.
 */
boolean         Map_EditEnd(map_t* map);

objectrecordid_t Map_CreateVertex(map_t* map, float x, float y);
boolean         Map_CreateVertices(map_t* map, size_t num, float* values, objectrecordid_t* indices);
objectrecordid_t Map_CreateSideDef(map_t* map, objectrecordid_t sector, short flags,
                                   material_t* topMaterial,
                                   float topOffsetX, float topOffsetY, float topRed,
                                   float topGreen, float topBlue,
                                   material_t* middleMaterial,
                                   float middleOffsetX, float middleOffsetY,
                                   float middleRed, float middleGreen,
                                   float middleBlue, float middleAlpha,
                                   material_t* bottomMaterial,
                                   float bottomOffsetX, float bottomOffsetY,
                                   float bottomRed, float bottomGreen,
                                   float bottomBlue);
objectrecordid_t Map_CreateLineDef(map_t* map, objectrecordid_t v1, objectrecordid_t v2, uint frontSide,
                                   uint backSide, int flags);
objectrecordid_t Map_CreateSector(map_t* map, float lightlevel, float red, float green, float blue);
objectrecordid_t Map_CreatePlane(map_t* map, float height, material_t* material,
                                 float matOffsetX, float matOffsetY, float r, float g, float b, float a,
                                 float normalX, float normalY, float normalZ);
objectrecordid_t Map_CreatePolyobj(map_t* map, objectrecordid_t* lines, uint linecount,
                                   int tag, int sequenceType, float startX, float startY);

boolean          Map_GameObjectRecordProperty(map_t* map, const char* objName, uint idx,
                                              const char* propName, valuetype_t type,
                                              void* data);

// Mobjs in bounding box iterators.
boolean         Map_MobjsBoxIterator(map_t* map, const float box[4],
                                     boolean (*func) (struct mobj_s*, void*),
                                     void* data);

boolean         Map_MobjsBoxIteratorv(map_t* map, const arvec2_t box,
                                      boolean (*func) (struct mobj_s*, void*),
                                      void* data);

// LineDefs in bounding box iterators:
boolean         Map_LineDefsBoxIteratorv(map_t* map, const arvec2_t box,
                                         boolean (*func) (linedef_t*, void*),
                                         void* data, boolean retObjRecord);

// Subsectors in bounding box iterators:
boolean         Map_SubsectorsBoxIterator2(map_t* map, const float box[4], sector_t* sector,
                                          boolean (*func) (subsector_t*, void*),
                                          void* parm, boolean retObjRecord);
boolean         Map_SubsectorsBoxIteratorv(map_t* map, const arvec2_t box, sector_t* sector,
                                           boolean (*func) (subsector_t*, void*),
                                           void* data, boolean retObjRecord);

boolean         Map_PathTraverse(map_t* map, float x1, float y1, float x2, float y2,
                                 int flags, boolean (*trav) (intercept_t*));
boolean         Map_CheckLineSight(map_t* map, const float from[3], const float to[3],
                                   float bottomSlope, float topSlope, int flags);

subsector_t*    Map_PointInSubsector2(map_t* map, float x, float y);
sector_t*       Map_SectorForOrigin(map_t* map, const void* ddMobjBase);

polyobj_t*      Map_PolyobjForOrigin(map_t* map, const void* ddMobjBase);
polyobj_t*      Map_Polyobj(map_t* map, uint num);

// @todo the following should be Map private:
thinkers_t*     Map_Thinkers(map_t* map);
halfedgeds_t*   Map_HalfEdgeDS(map_t* map);
mobjblockmap_t* Map_MobjBlockmap(map_t* map);
linedefblockmap_t* Map_LineDefBlockmap(map_t* map);
subsectorblockmap_t* Map_SubsectorBlockmap(map_t* map);
particleblockmap_t* Map_ParticleBlockmap(map_t* map);
lumobjblockmap_t* Map_LumobjBlockmap(map_t* map);
lightgrid_t*    Map_LightGrid(map_t* map);

// protected
seg_t*          Map_CreateSeg(map_t* map, linedef_t* lineDef, byte side, hedge_t* hEdge);
subsector_t*    Map_CreateSubsector(map_t* map, face_t* face, sector_t* sector);
node_t*         Map_CreateNode(map_t* map, float x, float y, float dX, float dY, float rightAABB[4], float leftAABB[4]);

void            Map_InitSkyFix(map_t* map);
void            Map_MarkAllSectorsForLightGridUpdate(map_t* map);

gameobjrecords_t* Map_GameObjectRecords(map_t* map);
void            Map_DestroyGameObjectRecords(map_t* map);

// Old public interface:
void            Map_InitThinkers(map_t* map);
void            Map_RunThinkers(map_t* map);
void            Map_ThinkerAdd(map_t* map, thinker_t* th);

#endif /* DOOMSDAY_MAP_H */
