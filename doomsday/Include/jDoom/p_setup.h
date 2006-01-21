/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Map setup routines
 */

#ifndef __D_SETUP_H__
#define __D_SETUP_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// Game specific map format properties for ALL games.
// (notice jHeretic/jHexen properties are here too temporarily).

// TODO: we don't need  to know about all of them once they
//       are registered via DED.
// TODO: these values ARE NOT correct.
enum {
    DAM_SIDE,
    DAM_LINE_TAG,
    DAM_LINE_SPECIAL,
    DAM_LINE_ARG1,
    DAM_LINE_ARG2,
    DAM_LINE_ARG3,
    DAM_LINE_ARG4,
    DAM_LINE_ARG5,
    DAM_TOP_TEXTURE,
    DAM_MIDDLE_TEXTURE,
    DAM_BOTTOM_TEXTURE,
    DAM_SECTOR_SPECIAL,
    DAM_SECTOR_TAG,
    DAM_THING_TID,
    DAM_THING_X,
    DAM_THING_Y,
    DAM_THING_HEIGHT,
    DAM_THING_ANGLE,
    DAM_THING_TYPE,
    DAM_THING_OPTIONS,
    DAM_THING_SPECIAL,
    DAM_THING_ARG1,
    DAM_THING_ARG2,
    DAM_THING_ARG3,
    DAM_THING_ARG4,
    DAM_THING_ARG5
};

int             P_HandleMapDataProperty(int id, int dtype, int prop, int type, void *data);
int             P_HandleMapDataPropertyValue(int id, int dtype, int prop, int type, void *data);

void            P_Init(void);

boolean         P_MapExists(int episode, int map);
void            P_SetupLevel(int episode, int map, int playermask, skill_t skill);

#endif
