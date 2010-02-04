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
#include "../Vector"

namespace de
{
    class MobjBlockmap
    {
    public:
        MobjBlockmap(const Vector2<dfloat>& min, const Vector2<dfloat>& max, duint width, duint height);
        ~MobjBlockmap();

        duint numInBlock(duint x, duint y);
        void link(struct mobj_s* mo);
        bool unlink(struct mobj_s* mo);

        void bounds(Vector2<dfloat>& min, Vector2<dfloat>& max) const;
        void blockSize(Vector2<dfloat>& blockSize) const;
        void dimensions(Vector2<duint>& dimensions) const;

        bool block(Vector2<duint>& block, dfloat x, dfloat y) const;
        bool block(Vector2<duint>& block, const Vector2<dfloat>& pos) const;

        //void boxToBlocks(duint blockBox[4], const arvec2_t box);
        bool iterate(const Vector2<duint>& block, bool (*func) (struct mobj_s*, void*), void* data);
        //bool boxIterate(const duint blockBox[4], bool (*func) (struct mobj_s*, void*), void* data);
        //bool pathTraverse(const Vector2<duint>& originBlock, const Vector2<duint>& block, const Vector2<dfloat>& origin, const Vector2<dfloat>& dest, bool (*func) (intercept_t*));

    private:
        Vector2<dfloat> _aabb[2];
        Vector2<dfloat> _blockSize;
        Gridmap<struct mobj_s*> _gridmap;
    };
}

#endif /* LIBDENG2_MOBJBLOCKMAP_H */
