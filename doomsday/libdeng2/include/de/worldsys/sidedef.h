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

#ifndef LIBDENG2_SIDEDEF_H
#define LIBDENG2_SIDEDEF_H

#include "../Error"
#include "../Flag"
#include "../MSurface"

namespace de
{
    // Used with FakeRadio.
    /*typedef struct {
        dfloat corner;
        class Sector* proximity;
        dfloat pOffset;
        dfloat pHeight;
    } shadowcorner_t;

    typedef struct {
        dfloat length;
        dfloat shift;
    } edgespan_t;

    typedef struct {
        dint fakeRadioUpdateCount; // frame number of last update
        shadowcorner_t topCorners[2];
        shadowcorner_t bottomCorners[2];
        shadowcorner_t sideCorners[2];
        edgespan_t spans[2];      // [left, right]
    } sideradioconfig_t;*/

    class LineDef;
    class Sector;

    class SideDef
    {
    public:
        /// Unknown property name was given. @ingroup errors
        DEFINE_ERROR(UnknownPropertyError);
        /// Attempt to access missing Sector. @ingroup errors
        DEFINE_ERROR(MissingSectorError);
        /// Attempt to access missing LineDef. @ingroup errors
        DEFINE_ERROR(MissingLineDefError);

        /** @name SideDef Flags */
        //@{
        DEFINE_FLAG(BLEND_TOP2MIDDLE, 1);
        DEFINE_FLAG(BLEND_MIDDLE2TOP, 2);
        DEFINE_FLAG(BLEND_MIDDLE2BOTTOM, 3);
        DEFINE_FLAG(BLEND_BOTTOM2MIDDLE, 4);
        /// When drawing, stretch the middle surface to reach from floor to ceiling.
        DEFINE_FINAL_FLAG(MIDDLE_STRETCH, 5, Flags);
        //@}

        enum Section {
            MIDDLE = 0,
            TOP,
            BOTTOM
        };

    public:
        //sideradioconfig_t radioConfig;
        struct buildData {
            // SideDef index. Always valid after loading & pruning.
            dint index;
            dint refCount;
        } buildData;

        SideDef(Sector* sector, dshort flags, Material* middleMaterial = 0,
            const Vector2f& middleOffset = Vector2f(0, 0),
            const Vector3f& middleTintColor = Vector3f(1, 1, 1),
            dfloat middleOpacity = 1, Material* topMaterial = 0,
            const Vector2f& topOffset = Vector2f(0, 0),
            const Vector3f& topTintColor = Vector3f(1, 1, 1),
            Material* bottomMaterial = 0,
            const Vector2f& bottomOffset = Vector2f(0, 0),
            const Vector3f& bottomTintColor = Vector3f(1, 1, 1));
        ~SideDef();

        void setLineDef(LineDef* lineDef);

        void colorTints(Section section, const dfloat** topColor, const dfloat** bottomColor);

        MSurface& middle() { return _middle; }
        MSurface& top() { return _top; }
        MSurface& bottom() { return _bottom; }

        bool hasSector() const { return _sector != 0; }
        Sector& sector() const {
            if(!hasSector())
                /// @throw MissingSectorError Attempt was made to access Sector when not present.
                throw MissingSectorError("SideDef::sector", "Sector not present.");
            return *_sector;
        }

        bool hasLineDef() const { return _lineDef != 0; }
        LineDef& lineDef() const {
            if(!hasLineDef())
                /// @throw MissingLineDefError Attempt was made to access LineDef when not present.
                throw MissingLineDefError("LineDef::lineDef", "LineDef not present.");
            return *_lineDef;
        }

        /**
         * Get the value of a sidedef property, selected by DMU_* name.
         */
        //bool getProperty(setargs_t* args) const;

        /**
         * Update the sidedef, property is selected by DMU_* name.
         */
        //bool setProperty(const setargs_t* args);

    private:
        Flags _flags;
        LineDef* _lineDef;
        Sector* _sector;

        MSurface _middle;
        MSurface _top;
        MSurface _bottom;

        /// Extra information about this sidedef.
        Record _info;
    };
}

#endif /* LIBDENG2_SIDEDEF_H */
