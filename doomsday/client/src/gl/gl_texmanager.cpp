/** @file gl_texmanager.cpp GL-Texture management
 *
 * @todo This file needs to be split into smaller portions.
 *
 * @authors Copyright &copy; 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2002 Graham Jackson <no contact email published>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_platform.h"
#include "gl/gl_texmanager.h"

#include "de_filesys.h"
#include "de_resource.h"
#include "clientapp.h"
#include "dd_main.h" // App_ResourceSystem()
#include "con_main.h"
#include "con_bar.h"
#include "def_main.h"

#include "gl/gl_main.h" // DENG_ASSERT_GL_CONTEXT_ACTIVE
#include "gl/texturecontent.h"

#include "render/r_main.h" // R_BuildTexGammaLut
#include "render/rend_halo.h" // haloRealistic
#include "render/rend_main.h" // misc global vars
#include "render/rend_particle.h" // Rend_ParticleLoadSystemTextures()

#include "resource/hq2x.h"

#include <de/memory.h>
#include <de/memoryzone.h>
#include <cstring>

using namespace de;

static boolean initedOk = false; // Init done.

// Names of the dynamic light textures.
static DGLuint lightingTextures[NUM_LIGHTING_TEXTURES];

// Names of the flare textures (halos).
static DGLuint sysFlareTextures[NUM_SYSFLARE_TEXTURES];

struct texturevariantspecificationlist_node_t
{
    texturevariantspecificationlist_node_t *next;
    texturevariantspecification_t *spec;
};
typedef texturevariantspecificationlist_node_t variantspecificationlist_t;
static variantspecificationlist_t *variantSpecs;

/// @c TST_DETAIL type specifications are stored separately into a set of
/// buckets. Bucket selection is determined by their quantized contrast value.
#define DETAILVARIANT_CONTRAST_HASHSIZE     (DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR+1)
static variantspecificationlist_t *detailVariantSpecs[DETAILVARIANT_CONTRAST_HASHSIZE];

static int hashDetailVariantSpecification(detailvariantspecification_t const &spec)
{
    return (spec.contrast * (1/255.f) * DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR + .5f);
}

static void unlinkVariantSpecification(texturevariantspecification_t &spec)
{
    variantspecificationlist_t **listHead;
    DENG2_ASSERT(initedOk);

    // Select list head according to variant specification type.
    switch(spec.type)
    {
    case TST_GENERAL:   listHead = &variantSpecs; break;
    case TST_DETAIL: {
        int hash = hashDetailVariantSpecification(TS_DETAIL(spec));
        listHead = &detailVariantSpecs[hash];
        break; }
    }

    if(*listHead)
    {
        texturevariantspecificationlist_node_t *node = 0;
        if((*listHead)->spec == &spec)
        {
            node = (*listHead);
            *listHead = (*listHead)->next;
        }
        else
        {
            // Find the previous node.
            texturevariantspecificationlist_node_t *prevNode = (*listHead);
            while(prevNode->next && prevNode->next->spec != &spec)
            {
                prevNode = prevNode->next;
            }
            if(prevNode)
            {
                node = prevNode->next;
                prevNode->next = prevNode->next->next;
            }
        }
        M_Free(node);
    }
}

static void destroyVariantSpecification(texturevariantspecification_t &spec)
{
    unlinkVariantSpecification(spec);
    if(spec.type == TST_GENERAL && (TS_GENERAL(spec).flags & TSF_HAS_COLORPALETTE_XLAT))
    {
        M_Free(TS_GENERAL(spec).translated);
    }
    M_Free(&spec);
}

static texturevariantspecification_t *copyVariantSpecification(
    texturevariantspecification_t const &tpl)
{
    texturevariantspecification_t *spec = (texturevariantspecification_t *) M_Malloc(sizeof(*spec));

    std::memcpy(spec, &tpl, sizeof(texturevariantspecification_t));
    if(TS_GENERAL(tpl).flags & TSF_HAS_COLORPALETTE_XLAT)
    {
        colorpalettetranslationspecification_t *cpt = (colorpalettetranslationspecification_t *) M_Malloc(sizeof(*cpt));

        std::memcpy(cpt, TS_GENERAL(tpl).translated, sizeof(colorpalettetranslationspecification_t));
        TS_GENERAL(*spec).translated = cpt;
    }
    return spec;
}

static texturevariantspecification_t *copyDetailVariantSpecification(
    texturevariantspecification_t const &tpl)
{
    texturevariantspecification_t *spec = (texturevariantspecification_t *) M_Malloc(sizeof(*spec));

    std::memcpy(spec, &tpl, sizeof(texturevariantspecification_t));
    return spec;
}

static colorpalettetranslationspecification_t *applyColorPaletteTranslationSpecification(
    colorpalettetranslationspecification_t *spec, int tClass, int tMap)
{
    DENG2_ASSERT(initedOk && spec);
    LOG_AS("applyColorPaletteTranslationSpecification");

    spec->tClass = de::max(0, tClass);
    spec->tMap   = de::max(0, tMap);

#ifdef DENG_DEBUG
    if(0 == tClass && 0 == tMap)
    {
        LOG_WARNING("Applied unnecessary zero-translation (tClass:0 tMap:0).");
    }
#endif

    return spec;
}

static variantspecification_t &applyVariantSpecification(
    variantspecification_t &spec, texturevariantusagecontext_t tc, int flags,
    byte border, colorpalettetranslationspecification_t *colorPaletteTranslationSpec,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter, boolean mipmapped,
    boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    DENG2_ASSERT(initedOk && (tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc)));

    flags &= ~TSF_INTERNAL_MASK;

    spec.context = tc;
    spec.flags = flags;
    spec.border = (flags & TSF_UPSCALE_AND_SHARPEN)? 1 : border;
    spec.mipmapped = mipmapped;
    spec.wrapS = wrapS;
    spec.wrapT = wrapT;
    spec.minFilter = MINMAX_OF(-1, minFilter, spec.mipmapped? 3:1);
    spec.magFilter = MINMAX_OF(-3, magFilter, 1);
    spec.anisoFilter = MINMAX_OF(-1, anisoFilter, 4);
    spec.gammaCorrection = gammaCorrection;
    spec.noStretch = noStretch;
    spec.toAlpha = toAlpha;
    if(colorPaletteTranslationSpec)
    {
        spec.flags |= TSF_HAS_COLORPALETTE_XLAT;
        spec.translated = colorPaletteTranslationSpec;
    }
    else
    {
        spec.translated = NULL;
    }

    return spec;
}

static detailvariantspecification_t &applyDetailVariantSpecification(
    detailvariantspecification_t &spec, float contrast)
{
    int const quantFactor = DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR;

    spec.contrast = 255 * (int)MINMAX_OF(0, contrast * quantFactor + .5f, quantFactor) * (1 / float(quantFactor));
    return spec;
}

static texturevariantspecification_t &linkVariantSpecification(
    texturevariantspecificationtype_t type, texturevariantspecification_t &spec)
{
    texturevariantspecificationlist_node_t *node;
    DENG2_ASSERT(initedOk && VALID_TEXTUREVARIANTSPECIFICATIONTYPE(type));

    node = (texturevariantspecificationlist_node_t *) M_Malloc(sizeof(*node));
    node->spec = &spec;
    switch(type)
    {
    case TST_GENERAL:
        node->next = variantSpecs;
        variantSpecs = (variantspecificationlist_t *)node;
        break;
    case TST_DETAIL: {
        int hash = hashDetailVariantSpecification(TS_DETAIL(spec));
        node->next = detailVariantSpecs[hash];
        detailVariantSpecs[hash] = (variantspecificationlist_t *)node;
        break; }
    }

    return spec;
}

static texturevariantspecification_t *findVariantSpecification(
    texturevariantspecificationtype_t type, texturevariantspecification_t const &tpl,
    bool canCreate)
{
    texturevariantspecificationlist_node_t *node = 0;
    DENG2_ASSERT(initedOk && VALID_TEXTUREVARIANTSPECIFICATIONTYPE(type));

    // Select list head according to variant specification type.
    switch(type)
    {
    case TST_GENERAL:   node = variantSpecs; break;
    case TST_DETAIL: {
        int hash = hashDetailVariantSpecification(TS_DETAIL(tpl));
        node = detailVariantSpecs[hash];
        break; }
    }

    // Do we already have a concrete version of the template specification?
    for(; node; node = node->next)
    {
        if(TextureVariantSpec_Compare(node->spec, &tpl))
        {
            return node->spec;
        }
    }

    // Not found, can we create?
    if(canCreate)
    {
        switch(type)
        {
        case TST_GENERAL:   return &linkVariantSpecification(type, *copyVariantSpecification(tpl));
        case TST_DETAIL:    return &linkVariantSpecification(type, *copyDetailVariantSpecification(tpl));
        }
    }

    return 0;
}

static texturevariantspecification_t *getVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    DENG2_ASSERT(initedOk);

    static texturevariantspecification_t tpl;
    static colorpalettetranslationspecification_t cptTpl;

    tpl.type = TST_GENERAL;

    bool haveCpt = false;
    if(0 != tClass || 0 != tMap)
    {
        // A color palette translation spec is required.
        applyColorPaletteTranslationSpecification(&cptTpl, tClass, tMap);
        haveCpt = true;
    }

    applyVariantSpecification(TS_GENERAL(tpl), tc, flags, border, haveCpt? &cptTpl : NULL,
        wrapS, wrapT, minFilter, magFilter, anisoFilter, mipmapped, gammaCorrection,
        noStretch, toAlpha);

    // Retrieve a concrete version of the rationalized specification.
    return findVariantSpecification(tpl.type, tpl, true);
}

static texturevariantspecification_t *getDetailVariantSpecificationForContext(
    float contrast)
{
    DENG2_ASSERT(initedOk);

    static texturevariantspecification_t tpl;

    tpl.type = TST_DETAIL;
    applyDetailVariantSpecification(TS_DETAIL(tpl), contrast);
    return findVariantSpecification(tpl.type, tpl, true);
}

static void emptyVariantSpecificationList(variantspecificationlist_t *list)
{
    DENG2_ASSERT(initedOk);

    texturevariantspecificationlist_node_t *node = (texturevariantspecificationlist_node_t *) list;
    while(node)
    {
        texturevariantspecificationlist_node_t *next = node->next;
        destroyVariantSpecification(*node->spec);
        node = next;
    }
}

static bool variantSpecInUse(texturevariantspecification_t const &spec)
{
    foreach(Texture *texture, App_Textures().all())
    foreach(TextureVariant *variant, texture->variants())
    {
        if(&variant->spec() == &spec)
        {
            return true; // Found one; stop.
        }
    }
    return false;
}

static int pruneUnusedVariantSpecificationsInList(variantspecificationlist_t *list)
{
    texturevariantspecificationlist_node_t *node = list;
    int numPruned = 0;
    while(node)
    {
        texturevariantspecificationlist_node_t *next = node->next;

        if(!variantSpecInUse(*node->spec))
        {
            destroyVariantSpecification(*node->spec);
            ++numPruned;
        }

        node = next;
    }
    return numPruned;
}

static int pruneUnusedVariantSpecifications(texturevariantspecificationtype_t specType)
{
    DENG2_ASSERT(initedOk);
    switch(specType)
    {
    case TST_GENERAL: return pruneUnusedVariantSpecificationsInList(variantSpecs);
    case TST_DETAIL: {
        int numPruned = 0;
        for(int i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
        {
            numPruned += pruneUnusedVariantSpecificationsInList(detailVariantSpecs[i]);
        }
        return numPruned; }
    }
}

static void destroyVariantSpecifications()
{
    DENG2_ASSERT(initedOk);

    emptyVariantSpecificationList(variantSpecs); variantSpecs = 0;
    for(int i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
    {
        emptyVariantSpecificationList(detailVariantSpecs[i]); detailVariantSpecs[i] = 0;
    }
}

void GL_InitTextureManager()
{
    if(initedOk)
    {
        GL_LoadLightingSystemTextures();
        GL_LoadFlareTextures();

        Rend_ParticleLoadSystemTextures();
        return; // Already been here.
    }

    // Disable the use of 'high resolution' textures and/or patches?
    noHighResTex     = CommandLine_Exists("-nohightex");
    noHighResPatches = CommandLine_Exists("-nohighpat");
    // Should we allow using external resources with PWAD textures?
    highResWithPWAD  = CommandLine_Exists("-pwadtex");

    // System textures.
    zap(sysFlareTextures);
    zap(lightingTextures);

    variantSpecs = 0;
    zap(detailVariantSpecs);

    GL_InitSmartFilterHQ2x();

    // Initialization done.
    initedOk = true;
}

void GL_ResetTextureManager()
{
    if(!initedOk) return;

    App_ResourceSystem().releaseAllGLTextures();
    GL_PruneTextureVariantSpecifications();

    GL_LoadLightingSystemTextures();
    GL_LoadFlareTextures();
    Rend_ParticleLoadSystemTextures();
}

static int reloadTextures(void *context)
{
    bool const usingBusyMode = *static_cast<bool *>(context);

    /// @todo re-upload ALL textures currently in use.
    GL_LoadLightingSystemTextures();
    GL_LoadFlareTextures();

    Rend_ParticleLoadSystemTextures();
    Rend_ParticleLoadExtraTextures();

    if(usingBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

void GL_TexReset()
{
    if(!initedOk) return;

    App_ResourceSystem().releaseAllGLTextures();
    LOG_MSG("All DGL textures deleted.");

    bool useBusyMode = !BusyMode_Active();
    if(useBusyMode)
    {
        Con_InitProgress(200);
        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                    reloadTextures, &useBusyMode, "Reseting textures...");
    }
    else
    {
        reloadTextures(&useBusyMode);
    }
}

void GL_PruneTextureVariantSpecifications()
{
    if(!initedOk) return;
    if(Sys_IsShuttingDown()) return;

    int numPruned = 0;
    numPruned += pruneUnusedVariantSpecifications(TST_GENERAL);
    numPruned += pruneUnusedVariantSpecifications(TST_DETAIL);

#ifdef DENG_DEBUG
    LOG_VERBOSE("Pruned %i unused texture variant %s.")
        << numPruned << (numPruned == 1? "specification" : "specifications");
#endif
}

texturevariantspecification_t &GL_TextureVariantSpec(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    DENG2_ASSERT(initedOk);

    texturevariantspecification_t *tvs =
        getVariantSpecificationForContext(tc, flags, border, tClass, tMap, wrapS, wrapT,
                                          minFilter, magFilter, anisoFilter,
                                          mipmapped, gammaCorrection, noStretch, toAlpha);

#ifdef DENG_DEBUG
    if(tClass || tMap)
    {
        DENG2_ASSERT(tvs->data.variant.flags & TSF_HAS_COLORPALETTE_XLAT);
        DENG2_ASSERT(tvs->data.variant.translated);
        DENG2_ASSERT(tvs->data.variant.translated->tClass == tClass);
        DENG2_ASSERT(tvs->data.variant.translated->tMap == tMap);
    }
#endif

    return *tvs;
}

texturevariantspecification_t &GL_DetailTextureSpec(float contrast)
{
    DENG2_ASSERT(initedOk);
    return *getDetailVariantSpecificationForContext(contrast);
}

void GL_ShutdownTextureManager()
{
    if(!initedOk) return;

    destroyVariantSpecifications();
    initedOk = false;
}

void GL_LoadLightingSystemTextures()
{
    if(novideo || !initedOk) return;

    // Preload lighting system textures.
    GL_PrepareLSTexture(LST_DYNAMIC);
    GL_PrepareLSTexture(LST_GRADIENT);
    GL_PrepareLSTexture(LST_CAMERA_VIGNETTE);
}

void GL_LoadFlareTextures()
{
    if(novideo || !initedOk) return;

    GL_PrepareSysFlaremap(FXT_ROUND);
    GL_PrepareSysFlaremap(FXT_FLARE);
    if(!haloRealistic)
    {
        GL_PrepareSysFlaremap(FXT_BRFLARE);
        GL_PrepareSysFlaremap(FXT_BIGFLARE);
    }
}

void GL_ReleaseAllLightingSystemTextures()
{
    if(novideo || !initedOk) return;

    glDeleteTextures(NUM_LIGHTING_TEXTURES, (GLuint const *) lightingTextures);
    zap(lightingTextures);
}

void GL_ReleaseAllFlareTextures()
{
    if(novideo || !initedOk) return;

    glDeleteTextures(NUM_SYSFLARE_TEXTURES, (GLuint const *) sysFlareTextures);
    zap(sysFlareTextures);
}

GLuint GL_PrepareLSTexture(lightingtexid_t which)
{
    if(novideo) return 0;
    if(which < 0 || which >= NUM_LIGHTING_TEXTURES) return 0;

    static const struct TexDef {
        char const *name;
        gfxmode_t mode;
    } texDefs[NUM_LIGHTING_TEXTURES] = {
        /* LST_DYNAMIC */         { "dlight",     LGM_WHITE_ALPHA },
        /* LST_GRADIENT */        { "wallglow",   LGM_WHITE_ALPHA },
        /* LST_RADIO_CO */        { "radioco",    LGM_WHITE_ALPHA },
        /* LST_RADIO_CC */        { "radiocc",    LGM_WHITE_ALPHA },
        /* LST_RADIO_OO */        { "radiooo",    LGM_WHITE_ALPHA },
        /* LST_RADIO_OE */        { "radiooe",    LGM_WHITE_ALPHA },
        /* LST_CAMERA_VIGNETTE */ { "vignette",   LGM_NORMAL }
    };
    struct TexDef const &def = texDefs[which];

    if(!lightingTextures[which])
    {
        image_t image;

        if(GL_LoadExtImage(image, def.name, def.mode))
        {
            // Loaded successfully and converted accordingly.
            // Upload the image to GL.
            DGLuint glName = GL_NewTextureWithParams(
                ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                  image.pixelSize == 3 ? DGL_RGB :
                  image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
                image.size.width, image.size.height, image.pixels,
                TXCF_NO_COMPRESSION, 0, GL_LINEAR, GL_LINEAR, -1 /*best anisotropy*/,
                ( which == LST_GRADIENT? GL_REPEAT : GL_CLAMP_TO_EDGE ), GL_CLAMP_TO_EDGE);

            lightingTextures[which] = glName;
        }

        Image_Destroy(&image);
    }

    DENG2_ASSERT(lightingTextures[which] != 0);
    return lightingTextures[which];
}

