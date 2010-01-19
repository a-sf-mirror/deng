/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * p_spec.h:
 */

#ifndef __P_SPEC_H__
#define __P_SPEC_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#define MO_TELEPORTMAN          14

void        GameMap_InitLava(struct map_s* map);

boolean     P_ExecuteLineSpecial(int special, byte* args, linedef_t* line, int side, mobj_t* mo);
boolean     P_StartLockedACS(linedef_t* line, byte* args, mobj_t* mo, int side);
boolean     P_ActivateLine(linedef_t* ld, mobj_t* mo, int side, int activationType);

void        P_PlayerInSpecialSector(player_t* plr);
void        P_PlayerOnSpecialFloor(player_t* plr);

void        P_InitLightning(struct map_s* map);
void        P_ForceLightning(void);

typedef enum {
    ok,
    crushed,
    pastdest
} result_e;

typedef enum {
    STAIRS_NORMAL,
    STAIRS_SYNC,
    STAIRS_PHASED
} stairs_e;

result_e    T_MovePlane(sector_t* sector, float speed, float dest, int crush, int floorOrCeiling, int direction);

int         EV_BuildStairs(linedef_t* line, byte* args, int direction, stairs_e type);
int         EV_FloorCrushStop(linedef_t* line, byte* args);

#define TELEFOGHEIGHTF          (32)

boolean     P_Teleport(mobj_t* mo, float x, float y, angle_t angle, boolean useFog);
boolean     EV_Teleport(int tid, mobj_t* mo, boolean fog);
void        P_ArtiTele(player_t* player);

mobjtype_t  P_MapScriptThingIdToMobjType(int thingId);

boolean     EV_ThingProjectile(struct map_s* map, byte* args, boolean gravity);
boolean     EV_ThingSpawn(struct map_s* map, byte* args, boolean fog);
boolean     EV_ThingActivate(struct map_s* map, int tid);
boolean     EV_ThingDeactivate(struct map_s* map, int tid);
boolean     EV_ThingRemove(struct map_s* map, int tid);
boolean     EV_ThingDestroy(struct map_s* map, int tid);

void        P_InitSky(int map);

#endif
