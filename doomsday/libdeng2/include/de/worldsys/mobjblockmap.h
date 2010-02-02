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

#ifndef LIBDENG2_MOBJBLOCKMAP_H
#define LIBDENG2_MOBJBLOCKMAP_H

#include "../Gridmap"

namespace de
{
    typedef struct mobjblockmap_s {
        vec2_t          aabb[2];
        vec2_t          blockSize;
        gridmap_t*      gridmap;
    } mobjblockmap_t;

    mobjblockmap_t* P_CreateMobjBlockmap(const pvec2_t min, const pvec2_t max,
                                         duint width, duint height);
    void            P_DestroyMobjBlockmap(mobjblockmap_t* blockmap);

    duint           MobjBlockmap_NumInBlock(mobjblockmap_t* blockmap, duint x, duint y);
    void            MobjBlockmap_Link(mobjblockmap_t* blockmap, struct mobj_s* mo);
    bool            MobjBlockmap_Unlink(mobjblockmap_t* blockmap, struct mobj_s* mo);

    void            MobjBlockmap_Bounds(mobjblockmap_t* blockmap, pvec2_t min, pvec2_t max);
    void            MobjBlockmap_BlockSize(mobjblockmap_t* blockmap, pvec2_t blockSize);
    void            MobjBlockmap_Dimensions(mobjblockmap_t* blockmap, duint v[2]);

    bool            MobjBlockmap_Block2f(mobjblockmap_t* blockmap, duint block[2], dfloat x, dfloat y);
    bool            MobjBlockmap_Block2fv(mobjblockmap_t* blockmap, duint block[2], const dfloat pos[2]);

    void            MobjBlockmap_BoxToBlocks(mobjblockmap_t* blockmap, duint blockBox[4],
                                             const arvec2_t box);
    bool            MobjBlockmap_Iterate(mobjblockmap_t* blockmap, const duint block[2],
                                         bool (*func) (struct mobj_s*, void*),
                                         void* data);
    bool            MobjBlockmap_BoxIterate(mobjblockmap_t* blockmap, const duint blockBox[4],
                                            bool (*func) (struct mobj_s*, void*),
                                            void* data);
    bool            MobjBlockmap_PathTraverse(mobjblockmap_t* blockmap, const duint originBlock[2],
                                              const duint block[2], const dfloat origin[2],
                                              const dfloat dest[2],
                                              bool (*func) (intercept_t*));
}

#endif /* LIBDENG2_MOBJBLOCKMAP_H */
