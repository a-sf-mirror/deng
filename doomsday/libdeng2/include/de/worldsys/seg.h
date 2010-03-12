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

#ifndef LIBDENG2_SEG_H
#define LIBDENG2_SEG_H

#include "../Flag"
#include "../Error"
#include "../HalfEdgeDS"
#include "../SideDef"
#include "../Subsector"

namespace de
{
    class Seg
    {
    public:
        /// Unknown property name was given. @ingroup errors
        DEFINE_ERROR(UnknownPropertyError);
        /// Attempt to access SideDef when not present. @ingroup errors
        DEFINE_ERROR(MissingSideDefError);
        /// Attempt to access back Seg when not present. @ingroup errors
        DEFINE_ERROR(MissingBackError);
        /// Attempt to access Subsector when not present. @ingroup errors
        DEFINE_ERROR(MissingSubsectorError);
        /// Attempt to access Vertex when not present. @ingroup errors
        DEFINE_ERROR(MissingVertexError);

        /** @name Seg Frame Flags */
        //@{
        /// Seg is front-facing for the current frame.
        DEFINE_FLAG(FACINGFRONT, 1);
        DEFINE_FINAL_FLAG(BACKSECSKYFIX, 2, FrameFlags);
        //@}

        static const dbyte FRONT = 0;
        static const dbyte BACK = 0;

        enum {
            MIDDLE = 0,
            TOP,
            BOTTOM
        };

    public:
        HalfEdge* halfEdge;
        dangle angle;

        /// @c true if this seg is on the back of the line.
        bool onBack;

        dfloat length; // Accurate length of the segment (v1 -> v2).
        dfloat offset;
        //struct biassurface_s* bsuf[3]; // 0=middle, 1=top, 2=bottom
        FrameFlags frameFlags;

        Seg(HalfEdge& halfEdge, SideDef* sideDef, bool back);
        ~Seg();

        bool hasVertex() const { return halfEdge->vertex != 0; }
        Vertex& vertex() const {
            if(!hasVertex())
                /// @throw MissingVertexError Attempt was made to access Vertex when not present.
                throw MissingVertexError("Seg::vertex", "Vertex not present.");
            return *halfEdge->vertex;
        }

        bool hasSideDef() const { return _sideDef != 0; }
        SideDef& sideDef() const {
            if(!hasSideDef())
                /// @throw MissingSideDefError Attempt was made to access SideDef when not present.
                throw MissingSideDefError("Seg::sideDef", "SideDef not present.");
            return *_sideDef;
        }

        bool hasBack() const { return halfEdge->twin != 0; }
        Seg& back() const {
            if(!hasBack())
                /// @throw MissingBackError Attempt was made to access back Seg when not present.
                throw MissingBackError("Seg::back", "Back Seg not present.");
            return *reinterpret_cast<Seg*>(halfEdge->twin->data);
        }

        bool hasSubsector() const { return halfEdge->face != 0; }
        Subsector& subsector() const {
            if(!hasSubsector())
                /// @throw MissingSubsectorError Attempt was made to access Subsector when not present.
                throw MissingSubsectorError("Seg::subsector", "Subsector not present.");
            return *reinterpret_cast<Subsector*>(halfEdge->face->data);
        }

        /**
         * Get the value of a seg property, selected by DMU_* name.
         */
        //bool getProperty(setargs_t* args) const;

        /**
         * Update the seg, property is selected by DMU_* name.
         */
        // bool setProperty(const setargs_t* args);

    private:
        SideDef* _sideDef;
    };
}

#endif /* LIBDENG2_SEG_H */
