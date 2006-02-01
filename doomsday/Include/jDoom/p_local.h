/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Play functions, animation, global header.
 */

#ifndef __P_LOCAL__
#define __P_LOCAL__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#ifndef __R_LOCAL__
#include "r_local.h"
#endif

#include "p_spec.h"
#include "p_start.h"
#include "p_actor.h"
#include "p_xg.h"

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS        1
#define STARTBONUSPALS      9
#define NUMREDPALS          8
#define NUMBONUSPALS        4

#define FLOATSPEED      (FRACUNIT*4)

#define DELTAMUL        6.324555320 // Used when calculating ticcmd_t.lookdirdelta


#define MAXHEALTH       maxhealth  //100
#define VIEWHEIGHT      (41*FRACUNIT)

#define TOCENTER        -8

// player radius for movement checking
#define PLAYERRADIUS    16*FRACUNIT

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       32*FRACUNIT

#define GRAVITY     Get(DD_GRAVITY) //FRACUNIT
#define MAXMOVE     (30*FRACUNIT)

#define USERANGE        (64*FRACUNIT)
#define MELEERANGE      (64*FRACUNIT)
#define MISSILERANGE    (32*64*FRACUNIT)

// follow a player exlusively for 3 seconds
#define BASETHRESHOLD       100


// GMJ 02/02/02
#define sentient(mobj) ((mobj)->health > 0 && (mobj)->info->seestate)

//
// P_TICK
//

#define thinkercap      (*gi.thinkercap)

extern int      TimerGame;         // tic countdown for deathmatch

//
// P_PSPR
//
void            P_SetupPsprites(player_t *curplayer);
void            P_MovePsprites(player_t *curplayer);
void            P_DropWeapon(player_t *player);
void            P_SetPsprite(player_t *player, int position, statenum_t stnum);

//
// P_USER
//
void            P_PlayerThink(player_t *player);
void            P_SetMessage(player_t *pl, char *msg);

extern int      maxhealth, healthlimit, godmodehealth;
extern int      soulspherehealth, soulspherelimit, megaspherehealth;
extern int      armorpoints[4];    // Green, blue, IDFA and IDKFA points.
extern int      armorclass[4];     // Green and blue classes.

//
// P_MOBJ
//
#define ONFLOORZ        MININT
#define ONCEILINGZ      MAXINT

// Time interval for item respawning.
#define ITEMQUESIZE     128

extern thing_t  itemrespawnque[ITEMQUESIZE];
extern int      itemrespawntime[ITEMQUESIZE];
extern int      iquehead;
extern int      iquetail;

void            P_RespawnSpecials(void);

mobj_t         *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);

void            P_RemoveMobj(mobj_t *th);
boolean         P_SetMobjState(mobj_t *mobj, statenum_t state);
void            P_MobjThinker(mobj_t *mobj);

void            P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z);
mobj_t         *P_SpawnCustomPuff(fixed_t x, fixed_t y, fixed_t z,
                                  mobjtype_t type);
void            P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage);
mobj_t         *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type);
void            P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type);
void            P_SpawnPlayer(thing_t * mthing, int pnum);
mobj_t         *P_SpawnTeleFog(int x, int y);

void            P_SetDoomsdayFlags(mobj_t *mo);

//
// P_ENEMY
//
void            P_NoiseAlert(mobj_t *target, mobj_t *emmiter);

/* killough 3/26/98: spawn icon landings */
void P_SpawnBrainTargets(void);

/* killough 3/26/98: global state of boss brain */
extern struct brain_s {
    int easy;
    int targeton;
} brain;

/* Removed fixed limit on number of brain targets */
extern mobj_t  **braintargets;
extern int      numbraintargets;
extern int      numbraintargets_alloc;

//
// P_MAPUTL
//

#define openrange           Get(DD_OPENRANGE)
#define opentop             Get(DD_OPENTOP)
#define openbottom          Get(DD_OPENBOTTOM)
#define lowfloor            Get(DD_LOWFLOOR)

void            P_UnsetThingPosition(mobj_t *thing);
void            P_SetThingPosition(mobj_t *thing);

int             P_Massacre(void);

//
// P_MAP
//

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
extern boolean  floatok;
extern fixed_t  tmfloorz;
extern fixed_t  tmceilingz;

extern line_t  *ceilingline;

boolean        P_CheckSides(mobj_t* actor, int x, int y);   // DJS - from prBoom

boolean         P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y);
boolean         P_CheckPosition2(mobj_t *thing, fixed_t x, fixed_t y,
                                 fixed_t z);
boolean         P_TryMove(mobj_t *thing, fixed_t x, fixed_t y,
                          boolean dropoff);
boolean         P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y, boolean alwaysstomp);
void            P_SlideMove(mobj_t *mo);

void            P_UseLines(player_t *player);

boolean         P_ChangeSector(sector_t *sector, boolean crunch);

extern mobj_t  *linetarget;        // who got hit (or NULL)

fixed_t         P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance);

void            P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance,
                             fixed_t slope, int damage);

void            P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage);

//
// P_INTER
//
extern int      maxammo[NUMAMMO];
extern int      clipammo[NUMAMMO];

void            P_TouchSpecialThing(mobj_t *special, mobj_t *toucher);

void            P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                             int damage);
void            P_DamageMobj2(mobj_t *target, mobj_t *inflictor,
                              mobj_t *source, int damage, boolean stomping);

void            P_ExplodeMissile(mobj_t *mo);

#endif
