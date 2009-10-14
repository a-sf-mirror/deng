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

/**
 * blockmap.h: Blockmaps.
 */

#ifndef DOOMSDAY_BLOCKMAP_H
#define DOOMSDAY_BLOCKMAP_H

blockmap_t*     P_CreateBlockmap(const pvec2_t min, const pvec2_t max,
                                 uint width, uint height);

//// \todo This stuff is obsolete and needs to be removed!
#define MAPBLOCKUNITS   128
#define MAPBLOCKSIZE    (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT   (FRACBITS+7)
#define MAPBMASK        (MAPBLOCKSIZE-1)
#define MAPBTOFRAC      (MAPBLOCKSHIFT-FRACBITS)

// Management:
void            Blockmap_SetBlock(blockmap_t* bmap, uint x, uint y,
                                  linedef_t** lines, linkmobj_t* moLink,
                                  linkpolyobj_t* poLink);
void            Blockmap_SetBlockSubsectors(blockmap_t* bmap, uint x, uint y,
                                            subsector_t** subsectors);

void            Blockmap_LinkMobj(blockmap_t* bmap, mobj_t* mo);
boolean         Blockmap_UnlinkMobj(blockmap_t* bmap, mobj_t* mo);
void            Blockmap_LinkPolyobj(blockmap_t* bmap, polyobj_t* po);
void            Blockmap_UnlinkPolyobj(blockmap_t* bmap, polyobj_t* po);

// Utility:
void            Blockmap_Bounds(blockmap_t* bmap, pvec2_t min, pvec2_t max);
void            Blockmap_BlockSize(blockmap_t* blockmap, pvec2_t blockSize);
void            Blockmap_Dimensions(blockmap_t* bmap, uint v[2]);

boolean         Blockmap_Block2fv(blockmap_t* bmap, uint destBlock[2],
                                  const pvec2_t sourcePos);

void            Blockmap_BoxToBlocks(blockmap_t* bmap, uint blockBox[4],
                                     const arvec2_t box);

int             Blockmap_NumLineDefs(blockmap_t* bmap, uint x, uint y);
int             Blockmap_NumMobjs(blockmap_t* bmap, uint x, uint y);
int             Blockmap_NumPolyobjs(blockmap_t* blockmap, uint x, uint y);
int             Blockmap_NumSubsectors(blockmap_t* blockmap, uint x, uint y);

// Block Iterators:
boolean         Blockmap_IterateMobjs(blockmap_t* bmap, const uint block[2],
                                      boolean (*func) (struct mobj_s*, void*),
                                      void* data);
boolean         Blockmap_IterateLineDefs(blockmap_t* bmap, const uint block[2],
                                         boolean (*func) (linedef_t*, void*),
                                         void* data, boolean retObjRecord);
boolean         Blockmap_IterateSubsectors(blockmap_t* bmap, const uint block[2],
                                           sector_t* sector, const arvec2_t box,
                                           int localValidCount,
                                           boolean (*func) (subsector_t*, void*),
                                           void* data);
boolean         Blockmap_IteratePolyobjs(blockmap_t* bmap, const uint block[2],
                                         boolean (*func) (polyobj_t*, void*),
                                         void* data);
boolean         Blockmap_IteratePolyobjLineDefs(blockmap_t* bmap, const uint block[2],
                                                boolean (*func) (linedef_t*, void*),
                                                void* data, boolean retObjRecord);

// Block Box Iterators:
boolean         Blockmap_BoxIterateMobjs(blockmap_t* bmap, const uint blockBox[4],
                                         boolean (*func) (struct mobj_s*, void*),
                                         void* data);
boolean         Blockmap_BoxIterateLineDefs(blockmap_t* bmap, const uint blockBox[4],
                                            boolean (*func) (linedef_t*, void*),
                                            void* data, boolean retObjRecord);
boolean         Blockmap_BoxIterateSubsectors(blockmap_t* bmap, const uint blockBox[4],
                                              sector_t* sector, const arvec2_t box,
                                              int localValidCount,
                                              boolean (*func) (subsector_t*, void*),
                                              void* data, boolean retObjRecord);
boolean         Blockmap_BoxIteratePolyobjs(blockmap_t* bmap, const uint blockBox[4],
                                            boolean (*func) (polyobj_t*, void*),
                                            void* data);
boolean         Blockmap_BoxIteratePolyobjLineDefs(blockmap_t* bmap, const uint blockBox[4],
                                                   boolean (*func) (linedef_t*, void*),
                                                   void* data, boolean retObjRecord);

// Specialized Traversals:
boolean         Blockmap_PathTraverse(blockmap_t* bmap, const uint start[2],
                                      const uint end[2], const float origin[2],
                                      const float dest[2], int flags,
                                      boolean (*func) (intercept_t*));
#endif /* DOOMSDAY_BLOCKMAP_H */
