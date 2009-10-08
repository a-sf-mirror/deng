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

/**
 * p_material.c: Materials for world surfaces.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_defs.h"

#include "s_environ.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Material_Ticker(material_t* mat, timespan_t time)
{
    byte                i;

    // Update layers
    for(i = 0; i < mat->numLayers; ++i)
    {
        material_layer_t*   layer = &mat->layers[i];

        if(!INRANGE_OF(layer->moveSpeed, 0, 0.0001f))
        {
            float               ang, offset[2];

            ang = PI * layer->moveAngle / 180;

            V2_Set(offset, cos(ang) * time * layer->moveSpeed,
                           sin(ang) * time * layer->moveSpeed);
            V2_Sum(layer->texPosition, layer->texPosition, offset);
            V2_Sum(layer->texPosition, layer->texPosition, layer->texOrigin);
        }

        /*if(layer->tics-- <= 0)
        {
            // Advance to next stage.
            if(++layer->stage == def->layers[i].stageCount.num)
            {
                // Loop back to the beginning.
                layer->stage = 0;
                continue;
            }

            layer->tics = def->layers[i].stages[layer->stage].tics *
                (1 - def->layers[i].stages[layer->stage].variance *
                    RNG_RandFloat()) * TICSPERSEC;
        }*/
    }
}

/**
 * Subroutine of Material_Prepare().
 */
static __inline void setTexUnit(material_snapshot_t* ss, byte unit,
                                blendmode_t blendMode, int magMode,
                                const gltexture_inst_t* texInst,
                                float sScale, float tScale,
                                float sOffset, float tOffset, float alpha)
{
    material_textureunit_t* mtp = &ss->units[unit];

    mtp->texInst = texInst;
    mtp->magMode = magMode;
    mtp->blendMode = blendMode;
    mtp->alpha = MINMAX_OF(0, alpha, 1);
    mtp->scale[0] = sScale;
    mtp->scale[1] = tScale;
    mtp->offset[0] = sOffset;
    mtp->offset[1] = tOffset;
}

static byte prepareGLTextures(material_t* mat,
                              gltexture_inst_t const ** texInsts, byte flags,
                              const material_prepare_params_t* params)
{
    byte                i, tmpResult = 0;

    // Ensure all resources needed to visualize this material are loaded.
    for(i = 0; i < mat->numLayers; ++i)
    {
        material_layer_t*   ml = &mat->layers[i];
        texture_load_params_t tParams;
        byte                result;

        memset(&tParams, 0, sizeof(tParams));
        if(flags & MPF_AS_SKY)
        {
            tParams.flags |= TLF_LOAD_AS_SKY;
        }
        else if(flags & MPF_AS_PSPRITE)
        {
            if(params)
            {
                tParams.tclass = params->tclass;
                tParams.tmap = params->tmap;
            }
            tParams.flags |= TLF_LOAD_AS_PSPRITE;
        }

        if(ml->flags & MPF_TEX_NO_COMPRESSION)
            tParams.flags |= TLF_NO_COMPRESSION;
        if(!(ml->flags & MATLF_MASKED))
            tParams.flags |= TLF_ZEROMASK;

        // Pick the instance matching the specified context.
        texInsts[i] = GL_PrepareGLTexture(ml->tex, &tParams, &result);

        if(result)
            tmpResult = result;
    }

    return tmpResult;
}

/**
 * @param tex           GLTexture to use with this layer.
 */
byte Material_AddLayer(material_t* mat, byte flags, gltextureid_t tex,
                       float xOrigin, float yOrigin, float moveAngle,
                       float moveSpeed)
{
    material_layer_t*   layer;

    if(!mat)
        return -1;

    if(!(mat->numLayers < DDMAX_MATERIAL_LAYERS))
        return -1;

    layer = &mat->layers[mat->numLayers++];

    layer->flags = flags;
    layer->tex = tex;
    layer->texOrigin[0] = layer->texPosition[0] = xOrigin;
    layer->texOrigin[1] = layer->texPosition[0] = yOrigin;
    layer->moveAngle = moveAngle;
    layer->moveSpeed = moveSpeed;

    return mat->numLayers - 1;
}

