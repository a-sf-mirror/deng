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

#include "de/Material"

#include <sstream>

using namespace de;

#if 0
namespace {
void setTexUnit(material_snapshot_t* ss, dbyte unit, blendmode_t blendMode,
    dint magMode, const gltexture_inst_t* texInst, dfloat sScale, dfloat tScale,
    dfloat sOffset, dfloat tOffset, dfloat alpha)
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

dbyte prepareGLTextures(Material* mat, gltexture_inst_t const** texInsts,
    dbyte flags, const material_prepare_params_t* params)
{
    dbyte tmpResult = 0;

    // Ensure all resources needed to visualize this material are loaded.
    for(dbyte i = 0; i < mat->numLayers; ++i)
    {
        MaterialLayer* ml = &mat->layers[i];
        texture_load_params_t tParams;
        dbyte result;

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
}
#endif

void Material::think(const Time::Delta& elapsed)
{
    // Update layers
    FOR_EACH(i, layers, Layers::iterator)
    {
        Layer& layer = (*i);

        if(!de::fequal(layer.scrollSpeed(), 0))
        {
            ddouble radians = PI * layer.scrollAngle() / 180;
            layer.texPosition = layer.textureOrigin() +
                Vector2f(dfloat(cos(radians) * elapsed * layer.scrollSpeed()),
                         dfloat(sin(radians) * elapsed * layer.scrollSpeed()));
        }

        /*if(layer.tics-- <= 0)
        {
            // Advance to next stage.
            if(++layer.stage == def->layers[i].stageCount.num)
            {
                // Loop back to the beginning.
                layer.stage = 0;
                continue;
            }

            layer.tics = def->layers[i].stages[layer.stage].tics *
                (1 - def->layers[i].stages[layer.stage].variance *
                    RNG_RandFloat()) * TICSPERSEC;
        }*/
    }
}

Material::Layers::size_type Material::addLayer(dbyte flags, TextureId tex,
    dfloat originX, dfloat originY, dfloat scrollAngle, dfloat scrollSpeed)
{
    if(layers.size() == MAX_LAYERS)
    {
        std::ostringstream os;
        os << "Out of layers (max " << MAX_LAYERS << ").";
        /// @throw OutOfLayersError Attempt to create a new layer when the maximum number is already present.
        throw OutOfLayersError("Material::addLayer", os.str());
    }

    layers.push_back(Layer(flags, tex, Vector2f(originX, originY), scrollAngle, scrollSpeed));
    return layers.size() - 1;
}

#if 0
dbyte Material::prepare(material_snapshot_t* snapshot, dbyte prepareFlags,
    const material_prepare_params_t* params)
{
    if(prepareFlags & MPF_SMOOTH)
    {
        if(current != this)
            return current->prepare(snapshot, prepareflags, params);
    }

    assert(numLayers > 0);

    dbyte tmpResult;
    const gltexture_inst_t* texInsts[DDMAX_MATERIAL_LAYERS];
    const gltexture_inst_t* detailInst = NULL, *shinyInst = NULL,
                       *shinyMaskInst = NULL;

    tmpResult = prepareGLTextures(mat, texInsts, prepareflags, params);

    if(tmpResult)
    {   // We need to update the assocated enhancements.
        // Decorations (lights, models, etc).
        decoration = Def_GetDecoration(this, tmpResult == 2);

        // Reflection (aka shiny surface).
        reflection = Def_GetReflection(this, tmpResult == 2);

        // Generator (particles).
        generator = Def_GetGenerator(this, tmpResult == 2);

        // Detail texture.
        detail = Def_GetDetailTex(this, tmpResult == 2);
    }

    // Do we need to prepare a detail texture?
    if(detail)
    {
        detailtex_t* dTex;
        lumpnum_t lump = W_CheckNumForName(detail->detailLump.path);
        const char* external = (detail->isExternal? detail->detailLump.path : NULL);

        /**
         * @todo No need to look up the detail texture record every time!
         * This will change anyway once the gltexture for the detailtex is
         * linked to (and prepared) via the layers (above).
         */
        if((dTex = R_GetDetailTexture(lump, external)))
        {
            dfloat contrast = detail->strength * detailFactor;
            // Pick an instance matching the specified context.
            detailInst = GL_PrepareGLTexture(dTex->id, &contrast, NULL);
        }
    }

    // Do we need to prepare a shiny texture (and possibly a mask)?
    if(reflection)
    {
        shinytex_t* sTex;
        masktex_t* mTex;

        /**
         * @todo No need to look up the shiny texture record every time!
         * This will change anyway once the gltexture for the shinytex is
         * linked to (and prepared) via the layers (above).
         */

        if((sTex = R_GetShinyTexture(reflection->shinyMap.path)))
        {
            // Pick an instance matching the specified context.
            shinyInst = GL_PrepareGLTexture(sTex->id, NULL, NULL);
        }

        if(shinyInst && // Don't bother searching unless the above succeeds.
           (mTex = R_GetMaskTexture(reflection->maskMap.path)))
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
    for(dbyte i = 0; i < DDMAX_MATERIAL_LAYERS; ++i)
        setTexUnit(snapshot, i, BM_NORMAL, GL_LINEAR, NULL, 1, 1, 0, 0, 0);

    snapshot->width = width;
    snapshot->height = height;

    // Setup the primary texturing pass.
    if(layers[0].tex)
    {
        const gltexture_t* tex = GL_GetGLTexture(layers[0].tex);

        dint magMode = glmode[texMagMode];
        if(tex->type == GLT_SPRITE)
            magMode = filterSprites? GL_LINEAR : GL_NEAREST;

        setTexUnit(snapshot, MTU_PRIMARY, BM_NORMAL, magMode, texInsts[0],
                   1.f / snapshot->width, 1.f / snapshot->height,
                   layers[0].texPosition[0], layers[0].texPosition[1], 1);

        snapshot->isOpaque = (texInsts[0]->flags & GLTF_MASKED) != 1;

        /// @fixme what about the other texture types?
        if(tex->type == GLT_DOOMTEXTURE || tex->type == GLT_FLAT)
        {
            for(dint c = 0; c < 3; ++c)
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
    if(!(flags & MATF_SKYMASK))
    {
        // Setup the detail texturing pass?
        if(detailInst && snapshot->isOpaque)
        {
            dfloat width, height, scale;

            width  = GLTexture_GetWidth(detailInst->tex);
            height = GLTexture_GetHeight(detailInst->tex);
            scale  = de:max(1, detail->scale);
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
            ded_reflection_t* def = reflection;

            snapshot->shinyMinColor[CR] = def->minColor[CR];
            snapshot->shinyMinColor[CG] = def->minColor[CG];
            snapshot->shinyMinColor[CB] = def->minColor[CB];

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

void Material::precache()
{
    if(inAnimGroup)
    {   // The material belongs in one or more animgroups, precache the group.
        Materials_CacheMaterial(mat);
        return;
    }

    // Just this one material.
    Materials_Prepare(mat, 0, NULL, NULL);
}

void Material::deleteTextures()
{
    for(dint i = 0; i < mat->numLayers; ++i)
        GL_ReleaseGLTexture(layers[i].tex);
}
#endif

void Material::setWidth(dfloat newWidth)
{
    width = newWidth;
}

void Material::setHeight(dfloat newHeight)
{
    height = newHeight;
}

void Material::setFlags(dshort newFlags)
{
    flags = newFlags;
}

void Material::setTranslation(Material& newCurrent, Material& newNext, dfloat inter)
{
    current = &newCurrent;
    next = &newNext;
    inter = 0;
}

#if 0
bool Material::isFromIWAD() const
{
    return isAutoMaterial && numLayers != 0 &&
           GLTexture_IsFromIWAD(GL_GetGLTexture(layers[0].tex));
}
#endif

Material::Layer::Layer(dbyte flags, TextureId tex, const Vector2f& origin,
    dfloat scrollSpeed, dfloat scrollAngle)
  : _flags(flags), _tex(tex), _texOrigin(origin), texPosition(origin),
    _scrollSpeed(scrollSpeed), _scrollAngle(scrollAngle)
{}

void Material::Layer::setTexture(TextureId tex)
{
    _tex = tex;
}

void Material::Layer::setTextureOriginX(dfloat originX)
{
    _texOrigin.x = texPosition.x = originX;
}

void Material::Layer::setTextureOriginY(dfloat originY)
{
    _texOrigin.y = texPosition.y = originY;
}

void Material::Layer::setTextureOrigin(const Vector2f& origin)
{
    _texOrigin = texPosition = origin;
}

void Material::Layer::setFlags(dbyte newFlags)
{
    if((_flags & MASKED) != (newFlags & MASKED))
        deleteTexture();
    _flags = newFlags;
}

void Material::Layer::setScrollAngle(dfloat angle)
{
    _scrollAngle = angle;
}

void Material::Layer::setScrollSpeed(dfloat speed)
{
    _scrollSpeed = speed;
}

#if 0
boole Material::setProperty(const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_WIDTH:
        DMU_SetValue(DMT_MATERIAL_WIDTH, &width, args, 0);
        break;
    case DMU_HEIGHT:
        DMU_SetValue(DMT_MATERIAL_HEIGHT, &height, args, 0);
        break;
    case DMU_LAYER1_FLAGS:
        {
        dbyte localFlags;
        DMU_SetValue(DMT_MATERIAL_LAYER_FLAGS, &localFlags, args, 0);
        setLayerFlags(0, localFlags);
        break;
        }
    case DMU_LAYER2_FLAGS:
        {
        dbyte localFlags;
        DMU_SetValue(DMT_MATERIAL_LAYER_FLAGS, &localFlags, args, 0);
        setLayerFlags(1, localFlags);
        break;
        }
    case DMU_LAYER1_OFFSET_X:
        {
        dfloat localX;
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_X, &localX, args, 0);
        setLayerTextureOriginX(0, localX);
        break;
        }
    case DMU_LAYER1_OFFSET_Y:
        {
        dfloat localY;
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &localY, args, 0);
        setLayerTextureOriginX(0, localY);
        break;
        }
    case DMU_LAYER1_OFFSET_XY:
        {
        dfloat localOffset[2];
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_X, &localOffset[0], args, 0);
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &localOffset[1], args, 1);
        setLayerTextureOriginXY(0, localOffset);
        break;
        }
    case DMU_LAYER2_OFFSET_X:
        {
        dfloat localX;
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_X, &localX, args, 0);
        setLayerTextureOriginX(1, localX);
        break;
        }
    case DMU_LAYER2_OFFSET_Y:
        {
        dfloat localY;
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &localY, args, 0);
        setLayerTextureOriginX(1, localY);
        break;
        }
    case DMU_LAYER2_OFFSET_XY:
        {
        dfloat localOffset[2];
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_X, &localOffset[0], args, 0);
        DMU_SetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &localOffset[1], args, 1);
        Material_SetLayerTextureOriginXY(1, localOffset);
        break;
        }
    case DMU_LAYER1_ANGLE:
        DMU_SetValue(DMT_MATERIAL_LAYER_ANGLE, &layers[0].scrollAngle, args, 0);
        break;
    case DMU_LAYER2_ANGLE:
        DMU_SetValue(DMT_MATERIAL_LAYER_ANGLE, &layers[1].scrollAngle, args, 0);
        break;
    case DMU_LAYER1_SPEED:
        DMU_SetValue(DMT_MATERIAL_LAYER_SPEED, &layers[0].scrollSpeed, args, 0);
        break;
    case DMU_LAYER2_SPEED:
        DMU_SetValue(DMT_MATERIAL_LAYER_SPEED, &layers[1].scrollSpeed, args, 0);
        break;
    default:
        Con_Error("Material::setProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

bool Material::getProperty(setargs_t* args) const
{
    switch(args->prop)
    {
    case DMU_FLAGS:
        DMU_GetValue(DMT_MATERIAL_FLAGS, &flags, args, 0);
        break;
    case DMU_WIDTH:
        DMU_GetValue(DMT_MATERIAL_WIDTH, &width, args, 0);
        break;
    case DMU_HEIGHT:
        DMU_GetValue(DMT_MATERIAL_HEIGHT, &height, args, 0);
        break;
    case DMU_NAME:
        {
        const char* name = Materials_NameOf(this);
        DMU_GetValue(DDVT_PTR, &name, args, 0);
        break;
        }
    case DMU_NAMESPACE:
        DMU_GetValue(DMT_MATERIAL_MNAMESPACE, &mnamespace, args, 0);
        break;
    case DMU_LAYER1_FLAGS:
        DMU_GetValue(DMT_MATERIAL_LAYER_FLAGS, &layers[0].flags, args, 0);
        break;
    case DMU_LAYER2_FLAGS:
        DMU_GetValue(DMT_MATERIAL_LAYER_FLAGS, &layers[1].flags, args, 0);
        break;
    case DMU_LAYER1_OFFSET_X:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_X, &layers[0].texOrigin[0], args, 0);
        break;
    case DMU_LAYER1_OFFSET_Y:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &layers[0].texOrigin[1], args, 0);
        break;
    case DMU_LAYER1_OFFSET_XY:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_X, &layers[0].texOrigin[0], args, 0);
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &layers[0].texOrigin[1], args, 1);
        break;
    case DMU_LAYER2_OFFSET_X:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_X, &layers[1].texOrigin[0], args, 0);
        break;
    case DMU_LAYER2_OFFSET_Y:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &layers[1].texOrigin[1], args, 0);
        break;
    case DMU_LAYER2_OFFSET_XY:
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_X, &layers[1].texOrigin[0], args, 0);
        DMU_GetValue(DMT_MATERIAL_LAYER_OFFSET_Y, &layers[1].texOrigin[1], args, 1);
        break;
    case DMU_LAYER1_ANGLE:
        DMU_GetValue(DMT_MATERIAL_LAYER_ANGLE, &layers[0].scrollAngle, args, 0);
        break;
    case DMU_LAYER2_ANGLE:
        DMU_GetValue(DMT_MATERIAL_LAYER_ANGLE, &layers[1].scrollAngle, args, 0);
        break;
    case DMU_LAYER1_SPEED:
        DMU_GetValue(DMT_MATERIAL_LAYER_SPEED, &layers[0].scrollSpeed, args, 0);
        break;
    case DMU_LAYER2_SPEED:
        DMU_GetValue(DMT_MATERIAL_LAYER_SPEED, &layers[1].scrollSpeed, args, 0);
        break;
    default:
        Con_Error("Material::getProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
#endif
