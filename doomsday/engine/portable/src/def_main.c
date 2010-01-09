/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * def_main.c: Definitions Subsystem
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_platform.h"
#include "de_refresh.h"
#include "de_console.h"
#include "de_audio.h"
#include "de_misc.h"

#include <string.h>
#include <ctype.h>

// XGClass.h is actually a part of the engine.
#include "../../../plugins/common/include/xgclass.h"

// MACROS ------------------------------------------------------------------

#define LOOPi(n)    for(i = 0; i < (n); ++i)
#define LOOPk(n)    for(k = 0; k < (n); ++k)

// Legacy frame flags.
#define FF_FULLBRIGHT       0x8000 // flag in mobj->frame
#define FF_FRAMEMASK        0x7fff

// TYPES -------------------------------------------------------------------

typedef struct {
    char*           name; // Name of the routine.
    void            (C_DECL * func) (); // Pointer to the function.
} actionlink_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Def_ReadProcessDED(const char* filename);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern filename_t defsFileName;
extern filename_t topDefsFileName;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ded_t defs; // The main definitions database.
sprname_t* sprNames; // Sprite name list.
state_t* states; // State list.
ded_light_t** stateLights;
ded_generator_t** stateGeneratorDefs;
mobjinfo_t* mobjInfo; // Map object info database.
sfxinfo_t* sounds; // Sound effect list.

ddtext_t* texts; // Text list.
mobjinfo_t** stateOwners; // A pointer for each state.
ded_count_t countSprNames;
ded_count_t countStates;
ded_count_t countMobjInfo;
ded_count_t countSounds;
ded_count_t countTexts;
ded_count_t countStateOwners;

boolean firstDED;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean defsInited = false;
static const char* dedFiles[MAX_READ];
static mobjinfo_t* gettingFor;

xgclass_t nullXgClassLinks; // Used when none defined.
xgclass_t* xgClassLinks;

// CODE --------------------------------------------------------------------

/**
 * Retrieves the XG Class list from the Game.
 * XGFunc links are provided by the Game, who owns the actual
 * XG classes and their functions.
 */
static int GetXGClasses(void)
{
    xgClassLinks = (xgclass_t *) gx.GetVariable(DD_XGFUNC_LINK);
    if(!xgClassLinks)
    {
        memset(&nullXgClassLinks, 0, sizeof(nullXgClassLinks));
        xgClassLinks = &nullXgClassLinks;
    }
    return 1;
}

/**
 * Initializes the definition databases.
 */
void Def_Init(void)
{
    int                 i, c, p;

    sprNames = NULL; // Sprite name list.
    mobjInfo = NULL;
    states = NULL;
    stateGeneratorDefs = NULL;
    stateLights = NULL;
    sounds = NULL;
    texts = NULL;
    stateOwners = NULL;
    DED_ZCount(&countSprNames);
    DED_ZCount(&countMobjInfo);
    DED_ZCount(&countStates);
    DED_ZCount(&countSounds);
    DED_ZCount(&countTexts);
    DED_ZCount(&countStateOwners);

    // Retrieve the XG Class links from the game .dll
    GetXGClasses();

    DED_Init(&defs);

    for(i = 0; i < MAX_READ; ++i)
        dedFiles[i] = NULL;

    // The engine defs.
    c = 0;
    dedFiles[c++] = defsFileName;

    // Add the default ded. It will be overwritten by -defs.
    dedFiles[c++] = topDefsFileName;

    // See which .ded files are specified on the command line.
    for(p = 0; p < Argc(); ++p)
    {
        const char*         arg = Argv(p);

        if(!ArgRecognize("-def", arg) && !ArgRecognize("-defs", arg))
            continue;

        while(c < MAX_READ && ++p != Argc() && !ArgIsOption(p))
        {
            // Add it to the list.
            dedFiles[c++] = Argv(p);

            Con_Message("Def_Init: Added '%s' to dedFiles.\n", Argv(p));
        }

        p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }
}

/**
 * Destroy databases.
 */
void Def_Destroy(void)
{
    // To make sure...
    DED_Destroy(&defs);
    DED_Init(&defs);

    // Destroy the databases.
    DED_DelArray((void **) &sprNames, &countSprNames);
    DED_DelArray((void **) &states, &countStates);
    DED_DelArray((void **) &mobjInfo, &countMobjInfo);
    DED_DelArray((void **) &sounds, &countSounds);
    DED_DelArray((void **) &texts, &countTexts);
    DED_DelArray((void **) &stateOwners, &countStateOwners);

    // Destroy the state array, parallel LUTs.
    if(stateGeneratorDefs)
        M_Free(stateGeneratorDefs);
    stateGeneratorDefs = NULL;
    if(stateLights)
        M_Free(stateLights);
    stateLights = NULL;

    defsInited = false;
}

/**
 * Guesses the location of the Defs Auto directory based on main DED
 * file.
 */
void Def_GetAutoPath(char* path, size_t len)
{
    char*               lastSlash;

    strncpy(path, topDefsFileName, len);
    lastSlash = strrchr(path, DIR_SEP_CHAR);
    if(!lastSlash)
    {
        strncpy(path, "", len); // Failure!
        return;
    }

    strncpy(lastSlash + 1, "auto" DIR_SEP_STR,
            len - ((lastSlash + 1) - path));
}

/**
 * @return              Number of the given sprite else,
 *                      @c -1, if it doesn't exist.
 */
int Def_GetSpriteNum(const char* name)
{
    int                 i;

    if(!name || !name[0])
        return -1;

    for(i = 0; i < countSprNames.num; ++i)
        if(!stricmp(sprNames[i].name, name))
            return i;

    return -1;
}

int Def_GetMobjNum(const char* id)
{
    int i;

    if(!id || !id[0])
        return -1;

    for(i = 0; i < defs.count.mobjs.num; ++i)
        if(!stricmp(defs.mobjs[i].id, id))
            return i;

    return -1;
}

int Def_GetMobjNumForName(const char* name)
{
    int                 i;

    if(!name || !name[0])
        return -1;

    for(i = defs.count.mobjs.num -1; i >= 0; --i)
        if(!stricmp(defs.mobjs[i].name, name))
            return i;

    return -1;
}

int Def_GetStateNum(const char* id)
{
    int                 i;

    if(!id || !id[0])
        return -1;

    for(i = 0; i < defs.count.states.num; ++i)
        if(!strcmp(defs.states[i].id, id))
            return i;

    return -1;
}

