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
 * r_sky.c: Sky Management
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_console.h"
#include "de_play.h"
#include "de_render.h"

#include "cl_def.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int skyHemispheres;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static sky_t sky;
sky_t* theSky = &sky;

// CODE --------------------------------------------------------------------

/**
 * Precache all resources needed for visualizing skies.
 */
void R_SkyPrecache(void)
{
    Sky_Precache(theSky);
}

/**
 * Animate sky models.
 */
void R_SkyTicker(void)
{
    if(clientPaused)
        return;

    Sky_Ticker(theSky);
}

void R_SetupSky(sky_t* sky, const ded_sky_t* skyDef)
{
    static boolean      inited = false;

    if(skyDef)
    {   
        Sky_InitFromDefinition(theSky, skyDef);
    }
    else
    {
        // Go with the defaults.
        Sky_InitDefault(theSky);
    }

    if(!inited)
    {
        DMU_AddObjRecord(DMU_SKY, theSky);
        inited = true;
    }
}

static void updateLayerStats(sky_t* sky)
{
    int                 i;

    sky->firstLayer = -1;
    sky->activeLayers = 0;

    for(i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        skylayer_t*         slayer = &sky->layers[i];

        if(slayer->enabled)
        {
            sky->activeLayers++;
            if(sky->firstLayer == -1)
                sky->firstLayer = i;
        }
    }
}

static void initSkyModels(sky_t* sky, const ded_sky_t* def)
{
    int                 i;

    // Clear the whole sky models data.
    memset(sky->models, 0, sizeof(sky->models));

    sky->modelsInited = false;

    for(i = 0; i < NUM_SKY_MODELS; ++i)
    {
        skymodel_t*         sm = &sky->models[i];
        const ded_skymodel_t* modef = &def->models[i];

        // Is the model ID set?
        if((sm->model = R_CheckIDModelFor(modef->id)) == NULL)
            continue;

        // There is a model here.
        sky->modelsInited = true;

        sm->def = modef;
        sm->maxTimer = (int) (TICSPERSEC * modef->frameInterval);
        sm->yaw = modef->yaw;
        sm->frame = sm->model->sub[0].frame;
    }
}

static void init(sky_t* sky)
{
    int                 i;
    float               offset[2] = { 0, 0 };
    material_t*         mat;

    sky->def = NULL;
    sky->firstLayer = sky->activeLayers = 0;
    sky->modelsInited = false;

    // Configure the material.
    mat = P_ToMaterial(P_MaterialNumForName("SKY1", MN_TEXTURES));
    Material_SetLayerFlags(mat, 0, 0);
    Material_SetLayerTextureOriginXY(mat, 0, offset);

    sky->material = mat;

    // Initialize the layers.
    for(i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        skylayer_t*         slayer = &sky->layers[i];

        slayer->fadeoutColorLimit = .3f;
    }
    
    Sky_ActivateLayer(sky, 0, true);
}

/**
 * Configure sky using default values.
 */
void Sky_InitDefault(sky_t* sky)
{
    if(!sky)
        return;

    init(sky);
}

/**
 * Configure sky according to the specified definition.
 */
void Sky_InitFromDefinition(sky_t* sky, const ded_sky_t* def)
{
    int                 i;
    material_t*         mat;

    if(!sky)
        return;

    init(sky);

    sky->def = def;

    {
    materialnum_t       matNum;
    if(!(matNum = P_MaterialNumForName(def->layers[0].material.name,
                                       def->layers[0].material.mnamespace)))
    {
        Con_Message("Sky_InitFromDefinition: Warning, failed to locate "
                    "material '%s' in namespace %i. Using default.\n",
                    def->layers[0].material.name,
                    def->layers[0].material.mnamespace);

        matNum = P_MaterialNumForName("SKY1", MN_TEXTURES);
    }
    mat = P_ToMaterial(matNum);
    }

     // Configure the material
    for(i = 0; i < 2; ++i)
    {
        const ded_skylayer_t* layer = &def->layers[i];
        byte                flags = 0;
        gltextureid_t       glTexID = 0;

        // @todo Refactor away this ugliness.
        if(layer->material.name[0])
        {
            gltexture_type_t    glTexType = GLT_ANY; // Means NULL in this context.

            switch(layer->material.mnamespace)
            {
            case MN_SYSTEM:     glTexType = GLT_SYSTEM;         break;
            case MN_TEXTURES:   glTexType = GLT_DOOMTEXTURE;    break;
            case MN_FLATS:      glTexType = GLT_FLAT;           break;
            case MN_SPRITES:    glTexType = GLT_SPRITE;         break;
                break;

            default:
                break;
            }

            if(glTexType != GLT_ANY)
            {
                const gltexture_t* glTex =
                    GL_GetGLTextureByName(layer->material.name, glTexType);

                if(glTex)
                    glTexID = glTex->id;
            }
        }

        Material_SetLayerTexture(mat, i, glTexID);
        Material_SetLayerTextureOriginXY(mat, i, layer->offset);

        if(layer->flags & SLF_MASKED)
            flags |= MATLF_MASKED;
        Material_SetLayerFlags(mat, i, flags);
    }

    Sky_SetSphereMaterial(sky, mat);

    for(i = 0; i < 2; ++i)
    {
        const ded_skylayer_t* layer = &def->layers[i];

        if(!(layer->flags & SLF_ENABLED))
        {
            Sky_ActivateLayer(sky, i, false);
            continue;
        }

        sky->layers[i].fadeoutColorLimit = layer->colorLimit;

        Sky_ActivateLayer(sky, i, true);
    }

    // Any sky models to setup? Models will override the normal sphere unless
    // always visible.
    initSkyModels(sky, def);
}

