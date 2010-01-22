/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_GAMEOBJRECORDS_H
#define DOOMSDAY_GAMEOBJRECORDS_H

#include "dd_share.h"

typedef struct {
    uint            num;
    struct valuetable_s**  tables;
} valuedb_t;

typedef struct gameobjrecords_s {
    uint            numNamespaces;
    struct gameobjrecord_namespace_s* namespaces;
    valuedb_t       values;
} gameobjrecords_t;

gameobjrecords_t* P_CreateGameObjectRecords(void);
void            P_DestroyGameObjectRecords(gameobjrecords_t* records);

void            GameObjRecords_Update(gameobjrecords_t* records, struct def_gameobject_s* def,
                                      uint propIdx, uint elmIdx, valuetype_t type,
                                      const void* data);
uint            GameObjRecords_Num(gameobjrecords_t* records, int typeIdentifier);
byte            GameObjRecords_GetByte(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier);
short           GameObjRecords_GetShort(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier);
int             GameObjRecords_GetInt(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier);
fixed_t         GameObjRecords_GetFixed(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier);
angle_t         GameObjRecords_GetAngle(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier);
float           GameObjRecords_GetFloat(gameobjrecords_t* records, int typeIdentifier, uint elmIdx, int propIdentifier);

#endif /* DOOMSDAY_GAMEOBJRECORDS_H */