byte Material_Prepare(material_snapshot_t* snapshot, material_t* mat,
                      byte flags, const material_prepare_params_t* params)
{
    byte                i, tmpResult;
    const gltexture_inst_t* texInsts[DDMAX_MATERIAL_LAYERS];
    const gltexture_inst_t* detailInst = NULL, *shinyInst = NULL,
                       *shinyMaskInst = NULL;

    if(!mat)
        return 0;

    if(flags & MPF_SMOOTH)
        mat = mat->current;

    assert(mat->numLayers > 0);

    tmpResult = prepareGLTextures(mat, texInsts, flags, params);

    if(tmpResult)
    {   // We need to update the assocated enhancements.
        // Decorations (lights, models, etc).
        mat->decoration = Def_GetDecoration(mat, tmpResult == 2);

        // Reflection (aka shiny surface).
        mat->reflection = Def_GetReflection(mat, tmpResult == 2);

        // Generator (particles).
        mat->ptcGen = Def_GetGenerator(mat, tmpResult == 2);

        // Detail texture.
        mat->detail = Def_GetDetailTex(mat, tmpResult == 2);
    }

    // Do we need to prepare any lightmaps?
    if(mat->decoration)
    {
        int                 i;

        /**
         * \todo No need to look up the lightmap texture records every time!
         */

        for(i = 0; i < DED_DECOR_NUM_LIGHTS; ++i)
        {
            const ded_decorlight_t* light = &mat->decoration->lights[i];
            lightmap_t*         lmap;

            if(!R_IsValidLightDecoration(light))
                break;

            if((lmap = R_GetLightMap(light->up.id)))
                GL_PrepareGLTexture(lmap->id, NULL, NULL);
            if((lmap = R_GetLightMap(light->down.id)))
                GL_PrepareGLTexture(lmap->id, NULL, NULL);
            if((lmap = R_GetLightMap(light->sides.id)))
                GL_PrepareGLTexture(lmap->id, NULL, NULL);
        }
    }

    // Do we need to prepare a detail texture?
    if(mat->detail)
    {
        detailtex_t*        dTex;
        lumpnum_t           lump =
            W_CheckNumForName(mat->detail->detailLump.path);
        const char*         external =
            (mat->detail->isExternal? mat->detail->detailLump.path : NULL);

        /**
         * \todo No need to look up the detail texture record every time!
         * This will change anyway once the gltexture for the detailtex is
         * linked to (and prepared) via the layers (above).
         */

        if((dTex = R_GetDetailTexture(lump, external)))
        {
            float               contrast =
                mat->detail->strength * detailFactor;

            // Pick an instance matching the specified context.
            detailInst = GL_PrepareGLTexture(dTex->id, &contrast, NULL);
        }
    }

    // Do we need to prepare a shiny texture (and possibly a mask)?
    if(mat->reflection)
    {
        shinytex_t*         sTex;
        masktex_t*          mTex;

        /**
         * \todo No need to look up the shiny texture record every time!
         * This will change anyway once the gltexture for the shinytex is
         * linked to (and prepared) via the layers (above).
         */

        if((sTex = R_GetShinyTexture(mat->reflection->shinyMap.path)))
        {
            // Pick an instance matching the specified context.
            shinyInst = GL_PrepareGLTexture(sTex->id, NULL, NULL);
        }

        if(shinyInst && // Don't bother searching unless the above succeeds.
           (mTex = R_GetMaskTexture(mat->reflection->maskMap.path)))
        {
            // Pick an instance matching the specified context.
            shinyMaskInst = GL_PrepareGLTexture(mTex->id, NULL, NULL);
        }
    }

    // If we arn't taking a snapshot, get out of here.
    if(!snapshot)
        return tmpResult;

    /**
     * Take a snapshot:
     */
    memset(snapshot, 0, sizeof(*snapshot));

    // Reset to the default state.
    for(i = 0; i < DDMAX_MATERIAL_LAYERS; ++i)
        setTexUnit(snapshot, i, BM_NORMAL, GL_LINEAR, NULL, 1, 1, 0, 0, 0);

    snapshot->width = mat->width;
    snapshot->height = mat->height;

    // Setup the primary texturing pass.
    if(mat->layers[0].tex)
    {
        const gltexture_t*  tex = GL_GetGLTexture(mat->layers[0].tex);
        int                 c;
        int                 magMode = glmode[texMagMode];

        if(tex->type == GLT_SPRITE)
            magMode = filterSprites? GL_LINEAR : GL_NEAREST;

        setTexUnit(snapshot, MTU_PRIMARY, BM_NORMAL, magMode, texInsts[0],
                   1.f / snapshot->width, 1.f / snapshot->height,
                   mat->layers[0].texPosition[0], mat->layers[0].texPosition[1],
                   1);

        snapshot->isOpaque = (texInsts[0]->flags & GLTF_MASKED) != 1;

        /// \fixme what about the other texture types?
        if(tex->type == GLT_DOOMTEXTURE || tex->type == GLT_FLAT)
        {
            for(c = 0; c < 3; ++c)
            {
                snapshot->color[c] = texInsts[0]->data.texture.color[c];
                snapshot->topColor[c] = texInsts[0]->data.texture.topColor[c];
            }
        }
        else
        {
            snapshot->color[CR] = snapshot->color[CG] =
                snapshot->color[CB] = 1;

            snapshot->topColor[CR] = snapshot->topColor[CG] =
                snapshot->topColor[CB] = 1;
        }
    }

    /**
     * If skymasked, we need only need to update the primary tex unit
     * (this is due to it being visible when skymask debug drawing is
     * enabled).
     */
    if(!(mat->flags & MATF_SKYMASK))
    {
        // Setup the detail texturing pass?
        if(detailInst && snapshot->isOpaque)
        {
            float               width, height, scale;

            width  = GLTexture_GetWidth(detailInst->tex);
            height = GLTexture_GetHeight(detailInst->tex);
            scale  = MAX_OF(1, mat->detail->scale);
            // Apply the global scaling factor.
            if(detailScale > .001f)
                scale *= detailScale;

            setTexUnit(snapshot, MTU_DETAIL, BM_NORMAL,
                       GL_LINEAR, detailInst, 1.f / width * scale,
                       1.f / height * scale, 0, 0, 1);
        }

        // Setup the reflection (aka shiny) texturing pass(es)?
        if(shinyInst)
        {
            ded_reflection_t* def = mat->reflection;

            snapshot->shiny.minColor[CR] = def->minColor[CR];
            snapshot->shiny.minColor[CG] = def->minColor[CG];
            snapshot->shiny.minColor[CB] = def->minColor[CB];

            setTexUnit(snapshot, MTU_REFLECTION, def->blendMode,
                       GL_LINEAR, shinyInst, 1, 1, 0, 0, def->shininess);

            if(shinyMaskInst)
                setTexUnit(snapshot, MTU_REFLECTION_MASK, BM_NORMAL,
                           snapshot->units[MTU_PRIMARY].magMode,
                           shinyMaskInst,
                           1.f / (snapshot->width * maskTextures[
                               shinyMaskInst->tex->ofTypeID]->width),
                           1.f / (snapshot->height * maskTextures[
                               shinyMaskInst->tex->ofTypeID]->height),
                           snapshot->units[MTU_PRIMARY].offset[0],
                           snapshot->units[MTU_PRIMARY].offset[1], 1);
        }
    }

    return tmpResult;
}

