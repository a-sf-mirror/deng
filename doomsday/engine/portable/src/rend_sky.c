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
 * rend_sky.c: Sky Sphere and 3D Models
 *
 * This version supports only two sky layers.
 * (More would be a waste of resources?)
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

#define SKYVTX_IDX(c, r)    ( (r)*columns + (c)%columns )

// TYPES -------------------------------------------------------------------

typedef struct skyvertex_s {
    float           pos[3];
} skyvertex_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(SkyDetail);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int skyDetail = 6, skySimple;
int skyColumns = 4 * 6, skyRows = 3;
float skyDist = 1600;
int skyFull = false;
boolean skyUpdateSphere = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static skyvertex_t* skyVerts = NULL; // Vertices for the upper hemisphere.
static int numSkyVerts = 0;

// CODE --------------------------------------------------------------------

void Rend_SkyRegister(void)
{
    // Cvars
    C_VAR_INT("rend-sky-detail", &skyDetail, CVF_PROTECTED, 3, 7);
    C_VAR_INT("rend-sky-rows", &skyRows, CVF_PROTECTED, 1, 8);
    C_VAR_FLOAT("rend-sky-distance", &skyDist, CVF_NO_MAX, 1, 0);
    C_VAR_INT("rend-sky-full", &skyFull, 0, 0, 1);
    C_VAR_INT("rend-sky-simple", &skySimple, 0, 0, 2);

    // Ccmds
    C_CMD_FLAGS("skydetail", "i", SkyDetail, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("skyrows", "i", SkyDetail, CMDF_NO_DEDICATED);
}

static void buildSkySphere(int columns, int rows, float horizonOffset,
                           float maxSideAngle)
{
    float               topAngle, sideAngle, realRadius, scale = 1 /*32 */ ;
    int                 c, r;
    skyvertex_t*        svtx;

    // Calculate the sky vertices.
    numSkyVerts = columns * (rows + 1);

    // Allocate memory for it.
    skyVerts = M_Realloc(skyVerts, sizeof(*skyVerts) * numSkyVerts);

    // Calculate the vertices.
    for(r = 0; r <= rows; ++r)
        for(c = 0; c < columns; ++c)
        {
            svtx = skyVerts + SKYVTX_IDX(c, r);
            topAngle = ((c / (float) columns) *2) * PI;
            sideAngle = horizonOffset +
                maxSideAngle * (rows - r) / (float) rows;
            realRadius = scale * cos(sideAngle);

            svtx->pos[VX] = realRadius * cos(topAngle);
            svtx->pos[VY] = scale * sin(sideAngle); // The height.
            svtx->pos[VZ] = realRadius * sin(topAngle);
        }
}

static __inline void capSideVertex(int r, int c, int columns, boolean yflip)
{   // Look up the precalculated vertex.
    skyvertex_t*        svtx = &skyVerts[SKYVTX_IDX(c, r)];

    glVertex3f(svtx->pos[VX], svtx->pos[VY] * (yflip ? -1 : 1), svtx->pos[VZ]);
}

static void sphereVertex(int r, int c, int rows, int columns, boolean yflip,
                         float texWidth, float texOffsetX, boolean fadeout)
{
    // The direction must be clockwise.
    skyvertex_t*        svtx = &skyVerts[SKYVTX_IDX(c, r)];
    float               s = (1024 / texWidth) *
        c / (float) columns + texOffsetX / texWidth;
    float               t = (yflip ? (rows - r) : r) / (float) rows;

    glTexCoord2f(s, t);

    // Also the color.
    if(fadeout)
    {
        if(r == 0)
            glColor4f(1, 1, 1, 0);
        else
            glColor3f(1, 1, 1);
    }
    else
    {
        if(r == 0)
            glColor3f(0, 0, 0);
        else
            glColor3f(1, 1, 1);
    }

    // And finally the vertex itself.
    glVertex3f(svtx->pos[VX], svtx->pos[VY] * (yflip ? -1 : 1), svtx->pos[VZ]);
}

static void renderCapTop(int columns, boolean yflip)
{
    int                 c;

    glBegin(GL_TRIANGLE_FAN);

    for(c = 0; c < columns; ++c)
    {
        capSideVertex(0, c, columns, yflip);
    }

    glEnd();
}

static void renderCapSide(int columns, boolean yflip)
{
    int                 c;

    glBegin(GL_TRIANGLE_STRIP);

    capSideVertex(0, 0, columns, yflip);
    for(c = 0; c < columns; ++c)
    {
        capSideVertex(1, c, columns, yflip); // One step down.
        capSideVertex(0, c + 1, columns, yflip); // And one step right.
    }
    capSideVertex(1, c, columns, yflip);

    glEnd();
}

static void renderCap(boolean upperHemi, int columns, const float fadeColor[3])
{
    static const float  black[] = { 0, 0, 0 };

    /**
     * The top row (row 0) is the one that's faded out.
     * There must be at least 4 columns. The preferable number is 4n, where
     * n is 1, 2, 3... There should be at least two rows because the first
     * one is always faded.
     */

    glDisable(GL_TEXTURE_2D);
    glColor3fv(fadeColor ? fadeColor : black);

    renderCapTop(columns, !upperHemi);

    // If we are doing a colored fadeout, we must fill the background for the
    // edge row since it'll be partially translucent.
    if(fadeColor)
    {
        renderCapSide(columns, !upperHemi);
    }

    glEnable(GL_TEXTURE_2D);
}

static void renderHemisphere2(boolean upperHemi, int rows, int columns,
                              float texWidth, float texOffsetX,
                              boolean fadeout, boolean simple)
{
    int                 r, c;

    // The total number of triangles per hemisphere can be calculated
    // as follows: rows * columns * 2 + 2 (for the top cap).
    for(r = 0; r < rows; ++r)
    {
        if(simple)
        {
            glBegin(GL_QUADS);
            for(c = 0; c < columns; ++c)
            {
                sphereVertex(r, c, rows, columns, !upperHemi, texWidth, texOffsetX, fadeout);
                sphereVertex(r + 1, c, rows, columns, !upperHemi, texWidth, texOffsetX, fadeout);
                sphereVertex(r + 1, c + 1, rows, columns, !upperHemi, texWidth, texOffsetX, fadeout);
                sphereVertex(r, c + 1, rows, columns, !upperHemi, texWidth, texOffsetX, fadeout);
            }
            glEnd();
        }
        else
        {
            glBegin(GL_TRIANGLE_STRIP);
            sphereVertex(r, 0, rows, columns, !upperHemi, texWidth, texOffsetX, fadeout);
            sphereVertex(r + 1, 0, rows, columns, !upperHemi, texWidth, texOffsetX, fadeout);
            for(c = 1; c <= columns; ++c)
            {
                sphereVertex(r, c, rows, columns, !upperHemi, texWidth, texOffsetX, fadeout);
                sphereVertex(r + 1, c, rows, columns, !upperHemi, texWidth, texOffsetX, fadeout);
            }
            glEnd();
        }
    }
}

static void renderHemisphere(sky_t* sky, boolean upperHemi)
{
    int                 i, firstLayer = Sky_GetFirstLayer(sky);
    material_snapshot_t ms;
    const float*        fadeColor = NULL;

    if(renderTextures)
    {
        material_t*         material = Sky_GetSphereMaterial(sky);

        Material_Prepare(&ms, material, MPF_SMOOTH | MPF_AS_SKY | MPF_TEX_NO_COMPRESSION, NULL);

        for(i = 0; i < 3; ++i)
            if(ms.topColor[i] > sky->layers[firstLayer].fadeoutColorLimit)
            {
                // Colored fadeout is needed.
                fadeColor = ms.topColor;
                break;
            }
    }

    // First render the cap and the background for fadeouts, if needed.
    // The color for both is the current fadeout color.
    renderCap(upperHemi, skyColumns, fadeColor);

    for(i = firstLayer; i < MAX_SKY_LAYERS; ++i)
    {
        float               texWidth, texHeight, texOffsetX;

        if(!Sky_IsLayerActive(sky, i))
            continue;

        // The texture is actually loaded when an update is done.
        if(renderTextures)
        {
            GL_BindTexture(ms.units[MTU_PRIMARY].texInst->id,
                           ms.units[MTU_PRIMARY].magMode);
            texWidth  = GLTexture_GetWidth(ms.units[MTU_PRIMARY].texInst->tex);
            texHeight = GLTexture_GetHeight(ms.units[MTU_PRIMARY].texInst->tex);
            texOffsetX = ms.units[MTU_PRIMARY].offset[0];
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, 0);
            texWidth = texHeight = 64;
            texOffsetX = 0;
        }

        renderHemisphere2(upperHemi, skyRows, skyColumns, texWidth, texOffsetX,
                          fadeColor != NULL, skySimple);
    }
}

