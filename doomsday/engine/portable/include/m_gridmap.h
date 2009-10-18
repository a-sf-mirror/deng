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
 * m_gridmap.h: Generalized blockmap
 */

#ifndef DOOMSDAY_MISC_GRIDMAP_H
#define DOOMSDAY_MISC_GRIDMAP_H

typedef void* gridmap_t;

gridmap_t*      M_CreateGridmap(uint width, uint height, int zoneTag);
void            M_DestroyGridmap(gridmap_t* gridmap);

uint            Gridmap_Width(gridmap_t* gridmap);
uint            Gridmap_Height(gridmap_t* gridmap);
void            Gridmap_Dimensions(gridmap_t* gridmap, uint dimensions[2]);

void*           Gridmap_Block(gridmap_t* gridmap, uint x, uint y);
void*           Gridmap_SetBlock(gridmap_t* gridmap, uint x, uint y, void* data);

// Iteration
boolean         Gridmap_Iterate(gridmap_t* gridmap,
                                boolean (*callback) (void* p, void* context),
                                void* param);
boolean         Gridmap_IterateBox(gridmap_t* gridmap,
                                   uint xl, uint xh, uint yl, uint yh,
                                   boolean (*callback) (void* p, void* context),
                                   void* param);
boolean         Gridmap_IterateBoxv(gridmap_t* gridmap, const uint box[4],
                                      boolean (*callback) (void* p, void* context),
                                      void* param);

#endif /* DOOMSDAY_MISC_GRIDMAP_H */