/**
 * Prepares all resources associated with the specified material including
 * all in the same animation group.
 *
 * \note Part of the Doomsday public API.
 *
 * \todo What about the load params? By limiting to the default params here, we
 * may be precaching unused texture instances.
 */
void Material_Precache(material_t* mat)
{
    if(!mat)
        return;

    if(mat->inAnimGroup)
    {   // The material belongs in one or more animgroups, precache the group.
        R_MaterialsPrecacheGroup(mat);
        return;
    }

    // Just this one material.
    Material_Prepare(NULL, mat, 0, NULL);
}

void Material_DeleteTextures(material_t* mat)
{
    if(mat)
    {
        byte                i;

        for(i = 0; i < mat->numLayers; ++i)
            GL_ReleaseGLTexture(mat->layers[i].tex);
    }
}

void Material_SetWidth(material_t* mat, float width)
{
    if(!mat)
        return;

    mat->width = width;
}

void Material_SetHeight(material_t* mat, float height)
{
    if(!mat)
        return;

    mat->height = height;
}

void Material_SetFlags(material_t* mat, short flags)
{
    if(!mat)
        return;

    mat->flags = flags;
}

void Material_SetTranslation(material_t* mat, material_t* current,
                              material_t* next, float inter)
{

    if(!mat || !current || !next)
    {
#if _DEBUG
Con_Error("Material_SetTranslation: Invalid paramaters.");
#endif
        return;
    }

    mat->current = current;
    mat->next = next;
    mat->inter = 0;
}

