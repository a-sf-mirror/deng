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
 * def_data.h: Doomsday Engine Definition Files
 *
 * \fixme Needs to be redesigned.
 */

#ifndef __DOOMSDAY_DED_FILES_H__
#define __DOOMSDAY_DED_FILES_H__

#ifdef __cplusplus
extern          "C" {
#endif

#include "def_share.h"
#include "p_mapdata.h"

    // Version 6 does not require semicolons.
#define DED_VERSION         6

#define DED_SPRITEID_LEN    4
#define DED_STRINGID_LEN    31
#define DED_PATH_LEN        FILENAME_T_MAXLEN
#define DED_FUNC_LEN        255

#define DED_MAX_SUB_MODELS  8

#define DED_MAX_MATERIAL_LAYERS 1 //// \temp

typedef char ded_stringid_t[DED_STRINGID_LEN + 1];
typedef ded_stringid_t ded_string_t;
typedef ded_stringid_t ded_mobjid_t;
typedef ded_stringid_t ded_stateid_t;
typedef ded_stringid_t ded_soundid_t;
typedef ded_stringid_t ded_musicid_t;
typedef ded_stringid_t ded_funcid_t;
typedef char ded_func_t[DED_FUNC_LEN + 1];
typedef int ded_flags_t;
typedef char* ded_anystring_t;

typedef struct ded_count_s {
    int             num, max;
} ded_count_t;

typedef struct {
    char            path[DED_PATH_LEN + 1];
} ded_path_t;

typedef struct {
    char            id[DED_SPRITEID_LEN + 1];
} ded_sprid_t;

typedef struct {
    char            str[DED_STRINGID_LEN + 1];
} ded_str_t;

typedef struct {
    ded_string_t    name;
    material_namespace_t mnamespace;
} ded_materialid_t;

typedef struct {
    ded_flags_t     id; // ID of this property
    ded_flags_t     flags;
    ded_string_t    name;
    ded_string_t    flagPrefix;
    int             map;
} ded_xgclass_property_t;

typedef struct {
    ded_stringid_t  id;
    ded_string_t    name;
    ded_count_t     propertiesCount;
    ded_xgclass_property_t* properties;
    // The rest of the properties are retrieved at runtime
    // by querying the game for the callbacks for the
    // classes by "name";
    int (C_DECL *doFunc)();
    void (*initFunc)(linedef_t* line);
    int             traverse;
    int             travRef;
    int             travData;
    int             evTypeFlags;
} ded_xgclass_t;

typedef struct {
    ded_stringid_t  id;
    ded_string_t    text;
    int             value;
} ded_flag_t;

typedef struct {
    ded_mobjid_t    id;
    int             doomedNum;
    ded_string_t    name;

    ded_stateid_t   states[NUM_STATE_NAMES];

    ded_soundid_t   seeSound;
    ded_soundid_t   attackSound;
    ded_soundid_t   painSound;
    ded_soundid_t   deathSound;
    ded_soundid_t   activeSound;

    int             reactionTime;
    int             painChance;
    int             spawnHealth;
    float           speed;
    float           radius;
    float           height;
    int             mass;
    int             damage;
    ded_flags_t     flags[NUM_MOBJ_FLAGS];
    int             misc[NUM_MOBJ_MISC];
} ded_mobj_t;

typedef struct {
    ded_stateid_t   id; // ID of this state.
    ded_sprid_t     sprite;
    ded_flags_t     flags;
    int             frame;
    int             tics;
    ded_funcid_t    action;
    ded_stateid_t   nextState;
    int             misc[NUM_STATE_MISC];
    ded_anystring_t execute; // Console command.
} ded_state_t;

typedef struct ded_lightmap_s {
    ded_stringid_t  id;
} ded_lightmap_t;

typedef struct ded_flaremap_s {
    ded_stringid_t  id;
} ded_flaremap_t;

typedef struct {
    ded_stateid_t   state;
    char            uniqueMapID[64];
    float           offset[3]; /* Origin offset in world coords
                                  Zero means automatic */
    float           size; // Zero: automatic
    float           color[3]; // Red Green Blue (0,1)
    float           lightLevel[2]; // Min/max lightlevel for bias
    ded_flags_t     flags;
    ded_lightmap_t  up, down, sides;
    ded_flaremap_t  flare;
    float           haloRadius; // Halo radius (zero = no halo).
} ded_light_t;

typedef struct {
    ded_path_t      filename;
    ded_path_t      skinFilename; // Optional; override model's skin.
    ded_string_t    frame;
    int             frameRange;
    ded_flags_t     flags; // ASCII string of the flags.
    int             skin;
    int             skinRange;
    float           offset[3]; // Offset XYZ within model.
    float           alpha;
    float           parm; // Custom parameter.
    unsigned char   selSkinBits[2]; // Mask and offset.
    unsigned char   selSkins[8];
    ded_string_t    shinySkin;
    float           shiny;
    float           shinyColor[3];
    float           shinyReact;
    blendmode_t     blendMode; // bm_*
} ded_submodel_t;

typedef struct {
    ded_stringid_t  id; // Optional identifier for the definition.
    ded_stateid_t   state;
    int             off;
    ded_sprid_t     sprite; // Only used by autoscale.
    int             spriteFrame; // Only used by autoscale.
    ded_flags_t     group;
    int             selector;
    ded_flags_t     flags;
    float           interMark;
    float           interRange[2]; // 0-1 by default.
    int             skinTics; // Tics per skin in range.
    float           scale[3]; // Scale XYZ
    float           resize;
    float           offset[3]; // Offset XYZ
    float           shadowRadius; // Radius for shadow (0=auto).
    ded_submodel_t  sub[DED_MAX_SUB_MODELS];
} ded_model_t;

typedef struct {
    ded_soundid_t   id; // ID of this sound, refered to by others.
    ded_string_t    lumpName; /* Actual lump name of the sound
                                 ("DS" not included). */
    ded_string_t    name; // A tag name for the sound.
    ded_soundid_t   link; // Link to another sound.
    int             linkPitch;
    int             linkVolume;
    int             priority; // Priority classification.
    int             channels; // Max number of channels to occupy.
    int             group; // Exclusion group.
    ded_flags_t     flags; // Flags (like chg_pitch).
    ded_path_t      ext; // External sound file (WAV).
} ded_sound_t;

typedef struct {
    ded_musicid_t   id; // ID of this piece of music.
    ded_string_t    lumpName; // Lump name.
    ded_path_t      path; // External file (not a normal MUS file).
    int             cdTrack; // 0 = no track.
} ded_music_t;

typedef struct {
    ded_flags_t     flags;
    ded_materialid_t material;
    float           offset;
    float           colorLimit;
} ded_skylayer_t;

typedef struct ded_skymodel_s {
    ded_stringid_t  id;
    int             layer; // Defaults to -1.
    float           frameInterval; // Seconds per frame.
    float           yaw;
    float           yawSpeed; // Angles per second.
    float           coordFactor[3];
    float           rotate[2];
    ded_anystring_t execute; // Executed on every frame change.
    float           color[4]; // RGBA
} ded_skymodel_t;

#define NUM_SKY_LAYERS      2
#define NUM_SKY_MODELS      32

typedef struct ded_sky_s {
    ded_stringid_t  id;
    ded_flags_t     flags; // Flags.
    float           height;
    float           horizonOffset;
    float           color[3]; // Color of sky-lit sectors.
    ded_skylayer_t  layers[NUM_SKY_LAYERS];
    ded_skymodel_t  models[NUM_SKY_MODELS];
} ded_sky_t;

typedef struct ded_mapinfo_s {
    ded_stringid_t  id; // ID of the map (e.g. E2M3 or MAP21).
    char            name[64]; // Name of the map.
    ded_string_t    author; // Author of the map.
    ded_flags_t     flags; // Flags.
    ded_musicid_t   music; // Music to play.
    float           parTime; // Par time, in seconds.
    float           fogColor[3]; // Fog color (RGB).
    float           fogStart;
    float           fogEnd;
    float           fogDensity;
    float           ambient; // Ambient light level.
    float           gravity; // 1 = normal.
    ded_stringid_t  skyID; // ID of the sky definition to use with this map. If not set, use the sky data in the mapinfo.
    ded_sky_t       sky;
    ded_anystring_t execute; // Executed during map setup (savegames, too).
} ded_mapinfo_t;

typedef struct {
    ded_stringid_t  id;
    char*           text;
} ded_text_t;

typedef struct {
    ded_stringid_t  id;
    ded_count_t     count;
    ded_materialid_t* materials;
} ded_tenviron_t;

typedef struct {
    char*           id;
    char*           text;
} ded_value_t;

typedef struct {
    ded_stringid_t  id;
    ded_stringid_t  before;
    ded_stringid_t  after;
    char*           script;
} ded_finale_t;

typedef struct {
    int             id;
    char            comment[64];
    ded_flags_t     flags[3];
    ded_flags_t     lineClass;
    ded_flags_t     actType;
    int             actCount;
    float           actTime;
    int             actTag;
    int             aparm[9];
    //ded_stringid_t    aparm_str[3];   // aparms 4, 6, 9
    ded_stringid_t  aparm9;
    float           tickerStart;
    float           tickerEnd;
    int             tickerInterval;
    ded_soundid_t   actSound;
    ded_soundid_t   deactSound;
    int             evChain;
    int             actChain;
    int             deactChain;
    int             actLineType;
    int             deactLineType;
    ded_flags_t     wallSection;
    ded_materialid_t actMaterial;
    ded_materialid_t deactMaterial;
    char            actMsg[128];
    char            deactMsg[128];
    float           materialMoveAngle;
    float           materialMoveSpeed;
    int             iparm[20];
    char            iparmStr[20][64];
    float           fparm[20];
    char            sparm[5][128];
} ded_linetype_t;

typedef struct {
    int             id;
    char            comment[64];
    ded_flags_t     flags;
    int             actTag;
    int             chain[5];
    ded_flags_t     chainFlags[5];
    float           start[5];
    float           end[5];
    float           interval[5][2];
    int             count[5];
    ded_soundid_t   ambientSound;
    float           soundInterval[2]; // min,max
    float           materialMoveAngle[2]; // floor, ceil
    float           materialMoveSpeed[2]; // floor, ceil
    float           windAngle;
    float           windSpeed;
    float           verticalWind;
    float           gravity;
    float           friction;
    ded_func_t      lightFunc;
    int             lightInterval[2];
    ded_func_t      colFunc[3]; // RGB
    int             colInterval[3][2];
    ded_func_t      floorFunc;
    float           floorMul, floorOff;
    int             floorInterval[2];
    ded_func_t      ceilFunc;
    float           ceilMul, ceilOff;
    int             ceilInterval[2];
} ded_sectortype_t;

typedef struct ded_detailtexture_s {
    ded_materialid_t material1;
    ded_materialid_t material2;
    ded_flags_t     flags;
    ded_path_t      detailLump; // The lump with the detail texture.
    boolean         isExternal; // True, if detailLump is external.
    float           scale;
    float           strength;
    float           maxDist;
} ded_detailtexture_t;

// Embedded sound information.
typedef struct ded_embsound_s {
    ded_string_t    name;
    int             id; // Figured out at runtime.
    float           volume;
} ded_embsound_t;

typedef struct {
    ded_flags_t     type;
    int             tics;
    float           variance; // Stage variance (time).
    float           color[4]; // RGBA
    float           radius;
    float           radiusVariance;
    ded_flags_t     flags;
    float           bounce;
    float           resistance; // Air resistance.
    float           gravity;
    float           vectorForce[3];
    float           spin[2]; // Yaw and pitch.
    float           spinResistance[2]; // Yaw and pitch.
    int             model;
    ded_string_t    frameName; // For model particles.
    ded_string_t    endFrameName;
    short           frame, endFrame;
    ded_embsound_t  sound, hitSound;
} ded_ptcstage_t;

typedef struct ded_ptcgen_s {
    struct ded_ptcgen_s* stateNext; // List of generators for a state.
    ded_stateid_t   state; // Triggered by this state (if mobj-gen).
    ded_materialid_t material;
    ded_mobjid_t    type; // Triggered by this type of mobjs.
    ded_mobjid_t    type2; // Also triggered by this type.
    int             typeNum;
    int             type2Num;
    ded_mobjid_t    damage; // Triggered by mobj damage of this type.
    int             damageNum;
    ded_string_t    map; // Triggered by this map (or empty string).
    ded_flags_t     flags;
    float           speed; // Particle spawn velocity.
    float           speedVariance; // Spawn speed variance (0-1).
    float           vector[3]; // Particle launch vector.
    float           vectorVariance; // Launch vector variance (0-1). 1=totally random.
    float           initVectorVariance; // Initial launch vector variance (0-1).
    float           center[3]; // Offset to the mobj (relat. to source).
    int             subModel; // Model source: origin submodel #.
    float           spawnRadius;
    float           spawnRadiusMin; // Spawn uncertainty box.
    float           maxDist; // Max visibility for particles.
    int             spawnAge; // How long until spawning stops?
    int             maxAge; // How long until generator dies?
    int             particles; // Maximum number of particles.
    float           spawnRate; // Particles spawned per tic.
    float           spawnRateVariance;
    int             preSim; // Tics to pre-simulate when spawned.
    int             altStart;
    float           altStartVariance; // Probability for alt start.
    float           force; // Radial strength of the sphere force.
    float           forceRadius; // Radius of the sphere force.
    float           forceAxis[3]; /* Rotation axis of the sphere force
                                      (+ speed). */
    float           forceOrigin[3]; // Offset for the force sphere.
    ded_ptcstage_t* stages;
    ded_count_t     stageCount;
} ded_ptcgen_t;

typedef struct ded_decorlight_s {
    float           pos[2]; // Coordinates on the surface.
    float           elevation; // Distance from the surface.
    float           color[3]; // Light color.
    float           radius; // Dynamic light radius (-1 = no light).
    float           haloRadius; // Halo radius (zero = no halo).
    int             patternOffset[2];
    int             patternSkip[2];
    float           lightLevels[2]; // Fade by sector lightlevel.
    int             flareTexture;
    ded_lightmap_t  up, down, sides;
    ded_flaremap_t  flare; // Overrides flare_texture
} ded_decorlight_t;

// There is a fixed number of light decorations in each decoration.
#define DED_DECOR_NUM_LIGHTS    16

typedef struct ded_decormodel_s {
    float           pos[2]; // Coordinates on the surface.
    float           elevation; // Distance from the surface.
    int             patternOffset[2];
    int             patternSkip[2];
    float           lightLevels[2]; // Fade by sector lightlevel.
    ded_stringid_t  id;
    float           frameInterval; // Seconds per frame.
} ded_decormodel_t;

// There is a fixed number of model decorations in each decoration.
#define DED_DECOR_NUM_MODELS    8

typedef struct ded_decor_s {
    ded_materialid_t material;
    ded_flags_t     flags;
    ded_decorlight_t lights[DED_DECOR_NUM_LIGHTS];
    ded_decormodel_t models[DED_DECOR_NUM_MODELS];
} ded_decor_t;

typedef struct ded_reflection_s {
    ded_materialid_t material;
    ded_flags_t     flags;
    blendmode_t     blendMode; // Blend mode flags (bm_*).
    float           shininess;
    float           minColor[3];
    ded_path_t      shinyMap;
    ded_path_t      maskMap;
    float           maskWidth;
    float           maskHeight;
} ded_reflection_t;

typedef struct ded_group_member_s {
    ded_materialid_t material;
    float           tics;
    float           randomTics;
} ded_group_member_t;

typedef struct ded_group_s {
    ded_flags_t     flags;
    ded_count_t     count;
    ded_group_member_t* members;
} ded_group_t;

typedef struct ded_material_layer_stage_s {
    ded_string_t    name; // Material tex name.
    int             type; // Material tex type, @see gltexture_type_t.
    int             tics;
    float           variance; // Stage variance (time).
} ded_material_layer_stage_t;

typedef struct ded_material_layer_s {
    ded_material_layer_stage_t* stages;
    ded_count_t     stageCount;
} ded_material_layer_t;

typedef struct ded_material_s {
    ded_materialid_t id;
    ded_flags_t     flags;
    float           width, height; // In world units.
    ded_material_layer_t layers[DED_MAX_MATERIAL_LAYERS];
} ded_material_t;

// The ded_t structure encapsulates all the data one definition file
// can contain. This is only an internal representation of the data.
// An ASCII version is written and read by DED_Write() and DED_Read()
// routines.

// It is VERY important not to sort the data arrays in any way: the
// index numbers are important. The Game DLL must be recompiled with the
// new constants if the order of the array items changes.

typedef struct ded_s {
    int             version; // DED version number.
    filename_t      modelPath; // Directories for searching MD2s.
    ded_flags_t     modelFlags; // Default values for models.
    float           modelScale;
    float           modelOffset;

    struct ded_counts_s {
        ded_count_t     flags;
        ded_count_t     mobjs;
        ded_count_t     states;
        ded_count_t     sprites;
        ded_count_t     lights;
        ded_count_t     materials;
        ded_count_t     models;
        ded_count_t     skies;
        ded_count_t     sounds;
        ded_count_t     music;
        ded_count_t     mapInfo;
        ded_count_t     text;
        ded_count_t     textureEnv;
        ded_count_t     values;
        ded_count_t     details;
        ded_count_t     ptcGens;
        ded_count_t     finales;
        ded_count_t     decorations;
        ded_count_t     reflections;
        ded_count_t     groups;
        ded_count_t     lineTypes;
        ded_count_t     sectorTypes;
        ded_count_t     xgClasses;
    } count;

    // Flag values (for all types of data).
    ded_flag_t*     flags;

    // Map object information.
    ded_mobj_t*     mobjs;

    // States.
    ded_state_t*    states;

    // Sprites.
    ded_sprid_t*    sprites;

    // Lights.
    ded_light_t*    lights;

    // Materials.
    ded_material_t* materials;

    // Models.
    ded_model_t*    models;

    // Skies.
    ded_sky_t*      skies;

    // Sounds.
    ded_sound_t*    sounds;

    // Music.
    ded_music_t*    music;

    // Map information.
    ded_mapinfo_t*  mapInfo;

    // Text.
    ded_text_t*     text;

    // Aural environments for textures.
    ded_tenviron_t* textureEnv;

    // Free-from string values.
    ded_value_t*    values;

    // Detail texture assignments.
    ded_detailtexture_t* details;

    // Particle generators.
    ded_ptcgen_t*   ptcGens;

    // Finales.
    ded_finale_t*   finales;

    // Decorations.
    ded_decor_t*    decorations;

    // Reflections.
    ded_reflection_t* reflections;

    // Animation/Precache groups for textures.
    ded_group_t*    groups;

    // XG line types.
    ded_linetype_t* lineTypes;

    // XG sector types.
    ded_sectortype_t* sectorTypes;

    // XG Classes
    ded_xgclass_t*  xgClasses;
} ded_t;

// Routines for managing DED files.
void            DED_Init(ded_t* ded);
void            DED_Destroy(ded_t* ded);
int             DED_Read(ded_t* ded, const char* sPathName);
int             DED_ReadLump(ded_t* ded, lumpnum_t lump);

int             DED_AddFlag(ded_t* ded, char* name, char* text, int value);
int             DED_AddMobj(ded_t* ded, char* idStr);
int             DED_AddState(ded_t* ded, char* id);
int             DED_AddSprite(ded_t* ded, const char* name);
int             DED_AddLight(ded_t* ded, const char* stateID);
int             DED_AddMaterial(ded_t* ded, const char* name);
int             DED_AddMaterialLayerStage(ded_material_layer_t* ml);
int             DED_AddModel(ded_t* ded, char* spr);
int             DED_AddSky(ded_t* ded, char* id);
int             DED_AddSound(ded_t* ded, char* id);
int             DED_AddMusic(ded_t* ded, char* id);
int             DED_AddMapInfo(ded_t* ded, char* str);
int             DED_AddText(ded_t* ded, char* id);
int             DED_AddTextureEnv(ded_t* ded, char* id);
int             DED_AddDetail(ded_t* ded, const char* lumpname);
int             DED_AddPtcGen(ded_t* ded, const char* state);
int             DED_AddPtcGenStage(ded_ptcgen_t* gen);
int             DED_AddFinale(ded_t* ded);
int             DED_AddDecoration(ded_t* ded);
int             DED_AddReflection(ded_t* ded);
int             DED_AddGroup(ded_t* ded);
int             DED_AddGroupMember(ded_group_t* grp);
int             DED_AddSectorType(ded_t* ded, int id);
int             DED_AddLineType(ded_t* ded, int id);
int             DED_AddXGClass(ded_t* ded);
int             DED_AddXGClassProperty(ded_xgclass_t* xgc);

void            DED_RemoveFlag(ded_t* ded, int index);
void            DED_RemoveMobj(ded_t* ded, int index);
void            DED_RemoveState(ded_t* ded, int index);
void            DED_RemoveSprite(ded_t* ded, int index);
void            DED_RemoveLight(ded_t* ded, int index);
void            DED_RemoveMaterial(ded_t* ded, int index);
void            DED_RemoveModel(ded_t* ded, int index);
void            DED_RemoveSky(ded_t* ded, int index);
void            DED_RemoveSound(ded_t* ded, int index);
void            DED_RemoveMusic(ded_t* ded, int index);
void            DED_RemoveMapInfo(ded_t* ded, int index);
void            DED_RemoveText(ded_t* ded, int index);
void            DED_RemoveTextureEnv(ded_t* ded, int index);
void            DED_RemoveValue(ded_t* ded, int index);
void            DED_RemoveDetail(ded_t* ded, int index);
void            DED_RemovePtcGen(ded_t* ded, int index);
void            DED_RemoveFinale(ded_t* ded, int index);
void            DED_RemoveDecoration(ded_t* ded, int index);
void            DED_RemoveReflection(ded_t* ded, int index);
void            DED_RemoveGroup(ded_t* ded, int index);
void            DED_RemoveSectorType(ded_t* ded, int index);
void            DED_RemoveLineType(ded_t* ded, int index);
void            DED_RemoveXGClass(ded_t* ded, int index);

void*           DED_NewEntry(void** ptr, ded_count_t* cnt, size_t elemSize);
void            DED_DelEntry(int index, void** ptr, ded_count_t* cnt,
                             size_t elemSize);
void            DED_DelArray(void** ptr, ded_count_t* cnt);
void            DED_ZCount(ded_count_t* c);

extern char dedReadError[];

#ifdef __cplusplus
}
#endif
#endif
