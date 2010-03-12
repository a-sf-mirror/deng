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

#ifndef LIBDENG2_SUBSECTOR_H
#define LIBDENG2_SUBSECTOR_H

#include "de/Error"
#include "de/Vector"
#include "de/Plane"
#include "de/Sector"
#include "de/HalfEdgeDS"

namespace de
{
    class Polyobj;
    class Seg;

    class Subsector
    {
    public:
        /// Attempt to access Sector when not present. @ingroup errors
        DEFINE_ERROR(MissingSectorError);

    public:
        duint hEdgeCount;
        Polyobj* polyobj;
        /// Frame number of last R_AddSprites.
        dint addSpriteCount;

        dint validCount;

        //duint reverb[NUM_REVERB_DATA];

        /// Offset to align the top left of the bBox to the world grid.
        Vector2f worldGridOffset;

        /// Center of the subsector.
        Vector2f midPoint;

        //struct shadowlink_s* shadows;
        //struct biassurface_s** bsuf; // [sector->planeCount] size.

        HalfEdge* firstFanHEdge;
        bool useMidPoint;

    public:
        Subsector(Face& face, Sector* sector);

        ~Subsector();

        bool hasSector() const { return _sector != 0; }
        Sector& sector() const {
            if(!hasSector())
                /// @throw MissingSectorError Attempt to access Sector when not present.
                throw MissingSectorError("Subsector::sector", "Sector not present.");
            return *_sector;
        }

        Plane& floor() const { return sector().floor(); }
        Plane& ceiling() const { return sector().ceiling(); }

        const MapRectanglef& aaBounds() const { return _aaBounds; }

        void spreadObjs();
        void updateMidPoint();

        bool pointInside(dfloat x, dfloat y) const;
        bool pointInside(const Vector2f& pos) const { return pointInside(pos.x, pos.y); }

        /**
         * Walk the half-edges of the specified subsector, looking for a half-edge
         * that is suitable for use as the base of a tri-fan.
         *
         * We do not want any overlapping tris so check that the area of each triangle
         * is > 0, if not; try the next vertice until we find a good one to use as the
         * center of the trifan. If a suitable point cannot be found use the center of
         * subsector instead (it will always be valid as subsectors are convex).
         */
        void pickFanBaseSeg();

        /**
         * Get the value of a subsector property, selected by DMU_* name.
         */
        //bool getProperty(setargs_t* args) const;

        /**
         * Update the subsector, property is selected by DMU_* name.
         */
        //bool setProperty(const setargs_t* args);

    private:
        Face& _face;
        Sector* _sector;

        /// Axis-Aligned Bounding box.
        MapRectanglef _aaBounds;
    };
}

#endif /* LIBDENG2_SUBSECTOR */
