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

static sky_t sky = { NULL, (float) PI / 3 };
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
    int                 i = 0;

    sky->firstLayer = -1;
    sky->activeLayers = 0;
    for(i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        skylayer_t*         slayer = &sky->layers[i];

        if(slayer->flags & SLF_ENABLED)
        {
            sky->activeLayers++;
            if(sky->firstLayer == -1)
                sky->firstLayer = i;
        }
    }
}

static void checkFadeoutColorLimit(skylayer_t* slayer)
{
    if(slayer->mat)
    {
        int                 i;

        slayer->fadeout.use = false;
        for(i = 0; i < 3; ++i)
            if(slayer->fadeout.rgb[i] > slayer->fadeout.limit)
            {
                // Colored fadeout is needed.
                slayer->fadeout.use = true;
                break;
            }
    }

    slayer->fadeout.use = true;
}

static void setLayerFadeoutColorLimit(skylayer_t* slayer, float limit)
{
    slayer->fadeout.limit = limit;
    checkFadeoutColorLimit(slayer);
}

static void setupFadeout(skylayer_t* slayer)
{
    if(slayer->mat)
    {
        material_load_params_t params;
        material_snapshot_t ms;

        // Ensure we have up to date info on the material.
        memset(&params, 0, sizeof(params));
        params.flags = MLF_LOAD_AS_SKY | MLF_TEX_NO_COMPRESSION;
        if(slayer->flags & SLF_MASKED)
            params.flags |= MLF_ZEROMASK;

        Material_Prepare(&ms, slayer->mat, true, &params);
        slayer->fadeout.rgb[CR] = ms.topColor[CR];
        slayer->fadeout.rgb[CG] = ms.topColor[CG];
        slayer->fadeout.rgb[CB] = ms.topColor[CB];
    }
    else
    {
        // An invalid texture, default to black.
        slayer->fadeout.rgb[CR] = slayer->fadeout.rgb[CG] =
            slayer->fadeout.rgb[CB] = 0;
    }

    checkFadeoutColorLimit(slayer);
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

    sky->firstLayer = 0;
    sky->def = NULL;

    // Initialize the layers.
    for(i = 0; i < MAX_SKY_LAYERS; ++i)
    {
        skylayer_t*         slayer = &sky->layers[i];

        slayer->mat = NULL; // No material.
        slayer->fadeout.limit = .3f;
    }
}

/**
 * Configure sky using default values.
 */
void Sky_InitDefault(sky_t* sky)
{
    if(!sky)
        return;

    init(sky);

    Sky_ActivateLayer(sky, 0, true);
    Sky_SetLayerMaterial(sky, 0, P_ToMaterial(P_MaterialNumForName("SKY1", MN_TEXTURES)));
    Sky_SetLayerMask(sky, 0, false);
    Sky_SetLayerMaterialOffsetX(sky, 0, 0);

    Sky_ActivateLayer(sky, 1, true);
}

/**
 * Configure sky according to the specified definition.
 */