int Def_GetModelNum(const char *id)
{
    int                 i;

    if(!id[0])
        return -1;

    for(i = 0; i < defs.count.models.num; ++i)
        if(!strcmp(defs.models[i].id, id))
            return i;

    return -1;
}

int Def_GetSoundNum(const char* id)
{
    int                 i;

    if(!id || !id[0])
        return -1;

    for(i = 0; i < defs.count.sounds.num; ++i)
    {
        if(!strcmp(defs.sounds[i].id, id))
            return i;
    }

    return -1;
}

/**
 * Looks up a sound using the Name key. If the name is not found, returns
 * the NULL sound index (zero).
 */
int Def_GetSoundNumForName(const char* name)
{
    int                 i;

    if(!name || !name[0])
        return -1;

    for(i = 0; i < defs.count.sounds.num; ++i)
        if(!stricmp(defs.sounds[i].name, name))
            return i;

    return 0;
}

int Def_GetMusicNum(const char* id)
{
    int                 i;

    if(!id || !id[0])
        return -1;

    for(i = 0; i < defs.count.music.num; ++i)
        if(!stricmp(defs.music[i].id, id))
            return i;

    return -1;
}

/*// A simple action function that will be executed.
   void A_ExecuteCommand(mobj_t *mobj)
   {

   } */

acfnptr_t Def_GetActionPtr(const char* name)
{
    // Action links are provided by the Game, who owns the actual
    // action functions.
    actionlink_t*       link =
        (actionlink_t *) gx.GetVariable(DD_ACTION_LINK);

    if(!name || !name[0])
        return 0;

    if(!link)
    {
        Con_Error("GetActionPtr: Game DLL doesn't have an action "
                  "function link table.\n");
    }
    for(; link->name; link++)
        if(!stricmp(name, link->name))
            return link->func;

    // The engine provides a couple of simple action functions.
    /*if(!strcmp(name, "A_ExecuteCommand"))
       return A_ExecuteCommand;
       if(!strcmp(name, "A_ExecuteCommandPSpr"))
       return A_ExecuteCommandPSpr; */

    return 0;
}

ded_mapinfo_t* Def_GetMapInfo(const char* mapID)
{
    int                 i;

    if(!mapID || !mapID[0])
        return 0;

    for(i = defs.count.mapInfo.num - 1; i >= 0; i--)
        if(!stricmp(defs.mapInfo[i].id, mapID))
            return defs.mapInfo + i;

    return 0;
}

ded_sky_t* Def_GetSky(const char* id)
{
    int                 i;

    if(!id || !id[0])
        return NULL;

    for(i = defs.count.skies.num - 1; i >= 0; i--)
        if(!stricmp(defs.skies[i].id, id))
            return defs.skies + i;

    return NULL;
}

ded_material_t* Def_GetMaterial(const char* name, material_namespace_t mnamespace)
{
    int                 i;

    if(!name || !name[0])
        return NULL;

    for(i = defs.count.materials.num - 1; i >= 0; i--)
    {
        ded_material_t*     def = &defs.materials[i];

        if(mnamespace != MN_ANY && def->id.mnamespace != mnamespace)
            continue;

        if(!stricmp(def->id.name, name))
            return def;
    }

    return 0;
}

ded_decor_t* Def_GetDecoration(material_t* mat, boolean hasExt)
{
    int                 i;
    ded_decor_t*        def;

    for(i = defs.count.decorations.num - 1, def = defs.decorations + i;
        i >= 0; i--, def--)
    {
        material_t*         defMat = Materials_ToMaterial(
            Materials_IndexForName(def->material.mnamespace, def->material.name));

        if(mat == defMat)
        {
            // Is this suitable?
            if(R_IsAllowedDecoration(def, mat, hasExt))
                return def;
        }
    }

    return 0;
}

ded_reflection_t* Def_GetReflection(material_t* mat, boolean hasExt)
{
    int                 i;
    ded_reflection_t*   def;

    for(i = defs.count.reflections.num - 1, def = defs.reflections + i;
        i >= 0; i--, def--)
    {
        material_t*         defMat = Materials_ToMaterial(
            Materials_IndexForName(def->material.mnamespace, def->material.name));

        if(mat == defMat)
        {
            // Is this suitable?
            if(R_IsAllowedReflection(def, mat, hasExt))
                return def;
        }
    }

    return NULL;
}

ded_detailtexture_t* Def_GetDetailTex(material_t* mat, boolean hasExt)
{
    int                 i;
    ded_detailtexture_t* def;

    // Search through the assignments.
    for(i = defs.count.details.num - 1, def = defs.details + i;
        i >= 0; i--, def--)
    {
        material_t*         defMat = Materials_ToMaterial(
            Materials_IndexForName(def->material1.mnamespace, def->material1.name));

        if(mat == defMat)
        {
            // Is this sutiable?
            if(R_IsAllowedDetailTex(def, mat, hasExt))
                return def;
        }

        defMat = Materials_ToMaterial(
            Materials_IndexForName(def->material2.mnamespace, def->material2.name));

        if(mat == defMat)
        {
            // Is this sutiable?
            if(R_IsAllowedDetailTex(def, mat, hasExt))
                return def;
        }
    }

    return NULL;
}

ded_generator_t* Def_GetGenerator(material_t* mat, boolean hasExt)
{
    ded_generator_t*       def;
    int                 i;

    // The generator will be determined now.
    for(i = 0, def = defs.generators; i < defs.count.generators.num; ++i, def++)
    {
        material_t*         defMat;

        if(!(defMat = Materials_ToMaterial(
                Materials_IndexForName(def->material.mnamespace, def->material.name))))
            continue;

        if(def->flags & PGF_GROUP)
        {   // Generator triggered by all materials in the (animation) group.
            /**
             * A search is necessary only if we know both the used material and
             * the specified material in this definition are in *a* group.
             */
            if(defMat->inAnimGroup && mat->inAnimGroup)
            {
                int                 g, numGroups = Materials_NumGroups();

                for(g = 0; g < numGroups; ++g)
                {
                    if(Materials_IsInGroup(g, defMat) && Materials_IsInGroup(g, mat))
                    {
                        if(Materials_CacheGroup(g))
                            continue; // Precache groups don't apply.

                        // Both are in this group! This def will do.
                        return def;
                    }
                }
            }
        }

        if(mat == defMat)
            return def;
    }

    return NULL; // Not found.
}

