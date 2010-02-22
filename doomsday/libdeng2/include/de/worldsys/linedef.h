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

#ifndef LIBDENG2_LINEDEF_H
#define LIBDENG2_LINEDEF_H

#include "../Flag"
#include "../Error"
#include "../Vector"
#include "../HalfEdgeDS"
#include "../Vertex"
#include "../SideDef"
#include "../Sector"
#include "../Seg"
#include "../MapRectangle"
#include "../Line"

namespace de
{
    class LineDef
    {
    public:
        /// Attempt to access missing front SideDef @ingroup errors
        DEFINE_ERROR(MissingFrontError);
        /// Attempt to access missing back SideDef @ingroup errors
        DEFINE_ERROR(MissingBackError);

        static const dbyte FRONT = 0;
        static const dbyte BACK = 1;

        typedef enum {
            ST_HORIZONTAL,
            ST_VERTICAL,
            ST_POSITIVE,
            ST_NEGATIVE
        } slopetype_t;

    public:
        lineowner_t* vo[2]; // Links to vertex line owner nodes [left, right]
        HalfEdge* halfEdges[2]; // [leftmost front seg, rightmost front seg]
        Vector2d direction;
        dbinangle angle; // Calculated from front side's normal
        dfloat length; // Accurate length
        slopetype_t slopeType;

        dint flags; // Public DDLF_* flags.
        bool polyobjOwned;
        dint validCount;

        //bool mapped[DDMAXPLAYERS]; // Whether the line has been mapped by each player yet.
        //duint16 shadowVisFrame[2]; // Framecount of last time shadows were drawn for this line, for each side [right, left].

        struct BuildData {
            Vertex* v[2];
            SideDef* sideDefs[2];
            // LineDef index. Always valid after loading & pruning of zero
            // length lines has occurred.
            dint index;

            // One-sided linedef used for a special effect (windows).
            // The value refers to the opposite sector on the back side.
            Sector* windowEffect;

            // Normally NULL, except when this linedef directly overlaps an earlier
            // one (a rarely-used trick to create higher mid-masked textures).
            // No segs should be created for these overlapping linedefs.
            LineDef* overlap;
        } buildData;

        dfloat lightLevelDelta() const;

        bool hasFront() const { return reinterpret_cast<Seg*>(halfEdges[0]->data)->hasSideDef(); }
        bool hasBack() const { return reinterpret_cast<Seg*>(halfEdges[0]->twin->data)->hasSideDef(); }

        Vertex& vtx1() const { return *halfEdges[0]->vertex; }
        Vertex& vtx2() const { return *halfEdges[1]->twin->vertex; }
        Vertex& vtx(bool to) const { return to? vtx2() : vtx1(); }

        SideDef& front() const {
            if(!hasFront())
                /// @throw MissingFrontError Attempt was made to access front SideDef when not present.
                throw MissingFrontError("LineDef::front", "Front SideDef not present.");
            return reinterpret_cast<Seg*>(halfEdges[0]->data)->sideDef();
        }
        SideDef& back() const {
            if(!hasBack())
                /// @throw MissingFrontError Attempt was made to access back SideDef when not present.
                throw MissingBackError("LineDef::back", "Back SideDef not present.");
            return reinterpret_cast<Seg*>(halfEdges[0]->twin->data)->sideDef();
        }

        bool hasFrontSector() const { return hasFront() && front().hasSector(); }
        bool hasBackSector() const { return hasBack() && back().hasSector(); }

        Sector& frontSector() const { return front().sector(); }
        Sector& backSector() const { return back().sector(); }

        bool isSelfreferencing() const {
            return hasFront() && hasBack() && &front().sector() == &back().sector();
        }

        const MapRectangled& aaBounds() const { return _aaBounds; }

        /**
         * Does the LineDef qualify as an edge shadow caster?
         */
        bool isShadowing() const {
            if(!polyobjOwned && !isSelfreferencing() &&
               !(vo[0]->next().lineDef == this || vo[1]->next().lineDef == this))
                return true;
            return false;
        }

        /**
         * Returns a two-component float unit vector parallel to the line.
         */
        Vector2f unitVector() const;

        /// Conversion operator to Line2.
        template <typename Type>
        operator Line2<Type> () const {
            return Line2<Type>(vtx1().pos, delta);
        }

        /**
         * Convenience methods for point-on-lineside tests.
         */
        template <typename Type>
        bool side(const Vector2<Type>& point) const {
            return Line2<Type>(*this).side(point);
        }
        template <typename Type>
        bool side(Type x, Type y) const {
            return side(Vector2<Type>(x, y));
        }

        dint boxOnSide(const MapRectangled& rectangle) const;
        dint boxOnSide(const Vector2d& bottomLeft, const Vector2d& topRight) const {
            return boxOnSide(bottomLeft.x, bottomLeft.y, topRight.x, topRight.y);
        }
        dint boxOnSide(ddouble xl, ddouble xh, ddouble yl, ddouble yh) const;

        void updateAABounds();

        /**
         * Get the value of a linedef property, selected by DMU_* name.
         */
        //bool getProperty(setargs_t* args) const;

        /**
         * Update the linedef, property is selected by DMU_* name.
         */
        //bool setProperty(const setargs_t* args);

        bool iterateThings(bool (*callback) (Thing*, void*), void* paramaters = 0);

    private:
        /// Axis-Aligned Bounding box.
        MapRectangled _aaBounds;

        /// Extra information about this linedef.
        Record _info;
    };
}

#endif /* LIBDENG2_LINEDEF_H */
