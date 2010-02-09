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

#ifndef LIBDENG2_MSURFACE_H
#define LIBDENG2_MSURFACE_H

#include "deng.h"

namespace de
{
    // Decoration types.
    /*typedef enum {
        DT_LIGHT,
        DT_MODEL,
        NUM_DECORTYPES
    } decortype_t;

    // Helper macros for accessing decor data.
    #define DEC_LIGHT(x)         (&((x)->data.light))
    #define DEC_MODEL(x)         (&((x)->data.model))

    typedef struct surfacedecor_s {
        dfloat              pos[3]; // World coordinates of the decoration.
        decortype_t         type;
        Subsector*        subsector;
        union surfacedecor_data_u {
            struct surfacedecor_light_s {
                const struct ded_decorlight_s* def;
            } light;
            struct surfacedecor_model_s {
                const struct ded_decormodel_s* def;
                struct modeldef_s* mf;
                dfloat              pitch, yaw;
            } model;
        } data;
    } surfacedecor_t;*/

    class MSurface
    {
    public:
        // Internal surface flags:
        /*#define SUIF_PVIS             0x0001
        #define SUIF_MATERIAL_FIX     0x0002 // Current texture is a fix replacement
                                             // (not sent to clients, returned via DMU etc).
        #define SUIF_BLEND            0x0004 // Surface possibly has a blended texture.
        #define SUIF_NO_RADIO         0x0008 // No fakeradio for this surface.

        #define SUIF_UPDATE_FLAG_MASK 0xff00
        #define SUIF_UPDATE_DECORATIONS 0x8000*/

    public:
        void* owner; // Either @c DMU_SIDEDEF, or @c DMU_PLANE
        dint flags; // SUF_ flags
        dint oldFlags;
        //struct material_t* material;
        //struct material_t* materialB;
        //dfloat matBlendFactor;
        //blendmode_t blendMode;
        Vector3f normal; // Surface normal
        Vector3f oldNormal;

        dfloat offset[2]; // [X, Y] Planar offset to surface material origin.
        dfloat oldOffset[2][2];

        dfloat visOffset[2];
        dfloat visOffsetDelta[2];

        dfloat rgba[4]; // Surface color tint
        dshort inFlags; // SUIF_* flags
        //duint numDecorations;
        //surfacedecor_t* decorations;

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

        //bool isSky() const { return (material && (material->flags & MATF_SKYMASK))? true : false; }

        /**
         * Change Material.
         *
         * @param mat           Material to change to.
         * @param fade          @c true = allow blending
         * @return              @c true, if changed successfully.
         */
        //bool setMaterial(Material* mat, bool fade);

        bool setMaterialOffsetX(dfloat x);
        bool setMaterialOffsetY(dfloat y);
        bool setMaterialOffsetXY(dfloat x, dfloat y);

        bool setColorR(dfloat r);
        bool setColorG(dfloat g);
        bool setColorB(dfloat b);
        bool setColorA(dfloat a);
        bool setColorRGBA(dfloat r, dfloat g, dfloat b, dfloat a);
        //bool setBlendMode(blendmode_t blendMode);

        //bool getProperty(setargs_t* args) const;
        //bool setProperty(const setargs_t* args);
    };
}

#endif /* LIBDENG2_MSURFACE_H */
