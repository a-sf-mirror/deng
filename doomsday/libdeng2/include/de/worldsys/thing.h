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

#include "../Error"
#include "../Flag"
#include "../String"
#include "../Animator"
#include "../NodePile"
#include "../Subsector"
#include "../User"

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
        /// Attempt to access user when not present. @ingroup errors
        DEFINE_ERROR(MissingUserError);

        /** @name Thing Link Flags */
        //@{
        DEFINE_FLAG(LINK_SECTOR, 1);
        DEFINE_FLAG(LINK_BLOCKMAP, 2);
        DEFINE_FINAL_FLAG(LINK_NOLINEDEF, 3, LinkFlags);
        //@}

        /// Maximum radius of a thing in world units.
        static const dint MAXRADIUS = 32;

    public:
        Thing();

        /// Is this thing owned by a user?
        bool hasUser() const { return _user != 0; }

        /// Retrieve the User of this thing.
        User& user() {
            if(!hasUser())
                /// @throw MissingUserError Attempt to access user when not present.
                throw MissingUserError("Thing::user", "No user present.");
            return *_user;
        }

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

        /// If the thing represents a User, this is it.
        User* _user;

        /// @todo  Appearance: sprite frame, 3D model, etc.
        ///        Behavior: states, scripts, counters, etc.
        
        /// Overall opacity of the thing (1.0 = fully opaque, 0.0 = invisible).
        Animator _opacity;

        /// Height of the thing.
        Animator _height;

        /// Extra information about this linedef.
        Record _info;
    };
}

#endif /* LIBDENG2_THING_H */