static void renderSphere(sky_t* sky, int hemis)
{
    static float        currentHorizonOffset = DDMAXFLOAT;
    static float        currentMaxSideAngle = DDMAXFLOAT;

    float               horizonOffset = PI / 2 *
        (sky->def? sky->def->horizonOffset : 0);
    float               maxSideAngle = PI / 2 *
        (sky->def? sky->def->height : .666667f);
    boolean             buildSphere = skyUpdateSphere ||
        !INRANGE_OF(horizonOffset, currentHorizonOffset, .00001f) ||
        !INRANGE_OF(maxSideAngle, currentMaxSideAngle, .00001f); 
 
    // Recalculate the vertices?
    if(buildSphere)
    {
        buildSkySphere(skyColumns, skyRows, horizonOffset, maxSideAngle);

        currentHorizonOffset = horizonOffset;
        currentMaxSideAngle = maxSideAngle;
        skyUpdateSphere = false;
    }

    // Always render the full sky?
    if(skyFull)
        hemis = SKYHEMI_UPPER | SKYHEMI_LOWER;

    // We don't want anything written in the depth buffer, not yet.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    // Disable culling, all triangles face the viewer.
    glDisable(GL_CULL_FACE);
    GL_DisableArrays(true, true, DDMAXINT);

    // Setup a proper matrix.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(vx, vy, vz);
    glScalef(skyDist, skyDist, skyDist);

    // Draw the possibly visible hemispheres.
    if(hemis & SKYHEMI_LOWER)
        renderHemisphere(sky, false);
    if(hemis & SKYHEMI_UPPER)
        renderHemisphere(sky, true);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Enable the disabled things.
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

static void renderModels(sky_t* sky)
{
    int                 i;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Setup basic translation.
    glTranslatef(vx, vy, vz);

    for(i = 0; i < NUM_SKY_MODELS; ++i)
    {
        const skymodel_t*   model = &sky->models[i];
        int                 c;
        float               inter, pos[3];
        rendmodelparams_t   params;

        if(!model->def)
            continue;

        if(model->def->layer > 0 && model->def->layer <= MAX_SKY_LAYERS &&
           !Sky_IsLayerActive(sky, model->def->layer - 1))
        {
            // The model has been assigned to a layer, but the layer is
            // not visible.
            continue;
        }

        // Calculate the coordinates for the model.
        pos[0] = vx * -model->def->coordFactor[0];
        pos[1] = vy * -model->def->coordFactor[1];
        pos[2] = vz * -model->def->coordFactor[2];

        inter = (model->maxTimer > 0 ? model->timer / (float) model->maxTimer : 0);

        memset(&params, 0, sizeof(params));

        params.distance = 1;
        params.center[VX] = pos[0];
        params.center[VY] = pos[2];
        params.center[VZ] = params.gzt = pos[1];
        params.extraYawAngle = params.yawAngleOffset = model->def->rotate[0];
        params.extraPitchAngle = params.pitchAngleOffset = model->def->rotate[1];
        params.inter = inter;
        params.mf = model->model;
        params.alwaysInterpolate = true;
        R_SetModelFrame(model->model, model->frame);
        params.yaw = model->yaw;
        for(c = 0; c < 4; ++c)
        {
            params.ambientColor[c] = model->def->color[c];
        }
        params.vLightListIdx = 0;
        params.shineTranslateWithViewerPos = true;

        Rend_RenderModel(&params);
    }

    // We don't want that anything interferes with what was drawn.
    glClear(GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Rend_ShutdownSky(void)
{
    if(skyVerts)
        M_Free(skyVerts);
    skyVerts = NULL;
    numSkyVerts = 0;
}

void Rend_RenderSky(sky_t* sky, int hemis)
{
    if(!sky)
        return;

    // IS there a sky to be rendered?
    if(!hemis || sky->firstLayer == -1)
        return;

    // The sky sphere is not drawn if models are in use unless overridden
    // in the sky definition (in which case it will be drawn first).
    if(!sky->modelsInited ||
       (sky->def && (sky->def->flags & SIF_DRAW_SPHERE) != 0))
    {
        renderSphere(sky, hemis);
    }

    // How about some 3D models?
    if(sky->modelsInited)
    {
        renderModels(sky);
    }
}

void Rend_SetSkyDetail(int quarterDivs, int rows)
{
    skyDetail = MAX_OF(quarterDivs, 1);
    skyColumns = 4 * skyDetail;
    skyRows = MAX_OF(rows, 1);

    skyUpdateSphere = true;
}

D_CMD(SkyDetail)
{
    if(!stricmp(argv[0], "skydetail"))
    {
        Rend_SetSkyDetail(strtol(argv[1], NULL, 0), skyRows);
    }
    else if(!stricmp(argv[0], "skyrows"))
    {
        Rend_SetSkyDetail(skyDetail, strtol(argv[1], NULL, 0));
    }
    return true;
}