ded_generator_t* Def_GetDamageGenerator(int mobjType)
{
    int i;
    ded_generator_t* def;

    // Search for a suitable definition.
    for(i = 0, def = defs.generators; i < defs.count.generators.num; ++i, def++)
    {
        // It must be for this type of mobj.
        if(def->damageNum == mobjType)
            return def;
    }

    return NULL;
}

ded_xgclass_t* Def_GetXGClass(const char* name)
{
    ded_xgclass_t*      def;
    int                 i;

    if(!name || !name[0])
        return 0;

    for(i = defs.count.xgClasses.num - 1, def = defs.xgClasses + i;
        i >= 0; i--, def--)
    {
        if(!(stricmp(name, def->id)))
            return def;
    }

    return 0;
}

int Def_GetFlagValue(const char* flag)
{
    int                 i;

    for(i = defs.count.flags.num - 1; i >= 0; i--)
        if(!stricmp(defs.flags[i].id, flag))
            return defs.flags[i].value;

    Con_Message("Def_GetFlagValue: Undefined flag '%s'.\n", flag);
    return 0;
}

/**
 * Attempts to retrieve a flag by its prefix and value.
 * Returns a ptr to the text string of the first flag it
 * finds that matches the criteria, else NULL.
 */
const char* Def_GetFlagTextByPrefixVal(const char* prefix, int val)
{
    int                 i;

    for(i = defs.count.flags.num - 1; i >= 0; i--)
        if(strnicmp(defs.flags[i].id, prefix, sizeof(prefix)) == 0 &&
            defs.flags[i].value == val)
            return defs.flags[i].text;

    return NULL;
}

int Def_EvalFlags(char* ptr)
{
    int                 value = 0, len;
    char                buf[64];

    while(*ptr)
    {
        ptr = M_SkipWhite(ptr);
        len = M_FindWhite(ptr) - ptr;
        strncpy(buf, ptr, len);
        buf[len] = 0;
        value |= Def_GetFlagValue(buf);
        ptr += len;
    }
    return value;
}

int Def_GetTextNumForName(const char* name)
{
    int                 i;

    if(!name[0])
        return -1;

    for(i = defs.count.text.num -1; i >= 0; --i)
        if(!(strcmp(defs.text[i].id, name)))
            return i;

    return -1;
}

/**
 * Escape sequences are un-escaped (\n, \r, \t, \s, \_).
 */
void Def_InitTextDef(ddtext_t* txt, char* str)
{
    char*               out, *in;

    if(!str)
        str = ""; // Handle null pointers with "".
    txt->text = M_Calloc(strlen(str) + 1);
    for(out = txt->text, in = str; *in; out++, in++)
    {
        if(*in == '\\')
        {
            in++;
            if(*in == 'n')
                *out = '\n'; // Newline.
            else if(*in == 'r')
                *out = '\r'; // Carriage return.
            else if(*in == 't')
                *out = '\t'; // Tab.
            else if(*in == '_' || *in == 's')
                *out = ' '; // Space.
            else
                *out = *in;
            continue;
        }
        *out = *in;
    }
    // Adjust buffer to fix exactly.
    txt->text = M_Realloc(txt->text, strlen(txt->text) + 1);
}

/**
 * Callback for DD_ReadProcessDED.
 */
int Def_ReadDEDFile(const char* fn, filetype_t type, void* parm)
{
    // Skip directories.
    if(type == FT_DIRECTORY)
        return true;

    if(M_CheckFileID(fn))
    {
        if(!DED_Read(&defs, fn))
        {
            // Damn.
            Con_Error("Def_ReadDEDFile: %s\n", dedReadError);
        }
        else if(verbose)
        {
            Con_Message("DED done: %s\n", M_PrettyPath(fn));
        }
    }

    // Continue processing files.
    return true;
}

void Def_ReadProcessDED(const char* fileName)
{
    filename_t          fn, fullFn;
    directory_t         dir;

    memset(&dir, 0, sizeof(dir));

    Dir_FileName(fn, fileName, FILENAME_T_MAXLEN);

    // We want an absolute path.
    if(!Dir_IsAbsolute(fileName))
    {
        Dir_FileDir(fileName, &dir);
        sprintf(fullFn, "%s%s", dir.path, fn);
    }
    else
    {
        strncpy(fullFn, fileName, FILENAME_T_MAXLEN);
    }

    if(strchr(fn, '*') || strchr(fn, '?'))
    {
        // Wildcard search.
        F_ForAll(fullFn, 0, Def_ReadDEDFile);
    }
    else
    {
        Def_ReadDEDFile(fullFn, FT_NORMAL, 0);
    }
}

/**
 * Prints a count with a 2-space indentation.
 */
void Def_CountMsg(int count, const char* label)
{
    if(!verbose && !count)
        return; // Don't print zeros if not verbose.

    Con_Message("%5i %s\n", count, label);
}

/**
 * Reads all DD_DEFNS lumps found in the lumpInfo.
 */
void Def_ReadLumpDefs(void)
{
    int                 i, c;

    for(i = 0, c = 0; i < numLumps; ++i)
        if(!strnicmp(W_LumpName(i), "DD_DEFNS", 8))
        {
            c++;
            if(!DED_ReadLump(&defs, i))
            {
                Con_Error("DD_ReadLumpDefs: Parse error when reading "
                          "DD_DEFNS from\n  %s.\n", W_LumpSourceFile(i));
            }
        }

    if(c || verbose)
    {
        Con_Message("ReadLumpDefs: %i definition lump%s read.\n", c,
                    c != 1 ? "s" : "");
    }
}

/**
 * Uses gettingFor. Initializes the state-owners information.
 */
int Def_StateForMobj(const char* state)
{
    int                 num = Def_GetStateNum(state);
    int                 st, count = 16;

    if(num < 0)
        num = 0;

    // State zero is the NULL state.
    if(num > 0)
    {
        stateOwners[num] = gettingFor;
        // Scan forward at most 'count' states, or until we hit a state with
        // an owner, or the NULL state.
        for(st = states[num].nextState; st > 0 && count-- && !stateOwners[st];
            st = states[st].nextState)
        {
            stateOwners[st] = gettingFor;
        }
    }

    return num;
}

int Def_GetIntValue(char* val, int* returned_val)
{
    char               *data;

    // First look for a DED Value
    if(Def_Get(DD_DEF_VALUE, val, &data))
    {
        *returned_val = strtol(data, 0, 0);
        return true;
    }

    // Convert the literal string
    *returned_val = strtol(val, 0, 0);
    return false;
}

