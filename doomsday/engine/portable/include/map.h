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

#include "dd_share.h"
#include "m_vector.h"
#include "mobjblockmap.h"
#include "linedefblockmap.h"
#include "subsectorblockmap.h"
#include "particleblockmap.h"
#include "lumobjblockmap.h"
#include "halfedgeds.h"
#include "m_binarytree.h"
#include "gameobjrecords.h"
#include "p_maptypes.h"
#include "m_nodepile.h"
#include "p_polyob.h"
#include "rend_bias.h"
#include "p_think.h"
#include "p_particle.h"

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

#define GBF_CHANGED     0x1     // Grid block sector light has changed.
#define GBF_CONTRIBUTOR 0x2     // Contributes light to a changed block.

typedef struct lgridblock_s {
    struct sector_s *sector;

    byte        flags;

    // Positive bias means that the light is shining in the floor of
    // the sector.
    char        bias;

    // Color of the light:
    float       rgb[3];
    float       oldRGB[3];  // Used instead of rgb if the lighting in this
                            // block has changed and we haven't yet done a
                            // a full grid update.
} lgridblock_t;

typedef struct shadowlink_s {
    struct shadowlink_s* next;
    linedef_t*      lineDef;
    byte            side;
} shadowlink_t;

typedef struct map_s {
    char            mapID[9];
    char            uniqueID[256];
    boolean         editActive;

    thinkers_t*     _thinkers;
    gameobjrecords_t* _gameObjectRecords;

    halfedgeds_t*   _halfEdgeDS;
    binarytree_t*   _rootNode;

    mobjblockmap_t* _mobjBlockmap;
    linedefblockmap_t* _lineDefBlockmap;
    subsectorblockmap_t* _subsectorBlockmap;

    // Following blockmaps are emptied each render frame.
    particleblockmap_t* _particleBlockmap;
    lumobjblockmap_t* _lumobjBlockmap;

    objcontactlist_t* _subsectorContacts; // List of obj contacts for each subsector.

    float           bBox[4];
    uint            numSectors;
    sector_t**      sectors;

    uint            numLineDefs;
    linedef_t**     lineDefs;

    uint            numSideDefs;
    sidedef_t**     sideDefs;

    uint            numNodes;
    node_t**        nodes;

    uint            numSubsectors;
    subsector_t**   subsectors;

    uint            numSegs;
    seg_t**         segs;

    uint            numPolyObjs;
    polyobj_t**     polyObjs;

    lineowner_t*    lineOwners;

    planelist_t     watchedPlaneList;
    surfacelist_t   movingSurfaceList;
    surfacelist_t   decoratedSurfaceList;

    nodepile_t*     mobjNodes, *lineNodes; // All kinds of wacky links.
    nodeindex_t*    lineLinks; // Indices to roots.

    float           globalGravity;
    int             ambientLightLevel;

    float           skyFixCeiling;
    float           skyFixFloor;

    struct {
        dynlist_t       linkList; // Surface-projected lumobjs (dynlights).
    } dlights;

    struct {
        unsigned int    lastChangeOnFrame;

        int             numSources, numSourceDelta;
        source_t        sources[MAX_BIAS_LIGHTS];

        biassurface_t*  surfaces; // Head of the biassurface list.

        int             editGrabbedID;
    } bias;

    struct {
        boolean         inited, needsUpdate;

        float           origin[3];
        int             blockWidth, blockHeight;
        lgridblock_t*   grid;
    } lg;
} map_t;

map_t*          P_CreateMap(const char* mapID);
void            P_DestroyMap(map_t* map);

const char*     Map_ID(map_t* map);
const char*     Map_UniqueName(map_t* map);
void            Map_Bounds(map_t* map, float* min, float* max);
int             Map_AmbientLightLevel(map_t* map);

void            Map_BeginFrame(map_t* map, boolean resetNextViewer);
void            Map_EndFrame(map_t* map);

void            Map_Update(map_t* map);
void            Map_UpdateWatchedPlanes(map_t* map);
void            Map_UpdateMovingSurfaces(map_t* map);
void            Map_UpdateSkyFixForSector(map_t* map, uint secIDX);

