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
 * Items: key cards, artifacts, weapon, ammunition.
 */

#ifndef __D_ITEMS__
#define __D_ITEMS__

#include "doomdef.h"

#ifdef __GNUG__
#pragma interface
#endif

// Weapon info: sprite frames, ammunition use.
typedef struct {
    ammotype_t      ammo;
    int             pershot;       // Ammo used per shot.
    int             upstate;
    int             downstate;
    int             readystate;
    int             atkstate;
    int             flashstate;
    int             static_switch; // Weapon is not lowered during switch.
} weaponinfo_t;

extern weaponinfo_t weaponinfo[NUMWEAPONS];

void            P_InitWeaponInfo(void);

#endif
