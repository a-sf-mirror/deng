/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008-2009 Daniel Swanson <danij@dengine.net>
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
 * edit_map.c: Public map editing interface.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static map_t* editMap = NULL;
static map_t* lastBuiltMap = NULL;

// CODE --------------------------------------------------------------------

/**
 * Called to begin the map building process.
 */
boolean MPE_Begin(const char* mapID)
{
    if(editMap)
        Con_Error("MPE_Begin: Error, already editing map %s.\n", editMap->mapID);

    editMap = P_CreateMap(mapID);

    return true;
}

/**
 * Called to complete the map building process.
 */
boolean MPE_End(void)
{
    if(!editMap)
        return false;

    Map_EditEnd(editMap);

    lastBuiltMap = editMap;
    editMap = NULL;

    return true;
}

map_t* MPE_GetLastBuiltMap(void)
{
    return lastBuiltMap;
}

/**
 * Create a new vertex in currently loaded editable map.
 *
 * @param x             X coordinate of the new vertex.
 * @param y             Y coordinate of the new vertex.
 *
 * @return              Index number of the newly created vertex else 0 if
 *                      the vertex could not be created for some reason.
 */
objectrecordid_t MPE_CreateVertex(float x, float y)
{
    vertex_t* v;
    map_t* map = editMap;

    if(!map)
        return 0;

    v = Map_CreateVertex(map, x, y);

    return v ? ((mvertex_t*) v->data)->index : 0;
}

/**
 * Create many new vertexs in the currently loaded editable map.
 *
 * @param num           Number of vertexes to be created.
 * @param values        Ptr to an array containing the coordinates for the
 *                      vertexs to create [v0 X, vo Y, v1 X, v1 Y...]
 * @param indices       If not @c NULL, the indices of the newly created
 *                      vertexes will be written back here.
 */
boolean MPE_CreateVertices(size_t num, float* values, objectrecordid_t* indices)
{
    uint n;
    map_t* map = editMap;

    if(!map)
        return false;
    if(!num || !values)
        return false;

    if(!map->editActive)
        return false;

    // Create many vertexes.
    for(n = 0; n < num; ++n)
    {
        vertex_t* v = Map_CreateVertex(map, values[n * 2], values[n * 2 + 1]);

        if(indices)
            indices[n] = ((mvertex_t*) v->data)->index;
    }

    return true;
}

objectrecordid_t MPE_CreateSideDef(objectrecordid_t sector, short flags,
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
                                   float bottomBlue)
{
    sidedef_t* s;
    map_t* map = editMap;

    if(!map)
        return 0;
    if(sector > map->numSectors)
        return 0;

    s = Map_CreateSideDef(map, (sector == 0? NULL: map->sectors[sector-1]), flags,
                          topMaterial? ((objectrecord_t*) topMaterial)->obj : NULL,
                          topOffsetX, topOffsetY, topRed, topGreen, topBlue,
                          middleMaterial? ((objectrecord_t*) middleMaterial)->obj : NULL,
                          middleOffsetX, middleOffsetY, middleRed, middleGreen, middleBlue,
                          middleAlpha, bottomMaterial? ((objectrecord_t*) bottomMaterial)->obj : NULL,
                          bottomOffsetX, bottomOffsetY, bottomRed, bottomGreen, bottomBlue);

    return s ? s->buildData.index : 0;
}

/**
 * Create a new linedef in the editable map.
 *
 * @param v1            Idx of the start vertex.
 * @param v2            Idx of the end vertex.
 * @param frontSide     Idx of the front sidedef.
 * @param backSide      Idx of the back sidedef.
 * @param flags         Currently unused.
 *
 * @return              Idx of the newly created linedef else @c 0 if there
 *                      was an error.
 */
objectrecordid_t MPE_CreateLineDef(objectrecordid_t v1, objectrecordid_t v2,
                                   uint frontSide, uint backSide, int flags)
{
    linedef_t* l;
    sidedef_t* front = NULL, *back = NULL;
    vertex_t* vtx1, *vtx2;
    float length, dx, dy;
    map_t* map = editMap;

    if(!map)
        return 0;
    if(frontSide > map->numSideDefs)
        return 0;
    if(backSide > map->numSideDefs)
        return 0;
    if(v1 == 0 || v1 > HalfEdgeDS_NumVertices(Map_HalfEdgeDS(map)))
        return 0;
    if(v2 == 0 || v2 > HalfEdgeDS_NumVertices(Map_HalfEdgeDS(map)))
        return 0;
    if(v1 == v2)
        return 0;

    // First, ensure that the side indices are unique.
    if(frontSide && map->sideDefs[frontSide - 1]->buildData.refCount)
        return 0; // Already in use.
    if(backSide && map->sideDefs[backSide - 1]->buildData.refCount)
        return 0; // Already in use.

    // Next, check the length is not zero.
    vtx1 = Map_HalfEdgeDS(map)->vertices[v1 - 1];
    vtx2 = Map_HalfEdgeDS(map)->vertices[v2 - 1];
    dx = vtx2->pos[VX] - vtx1->pos[VX];
    dy = vtx2->pos[VY] - vtx1->pos[VY];
    length = P_AccurateDistance(dx, dy);
    if(!(length > 0))
        return 0;

    if(frontSide > 0)
        front = map->sideDefs[frontSide - 1];
    if(backSide > 0)
        back = map->sideDefs[backSide - 1];

    l = Map_CreateLineDef(map, vtx1, vtx2, front, back);

    return l->buildData.index;
}

