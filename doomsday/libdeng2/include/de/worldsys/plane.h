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

#ifndef LIBDENG2_PLANE_H
#define LIBDENG2_PLANE_H

#include "../MSurface"
#include "../Material"
#include "../Vector"
#include "../Render"

namespace de
{
    class Plane
    {
    public:
        /// Maximum speed for a smoothed plane (world units per tic).
        static const dint MAX_SMOOTH_MOVE = 64;

    public:
        /// Current height.
        dfloat height;

        /// Target height.
        dfloat target;

        /// Height tracking buffer.
        dfloat oldHeight[2];

        /// Visible plane height (smoothed).
        dfloat visHeight;

        /// Visible plane height delta (smoothed).
        dfloat visHeightDelta;

        /// Glow intensity.
        dfloat glowIntensity; 

        /// Glow color.
        Vector3f glowColor;

        /// Current speed.
        dfloat momentum; 

        struct BuildData{
            duint index;
        } buildData;

        Plane(dfloat height, const Vector3f& normal, Material* material,
              const Vector2f& materialOffset = Vector2f(0, 0),
              dfloat opacity = 1, Blendmode blendmode = BM_NORMAL,
              const Vector3f& tintColor = Vector3f(1, 1, 1),
              dfloat glowIntensity = 0, const Vector3f& glowColor = Vector3f(1, 1, 1));
        ~Plane();

        void setNormal(const Vector3f& normal);

        MSurface& surface() { return _surface; }

        /// Convenient access method to the material of this plane's surface.
        Material& material() const { return _surface.material(); }

        /**
         * Is the plane glowing (it glows or is a sky mask surface)?
         *
         * @return              @c true, if the specified plane is non-glowing,
         *                      i.e. not glowing or a sky.
         */
        bool isGlowing() const {
            const Material& mat = material();
            return (mat.flags & MATF_NO_DRAW) || glowIntensity > 0 || mat.isSky();
        }

        void resetHeightTracking();
        void updateHeightTracking();
        void interpolateHeight(dfloat frameTimePos);

        //bool getProperty(setargs_t* args) const;
        //bool setProperty(const setargs_t* args);

    private:
        MSurface _surface;
    };
}

#endif /* LIBDENG2_PLANE_H */
