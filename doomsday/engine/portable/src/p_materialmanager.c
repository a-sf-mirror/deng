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
 * materials.c: Material collection.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h> // For tolower()

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_audio.h" // For texture, environmental audio properties.

// MACROS ------------------------------------------------------------------

#define MATERIALS_BLOCK_ALLOC (32) // Num materials to allocate per block.
#define MATERIAL_NAME_HASH_SIZE (512)

// TYPES -------------------------------------------------------------------

typedef struct materialbind_s {
    char            name[9];
    material_t*     mat;

    uint            hashNext; // 1-based index
} materialbind_t;

typedef struct animframe_s {
    material_t*     mat;
    ushort          tics;
    ushort          random;
} animframe_t;

typedef struct animgroup_s {
    int             id;
    int             flags;
    int             index;
    int             maxTimer;
    int             timer;
    int             count;
    animframe_t*    frames;
} animgroup_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(ListMaterials);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void animateGroups(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean ddMapSetup;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

materialnum_t numMaterialBinds = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initedOk = false;

/**
 * The following data structures and variables are intrinsically linked and
 * are inter-dependant. The scheme used is somewhat complicated due to the
 * required traits of the materials themselves and in of the system itself:
 *
 * 1) Pointers to material_t are eternal, they are always valid and continue
 *    to reference the same logical material data even after engine reset.
 * 2) Public material identifiers (materialnum_t) are similarly eternal.
 *    Note that they are used to index the material name bindings array.
 * 3) Dynamic creation/update of materials.
 * 4) Material name bindings are semi-independant from the materials. There
 *    may be multiple name bindings for a given material (aliases).
 *    The only requirement is that the name is unique among materials in
 *    a given material mnamespace.
 * 5) Super-fast look up by public material identifier.
 * 6) Fast look up by material name (a hashing scheme is used).
 */
static zblockset_t* materialsBlockSet;
static material_t* materialsHead; // Head of the linked list of materials.

static materialbind_t* materialBinds;
static materialnum_t maxMaterialBinds;
static uint hashTable[NUM_MATERIAL_NAMESPACES][MATERIAL_NAME_HASH_SIZE];

static int numgroups;
static animgroup_t* groups;

// CODE --------------------------------------------------------------------

void Materials_Register(void)
{
    C_CMD("listmaterials",  NULL,     ListMaterials);
}

/**
 * Is the specified id a valid (known) material namespace.
 *
 * \note The special case MN_ANY is considered invalid here as it does not
 *       relate to one specific mnamespace.
 *
 * @return              @c true, iff the id is valid.
 */
static boolean isKnownMNamespace(material_namespace_t mnamespace)
{
    if(mnamespace >= MN_FIRST && mnamespace < NUM_MATERIAL_NAMESPACES)
        return true;

    return false;
}

/**
 * This is a hash function. Given a material name it generates a
 * somewhat-random number between 0 and MATERIAL_NAME_HASH_SIZE.
 *
 * @return              The generated hash index.
 */
static uint hashForName(const char* name)
{
    ushort              key = 0;
    int                 i;

    // Stop when the name ends.
    for(i = 0; *name; ++i, name++)
    {
        if(i == 0)
            key ^= (int) (*name);
        else if(i == 1)
            key *= (int) (*name);
        else if(i == 2)
        {
            key -= (int) (*name);
            i = -1;
        }
    }

    return key % MATERIAL_NAME_HASH_SIZE;
}

/**
 * Given a name and namespace, search the materials db for a match.
 * \assume Caller knows what it's doing; params arn't validity checked.
 *
 * @param name          Name of the material to search for. Must have been
 *                      transformed to all lower case.
 * @param mnamespace    Specific MG_* material namespace NOT @c MN_ANY.
 * @return              Unique number of the found material, else zero.
 */
static materialnum_t getMaterialNumForName(const char* name, uint hash,
                                           material_namespace_t mnamespace)
{
    // Go through the candidates.
    if(hashTable[mnamespace][hash])
    {
        materialbind_t*     mb = &materialBinds[
            hashTable[mnamespace][hash] - 1];

        for(;;)
        {
            material_t*        mat;

            mat = mb->mat;

            if(!strncmp(mb->name, name, 8))
                return ((mb) - materialBinds) + 1;

            if(!mb->hashNext)
                break;

            mb = &materialBinds[mb->hashNext - 1];
        }
    }

    return 0; // Not found.
}

static material_t* createMaterial(short width, short height, byte flags,
                                  material_namespace_t mnamespace,
                                  const ded_material_t* def,
                                  boolean isAutoMaterial)
{
    material_t*         mat = Z_BlockNewElement(materialsBlockSet);

    memset(mat, 0, sizeof(*mat));

    mat->mnamespace = mnamespace;
    mat->width = width;
    mat->height = height;
    mat->flags = flags;
    mat->envClass = MEC_UNKNOWN;
    mat->current = mat->next = mat;
    mat->def = def;
    mat->isAutoMaterial = isAutoMaterial;

    // Link the new material into the global list of materials.
    mat->globalNext = materialsHead;
    materialsHead = mat;

    return mat;
}

static material_t* getMaterialByNum(materialnum_t num)
{
    if(num < numMaterialBinds)
        return materialBinds[num].mat;

    return NULL;
}

static void newMaterialNameBinding(material_t* mat, const char* name,
                                   material_namespace_t mnamespace,
                                   uint hash)
{
    materialbind_t*     mb;

    if(++numMaterialBinds > maxMaterialBinds)
    {   // Allocate more memory.
        maxMaterialBinds += MATERIALS_BLOCK_ALLOC;
        materialBinds =
            Z_Realloc(materialBinds, sizeof(*materialBinds) * maxMaterialBinds,
                      PU_STATIC);
    }

    // Add the new material to the end.
    mb = &materialBinds[numMaterialBinds - 1];
    strncpy(mb->name, name, 8);
    mb->name[8] = '\0';
    mb->mat = mat;

    // We also hash the name for faster searching.
    mb->hashNext = hashTable[mnamespace][hash];
    hashTable[mnamespace][hash] = (mb - materialBinds) + 1;
}

static animgroup_t* getGroup(int number)
{
    if(--number < 0 || number >= numgroups)
        return NULL;

    return &groups[number];
}

static boolean isInGroup(animgroup_t* group, const material_t* mat)
{
    int                 i;

    if(!mat || !group)
        return false;

    // Is it in there?
    for(i = 0; i < group->count; ++i)
    {
        animframe_t*        frame = &group->frames[i];

        if(frame->mat == mat)
            return true;
    }

    return false;
}

static void animateGroups(void)
{
    int                 i, timer, k;
    animgroup_t*        group;

    for(i = 0, group = groups; i < numgroups; ++i, group++)
    {
        // The Precache groups are not intended for animation.
        if((group->flags & AGF_PRECACHE) || !group->count)
            continue;

        if(--group->timer <= 0)
        {
            // Advance to next frame.
            group->index = (group->index + 1) % group->count;
            timer = (int) group->frames[group->index].tics;

            if(group->frames[group->index].random)
            {
                timer += (int) RNG_RandByte() % (group->frames[group->index].random + 1);
            }
            group->timer = group->maxTimer = timer;

            // Update translations.
            for(k = 0; k < group->count; ++k)
            {
                material_t*            real, *current, *next;

                real = group->frames[k].mat;
                current =
                    group->frames[(group->index + k) % group->count].mat;
                next =
                    group->frames[(group->index + k + 1) % group->count].mat;

                Material_SetTranslation(real, current, next, 0);

                // Just animate the first in the sequence?
                if(group->flags & AGF_FIRST_ONLY)
                    break;
            }
        }
        else
        {
            // Update the interpolation point of animated group members.
            for(k = 0; k < group->count; ++k)
            {
                material_t*            mat = group->frames[k].mat;

                if(group->flags & AGF_SMOOTH)
                {
                    mat->inter = 1 - group->timer / (float) group->maxTimer;
                }
                else
                {
                    mat->inter = 0;
                }

                // Just animate the first in the sequence?
                if(group->flags & AGF_FIRST_ONLY)
                    break;
            }
        }
    }
}

/**
 * One time initialization of the materials list. Called during init.
 */
void Materials_Init(void)
{
    if(initedOk)
        return; // Already been here.

    materialsBlockSet = Z_BlockCreate(sizeof(material_t),
                                      MATERIALS_BLOCK_ALLOC, PU_STATIC);
    materialsHead = NULL;

    materialBinds = NULL;
    numMaterialBinds = maxMaterialBinds = 0;

    // Clear the name bind hash tables.
    memset(hashTable, 0, sizeof(hashTable));

    initedOk = true;
}

/**
 * Release all memory acquired for the materials list.
 * Called during shutdown.
 */
void Materials_Shutdown(void)
{
    if(!initedOk)
        return;

    P_DestroyObjectRecordsByType(DMU_MATERIAL);

    Z_BlockDestroy(materialsBlockSet);
    materialsBlockSet = NULL;
    materialsHead = NULL;

    // Destroy the bindings.
    if(materialBinds)
    {
        Z_Free(materialBinds);
        materialBinds = NULL;
    }
    numMaterialBinds = maxMaterialBinds = 0;

    initedOk = false;
}

/**
 * Deletes all GL texture instances, linked to materials.
 *
 * @param mnamespace    @c MN_ANY = delete everything, ELSE
 *                      Only delete those currently in use by materials
 *                      in the specified namespace.
 */
void Materials_DeleteTextures(material_namespace_t mnamespace)
{
    if(mnamespace == MN_ANY)
    {   // Delete the lot.
        GL_DeleteAllTexturesForGLTextures(GLT_ANY);
        return;
    }

    if(!isKnownMNamespace(mnamespace))
        Con_Error("Materials_DeleteTextures: Internal error, "
                  "invalid namespace '%i'.", (int) mnamespace);

    if(materialBinds)
    {
        uint                i;

        for(i = 0; i < MATERIAL_NAME_HASH_SIZE; ++i)
            if(hashTable[mnamespace][i])
            {
                materialbind_t*     mb = &materialBinds[
                    hashTable[mnamespace][i] - 1];

                for(;;)
                {
                    material_t*         mat = mb->mat;
                    byte                j;

                    for(j = 0; j < mat->numLayers; ++j)
                        GL_ReleaseGLTexture(mat->layers[j].tex);

                    if(!mb->hashNext)
                        break;

                    mb = &materialBinds[mb->hashNext - 1];
                }
            }
    }
}

/**
 * Given a unique material number return the associated material.
 *
 * @param num           Unique material number.
 *
 * @return              The associated material, ELSE @c NULL.
 */
material_t* Materials_ToMaterial(materialnum_t num)
{
    if(!initedOk)
        return NULL;

    if(num != 0)
        return getMaterialByNum(num - 1); // 1-based index.

    return NULL;
}

/**
 * Retrieve the unique material number for the given material.
 *
 * @param mat           The material to retrieve the unique number of.
 *
 * @return              The associated unique number.
 */
materialnum_t Materials_ToIndex(material_t* mat)
{
    if(mat)
    {
        materialnum_t       i;

        for(i = 0; i < numMaterialBinds; ++i)
            if(materialBinds[i].mat == mat)
                return i + 1; // 1-based index.
    }

    return 0;
}

/**
 * Create a new material.
 *
 * \note: May fail if the name is invalid.
 *
 * @param mnamespace    MG_* material namespace.
 *                      MN_ANY is only valid when updating an existing material.
 * @param name          Name of the new material.
 * @param width         Width of the material (not of the texture).
 * @param height        Height of the material (not of the texture).
 * @param flags         MATF_* material flags
 * @param isAutoMaterial @c true = Material is being generated automatically
 *                      to represent an IWAD texture/flat/sprite/etc...
 *
 * @return              The created material, ELSE @c NULL.
 */
material_t* Materials_NewMaterial(material_namespace_t mnamespace,
                                  const char* rawName, short width, short height,
                                  byte flags, const ded_material_t* def,
                                  boolean isAutoMaterial)
{
    int                 n;
    uint                hash;
    char                name[9];
    materialnum_t       oldMat;
    material_t*         mat;

    if(!initedOk)
        return NULL;

    if(!rawName || !rawName[0])
    {
#if _DEBUG
Con_Message("Materials_NewMaterial: Warning, attempted to create material with "
            "NULL name\n.");
#endif
        return NULL;
    }

    // Prepare 'name'.
    for(n = 0; *rawName && n < 8; ++n, rawName++)
        name[n] = tolower(*rawName);
    name[n] = '\0';
    hash = hashForName(name);

    // Check if we've already created a material for this.
    if(mnamespace == MN_ANY)
    {   // Caller doesn't care which namespace. This is only valid if we
        // can find a material by this name using a priority search order.
        oldMat = getMaterialNumForName(name, hash, MN_SPRITES);
        if(!oldMat)
            oldMat = getMaterialNumForName(name, hash, MN_TEXTURES);
        if(!oldMat)
            oldMat = getMaterialNumForName(name, hash, MN_FLATS);
    }
    else
    {
        if(!isKnownMNamespace(mnamespace))
        {
#if _DEBUG
Con_Message("Materials_NewMaterial: Warning, attempted to create material in "
            "unknown namespace '%i'.\n", (int) mnamespace);
#endif
            return NULL;
        }

        oldMat = getMaterialNumForName(name, hash, mnamespace);
    }

    if(mnamespace == MN_ANY)
    {
#if _DEBUG
Con_Message("Materials_NewMaterial: Warning, attempted to create material "
            "without specifying a namespace.\n");
#endif
        return NULL;
    }

    if(oldMat)
        Con_Error("Materials_NewMaterial: Can not create material named '%s' in "
                  "namespace %i, name already in use.", name, mnamespace);

    /**
     * A new material.
     */

    // Only create complete materials.
    // \todo Doing this here isn't ideal.
    if(/*tex == 0 ||*/ !(width > 0) || !(height > 0))
        return NULL;

    mat = createMaterial(width, height, flags, mnamespace, def, isAutoMaterial);

    // Now create a name binding for it.
    newMaterialNameBinding(mat, name, mnamespace, hash);

    // Add this material to the DMU object records.
    P_CreateObjectRecord(DMU_MATERIAL, mat);

    return mat;
}

/**
 * Given a Texture/Flat/Sprite etc idx and a MG_* namespace, search
 * for a matching material.
 *
 * @param ofTypeID      Texture/Flat/Sprite etc idx.
 * @param mnamespace    Specific MG_* namespace (not MN_ANY).
 *
 * @return              The associated material, ELSE @c NULL.
 */
material_t* Materials_ToMaterial2(material_namespace_t mnamespace, int ofTypeID)
{
    if(!initedOk)
        return NULL;

    if(!isKnownMNamespace(mnamespace)) // MN_ANY is considered invalid.
    {
#if _DEBUG
Con_Message("Materials_ToMaterial2: Internal error, invalid namespace '%i'\n",
            (int) mnamespace);
#endif
        return NULL;
    }

    if(materialsHead)
    {
        material_t*        mat;

        mat = materialsHead;
        do
        {
            if(mnamespace == mat->mnamespace &&
               GL_GetGLTexture(mat->layers[0].tex)->ofTypeID == ofTypeID)
            {
                if(mat->flags & MATF_NO_DRAW)
                   return NULL;

                return mat;
            }
        } while((mat = mat->globalNext));
    }

    return NULL;
}

/**
 * Given a name and namespace, search the materials db for a match.
 *
 * @param mnamespace    Specific MG_* namespace ELSE,
 *                      @c MN_ANY = no namespace requirement in which case
 *                      the material is searched for among all namespaces
 *                      using a logic which prioritizes the namespaces as
 *                      follows:
 *                      1st: MN_SPRITES
 *                      2nd: MN_TEXTURES
 *                      3rd: MN_FLATS
 * @param name          Name of the material to search for.
 *
 * @return              Unique number of the found material, else zero.
 */
materialnum_t Materials_CheckIndexForName(material_namespace_t mnamespace,
                                          const char* rawName)
{
    int                 n;
    uint                hash;
    char                name[9];

    if(!initedOk)
        return 0;

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(!rawName || !rawName[0] || rawName[0] == '-')
        return 0;

    if(mnamespace != MN_ANY && !isKnownMNamespace(mnamespace))
    {
#if _DEBUG
Con_Message("Materials_CheckIndexForName: Internal error, invalid namespace '%i'\n",
            (int) mnamespace);
#endif
        return 0;
    }

    // Prepare 'name'.
    for(n = 0; *rawName && n < 8; ++n, rawName++)
        name[n] = tolower(*rawName);
    name[n] = '\0';
    hash = hashForName(name);

    if(mnamespace == MN_ANY)
    {   // Caller doesn't care which namespace.
        materialnum_t       matNum;

        // Check for the material in these namespaces, in priority order.
        if((matNum = getMaterialNumForName(name, hash, MN_SPRITES)))
            return matNum;
        if((matNum = getMaterialNumForName(name, hash, MN_TEXTURES)))
            return matNum;
        if((matNum = getMaterialNumForName(name, hash, MN_FLATS)))
            return matNum;

        return 0; // Not found.
    }

    // Caller wants a material in a specific namespace.
    return getMaterialNumForName(name, hash, mnamespace);
}

/**
 * Given a name and namespace, search the materials db for a match.
 * \note  Same as Materials_CheckIndexForName except will log an error
 *        message if the material being searched for is not found.
 *
 * @param mnamespace    @see materialNamespace.
 * @param name          Name of the material to search for.
 *
 * @return              Unique identifier of the found material, else zero.
 */
materialnum_t Materials_IndexForName(material_namespace_t mnamespace,
                                     const char* name)
{
    materialnum_t       result;

    if(!initedOk)
        return 0;

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(!name || !name[0] || name[0] == '-')
        return 0;

    result = Materials_CheckIndexForName(mnamespace, name);

    // Not found?
    if(verbose && result == 0 && !ddMapSetup) // Don't announce during map setup.
        Con_Message("Materials_IndexForName: \"%.8s\" in namespace %i not found!\n",
                    name, mnamespace);
    return result;
}

/**
 * Given a unique material identifier, lookup the associated name.
 *
 * @param mat           The material to lookup the name for.
 *
 * @return              The associated name.
 */
const char* Materials_NameOf(material_t* mat)
{
    materialnum_t       num;

    if(!initedOk)
        return NULL;

    if(mat && (num = Materials_ToIndex(mat)))
        return materialBinds[num-1].name;

    return "NOMAT"; // Should never happen.
}

/**
 * Called every tic by P_Ticker.
 */
void Materials_Ticker(timespan_t time)
{
    static trigger_t    fixed = { 1.0 / 35, 0 };
    material_t*         mat;

    // The animation will only progress when the game is not paused.
    if(clientPaused)
        return;

    mat = materialsHead;
    while(mat)
    {
        Material_Ticker(mat, time);
        mat = mat->globalNext;
    }

    if(!M_RunTrigger(&fixed, time))
        return;

    animateGroups();
}


boolean Materials_IsInGroup(int groupNum, material_t* mat)
{
    return isInGroup(getGroup(groupNum), mat);
}

int Materials_NumGroups(void)
{
    return numgroups;
}

/**
 * Create a new animation group. Returns the group number.
 */
int Materials_NewGroup(int flags)
{
    animgroup_t*        group;

    // Allocating one by one is inefficient, but it doesn't really matter.
    groups =
        Z_Realloc(groups, sizeof(animgroup_t) * (numgroups + 1), PU_STATIC);

    // Init the new group.
    group = &groups[numgroups];
    memset(group, 0, sizeof(*group));

    // The group number is (index + 1).
    group->id = ++numgroups;
    group->flags = flags;

    return group->id;
}

/**
 * Initialize an entire animation using the data in the definition.
 */
int Materials_NewGroupFromDefiniton(ded_group_t* def)
{
    int                 i, groupNumber = -1;
    int                 num;

    for(i = 0; i < def->count.num; ++i)
    {
        ded_group_member_t *gm = &def->members[i];

        num = Materials_CheckIndexForName(gm->material.mnamespace, gm->material.name);

        if(!num)
            continue;

        // Only create a group when the first texture is found.
        if(groupNumber == -1)
        {
            // Create a new animation group.
            groupNumber = Materials_NewGroup(def->flags);
        }

        Materials_AddToGroup(groupNumber, num, gm->tics, gm->randomTics);
    }

    return groupNumber;
}

/**
 * Called during engine reset to clear the existing animation groups.
 */
void Materials_DestroyGroups(void)
{
    int                 i;

    if(numgroups > 0)
    {
        for(i = 0; i < numgroups; ++i)
        {
            animgroup_t*        group = &groups[i];
            Z_Free(group->frames);
        }

        Z_Free(groups);
        groups = NULL;
        numgroups = 0;
    }
}

void Materials_AddToGroup(int groupNum, int num, int tics, int randomTics)
{
    animgroup_t*       group;
    animframe_t*       frame;
    material_t*        mat;

    group = getGroup(groupNum);
    if(!group)
        Con_Error("Materials_AddToGroup: Unknown anim group '%i'\n.", groupNum);

    if(!num || !(mat = Materials_ToMaterial(num)))
    {
        Con_Message("Materials_AddToGroup: Invalid material num '%i'\n.", num);
        return;
    }

    // Mark the material as being in an animgroup.
    mat->inAnimGroup = true;

    // Allocate a new animframe.
    group->frames =
        Z_Realloc(group->frames, sizeof(animframe_t) * ++group->count,
                  PU_STATIC);

    frame = &group->frames[group->count - 1];

    frame->mat = mat;
    frame->tics = tics;
    frame->random = randomTics;
}

boolean Materials_CacheGroup(int groupNum)
{
    animgroup_t*        group;

    if(group = getGroup(groupNum))
    {
        return (group->flags & AGF_PRECACHE)? true : false;
    }

    return false;
}

/**
 * All animation groups are reseted back to their original state.
 * Called when setting up a map.
 */
void Materials_RewindAnimationGroups(void)
{
    int                 i;
    animgroup_t*        group;

    for(i = 0, group = groups; i < numgroups; ++i, group++)
    {
        // The Precache groups are not intended for animation.
        if((group->flags & AGF_PRECACHE) || !group->count)
            continue;

        group->timer = 0;
        group->maxTimer = 1;

        // The anim group should start from the first step using the
        // correct timings.
        group->index = group->count - 1;
    }

    // This'll get every group started on the first step.
    animateGroups();
}

void Materials_CacheMaterial(material_t* mat)
{
    int                 i;

    for(i = 0; i < numgroups; ++i)
    {
        if(isInGroup(&groups[i], mat))
        {
            int                 k;

            // Precache this group.
            for(k = 0; k < groups[i].count; ++k)
            {
                animframe_t*        frame = &groups[i].frames[k];

                Materials_Prepare(frame->mat, 0, NULL, NULL);
            }
        }
    }
}

byte Materials_Prepare(material_t* mat, byte flags,
                       const material_prepare_params_t* params,
                       material_snapshot_t* snapshot)
{
    return Material_Prepare(snapshot, mat, flags, params);
}

static void printMaterialInfo(materialnum_t num, boolean printNamespace)
{
    const materialbind_t* mb = &materialBinds[num];
    byte                i;
    int                 numDigits = M_NumDigits(numMaterialBinds);

    Con_Printf(" %*lu - \"%s\"", numDigits, num, mb->name);
    if(printNamespace)
        Con_Printf(" (%i)", mb->mat->mnamespace);
    Con_Printf(" [%i, %i]", mb->mat->width, mb->mat->height);

    for(i = 0; i < mb->mat->numLayers; ++i)
    {
        Con_Printf(" %i:%s", i, GL_GetGLTexture(mb->mat->layers[i].tex)->name);
    }
    Con_Printf("\n");
}

static void printMaterials(material_namespace_t mnamespace, const char* like)
{
    materialnum_t       i;

    if(!(mnamespace < NUM_MATERIAL_NAMESPACES))
        return;

    if(mnamespace == MN_ANY)
    {
        Con_Printf("Known Materials (IDX - Name (Namespace) [width, height]):\n");

        for(i = 0; i < numMaterialBinds; ++i)
        {
            materialbind_t*     mb = &materialBinds[i];

            if(like && like[0] && strnicmp(mb->name, like, strlen(like)))
                continue;

            printMaterialInfo(i, true);
        }
    }
    else
    {
        Con_Printf("Known Materials in Namespace %i (IDX - Name "
                   "[width, height]):\n", mnamespace);

        if(materialBinds)
        {
            uint                i;

            for(i = 0; i < MATERIAL_NAME_HASH_SIZE; ++i)
                if(hashTable[mnamespace][i])
                {
                    materialnum_t       num = hashTable[mnamespace][i] - 1;
                    materialbind_t*     mb = &materialBinds[num];

                    for(;;)
                    {
                        if(like && like[0] && strnicmp(mb->name, like, strlen(like)))
                            continue;

                        printMaterialInfo(num, false);

                        if(!mb->hashNext)
                            break;

                        mb = &materialBinds[mb->hashNext - 1];
                    }
                }
        }
    }
}

D_CMD(ListMaterials)
{
    material_namespace_t     mnamespace = MN_ANY;

    if(argc > 1)
    {
        mnamespace = atoi(argv[1]);
        if(mnamespace < MN_FIRST)
        {
            mnamespace = MN_ANY;
        }
        else if(!(mnamespace < NUM_MATERIAL_NAMESPACES))
        {
            Con_Printf("Invalid namespace \"%s\".\n", argv[1]);
            return false;
        }
    }

    printMaterials(mnamespace, argc > 2? argv[2] : NULL);
    return true;
}

/**
 * \note Part of the Doomsday public API.
 */
int P_NewMaterialGroup(int flags)
{
    return Materials_NewGroup(flags);
}

/**
 * \note Part of the Doomsday public API.
 */
void P_AddMaterialToGroup(int groupNum, int num, int tics, int randomTics)
{
    if(num)
    {
        Materials_AddToGroup(groupNum, Materials_ToIndex(
            ((objectrecord_t*) P_ToPtr(DMU_MATERIAL, num))->obj), tics, randomTics);
    }
}

/**
 * \note Part of the Doomsday public API.
 */
material_t* P_MaterialForName(material_namespace_t mnamespace, const char* name)
{
    return (material_t*) P_ObjectRecord(DMU_MATERIAL,
        Materials_ToMaterial(Materials_CheckIndexForName(mnamespace, name)));
}

/**
 * Precache the specified material.
 * \note Part of the Doomsday public API.
 *
 * @param mat           The material to be precached.
 */
void P_MaterialPreload(material_t* mat)
{
    if(!initedOk)
        return;

    Material_Precache((material_t*) ((objectrecord_t*) mat)->obj);
}
