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

#ifndef LIBDENG2_THINGBLOCKMAP_H
#define LIBDENG2_THINGBLOCKMAP_H

#include "../Gridmap"
#include "../Vector"
#include "../Thing"

#include <list>

namespace de
{
    class ThingBlockmap
    {
    public:
        /// List of things per block.
        typedef std::list<Thing*> Things;

    public:
        ThingBlockmap(const Vector2f& min, const Vector2f& max, duint width, duint height);
        ~ThingBlockmap();

        dsize numInBlock(duint x, duint y) const;
        dsize numInBlock(const Vector2ui block) const {
            return numInBlock(block.x, block.y);
        }

        void link(Thing* thing);
        bool unlink(Thing* thing);

        const MapRectanglef& aaBounds() const;
        const Vector2f& blockSize() const;
        const Vector2ui& dimensions() const;

        /**
         * Given a world coordinate, output the blockmap block[x, y] it resides in.
         */
        bool block(Vector2ui& dest, const Vector3f& pos) const;
        bool block(Vector2ui& dest, dfloat x, dfloat y) const {
            return block(dest, Vector2f(x, y));
        }

        //void boxToBlocks(duint blockBox[4], const arvec2_t box);
        bool iterate(const Vector2ui& block, bool (*func) (Thing*, void*), void* paramaters = 0);
        //bool boxIterate(const duint blockBox[4], bool (*func) (Thing*, void*), void* paramaters = 0);
        //bool pathTraverse(const Vector2ui& originBlock, const Vector2ui& block, const Vector2f& origin, const Vector2f& dest, bool (*func) (intercept_t*));

    private:
        void linkInBlock(duint x, duint y, Thing* thing);
        bool unlinkInBlock(duint x, duint y, Thing* thing);

        /// Axis-Aligned Bounding box, in the map coordinate space.
        MapRectanglef _aaBounds;

        /// Dimensions of the blocks of the blockmap in map units.
        Vector2f _blockSize;

        /// Grid of Thing lists per block.
        Gridmap<Things*> _gridmap;
    };
}

#endif /* LIBDENG2_THINGBLOCKMAP_H */