objectrecordid_t MPE_CreatePlane(float height, material_t* material, float matOffsetX,
                                 float matOffsetY, float r, float g, float b, float a,
                                 float normalX, float normalY, float normalZ)
{
    plane_t* p;
    map_t* map = editMap;

    if(!map)
        return 0;

    p = Map_CreatePlane(map, height, material, matOffsetX, matOffsetY,
                        r, g, b, a, normalX, normalY, normalZ);
    return p ? p->buildData.index : 0;
}

objectrecordid_t MPE_CreateSector(float lightLevel, float red, float green, float blue)
{
    sector_t* s;
    map_t* map = editMap;

    if(!map)
        return 0;

    s = Map_CreateSector(map, lightLevel, red, green, blue);

    return s ? s->buildData.index : 0;
}

void MPE_SetSectorPlane(objectrecordid_t sector, uint type, objectrecordid_t plane)
{
    map_t* map = editMap;
    sector_t* sec;
    plane_t* pln;
    objectrecord_t* obj;
    uint i;

    if(!map)
        return;
    if(sector > map->numSectors)
        return;
    if(plane > map->numPlanes)
        return;

    sec = map->sectors[sector - 1];

    // First check whether sector is already linked with this plane.
    for(i = 0; i < sec->planeCount; ++i)
    {
        if(sec->planes[i]->buildData.index == plane)
            return;
    }

    obj = (objectrecord_t*) P_ToPtr(DMU_PLANE, plane);

    if(sec->planeCount == 1 || obj == NULL)
        obj = NULL;

    pln = (plane_t*) ((objectrecord_t*) P_ToPtr(DMU_PLANE, plane))->obj;
    pln->type = type == 0? PLN_FLOOR : type == 1? PLN_CEILING : PLN_MID;

    sec->planes = Z_Realloc(sec->planes, sizeof(plane_t*) * (++sec->planeCount + 1), PU_STATIC);
    sec->planes[type == PLN_MID? sec->planeCount-1 : type] = pln;
    sec->planes[sec->planeCount] = NULL; // Terminate.
}

objectrecordid_t MPE_CreatePolyobj(objectrecordid_t* lines, uint lineCount, int tag,
                                   int sequenceType, float anchorX, float anchorY)
{
    uint i;
    polyobj_t* po;
    map_t* map = editMap;

    if(!map)
        return 0;
    if(!lineCount || !lines)
        return 0;

    // First check that all the line indices are valid and that they arn't
    // already part of another polyobj.
    for(i = 0; i < lineCount; ++i)
    {
        linedef_t* line;

        if(lines[i] == 0 || lines[i] > map->numLineDefs)
            return 0;

        line = map->lineDefs[lines[i] - 1];
        if(line->inFlags & LF_POLYOBJ)
            return 0;
    }

    po = Map_CreatePolyobj(map, lines, lineCount, tag, sequenceType, anchorX, anchorY);

    return po ? po->buildData.index : 0;
}

boolean MPE_GameObjectRecordProperty(const char* objName, uint idx,
                                     const char* propName, valuetype_t type,
                                     void* data)
{
    uint i;
    size_t len;
    def_gameobject_t* def;
    map_t* map = editMap;

    if(!map || !map->editActive)
        return false;
    if(!objName || !propName || !data)
        return false; // Hmm...

    // Is this a known object?
    if((def = P_GameObjectDef(0, objName, false)) == NULL)
        return false; // Nope.

    // Is this a known property?
    len = strlen(propName);
    for(i = 0; i < def->numProperties; ++i)
    {
        if(!strnicmp(propName, def->properties[i].name, len))
        {   // Found a match!
            // Create a record of this so that the game can query it later.
            GameObjRecords_Update(Map_GameObjectRecords(map), def, i, idx, type, data);
            return true; // We're done.
        }
    }

    // An unknown property.
    VERBOSE(Con_Message("MPE_GameObjectRecordProperty: %s has no property \"%s\".\n",
                        def->name, propName));

    return false;
}
