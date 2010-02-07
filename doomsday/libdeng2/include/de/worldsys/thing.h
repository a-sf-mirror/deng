/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_THING_H
#define LIBDENG2_THING_H

#include "../Flag"
#include "../String"
#include "../Animator"
#include "../NodePile"
#include "../Subsector"

namespace de
{
    /**
     * Maintains information about the state of an object in the world.
     * Each Thing instance is constructed based on thing definitions that
     * describe how the thing appears and behaves. Each Thing instance is specific
     * to one object.
     */
    class Thing
    {
    public:
        /** @name Thing Link Flags */
        //@{
        DEFINE_FLAG(LINK_SECTOR, 1);
        DEFINE_FLAG(LINK_BLOCKMAP, 2);
        DEFINE_FINAL_FLAG(LINK_NOLINEDEF, 3, LinkFlags);
        //@}

    public:
        Thing();

    // @todo Make private.
        /// LineDefs to which this is linked.
        NodePile::Index lineRoot;

        /// Links in Sector (if needed).
        Thing* sNext, **sPrev;

        /// Subsector in which this resides.
        Subsector* subsector;

        /// Location of the thing's origin within the object's local space.
        AnimatorVector3 origin;

        /// Radius of the thing.
        Animator radius;

    private:
        /// Type identifier.
        String _id;

        /// @todo  Appearance: sprite frame, 3D model, etc.
        ///        Behavior: states, scripts, counters, etc.
        
        /// Overall opacity of the thing (1.0 = fully opaque, 0.0 = invisible).
        Animator _opacity;

        /// Height of the thing.
        Animator _height;
    };
}

#endif /* LIBDENG2_THING_H */
