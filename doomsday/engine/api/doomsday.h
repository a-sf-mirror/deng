/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * doomsday.h: Doomsday Engine API (Routines exported from Doomsday.exe).
 *
 * Games and plugins need to include this to gain access to the engine's
 * features.
 */

#ifndef DOOMSDAY_EXPORTS_H
#define DOOMSDAY_EXPORTS_H

// The calling convention.
#if defined(WIN32)
#   define _DECALL  __cdecl
#elif defined(UNIX)
#   define _DECALL
#endif

#ifdef __cplusplus
extern          "C" {
#endif

/**
 * Public definitions of the internal map data pointers.  These can be
 * accessed externally, but only as identifiers to data instances.
 * For example, a game could use sector_t to identify to sector to
 * change with the Map Update API.
 */
typedef struct node_s { int type; } node_t;
typedef struct vertex_s {int type; } vertex_t;
typedef struct linedef_s { int type; } linedef_t;
typedef struct sidedef_s { int type; } sidedef_t;
typedef struct seg_s { int type; } seg_t;
typedef struct subsector_s { int type; } subsector_t;
typedef struct sector_s { int type; } sector_t;
typedef struct plane_s { int type; } plane_t;
typedef struct material_s { int type; } material_t;

#include "dd_share.h"
#include "dd_plugin.h"

    // Base.
    void            DD_AddIWAD(const char* path);
    void            DD_AddStartupWAD(const char* file);
    void            DD_SetConfigFile(const char* filename);
    void            DD_SetDefsFile(const char* file);
    int _DECALL     DD_GetInteger(int ddvalue);
    void            DD_SetInteger(int ddvalue, int parm);
    void            DD_SetVariable(int ddvalue, void* ptr);
    void*           DD_GetVariable(int ddvalue);
    ddplayer_t*     DD_GetPlayer(int number);

    // Base: Definitions.
    int             Def_Get(int type, const char* id, void* out);
    int             Def_Set(int type, int index, int value, const void* ptr);
    int             Def_EvalFlags(char* flags);

    // Base: Input.
    void            DD_ClearKeyRepeaters(void);
    int             DD_GetKeyCode(const char* name);

    // Base: WAD.
    lumpnum_t       W_CheckNumForName(const char* name);
    lumpnum_t       W_GetNumForName(const char* name);
    size_t          W_LumpLength(lumpnum_t lump);
    const char*     W_LumpName(lumpnum_t lump);
    void            W_ReadLump(lumpnum_t lump, void* dest);
    void            W_ReadLumpSection(lumpnum_t lump, void* dest,
                                      size_t startOffset, size_t length);
    const void*     W_CacheLumpNum(lumpnum_t lump, int tag);
    const void*     W_CacheLumpName(const char* name, int tag);
    void            W_ChangeCacheTag(lumpnum_t lump, int tag);
    const char*     W_LumpSourceFile(lumpnum_t lump);
    uint            W_CRCNumber(void);
    boolean         W_IsFromIWAD(lumpnum_t lump);
    lumpnum_t       W_OpenAuxiliary(const char* fileName);
    int             W_NumLumps(void);

    // Base: Zone.
    void* _DECALL   Z_Malloc(size_t size, int tag, void* ptr);
    void* _DECALL   Z_Calloc(size_t size, int tag, void* user);
    void*           Z_Realloc(void* ptr, size_t n, int mallocTag);
    void*           Z_Recalloc(void* ptr, size_t n, int callocTag);
    void _DECALL    Z_Free(void* ptr);
    void            Z_FreeTags(int lowTag, int highTag);
    void            Z_ChangeTag2(void* ptr, int tag);
    void            Z_CheckHeap(void);

    // Console.
    int             Con_Busy(int flags, const char* taskName,
                             int (*workerFunc)(void*), void* workerData);
    void            Con_BusyWorkerEnd(void);
    boolean         Con_IsBusy(void);
    void            Con_Open(int yes);
    void            Con_SetFont(ddfont_t* cfont);
    void            Con_AddCommand(ccmd_t* cmd);
    void            Con_AddVariable(cvar_t* var);
    void            Con_AddCommandList(ccmd_t* cmdlist);
    void            Con_AddVariableList(cvar_t* varlist);
    cvar_t*         Con_GetVariable(const char* name);
    byte            Con_GetByte(const char* name);
    int             Con_GetInteger(const char* name);
    float           Con_GetFloat(const char* name);
    char*           Con_GetString(const char* name);
    void            Con_SetInteger(const char* name, int value,
                                   byte override);
    void            Con_SetFloat(const char* name, float value,
                                 byte override);
    void            Con_SetString(const char* name, char* text,
                                  byte override);
    void            Con_Printf(const char* format, ...) PRINTF_F(1,2);
    void            Con_FPrintf(int flags, const char* format, ...) PRINTF_F(2,3);
    int             DD_Execute(int silent, const char* command);
    int             DD_Executef(int silent, const char* command, ...);
    void            Con_Message(const char* message, ...) PRINTF_F(1,2);
    void            Con_Error(const char* error, ...) PRINTF_F(1,2);

    // Console: Bindings.
    void            B_SetContextFallback(const char* name,
                                         int (*responderFunc)(event_t*));
    int             B_BindingsForCommand(const char* cmd, char* buf,
                                         size_t bufSize);
    int             B_BindingsForControl(int localPlayer,
                                         const char* controlName,
                                         int inverse,
                                         char* buf, size_t bufSize);

    // System.
    void            Sys_TicksPerSecond(float num);
    int             Sys_GetTime(void);
    double          Sys_GetSeconds(void);
    uint            Sys_GetRealTime(void);
    void            Sys_Sleep(int millisecs);
    int             Sys_CriticalMessage(char* msg);
    void            Sys_Quit(void);

    // System: Files.
    int             F_Access(const char* path);
    unsigned int    F_LastModified(const char* fileName);

    // World: Map.
    struct map_s*   P_CurrentMap(void);
    void            P_SetCurrentMap(struct map_s* map);
    struct map_s*   P_CreateMap(const char* mapID);
    void            P_DestroyMap(struct map_s* map);

    boolean         Map_Load(struct map_s* map);
    void            Map_Precache(struct map_s* map);

    void            Map_Bounds(struct map_s* map, float* min, float* max);

    uint            Map_NumSectors(struct map_s* map);
    uint            Map_NumLineDefs(struct map_s* map);
    uint            Map_NumSideDefs(struct map_s* map);
    uint            Map_NumVertexes(struct map_s* map);
    uint            Map_NumPolyobjs(struct map_s* map);
    uint            Map_NumSegs(struct map_s* map);
    uint            Map_NumSubsectors(struct map_s* map);
    uint            Map_NumNodes(struct map_s* map);
    uint            Map_NumPlanes(struct map_s* map);

    // World: Map: Thinkers.
    void            Map_InitThinkers(struct map_s* map);
    int             Map_IterateThinkers(struct map_s* map, think_t func, int (*callback) (void* p, void* ctx), void* context);
    void            Map_RunThinkers(struct map_s* map);

    void            Map_ThinkerAdd(struct map_s* map, thinker_t* th);
    void            Map_RemoveThinker(struct map_s* map, thinker_t* th);

    struct polyobj_s* Map_Polyobj(struct map_s* map, uint num);

    struct subsector_s* Map_PointInSubsector(struct map_s* map, float x, float y);

    // Object in bounding box iterators.
    boolean         Map_MobjsBoxIterator(struct map_s* map, const float box[4],
                                         boolean (*func) (struct mobj_s*, void*),
                                         void* data);
    boolean         Map_LineDefsBoxIterator(struct map_s* map, const float box[4],
                                            boolean (*func) (struct linedef_s*, void*),
                                            void* data);
    boolean         Map_SubsectorsBoxIterator(struct map_s* map, const float box[4], void* p,
                                              boolean (*func) (subsector_t*, void*),
                                              void* data);

    boolean         Map_PathTraverse(struct map_s* map, float x1, float y1, float x2, float y2, int flags,
                                     boolean (*trav) (intercept_t*));
    boolean         Map_CheckLineSight(struct map_s* map, const float from[3], const float to[3],
                                       float bottomSlope, float topSlope, int flags);

    // Map building interface.
    boolean         MPE_Begin(struct map_s* map);
    boolean         Map_EditEnd(struct map_s* map);

    objectrecordid_t MPE_CreateVertex(struct map_s* map, float x, float y);
    boolean         MPE_CreateVertices(struct map_s* map, size_t num, float* values, objectrecordid_t* indices);
    objectrecordid_t MPE_CreateSideDef(struct map_s* map, objectrecordid_t sector, short flags,
                                      material_t* topMaterial,
                                      float topOffsetX, float topOffsetY, float topRed,
                                      float topGreen, float topBlue,
                                      material_t* middleMaterial,
                                      float middleOffsetX, float middleOffsetY,
                                      float middleRed, float middleGreen,
                                      float middleBlue, float middleAlpha,
                                      material_t* bottomMaterial,
                                      float bottomOffsetX, float bottomOffsetY,
                                      float bottomRed, float bottomGreen,
                                      float bottomBlue);
    objectrecordid_t MPE_CreateLineDef(struct map_s* map, objectrecordid_t v1, objectrecordid_t v2, objectrecordid_t frontSide,
                                      objectrecordid_t backSide, int flags);
    objectrecordid_t MPE_CreateSector(struct map_s* map, float lightlevel, float red, float green, float blue);
    objectrecordid_t MPE_CreatePlane(struct map_s* map, float height, material_t* material,
                                     float matOffsetX, float matOffsetY,
                                     float r, float g, float b, float a,
                                     float normalX, float normalY, float normalZ);
    void             MPE_SetSectorPlane(struct map_s* map, objectrecordid_t sector, uint type, objectrecordid_t plane);
    objectrecordid_t MPE_CreatePolyobj(struct map_s* map, objectrecordid_t* lines, uint linecount,
                                       int tag, int sequenceType, float anchorX, float anchorY);
    boolean          MPE_GameObjectRecordProperty(struct map_s* map, const char* objName, uint idx,
                                         const char* propName, valuetype_t type,
                                         void* data);

    // Custom map object data types.
    boolean         P_CreateObjectDef(int identifier, const char* name);
    boolean         P_AddPropertyToObjectDef(int identifier, int propIdentifier,
                                             const char* propName, valuetype_t type);

    // Network.
    void            Net_SendPacket(int to_player, int type, void* data,
                                   size_t length);
    int             Net_GetTicCmd(void* command, int player);
    const char*     Net_GetPlayerName(int player);
    ident_t         Net_GetPlayerID(int player);

    // Play.
    float           P_AccurateDistance(float dx, float dy);
    float           P_ApproxDistance(float dx, float dy);
    float           P_ApproxDistance3(float dx, float dy, float dz);
    int             DMU_PointOnLineDefSide(float x, float y, void* p);
    int             DMU_BoxOnLineSide(const float* tmbox, void* p);
    void            DMU_MakeDivline(void* p, divline_t* dl);
    int             P_PointOnDivlineSide(float x, float y,
                                         const divline_t* line);
    float           P_InterceptVector(divline_t* v2, divline_t* v1);
    void            DMU_LineOpening(void* p);

    // Play: Controls.
    void            P_NewPlayerControl(int id, controltype_t type, const char* name, const char* bindContext);
    void            P_GetControlState(int playerNum, int control, float* pos, float* relativeOffset);
    int             P_GetImpulseControlState(int playerNum, int control);

    // Play: Map Data Updates and Information Access.
    objectrecordid_t P_ToIndex(const void* ptr);
    void*           P_ToPtr(int type, objectrecordid_t index);
    int             P_Callback(int type, objectrecordid_t index,
                               int (*callback)(void* p, void* ctx),
                               void* context);
    int             P_Callbackp(int type, void* ptr,
                                int (*callback)(void* p, void* ctx),
                                void* context);
    int             P_Iteratep(void *ptr, uint prop,
                               int (*callback) (void* p, void* ctx),
                               void* context);

    // Materials.
    struct material_s* P_MaterialForName(material_namespace_t mnamespace, const char* name);
    unsigned int    P_MaterialCheckIndexForName(material_namespace_t mnamespace, const char* rawName);

    int             P_NewMaterialGroup(int flags);
    void            P_AddMaterialToGroup(int groupNum, int num, int tics, int randomTics);
    void            P_MaterialPreload(material_t* mat);

    // @kludge
    material_t*     R_MaterialForTextureId(material_namespace_t mnamespace, int idx);
    int             R_TextureIdForName(material_namespace_t mnamespace, const char* rawName);

    /* dummy functions */
    void*           P_AllocDummy(int type, void* extraData);
    void            P_FreeDummy(void* dummy);
    int             P_DummyType(void* dummy);
    boolean         P_IsDummy(void* dummy);
    void*           P_DummyExtraData(void* dummy);

    /* index-based write functions */
    void            P_SetBool(int type, objectrecordid_t index, uint prop, boolean param);
    void            P_SetByte(int type, objectrecordid_t index, uint prop, byte param);
    void            P_SetInt(int type, objectrecordid_t index, uint prop, int param);
    void            P_SetFixed(int type, objectrecordid_t index, uint prop, fixed_t param);
    void            P_SetAngle(int type, objectrecordid_t index, uint prop, angle_t param);
    void            P_SetFloat(int type, objectrecordid_t index, uint prop, float param);
    void            P_SetPtr(int type, objectrecordid_t index, uint prop, void* param);

    void            P_SetBoolv(int type, objectrecordid_t index, uint prop, boolean* params);
    void            P_SetBytev(int type, objectrecordid_t index, uint prop, byte* params);
    void            P_SetIntv(int type, objectrecordid_t index, uint prop, int* params);
    void            P_SetFixedv(int type, objectrecordid_t index, uint prop, fixed_t* params);
    void            P_SetAnglev(int type, objectrecordid_t index, uint prop, angle_t* params);
    void            P_SetFloatv(int type, objectrecordid_t index, uint prop, float* params);
    void            P_SetPtrv(int type, objectrecordid_t index, uint prop, void* params);

    /* pointer-based write functions */
    void            P_SetBoolp(void* ptr, uint prop, boolean param);
    void            P_SetBytep(void* ptr, uint prop, byte param);
    void            P_SetIntp(void* ptr, uint prop, int param);
    void            P_SetFixedp(void* ptr, uint prop, fixed_t param);
    void            P_SetAnglep(void* ptr, uint prop, angle_t param);
    void            P_SetFloatp(void* ptr, uint prop, float param);
    void            P_SetPtrp(void* ptr, uint prop, void* param);

    void            P_SetBoolpv(void* ptr, uint prop, boolean* params);
    void            P_SetBytepv(void* ptr, uint prop, byte* params);
    void            P_SetIntpv(void* ptr, uint prop, int* params);
    void            P_SetFixedpv(void* ptr, uint prop, fixed_t* params);
    void            P_SetAnglepv(void* ptr, uint prop, angle_t* params);
    void            P_SetFloatpv(void* ptr, uint prop, float* params);
    void            P_SetPtrpv(void* ptr, uint prop, void* params);

    /* index-based read functions */
    boolean         P_GetBool(int type, objectrecordid_t index, uint prop);
    byte            P_GetByte(int type, objectrecordid_t index, uint prop);
    int             P_GetInt(int type, objectrecordid_t index, uint prop);
    fixed_t         P_GetFixed(int type, objectrecordid_t index, uint prop);
    angle_t         P_GetAngle(int type, objectrecordid_t index, uint prop);
    float           P_GetFloat(int type, objectrecordid_t index, uint prop);
    void*           P_GetPtr(int type, objectrecordid_t index, uint prop);

    void            P_GetBoolv(int type, objectrecordid_t index, uint prop, boolean* params);
    void            P_GetBytev(int type, objectrecordid_t index, uint prop, byte* params);
    void            P_GetIntv(int type, objectrecordid_t index, uint prop, int* params);
    void            P_GetFixedv(int type, objectrecordid_t index, uint prop, fixed_t* params);
    void            P_GetAnglev(int type, objectrecordid_t index, uint prop, angle_t* params);
    void            P_GetFloatv(int type, objectrecordid_t index, uint prop, float* params);
    void            P_GetPtrv(int type, objectrecordid_t index, uint prop, void* params);

    /* pointer-based read functions */
    boolean         P_GetBoolp(void* ptr, uint prop);
    byte            P_GetBytep(void* ptr, uint prop);
    int             P_GetIntp(void* ptr, uint prop);
    fixed_t         P_GetFixedp(void* ptr, uint prop);
    angle_t         P_GetAnglep(void* ptr, uint prop);
    float           P_GetFloatp(void* ptr, uint prop);
    void*           P_GetPtrp(void* ptr, uint prop);

    void            P_GetBoolpv(void* ptr, uint prop, boolean* params);
    void            P_GetBytepv(void* ptr, uint prop, byte* params);
    void            P_GetIntpv(void* ptr, uint prop, int* params);
    void            P_GetFixedpv(void* ptr, uint prop, fixed_t* params);
    void            P_GetAnglepv(void* ptr, uint prop, angle_t* params);
    void            P_GetFloatpv(void* ptr, uint prop, float* params);
    void            P_GetPtrpv(void* ptr, uint prop, void* params);

    uint            P_NumObjectRecords(struct map_s* map, int identifier);
    byte            P_GetObjectRecordByte(struct map_s* map, int identifier, uint elmIdx, int propIdentifier);
    short           P_GetObjectRecordShort(struct map_s* map, int identifier, uint elmIdx, int propIdentifier);
    int             P_GetObjectRecordInt(struct map_s* map, int identifier, uint elmIdx, int propIdentifier);
    fixed_t         P_GetObjectRecordFixed(struct map_s* map, int identifier, uint elmIdx, int propIdentifier);
    angle_t         P_GetObjectRecordAngle(struct map_s* map, int identifier, uint elmIdx, int propIdentifier);
    float           P_GetObjectRecordFloat(struct map_s* map, int identifier, uint elmIdx, int propIdentifier);

    // Play: Misc.
    void            P_MergeCommand(ticcmd_t* dest, ticcmd_t* src); // temporary.
    void            P_SpawnDamageParticleGen(struct mobj_s* mo,
                                             struct mobj_s* inflictor,
                                             int amount);

    // Play: Mobjs.
    struct mobj_s*  P_MobjCreate(struct map_s* map, think_t function, float x, float y, float z,
                                 angle_t angle, float radius, float height, int ddflags);
    void            P_MobjDestroy(struct mobj_s* mo);
    void            P_MobjSetState(struct mobj_s* mo, int statenum);
    void            P_MobjLink(struct mobj_s* mo, byte flags);
    int             P_MobjUnlink(struct mobj_s* mo);

    // Mobj linked object iterators.
    boolean         P_MobjLinesIterator(struct mobj_s* mo,
                                        boolean (*func) (struct linedef_s*,
                                                          void*), void*);
    boolean         P_MobjSectorsIterator(struct mobj_s* mo,
                                          boolean (*func) (sector_t*, void*),
                                          void* data);

    // Play: Polyobjs.
    boolean         P_PolyobjMove(struct polyobj_s* po, float x, float y);
    boolean         P_PolyobjRotate(struct polyobj_s* po, angle_t angle);
    void            P_PolyobjLinkLineDefs(struct polyobj_s* po);
    void            P_PolyobjUnlinkLineDefs(struct polyobj_s* po);

    void            P_SetPolyobjCallback(void (*func)(struct mobj_s*, void*, void*));

    // Play: Thinkers.
    struct map_s*   Thinker_Map(thinker_t* th);
    void            Thinker_SetStasis(thinker_t* th, boolean on);

    // Refresh.
    int             DD_GetFrameRate(void);
    void            R_SetDataPath(const char* path);
    void            R_BeginSetupMap(void);
    void            R_SetupMap(struct map_s* map, int mode, int flags);
    void            R_PrecacheMobjNum(int mobjtypeNum);
    void            R_PrecachePatch(lumpnum_t lump);
    void            R_PrecacheSkinsForState(int stateIndex);
    void            R_RenderPlayerView(int num);
    void            R_SetViewWindow(int x, int y, int w, int h);
    int             R_GetViewPort(int player, int* x, int* y, int* w, int* h);
    void            R_SetBorderGfx(char* lumps[9]);
    boolean         R_GetSpriteInfo(int sprite, int frame,
                                    spriteinfo_t* sprinfo);
    boolean         R_GetPatchInfo(lumpnum_t lump, patchinfo_t* info);
    void            R_HSVToRGB(float* rgb, float h, float s, float v);
    angle_t         R_PointToAngle2(float x1, float y1, float x2,
                                    float y2);

    colorpaletteid_t R_CreateColorPalette(const char* fmt, const char* name,
                                          const byte* data, size_t num);
    colorpaletteid_t R_GetColorPaletteNumForName(const char* name);
    const char*     R_GetColorPaletteNameForNum(colorpaletteid_t id);
    void            R_GetColorPaletteRGBf(colorpaletteid_t id, float rgb[3],
                                          int idx, boolean correctGamma);

    // Renderer.
    void            Rend_Reset(void);

    // Graphics.
    void            GL_UseFog(int yes);
    byte*           GL_GrabScreen(void);
    DGLuint         GL_LoadGraphics(ddresourceclass_t resClass,
                                     const char* name, gfxmode_t mode,
                                     int useMipmap, boolean clamped,
                                     int otherFlags);
    DGLuint         GL_NewTextureWithParams3(int format, int width,
                                             int height, void* pixels,
                                             int flags, int minFilter,
                                             int magFilter, int anisoFilter,
                                             int wrapS, int wrapT);
    void            GL_SetFilter(boolean enable);
    void            GL_SetFilterColor(float r, float g, float b, float a);

    // Graphics: 2D drawing.
    void            GL_DrawPatch(int x, int y, lumpnum_t lump);
    void            GL_DrawPatch_CS(int x, int y, lumpnum_t lump);
    void            GL_DrawPatchLitAlpha(int x, int y, float light,
                                         float alpha, lumpnum_t lump);
    void            GL_DrawFuzzPatch(int x, int y, lumpnum_t lump);
    void            GL_DrawAltFuzzPatch(int x, int y, lumpnum_t lump);
    void            GL_DrawShadowedPatch(int x, int y, lumpnum_t lump);
    void            GL_DrawRawScreen(lumpnum_t lump, float offx,
                                     float offy);
    void            GL_DrawRawScreen_CS(lumpnum_t lump, float offx,
                                        float offy, float scalex,
                                        float scaley);

    // Audio.
    int             S_LocalSoundAtVolumeFrom(int sound_id,
                                             struct mobj_s* origin,
                                             float* pos, float volume);
    int             S_LocalSoundAtVolume(int soundID, struct mobj_s* origin,
                                         float volume);
    int             S_LocalSound(int soundID, struct mobj_s* origin);
    int             S_LocalSoundFrom(int soundID, float* fixedpos);
    int             S_StartSound(int soundId, struct mobj_s* origin);
    int             S_StartSoundEx(int soundId, struct mobj_s* origin);
    int             S_StartSoundAtVolume(int soundID, struct mobj_s* origin,
                                         float volume);
    int             S_ConsoleSound(int soundID, struct mobj_s* origin,
                                   int targetConsole);
    void            S_StopSound(int soundID, struct mobj_s* origin);
    int             S_IsPlaying(int soundID, struct mobj_s* origin);
    int             S_StartMusic(const char* musicID, boolean looped);
    int             S_StartMusicNum(int id, boolean looped);
    void            S_StopMusic(void);

    // Miscellaneous.
    size_t          M_ReadFile(const char* name, byte** buffer);
    size_t          M_ReadFileCLib(const char* name, byte** buffer);
    boolean         M_WriteFile(const char* name, void* source,
                                size_t length);
    void            M_ExtractFileBase(char* dest, const char* path,
                                      size_t len);
    char*           M_FindFileExtension(char* path);
    boolean         M_CheckPath(const char* path);
    int             M_FileExists(const char* file);
    void            M_TranslatePath(char* translated, const char* path,
                                    size_t len);
    const char*     M_PrettyPath(const char* path);
    char*           M_SkipWhite(char* str);
    char*           M_FindWhite(char* str);
    char*           M_StrCatQuoted(char* dest, const char* src, size_t len);
    byte            RNG_RandByte(void);
    float           RNG_RandFloat(void);
    void            M_ClearBox(fixed_t* box);
    void            M_AddToBox(fixed_t* box, fixed_t x, fixed_t y);
    int             M_ScreenShot(const char* filename, int bits);
    int             M_CeilPow2(int num);

    // MiscellaneousL: Time utilities.
    boolean         M_RunTrigger(trigger_t* trigger, timespan_t advanceTime);
    boolean         M_CheckTrigger(const trigger_t* trigger, timespan_t advanceTime);

    // Miscellaneous: Math.
    void            V2_Rotate(float vec[2], float radians);
    float           M_PointLineDistance(const float* a, const float* b, const float* c);
    float           M_ProjectPointOnLine(const float* point, const float* linepoint,
                                         const float* delta, float gap, float* result);
    binangle_t      bamsAtan2(int y, int x);

    // Miscellaneous: Command line.
    void _DECALL    ArgAbbreviate(const char* longName,
                                  const char* shortName);
    int _DECALL     Argc(void);
    const char* _DECALL Argv(int i);
    const char** _DECALL ArgvPtr(int i);
    const char* _DECALL ArgNext(void);
    int _DECALL     ArgCheck(const char* check);
    int _DECALL     ArgCheckWith(const char* check, int num);
    int _DECALL     ArgExists(const char* check);
    int _DECALL     ArgIsOption(int i);

#ifdef __cplusplus
}
#endif
#endif /* DOOMSDAY_EXPORTS_H */