GLuint GL_PrepareSysFlaremap(flaretexid_t which)
{
    if(novideo) return 0;
    if(which < 0 || which >= NUM_SYSFLARE_TEXTURES) return 0;

    static const struct TexDef {
        char const *name;
    } texDefs[NUM_SYSFLARE_TEXTURES] = {
        /* FXT_ROUND */     { "dlight" },
        /* FXT_FLARE */     { "flare" },
        /* FXT_BRFLARE */   { "brflare" },
        /* FXT_BIGFLARE */  { "bigflare" }
    };
    struct TexDef const &def = texDefs[which];

    if(!sysFlareTextures[which])
    {
        image_t image;

        if(GL_LoadExtImage(image, def.name, LGM_WHITE_ALPHA))
        {
            // Loaded successfully and converted accordingly.
            // Upload the image to GL.
            DGLuint glName = GL_NewTextureWithParams(
                ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                  image.pixelSize == 3 ? DGL_RGB :
                  image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
                image.size.width, image.size.height, image.pixels,
                TXCF_NO_COMPRESSION, 0, GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

            sysFlareTextures[which] = glName;
        }

        Image_Destroy(&image);
    }

    DENG2_ASSERT(sysFlareTextures[which] != 0);
    return sysFlareTextures[which];
}

GLuint GL_PrepareFlaremap(de::Uri const &resourceUri)
{
    if(resourceUri.path().length() == 1)
    {
        // Select a system flare by numeric identifier?
        int number = resourceUri.path().toStringRef().first().digitValue();
        if(number == 0) return 0; // automatic
        if(number >= 1 && number <= 4)
        {
            return GL_PrepareSysFlaremap(flaretexid_t(number - 1));
        }
    }
    if(Texture *tex = App_ResourceSystem().texture("Flaremaps", &resourceUri))
    {
        if(TextureVariant const *variant = tex->prepareVariant(Rend_HaloTextureSpec()))
        {
            return variant->glName();
        }
        // Dang...
    }
    return 0;
}

static res::Source loadRaw(image_t &image, rawtex_t const &raw)
{
    // First try an external resource.
    try
    {
        String foundPath = App_FileSystem().findPath(de::Uri("Patches", Path(Str_Text(&raw.name))),
                                                     RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        // Ensure the found path is absolute.
        foundPath = App_BasePath() / foundPath;

        return GL_LoadImage(image, foundPath)? res::External : res::None;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.

    if(raw.lumpNum >= 0)
    {
        filehandle_s *file = F_OpenLump(raw.lumpNum);
        if(file)
        {
            if(Image_LoadFromFile(&image, file))
            {
                F_Delete(file);
                return res::Original;
            }

            // It must be an old-fashioned "raw" image.
#define RAW_WIDTH           320
#define RAW_HEIGHT          200

            Image_Init(&image);

            size_t fileLength = FileHandle_Length(file);
            size_t bufSize = 3 * RAW_WIDTH * RAW_HEIGHT;

            image.pixels = (uint8_t *) M_Malloc(bufSize);
            if(fileLength < bufSize)
                std::memset(image.pixels, 0, bufSize);

            // Load the raw image data.
            FileHandle_Read(file, image.pixels, fileLength);
            image.size.width = RAW_WIDTH;
            image.size.height = int(fileLength / image.size.width);
            image.pixelSize = 1;

            F_Delete(file);
            return res::Original;

#undef RAW_HEIGHT
#undef RAW_WIDTH
        }
    }

    return res::None;
}

GLuint GL_PrepareRawTexture(rawtex_t &raw)
{
    if(raw.lumpNum < 0 || raw.lumpNum >= F_LumpCount()) return 0;

    if(!raw.tex)
    {
        image_t image;
        Image_Init(&image);

        if(loadRaw(image, raw) == res::External)
        {
            // Loaded an external raw texture.
            raw.tex = GL_NewTextureWithParams(image.pixelSize == 4? DGL_RGBA : DGL_RGB,
                image.size.width, image.size.height, image.pixels, 0, 0,
                GL_NEAREST, (filterUI ? GL_LINEAR : GL_NEAREST), 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        }
        else
        {
            raw.tex = GL_NewTextureWithParams(
                (image.flags & IMGF_IS_MASKED)? DGL_COLOR_INDEX_8_PLUS_A8 :
                          image.pixelSize == 4? DGL_RGBA :
                          image.pixelSize == 3? DGL_RGB : DGL_COLOR_INDEX_8,
                image.size.width, image.size.height, image.pixels, 0, 0,
                GL_NEAREST, (filterUI? GL_LINEAR:GL_NEAREST), 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        }

        raw.width  = image.size.width;
        raw.height = image.size.height;
        Image_Destroy(&image);
    }

    return raw.tex;
}

void GL_SetRawTexturesMinFilter(int newMinFilter)
{
    rawtex_t **rawTexs = R_CollectRawTexs();
    for(rawtex_t **ptr = rawTexs; *ptr; ptr++)
    {
        rawtex_t *r = *ptr;
        if(r->tex) // Is the texture loaded?
        {
            DENG_ASSERT_IN_MAIN_THREAD();
            DENG_ASSERT_GL_CONTEXT_ACTIVE();

            glBindTexture(GL_TEXTURE_2D, r->tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, newMinFilter);
        }
    }
    Z_Free(rawTexs);
}

void GL_ReleaseTexturesForRawImages()
{
    rawtex_t **rawTexs = R_CollectRawTexs();
    for(rawtex_t **ptr = rawTexs; *ptr; ptr++)
    {
        rawtex_t *r = (*ptr);
        if(r->tex)
        {
            glDeleteTextures(1, (GLuint const *) &r->tex);
            r->tex = 0;
        }
    }
    Z_Free(rawTexs);
}
