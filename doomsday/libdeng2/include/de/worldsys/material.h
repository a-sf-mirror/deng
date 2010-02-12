/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2009-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_MATERIAL_H
#define LIBDENG2_MATERIAL_H

#include "../Flag"
#include "../Error"
#include "../Time"
#include "../Vector"
#include "../Texture"

#include <vector>
#include <sstream>

namespace de
{
    typedef enum material_namespace_e {
        MN_ANY = -1,
        MN_FIRST,
        MN_TEXTURES = MN_FIRST,
        MN_FLATS,
        MN_SPRITES,
        MN_SYSTEM,
        NUM_MATERIAL_NAMESPACES
    } material_namespace_t;

    /**
     * @defGroup materialFlags Material Flags
     */
    /*@{*/
    #define MATF_NO_DRAW            0x0001 // Material should never be drawn.
    #define MATF_GLOW               0x0002 // Glowing material.
    #define MATF_SKYMASK            0x0004 // Sky-mask surfaces using this material.
    /*@}*/

    typedef enum {
        MEC_UNKNOWN = -1,
        MEC_METAL = 0,
        MEC_ROCK,
        MEC_WOOD,
        MEC_WATER,
        MEC_CLOTH,
        NUM_MATERIAL_ENV_CLASSES
    } material_env_class_t;

    /**
     * @defGroup materialPrepareFlags Material Prepare Flags
     */
    /*@{*/
    #define MPF_SMOOTH          0x1
    #define MPF_AS_SKY          0x2
    #define MPF_AS_PSPRITE      0x4
    #define MPF_TEX_NO_COMPRESSION 0x8
    /*@}*/

    typedef struct material_prepare_params_s {
        dint tmap, tclass;
    } material_prepare_params_t;

    // Material texture unit idents:
    enum {
        MTU_PRIMARY,
        MTU_DETAIL,
        MTU_REFLECTION,
        MTU_REFLECTION_MASK,
        NUM_MATERIAL_TEXTURE_UNITS
    };

    typedef struct material_textureunit_s {
        const struct gltexture_inst_s* texInst;
        dint magMode;
        //blendmode_t blendMode; // Currently used only with reflection.
        dfloat alpha;
        Vector2f scale, offset; // For use with the texture matrix.
    } material_textureunit_t;

    typedef struct material_snapshot_s {
        dshort width, height; // In world units.
        bool isOpaque;
        dfloat color[3]; // Average color (for lighting).
        dfloat topColor[3]; // Averaged top line color, used for sky fadeouts.
        dfloat shinyMinColor[3];
        material_textureunit_t units[NUM_MATERIAL_TEXTURE_UNITS];
    } material_snapshot_t;

    class Material
    {
    public:
        /// Attempt was made to access a layer when not present. @ingroup errors
        DEFINE_ERROR(MissingLayerError);
        /// Attempted to create a new layer when the maximum number of layers are already in use. @ingroup errors
        DEFINE_ERROR(OutOfLayersError);

        /// Maximum number of texture layers per Material.
        static const dbyte MAX_LAYERS = 2;

        class Layer
        {
        public:
            /**
             * @defGroup materialLayerFlags Material Layer Flags
             */
            /*@{*/
            #define MASKED 0x1
            /*@}*/

        public:
            /// Current interpolated position.
            Vector2f texPosition;

            Layer(dbyte flags, TextureId tex, const Vector2f& origin,
                  dfloat scrollSpeed, dfloat scrollAngle);

            void deleteTexture();

            dbyte flags() const { return _flags; }
            const Vector2f& textureOrigin() const { return _texOrigin; }
            dfloat textureOriginX() const { return _texOrigin.x; }
            dfloat textureOriginY() const { return _texOrigin.y; }
            dfloat scrollSpeed() const { return _scrollSpeed; }
            dfloat scrollAngle() const { return _scrollAngle; }

