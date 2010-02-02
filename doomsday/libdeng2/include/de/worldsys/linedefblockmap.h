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

#ifndef LIBDENG2_LINEDEFBLOCKMAP_H
#define LIBDENG2_LINEDEFBLOCKMAP_H

#include "../Gridmap"

namespace de
{
    typedef struct linedefblockmap_s {
        vec2_t          aabb[2];
        vec2_t          blockSize;
        gridmap_t*      gridmap;
    } linedefblockmap_t;

    linedefblockmap_t* P_CreateLineDefBlockmap(const pvec2_t min, const pvec2_t max,
                                               duint width, duint height);
    void            P_DestroyLineDefBlockmap(linedefblockmap_t* blockmap);

    duint           LineDefBlockmap_NumInBlock(linedefblockmap_t* blockmap, duint x, duint y);
    void            LineDefBlockmap_Link(linedefblockmap_t* blockmap, struct linedef_s* lineDef);
    void            LineDefBlockmap_Link2(linedefblockmap_t* blockmap, struct linedef_s* lineDef);
    bool            LineDefBlockmap_Unlink(linedefblockmap_t* blockmap, struct linedef_s* lineDef);
    void            LineDefBlockmap_Bounds(linedefblockmap_t* blockmap, pvec2_t min, pvec2_t max);
    void            LineDefBlockmap_BlockSize(linedefblockmap_t* blockmap, pvec2_t blockSize);
    void            LineDefBlockmap_Dimensions(linedefblockmap_t* blockmap, duint v[2]);

    bool            LineDefBlockmap_Block2f(linedefblockmap_t* blockmap, duint block[2], dfloat x, dfloat y);
    bool            LineDefBlockmap_Block2fv(linedefblockmap_t* blockmap, duint block[2], const dfloat pos[2]);
    void            LineDefBlockmap_BoxToBlocks(linedefblockmap_t* blockmap, duint blockBox[4], const arvec2_t box);
    bool            LineDefBlockmap_Iterate(linedefblockmap_t* blockmap, const duint block[2],
                                            bool (*func) (struct linedef_s*, void*),
                                            void* data, bool retObjRecord);
    bool            LineDefBlockmap_BoxIterate(linedefblockmap_t* blockmap, const duint blockBox[4],
                                               bool (*func) (struct linedef_s*, void*),
                                               void* data, bool retObjRecord);
    bool            LineDefBlockmap_PathTraverse(linedefblockmap_t* blockmap, const duint originBlock[2],
                                                 const duint block[2], const dfloat origin[2],
                                                 const dfloat dest[2],
                                                 bool (*func) (intercept_t*));
}

#endif /* LIBDENG2_LINEDEFBLOCKMAP_H */