boolean Material_FromIWAD(const material_t* mat)
{
    return (mat->isAutoMaterial && mat->numLayers != 0 &&
            GLTexture_IsFromIWAD(GL_GetGLTexture(mat->layers[0].tex))) ?
            true : false;
}

/**
 * Retrieve the decoration definition associated with the material.
 *
 * @return              The associated decoration definition, else @c NULL.
 */
const ded_decor_t* Material_GetDecoration(material_t* mat)
{
    if(mat)
    {
        // Ensure we've already prepared this material.
        Material_Prepare(NULL, mat, 0, NULL);

        return mat->decoration;
    }

    return NULL;
}

/**
 * Retrieve the ptcgen definition associated with the material.
 *
 * @return              The associated ptcgen definition, else @c NULL.
 */
const ded_ptcgen_t* Material_GetPtcGen(material_t* mat)
{
    if(mat)
    {
        // Ensure we've already prepared this material.
        //Material_Prepare(NULL, mat, 0, NULL);

        return mat->ptcGen;
    }

    return NULL;
}

material_env_class_t Material_GetEnvClass(material_t* mat)
{
    if(mat)
    {
        if(mat->envClass == MEC_UNKNOWN)
        {
            mat->envClass =
                S_MaterialClassForName(P_GetMaterialName(mat), mat->mnamespace);
        }

        if(!(mat->flags & MATF_NO_DRAW))
        {
            return mat->envClass;
        }
    }

    return MEC_UNKNOWN;
}

void Material_SetLayerTexture(material_t* mat, byte layer, gltextureid_t tex)
{
    material_layer_t*   mlayer;

    if(!mat || !(layer < mat->numLayers))
        return;
    
    mlayer = &mat->layers[layer];
    mlayer->tex = tex;
}

byte Material_GetLayerFlags(const material_t* mat, byte layer)
{
    if(!mat || layer < 0 || !(layer < mat->numLayers))
        return 0;

    return mat->layers[layer].flags;
}

float Material_GetLayerTextureOriginX(const material_t* mat, byte layer)
{
    if(!mat || layer < 0 || !(layer < mat->numLayers))
        return 0;

    return mat->layers[layer].texOrigin[0];
}

float Material_GetLayerTextureOriginY(const material_t* mat, byte layer)
{
    if(!mat || layer < 0 || !(layer < mat->numLayers))
        return 0;

    return mat->layers[layer].texOrigin[1];
}

void Material_GetLayerTextureOriginXY(const material_t* mat, byte layer, float offset[2])
{
    const material_layer_t* mlayer;

    if(!mat || layer < 0 || !(layer < mat->numLayers))
        return;

    mlayer = &mat->layers[layer];
    offset[0] = mlayer->texOrigin[0];
    offset[1] = mlayer->texOrigin[1];
}

