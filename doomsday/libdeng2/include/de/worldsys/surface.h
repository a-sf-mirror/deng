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

#ifndef LIBDENG2_MSURFACE_H
#define LIBDENG2_MSURFACE_H

#include "deng.h"

#include "../App"
#include "../Flag"
#include "../Render"
#include "../Vector"

namespace de
{
    class Decoration;
    class Material;

    class MSurface
    {
    public:
        /** @name Surface Flags */
        //@{
        /// Surface glows (drawn fully bright).
        DEFINE_FLAG(GLOW, 1);
        /// Material is flipped horizontally.
        DEFINE_FLAG(MATERIAL_FLIPH, 2);
        /// Material is flipped vertically.
        DEFINE_FINAL_FLAG(MATERIAL_FLIPV, 3, Flags);
        //@}

        /// Maximum speed for a smoothed material offset change (units per tic).
        static const dint MAX_MATERIAL_SMOOTHSCROLL_DISTANCE = 8;

    public:
        void* owner; // Either @c DMU_SIDEDEF, or @c DMU_PLANE

        /// Surface flags.
        Flags flags;

        /// Surface normal.
        Vector3f normal;

        /// Offset to material origin.
        Vector2f offset;

        /// Old material origin offsets (for smoothing).
        Vector2f oldOffset[2];

        /// Offset to material origin (smoothed).
        Vector2f visOffset;
        Vector2f visOffsetDelta;

        /// Surface color tint
        Vector3f tintColor;

        /// Overall opacity (1.0 = fully opaque, 0.0 = invisible).
        dfloat opacity;

        Blendmode blendmode;

        MSurface(const Vector3f& normal, Material* material,
                 const Vector2f& materialOffset = Vector2f(0, 0),
                 dfloat opacity = 1, Blendmode blendmode = BM_NORMAL,
                 const Vector3f& tintColor = Vector3f(1, 1, 1));
        ~MSurface();

        Material& material() const { return *_material; }

        /**
         * Mark the surface as requiring a full update. Called during engine-reset.
         */
        void update();

        /**
         * $smoothmatoffset: Roll the Surface material offset tracker buffers.
         */
        void updateScroll();

        /**
         * $smoothmatoffset: Interpolate the Surface, Material offset.
         */
        void interpolateScroll(dfloat frameTimePos);

        void resetScroll();

        /**
         * Change material.
         *
         * @param mat           Material to change to.
         * @param smooth        @c true = allow blending.
         */
        void setMaterial(Material* mat, bool smooth);

        /**
         * Set material offset.
         */
        void setMaterialOffsetX(dfloat x);
        void setMaterialOffsetY(dfloat y);
        void setMaterialOffset(dfloat x, dfloat y);

        /**
         * Set tint color. Values are clamped in range 0..1
         */
        void setTintColorRed(dfloat val);
        void setTintColorGreen(dfloat val);
        void setTintColorBlue(dfloat val);
        void setTintColor(const Vector3f& color);
        void setTintColor(dfloat red, dfloat green, dfloat blue) {
            setTintColor(Vector3f(red, green, blue));
        }

        /**
         * Set opacity. Value is clamped in range 0..1
         */
        void setOpacity(dfloat val);

        void setBlendmode(Blendmode blendmode);

        /**
         * Adds a decoration to the surface.
         *
         * @param decoration  Decoration to add. Surface takes ownership.
         */
        Decoration& add(Decoration* decoration);

        void destroyDecorations();

        /**
         * Get the value of a surface property, selected by DMU_* name.
         */
        //bool getProperty(setargs_t* args) const;

        /**
         * Update the surface, property is selected by DMU_* name.
         */
        //bool setProperty(const setargs_t* args);

    private:
        /** @name SurfaceInternalFlags */
        //@{
        /// Surface is currently potentially visible.
        DEFINE_FLAG(POTENTIALLYVISIBLE, 1);
        /// Current texture is a fix replacement (not sent to clients, returned via DMU etc).
        DEFINE_FLAG(MATERIALFIX, 2);
        /// Surface possibly has a blended texture.
        DEFINE_FLAG(BLEND, 3);
        /// No fakeradio for this surface.
        DEFINE_FINAL_FLAG(NORADIO, 4, InternalFlags);
        //@}

    private:
        InternalFlags _inFlags;

        typedef std::vector<Decoration*> Decorations;

        Decorations _decorations;
        bool _updateDecorations;

        Material* _material;
        Material* _materialB;
        dfloat _materialBlendFactor;
    };
}

#endif /* LIBDENG2_MSURFACE_H */
