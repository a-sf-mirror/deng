/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * D_Action.h: Game controls and system actions.
 */

#ifndef __JDOOM_ACTIONS_H__
#define __JDOOM_ACTIONS_H__

#include "../dd_share.h"

// These must correspond the action_t's in the actions array!
// Info needed in building ticcmds should be here.
typedef enum {
    // Game controls.
    A_TURNLEFT,
    A_TURNRIGHT,
    A_FORWARD,
    A_BACKWARD,
    A_STRAFELEFT,
    A_STRAFERIGHT,
    A_FIRE,
    A_USE,
    A_STRAFE,
    A_SPEED,
    A_WEAPON1,
    A_WEAPON2,
    A_WEAPON3,
    A_WEAPON4,
    A_WEAPON5,
    A_WEAPON6,
    A_WEAPON7,
    A_WEAPON8,
    A_WEAPON9,
    A_NEXTWEAPON,
    A_PREVIOUSWEAPON,
    A_MLOOK,
    A_JLOOK,
    A_LOOKUP,
    A_LOOKDOWN,
    A_LOOKCENTER,
    A_JUMP,

    // Game system actions.
    A_STOPDEMO,

    A_WEAPONCYCLE1,
    A_WEAPONCYCLE2,

    A_MAPZOOMIN,
    A_MAPZOOMOUT,
    A_MAPPANUP,
    A_MAPPANDOWN,
    A_MAPPANLEFT,
    A_MAPPANRIGHT,
    NUM_ACTIONS
} haction_t;

// This is the actions array.
extern action_t actions[NUM_ACTIONS + 1];

#endif