            void setTexture(TextureId tex);
            void setTextureOrigin(const Vector2f& origin);
            void setTextureOriginX(dfloat originX);
            void setTextureOriginY(dfloat originY);

            void setFlags(dbyte flags);
            void setScrollAngle(dfloat angle);
            void setScrollSpeed(dfloat speed);

        private:
            /// MLF_* flags, @see materialLayerFlags
            dbyte _flags;

            TextureId _tex;

            Vector2f _texOrigin;

            dfloat _scrollAngle;

            dfloat _scrollSpeed;
        };

    public:
        material_namespace_t mnamespace;

        const struct ded_material_s* def;  // Can be NULL.
        bool isAutoMaterial; // Was generated automatically.

#if 0
        struct ded_detailtexture_s* detail;
        struct ded_decor_s* decoration;
        struct ded_ptcgen_s* generator;
        struct ded_reflection_s* reflection;
#endif

        dshort flags; // MATF_* flags

        /// Width of the material in world units.
        dfloat width;

        /// Height of the material in world units.
        dfloat height;

        typedef std::vector<Layer> Layers;
        Layers layers;

        material_env_class_t envClass; // Used for environmental sound properties.

        Material* current;
        Material* next;
        dfloat inter;

        /// True if belongs to some animgroup.
        bool inAnimGroup;

        Material* globalNext; // Linear list linking all materials.

        void think(const Time::Delta& elapsed);

        //dbyte prepare(struct material_snapshot_s* snapshot, dbyte flags, const material_prepare_params_t* params);
        //void deleteTextures();

        /**
         * Prepares all resources associated with the specified material including
         * all in the same animation group.
         *
         * \note Part of the Doomsday public API.
         *
         * \todo What about the load params? By limiting to the default params here, we
         * may be precaching unused texture instances.
         */
        //void precache();

        /**
         * @param tex           GLTexture to use with this layer.
         */
        Layers::size_type addLayer(dbyte flags, TextureId tex, dfloat xOrigin, dfloat yOrigin, dfloat moveAngle, dfloat moveSpeed);

#if 0
        /**
         * Retrieve the decoration definition associated with the material.
         *
         * @return              The associated decoration definition, else @c NULL.
         */
        const ded_decor_t* decorationDef() {
            // Ensure we've already prepared this material.
            Materials_Prepare(this, 0, NULL, NULL);
            return decoration;
        }

        /**
         * Retrieve the ptcgen definition associated with the material.
         *
         * @return              The associated ptcgen definition, else @c NULL.
         */
        const ded_generator_t* generatorDef() {
            //Ensure we've already prepared this material.
            //Materials_Prepare(NULL, mat, 0, NULL);
            return generator;
        }

        material_env_class_t environmentClass() {
            if(envClass == MEC_UNKNOWN)
                envClass = S_MaterialClassForName(Materials_NameOf(this), mnamespace);
            if(!(flags & MATF_NO_DRAW))
                return envClass;
            return MEC_UNKNOWN;
        }
#endif

        //bool isFromIWAD() const;

        bool isSky() const { return (flags & MATF_SKYMASK) != 0; }

        Layer& layer(dbyte layerNum) {
            if(!(layerNum < layers.size()))
            {
                std::ostringstream os;
                os << "Invalid layer num #" << layerNum << ".";
                /// @throw MissingLayerError Attempt was made to access a missing MaterialLayer.
                throw MissingLayerError("Material::layer", os.str());
            }
            return layers[layerNum];
        }

        void setWidth(dfloat width);
        void setHeight(dfloat height);
        void setFlags(dshort flags);
        void setTranslation(Material& current, Material& next, dfloat inter);

        /**
         * Get the value of a material property, selected by DMU_* name.
         */
        //bool getProperty(setargs_t* args) const;

        /**
         * Update the material, property is selected by DMU_* name.
         */
        //bool setProperty(const setargs_t* args);
    };
}

#endif /* LIBDENG2_MATERIAL_H */
