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

#ifndef LIBDENG2_SECTOR_H
#define LIBDENG2_SECTOR_H

#include "../Error"
#include "../Flag"
#include "../Id"
#include "../Plane"
#include "../MapRectangle"

#include <set>

namespace de
{
    class LineDef;
    class Subsector;
    class Thing;

    class Sector
    {
    public:
        /// Unknown property name was given. @ingroup errors
        DEFINE_ERROR(UnknownPropertyError);

        /** @name Sector Flags */
        //@{
        /// An unclosed sector (some sort of fancy hack).
        DEFINE_FINAL_FLAG(UNCLOSED, 1, Flags);
        //@}

        /** @name Sector Frame Flags */
        //@{
        /// Sector is visible on this frame.
        DEFINE_FLAG(FRAME_VISIBLE, 1);
        DEFINE_FINAL_FLAG(FRAME_LIGHT_CHANGED, 2, FrameFlags);
        //@}

        /// Flags to clear before each frame.
        static const Flag FRAME_CLEARMASK = FRAME_VISIBLE;

        typedef std::set<LineDef*> LineDefSet;
        typedef std::set<Subsector*> SubsectorSet;

    public:
        FrameFlags frameFlags;
        Flags flags;

        /// Axis-Aligned Bounding box.
        MapRectanglef _aaBounds;

        /// Rough approximation of the area of this Sector.
        dfloat approxArea;

        /// Head of the list of Things in this Sector.
        Thing* thingList;

        /// Set of LineDefs which reference this Sector.
        LineDefSet lineDefs;

        /// Set of Subsectors of this Sector.
        SubsectorSet subsectors;

        /// Set of Subsectors which contribute to this Sector's environmental audio characteristics.
        SubsectorSet reverbSubsectors;

        /// Environmental audio characteristics.
        //dfloat reverb[NUM_REVERB_DATA];
        //ddmobj_base_t soundOrg;

        dint validCount; // if == validCount, already checked.

        struct BuildData {
            // Sector index. Always valid after loading & pruning.
            dint index;
            // Suppress superfluous mini warnings.
            dint warnedFacing;
            dint refCount;
        } buildData;

        Sector(dfloat lightIntensity = 1, const Vector3f& lightColor = Vector3f(1, 1, 1));

        ~Sector();

        void updateSoundEnvironment();

        /// Retrieve the current light intensity.
        dfloat lightIntensity() const;

        Plane& floor() const { return *_floorPlane; }

        Plane& ceiling() const { return *_ceilingPlane; }

        /// Change the light intensity (with smoothing).
        void setLightIntensity(dfloat);

        /**
         * @pre Lines in sector must be setup before this is called!
         */
        void updateAABounds();

        const MapRectanglef aaBounds() const { return _aaBounds; }

        /**
         * Does Sector contain any sky surfaces?
         */
        bool hasSkySurface() const {
            return floor().material().isSky() || ceiling().material().isSky();
        }

        /**
         * After modifying the floor or ceiling height, call this routine
         * to adjust the positions of all Thing that touch this Sector.
         *
         * If anything doesn't fit anymore, true will be returned.
         */
        bool planesChanged();

        /**
         * Called when a floor or ceiling height changes to update the plotted
         * decoration origins for surfaces whose material offset is dependant upon
         * the given plane.
         */
        void markDependantSurfacesForDecorationUpdate();

        /**
         * @fixme Fundamentally flawed; Assumes Sector == 1 n-sided polygon!
         *
         * Is the point inside the Sector, according to the edge LineDefs of the
         * Subsector. Uses the well-known algorithm described here:
         * http://www.alienryderflex.com/polygon/
         *
         * @param               X coordinate to test.
         * @param               Y coordinate to test.
         *
         * @return              @c true, if the point is inside.
         */
        bool pointInside(dfloat x, dfloat y) const;

        /**
         * @fixme Fundamentally flawed; Assumes Sector == 1 n-sided polygon!
         *
         * Is the point inside the Sector, according to the edge LineDefs of the
         * Subsector. Uses the well-known algorithm described here:
         * http://www.alienryderflex.com/polygon/
         *
         * More accurate than pointInside.
         *
         * @param               X coordinate to test.
         * @param               Y coordinate to test.
         *
         * @return              @c true, if the point is inside.
         */
        bool pointInside2(dfloat x, dfloat y) const;

        void clearFrameFlags();

        /**
         * Get the value of a sector property, selected by DMU_* name.
         */
        //bool getProperty(setargs_t* args) const;

        /**
         * Update the sector, property is selected by DMU_* name.
         */
        //bool setProperty(const setargs_t* args);

        /**
         * Increment validCount before using this. 'callback' is called for each Thing
         * that is (even partly) inside the Sector. This is not a 3D test, the Things
         * may actually be above or under the Sector's ceiling/floor (respectively).
         */
        bool iterateThingsTouching(bool (*callback) (Thing*, void*), void* paramaters = 0);

    private:
        /// Plane describing the floor of this Sector.
        Plane* _floorPlane;

        /// Plane describing the ceiling of this Sector.
        Plane* _ceilingPlane;

        /// Ambient light intensity.
        dfloat _lightIntensity;

        /// Ambient light color.
        Vector3f _lightColor;

        /// Overriding light source.
        Sector* lightSource;

        /// Number of gridblocks in the sector (bias light model).
        duint blockCount;

        /// Number of blocks to mark changed (bias light model).
        duint changedBlockCount;

        /// Light grid block indices (bias light model).
        dushort* blocks;

        /// Extra information about this sector.
        Record _info;
    };
}

#endif /* LIBDENG2_SECTOR_H */
