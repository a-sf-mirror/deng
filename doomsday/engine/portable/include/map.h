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
#include "halfedgeds.h"
#include "p_maptypes.h"
#include "m_nodepile.h"
#include "p_polyob.h"
#include "rend_bias.h"

// Return the index of plane within a sector's planes array.
#define GET_PLANE_IDX(pln)  ((pln) - (pln)->sector->planes[0])

// Node flags.
#define NF_SUBSECTOR        0x80000000

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

typedef struct skyfix_s {
    float           height;
} skyfix_t;

// Map objects.
typedef struct {
    uint            idx;
    valuetype_t     type;
    uint            valueIdx;
} gameobjectrecord_property_t;

typedef struct {
    uint            elmIdx;
    uint            numProperties;
    gameobjectrecord_property_t* properties;
} gameobjectrecord_t;

typedef struct {
    struct def_gameobject_s* def;
    uint            numRecords;
    gameobjectrecord_t** records;
} gameobjectrecordnamespace_t;

typedef struct {
    valuetype_t     type;
    uint            numElements;
    void*           data;
} valuetable_t;

typedef struct {
    uint            num;
    valuetable_t**  tables;
} valuedb_t;

typedef struct {
    uint            numNamespaces;
    gameobjectrecordnamespace_t* namespaces;
    valuedb_t       values;
} gameobjectrecordset_t;

typedef struct shadowlink_s {
    struct shadowlink_s* next;
    linedef_t*      lineDef;
    byte            side;
} shadowlink_t;

typedef struct {
    uint            numLineOwners;
    lineowner_t*    lineOwners; // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.
} vertexinfo_t;

extern nodeindex_t* linelinks;
extern nodepile_t* mobjNodes, *lineNodes;

typedef struct gamemap_s {
    char            mapID[9];
    char            uniqueID[256];

    halfedgeds_t    _halfEdgeDS;

    mobjblockmap_t* _mobjBlockmap;
    linedefblockmap_t* _lineDefBlockmap;
    subsectorblockmap_t* _subsectorBlockmap;

    gameobjectrecordset_t _gameObjectRecordSet;

    boolean         editActive;
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

    planelist_t     watchedPlaneList;
    surfacelist_t   movingSurfaceList;
    surfacelist_t   decoratedSurfaceList;

    nodepile_t      mobjNodes, lineNodes; // All kinds of wacky links.
    nodeindex_t*    lineLinks; // Indices to roots.

    float           globalGravity; // Gravity for the current map.
    int             ambientLightLevel; // Ambient lightlevel for the current map.

    skyfix_t        skyFix[2];

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
} gamemap_t;

gamemap_t*      P_CreateMap(const char* mapID);
void            P_DestroyMap(gamemap_t* map);

const char*     Map_ID(gamemap_t* map);
const char*     Map_UniqueName(gamemap_t* map);
void            Map_Bounds(gamemap_t* map, float* min, float* max);
int             Map_AmbientLightLevel(gamemap_t* map);

void            Map_LinkMobj(gamemap_t* map, struct mobj_s* mo, byte flags);
int             Map_UnlinkMobj(gamemap_t* map, struct mobj_s* mo);

/**
 * Map Edit interface.
 */
void            Map_EditEnd(gamemap_t* map);
vertex_t*       Map_CreateVertex(gamemap_t* map, float x, float y);
linedef_t*      Map_CreateLineDef(gamemap_t* map, vertex_t* vtx1, vertex_t* vtx2,
                                  sidedef_t* front, sidedef_t* back);
sidedef_t*      Map_CreateSideDef(gamemap_t* map, sector_t* sector, short flags, material_t* topMaterial,
                                  float topOffsetX, float topOffsetY, float topRed, float topGreen,
                                  float topBlue, material_t* middleMaterial, float middleOffsetX,
                                  float middleOffsetY, float middleRed, float middleGreen, float middleBlue,
                                  float middleAlpha, material_t* bottomMaterial, float bottomOffsetX,
                                  float bottomOffsetY, float bottomRed, float bottomGreen, float bottomBlue);
sector_t*       Map_CreateSector(gamemap_t* map, float lightLevel, float red, float green, float blue);
void            Map_CreatePlane(gamemap_t* map, sector_t* sector, float height, material_t* material,
                                float matOffsetX, float matOffsetY, float r, float g, float b, float a,
                                float normalX, float normalY, float normalZ);
polyobj_t*      Map_CreatePolyobj(gamemap_t* map, objectrecordid_t* lines, uint lineCount, int tag,
                                  int sequenceType, float anchorX, float anchorY);

// Mobjs in bounding box iterators.
boolean         Map_MobjsBoxIterator(struct gamemap_s* map, const float box[4],
                                     boolean (*func) (struct mobj_s*, void*),
                                     void* data);

boolean         Map_MobjsBoxIteratorv(struct gamemap_s* map, const arvec2_t box,
                                      boolean (*func) (struct mobj_s*, void*),
                                      void* data);

// LineDefs in bounding box iterators:
boolean         Map_LineDefsBoxIteratorv(struct gamemap_s* map, const arvec2_t box,
                                         boolean (*func) (linedef_t*, void*),
                                         void* data, boolean retObjRecord);

// Subsectors in bounding box iterators:
boolean         Map_SubsectorsBoxIterator(struct gamemap_s* map, const float box[4], sector_t* sector,
                                          boolean (*func) (subsector_t*, void*),
                                          void* parm, boolean retObjRecord);
boolean         Map_SubsectorsBoxIteratorv(struct gamemap_s* map, const arvec2_t box, sector_t* sector,
                                           boolean (*func) (subsector_t*, void*),
                                           void* data, boolean retObjRecord);

boolean         Map_PathTraverse(struct gamemap_s* map, float x1, float y1, float x2, float y2,
                                 int flags, boolean (*trav) (intercept_t*));

// @todo Push this data game-side:
void            Map_UpdateGameObjectRecord(gamemap_t* map, struct def_gameobject_s* def,
                                           uint propIdx, uint elmIdx, valuetype_t type,
                                           void* data);
void            Map_DestroyGameObjectRecords(gamemap_t* map);
uint            Map_NumGameObjectRecords(gamemap_t* map, int typeIdentifier);
byte            Map_GameObjectRecordByte(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier);
short           Map_GameObjectRecordShort(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier);
int             Map_GameObjectRecordInt(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier);
fixed_t         Map_GameObjectRecordFixed(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier);
angle_t         Map_GameObjectRecordAngle(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier);
float           Map_GameObjectRecordFloat(gamemap_t* map, int typeIdentifier, uint elmIdx, int propIdentifier);

// @todo the following should be Map private:
halfedgeds_t*   Map_HalfEdgeDS(gamemap_t* map);
mobjblockmap_t* Map_MobjBlockmap(gamemap_t* map);
linedefblockmap_t* Map_LineDefBlockmap(gamemap_t* map);
subsectorblockmap_t* Map_SubsectorBlockmap(gamemap_t* map);

void            Map_BuildMobjBlockmap(gamemap_t* map);
void            Map_BuildLineDefBlockmap(gamemap_t* map);
void            Map_BuildSubsectorBlockmap(gamemap_t* map);
void            Map_InitSoundEnvironment(gamemap_t* map);
#endif /* DOOMSDAY_MAP_H */
