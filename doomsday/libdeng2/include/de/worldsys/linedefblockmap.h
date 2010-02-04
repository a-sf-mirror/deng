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
#include "../Vector"
#include "../Rectangle"
#include "../LineDef"

namespace de
{
    class LineDefBlockmap
    {
    public:
        LineDefBlockmap(const Vector2f& min, const Vector2f& max, duint width, duint height);
        ~LineDefBlockmap();

        duint numInBlock(duint x, duint y) const;

        void link(LineDef* lineDef);
        void link2(LineDef* lineDef);
        bool unlink(LineDef* lineDef);

        void bounds(Vector2f& min, Vector2f& max) const;
        void blockSize(Vector2f& blockSize) const;
        void dimensions(Vector2ui& dimensions) const;

        /**
         * Given a world coordinate, output the blockmap block[x, y] it resides in.
         */
        bool block(Vector2ui& block, dfloat x, dfloat y) const;

        /*bool block(Vector2ui& block, const Vector2f& pos) const {
            return block(block, pos.x, pos.y);
        }*/

        void boxToBlocks(Rectangle<Vector2ui>& blocks, const Rectangle<Vector2f>& box) const;

        bool iterate(const Vector2ui& block, bool (*func) (LineDef*, void*), void* data, bool retObjRecord);
        bool iterate(const Rectangle<Vector2ui>& blocks, bool (*func) (LineDef*, void*), void* data, bool retObjRecord);
        //bool pathTraverse(const Vector2ui& originBlock, const Vector2ui& block, const Vector2f& origin, const Vector2f& dest, bool (*func) (intercept_t*));

    private:
        Rectangle<Vector2f> _aaBB;
        Vector2f _blockSize;
        Gridmap<LineDef*> _gridmap;

    private:
        void tryLinkLineDefToBlock(duint x, duint y, LineDef* lineDef);
        void linkLineDef(LineDef* lineDef, const Vector2i& vtx1, const Vector2i& vtx2);
    };
}

#endif /* LIBDENG2_LINEDEFBLOCKMAP_H */
