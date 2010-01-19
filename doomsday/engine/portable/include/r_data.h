/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * r_data.h: Data Structures For Refresh
 */

#ifndef __DOOMSDAY_REFRESH_DATA_H__
#define __DOOMSDAY_REFRESH_DATA_H__

#include "dd_def.h"
#include "gl_main.h"
#include "p_think.h"
#include "m_nodepile.h"
#include "def_data.h"
#include "r_extres.h"
#include "gl_texmanager.h"
#include "p_maptypes.h"

// Flags for material decorations.
#define DCRF_NO_IWAD        0x1 // Don't use if from IWAD.
#define DCRF_PWAD           0x2 // Can use if from PWAD.
#define DCRF_EXTERNAL       0x4 // Can use if from external resource.

// Flags for material reflections.
#define REFF_NO_IWAD        0x1 // Don't use if from IWAD.
#define REFF_PWAD           0x2 // Can use if from PWAD.
#define REFF_EXTERNAL       0x4 // Can use if from external resource.

// Flags for material detailtexs.
#define DTLF_NO_IWAD        0x1 // Don't use if from IWAD.
#define DTLF_PWAD           0x2 // Can use if from PWAD.
#define DTLF_EXTERNAL       0x4 // Can use if from external resource.

typedef struct detailtex_s {
    gltextureid_t   id;
    lumpnum_t       lump;
    const char*     external;
} detailtex_t;

typedef struct lightmap_s {
    gltextureid_t   id;
    const char*     external;
} lightmap_t;

typedef struct flaretex_s {
    gltextureid_t   id;
    const char*     external;
} flaretex_t;

typedef struct shinytex_s {
    gltextureid_t   id;
    const char*     external;
} shinytex_t;

typedef struct masktex_s {
    gltextureid_t   id;
    const char*     external;
    short           width, height;
} masktex_t;

typedef enum {
    TU_PRIMARY = 0,
    TU_PRIMARY_DETAIL,
    TU_INTER,
    TU_INTER_DETAIL,
    NUM_TEXMAP_UNITS
} gltexunit_t;

typedef struct glcommand_vertex_s {
    float           s, t;
    int             index;
} glcommand_vertex_t;

typedef struct rvertex_s {
    float           pos[3];
} rvertex_t;

typedef struct rcolor_s {
    float           rgba[4];
} rcolor_t;

typedef struct rtexcoord_s {
    float           st[2];
} rtexcoord_t;

typedef struct rtexmapunit_s {
    DGLuint         tex;
    int             magMode;
    float           blend;
    float           scale[2], offset[2]; // For use with the texture matrix.
    blendmode_t     blendMode;
} rtexmapunit_t;

typedef float       colorcomp_t;
typedef colorcomp_t rgbcol_t[3];
typedef colorcomp_t rgbacol_t[4];

typedef struct {
    lumpnum_t       lump;
    short           offX; // block origin (allways UL), which has allready
    short           offY; // accounted for the patch's internal origin
} texpatch_t;

#define TXDF_NODRAW         0x0001 // Not to be drawn.
#define TXDF_IWAD           0x0002 // Defines an IWAD texture. Note the definition may NOT be from the IWAD.

// Describes a rectangular texture, which is composed of one
// or more texpatch_t structures that arrange graphic patches.
typedef struct {
    char            name[9];
    short           width, height;
    short           flags;
    short           patchCount;
    texpatch_t      patches[1]; // [patchcount] drawn back to front into the cached texture.
} doomtexturedef_t;

typedef struct flat_s {
    lumpnum_t       lump;
    short           width, height;
} flat_t;

typedef struct {
    lumpnum_t       lump; // Real lump number.
    short           width, height, offX, offY;
} spritetex_t;

// Model skin.
typedef struct {
    filename_t      path;
    gltextureid_t   id;
} skinname_t;

// Patch flags.
#define PF_MONOCHROME         0x1
#define PF_UPSCALE_AND_SHARPEN 0x2

// A patchtex is a lumppatch that has been prepared for render.
typedef struct patchtex_s {
    lumpnum_t       lump;
    short           offX, offY;
    short           extraOffset[2]; // Only used with upscaled and sharpened patches.
    int             flags; // Possible modifier filters to apply (monochrome, scale+sharp)

    // Part 1
    DGLuint         tex; // Name of the associated DGL texture.
    short           width, height;

    // Part 2 (only used with textures larger than the max texture size).
    DGLuint         tex2;
    short           width2, height2;

    struct patchtex_s* next;
} patchtex_t;

// A rawtex is a lump raw graphic that has been prepared for render.
typedef struct rawtex_s {
    lumpnum_t       lump;

    // Part 1
    DGLuint         tex; // Name of the associated DGL texture.
    short           width, height;
    byte            masked;

    // Part 2 (only used with textures larger than the max texture size).
    DGLuint         tex2;
    short           width2, height2;
    byte            masked2;

    struct rawtex_s* next;
} rawtex_t;

/**
 * Textures used in the lighting system.
 */
