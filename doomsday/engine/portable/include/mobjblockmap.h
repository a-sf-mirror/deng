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

#ifndef DOOMSDAY_MOBJBLOCKMAP_H
#define DOOMSDAY_MOBJBLOCKMAP_H

#include "m_gridmap.h"

//// \todo This stuff is obsolete and needs to be removed!
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

typedef struct blockmap_s {
    vec2_t          aabb[2];
    vec2_t          blockSize;
    gridmap_t*      gridmap;
} blockmap_t;

/**
 * @todo C++ refactor; make interface.
 */
typedef blockmap_t mobjblockmap_t;
typedef blockmap_t polyobjblockmap_t;

/**
 * MobjBlockmap
 */
mobjblockmap_t* P_CreateMobjBlockmap(const pvec2_t min, const pvec2_t max,
                                     uint width, uint height);
void            P_DestroyMobjBlockmap(mobjblockmap_t* blockmap);

uint            MobjBlockmap_NumInBlock(mobjblockmap_t* blockmap, uint x, uint y);
void            MobjBlockmap_Insert(mobjblockmap_t* blockmap, struct mobj_s* mo);
boolean         MobjBlockmap_Remove(mobjblockmap_t* blockmap, struct mobj_s* mo);
void            MobjBlockmap_Bounds(mobjblockmap_t* blockmap, pvec2_t min, pvec2_t max);
void            MobjBlockmap_BlockSize(mobjblockmap_t* blockmap, pvec2_t blockSize);
void            MobjBlockmap_Dimensions(mobjblockmap_t* blockmap, uint v[2]);

boolean         MobjBlockmap_Block2f(mobjblockmap_t* blockmap, uint destBlock[2], float x, float y);
boolean         MobjBlockmap_Block2fv(mobjblockmap_t* blockmap, uint destBlock[2], const float pos[2]);

void            MobjBlockmap_BoxToBlocks(mobjblockmap_t* blockmap, uint blockBox[4],
                                         const arvec2_t box);
boolean         MobjBlockmap_Iterate(mobjblockmap_t* blockmap, const uint block[2],
                                     boolean (*func) (struct mobj_s*, void*),
                                     void* data);
boolean         MobjBlockmap_BoxIterate(mobjblockmap_t* blockmap, const uint blockBox[4],
                                        boolean (*func) (struct mobj_s*, void*),
                                        void* data);
boolean         MobjBlockmap_PathTraverse(mobjblockmap_t* blockmap, const uint originBlock[2],
                                          const uint destBlock[2], const float origin[2],
                                          const float dest[2],
                                          boolean (*func) (intercept_t*));
/**
 * PolyobjBlockmap
 */
polyobjblockmap_t* P_CreatePolyobjBlockmap(const pvec2_t min, const pvec2_t max,
                                           uint width, uint height);
void            P_DestroyPolyobjBlockmap(polyobjblockmap_t* blockmap);

void            PolyobjBlockmap_SetBlock(polyobjblockmap_t* blockmap, uint x, uint y,
                                         struct linkpolyobj_s* poLink);
uint            PolyobjBlockmap_NumInBlock(polyobjblockmap_t* blockmap, uint x, uint y);
void            PolyobjBlockmap_Insert(polyobjblockmap_t* blockmap, struct polyobj_s* po);
void            PolyobjBlockmap_Remove(polyobjblockmap_t* blockmap, struct polyobj_s* po);
void            PolyobjBlockmap_Bounds(polyobjblockmap_t* blockmap, pvec2_t min, pvec2_t max);
void            PolyobjBlockmap_BlockSize(polyobjblockmap_t* blockmap, pvec2_t blockSize);
void            PolyobjBlockmap_Dimensions(polyobjblockmap_t* blockmap, uint v[2]);

boolean         PolyobjBlockmap_Block2f(polyobjblockmap_t* blockmap, uint destBlock[2], float x, float y);
boolean         PolyobjBlockmap_Block2fv(polyobjblockmap_t* blockmap, uint destBlock[2], const float pos[2]);
void            PolyobjBlockmap_BoxToBlocks(polyobjblockmap_t* blockmap, uint blockBox[4],
                                            const arvec2_t box);
boolean         PolyobjBlockmap_Iterate(polyobjblockmap_t* blockmap, const uint block[2],
                                        boolean (*func) (struct polyobj_s*, void*),
                                        void* data);
boolean         PolyobjBlockmap_BoxIterate(polyobjblockmap_t* blockmap, const uint blockBox[4],
                                           boolean (*func) (struct polyobj_s*, void*),
                                           void* data);
#endif /* DOOMSDAY_MOBJBLOCKMAP_H */
