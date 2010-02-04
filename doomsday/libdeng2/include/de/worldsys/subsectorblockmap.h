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
#include "../Subsector"

namespace de
{
    class SubsectorBlockmap
    {
    public: 
        SubsectorBlockmap(const Vector2<dfloat>& min, const Vector2<dfloat>& max, duint width, duint height);
        ~SubsectorBlockmap();

        duint numInBlock(duint x, duint y) const;
        void link(Subsector* subsector);
        bool unlink(Subsector* subsector);

        void bounds(Vector2<dfloat>& min, Vector2<dfloat>& max) const;
        void blockSize(Vector2<dfloat>& blockSize) const;
        void dimensions(Vector2<duint>& dimensions) const;

        bool block(Vector2<duint>& block, dfloat x, dfloat y);
        bool block(Vector2<duint>& block, const Vector2<dfloat>& pos);

        //void boxToBlocks(duint blockBox[4], const arvec2_t box);
        //bool iterate(const Vector2<duint>& block, Sector* sector, const arvec2_t box, dint localValidCount, bool (*func) (Subsector*, void*), void* data);
        //bool boxIterate(const duint blockBox[4], Sector* sector, const arvec2_t box, dint localValidCount, bool (*func) (Subsector*, void*), void* data, bool retObjRecord);

    private:
        Vector2<dfloat> _aabb[2];
        Vector2<dfloat> _blockSize;
        Gridmap<Subsector*> _gridmap;
    };
}

#endif /* LIBDENG2_SUBSECTORBLOCKMAP_H */