typedef enum lightingtexid_e {
    LST_DYNAMIC, // Round dynamic light
    LST_GRADIENT, // Top-down gradient
    LST_RADIO_CO, // FakeRadio closed/open corner shadow
    LST_RADIO_CC, // FakeRadio closed/closed corner shadow
    LST_RADIO_OO, // FakeRadio open/open shadow
    LST_RADIO_OE, // FakeRadio open/edge shadow
    NUM_LIGHTING_TEXTURES
} lightingtexid_t;

typedef enum flaretexid_e {
    FXT_ROUND,
    FXT_FLARE, // (flare)
    FXT_BRFLARE, // (brFlare)
    FXT_BIGFLARE, // (bigFlare)
    NUM_SYSFLARE_TEXTURES
} flaretexid_t;

/**
 * Textures used in world rendering.
 * eg a surface with a missing tex/flat is drawn using the "missing" graphic
 */
typedef enum ddtextureid_e {
    DDT_UNKNOWN, // Drawn if a texture/flat is unknown
    DDT_MISSING, // Drawn in place of HOMs in dev mode.
    DDT_BBOX, // Drawn when rendering bounding boxes
    DDT_GRAY, // For lighting debug.
    NUM_DD_TEXTURES
} ddtextureid_t;

typedef struct {
    DGLuint         tex;
} ddtexture_t;

typedef struct {
    float           approxDist; // Only an approximation.
    float           vector[3]; // Light direction vector.
    float           color[3]; // How intense the light is (0..1, RGB).
    float           offset;
    float           lightSide, darkSide; // Factors for world light.
    boolean         affectedByAmbient;
} vlight_t;

#define RL_MAX_DIVS         64
typedef struct walldiv_s {
    unsigned int    num;
    plane_t*        divs[RL_MAX_DIVS];
} walldiv_t;

extern int viewwidth, viewheight;
extern int mapFullBright;
extern int glowingTextures;
extern byte precacheSprites, precacheSkins;

extern int numFlats;
extern flat_t** flats;

extern spritetex_t** spriteTextures;
extern int numSpriteTextures;

extern detailtex_t** detailTextures;
extern int numDetailTextures;

extern lightmap_t** lightMaps;
extern int numLightMaps;

extern flaretex_t** flareTextures;
extern int numFlareTextures;

extern shinytex_t** shinyTextures;
extern int numShinyTextures;

extern masktex_t** maskTextures;
extern int numMaskTextures;

extern uint numSkinNames;
extern skinname_t* skinNames;

rvertex_t*      R_AllocRendVertices(uint num);
rcolor_t*       R_AllocRendColors(uint num);
rtexcoord_t*    R_AllocRendTexCoords(uint num);
void            R_FreeRendVertices(rvertex_t* rvertices);
void            R_FreeRendColors(rcolor_t* rcolors);
void            R_FreeRendTexCoords(rtexcoord_t* rtexcoords);

rvertex_t*      R_VerticesFromRendSeg(struct rendseg_s* rseg, uint* size);
void            R_TexmapUnitsFromRendSeg(struct rendseg_s* rseg, rtexmapunit_t rTU[NUM_TEXMAP_UNITS],
                                         rtexmapunit_t rTUs[NUM_TEXMAP_UNITS],
                                         rtexmapunit_t radioTU[NUM_TEXMAP_UNITS],
                                         rtexmapunit_t radioTU2[NUM_TEXMAP_UNITS],
                                         rtexmapunit_t radioTU3[NUM_TEXMAP_UNITS],
                                         rtexmapunit_t radioTU4[NUM_TEXMAP_UNITS]);

rvertex_t*      R_VerticesFromRendPlane(struct rendplane_s* rplane, const struct face_s* face,
                                        float height, boolean antiClockwise, uint* size);

void Rend_SetupRTU(rtexmapunit_t rTU[NUM_TEXMAP_UNITS],
                   rtexmapunit_t rTUs[NUM_TEXMAP_UNITS],
                   const struct material_snapshot_s* msA, float inter,
                   const struct material_snapshot_s* msB);
void Rend_SetupRTU2(rtexmapunit_t rTU[NUM_TEXMAP_UNITS],
                    rtexmapunit_t rTUs[NUM_TEXMAP_UNITS],
                      boolean isWall, const float texOffset[2],
                      const float texScale[2],
                      const struct material_snapshot_s* msA,
                      const struct material_snapshot_s* msB);

void            R_ColorApplyTorchLight(float* color, float distance);

void            R_VertexColorsGlow(rcolor_t* colors, size_t num);
void            R_VertexColorsAlpha(rcolor_t* colors, size_t num, float alpha);
void            R_VertexColorsApplyTorchLight(rcolor_t* colors, const rvertex_t* vertices,
                                              size_t numVertices);
void            R_VertexColorsApplyAmbientLight(rcolor_t* color, const rvertex_t* vtx,
                                                float lightLevel, const float* ambientColor);



void            R_DivVerts(rvertex_t* dst, const rvertex_t* src,
                           const walldiv_t* divs);