static void readDefs(void)
{
    int                 i;
    float               starttime = Sys_GetSeconds();

    for(i = 0; dedFiles[i]; ++i)
    {
        Con_Message("Reading definition file: %s\n", M_PrettyPath(dedFiles[i]));
        Def_ReadProcessDED(dedFiles[i]);
    }

    // Read definitions from WAD files.
    Def_ReadLumpDefs();

    VERBOSE(Con_Message("Def_ReadDefs: Done in %.2f seconds.\n",
                        Sys_GetSeconds() - starttime));
}

static void processMaterialDefinition(const ded_material_t* def)
{
    byte                i;
    materialnum_t       oldNum;

    // Are we patching an existing material or creating a new?
    oldNum = Materials_CheckIndexForName(def->id.mnamespace, def->id.name);

    if(oldNum)
    {   // Patching an existing material.
        material_t*         mat = Materials_ToMaterial(oldNum);

        if(def->width > 0)
            Material_SetWidth(mat, def->width);
        if(def->height > 0)
            Material_SetHeight(mat, def->height);
        Material_SetFlags(mat, def->flags);

        for(i = 0; i < DED_MAX_MATERIAL_LAYERS; ++i)
        {
            const ded_material_layer_t* layer = &def->layers[i];
            const ded_material_layer_stage_t* stage = &layer->stages[0];
            const gltexture_t*  tex = NULL;

            if(!layer->stageCount.num)
                break;

            if(!(tex = GL_GetGLTextureByName(stage->name, stage->type)))
            {
                Con_Message("Def_Read: Warning, unknown %s '%s' in material "
                            "'%s' (layer %i stage %i).\n",
                            GLTEXTURE_TYPE_STRING(stage->type), stage->name,
                            def->id.name, 0, 0);
            }

            Material_SetLayerTexture(mat, i, tex? tex->id : 0);
            Material_SetLayerTextureOriginXY(mat, i, stage->origin);
            Material_SetLayerMoveAngle(mat, i, stage->moveAngle);
            Material_SetLayerMoveSpeed(mat, i, stage->moveSpeed);
            //Material_SetLayerFlags(mat, i, stage->flags);
        }
    }
    else
    {   // An entirely new material.
        material_t*         mat;

        if(!(mat = Materials_NewMaterial(def->id.mnamespace, def->id.name, def->width,
                                    def->height, def->flags, def, false)))
        {
            Con_Message("Def_Read: Warning, failed creating material '%s' "
                        "in namespace %i.\n", def->id.name, def->id.mnamespace);
            return;
        }

        for(i = 0; i < DED_MAX_MATERIAL_LAYERS; ++i)
        {
            const ded_material_layer_t* layer = &def->layers[i];
            const ded_material_layer_stage_t* stage = &layer->stages[0];
            const gltexture_t*  tex = NULL;

            if(stage->type == -1) // Unused.
                break;

            if(!(tex = GL_GetGLTextureByName(stage->name, stage->type)))
            {
                Con_Message("Def_Read: Warning, unknown %s '%s' in "
                            "material '%s' (layer %i stage %i).\n",
                            GLTEXTURE_TYPE_STRING(stage->type), stage->name,
                            def->id.name, 0, 0);
            }

            Material_AddLayer(mat, 0, tex? tex->id : 0, stage->origin[0],
                              stage->origin[1], stage->moveAngle,
                              stage->moveSpeed);
        }
    }
}

/**
 * Reads the specified definition files, and creates the sprite name,
 * state, mobjinfo, sound, music, text and mapinfo databases accordingly.
 */
