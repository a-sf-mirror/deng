/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_local.h: Play functions, animation, global header.
 */

#ifndef __P_LOCAL_H__
#define __P_LOCAL_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "p_spec.h"
#include "p_start.h"
#include "p_actor.h"
#include "p_xg.h"
#include "p_terraintype.h"

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS        (1)
#define STARTBONUSPALS      (9)
#define NUMREDPALS          (8)
#define NUMBONUSPALS        (4)

#define FLOATSPEED          (4)

#define DELTAMUL            (6.324555320) // Used when calculating ticcmd_t.lookdirdelta


#define MAXHEALTH           (maxHealth)  //100
#define VIEWHEIGHT          (41)

#define TOCENTER            (-8)

// player radius for movement checking
#define PLAYERRADIUS        (16)

// MAXRADIUS is for precalculated sector block boxes the spider demon is
// larger, but we do not have any moving sectors nearby.
#define MAXRADIUS           (32)
#define MAXMOVE             (30)

#define USERANGE            (64)
#define MELEERANGE          (64)
#define MISSILERANGE        (32*64)

#define BASETHRESHOLD       (100)

#define sentient(mobj)      ((mobj)->health > 0 && P_GetState((mobj)->type, SN_SEE))

#define FRICTION_NORMAL     (0.90625f)
#define FRICTION_FLY        (0.91796875f)
#define FRICTION_HIGH       (0.5f)

#define OPENRANGE           (*(float*) DD_GetVariable(DD_OPENRANGE))
#define OPENTOP             (*(float*) DD_GetVariable(DD_OPENTOP))
#define OPENBOTTOM          (*(float*) DD_GetVariable(DD_OPENBOTTOM))
#define LOWFLOOR            (*(float*) DD_GetVariable(DD_LOWFLOOR))

extern float turboMul;
extern int maxAmmo[NUM_AMMO_TYPES];
extern int clipAmmo[NUM_AMMO_TYPES];

void        P_SetupPsprites(player_t *plr);
void        P_MovePsprites(player_t *plr);
void        P_DropWeapon(player_t *plr);
void        P_SetPsprite(player_t *plr, int position, statenum_t stnum);

void        P_MobjRemove(mobj_t* mo, boolean noRespawn);
boolean     P_MobjChangeState(mobj_t* mo, statenum_t state);
void        P_MobjThinker(mobj_t* mo);
void        P_RipperBlood(mobj_t* mo);

void        P_SetDoomsdayFlags(mobj_t* mo);
void        P_HitFloor(mobj_t* mo);

void        P_SpawnMapThing(spawnspot_t* th);
void        P_SpawnPlayer(spawnspot_t* mthing, int pnum);

void        P_TouchSpecialMobj(mobj_t* special, mobj_t* toucher);

int         P_DamageMobj(mobj_t* target, mobj_t* inflictor,
                         mobj_t* source, int damage, boolean stomping);

void        P_ExplodeMissile(mobj_t* mo);

#endif
