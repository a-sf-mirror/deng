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
 * gl_texmanager.h: Texture Management
 */

#ifndef __DOOMSDAY_TEXTURE_MANAGER_H__
#define __DOOMSDAY_TEXTURE_MANAGER_H__

#define TEXQ_BEST               8

#include "gl_defer.h"

// Texture load flags:
#define TLF_LOAD_AS_SKY     0x1
#define TLF_LOAD_AS_PSPRITE 0x2
#define TLF_ZEROMASK        0x4 // Zero the alpha of loaded textures.
#define TLF_NO_COMPRESSION  0x8 // Do not compress the loaded textures.

typedef struct {
    byte            flags; // TLF_* texture load flags
    int             tmap, tclass;
} texture_load_params_t;

/**
 * gltexture
 *
 * Presents an abstract interface to all supported texture types so that
 * they may be managed transparently.
 */

typedef enum {
    GLT_ANY = -1,
    GLT_SYSTEM, // system texture e.g., the "missing" texture.
    GLT_FLAT,
    GLT_DOOMTEXTURE,
    GLT_SPRITE,
    GLT_DETAIL,
    GLT_SHINY,
    GLT_MASK,
    GLT_MODELSKIN,
    GLT_MODELSHINYSKIN,
    GLT_LIGHTMAP,
    GLT_FLARE,
    NUM_GLTEXTURE_TYPES
} gltexture_type_t;

#define GLTEXTURE_TYPE_STRING(t)     ((t) == GLT_FLAT? "flat" : \
    (t) == GLT_DOOMTEXTURE? "doomtexture" : \
    (t) == GLT_SPRITE? "sprite" : \
    (t) == GLT_DETAIL? "detailtex" : \
    (t) == GLT_SHINY? "shinytex" : \
    (t) == GLT_MASK? "masktex" : \
    (t) == GLT_MODELSKIN? "modelskin" : \
    (t) == GLT_MODELSHINYSKIN? "modelshinyskin" : \
    (t) == GLT_LIGHTMAP? "lightmap" : \
    (t) == GLT_FLARE? "flaretex" : "systemtex")

typedef unsigned int gltextureid_t; /// \todo Does not belong here.

typedef struct gltexture_s {
    gltextureid_t   id;
    char            name[9];
    gltexture_type_t type;
    int             ofTypeID;
    void*           instances;

    uint            hashNext; // 1-based index
} gltexture_t;

// gltexture flags:
#define GLTF_MASKED         0x1

typedef struct gltexture_inst_s {
    DGLuint         id; // Name of the associated DGL texture.
    byte            flags; // GLTF_* flags.
    const gltexture_t* tex;
    union {
        struct {
            float           color[3]; // Average color (for lighting).
            float           topColor[3]; // Averaged top line color, used for sky fadeouts.
        } texture; // also used with GLT_FLAT.
        struct {
            boolean         pSprite; // @c true, iff this is for use as a psprite.
            float           flareX, flareY, lumSize;
            float           autoLightColor[3];
            float           texCoord[2]; // Prepared texture coordinates.
            int             tmap, tclass; // Color translation.
        } sprite;
        struct {
            float           contrast;
        } detail;
    } data; // type-specific data.
} gltexture_inst_t;

/**
 * This structure is used with GL_LoadImage. When it is no longer needed
 * it must be discarded with GL_DestroyImage.
 */
typedef struct image_s {
    filename_t      fileName;
    int             width;
    int             height;
    int             pixelSize;
    boolean         isMasked;
    int             originalBits;  // Bits per pixel in the image file.
    byte*           pixels;
} image_t;

extern int ratioLimit;
extern int mipmapping, linearRaw, texQuality, filterSprites;
extern int texMagMode, texAniso;
extern int useSmartFilter;
extern byte loadExtAlways;
extern int texMagMode;
extern int upscaleAndSharpenPatches;
extern int glmode[6];

void            GL_TexRegister(void);

void            GL_EarlyInitTextureManager(void);
void            GL_InitTextureManager(void);
void            GL_ResetTextureManager(void);
void            GL_ShutdownTextureManager(void);

void            GL_TexReset(void);
void            GL_LoadSystemTextures(void);
void            GL_ClearTextureMemory(void);
void            GL_ClearRuntimeTextures(void);
void            GL_ClearSystemTextures(void);
void            GL_DeleteRawImages(void);
int             GL_InitPalettedTexture(void);

void            GL_DoTexReset(cvar_t* unused);
void            GL_DoUpdateTexGamma(cvar_t* unused);
void            GL_DoUpdateTexParams(cvar_t* unused);
void            GL_UpdateTexParams(int mipmode);

void            GL_BindTexture(DGLuint texname, int magMode);
void            GL_SetNoTexture(void);
void            GL_SetTextureParams(int minMode, int gameTex, int uiTex);
boolean         GL_IsColorKeyed(const char* path);

byte*           GL_LoadImage(image_t* img, const char* imagefn);
void            GL_DestroyImage(image_t* img);

DGLuint         GL_UploadTexture(byte* data, int width, int height,
                                 boolean flagAlphaChannel,
                                 boolean flagGenerateMipmaps,
                                 boolean flagRgbData,
                                 boolean flagNoStretch,
                                 boolean flagNoSmartFilter,
                                 int minFilter, int magFilter,
                                 int anisoFilter, int wrapS, int wrapT,
                                 int otherFlags);
DGLuint         GL_UploadTexture2(texturecontent_t *content);

byte            GL_LoadFlat(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadDoomTexture(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadSprite(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadDDTexture(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadShinyTexture(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadMaskTexture(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadDetailTexture(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadModelSkin(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadModelShinySkin(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadLightMap(image_t* image, const gltexture_inst_t* inst, void* context);
byte            GL_LoadFlareTexture(image_t* image, const gltexture_inst_t* inst, void* context);


byte            GL_LoadExtTexture(image_t* image, ddresourceclass_t resClass,
                                  const char* name, gfxmode_t mode);

DGLuint         GL_PrepareExtTexture(ddresourceclass_t resClass,
                                     const char* name, gfxmode_t mode,
                                     int useMipmap, int minFilter,
                                     int magFilter, int anisoFilter,
                                     int wrapS, int wrapT, int otherFlags);

DGLuint         GL_GetLightMapTexture(const char* name);
DGLuint         GL_GetFlareTexture(const char* name, int oldIdx);

void            GL_SetPatch(lumpnum_t lump, int wrapS, int wrapT); // No mipmaps are generated.

void            GL_SetRawImage(lumpnum_t lump, boolean part2, int wrapS,
                               int wrapT);


// Management of and access to gltextures (via the texmanager):
const gltexture_t* GL_CreateGLTexture(const char* name, int ofTypeID,
                                      gltexture_type_t type);
void            GL_ReleaseGLTexture(gltextureid_t id);
const gltexture_inst_t* GL_PrepareGLTexture(gltextureid_t id, void* context,
                                            byte* result);
const gltexture_t* GL_GetGLTexture(gltextureid_t id);
const gltexture_t* GL_GetGLTextureByName(const char* name,
                                         gltexture_type_t type);
void            GL_SetAllGLTexturesMinMode(int minMode);
void            GL_DeleteAllTexturesForGLTextures(gltexture_type_t);

/// \todo should not be visible outside the texmanager?
float           GLTexture_GetWidth(const gltexture_t* tex);
float           GLTexture_GetHeight(const gltexture_t* tex);
boolean         GLTexture_IsFromIWAD(const gltexture_t* tex);

#endif
