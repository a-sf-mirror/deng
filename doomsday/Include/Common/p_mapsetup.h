/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * p_setup.c: Common map setup routines.
 */

#ifndef __COMMON_PLAYSETUP_H__
#define __COMMON_PLAYSETUP_H__

extern int numvertexes;
extern int numsegs;
extern int numsectors;
extern int numsubsectors;
extern int numnodes;
extern int numlines;
extern int numsides;
extern int numthings;

#if __JHEXEN__
extern int numpolyobjs;
#endif

void        P_SetupLevel(int episode, int map, int playermask, skill_t skill);
void        P_LocateMapLumps(int episode, int map, int *lumpIndices);


#endif
