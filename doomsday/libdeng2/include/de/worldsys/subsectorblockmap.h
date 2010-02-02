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

#ifndef LIBDENG2_SUBSECTORBLOCKMAP_H
#define LIBDENG2_SUBSECTORBLOCKMAP_H

#include "../Gridmap"

namespace de
{
    typedef struct subsectorblockmap_s {
        vec2_t          aabb[2];
        vec2_t          blockSize;
        gridmap_t*      gridmap;
    } subsectorblockmap_t;

    subsectorblockmap_t* P_CreateSubsectorBlockmap(const pvec2_t min, const pvec2_t max,
                                                   duint width, duint height);
    void            P_DestroySubsectorBlockmap(subsectorblockmap_t* blockmap);

    duint           SubsectorBlockmap_NumInBlock(subsectorblockmap_t* blockmap, duint x, duint y);
    void            SubsectorBlockmap_Link(subsectorblockmap_t* blockmap, struct subsector_s* subsector);
    bool            SubsectorBlockmap_Unlink(subsectorblockmap_t* blockmap, struct subsector_s* subsector);

    void            SubsectorBlockmap_Bounds(subsectorblockmap_t* blockmap, pvec2_t min, pvec2_t max);
    void            SubsectorBlockmap_BlockSize(subsectorblockmap_t* blockmap, pvec2_t blockSize);
    void            SubsectorBlockmap_Dimensions(subsectorblockmap_t* blockmap, duint v[2]);

    bool            SubsectorBlockmap_Block2f(subsectorblockmap_t* blockmap, duint block[2], dfloat x, dfloat y);
    bool            SubsectorBlockmap_Block2fv(subsectorblockmap_t* blockmap, duint block[2], const dfloat pos[2]);
    void            SubsectorBlockmap_BoxToBlocks(subsectorblockmap_t* blockmap, duint blockBox[4],
                                                  const arvec2_t box);
    bool            SubsectorBlockmap_Iterate(subsectorblockmap_t* blockmap, const duint block[2],
                                              struct sector_s* sector, const arvec2_t box,
                                              dint localValidCount,
                                              bool (*func) (struct subsector_s*, void*),
                                              void* data);
    bool            SubsectorBlockmap_BoxIterate(subsectorblockmap_t* blockmap, const duint blockBox[4],
                                                 struct sector_s* sector, const arvec2_t box,
                                                 dint localValidCount,
                                                 bool (*func) (struct subsector_s*, void*),
                                                 void* data, bool retObjRecord);
}

#endif /* LIBDENG2_SUBSECTORBLOCKMAP_H */