void Material_SetLayerTextureOriginX(material_t* mat, byte layer, float offset)
{
    material_layer_t*   mlayer;

    if(!mat || layer < 0 || !(layer < mat->numLayers))
        return;

    mlayer = &mat->layers[layer];

    mlayer->texOrigin[0] = mlayer->texPosition[0] = offset;
}

void Material_SetLayerTextureOriginY(material_t* mat, byte layer, float offset)
{
    material_layer_t*   mlayer;

    if(!mat || layer < 0 || !(layer < mat->numLayers))
        return;

    mlayer = &mat->layers[layer];

    mlayer->texOrigin[1] = mlayer->texPosition[1] = offset;
}

void Material_SetLayerTextureOriginXY(material_t* mat, byte layer, const float offset[2])
{
    material_layer_t*   mlayer;

    if(!mat || layer < 0 || !(layer < mat->numLayers))
        return;

    mlayer = &mat->layers[layer];

    mlayer->texOrigin[0] = mlayer->texPosition[0] = offset[0];
    mlayer->texOrigin[1] = mlayer->texPosition[1] = offset[1];
}

void Material_SetLayerFlags(material_t* mat, byte layer, byte flags)
{
    material_layer_t*   mlayer;

    if(!mat || layer < 0 || !(layer < mat->numLayers))
        return;
    mlayer = &mat->layers[layer];

    if((mlayer->flags & MATLF_MASKED) != (flags & MATLF_MASKED))
        Material_DeleteTextures(mat);

    mlayer->flags = flags;
}

void Material_SetLayerMoveAngle(material_t* mat, byte layer, float angle)
{
    material_layer_t*   mlayer;

    if(!mat || layer < 0 || !(layer < mat->numLayers))
        return;
    mlayer = &mat->layers[layer];

    mlayer->moveAngle = angle;
}

void Material_SetLayerMoveSpeed(material_t* mat, byte layer, float speed)
{
    material_layer_t*   mlayer;

    if(!mat || layer < 0 || !(layer < mat->numLayers))
        return;
    mlayer = &mat->layers[layer];

    mlayer->moveSpeed = speed;
}

/**
 * Update the material, property is selected by DMU_* name.
 */