void Def_Read(void)
{
    int                 i, k;

    if(defsInited)
    {
        // We've already initialized the definitions once.
        // Get rid of everything.
        R_ClearClassDataPath(DDRC_MODEL);
        Def_Destroy();
    }

    firstDED = true;

    // Clear all existing definitions.
    DED_Destroy(&defs);
    DED_Init(&defs);

    // Reset file IDs so previously seen files can be processed again.
    M_ResetFileIDs();

    // Read all definitions, files and lumps.
    readDefs();

    // Any definition hooks?
    Plug_DoHook(HOOK_DEFS, 0, &defs);

    // Check that enough defs were found.
    if(!defs.count.states.num || !defs.count.mobjs.num)
        Con_Error("DD_ReadDefs: No state or mobj definitions found!\n");

    Con_Message("Definitions:\n");

    // Sprite names.
    DED_NewEntries((void **) &sprNames, &countSprNames, sizeof(*sprNames),
                   defs.count.sprites.num);
    for(i = 0; i < countSprNames.num; ++i)
        strcpy(sprNames[i].name, defs.sprites[i].id);
    Def_CountMsg(countSprNames.num, "sprite names");

    // States.
    DED_NewEntries((void **) &states, &countStates, sizeof(*states),
                   defs.count.states.num);
    // Zero the parallel LUTs to the states array. These will be re-inited
    // anyway so there is no need to worry about updating any old values.
    stateGeneratorDefs =
        M_Realloc(stateGeneratorDefs, sizeof(*stateGeneratorDefs) * countStates.num);
    memset(stateGeneratorDefs, 0, sizeof(*stateGeneratorDefs) * countStates.num);
    stateLights =
        M_Realloc(stateLights, sizeof(*stateLights) * countStates.num);
    memset(stateLights, 0, sizeof(*stateLights) * countStates.num);

    for(i = 0; i < countStates.num; ++i)
    {
        ded_state_t*        dstNew, *dst = &defs.states[i];
        // Make sure duplicate IDs overwrite the earliest.
        int                 stateNum = Def_GetStateNum(dst->id);
        state_t*            st;

        if(stateNum == -1)
            continue;

        dstNew = defs.states + stateNum;
        st = states + stateNum;
        st->sprite = Def_GetSpriteNum(dst->sprite.id);
        st->flags = dst->flags;
        st->frame = dst->frame;

        // Check for the old or'd in fullbright flag.
        if(st->frame & FF_FULLBRIGHT)
        {
            st->frame &= FF_FRAMEMASK;
            st->flags |= STF_FULLBRIGHT;
        }

        st->tics = dst->tics;
        st->action = Def_GetActionPtr(dst->action);
        st->nextState = Def_GetStateNum(dst->nextState);
        for(k = 0; k < NUM_STATE_MISC; ++k)
            st->misc[k] = dst->misc[k];

        // Replace the older execute string.
        if(dst != dstNew)
        {
            if(dstNew->execute)
                M_Free(dstNew->execute);
            dstNew->execute = dst->execute;
            dst->execute = NULL;
        }
    }
    Def_CountMsg(countStates.num, "states");

    DED_NewEntries((void **) &stateOwners, &countStateOwners,
                   sizeof(mobjinfo_t *), defs.count.states.num);

    // Mobj info.
    DED_NewEntries((void **) &mobjInfo, &countMobjInfo, sizeof(*mobjInfo),
                   defs.count.mobjs.num);
    for(i = 0; i < countMobjInfo.num; ++i)
    {
        ded_mobj_t*         dmo = &defs.mobjs[i];
        // Make sure duplicate defs overwrite the earliest.
        mobjinfo_t*         mo = &mobjInfo[Def_GetMobjNum(dmo->id)];

        gettingFor = mo;
        mo->doomEdNum = dmo->doomEdNum;
        mo->spawnHealth = dmo->spawnHealth;
        mo->reactionTime = dmo->reactionTime;
        mo->painChance = dmo->painChance;
        mo->speed = dmo->speed;
        mo->radius = dmo->radius;
        mo->height = dmo->height;
        mo->mass = dmo->mass;
        mo->damage = dmo->damage;
        mo->flags = dmo->flags[0];
        mo->flags2 = dmo->flags[1];
        mo->flags3 = dmo->flags[2];
        for(k = 0; k < NUM_STATE_NAMES; ++k)
        {
            mo->states[k] = Def_StateForMobj(dmo->states[k]);
        }
        mo->seeSound = Def_GetSoundNum(dmo->seeSound);
        mo->attackSound = Def_GetSoundNum(dmo->attackSound);
        mo->painSound = Def_GetSoundNum(dmo->painSound);
        mo->deathSound = Def_GetSoundNum(dmo->deathSound);
        mo->activeSound = Def_GetSoundNum(dmo->activeSound);
        for(k = 0; k < NUM_MOBJ_MISC; ++k)
            mo->misc[k] = dmo->misc[k];
    }
    Def_CountMsg(countMobjInfo.num, "things");
    Def_CountMsg(defs.count.models.num, "models");

    // Materials.
    for(i = 0; i < defs.count.materials.num; ++i)
    {
        processMaterialDefinition(&defs.materials[i]);
    }

    // Dynamic lights. Update the sprite numbers.
    for(i = 0; i < defs.count.lights.num; ++i)
    {
        k = Def_GetStateNum(defs.lights[i].state);
        if(k < 0)
        {
            // It's probably a bias light definition, then?
            if(!defs.lights[i].uniqueMapID[0])
            {
                Con_Message("Def_Read: Lights: Undefined state '%s'.\n",
                            defs.lights[i].state);
            }
            continue;
        }
        stateLights[k] = &defs.lights[i];
    }
    Def_CountMsg(defs.count.lights.num, "lights");

    // Sound effects.
    DED_NewEntries((void **) &sounds, &countSounds, sizeof(*sounds),
                   defs.count.sounds.num);
    for(i = 0; i < countSounds.num; ++i)
    {
        ded_sound_t*        snd = defs.sounds + i;
        // Make sure duplicate defs overwrite the earliest.
        sfxinfo_t*          si = sounds + Def_GetSoundNum(snd->id);

        strcpy(si->id, snd->id);
        strcpy(si->lumpName, snd->lumpName);
        si->lumpNum = W_CheckNumForName(snd->lumpName);
        strcpy(si->name, snd->name);
        k = Def_GetSoundNum(snd->link);
        si->link = (k >= 0 ? sounds + k : NULL);
        si->linkPitch = snd->linkPitch;
        si->linkVolume = snd->linkVolume;
        si->priority = snd->priority;
        si->channels = snd->channels;
        si->flags = snd->flags;
        si->group = snd->group;
        strcpy(si->external, snd->ext.path);
    }
    Def_CountMsg(countSounds.num, "sound effects");

    // Music.
    for(i = 0; i < defs.count.music.num; ++i)
    {
        ded_music_t*        mus = defs.music + i;
        // Make sure duplicate defs overwrite the earliest.
        ded_music_t*        earliest = defs.music + Def_GetMusicNum(mus->id);

        if(earliest == mus)
            continue;

        strcpy(earliest->lumpName, mus->lumpName);
        strcpy(earliest->path.path, mus->path.path);
        earliest->cdTrack = mus->cdTrack;
    }
    Def_CountMsg(defs.count.music.num, "songs");

    // Text.
    DED_NewEntries((void **) &texts, &countTexts, sizeof(*texts),
                   defs.count.text.num);

    for(i = 0; i < countTexts.num; ++i)
        Def_InitTextDef(texts + i, defs.text[i].text);

    // Handle duplicate strings.
    for(i = 0; i < countTexts.num; ++i)
    {
        if(!texts[i].text)
            continue;

        for(k = i + 1; k < countTexts.num; ++k)
            if(!strcmp(defs.text[i].id, defs.text[k].id) && texts[k].text)
            {
                // Update the earlier string.
                texts[i].text =
                    M_Realloc(texts[i].text, strlen(texts[k].text) + 1);
                strcpy(texts[i].text, texts[k].text);

                // Free the later string, it isn't used (>NUMTEXT).
                M_Free(texts[k].text);
                texts[k].text = 0;
            }
    }
    Def_CountMsg(countTexts.num, "text strings");

    // Particle generators.
    for(i = 0; i < defs.count.generators.num; ++i)
    {
        ded_generator_t* pg = &defs.generators[i];
        int st = Def_GetStateNum(pg->state);

        pg->typeNum = Def_GetMobjNum(pg->type);
        pg->type2Num = Def_GetMobjNum(pg->type2);
        pg->damageNum = Def_GetMobjNum(pg->damage);

        // Figure out embedded sound ID numbers.
        for(k = 0; k < pg->stageCount.num; ++k)
        {
            if(pg->stages[k].sound.name[0])
            {
                pg->stages[k].sound.id =
                    Def_GetSoundNum(pg->stages[k].sound.name);
            }
            if(pg->stages[k].hitSound.name[0])
            {
                pg->stages[k].hitSound.id =
                    Def_GetSoundNum(pg->stages[k].hitSound.name);
            }
        }

        if(st <= 0)
            continue; // Not state triggered, then...

        // Link the definition to the state.
        if(pg->flags & PGF_STATE_CHAIN)
        {
            // Add to the chain.
            pg->stateNext = stateGeneratorDefs[st];
            stateGeneratorDefs[st] = pg;
        }
        else
        {
            // Make sure the previously built list is unlinked.
            while(stateGeneratorDefs[st])
            {
                ded_generator_t*       temp = stateGeneratorDefs[st]->stateNext;

                stateGeneratorDefs[st]->stateNext = NULL;
                stateGeneratorDefs[st] = temp;
            }
            stateGeneratorDefs[st] = pg;
            pg->stateNext = NULL;
        }
    }
    Def_CountMsg(defs.count.generators.num, "particle generators");

    // Detail textures. Initialize later...
    Def_CountMsg(defs.count.details.num, "detail textures");

    // Texture animation groups.
    Def_CountMsg(defs.count.groups.num, "animation groups");

    // Surface decorations.
    Def_CountMsg(defs.count.decorations.num, "surface decorations");

    // Surface reflections.
    Def_CountMsg(defs.count.reflections.num, "surface reflections");

    // Materials.
    Def_CountMsg(defs.count.materials.num, "surface materials");

    // Map infos.
    for(i = 0; i < defs.count.mapInfo.num; ++i)
    {
        ded_mapinfo_t*      mi = &defs.mapInfo[i];

        /**
         * Historically, the map info flags field was used for sky flags,
         * here we copy those flags to the embedded sky definition for
         * backward-compatibility.
         */
        if(mi->flags & MIF_DRAW_SPHERE)
            mi->sky.flags |= SIF_DRAW_SPHERE;
    }
    Def_CountMsg(defs.count.mapInfo.num, "map infos");

    // Other data:
    Def_CountMsg(defs.count.skies.num, "skies");
    Def_CountMsg(defs.count.finales.num, "finales");

    Def_CountMsg(defs.count.lineTypes.num, "line types");
    Def_CountMsg(defs.count.sectorTypes.num, "sector types");

    // Init the base model search path (prepend).
    Dir_ValidDir(defs.modelPath, FILENAME_T_MAXLEN);
    R_AddClassDataPath(DDRC_MODEL, defs.modelPath, false);

    // Model search path specified on the command line?
    if(ArgCheckWith("-modeldir", 1))
    {
        filename_t          path;

        strncpy(path, ArgNext(), FILENAME_T_MAXLEN);
        Dir_ValidDir(path, FILENAME_T_MAXLEN);

        // Prepend to the search list; takes precedence.
        R_AddClassDataPath(DDRC_MODEL, path, false);
    }
    if(ArgCheckWith("-modeldir2", 1))
    {
        filename_t          path;

        strncpy(path, ArgNext(), FILENAME_T_MAXLEN);
        Dir_ValidDir(path, FILENAME_T_MAXLEN);

        // Prepend to the search list; takes precedence.
        R_AddClassDataPath(DDRC_MODEL, ArgNext(), false);
    }

    defsInited = true;
}

