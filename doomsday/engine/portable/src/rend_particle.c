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
 * rend_particle.c: Particle Effects
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

// Point + custom textures.
#define NUM_TEX_NAMES           (MAX_PTC_TEXTURES)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

DGLuint pointTex, ptctexname[MAX_PTC_TEXTURES];
int particleLight = 2;
int particleNearLimit = 0;
float particleDiffuse = 4;
byte devDrawGenerators = false; // Display active generators?

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Rend_ParticleRegister(void)
{
    // Cvars
    C_VAR_BYTE("rend-particle", &useParticles, 0, 0, 1);
    C_VAR_INT("rend-particle-max", &maxParticles, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-particle-rate", &particleSpawnRate, 0, 0, 5);
    C_VAR_FLOAT("rend-particle-diffuse", &particleDiffuse,
                CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-particle-visible-near", &particleNearLimit,
              CVF_NO_MAX, 0, 0);
    C_VAR_INT("rend-particle-lights", &particleLight, 0, 0, 10);
    C_VAR_BYTE("rend-dev-generator-show-indices", &devDrawGenerators,
               CVF_NO_ARCHIVE, 0, 1);
}

byte GL_LoadParticleTexture(image_t* image, const char* name)
{
    filename_t          fileName;

    if(R_FindResource2(RT_GRAPHIC, DDRC_TEXTURE, fileName, name, "-ck",
                       FILENAME_T_MAXLEN) &&
       GL_LoadImage(image, fileName))
    {
        return 2;
    }

    return 0;
}

/**
 * The particle texture is a modification of the dynlight texture.
 */
void Rend_ParticleInitTextures(void)
{
    int                 i;
    boolean             reported;

    if(pointTex)
        return; // Already been here.

    // Load the zeroth texture (the default: a blurred point).
    pointTex = GL_PrepareExtTexture(DDRC_GRAPHICS, "Zeroth",
        LGM_WHITE_ALPHA, true, GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 0);

    if(pointTex == 0)
    {
        Con_Error("Rend_ParticleInitTextures: \"Zeroth\" not found.\n");
    }

    // Load any custom particle textures. They are loaded from the
    // highres texture directory and are named "ParticleNN.(tga|png|pcx)".
    // The first is "Particle00". (based on Leesz' textured particles mod)

    // Clear the texture names array.
    memset(ptctexname, 0, sizeof(ptctexname));
    reported = false;
    for(i = 0; i < MAX_PTC_TEXTURES; ++i)
    {
        image_t             image;
        char                name[80];

        // Try to load the texture.
        sprintf(name, "Particle%02i", i);

        if(GL_LoadParticleTexture(&image, name))
        {
            VERBOSE(
            Con_Message("Rend_ParticleInitTextures: Texture "
                        "%02i: %i * %i * %i\n", i, image.width,
                        image.height, image.pixelSize));

            // If 8-bit with no alpha, generate alpha automatically.
            if(image.originalBits == 8)
            {
                GL_ConvertToAlpha(&image, true);
            }

            // Create a new texture and upload the image.
            ptctexname[i] = GL_NewTextureWithParams(
                image.pixelSize == 4 ? DGL_RGBA :
                image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 : DGL_RGB,
                image.width, image.height, image.pixels,
                TXCF_NO_COMPRESSION);

            // Free the buffer.
            GL_DestroyImage(&image);
        }
        else
        {
            // Just show the first 'not found'.
            if(verbose && !reported)
            {
                Con_Message("Rend_ParticleInitTextures: %s not found.\n",
                            name);
                reported = true;
            }
        }
    }
}

void Rend_ParticleShutdownTextures(void)
{
    glDeleteTextures(1, (const GLuint*) &pointTex);
    pointTex = 0;

    glDeleteTextures(NUM_TEX_NAMES, (const GLuint*) ptctexname);
    memset(ptctexname, 0, sizeof(ptctexname));
}

static int drawGeneratorOrigin(void* ptr, void* context)
{
#define MAX_GENERATOR_DIST  2048

    generator_t* gen = (generator_t*) ptr;
    float* eye = (float*) context;

    if(!gen->thinker.function)
        return true; // Continue iteration.

    // Determine approximate center.
    if((gen->source || (gen->flags & PGF_UNTRIGGERED)))
    {
        float pos[3], dist, alpha;

        if(gen->source)
        {
            pos[VX] = gen->source->pos[VX];
            pos[VY] = gen->source->pos[VY];
            pos[VZ] = gen->source->pos[VZ] - gen->source->floorClip +
                FIX2FLT(gen->center[VZ]);
        }
        else
        {
            pos[VX] = FIX2FLT(gen->center[VX]);
            pos[VY] = FIX2FLT(gen->center[VY]);
            pos[VZ] = FIX2FLT(gen->center[VZ]);
        }

        dist = M_Distance(pos, eye);
        alpha = 1 - MIN_OF(dist, MAX_GENERATOR_DIST) / MAX_GENERATOR_DIST;

        if(alpha > 0)
        {
            char buf[80];
            float scale = dist / (theWindow->width / 2);

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();

            glTranslatef(pos[VX], pos[VZ], pos[VY]);
            glRotatef(-vang + 180, 0, 1, 0);
            glRotatef(vpitch, 1, 0, 0);
            glScalef(-scale, -scale, 1);

            // @todo Generator should return the map its linked to.
            sprintf(buf, "Generator: type=%s ptcs=%i age=%i max=%i",
                    gen->source? "src":"static", gen->count, gen->age,
                    gen->def->maxAge);
            UI_TextOutEx(buf, 2, 2, false, false, UI_Color(UIC_TITLE), alpha);

            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
        }
    }

    return true; // Continue iteration.

#undef MAX_GENERATOR_DIST
}

/**
 * Debugging aid; Draw all active generators.
 */
void Rend_RenderGenerators(map_t* map)
{
    float eye[3];

    if(!devDrawGenerators)
        return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    eye[VX] = vx;
    eye[VY] = vz;
    eye[VZ] = vy;

    Map_IterateThinkers2(map, (think_t) P_GeneratorThinker, ITF_PRIVATE,
                        drawGeneratorOrigin, eye);

    // Restore previous state.
    glEnable(GL_DEPTH_TEST);
}
