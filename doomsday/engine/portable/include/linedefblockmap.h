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

#ifndef DOOMSDAY_LINEDEFBLOCKMAP_H
#define DOOMSDAY_LINEDEFBLOCKMAP_H

#include "m_gridmap.h"

//// \todo This stuff is obsolete and needs to be removed!
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

typedef struct linedefblockmap_s {
    vec2_t          aabb[2];
    vec2_t          blockSize;
    gridmap_t*      gridmap;
} linedefblockmap_t;

/**
 * LineDefBlockmap
 */
linedefblockmap_t* P_CreateLineDefBlockmap(const pvec2_t min, const pvec2_t max,
                                           uint width, uint height);
void            P_DestroyLineDefBlockmap(linedefblockmap_t* blockmap);

uint            LineDefBlockmap_NumInBlock(linedefblockmap_t* blockmap, uint x, uint y);
void            LineDefBlockmap_Insert(linedefblockmap_t* blockmap, struct linedef_s* lineDef);
boolean         LineDefBlockmap_Remove(linedefblockmap_t* blockmap, struct linedef_s* lineDef);
void            LineDefBlockmap_Bounds(linedefblockmap_t* blockmap, pvec2_t min, pvec2_t max);
void            LineDefBlockmap_BlockSize(linedefblockmap_t* blockmap, pvec2_t blockSize);
void            LineDefBlockmap_Dimensions(linedefblockmap_t* blockmap, uint v[2]);

boolean         LineDefBlockmap_Block2f(linedefblockmap_t* blockmap, uint destBlock[2], float x, float y);
boolean         LineDefBlockmap_Block2fv(linedefblockmap_t* blockmap, uint destBlock[2], const float pos[2]);
void            LineDefBlockmap_BoxToBlocks(linedefblockmap_t* blockmap, uint blockBox[4],
                                            const arvec2_t box);
boolean         LineDefBlockmap_Iterate(linedefblockmap_t* blockmap, const uint block[2],
                                        boolean (*func) (struct linedef_s*, void*),
                                        void* data, boolean retObjRecord);
boolean         LineDefBlockmap_BoxIterate(linedefblockmap_t* blockmap, const uint blockBox[4],
                                           boolean (*func) (struct linedef_s*, void*),
                                           void* data, boolean retObjRecord);
boolean         LineDefBlockmap_PathTraverse(linedefblockmap_t* blockmap, const uint originBlock[2],
                                             const uint destBlock[2], const float origin[2],
                                             const float dest[2],
                                             boolean (*func) (intercept_t*));
#endif /* DOOMSDAY_LINEDEFBLOCKMAP_H */
