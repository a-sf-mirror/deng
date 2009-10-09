/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * materials.h: Material collection.
 */

#ifndef DOOMSDAY_MATERIALS_H
#define DOOMSDAY_MATERIALS_H

#include "gl_texmanager.h"

extern materialnum_t numMaterialBinds;

void            Materials_Register(void);

void            Materials_Ticker(timespan_t time);

void            Materials_Init(void);
void            Materials_Shutdown(void);
void            Materials_DeleteTextures(material_namespace_t mnamespace);

material_t*     Materials_NewMaterial(material_namespace_t mnamespace,
                                      const char* name, short width,
                                      short height, byte flags,
                                      const ded_material_t* def,
                                      boolean isAutoMaterial);

const char*     Materials_NameOf(material_t* mat);
void            Materials_CacheMaterial(material_t* mat);
byte            Materials_Prepare(material_t* mat, byte flags,
                                  const struct material_prepare_params_s* params,
                                  struct material_snapshot_s* snapshot);

int             Materials_NewGroup(int flags);
int             Materials_NewGroupFromDefiniton(ded_group_t* def);

void            Materials_AddToGroup(int animGroupNum, int num, int tics,
                                     int randomTics);
boolean         Materials_IsInGroup(int animGroupNum, material_t* mat);
boolean         Materials_CacheGroup(int groupNum);

int             Materials_NumGroups(void);
void            Materials_RewindAnimationGroups(void);
void            Materials_DestroyGroups(void);

material_t*     Materials_ToMaterial(materialnum_t num);
material_t*     Materials_ToMaterial2(material_namespace_t mnamespace, int index);
materialnum_t   Materials_ToIndex(material_t* mat);

materialnum_t   Materials_CheckIndexForName(material_namespace_t mnamespace, const char* name);
materialnum_t   Materials_IndexForName(material_namespace_t mnamespace, const char* name);

// Public API access:
struct material_s* P_MaterialForName(material_namespace_t mnamespace, const char* name);
#endif /* DOOMSDAY_MATERIALS_H */
