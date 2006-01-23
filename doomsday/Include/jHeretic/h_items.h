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

#ifndef __H_ITEMS__
#define __H_ITEMS__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "Doomdef.h"

#ifdef __GNUG__
#pragma interface
#endif

// Weapon info: sprite frames, ammunition use.
typedef struct {
    ammotype_t      ammo;
    int             upstate;
    int             downstate;
    int             readystate;
    int             atkstate;
    int             holdatkstate;
    int             flashstate;
} weaponinfo_t;

extern weaponinfo_t wpnlev1info[NUMWEAPONS];
extern weaponinfo_t wpnlev2info[NUMWEAPONS];

#define NUMINVENTORYSLOTS   14
typedef struct {
    int             type;
    int             count;
} inventory_t;

#endif