static void modelTicker(skymodel_t* model)
{
    // Turn the model.
    model->yaw += model->def->yawSpeed / TICSPERSEC;

    // Is it time to advance to the next frame?
    if(model->maxTimer > 0 && ++model->timer >= model->maxTimer)
    {
        model->timer = 0;
        model->frame++;

        // Execute a console command?
        if(model->def->execute)
            Con_Execute(CMDS_DED, model->def->execute, true, false);
    }
}

void Sky_Ticker(sky_t* sky)
{
    int                 i;

    if(!sky)
        return;

    if(!sky->modelsInited)
        return;

    for(i = 0; i < NUM_SKY_MODELS; ++i)
    {
        skymodel_t*         skyModel = &sky->models[i];

        if(!skyModel->def)
            continue;

        modelTicker(skyModel);
    }
}

void Sky_Precache(sky_t* sky)
{
    int                 i;

    if(!sky)
        return;

    if(!sky->modelsInited)
        return;

    for(i = 0; i < NUM_SKY_MODELS; ++i)
    {
        skymodel_t*         skyModel = &sky->models[i];

        if(!skyModel->def)
            continue;

        R_PrecacheModelSkins(skyModel->model);
    }
}

int Sky_GetFirstLayer(sky_t* sky)
{
    if(!sky)
        return 0;

    return sky->firstLayer;
}

const float* Sky_GetLightColor(sky_t* sky)
{
    if(!sky)
        return NULL;

    if(sky->def && (sky->def->color[CR] > 0 || sky->def->color[CG] > 0 ||
                    sky->def->color[CB] > 0))
        return sky->def->color;

    return NULL;
}

void Sky_ActivateLayer(sky_t* sky, int layer, boolean active)
{
    skylayer_t*         slayer;

    if(!sky || layer < 0 || !(layer < MAX_SKY_LAYERS))
        return;

    slayer = &sky->layers[layer];

    slayer->enabled = active;

    updateLayerStats(sky);
}

boolean Sky_IsLayerActive(sky_t* sky, int layer)
{
    skylayer_t*         slayer;

    if(!sky || layer < 0 || !(layer < MAX_SKY_LAYERS))
        return false;

    slayer = &sky->layers[layer];

    return (slayer->enabled ? true : false);
}

material_t* Sky_GetSphereMaterial(sky_t* sky)
{
    if(!sky)
        return NULL;

    return sky->material;
}

void Sky_SetSphereMaterial(sky_t* sky, material_t* material)
{
    if(!sky)
        return;

    sky->material = material;
}

/**
 * Update the sky, property is selected by DMU_* name.
 */
boolean Sky_SetProperty(sky_t* sky, const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_MATERIAL:
        {
        void*           p;
        DMU_SetValue(DDVT_PTR, &p, args, 0);
        Sky_SetSphereMaterial(sky, ((dmuobjrecord_t*) p)->obj);
        }
        break;
    case DMU_LAYER1_ACTIVE:
        {
        boolean         active;
        DMU_SetValue(DMT_SKY_LAYER_ACTIVE, &active, args, 0);
        Sky_ActivateLayer(sky, 0, active);
        break;
        }
    case DMU_LAYER2_ACTIVE:
        {
        boolean         active;
        DMU_SetValue(DMT_SKY_LAYER_ACTIVE, &active, args, 0);
        Sky_ActivateLayer(sky, 1, active);
        break;
        }
    default:
        Con_Error("Sky_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a sky property, selected by DMU_* name.
 */
boolean Sky_GetProperty(sky_t* sky, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_MATERIAL:
        {
        material_t*     mat = Sky_GetSphereMaterial(sky);
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_MATERIAL, mat);
        DMU_GetValue(DMT_SKY_MATERIAL, &r, args, 0);
        break;
        }
    case DMU_LAYER1_ACTIVE:
        DMU_GetValue(DMT_SKY_LAYER_ACTIVE, &sky->layers[0].enabled, args, 0);
        break;

    case DMU_LAYER2_ACTIVE:
        DMU_GetValue(DMT_SKY_LAYER_ACTIVE, &sky->layers[1].enabled, args, 0);
        break;
    default:
        Con_Error("Sky_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
