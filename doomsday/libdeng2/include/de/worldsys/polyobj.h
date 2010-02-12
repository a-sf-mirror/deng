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

#ifndef LIBDENG2_POLYOBJ_H
#define LIBDENG2_POLYOBJ_H

#include "../Vector"

#include <vector>

namespace de
{
    class Subsector;
    class LineDef;
    class Seg;

    class Polyobj
    {
    public:
        typedef std::vector<Vector2f> EdgePoints;

    public:
        /// Thinker node.
        //thinker_t thinker;

        /// Position [x,y,z]
        Vector2f origin;

        /// Subsector in which this resides.
        Subsector* subsector;

        /// Idx of polyobject.
        duint idx;

        /// Reference tag.
        dint tag;

        dint validCount;

        dfloat box[2][2];

        /// Destination XY.
        dfloat dest[2];

        dangle angle;

        /// Destination angle.
        dangle destAngle;

        /// Rotation speed.
        dangle angleSpeed;

        duint _numLineDefs;
        LineDef** lineDefs;

        duint _numSegs;
        Seg* segs;

        /// Used as the base for the rotations.
        EdgePoints _originalPts;

        /// Use to restore the old point values.
        EdgePoints _prevPts;

        /// Movement speed.
        dfloat speed;

        /// Should the polyobj attempt to crush Things?
        bool crush;

        dint seqType;

        struct BuildData {
            dint index;
        } buildData;

        ~Polyobj();

        /**
         * Called at the start of the map after all the structures needed for
         * refresh have been setup.
         */
        void initalize();

        bool translate(const Vector2f& delta);

        bool rotate(dangle angle);

        /**
         * Iterate the LineDefs linked to this Polyobj, making a callback for each
         * unless a callback returns false at which point iteration will stop.
         *
         * @param callback      Function to call for each lineDef.
         * @return              @c true, if all callbacks return @c true.
         */
        bool iterateLineDefs(bool (*callback) (LineDef*, void*), bool retObjRecord, void* paramaters = 0);

        //bool getProperty(setargs_t* args) const;
        //bool setProperty(const setargs_t *args);

    private:
        /// Update the Axis-Aligned Bounding box.
        void updateAABounds();

        void changed();

        void linkInLineDefBlockmap();

        void unlinkInLineDefBlockmap();
    };
}

#endif /* LIBDENG2_POLYOBJ_H */