/**
 * Initialize definitions that must be initialized when engine init is
 * complete (called from R_Init).
 */
void Def_PostInit(void)
{
    int                 i, k;
    ded_generator_t*    gen;
    char                name[40];
    modeldef_t*         modef;
    ded_ptcstage_t*     st;

    // Particle generators: model setup.
    for(i = 0, gen = defs.generators; i < defs.count.generators.num; ++i, gen++)
    {
        for(k = 0, st = gen->stages; k < gen->stageCount.num; ++k, st++)
        {
            if(st->type < PTC_MODEL || st->type >= PTC_MODEL + MAX_PTC_MODELS)
                continue;

            sprintf(name, "Particle%02i", st->type - PTC_MODEL);
            if(!(modef = R_CheckIDModelFor(name)) || modef->sub[0].model <= 0)
            {
                st->model = -1;
                continue;
            }

            st->model = modef - modefs;
            st->frame =
                R_ModelFrameNumForName(modef->sub[0].model, st->frameName);
            if(st->endFrameName[0])
            {
                st->endFrame =
                    R_ModelFrameNumForName(modef->sub[0].model,
                                           st->endFrameName);
            }
            else
            {
                st->endFrame = -1;
            }
        }
    }

    // Detail textures.
    GL_DeleteAllTexturesForGLTextures(GLT_DETAIL);
    R_DestroyDetailTextures();
    for(i = 0; i < defs.count.details.num; ++i)
    {
        R_CreateDetailTexture(&defs.details[i]);
    }

    // Lightmaps and flare textures.
    GL_DeleteAllTexturesForGLTextures(GLT_LIGHTMAP);
    GL_DeleteAllTexturesForGLTextures(GLT_FLARE);
    R_DestroyLightMaps();
    R_DestroyFlareTextures();
    for(i = 0; i < defs.count.lights.num; ++i)
    {
        ded_light_t*        lig = &defs.lights[i];

        R_CreateLightMap(&lig->up);
        R_CreateLightMap(&lig->down);
        R_CreateLightMap(&lig->sides);

        R_CreateFlareTexture(&lig->flare);
    }

    for(i = 0; i < defs.count.decorations.num; ++i)
    {
        ded_decor_t*        decor = &defs.decorations[i];

        for(k = 0; k < DED_DECOR_NUM_LIGHTS; ++k)
        {
            ded_decorlight_t*   lig = &decor->lights[k];

            if(!R_IsValidLightDecoration(lig))
                break;

            R_CreateLightMap(&lig->up);
            R_CreateLightMap(&lig->down);
            R_CreateLightMap(&lig->sides);

            R_CreateFlareTexture(&lig->flare);
        }
    }

    // Surface reflections.
    GL_DeleteAllTexturesForGLTextures(GLT_SHINY);
    GL_DeleteAllTexturesForGLTextures(GLT_MASK);
    R_DestroyShinyTextures();
    R_DestroyMaskTextures();
    for(i = 0; i < defs.count.reflections.num; ++i)
    {
        ded_reflection_t* ref = &defs.reflections[i];

        if(ref->shinyMap.path[0])
            R_CreateShinyTexture(ref);

        if(ref->maskMap.path[0])
            R_CreateMaskTexture(ref);
    }

    // Animation groups.
    Materials_DestroyGroups();
    for(i = 0; i < defs.count.groups.num; ++i)
    {
        Materials_NewGroupFromDefiniton(&defs.groups[i]);
    }
}

/**
 * Can we reach 'snew' if we start searching from 'sold'?
 * Take a maximum of 16 steps.
 */
boolean Def_SameStateSequence(state_t* snew, state_t* sold)
{
    int                 it, target = snew - states, start = sold - states;
    int                 count = 0;

    if(!snew || !sold)
        return false;

    if(snew == sold)
        return true; // Trivial.

    for(it = sold->nextState; it >= 0 && it != start && count < 16;
        it = states[it].nextState, ++count)
    {
        if(it == target)
            return true;

        if(it == states[it].nextState)
            break;
    }
    return false;
}

