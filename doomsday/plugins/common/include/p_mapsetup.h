/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_setup.c: Common map setup routines.
 */

#ifndef LIBCOMMON_PLAYSETUP_H
#define LIBCOMMON_PLAYSETUP_H

#include "gamemap.h"

#define numvertexes (*(uint*) P_GetVariable(DMU_VERTEX_COUNT))
#define numsectors  (*(uint*) P_GetVariable(DMU_SECTOR_COUNT))
#define numsubsectors (*(uint*) P_GetVariable(DMU_SUBSECTOR_COUNT))
#define numnodes    (*(uint*) P_GetVariable(DMU_NODE_COUNT))
#define numlines    (*(uint*) P_GetVariable(DMU_LINE_COUNT))
#define numsides    (*(uint*) P_GetVariable(DMU_SIDE_COUNT))
#define numpolyobjs (*(uint*) P_GetVariable(DMU_POLYOBJ_COUNT))

#define nummaterials (*(uint*) DD_GetVariable(DD_MATERIAL_COUNT))

void            P_SetupForMapData(int type, uint num);

void            P_SetupMap(struct gamemap_s* map, skillmode_t skill);
const char*     P_GetMapNiceName(void);
const char*     P_GetMapAuthor(boolean surpressIWADAuthors);

xlinedef_t*     P_ToXLine(linedef_t* line);
xsector_t*      P_ToXSector(sector_t* sector);
xsector_t*      P_ToXSectorOfSubsector(subsector_t* sub);
#endif /* LIBCOMMON_PLAYSETUP_H */
