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
#include "../Vector"

namespace de
{
    class Subsector;

    class SubsectorBlockmap
    {
    public: 
        SubsectorBlockmap(const Vector2f& min, const Vector2f& max, duint width, duint height);
        ~SubsectorBlockmap();

        duint numInBlock(duint x, duint y) const;
        void link(Subsector* subsector);
        bool unlink(Subsector* subsector);

        void bounds(Vector2f& min, Vector2f& max) const;
        void blockSize(Vector2f& blockSize) const;
        void dimensions(Vector2ui& dimensions) const;

        bool block(Vector2ui& block, dfloat x, dfloat y);
        bool block(Vector2ui& block, const Vector2f& pos);

        //void boxToBlocks(duint blockBox[4], const arvec2_t box);
        //bool iterate(const Vector2ui& block, Sector* sector, const arvec2_t box, dint localValidCount, bool (*func) (Subsector*, void*), void* data);
        //bool boxIterate(const duint blockBox[4], Sector* sector, const arvec2_t box, dint localValidCount, bool (*func) (Subsector*, void*), void* data, bool retObjRecord);

    private:
        /// Axis-Aligned Bounding box, in the map coordinate space.
        MapRectanglef _aaBounds;

        /// Dimensions of the blocks of the blockmap in map units.
        Vector2f _blockSize;

        /// Grid of Subsector lists per block.
        Gridmap<Subsector*> _gridmap;
    };
}

#endif /* LIBDENG2_SUBSECTORBLOCKMAP_H */