boolean Material_SetProperty(material_t* mat, const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_WIDTH:
        DMU_SetValue(DMT_MATERIAL_WIDTH, &mat->width, args, 0);
        break;
    case DMU_HEIGHT:
        DMU_SetValue(DMT_MATERIAL_HEIGHT, &mat->height, args, 0);
        break;
    case DMU_LAYER1_FLAGS:
        {
        byte            flags;
        DMU_SetValue(DMT_MATERIAL_LAYER_FLAGS, &flags, args, 0);
        Material_SetLayerFlags(mat, 0, flags);
        break;
        }
    case DMU_LAYER2_FLAGS:
        {
        byte            flags;
        DMU_SetValue(DMT_MATERIAL_LAYER_FLAGS, &flags, args, 0);
        Material_SetLayerFlags(mat, 1, flags);
        break;
        }
    case DMU_LAYER1_OFFSET_X:
        {
        float           x;
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_X, &x, args, 0);
        Material_SetLayerTextureOriginX(mat, 0, x);
        break;
        }
    case DMU_LAYER1_OFFSET_Y:
        {
        float           y;
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &y, args, 0);
        Material_SetLayerTextureOriginX(mat, 0, y);
        break;
        }
    case DMU_LAYER1_OFFSET_XY:
        {
        float           offset[2];
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_X, &offset[0], args, 0);
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &offset[1], args, 1);
        Material_SetLayerTextureOriginXY(mat, 0, offset);
        break;
        }
    case DMU_LAYER2_OFFSET_X:
        {
        float           x;
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_X, &x, args, 0);
        Material_SetLayerTextureOriginX(mat, 1, x);
        break;
        }
    case DMU_LAYER2_OFFSET_Y:
        {
        float           y;
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &y, args, 0);
        Material_SetLayerTextureOriginX(mat, 1, y);
        break;
        }
    case DMU_LAYER2_OFFSET_XY:
        {
        float           offset[2];
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_X, &offset[0], args, 0);
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &offset[1], args, 1);
        Material_SetLayerTextureOriginXY(mat, 1, offset);
        break;
        }
    case DMU_LAYER1_ANGLE:
        DMU_SetValue(DMT_MATERIAL_LAYER_ANGLE, &mat->layers[0].moveAngle, args, 0);
        break;
    case DMU_LAYER2_ANGLE:
        DMU_SetValue(DMT_MATERIAL_LAYER_ANGLE, &mat->layers[1].moveAngle, args, 0);
        break;
    case DMU_LAYER1_SPEED:
        DMU_SetValue(DMT_MATERIAL_LAYER_SPEED, &mat->layers[0].moveSpeed, args, 0);
        break;
    case DMU_LAYER2_SPEED:
        DMU_SetValue(DMT_MATERIAL_LAYER_SPEED, &mat->layers[1].moveSpeed, args, 0);
        break;
    default:
        Con_Error("Material_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a material property, selected by DMU_* name.
 */
boolean Material_GetProperty(const material_t* mat, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_FLAGS:
        DMU_GetValue(DMT_MATERIAL_FLAGS, &mat->flags, args, 0);
        break;
    case DMU_WIDTH:
        DMU_GetValue(DMT_MATERIAL_WIDTH, &mat->width, args, 0);
        break;
    case DMU_HEIGHT:
        DMU_GetValue(DMT_MATERIAL_HEIGHT, &mat->height, args, 0);
        break;
    case DMU_NAME:
        {
        const char*         name = P_GetMaterialName(mat);
        DMU_GetValue(DDVT_PTR, &name, args, 0);
        break;
        }
    case DMU_NAMESPACE:
        DMU_GetValue(DMT_MATERIAL_MNAMESPACE, &mat->mnamespace, args, 0);
        break;
    case DMU_LAYER1_FLAGS:
        DMU_GetValue(DMT_MATERIAL_LAYER_FLAGS, &mat->layers[0].flags, args, 0);
        break;
    case DMU_LAYER2_FLAGS:
        DMU_GetValue(DMT_MATERIAL_LAYER_FLAGS, &mat->layers[1].flags, args, 0);
        break;
    case DMU_LAYER1_OFFSET_X:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_X, &mat->layers[0].texOrigin[0], args, 0);
        break;
    case DMU_LAYER1_OFFSET_Y:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &mat->layers[0].texOrigin[1], args, 0);
        break;
    case DMU_LAYER1_OFFSET_XY:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_X, &mat->layers[0].texOrigin[0], args, 0);
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &mat->layers[0].texOrigin[1], args, 1);
        break;
    case DMU_LAYER2_OFFSET_X:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_X, &mat->layers[1].texOrigin[0], args, 0);
        break;
    case DMU_LAYER2_OFFSET_Y:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &mat->layers[1].texOrigin[1], args, 0);
        break;
    case DMU_LAYER2_OFFSET_XY:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_X, &mat->layers[1].texOrigin[0], args, 0);
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &mat->layers[1].texOrigin[1], args, 1);
        break;
    case DMU_LAYER1_ANGLE:
        DMU_GetValue(DMT_MATERIAL_LAYER_ANGLE, &mat->layers[0].moveAngle, args, 0);
        break;
    case DMU_LAYER2_ANGLE:
        DMU_GetValue(DMT_MATERIAL_LAYER_ANGLE, &mat->layers[1].moveAngle, args, 0);
        break;
    case DMU_LAYER1_SPEED:
        DMU_GetValue(DMT_MATERIAL_LAYER_SPEED, &mat->layers[0].moveSpeed, args, 0);
        break;
    case DMU_LAYER2_SPEED:
        DMU_GetValue(DMT_MATERIAL_LAYER_SPEED, &mat->layers[1].moveSpeed, args, 0);
        break;
    default:
        Con_Error("Material_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