static int Friendly(int num)
{
    if(num < 0)
        num = 0;
    return num;
}

/**
 * Converts a DED line type to the internal format.
 * Bit of a nuisance really...
 */
void Def_CopyLineType(linetype_t* l, ded_linetype_t* def)
{
    int                 i, k, a, temp;

    l->id = def->id;
    l->flags = def->flags[0];
    l->flags2 = def->flags[1];
    l->flags3 = def->flags[2];
    l->lineClass = def->lineClass;
    l->actType = def->actType;
    l->actCount = def->actCount;
    l->actTime = def->actTime;
    l->actTag = def->actTag;

    for(i = 0; i < 10; ++i)
    {
        if(i == 9)
            l->aparm[i] = Def_GetMobjNum(def->aparm9);
        else
            l->aparm[i] = def->aparm[i];
    }

    l->tickerStart = def->tickerStart;
    l->tickerEnd = def->tickerEnd;
    l->tickerInterval = def->tickerInterval;
    l->actSound = Friendly(Def_GetSoundNum(def->actSound));
    l->deactSound = Friendly(Def_GetSoundNum(def->deactSound));
    l->evChain = def->evChain;
    l->actChain = def->actChain;
    l->deactChain = def->deactChain;
    l->actLineType = def->actLineType;
    l->deactLineType = def->deactLineType;
    l->wallSection = def->wallSection;
    l->actMaterial = Materials_CheckIndexForName(def->actMaterial.mnamespace,
                                                 def->actMaterial.name);
    l->deactMaterial = Materials_CheckIndexForName(def->deactMaterial.mnamespace,
                                                   def->deactMaterial.name);
    l->actMsg = def->actMsg;
    l->deactMsg = def->deactMsg;
    l->materialMoveAngle = def->materialMoveAngle;
    l->materialMoveSpeed = def->materialMoveSpeed;
    memcpy(l->iparm, def->iparm, sizeof(int) * 20);
    memcpy(l->fparm, def->fparm, sizeof(int) * 20);
    LOOPi(5) l->sparm[i] = def->sparm[i];

    // Some of the parameters might be strings depending on the line class.
    // Find the right mapping table.
    for(k = 0; k < 20; ++k)
    {
        temp = 0;
        a = xgClassLinks[l->lineClass].iparm[k].map;
        if(a < 0)
            continue;

        if(a & MAP_SND)
        {
            l->iparm[k] = Friendly(Def_GetSoundNum(def->iparmStr[k]));
        }
        else if(a & MAP_MATERIAL)
        {
            if(def->iparmStr[k][0])
            {
                if(!stricmp(def->iparmStr[k], "-1"))
                    l->iparm[k] = -1;
                else
                    l->iparm[k] =
                        Materials_CheckIndexForName(MN_ANY, def->iparmStr[k]);
            }
        }
        else if(a & MAP_MUS)
        {
            temp = Friendly(Def_GetMusicNum(def->iparmStr[k]));

            if(temp == 0)
            {
                temp = Def_EvalFlags(def->iparmStr[k]);
                if(temp)
                    l->iparm[k] = temp;
            } else
                l->iparm[k] = Friendly(Def_GetMusicNum(def->iparmStr[k]));
        }
        else
        {
            temp = Def_EvalFlags(def->iparmStr[k]);

            if(temp)
                l->iparm[k] = temp;
        }
    }
}

/**
 * Converts a DED sector type to the internal format.
 */
void Def_CopySectorType(sectortype_t* s, ded_sectortype_t* def)
{
    int                 i, k;

    s->id = def->id;
    s->flags = def->flags;
    s->actTag = def->actTag;
    LOOPi(5)
    {
        s->chain[i] = def->chain[i];
        s->chainFlags[i] = def->chainFlags[i];
        s->start[i] = def->start[i];
        s->end[i] = def->end[i];
        LOOPk(2) s->interval[i][k] = def->interval[i][k];
        s->count[i] = def->count[i];
    }
    s->ambientSound = Friendly(Def_GetSoundNum(def->ambientSound));
    LOOPi(2)
    {
        s->soundInterval[i] = def->soundInterval[i];
        s->materialMoveAngle[i] = def->materialMoveAngle[i];
        s->materialMoveSpeed[i] = def->materialMoveSpeed[i];
    }
    s->windAngle = def->windAngle;
    s->windSpeed = def->windSpeed;
    s->verticalWind = def->verticalWind;
    s->gravity = def->gravity;
    s->friction = def->friction;
    s->lightFunc = def->lightFunc;
    LOOPi(2) s->lightInterval[i] = def->lightInterval[i];
    LOOPi(3)
    {
        s->colFunc[i] = def->colFunc[i];
        LOOPk(2) s->colInterval[i][k] = def->colInterval[i][k];
    }
    s->floorFunc = def->floorFunc;
    s->floorMul = def->floorMul;
    s->floorOff = def->floorOff;
    LOOPi(2) s->floorInterval[i] = def->floorInterval[i];
    s->ceilFunc = def->ceilFunc;
    s->ceilMul = def->ceilMul;
    s->ceilOff = def->ceilOff;
    LOOPi(2) s->ceilInterval[i] = def->ceilInterval[i];
}

/**
 * @return              @c true, if the definition was found.
 */
