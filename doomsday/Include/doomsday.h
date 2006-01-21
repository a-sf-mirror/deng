/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * doomsday.h: Doomsday Engine API
 *
 * Doomsday Engine API. Routines exported from Doomsday.exe (all of
 * this uses __stdcall). Games and plugins need to include this to
 * gain access to the engine's features.
 */

#ifndef __DOOMSDAY_EXPORTS_H__
#define __DOOMSDAY_EXPORTS_H__

// The calling convention.
#if defined(WIN32)
#   define _DECALL  __stdcall
#elif defined(UNIX)
#   define _DECALL
#endif

#ifdef __cplusplus
extern          "C" {
#endif

#include "dd_share.h"
#include "dd_plugin.h"

/*
 * Public definitions of the internal map data pointers.  These can be
 * accessed externally, but only as identifiers to data instances.
 * For example, a game could use sector_t to identify to sector to
 * change with the Map Update API.
 */
    typedef void node_s;
    typedef void vertex_s;
    typedef void line_s;
    typedef void side_s;
    typedef void seg_s;
    typedef void subsector_s;
    typedef void sector_s;
    typedef void polyblock_s;
    typedef void polyobj_s;

    typedef struct node_s node_t;
    typedef struct vertex_s vertex_t;
    typedef struct line_s line_t;
    typedef struct side_s side_t;
    typedef struct seg_s seg_t;
    typedef struct subsector_s subsector_t;
    typedef struct sector_s sector_t;
    typedef struct polyblock_s polyblock_t;
    typedef struct polyobj_s polyobj_t;

	// Base.
	void           *DD_GetDGLProcAddress(const char *name);
	void            DD_AddIWAD(const char *path);
	void            DD_AddStartupWAD(const char *file);
	void            DD_SetConfigFile(char *filename);
	void            DD_SetDefsFile(char *filename);
	int _DECALL     DD_GetInteger(int ddvalue);
	void            DD_SetInteger(int ddvalue, int parm);
    void            DD_SetVariable(int ddvalue, void *ptr);
    void           *DD_GetVariable(int ddvalue);
    ddplayer_t     *DD_GetPlayer(int number);

	// Base: Definitions.
	int             Def_Get(int type, char *id, void *out);
	int             Def_Set(int type, int index, int value, void *ptr);
	int             Def_EvalFlags(char *flags);

	// Base: Input.
	void            DD_ClearKeyRepeaters(void);
	int             DD_GetKeyCode(const char *name);

	// Base: WAD.
	int             W_CheckNumForName(char *name);
	int             W_GetNumForName(char *name);
	int             W_LumpLength(int lump);
	const char     *W_LumpName(int lump);
	void            W_ReadLump(int lump, void *dest);
    void        W_ReadLumpSection(int lump, void *dest, int startoffset, int length);
	void           *W_CacheLumpNum(int lump, int tag);
	void           *W_CacheLumpName(char *name, int tag);
	void            W_ChangeCacheTag(int lump, int tag);
	const char     *W_LumpSourceFile(int lump);
	uint            W_CRCNumber(void);
	boolean         W_IsFromIWAD(int lump);
	int             W_OpenAuxiliary(const char *filename);

	// Base: Zone.
	void           *_DECALL Z_Malloc(size_t size, int tag, void *ptr);
	void           *Z_Calloc(size_t size, int tag, void *user);
	void           *Z_Realloc(void *ptr, size_t n, int malloctag);
	void           *Z_Recalloc(void *ptr, size_t n, int calloctag);
	void _DECALL    Z_Free(void *ptr);
	void            Z_FreeTags(int lowtag, int hightag);
	void            Z_ChangeTag2(void *ptr, int tag);
	void            Z_CheckHeap(void);

	// Console.
	void            Con_Open(int yes);
	void            Con_SetFont(ddfont_t *cfont);
	void            Con_AddCommand(ccmd_t *cmd);
	void            Con_AddVariable(cvar_t *var);
	void            Con_AddCommandList(ccmd_t *cmdlist);
	void            Con_AddVariableList(cvar_t *varlist);
	cvar_t         *Con_GetVariable(char *name);
	byte            Con_GetByte(char *name);
	int             Con_GetInteger(char *name);
	float           Con_GetFloat(char *name);
	char           *Con_GetString(char *name);
    void            Con_SetInteger(char *name, int value, byte override);
    void            Con_SetFloat(char *name, float value, byte override);
    void            Con_SetString(char *name, char *text, byte override);
	void            Con_Printf(char *format, ...);
	void            Con_FPrintf(int flags, char *format, ...);
    int             DD_Execute(char *command, int silent);
    int             DD_Executef(int silent, char *command, ...);
	void            Con_Message(char *message, ...);
	void            Con_Error(char *error, ...);

	// Console: Actions.
	void            Con_DefineActions(action_t *acts);

	// Console: Bindings.
	void            B_EventBuilder(char *buff, event_t *ev, boolean to_event);
	int             B_BindingsForCommand(char *command, char *buffer, int bindClass);
    void            DD_AddBindClass(struct bindclass_s *);
    boolean         DD_SetBindClass(int classID, int type);

	// System.
	void            Sys_TicksPerSecond(float num);
	int             Sys_GetTime(void);
	double          Sys_GetSeconds(void);
	uint            Sys_GetRealTime(void);
	void            Sys_Sleep(int millisecs);
	int             Sys_CriticalMessage(char *msg);
	void            Sys_Quit(void);

	// System: Files.
	int             F_Access(const char *path);
	unsigned int	F_LastModified(const char *fileName);

	// Network.
	void            Net_SendPacket(int to_player, int type, void *data,
								   int length);
	int             Net_GetTicCmd(void *command, int player);
	char           *Net_GetPlayerName(int player);
	ident_t         Net_GetPlayerID(int player);

	// Play.
	float           P_AccurateDistance(fixed_t dx, fixed_t dy);
	fixed_t         P_ApproxDistance(fixed_t dx, fixed_t dy);
	fixed_t         P_ApproxDistance3(fixed_t dx, fixed_t dy, fixed_t dz);
	int             P_PointOnLineSide(fixed_t x, fixed_t y,
									  struct line_s *line);
	int             P_BoxOnLineSide(fixed_t *tmbox, struct line_s *ld);
	void            P_MakeDivline(struct line_s *li, divline_t * dl);
	int             P_PointOnDivlineSide(fixed_t x, fixed_t y,
										 divline_t * line);
	fixed_t         P_InterceptVector(divline_t * v2, divline_t * v1);
	void            P_LineOpening(struct line_s *linedef);
	struct mobj_s  *P_GetBlockRootIdx(int index);
	void            P_LinkThing(struct mobj_s *thing, byte flags);
	void            P_UnlinkThing(struct mobj_s *thing);
    void            P_PointToBlock(fixed_t x, fixed_t y, int *bx, int *by);
	boolean         P_BlockLinesIterator(int x, int y,
										 boolean (*func) (struct line_s *,
														  void *), void *);
	boolean         P_BlockThingsIterator(int x, int y,
										  boolean (*func) (struct mobj_s *,
														   void *), void *);
	boolean         P_BlockPolyobjsIterator(int x, int y,
											boolean (*func) (void *, void *),
											void *);
	boolean         P_ThingLinesIterator(struct mobj_s *thing,
										 boolean (*func) (struct line_s *,
														  void *), void *);
	boolean         P_ThingSectorsIterator(struct mobj_s *thing,
										   boolean (*func) (struct sector_s *,
															void *),
										   void *data);
	boolean         P_LineThingsIterator(struct line_s *line,
										 boolean (*func) (struct mobj_s *,
														  void *), void *data);
	boolean         P_SectorTouchingThingsIterator(struct sector_s *sector,
												   boolean (*func) (struct
																	mobj_s *,
																	void *),
												   void *data);
	boolean         P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2,
								   fixed_t y2, int flags,
								   boolean (*trav) (intercept_t *));
	boolean         P_CheckSight(struct mobj_s *t1, struct mobj_s *t2);
	void            P_SetState(struct mobj_s *mobj, int statenum);
	void            P_SpawnDamageParticleGen(struct mobj_s *mo,
											 struct mobj_s *inflictor,
											 int amount);

	// Play: Setup.
	void            P_LoadBlockMap(int lump);
	void            P_LoadReject(int lump);

    // Play: Map Data Updates and Information Access.
    int             P_ToIndex(int type, void* ptr);
    void*           P_ToPtr(int type, int index);
    int             P_Callback(int type, int index, void* context, 
                               int (*callback)(void* p, void* ctx));
    int             P_Callbackp(int type, void* ptr, void* context, 
                                int (*callback)(void* p, void* ctx));
    
    /* dummy functions */
    void           *P_AllocDummy(int type, void* extraData);
    void            P_FreeDummy(void* dummy)
    int             P_DummyType(void* dummy);
    boolean         P_IsDummy(void* dummy);
    void           *P_DummyExtraData(void* dummy);
                                
    /* property manipulation functions */                            
    void            P_Copy(int type, int prop, int fromIndex, int toIndex);
    void            P_Swap(int type, int prop, int fromIndex, int toIndex);
    void            P_Copyp(int type, int prop, void* from, void* to);
    void            P_Swapp(int type, int prop, void* from, void* to);

    /* index-based write functions */
    void            P_SetBool(int type, int index, int prop, boolean param);
    void            P_SetByte(int type, int index, int prop, byte param);
    void            P_SetInt(int type, int index, int prop, int param);
    void            P_SetFixed(int type, int index, int prop, fixed_t param);
    void            P_SetAngle(int type, int index, int prop, angle_t param);
    void            P_SetFloat(int type, int index, int prop, float param);
    void            P_SetPtr(int type, int index, int prop, void* param);

    void            P_SetBoolv(int type, int index, int prop, boolean* params);
    void            P_SetBytev(int type, int index, int prop, byte* params);
    void            P_SetIntv(int type, int index, int prop, int* params);
    void            P_SetFixedv(int type, int index, int prop, fixed_t* params);
    void            P_SetAnglev(int type, int index, int prop, angle_t* params);
    void            P_SetFloatv(int type, int index, int prop, float* params);
    void            P_SetPtrv(int type, int index, int prop, void* params);

    /* pointer-based write functions */
    void            P_SetBoolp(int type, void* ptr, int prop, boolean param);
    void            P_SetBytep(int type, void* ptr, int prop, byte param);
    void            P_SetIntp(int type, void* ptr, int prop, int param);
    void            P_SetFixedp(int type, void* ptr, int prop, fixed_t param);
    void            P_SetAnglep(int type, void* ptr, int prop, angle_t param);
    void            P_SetFloatp(int type, void* ptr, int prop, float param);
    void            P_SetPtrp(int type, void* ptr, int prop, void* param);

    void            P_SetBoolpv(int type, void* ptr, int prop, boolean* params);
    void            P_SetBytepv(int type, void* ptr, int prop, byte* params);
    void            P_SetIntpv(int type, void* ptr, int prop, int* params);
    void            P_SetFixedpv(int type, void* ptr, int prop, fixed_t* params);
    void            P_SetAnglepv(int type, void* ptr, int prop, angle_t* params);
    void            P_SetFloatpv(int type, void* ptr, int prop, float* params);
    void            P_SetPtrpv(int type, void* ptr, int prop, void* params);    

    /* index-based read functions */
    boolean         P_GetBool(int type, int index, int prop);
    byte            P_GetByte(int type, int index, int prop);
    int             P_GetInt(int type, int index, int prop);
    fixed_t         P_GetFixed(int type, int index, int prop);
    angle_t         P_GetAngle(int type, int index, int prop);
    float           P_GetFloat(int type, int index, int prop);
    void*           P_GetPtr(int type, int index, int prop);

    void            P_GetBoolv(int type, int index, int prop, boolean* params);
    void            P_GetBytev(int type, int index, int prop, byte* params);
    void            P_GetIntv(int type, int index, int prop, int* params);
    void            P_GetFixedv(int type, int index, int prop, fixed_t* params);
    void            P_GetAnglev(int type, int index, int prop, angle_t* params);
    void            P_GetFloatv(int type, int index, int prop, float* params);
    void            P_GetPtrv(int type, int index, int prop, void* params);
    
    /* pointer-based read functions */
    boolean         P_GetBoolp(int type, void* ptr, int prop);
    byte            P_GetBytep(int type, void* ptr, int prop);
    int             P_GetIntp(int type, void* ptr, int prop);
    fixed_t         P_GetFixedp(int type, void* ptr, int prop);
    angle_t         P_GetAnglep(int type, void* ptr, int prop);
    float           P_GetFloatp(int type, void* ptr, int prop);
    void*           P_GetPtrp(int type, void* ptr, int prop);

    void            P_GetBoolpv(int type, void* ptr, int prop, boolean* params);
    void            P_GetBytepv(int type, void* ptr, int prop, byte* params);
    void            P_GetIntpv(int type, void* ptr, int prop, int* params);
    void            P_GetFixedpv(int type, void* ptr, int prop, fixed_t* params);
    void            P_GetAnglepv(int type, void* ptr, int prop, angle_t* params);
    void            P_GetFloatpv(int type, void* ptr, int prop, float* params);
    void            P_GetPtrpv(int type, void* ptr, int prop, void* params);

	// Play: Polyobjs.
    void            PO_Allocate(void);
	boolean         PO_MovePolyobj(int num, int x, int y);
	boolean         PO_RotatePolyobj(int num, angle_t angle);
	void            PO_UnLinkPolyobj(void *po);
	void            PO_LinkPolyobj(void *po);
	void           	PO_SetCallback(void (*func) 
								   (struct mobj_s *, void *, void *));

	// Play: Thinkers.
	void            P_RunThinkers(void);
	void            P_InitThinkers(void);
	void            P_AddThinker(thinker_t *thinker);
	void            P_RemoveThinker(thinker_t *thinker);

	// Refresh.
	int             DD_GetFrameRate(void);
	void            R_SetDataPath(const char *path);
	void            R_SetupLevel(char *level_id, int flags);
	void            R_PrecacheLevel(void);
	void            R_PrecacheSkinsForState(int stateIndex);
	void            R_RenderPlayerView(ddplayer_t *player);
	void            R_ViewWindow(int x, int y, int w, int h);
	void            R_SetBorderGfx(char *lumps[9]);
	void            R_GetSpriteInfo(int sprite, int frame,
									spriteinfo_t *sprinfo);
	void            R_GetPatchInfo(int lump, spriteinfo_t *info);
	int             R_FlatNumForName(char *name);
	int             R_CheckTextureNumForName(char *name);
	int             R_TextureNumForName(char *name);
	char           *R_TextureNameForNum(int num);
	int             R_SetFlatTranslation(int flat, int translate_to);
	int             R_SetTextureTranslation(int tex, int translate_to);
	void            R_SetAnimGroup(int type, int number, int group);
	int             R_CreateAnimGroup(int type, int flags);
	void            R_AddToAnimGroup(int groupNum, int number, int tics,
									 int randomTics);
	angle_t         R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2,
									fixed_t y2);
	struct subsector_s *R_PointInSubsector(fixed_t x, fixed_t y);

	// Renderer.
	void            Rend_Reset(void);
	void            Rend_SkyParams(int layer, int parm, float value);

	// Graphics.
	void            GL_Update(int flags);
	void            GL_DoUpdate(void);
	void            GL_UseFog(int yes);
	int             GL_ChangeResolution(int w, int h, int bits);
	byte           *GL_GrabScreen(void);
	void            GL_TextureFilterMode(int target, int parm);
	void            GL_SetColor(int palidx);
	void            GL_SetColor2(int palidx, float alpha);
	void            GL_SetColorAndAlpha(float r, float g, float b, float a);
	void            GL_SetNoTexture(void);
	void            GL_SetPatch(int lump);
	void            GL_SetSprite(int pnum, int spriteType);
	void            GL_SetFlat(int idx);
	void            GL_SetTexture(int idx);
	unsigned int    GL_SetRawImage(int lump, int part);
	unsigned int    GL_LoadGraphics(const char *name, int mode);

	// Graphics: 2D drawing.
	void            GL_DrawPatch(int x, int y, int lumpnum);
	void            GL_DrawPatch_CS(int x, int y, int lumpnum);
	void            GL_DrawPatchLitAlpha(int x, int y, float light,
										 float alpha, int lumpnum);
	void            GL_DrawFuzzPatch(int x, int y, int lumpnum);
	void            GL_DrawAltFuzzPatch(int x, int y, int lumpnum);
	void            GL_DrawShadowedPatch(int x, int y, int lumpnum);
	void            GL_DrawRawScreen(int lump, float offx, float offy);
	void            GL_DrawRawScreen_CS(int lump, float offx, float offy,
										float scalex, float scaley);
	void            GL_DrawLine(float x1, float y1, float x2, float y2,
								float r, float g, float b, float a);
	void            GL_DrawRect(float x, float y, float w, float h, float r,
								float g, float b, float a);
	void            GL_DrawRectTiled(int x, int y, int w, int h, int tw,
									 int th);
	void			GL_DrawCutRectTiled(int x, int y, int w, int h, int tw,
									int th, int txoff, int tyoff, int cx,
									int cy, int cw, int ch);
	void            GL_DrawPSprite(float x, float y, float scale, int flip,
								   int lump);
	void            GL_SetFilter(int filter_rgba);

	// Graphics: PCX.
	int             PCX_GetSize(const char *fn, int *w, int *h);
	void            PCX_Load(const char *fn, int buf_w, int buf_h,
							 byte *outBuffer);
	int             PCX_MemoryLoad(byte *imgdata, int len, int buf_w,
								   int buf_h, byte *outBuffer);

	// Graphics: PNG.
	byte           *PNG_Load(const char *fileName, int *width, int *height,
							 int *pixelSize);

	// Graphics: TGA.
	int             TGA_Save24_rgb565(char *filename, int w, int h,
									  unsigned short *buffer);
	int             TGA_Save24_rgb888(char *filename, int w, int h,
									  byte *buffer);
	int             TGA_Save24_rgba8888(char *filename, int w, int h,
										byte *buffer);
	int             TGA_Save16_rgb888(char *filename, int w, int h,
									  byte *buffer);
	int             TGA_GetSize(char *filename, int *w, int *h);

	// Audio.
	void            S_LevelChange(void);
	int             S_LocalSoundAtVolumeFrom(int sound_id,
											 struct mobj_s *origin,
											 float *fixedpos, float volume);
	int             S_LocalSoundAtVolume(int sound_id, struct mobj_s *origin,
										 float volume);
	int             S_LocalSound(int sound_id, struct mobj_s *origin);
	int             S_LocalSoundFrom(int sound_id, float *fixedpos);
	int             S_StartSound(int sound_id, struct mobj_s *origin);
	int             S_StartSoundAtVolume(int sound_id, struct mobj_s *origin,
										 float volume);
	int             S_ConsoleSound(int sound_id, struct mobj_s *origin,
								   int target_console);
	void            S_StopSound(int sound_id, struct mobj_s *origin);
	int             S_IsPlaying(int sound_id, struct mobj_s *origin);
	int             S_StartMusic(char *musicid, boolean looped);
	int             S_StartMusicNum(int id, boolean looped);
	void            S_StopMusic(void);

	// Miscellaneous.
	int             M_ReadFile(char const *name, byte **buffer);
	int             M_ReadFileCLib(char const *name, byte **buffer);
	boolean         M_WriteFile(char const *name, void *source, int length);
	void            M_ExtractFileBase(const char *path, char *dest);
	void            M_GetFileExt(const char *path, char *ext);
	boolean         M_CheckPath(char *path);
	int             M_FileExists(const char *file);
	void            M_TranslatePath(const char *path, char *translated);
	char           *M_SkipWhite(char *str);
	char           *M_FindWhite(char *str);
    char           *M_StrCatQuoted(char *dest, char *src);
    byte            M_Random(void);
    float           M_FRandom(void);
    void            M_ClearBox(fixed_t *box);
    void            M_AddToBox(fixed_t *box, fixed_t x, fixed_t y);
    int             M_ScreenShot(char *filename, int bits);

    // Miscellaneous: Math.
    binangle_t      bamsAtan2(int y, int x);

    // Miscellaneous: Command line.
    void            ArgAbbreviate(char *longname, char *shortname);
    int             Argc(void);
    char           *Argv(int i);
    char          **ArgvPtr(int i);
    char           *ArgNext(void);
    int             ArgCheck(char *check);
    int             ArgCheckWith(char *check, int num);
    int _DECALL     ArgExists(char *check);
    int             ArgIsOption(int i);

    // DMU
    void            P_LoadMap(int mapLumpStartNumber, int glLumpStartNum, char *levelID);
#ifdef __cplusplus
}
#endif
#endif
