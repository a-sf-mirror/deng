/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_PARTICLEBLOCKMAP_H
#define LIBDENG2_PARTICLEBLOCKMAP_H

#include "../Gridmap"
#include "../Vector"

namespace de
{
    struct particle_t;

    class ParticleBlockmap
    {
    public:
        ParticleBlockmap(const Vector2f& min, const Vector2f& max, duint width, duint height);
        ~ParticleBlockmap();

        void incValidcount();
        void empty();

        dsize numInBlock(duint x, duint y) const;
        dsize numInBlock(const Vector2ui block) const {
            return numInBlock(block.x, block.y);
        }

        void link(particle_t* particle);
        bool unlink(particle_t* particle);

        const MapRectanglef& aaBounds() const;
        const Vector2f& blockSize() const;
        const Vector2ui& dimensions() const;

        bool block(Vector2ui& block, dfloat x, dfloat y) const;
        bool block(Vector2ui& block, const Vector2f& pos) const;
        //void boxToBlocks(duint blockBox[4], const arvec2_t box);

        bool iterate(const Vector2ui& block, bool (*func) (particle_t*, void*), void* paramaters = 0);
        bool iterate(const Vector2ui& bottomLeft, const Vector2ui& topRight, bool (*func) (particle_t*, void*), void* paramaters = 0);

    private:
        /// Axis-Aligned Bounding box, in the map coordinate space.
        MapRectanglef _aaBounds;

        /// Dimensions of the blocks of the blockmap in map units.
        Vector2f _blockSize;

        /// Grid of Particle lists per block.
        Gridmap<particle_t*> _gridmap;
    };
}

#endif /* LIBDENG2_PARTICLEBLOCKMAP_H */