void Sky_InitFromDefinition(sky_t* sky, const ded_sky_t* def)
{
    int                 i;

    if(!sky)
        return;

    init(sky);

    sky->def = def;

    for(i = 0; i < 2; ++i)
    {
        const ded_skylayer_t* layer = &def->layers[i];

        if(layer->flags & SLF_ENABLED)
        {
            materialnum_t       matNum =
                P_MaterialNumForName(layer->material.name,
                                     layer->material.mnamespace);
            if(!matNum)
            {
                Con_Message("Sky_InitFromDefinition: Invalid/missing material "
                            "\"%s\"\n", layer->material.name);

                matNum = P_MaterialNumForName("SKY1", MN_TEXTURES);
            }

            Sky_ActivateLayer(sky, i, true);
            Sky_SetLayerMaterial(sky, i, P_ToMaterial(matNum));

            Sky_SetLayerMask(sky, i, (layer->flags & SLF_MASKED)? true : false);
            Sky_SetLayerMaterialOffsetX(sky, i, layer->offset);

            setLayerFadeoutColorLimit(&sky->layers[i], layer->colorLimit);
        }
        else
        {
            Sky_ActivateLayer(sky, i, false);
        }
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

int Sky_GetFirstLayer(const sky_t* sky)
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

const fadeout_t* Sky_GetFadeout(const sky_t* sky)
{
    const skylayer_t*   slayer;

    if(!sky)
        return NULL;

    // The fadeout is that of the first layer.
    slayer = &sky->layers[sky->firstLayer];

    return &slayer->fadeout;
}

boolean Sky_IsLayerActive(const sky_t* sky, int layer)
{
    const skylayer_t*   slayer;

    if(!sky || layer < 0 || !(layer < MAX_SKY_LAYERS))
        return false;

    slayer = &sky->layers[layer];

    return (slayer->flags & SLF_ENABLED) ? true : false;
}

boolean Sky_GetLayerMask(const sky_t* sky, int layer)
{
    const skylayer_t*   slayer;

    if(!sky || layer < 0 || !(layer < MAX_SKY_LAYERS))
        return false;

    slayer = &sky->layers[layer];

    return (slayer->flags & SLF_MASKED) ? true : false;
}

material_t* Sky_GetLayerMaterial(const sky_t* sky, int layer)
{
    const skylayer_t*   slayer;

    if(!sky || layer < 0 || !(layer < MAX_SKY_LAYERS))
        return NULL;

    slayer = &sky->layers[layer];

    return slayer->mat;
}

float Sky_GetLayerMaterialOffsetX(const sky_t* sky, int layer)
{
    const skylayer_t*   slayer;

    if(!sky || layer < 0 || !(layer < MAX_SKY_LAYERS))
        return 0;

    slayer = &sky->layers[layer];

    return slayer->offset;
}

void Sky_ActivateLayer(sky_t* sky, int layer, boolean active)
{
    skylayer_t*         slayer;

    if(!sky || layer < 0 || !(layer < MAX_SKY_LAYERS))
        return;

    slayer = &sky->layers[layer];

    if(active)
        slayer->flags |= SLF_ENABLED;
    else
        slayer->flags &= ~SLF_ENABLED;

    updateLayerStats(sky);
}

void Sky_SetLayerMask(sky_t* sky, int layer, boolean enable)
{
    boolean             deleteTextures = false;
    skylayer_t*         slayer;

    if(!sky || layer < 0 || !(layer < MAX_SKY_LAYERS))
        return;

    slayer = &sky->layers[layer];

    if(enable)
    {
        // Invalidate the loaded texture, if necessary.
        if(slayer->mat && !(slayer->flags & SLF_MASKED))
            deleteTextures = true;
        slayer->flags |= SLF_MASKED;
    }
    else
    {
        // Invalidate the loaded texture, if necessary.
        if(slayer->mat && (slayer->flags & SLF_MASKED))
            deleteTextures = true;
        slayer->flags &= ~SLF_MASKED;
    }

    if(deleteTextures)
        Material_DeleteTextures(slayer->mat);
}

void Sky_SetLayerMaterial(sky_t* sky, int layer, material_t* material)
{
    skylayer_t*         slayer;

    if(!sky || layer < 0 || !(layer < MAX_SKY_LAYERS))
        return;

    slayer = &sky->layers[layer];

    slayer->mat = material;

    if(material)
    {
        material_load_params_t params;
    
        memset(&params, 0, sizeof(params));

        params.flags = MLF_LOAD_AS_SKY | MLF_TEX_NO_COMPRESSION;
        if(slayer->flags & SLF_MASKED)
            params.flags |= MLF_ZEROMASK;

        Material_Prepare(NULL, slayer->mat, true, &params);
    }

    setupFadeout(slayer);
}

void Sky_SetLayerMaterialOffsetX(sky_t* sky, int layer, float offset)
{
    skylayer_t*         slayer;

    if(!sky || layer < 0 || !(layer < MAX_SKY_LAYERS))
        return;

    slayer = &sky->layers[layer];

    slayer->offset = offset;
}

/**
 * Update the sky, property is selected by DMU_* name.
 */
boolean Sky_SetProperty(sky_t* sky, const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_LAYER1_MATERIAL:
        {
        void*           p;
        DMU_SetValue(DDVT_PTR, &p, args, 0);
        Sky_SetLayerMaterial(sky, 0, ((dmuobjrecord_t*) p)->obj);
        }
        break;
    case DMU_LAYER2_MATERIAL:
        {
        void*           p;
        DMU_SetValue(DDVT_PTR, &p, args, 0);
        Sky_SetLayerMaterial(sky, 1, ((dmuobjrecord_t*) p)->obj);
        }
        break;
    case DMU_LAYER1_OFFSET_X:
        {
        float           p;
        DMU_SetValue(DMT_SKY_OFFSET_X, &p, args, 0);
        Sky_SetLayerMaterialOffsetX(sky, 0, p);
        break;
        }
    case DMU_LAYER2_OFFSET_X:
        {
        float           p;
        DMU_SetValue(DMT_SKY_OFFSET_X, &p, args, 0);
        Sky_SetLayerMaterialOffsetX(sky, 1, p);
        break;
        }
    case DMU_LAYER1_ACTIVE:
        {
        boolean         vis;
        DMU_SetValue(DMT_SKY_ACTIVE, &vis, args, 0);
        Sky_ActivateLayer(sky, 0, vis);
        break;
        }
    case DMU_LAYER2_ACTIVE:
        {
        boolean         vis;
        DMU_SetValue(DMT_SKY_ACTIVE, &vis, args, 0);
        Sky_ActivateLayer(sky, 1, vis);
        break;
        }
    case DMU_LAYER1_MASK:
        {
        boolean         mask;
        DMU_SetValue(DMT_SKY_MASK, &mask, args, 0);
        Sky_SetLayerMask(sky, 0, mask);
        break;
        }
    case DMU_LAYER2_MASK:
        {
        boolean         mask;
        DMU_SetValue(DMT_SKY_MASK, &mask, args, 0);
        Sky_SetLayerMask(sky, 1, mask);
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
boolean Sky_GetProperty(const sky_t* sky, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_LAYER1_MATERIAL:
        {
        material_t*     mat = sky->layers[0].mat;
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_MATERIAL, mat);
        DMU_GetValue(DMT_SKY_MATERIAL, &r, args, 0);
        break;
        }
    case DMU_LAYER2_MATERIAL:
        {
        material_t*     mat = sky->layers[1].mat;
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_MATERIAL, mat);
        DMU_GetValue(DMT_SKY_MATERIAL, &r, args, 0);
        break;
        }
    case DMU_LAYER1_OFFSET_X:
        DMU_GetValue(DMT_SKY_OFFSET_X, &sky->layers[0].offset, args, 0);
        break;
    case DMU_LAYER2_OFFSET_X:
        DMU_GetValue(DMT_SKY_OFFSET_X, &sky->layers[1].offset, args, 0);
        break;
    case DMU_LAYER1_ACTIVE:
        {
        boolean         vis = (sky->layers[0].flags & SLF_ENABLED)? true : false;
        DMU_GetValue(DMT_SKY_ACTIVE, &vis, args, 0);
        break;
        }
    case DMU_LAYER2_ACTIVE:
        {
        boolean         vis = (sky->layers[1].flags & SLF_ENABLED)? true : false;
        DMU_GetValue(DMT_SKY_ACTIVE, &vis, args, 0);
        break;
        }
    case DMU_LAYER1_MASK:
        {
        boolean         vis = (sky->layers[0].flags & SLF_MASKED)? true : false;
        DMU_GetValue(DMT_SKY_MASK, &vis, args, 0);
        break;
        }
    case DMU_LAYER2_MASK:
        {
        boolean         vis = (sky->layers[1].flags & SLF_MASKED)? true : false;
        DMU_GetValue(DMT_SKY_MASK, &vis, args, 0);
        break;
        }
    default:
        Con_Error("Sky_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
