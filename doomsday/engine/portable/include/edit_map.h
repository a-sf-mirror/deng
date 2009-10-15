/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * edit_map.h: Public map creation/modification API.
 */

#ifndef DOOMSDAY_MAP_EDITOR_H
#define DOOMSDAY_MAP_EDITOR_H

#include "p_mapdata.h"
#include "m_binarytree.h"
#include "p_materialmanager.h"

extern gamemap_t* editMap;

boolean         MPE_Begin(const char* mapID);
boolean         MPE_End(void);

dmuobjrecordid_t MPE_VertexCreate(float x, float y);
boolean         MPE_VertexCreatev(size_t num, float* values, dmuobjrecordid_t* indices);
dmuobjrecordid_t MPE_SideDefCreate(dmuobjrecordid_t sector, short flags,
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
dmuobjrecordid_t MPE_LineDefCreate(dmuobjrecordid_t v1, dmuobjrecordid_t v2, uint frontSide,
                                   uint backSide, int flags);
dmuobjrecordid_t MPE_SectorCreate(float lightlevel, float red, float green, float blue);
void             MPE_PlaneCreate(dmuobjrecordid_t sector, float height,
                                 material_t* material,
                                 float matOffsetX, float matOffsetY,
                                 float r, float g, float b, float a,
                                 float normalX, float normalY, float normalZ);
dmuobjrecordid_t MPE_PolyobjCreate(dmuobjrecordid_t* lines, uint linecount,
                                   int tag, int sequenceType, float startX, float startY);

boolean         MPE_GameObjProperty(const char* objName, uint idx,
                                    const char* propName, valuetype_t type,
                                    void* data);

// Non-public (temporary)
// Flags for MPE_PruneRedundantMapData().
#define PRUNE_LINEDEFS      0x1
#define PRUNE_VERTEXES      0x2
#define PRUNE_SIDEDEFS      0x4
#define PRUNE_SECTORS       0x8
#define PRUNE_ALL           (PRUNE_LINEDEFS|PRUNE_VERTEXES|PRUNE_SIDEDEFS|PRUNE_SECTORS)

gamemap_t*      MPE_GetLastBuiltMap(void);
#endif /* DOOMSDAY_MAP_EDITOR_H */