void            Map_LinkMobj(map_t* map, struct mobj_s* mo, byte flags);
int             Map_UnlinkMobj(map_t* map, struct mobj_s* mo);

void            Map_AddThinker(map_t* map, thinker_t* th, boolean makePublic);
void            Map_RemoveThinker(map_t* map, thinker_t* th);

/**
 * @defgroup iterateThinkerFlags Iterate Thinker Flags
 * Used with Map_IterateThinkers to specify which thinkers to iterate.
 */
/*@{*/
#define ITF_PUBLIC          0x1
#define ITF_PRIVATE         0x2
/*@}*/

boolean         Map_IterateThinkers(map_t* map, think_t func, byte flags,
                                    int (*callback) (void* p, void*),
                                    void* context);

void            Map_AddSubsectorContact(map_t* map, uint subsectorIdx, objcontacttype_t type, void* obj);
boolean         Map_IterateSubsectorContacts(map_t* map, uint subsectorIdx, objcontacttype_t type,
                                             boolean (*func) (void*, void*),
                                             void* data);
/**
 * Map Edit interface.
 */
void            Map_EditEnd(map_t* map);
vertex_t*       Map_CreateVertex(map_t* map, float x, float y);
linedef_t*      Map_CreateLineDef(map_t* map, vertex_t* vtx1, vertex_t* vtx2,
                                  sidedef_t* front, sidedef_t* back);
sidedef_t*      Map_CreateSideDef(map_t* map, sector_t* sector, short flags, material_t* topMaterial,
                                  float topOffsetX, float topOffsetY, float topRed, float topGreen,
                                  float topBlue, material_t* middleMaterial, float middleOffsetX,
                                  float middleOffsetY, float middleRed, float middleGreen, float middleBlue,
                                  float middleAlpha, material_t* bottomMaterial, float bottomOffsetX,
                                  float bottomOffsetY, float bottomRed, float bottomGreen, float bottomBlue);
sector_t*       Map_CreateSector(map_t* map, float lightLevel, float red, float green, float blue);
void            Map_CreatePlane(map_t* map, sector_t* sector, float height, material_t* material,
                                float matOffsetX, float matOffsetY, float r, float g, float b, float a,
                                float normalX, float normalY, float normalZ);
polyobj_t*      Map_CreatePolyobj(map_t* map, objectrecordid_t* lines, uint lineCount, int tag,
                                  int sequenceType, float anchorX, float anchorY);

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
boolean         Map_SubsectorsBoxIterator(map_t* map, const float box[4], sector_t* sector,
                                          boolean (*func) (subsector_t*, void*),
                                          void* parm, boolean retObjRecord);
boolean         Map_SubsectorsBoxIteratorv(map_t* map, const arvec2_t box, sector_t* sector,
                                           boolean (*func) (subsector_t*, void*),
                                           void* data, boolean retObjRecord);

boolean         Map_PathTraverse(map_t* map, float x1, float y1, float x2, float y2,
                                 int flags, boolean (*trav) (intercept_t*));

// @todo the following should be Map private:
thinkers_t*     Map_Thinkers(map_t* map);
halfedgeds_t*   Map_HalfEdgeDS(map_t* map);
mobjblockmap_t* Map_MobjBlockmap(map_t* map);
linedefblockmap_t* Map_LineDefBlockmap(map_t* map);
subsectorblockmap_t* Map_SubsectorBlockmap(map_t* map);
particleblockmap_t* Map_ParticleBlockmap(map_t* map);
lumobjblockmap_t* Map_LumobjBlockmap(map_t* map);

void            Map_InitLinks(map_t* map);
void            Map_InitSkyFix(map_t* map);
void            Map_InitSoundEnvironment(map_t* map);

gameobjrecords_t* Map_GameObjectRecords(map_t* map);
void            Map_DestroyGameObjectRecords(map_t* map);

// Old public interface:
void            DD_InitThinkers(void);
void            DD_RunThinkers(void);
void            DD_ThinkerAdd(thinker_t* th);
void            DD_ThinkerRemove(thinker_t* th);
void            DD_ThinkerSetStasis(thinker_t* th, boolean on);
#endif /* DOOMSDAY_MAP_H */
