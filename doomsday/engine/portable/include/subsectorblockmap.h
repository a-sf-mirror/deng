/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_SUBSECTORBLOCKMAP_H
#define DOOMSDAY_SUBSECTORBLOCKMAP_H

#include "m_gridmap.h"

//// \todo This stuff is obsolete and needs to be removed!
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

typedef struct subsectorblockmap_s {
    vec2_t          aabb[2];
    vec2_t          blockSize;
    gridmap_t*      gridmap;
} subsectorblockmap_t;

subsectorblockmap_t* P_CreateSubsectorBlockmap(const pvec2_t min, const pvec2_t max,
                                               uint width, uint height);
void            P_DestroySubsectorBlockmap(subsectorblockmap_t* blockmap);

void            SubsectorBlockmap_SetBlock(subsectorblockmap_t* blockmap, uint x, uint y,
                                           struct subsector_s** subsectors);
uint            SubsectorBlockmap_NumInBlock(subsectorblockmap_t* blockmap, uint x, uint y);
void            SubsectorBlockmap_Bounds(subsectorblockmap_t* blockmap, pvec2_t min, pvec2_t max);
void            SubsectorBlockmap_BlockSize(subsectorblockmap_t* blockmap, pvec2_t blockSize);
void            SubsectorBlockmap_Dimensions(subsectorblockmap_t* blockmap, uint v[2]);

boolean         SubsectorBlockmap_Block2f(subsectorblockmap_t* blockmap, uint destBlock[2], float x, float y);
boolean         SubsectorBlockmap_Block2fv(subsectorblockmap_t* blockmap, uint destBlock[2], const float pos[2]);
void            SubsectorBlockmap_BoxToBlocks(subsectorblockmap_t* blockmap, uint blockBox[4],
                                              const arvec2_t box);
boolean         SubsectorBlockmap_Iterate(subsectorblockmap_t* blockmap, const uint block[2],
                                          struct sector_s* sector, const arvec2_t box,
                                          int localValidCount,
                                          boolean (*func) (struct subsector_s*, void*),
                                          void* data);
boolean         SubsectorBlockmap_BoxIterate(subsectorblockmap_t* blockmap, const uint blockBox[4],
                                             struct sector_s* sector, const arvec2_t box,
                                             int localValidCount,
                                             boolean (*func) (struct subsector_s*, void*),
                                             void* data, boolean retObjRecord);

#endif /* DOOMSDAY_SUBSECTORBLOCKMAP_H */
