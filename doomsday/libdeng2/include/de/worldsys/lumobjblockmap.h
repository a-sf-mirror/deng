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

#ifndef LIBDENG2_LUMOBJBLOCKMAP_H
#define LIBDENG2_LUMOBJBLOCKMAP_H

#include "../Gridmap"

namespace de
{
    typedef struct lumobjblockmap_s {
        vec2_t          aabb[2];
        vec2_t          blockSize;
        gridmap_t*      gridmap;
    } lumobjblockmap_t;

    lumobjblockmap_t* P_CreateLumobjBlockmap(const pvec2_t min, const pvec2_t max,
                                                 duint width, duint height);
    void            P_DestroyLumobjBlockmap(lumobjblockmap_t* blockmap);

    void            LumobjBlockmap_Empty(lumobjblockmap_t* blockmap);

    duint           LumobjBlockmap_NumInBlock(lumobjblockmap_t* blockmap, duint x, duint y);
    void            LumobjBlockmap_Link(lumobjblockmap_t* blockmap, struct lumobj_s* lum);
    bool            LumobjBlockmap_Unlink(lumobjblockmap_t* blockmap, struct lumobj_s* lum);

    void            LumobjBlockmap_Bounds(lumobjblockmap_t* blockmap, pvec2_t min, pvec2_t max);
    void            LumobjBlockmap_BlockSize(lumobjblockmap_t* blockmap, pvec2_t blockSize);
    void            LumobjBlockmap_Dimensions(lumobjblockmap_t* blockmap, duint v[2]);

    bool            LumobjBlockmap_Block2f(lumobjblockmap_t* blockmap, duint block[2], dfloat x, dfloat y);
    bool            LumobjBlockmap_Block2fv(lumobjblockmap_t* blockmap, duint block[2], const dfloat pos[2]);
    void            LumobjBlockmap_BoxToBlocks(lumobjblockmap_t* blockmap, duint blockBox[4],
                                               const arvec2_t box);
    bool            LumobjBlockmap_Iterate(lumobjblockmap_t* blockmap, const duint block[2],
                                           bool (*func) (struct lumobj_s*, void*),
                                           void* data);
    bool            LumobjBlockmap_BoxIterate(lumobjblockmap_t* blockmap, const duint blockBox[4],
                                              bool (*func) (struct lumobj_s*, void*),
                                              void* data);
}

#endif /* LIBDENG2_LUMOBJBLOCKMAP_H */
