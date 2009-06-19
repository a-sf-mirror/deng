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

void        P_InitLava(void);

void        P_SpawnSpecials(void);
void        P_UpdateSpecials(void);

boolean     P_ExecuteLineSpecial(int special, byte *args, linedef_t *line,
                                 int side, mobj_t *mo);
boolean     P_ActivateLine(linedef_t *ld, mobj_t *mo, int side,
                           int activationType);

void        P_PlayerInSpecialSector(player_t *plr);
void        P_PlayerOnSpecialFloor(player_t *plr);

void        P_AnimateSurfaces(void);
void        P_InitPicAnims(void);
void        P_InitLightning(void);
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

result_e    T_MovePlane(sector_t *sector, float speed, float dest,
                        int crush, int floorOrCeiling, int direction);

int         EV_BuildStairs(linedef_t *line, byte *args, int direction,
                           stairs_e type);
int         EV_FloorCrushStop(linedef_t *line, byte *args);

#define TELEFOGHEIGHTF          (32)

boolean     P_Teleport(mobj_t *mo, float x, float y, angle_t angle,
                       boolean useFog);
boolean     EV_Teleport(int tid, mobj_t *thing, boolean fog);
void        P_ArtiTele(player_t *player);

extern mobjtype_t TranslateThingType[];

boolean     EV_ThingProjectile(byte *args, boolean gravity);
boolean     EV_ThingSpawn(byte *args, boolean fog);
boolean     EV_ThingActivate(int tid);
boolean     EV_ThingDeactivate(int tid);
boolean     EV_ThingRemove(int tid);
boolean     EV_ThingDestroy(int tid);

void        P_InitSky(int map);

#endif