void            R_DivVertColors(rcolor_t* dst, const rcolor_t* src,
                                const walldiv_t* divs, float bL, float tL,
                                float bR, float tR);
void            R_DivTexCoords(rtexcoord_t* dst, const rtexcoord_t* src,
                               const walldiv_t* divs, float bL, float tL,
                               float bR, float tR);

void            R_UpdateTexturesAndFlats(void);
void            R_InitTextures(void);
void            R_InitFlats(void);

void            R_InitData(void);
void            R_UpdateData(void);
void            R_ShutdownData(void);

colorpaletteid_t R_CreateColorPalette(const char* fmt, const char* name,
                                      const byte* data, ushort num);
const char*     R_GetColorPaletteNameForNum(colorpaletteid_t id);
colorpaletteid_t R_GetColorPaletteNumForName(const char* name);

DGLuint         R_GetColorPalette(colorpaletteid_t id);
void            R_GetColorPaletteRGBf(colorpaletteid_t id, float rgb[3],
                                      int idx, boolean correctGamma);
boolean         R_SetDefaultColorPalette(colorpaletteid_t id);

boolean         R_UpdateSector(struct sector_s* sec, boolean forceUpdate);
boolean         R_UpdateLineDef(struct linedef_s* line, boolean forceUpdate);
boolean         R_UpdateSideDef(struct sidedef_s* side, boolean forceUpdate);
boolean         R_UpdatePlane(struct plane_s* pln, boolean forceUpdate);
boolean         R_UpdateSurface(struct surface_s* suf, boolean forceUpdate);

void            R_PrecacheMobjNum(int mobjtypeNum);
void            R_PrecachePatch(lumpnum_t lump);

doomtexturedef_t* R_GetDoomTextureDef(int num);

uint            R_GetSkinNumForName(const char* path);
const skinname_t* R_GetSkinNameByIndex(uint id);
uint            R_RegisterSkin(char* fullpath, const char* skin,
                               const char* modelfn, boolean isShinySkin,
                               size_t len);
void            R_DestroySkins(void); // Called at shutdown.

detailtex_t*    R_CreateDetailTexture(const ded_detailtexture_t* def);
detailtex_t*    R_GetDetailTexture(lumpnum_t lump, const char* external);
void            R_DestroyDetailTextures(void); // Called at shutdown.

lightmap_t*     R_CreateLightMap(const ded_lightmap_t* def);
lightmap_t*     R_GetLightMap(const char* external);
void            R_DestroyLightMaps(void); // Called at shutdown.

flaretex_t*     R_CreateFlareTexture(const ded_flaremap_t* def);
flaretex_t*     R_GetFlareTexture(const char* external);
void            R_DestroyFlareTextures(void); // Called at shutdown.

shinytex_t*     R_CreateShinyTexture(const ded_reflection_t* def);
shinytex_t*     R_GetShinyTexture(const char* external);
void            R_DestroyShinyTextures(void); // Called at shutdown.

masktex_t*      R_CreateMaskTexture(const ded_reflection_t* def);
masktex_t*      R_GetMaskTexture(const char* external);
void            R_DestroyMaskTextures(void); // Called at shutdown.

patchtex_t*     R_FindPatchTex(lumpnum_t lump); // May return NULL.
patchtex_t*     R_GetPatchTex(lumpnum_t lump); // Creates new entries.
patchtex_t**    R_CollectPatchTexs(int* count);

rawtex_t*       R_FindRawTex(lumpnum_t lump); // May return NULL.
rawtex_t*       R_GetRawTex(lumpnum_t lump); // Creates new entries.
rawtex_t**      R_CollectRawTexs(int* count);

byte            GL_LoadDoomPatch(image_t* image, const patchtex_t* p);
byte            GL_LoadRawTex(image_t* image, const rawtex_t* r);

DGLuint         GL_PreparePatch(patchtex_t* patch);
DGLuint         GL_PreparePatchOtherPart(patchtex_t* patch);
DGLuint         GL_PrepareRawTex(rawtex_t* rawTex);
DGLuint         GL_PrepareRawTexOtherPart(rawtex_t* rawTex);

DGLuint         GL_PrepareLSTexture(lightingtexid_t which);
DGLuint         GL_PrepareSysFlareTexture(flaretexid_t flare);

boolean         R_IsAllowedDecoration(ded_decor_t* def, material_t* mat,
                                      boolean hasExternal);
boolean         R_IsAllowedReflection(ded_reflection_t* def, material_t* mat,
                                      boolean hasExternal);
boolean         R_IsAllowedDetailTex(ded_detailtexture_t* def, material_t* mat,
                                     boolean hasExternal);
boolean         R_IsValidLightDecoration(const ded_decorlight_t* lightDef);

//@todo Move out of the engine and into a plugin.
int             R_TextureIdForName(material_namespace_t mnamespace, const char* rawName);
material_t*     R_MaterialForTextureId(material_namespace_t mnamespace, int idx);
#endif