int Def_Get(int type, const char* id, void* out)
{
    int                 i;
    ded_mapinfo_t*      map;
    ddmapinfo_t*        mout;
    finalescript_t*     fin;

    switch(type)
    {
    case DD_DEF_MOBJ:
        return Def_GetMobjNum(id);

    case DD_DEF_MOBJ_BY_NAME:
        return Def_GetMobjNumForName(id);

    case DD_DEF_STATE:
        return Def_GetStateNum(id);

    case DD_DEF_SPRITE:
        return Def_GetSpriteNum(id);

    case DD_DEF_SOUND:
        return Def_GetSoundNum(id);

    case DD_DEF_SOUND_BY_NAME:
        return Def_GetSoundNumForName(id);

    case DD_DEF_SOUND_LUMPNAME:
        i = *((long*) id);
        if(i < 0 || i >= countSounds.num)
            return false;
        strcpy(out, sounds[i].lumpName);
        break;

    case DD_DEF_MUSIC:
        return Def_GetMusicNum(id);

    case DD_DEF_MAP_INFO:
        map = Def_GetMapInfo(id);
        if(!map)
            return false;

        mout = (ddmapinfo_t *) out;
        mout->name = map->name;
        mout->author = map->author;
        mout->music = Def_GetMusicNum(map->music);
        mout->flags = map->flags;
        mout->ambient = map->ambient;
        mout->gravity = map->gravity;
        mout->parTime = map->parTime;
        break;

    case DD_DEF_TEXT:
        if(id && id[0])
        {
            int                 i;

            // Read backwards to allow patching.
            for(i = defs.count.text.num - 1; i >= 0; i--)
                if(!stricmp(defs.text[i].id, id))
                {
                    // \fixme: should be returning an immutable ptr.
                    if(out)
                        *(char **) out = defs.text[i].text;
                    return i;
                }
        }
        return -1;

    case DD_DEF_VALUE:
        // Read backwards to allow patching.
        for(i = defs.count.values.num - 1; i >= 0; i--)
            if(!stricmp(defs.values[i].id, id))
            {
                if(out)
                    *(char **) out = defs.values[i].text;
                return true;
            }
        return false;

    case DD_DEF_FINALE:
        // Find InFine script by ID.
        for(i = defs.count.finales.num - 1; i >= 0; i--)
            if(!stricmp(defs.finales[i].id, id))
            {
                // This has a matching ID. Return a pointer to the script.
                *(void **) out = defs.finales[i].script;
                return true;
            }
        return false;

    case DD_DEF_FINALE_BEFORE:
        fin = (finalescript_t *) out;
        for(i = defs.count.finales.num - 1; i >= 0; i--)
            if(!stricmp(defs.finales[i].before, id))
            {
                fin->before = defs.finales[i].before;
                fin->after = defs.finales[i].after;
                fin->script = defs.finales[i].script;
                return true;
            }
        return false;

    case DD_DEF_FINALE_AFTER:
        fin = (finalescript_t *) out;
        for(i = defs.count.finales.num - 1; i >= 0; i--)
            if(!stricmp(defs.finales[i].after, id))
            {

                fin->before = defs.finales[i].before;
                fin->after = defs.finales[i].after;
                fin->script = defs.finales[i].script;
                return true;
            }
        return false;

    case DD_DEF_LINE_TYPE:
        {
        int                 typeId = strtol(id, (char **)NULL, 10);

        for(i = defs.count.lineTypes.num - 1; i >= 0; i--)
            if(defs.lineTypes[i].id == typeId)
            {
                if(out)
                    Def_CopyLineType(out, &defs.lineTypes[i]);
                return true;
            }
        }
        return false;

    case DD_DEF_SECTOR_TYPE:
        {
        int                 typeId = strtol(id, (char **)NULL, 10);

        for(i = defs.count.sectorTypes.num - 1; i >= 0; i--)
            if(defs.sectorTypes[i].id == typeId)
            {
                if(out)
                    Def_CopySectorType(out, &defs.sectorTypes[i]);
                return true;
            }
        }
        return false;

    default:
        return false;
    }
    return true;
}

/**
 * This is supposed to be the main interface for outside parties to
 * modify definitions (unless they want to do it manually with dedfile.h).
 */
int Def_Set(int type, int index, int value, const void* ptr)
{
    int                 i;
    ded_music_t*        musdef = 0;

    switch(type)
    {
    case DD_DEF_TEXT:
        if(index < 0 || index >= defs.count.text.num)
            Con_Error("Def_Set: Text index %i is invalid.\n", index);

        defs.text[index].text = M_Realloc(defs.text[index].text,
            strlen((char*)ptr) + 1);
        strcpy(defs.text[index].text, ptr);
        break;

    case DD_DEF_STATE:
        {
        ded_state_t* stateDef;

        if(index < 0 || index >= defs.count.states.num)
            Con_Error("Def_Set: State index %i is invalid.\n", index);

        stateDef = &defs.states[index];
        switch(value)
        {
        case DD_SPRITE:
            {
            int sprite = *(int*) ptr;

            if(sprite < 0 || sprite >= defs.count.sprites.num)
            {
                Con_Message("Def_Set: Warning, invalid sprite index %i.\n",
                            sprite);
                break;
            }

            strcpy((char*) stateDef->sprite.id, defs.sprites[value].id);
            break;
            }
        case DD_FRAME:
            {
            int frame = *(int*) ptr;

            if(frame & FF_FULLBRIGHT)
                stateDef->flags |= STF_FULLBRIGHT;
            else
                stateDef->flags &= ~STF_FULLBRIGHT;
            stateDef->frame = frame & ~FF_FULLBRIGHT;
            break;
            }
        default:
            break;
        }
        break;
        }
    case DD_DEF_SOUND:
        if(index < 0 || index >= countSounds.num)
            Con_Error("Def_Set: Sound index %i is invalid.\n", index);

        switch(value)
        {
        case DD_LUMP:
            S_StopSound(index, 0);
            strcpy(sounds[index].lumpName, ptr);
            sounds[index].lumpNum = W_CheckNumForName(sounds[index].lumpName);
            break;

        default:
            break;
        }
        break;

    case DD_DEF_MUSIC:
        if(index == DD_NEW)
        {
            // We should create a new music definition.
            i = DED_AddMusic(&defs, "");    // No ID is known at this stage.
            musdef = defs.music + i;
        }
        else if(index >= 0 && index < defs.count.music.num)
        {
            musdef = defs.music + index;
        }
        else
            Con_Error("Def_Set: Music index %i is invalid.\n", index);

        // Which key to set?
        switch(value)
        {
        case DD_ID:
            if(ptr)
                strcpy(musdef->id, ptr);
            break;

        case DD_LUMP:
            if(ptr)
                strcpy(musdef->lumpName, ptr);
            break;

        case DD_CD_TRACK:
            musdef->cdTrack = *(int *) ptr;
            break;

        default:
            break;
        }

        // If the def was just created, return its index.
        if(index == DD_NEW)
            return musdef - defs.music;
        break;

    default:
        return false;
    }
    return true;
}

/**
 * Prints a list of all the registered mobjs to the console.
 * \fixme Does this belong here?
 */
D_CMD(ListMobjs)
{
    int                 i;

    Con_Printf("Registered Mobjs (ID | Name):\n");

    for(i = 0; i < defs.count.mobjs.num; ++i)
    {
        if(defs.mobjs[i].name[0])
            Con_Printf(" %s | %s\n", defs.mobjs[i].id, defs.mobjs[i].name);
        else
            Con_Printf(" %s | (Unnamed)\n", defs.mobjs[i].id);
    }

    return true;
}
