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

#ifndef DOOMSDAY_PARTICLEBLOCKMAP_H
#define DOOMSDAY_PARTICLEBLOCKMAP_H

#include "m_gridmap.h"

typedef struct particleblockmap_s {
    vec2_t          aabb[2];
    vec2_t          blockSize;
    gridmap_t*      gridmap;
} particleblockmap_t;

/**
 * ParticleBlockmap
 */
particleblockmap_t* P_CreateParticleBlockmap(const pvec2_t min, const pvec2_t max,
                                             uint width, uint height);
void            P_DestroyParticleBlockmap(particleblockmap_t* blockmap);

void            ParticleBlockmap_IncValidcount(particleblockmap_t* blockmap);
void            ParticleBlockmap_Empty(particleblockmap_t* blockmap);

uint            ParticleBlockmap_NumInBlock(particleblockmap_t* blockmap, uint x, uint y);
void            ParticleBlockmap_Link(particleblockmap_t* blockmap, struct particle_s* particle);
boolean         ParticleBlockmap_Unlink(particleblockmap_t* blockmap, struct particle_s* particle);

void            ParticleBlockmap_Bounds(particleblockmap_t* blockmap, pvec2_t min, pvec2_t max);
void            ParticleBlockmap_BlockSize(particleblockmap_t* blockmap, pvec2_t blockSize);
void            ParticleBlockmap_Dimensions(particleblockmap_t* blockmap, uint v[2]);

boolean         ParticleBlockmap_Block2f(particleblockmap_t* blockmap, uint block[2], float x, float y);
boolean         ParticleBlockmap_Block2fv(particleblockmap_t* blockmap, uint block[2], const float pos[2]);
void            ParticleBlockmap_BoxToBlocks(particleblockmap_t* blockmap, uint blockBox[4],
                                             const arvec2_t box);
boolean         ParticleBlockmap_Iterate(particleblockmap_t* blockmap, const uint block[2],
                                         boolean (*func) (struct particle_s*, void*),
                                         void* data);
boolean         ParticleBlockmap_BoxIterate(particleblockmap_t* blockmap, const uint blockBox[4],
                                            boolean (*func) (struct particle_s*, void*),
                                            void* data);
#endif /* DOOMSDAY_PARTICLEBLOCKMAP_H */
