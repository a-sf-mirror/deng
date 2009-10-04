/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright Â© 2009 Daniel Swanson <danij@dengine.net>
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

/**
 * p_material.h: Materials for world surfaces.
 */

#ifndef __DOOMSDAY_MATERIAL_H__
#define __DOOMSDAY_MATERIAL_H__

#include "p_dmu.h"

/**
 * @defGroup materialPrepareFlags Material Prepare Flags
 */
/*@{*/
#define MPF_SMOOTH          0x1
#define MPF_AS_SKY          0x2
#define MPF_AS_PSPRITE      0x4
#define MPF_TEX_NO_COMPRESSION 0x8
/*@}*/

typedef struct {
    int             tmap, tclass;
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
    int             magMode;
    blendmode_t     blendMode; // Currently used only with reflection.
    float           alpha;
    float           scale[2], offset[2]; // For use with the texture matrix.
} material_textureunit_t;

typedef struct material_snapshot_s {
    short           width, height; // In world units.
    boolean         isOpaque;
    float           color[3]; // Average color (for lighting).
    float           topColor[3]; // Averaged top line color, used for sky fadeouts.
    material_textureunit_t units[NUM_MATERIAL_TEXTURE_UNITS];

/**
 * \todo: the following should be removed once incorporated into the layers (above).
 */
    struct shinydata_s {
        float           minColor[3];
    } shiny;
} material_snapshot_t;

byte            Material_Prepare(material_snapshot_t* snapshot,
                                 material_t* mat, byte flags,
                                 const material_prepare_params_t* params);
void            Material_Ticker(material_t* mat, timespan_t time);
void            Material_DeleteTextures(material_t* mat);
void            Material_Precache(material_t* mat);

byte            Material_AddLayer(material_t* mat, byte flags, gltextureid_t tex,
                                  float xOrigin, float yOrigin, float moveAngle,
                                  float moveSpeed);

const ded_decor_t* Material_GetDecoration(material_t* mat);
const ded_ptcgen_t* Material_GetPtcGen(material_t* mat);
material_env_class_t Material_GetEnvClass(material_t* mat);

boolean         Material_FromIWAD(const material_t* mat);

byte            Material_GetLayerFlags(const material_t* mat, byte layer);
float           Material_GetLayerTextureOriginX(const material_t* mat, byte layer);
float           Material_GetLayerTextureOriginY(const material_t* mat, byte layer);
void            Material_GetLayerTextureOriginXY(const material_t* mat, byte layer, float offset[2]);

void            Material_SetWidth(material_t* mat, float width);
void            Material_SetHeight(material_t* mat, float height);
void            Material_SetFlags(material_t* mat, short flags);
void            Material_SetTranslation(material_t* mat,
                                        material_t* current,
                                        material_t* next, float inter);

void            Material_SetLayerTexture(material_t* mat, byte layer, gltextureid_t tex);
void            Material_SetLayerTextureOriginX(material_t* mat, byte layer, float offset);
void            Material_SetLayerTextureOriginY(material_t* mat, byte layer, float offset);
void            Material_SetLayerTextureOriginXY(material_t* mat, byte layer, const float offset[2]);

void            Material_SetLayerFlags(material_t* mat, byte layer, byte flags);
void            Material_SetLayerMoveAngle(material_t* mat, byte layer, float angle);
void            Material_SetLayerMoveSpeed(material_t* mat, byte layer, float speed);

// DMU interface:
boolean         Material_GetProperty(const material_t* mat, setargs_t* args);
boolean         Material_SetProperty(material_t* mat, const setargs_t* args);
#endif
