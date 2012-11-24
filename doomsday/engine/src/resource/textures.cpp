/**
 * @file textures.cpp
 * Textures collection. @ingroup resource
 *
 * @authors Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
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

#include <cstdlib>
#include <ctype.h>
#include <cstring>

#include "de_base.h"
#include "de_console.h"

#include "m_misc.h"             // for M_NumDigits
#include "map/r_world.h"        // for ddMapSetup
#include "filesys/fs_util.h"    // for F_PrettyPath
#include "gl/gl_texmanager.h"
#include "resource/texturevariant.h"
#include "resource/textures.h"

#include <de/Error>
#include <de/Log>
#include <de/PathTree>
#include <de/memory.h>
#include <de/memoryzone.h>

typedef de::PathTree TextureRepository;

/**
 * POD object. Contains metadata for a unique Texture in the collection.
 */
struct TextureRecord
{
    /// Scheme-unique identifier chosen by the owner of the collection.
    int uniqueId;

    /// Path to the data resource which contains/wraps the loadable texture data.
    de::Uri* resourcePath;

    /// The defined texture instance (if any).
    Texture* texture;
};

struct TextureScheme
{
    /// The texture directory contains the mappings between names and unique texture records.
    TextureRepository* directory;

    /// LUT which translates scheme-unique-ids to their associated textureid_t (if any).
    /// Index with uniqueId - uniqueIdBase.
    bool uniqueIdMapDirty;
    int uniqueIdBase;
    uint uniqueIdMapSize;
    textureid_t* uniqueIdMap;
};

D_CMD(ListTextures);
D_CMD(InspectTexture);
#if _DEBUG
D_CMD(PrintTextureStats);
#endif

static de::Uri* emptyUri;

// LUT which translates textureid_t to TextureRepository::Node*. Index with textureid_t-1
static uint textureIdMapSize;
static TextureRepository::Node** textureIdMap;

// Texture schemes contain mappings between names and TextureRecord instances.
static TextureScheme schemes[TEXTURESCHEME_COUNT];

void Textures_Register(void)
{
    C_CMD("inspecttexture", NULL, InspectTexture)
    C_CMD("listtextures", NULL, ListTextures)
#if _DEBUG
    C_CMD("texturestats", NULL, PrintTextureStats)
#endif
}

const char* TexSource_Name(TexSource source)
{
    if(source == TEXS_ORIGINAL) return "original";
    if(source == TEXS_EXTERNAL) return "external";
    return "none";
}

static inline TextureRepository& schemeById(textureschemeid_t id)
{
    DENG2_ASSERT(VALID_TEXTURESCHEMEID(id));
    DENG2_ASSERT(schemes[id-TEXTURESCHEME_FIRST].directory);
    return *schemes[id-TEXTURESCHEME_FIRST].directory;
}

static textureschemeid_t schemeIdForDirectory(TextureRepository const& directory)
{
    for(uint i = uint(TEXTURESCHEME_FIRST); i <= uint(TEXTURESCHEME_LAST); ++i)
    {
        uint idx = i - TEXTURESCHEME_FIRST;
        if(schemes[idx].directory == &directory) return textureschemeid_t(i);
    }

    // Only reachable if attempting to find the id for a Texture that is not
    // in the collection, or the collection has not yet been initialized.
    throw de::Error("Textures::schemeIdForDirectory",
                    de::String().sprintf("Failed to determine id for directory %p.", (void*)&directory));
}

static inline bool validTextureId(textureid_t id)
{
    return (id != NOTEXTUREID && id <= textureIdMapSize);
}

static TextureRepository::Node* directoryNodeForBindId(textureid_t id)
{
    if(!validTextureId(id)) return NULL;
    return textureIdMap[id-1/*1-based index*/];
}

static textureid_t findBindIdForDirectoryNode(TextureRepository::Node const& node)
{
    /// @todo Optimize: (Low priority) do not use a linear search.
    for(uint i = 0; i < textureIdMapSize; ++i)
    {
        if(textureIdMap[i] == &node)
            return textureid_t(i+1); // 1-based index.
    }
    return NOTEXTUREID; // Not linked.
}

static inline textureschemeid_t schemeIdForDirectoryNode(TextureRepository::Node const& node)
{
    return schemeIdForDirectory(node.tree());
}

/// @return  Newly composed Uri for @a node. Must be delete'd when no longer needed.
static de::Uri composeUriForDirectoryNode(TextureRepository::Node const& node)
{
    Str const* schemeName = Textures_SchemeName(schemeIdForDirectoryNode(node));
    return de::Uri(node.composePath()).setScheme(Str_Text(schemeName));
}

/// @pre textureIdMap has been initialized and is large enough!
static void unlinkDirectoryNodeFromBindIdMap(TextureRepository::Node const& node)
{
    textureid_t id = findBindIdForDirectoryNode(node);
    if(!validTextureId(id)) return; // Not linked.
    textureIdMap[id - 1/*1-based index*/] = NULL;
}

/// @pre uniqueIdMap has been initialized and is large enough!
static void linkRecordInUniqueIdMap(TextureRecord const* record, TextureScheme* tn,
                                    textureid_t textureId)
{
    DENG2_ASSERT(record && tn);
    DENG2_ASSERT(record->uniqueId - tn->uniqueIdBase >= 0 && (unsigned)(record->uniqueId - tn->uniqueIdBase) < tn->uniqueIdMapSize);
    tn->uniqueIdMap[record->uniqueId - tn->uniqueIdBase] = textureId;
}

/// @pre uniqueIdMap is large enough if initialized!
static void unlinkRecordInUniqueIdMap(TextureRecord const* record, TextureScheme* tn)
{
    DENG2_ASSERT(record && tn);
    // If the map is already considered 'dirty' do not unlink.
    if(tn->uniqueIdMap && !tn->uniqueIdMapDirty)
    {
        DENG2_ASSERT(record->uniqueId - tn->uniqueIdBase >= 0 && (unsigned)(record->uniqueId - tn->uniqueIdBase) < tn->uniqueIdMapSize);
        tn->uniqueIdMap[record->uniqueId - tn->uniqueIdBase] = NOTEXTUREID;
    }
}

/**
 * @defgroup validateTextureUriFlags  Validate Texture Uri Flags
 * @ingroup flags
 */
///@{
#define VTUF_ALLOW_ANY_SCHEME       0x1 ///< The scheme of the URI may be of zero-length; signifying "any scheme".
#define VTUF_NO_URN                 0x2 ///< Do not accept a URN.
///@}

/**
 * @param uri       Uri to be validated.
 * @param flags     @ref validateTextureUriFlags
 * @param quiet     @c true= Do not output validation remarks to the log.
 *
 * @return  @c true if @a Uri passes validation.
 */
static bool validateTextureUri(de::Uri const& uri, int flags, bool quiet = false)
{
    LOG_AS("validateTextureUri");

    if(uri.isEmpty())
    {
        if(!quiet)
        {
            LOG_MSG("Invalid path in texture URI \"%s\".") << uri;
        }
        return false;
    }

    // If this is a URN we extract the scheme from the path.
    de::String schemeString;
    if(!uri.scheme().compareWithoutCase("urn"))
    {
        if(flags & VTUF_NO_URN) return false;
        schemeString = uri.path();
    }
    else
    {
        schemeString = uri.scheme();
    }

    textureschemeid_t schemeId = Textures_ParseSchemeName(schemeString.toUtf8().constData());
    if(!((flags & VTUF_ALLOW_ANY_SCHEME) && schemeId == TS_ANY) &&
       !VALID_TEXTURESCHEMEID(schemeId))
    {
        if(!quiet)
        {
            LOG_MSG("Unknown scheme in texture URI \"%s\".") << uri;
        }
        return false;
    }

    return true;
}

/**
 * Given a directory and path, search the Textures collection for a match.
 *
 * @param directory Scheme-specific TextureRepository to search in.
 * @param path      Path of the texture to search for.
 *
 * @return  Found DirectoryNode else @c NULL
 */
static TextureRepository::Node* findDirectoryNodeForPath(TextureRepository& texDirectory, de::String path)
{
    try
    {
        TextureRepository::Node &node = texDirectory.find(path, de::PathTree::NoBranch | de::PathTree::MatchFull);
        return &node;
    }
    catch(TextureRepository::NotFoundError const&)
    {} // Ignore this error.
    return 0;
}

/// @pre @a uri has already been validated and is well-formed.
static TextureRepository::Node* findDirectoryNodeForUri(de::Uri const& uri)
{
    if(!uri.scheme().compareWithoutCase("urn"))
    {
        // This is a URN of the form; urn:schemename:uniqueid
        textureschemeid_t schemeId = Textures_ParseSchemeName(uri.pathCStr());
        int uidPos = uri.path().toStringRef().indexOf(':');
        if(uidPos >= 0)
        {
            int uid = uri.path().toString().mid(uidPos + 1 /*skip scheme delimiter*/).toInt();
            textureid_t id = Textures_TextureForUniqueId(schemeId, uid);
            if(id != NOTEXTUREID)
            {
                return directoryNodeForBindId(id);
            }
        }
        return NULL;
    }

    // This is a URI.
    textureschemeid_t schemeId = Textures_ParseSchemeName(uri.schemeCStr());
    de::String const& path = uri.path();

    TextureRepository::Node* node = NULL;
    if(schemeId != TS_ANY)
    {
        // Caller wants a texture in a specific scheme.
        node = findDirectoryNodeForPath(schemeById(schemeId), path);
    }
    else
    {
        // Caller does not care which scheme.
        // Check for the texture in these schemes in priority order.
        static const textureschemeid_t order[] = {
            TS_SPRITES,
            TS_TEXTURES,
            TS_FLATS,
            TS_PATCHES,
            TS_SYSTEM,
            TS_DETAILS,
            TS_REFLECTIONS,
            TS_MASKS,
            TS_MODELSKINS,
            TS_MODELREFLECTIONSKINS,
            TS_LIGHTMAPS,
            TS_FLAREMAPS,
            TS_ANY
        };
        int n = 0;
        do
        {
            node = findDirectoryNodeForPath(schemeById(order[n]), path);
        } while(!node && order[++n] != TS_ANY);
    }
    return node;
}

static void clearTextureAnalyses(Texture* tex)
{
    DENG2_ASSERT(tex);
    for(uint i = uint(TEXTURE_ANALYSIS_FIRST); i < uint(TEXTURE_ANALYSIS_COUNT); ++i)
    {
        texture_analysisid_t analysis = texture_analysisid_t(i);
        void* data = Texture_AnalysisDataPointer(tex, analysis);
        if(data) M_Free(data);
        Texture_SetAnalysisDataPointer(tex, analysis, 0);
    }
}

static void destroyTexture(Texture* tex)
{
    DENG2_ASSERT(tex);

    GL_ReleaseGLTexturesByTexture(tex);
    switch(Textures_Scheme(Textures_Id(tex)))
    {
    case TS_SYSTEM:
    case TS_DETAILS:
    case TS_REFLECTIONS:
    case TS_MASKS:
    case TS_MODELSKINS:
    case TS_MODELREFLECTIONSKINS:
    case TS_LIGHTMAPS:
    case TS_FLAREMAPS:
    case TS_FLATS: break;

    case TS_TEXTURES: {
        patchcompositetex_t* pcTex = (patchcompositetex_t*)Texture_UserDataPointer(tex);
        if(pcTex)
        {
            Str_Free(&pcTex->name);
            if(pcTex->patches) M_Free(pcTex->patches);
            M_Free(pcTex);
        }
        break;
    }
    case TS_SPRITES: {
        patchtex_t* pTex = (patchtex_t*)Texture_UserDataPointer(tex);
        if(pTex) M_Free(pTex);
        break;
    }
    case TS_PATCHES: {
        patchtex_t* pTex = (patchtex_t*)Texture_UserDataPointer(tex);
        if(pTex) M_Free(pTex);
        break;
    }
    default:
        throw de::Error("Textures::destroyTexture",
                        de::String("Internal error, invalid scheme id %1.")
                            .arg((int)Textures_Scheme(Textures_Id(tex))));
    }

    clearTextureAnalyses(tex);
    Texture_Delete(tex);
}

static void destroyBoundTexture(TextureRepository::Node& node)
{
    TextureRecord* record = reinterpret_cast<TextureRecord*>(node.userPointer());
    if(record && record->texture)
    {
        destroyTexture(record->texture); record->texture = NULL;
    }
}

static void destroyRecord(TextureRepository::Node& node)
{
    TextureRecord* record = reinterpret_cast<TextureRecord*>(node.userPointer());

    LOG_AS("Textures::destroyRecord");

    if(record)
    {
        if(record->texture)
        {
#if _DEBUG
            de::Uri uri = composeUriForDirectoryNode(node);
            LOG_WARNING("Record for \"%s\" still has Texture data!") << uri;
#endif
            destroyTexture(record->texture);
        }

        if(record->resourcePath)
        {
            delete record->resourcePath; record->resourcePath = 0;
        }

        unlinkDirectoryNodeFromBindIdMap(node);

        textureschemeid_t const schemeId = schemeIdForDirectoryNode(node);
        TextureScheme* tn = &schemes[schemeId - TEXTURESCHEME_FIRST];
        unlinkRecordInUniqueIdMap(record, tn);

        // Detach our user data from this node.
        node.setUserPointer(0);
        M_Free(record);
    }
}

void Textures_Init(void)
{
    LOG_VERBOSE("Initializing Textures collection...");

    emptyUri = new de::Uri();

    textureIdMap = NULL;
    textureIdMapSize = 0;

    for(uint i = 0; i < TEXTURESCHEME_COUNT; ++i)
    {
        TextureScheme* tn = &schemes[i];
        tn->directory = new TextureRepository();
        tn->uniqueIdBase = 0;
        tn->uniqueIdMapSize = 0;
        tn->uniqueIdMap = NULL;
        tn->uniqueIdMapDirty = false;
    }
}

void Textures_Shutdown(void)
{
    Textures_Clear();

    for(uint i = 0; i < TEXTURESCHEME_COUNT; ++i)
    {
        TextureScheme* tn = &schemes[i];

        if(tn->directory)
        {
            DENG2_FOR_EACH_CONST(TextureRepository::Nodes, nodeIt, tn->directory->leafNodes())
            {
                destroyRecord(*reinterpret_cast<TextureRepository::Node*>(*nodeIt));
            }
            delete tn->directory; tn->directory = 0;
        }

        if(!tn->uniqueIdMap) continue;
        M_Free(tn->uniqueIdMap); tn->uniqueIdMap = 0;

        tn->uniqueIdBase = 0;
        tn->uniqueIdMapSize = 0;
        tn->uniqueIdMapDirty = false;
    }

    // Clear the bindId to TextureRepository::Node LUT.
    if(textureIdMap)
    {
        M_Free(textureIdMap); textureIdMap = 0;
    }
    textureIdMapSize = 0;

    if(emptyUri)
    {
        delete emptyUri; emptyUri = 0;
    }
}

textureschemeid_t Textures_ParseSchemeName(const char* str)
{
    static const struct scheme_s {
        const char* name;
        size_t nameLen;
        textureschemeid_t id;
    } schemeNameIdMap[TEXTURESCHEME_COUNT+1] = {
        // Ordered according to a best guess of occurance frequency.
        { TS_TEXTURES_NAME,     sizeof(TS_TEXTURES_NAME)-1,     TS_TEXTURES },
        { TS_FLATS_NAME,        sizeof(TS_FLATS_NAME)-1,        TS_FLATS },
        { TS_SPRITES_NAME,      sizeof(TS_SPRITES_NAME)-1,      TS_SPRITES },
        { TS_PATCHES_NAME,      sizeof(TS_PATCHES_NAME)-1,      TS_PATCHES },
        { TS_SYSTEM_NAME,       sizeof(TS_SYSTEM_NAME)-1,       TS_SYSTEM },
        { TS_DETAILS_NAME,      sizeof(TS_DETAILS_NAME)-1,      TS_DETAILS },
        { TS_REFLECTIONS_NAME,  sizeof(TS_REFLECTIONS_NAME)-1,  TS_REFLECTIONS },
        { TS_MASKS_NAME,        sizeof(TS_MASKS_NAME)-1,        TS_MASKS },
        { TS_MODELSKINS_NAME,   sizeof(TS_MODELSKINS_NAME)-1,   TS_MODELSKINS },
        { TS_MODELREFLECTIONSKINS_NAME, sizeof(TS_MODELREFLECTIONSKINS_NAME)-1, TS_MODELREFLECTIONSKINS },
        { TS_LIGHTMAPS_NAME,    sizeof(TS_LIGHTMAPS_NAME)-1,    TS_LIGHTMAPS },
        { TS_FLAREMAPS_NAME,    sizeof(TS_FLAREMAPS_NAME)-1,    TS_FLAREMAPS },
        { NULL,                 0,                              TS_INVALID }
    };

    // Special case: zero-length string means "any scheme".
    size_t len;
    if(!str || 0 == (len = strlen(str))) return TS_ANY;

    // Stop comparing characters at the first occurance of ':'
    const char* end = strchr(str, ':');
    if(end) len = end - str;

    for(size_t n = 0; schemeNameIdMap[n].name; ++n)
    {
        if(len < schemeNameIdMap[n].nameLen) continue;
        if(strnicmp(str, schemeNameIdMap[n].name, len)) continue;
        return schemeNameIdMap[n].id;
    }

    return TS_INVALID; // Unknown.
}

const Str* Textures_SchemeName(textureschemeid_t id)
{
    static const de::Str names[1+TEXTURESCHEME_COUNT] = {
        /* No scheme name */            "",
        /* TS_SYSTEM */                 TS_SYSTEM_NAME,
        /* TS_FLATS */                  TS_FLATS_NAME,
        /* TS_TEXTURES */               TS_TEXTURES_NAME,
        /* TS_SPRITES */                TS_SPRITES_NAME,
        /* TS_PATCHES */                TS_PATCHES_NAME,
        /* TS_DETAILS */                TS_DETAILS_NAME,
        /* TS_REFLECTIONS */            TS_REFLECTIONS_NAME,
        /* TS_MASKS */                  TS_MASKS_NAME,
        /* TS_MODELSKINS */             TS_MODELSKINS_NAME,
        /* TS_MODELREFLECTIONSKINS */   TS_MODELREFLECTIONSKINS_NAME,
        /* TS_LIGHTMAPS */              TS_LIGHTMAPS_NAME,
        /* TS_FLAREMAPS */              TS_FLAREMAPS_NAME
    };
    if(VALID_TEXTURESCHEMEID(id))
    {
        return names[1 + (id - TEXTURESCHEME_FIRST)];
    }
    return names[0];
}

uint Textures_Size(void)
{
    return textureIdMapSize;
}

uint Textures_Count(textureschemeid_t schemeId)
{
    if(!VALID_TEXTURESCHEMEID(schemeId) || !Textures_Size()) return 0;
    return schemeById(schemeId).size();
}

void Textures_Clear(void)
{
    if(!Textures_Size()) return;

    Textures_ClearScheme(TS_ANY);
    GL_PruneTextureVariantSpecifications();
}

void Textures_ClearRuntime(void)
{
    if(!Textures_Size()) return;

    Textures_ClearScheme(TS_FLATS);
    Textures_ClearScheme(TS_TEXTURES);
    Textures_ClearScheme(TS_PATCHES);
    Textures_ClearScheme(TS_SPRITES);
    Textures_ClearScheme(TS_DETAILS);
    Textures_ClearScheme(TS_REFLECTIONS);
    Textures_ClearScheme(TS_MASKS);
    Textures_ClearScheme(TS_MODELSKINS);
    Textures_ClearScheme(TS_MODELREFLECTIONSKINS);
    Textures_ClearScheme(TS_LIGHTMAPS);
    Textures_ClearScheme(TS_FLAREMAPS);

    GL_PruneTextureVariantSpecifications();
}

void Textures_ClearSystem(void)
{
    if(!Textures_Size()) return;

    Textures_ClearScheme(TS_SYSTEM);
    GL_PruneTextureVariantSpecifications();
}

void Textures_ClearScheme(textureschemeid_t schemeId)
{
    if(!Textures_Size()) return;

    textureschemeid_t from, to;
    if(schemeId == TS_ANY)
    {
        from = TEXTURESCHEME_FIRST;
        to   = TEXTURESCHEME_LAST;
    }
    else if(VALID_TEXTURESCHEMEID(schemeId))
    {
        from = to = schemeId;
    }
    else
    {
        Con_Error("Textures::ClearScheme: Invalid texture scheme %i.", (int) schemeId);
        exit(1); // Unreachable.
    }

    for(uint i = uint(from); i <= uint(to); ++i)
    {
        textureschemeid_t iter = textureschemeid_t(i);
        TextureScheme* tn = &schemes[iter - TEXTURESCHEME_FIRST];

        DENG2_FOR_EACH_CONST(TextureRepository::Nodes, nodeIt, tn->directory->leafNodes())
        {
            TextureRepository::Node& node = **nodeIt;
            destroyBoundTexture(node);
            destroyRecord(node);
        }
        tn->directory->clear();
        tn->uniqueIdMapDirty = true;
    }
}

void Textures_Release(Texture* tex)
{
    /// Stub.
    GL_ReleaseGLTexturesByTexture(tex);
    /// @todo Update any Materials (and thus Surfaces) which reference this.
}

Texture* Textures_ToTexture(textureid_t id)
{
    TextureRepository::Node* node = directoryNodeForBindId(id);
    TextureRecord* record = (node? reinterpret_cast<TextureRecord*>(node->userPointer()) : NULL);
    if(record)
    {
        return record->texture;
    }
#if _DEBUG
    else if(id != NOTEXTUREID)
    {
        Con_Message("Warning:Textures::ToTexture: Failed to locate texture for id #%i, returning NULL.\n", id);
    }
#endif
    return NULL;
}

static void findUniqueIdBounds(TextureScheme* tn, int* minId, int* maxId)
{
    DENG2_ASSERT(tn);
    if(!minId && !maxId) return;

    if(!tn->directory)
    {
        if(minId) *minId = 0;
        if(maxId) *maxId = 0;
        return;
    }

    if(minId) *minId = DDMAXINT;
    if(maxId) *maxId = DDMININT;

    DENG2_FOR_EACH_CONST(TextureRepository::Nodes, i, tn->directory->leafNodes())
    {
        TextureRecord const* record = reinterpret_cast<TextureRecord*>((*i)->userPointer());
        if(!record) continue;

        if(minId && record->uniqueId < *minId) *minId = record->uniqueId;
        if(maxId && record->uniqueId > *maxId) *maxId = record->uniqueId;
    }
}

static void rebuildUniqueIdMap(textureschemeid_t schemeId)
{
    DENG2_ASSERT(VALID_TEXTURESCHEMEID(schemeId));

    TextureScheme* tn = &schemes[schemeId - TEXTURESCHEME_FIRST];

    // Is a rebuild necessary?
    if(!tn->uniqueIdMapDirty) return;

    // Determine the size of the LUT.
    int minId, maxId;
    findUniqueIdBounds(tn, &minId, &maxId);

    if(minId > maxId)
    {
        // None found.
        tn->uniqueIdBase = 0;
        tn->uniqueIdMapSize = 0;
    }
    else
    {
        tn->uniqueIdBase = minId;
        tn->uniqueIdMapSize = maxId - minId + 1;
    }

    // Allocate and (re)populate the LUT.
    tn->uniqueIdMap = static_cast<textureid_t*>(M_Realloc(tn->uniqueIdMap, sizeof(*tn->uniqueIdMap) * tn->uniqueIdMapSize));
    if(!tn->uniqueIdMap && tn->uniqueIdMapSize)
    {
        throw de::Error("Textures::rebuildUniqueIdMap",
                        de::String("Failed on (re)allocation of %1 bytes resizing the map.")
                            .arg(sizeof(*tn->uniqueIdMap) * tn->uniqueIdMapSize));
    }
    if(tn->uniqueIdMapSize)
    {
        memset(tn->uniqueIdMap, NOTEXTUREID, sizeof(*tn->uniqueIdMap) * tn->uniqueIdMapSize);

        DENG2_FOR_EACH_CONST(TextureRepository::Nodes, i, tn->directory->leafNodes())
        {
            TextureRepository::Node& node = **i;
            TextureRecord const* record = reinterpret_cast<TextureRecord*>(node.userPointer());
            if(!record) continue;
            linkRecordInUniqueIdMap(record, tn, findBindIdForDirectoryNode(node));
        }
    }

    tn->uniqueIdMapDirty = false;
}

textureid_t Textures_TextureForUniqueId(textureschemeid_t schemeId, int uniqueId)
{
    if(VALID_TEXTURESCHEMEID(schemeId))
    {
        TextureScheme* tn = &schemes[schemeId - TEXTURESCHEME_FIRST];

        rebuildUniqueIdMap(schemeId);
        if(tn->uniqueIdMap && uniqueId >= tn->uniqueIdBase &&
           (unsigned)(uniqueId - tn->uniqueIdBase) <= tn->uniqueIdMapSize)
        {
            return tn->uniqueIdMap[uniqueId - tn->uniqueIdBase];
        }
    }
    return NOTEXTUREID; // Not found.
}

textureid_t Textures_ResolveUri2(Uri const* _uri, boolean quiet)
{
    LOG_AS("Textures::resolveUri");

    if(!_uri || !Textures_Size()) return NOTEXTUREID;

    de::Uri const& uri = reinterpret_cast<de::Uri const&>(*_uri);
    if(!validateTextureUri(uri, VTUF_ALLOW_ANY_SCHEME, true /*quiet please*/))
    {
#if _DEBUG
        LOG_WARNING("Uri \"%s\" failed validation, returning NOTEXTUREID.") << uri;
#endif
        return NOTEXTUREID;
    }

    // Perform the search.
    TextureRepository::Node* node = findDirectoryNodeForUri(uri);
    if(node)
    {
        // If we have bound a texture - it can provide the id.
        TextureRecord* record = reinterpret_cast<TextureRecord*>(node->userPointer());
        DENG2_ASSERT(record);
        if(record->texture)
        {
            textureid_t id = Texture_PrimaryBind(record->texture);
            if(validTextureId(id)) return id;
        }
        // Oh well, look it up then.
        return findBindIdForDirectoryNode(*node);
    }

    // Not found.
    if(!quiet && !ddMapSetup) // Do not announce during map setup.
    {
        LOG_DEBUG("\"%s\" not found!") << uri;
    }
    return NOTEXTUREID;
}

textureid_t Textures_ResolveUri(Uri const* uri)
{
    return Textures_ResolveUri2(uri, !(verbose >= 1)/*log warnings if verbose*/);
}

textureid_t Textures_ResolveUriCString2(char const* path, boolean quiet)
{
    if(path && path[0])
    {
        de::Uri uri = de::Uri(path, RC_NULL);
        return Textures_ResolveUri2(reinterpret_cast<uri_s*>(&uri), quiet);
    }
    return NOTEXTUREID;
}

textureid_t Textures_ResolveUriCString(char const* path)
{
    return Textures_ResolveUriCString2(path, !(verbose >= 1)/*log warnings if verbose*/);
}

static textureid_t Textures_Declare2(de::Uri& uri, int uniqueId, de::Uri const* resourcePath)
{
    LOG_AS("Textures::declare");

    // We require a properly formed uri (but not a urn - this is a path).
    if(!validateTextureUri(uri, VTUF_NO_URN, (verbose >= 1)))
    {
        LOG_WARNING("Failed declaring texture \"%s\" (invalid Uri), ignoring.") << uri;
        return NOTEXTUREID;
    }

    // Have we already created a binding for this?
    TextureRepository::Node* node = findDirectoryNodeForUri(uri);
    TextureRecord* record;
    textureid_t id;
    if(node)
    {
        record = reinterpret_cast<TextureRecord*>(node->userPointer());
        DENG2_ASSERT(record);
        id = findBindIdForDirectoryNode(*node);
    }
    else
    {
        /*
         * A new binding.
         */

        record = (TextureRecord*)M_Malloc(sizeof *record);
        if(!record) throw de::Error("Textures::Declare", de::String("Failed on allocation of %1 bytes for new TextureRecord.").arg((unsigned long) sizeof *record));

        record->texture      = NULL;
        record->resourcePath = NULL;
        record->uniqueId     = uniqueId;

        textureschemeid_t schemeId = Textures_ParseSchemeName(uri.schemeCStr());
        TextureScheme* tn = &schemes[schemeId - TEXTURESCHEME_FIRST];

        node = tn->directory->insert(uri.path());
        node->setUserPointer(record);

        // We'll need to rebuild the unique id map too.
        tn->uniqueIdMapDirty = true;

        id = textureIdMapSize + 1; // 1-based identfier
        // Link it into the id map.
        textureIdMap = (TextureRepository::Node**) M_Realloc(textureIdMap, sizeof *textureIdMap * ++textureIdMapSize);
        if(!textureIdMap) throw de::Error("Textures::Declare", de::String("Failed on (re)allocation of %1 bytes enlarging bindId => TextureRepository::Node LUT.").arg((unsigned long) sizeof *textureIdMap * textureIdMapSize));

        textureIdMap[id - 1] = node;
    }

    /**
     * (Re)configure this binding.
     */

    // We don't care whether these identfiers are truely unique. Our only
    // responsibility is to release textures when they change.
    bool releaseTexture = false;
    if(record->uniqueId != uniqueId)
    {
        textureschemeid_t const schemeId = schemeIdForDirectoryNode(*node);
        TextureScheme* tn = &schemes[schemeId - TEXTURESCHEME_FIRST];

        record->uniqueId = uniqueId;
        releaseTexture = true;

        // We'll need to rebuild the id map too.
        tn->uniqueIdMapDirty = true;
    }

    if(resourcePath)
    {
        if(!record->resourcePath)
        {
            record->resourcePath = new de::Uri(*resourcePath);
            releaseTexture = true;
        }
        else if(record->resourcePath != resourcePath)
        {
            *record->resourcePath = *resourcePath;
            releaseTexture = true;
        }
    }
    else if(record->resourcePath)
    {
        delete record->resourcePath; record->resourcePath = NULL;
        releaseTexture = true;
    }

    if(releaseTexture && record->texture)
    {
        // The mapped resource is being replaced, so release any existing Texture.
        /// @todo Only release if this Texture is bound to only this binding.
        Textures_Release(record->texture);
    }

    return id;
}

textureid_t Textures_Declare(Uri* uri, int uniqueId, Uri const* resourcePath)
{
    if(!uri) return NOTEXTUREID;
    return Textures_Declare2(reinterpret_cast<de::Uri&>(*uri), uniqueId, reinterpret_cast<de::Uri const*>(resourcePath));
}

Texture* Textures_CreateWithSize(textureid_t id, boolean custom, const Size2Raw* size,
    void* userData)
{
    LOG_AS("Textures_CreateWithSize");

    if(!size)
    {
        LOG_WARNING("Failed defining Texture #%u (invalid size), ignoring.") << id;
        return NULL;
    }

    TextureRepository::Node* node = directoryNodeForBindId(id);
    TextureRecord* record = (node? reinterpret_cast<TextureRecord*>(node->userPointer()) : NULL);
    if(!record)
    {
        LOG_WARNING("Failed defining Texture #%u (invalid id), ignoring.") << id;
        return NULL;
    }

    if(record->texture)
    {
        /// @todo Do not update textures here (not enough knowledge). We should instead
        /// return an invalid reference/signal and force the caller to implement the
        /// necessary update logic.
        Texture* tex = record->texture;
#if _DEBUG
        de::Uri* uri = reinterpret_cast<de::Uri*>(Textures_ComposeUri(id));
        LOG_WARNING("A Texture with uri \"%s\" already exists, returning existing.") << uri;
        delete uri;
#endif
        Texture_FlagCustom(tex, custom);
        Texture_SetSize(tex, size);
        Texture_SetUserDataPointer(tex, userData);
        /// @todo Materials and Surfaces should be notified of this!
        return tex;
    }

    // A new texture.
    Texture* tex = record->texture = Texture_NewWithSize(id, size, userData);
    Texture_FlagCustom(tex, custom);
    return tex;
}

Texture* Textures_Create(textureid_t id, boolean custom, void* userData)
{
    Size2Raw size(0, 0);
    return Textures_CreateWithSize(id, custom, &size, userData);
}

int Textures_UniqueId(textureid_t id)
{
    LOG_AS("Textures::uniqueId");

    TextureRepository::Node* node = directoryNodeForBindId(id);
    TextureRecord* record = (node? reinterpret_cast<TextureRecord*>(node->userPointer()) : NULL);
    if(record)
    {
        return record->uniqueId;
    }
#if _DEBUG
    if(id != NOTEXTUREID)
    {
        LOG_WARNING("Attempted with unbound textureId #%u, returning zero.") << id;
    }
#endif
    return 0;
}

Uri const* Textures_ResourcePath(textureid_t id)
{
    LOG_AS("Textures::resourcePath");

    TextureRepository::Node* node = directoryNodeForBindId(id);
    TextureRecord* record = (node? reinterpret_cast<TextureRecord*>(node->userPointer()) : NULL);
    if(record)
    {
        if(record->resourcePath)
        {
            return reinterpret_cast<Uri*>(record->resourcePath);
        }
    }
#if _DEBUG
    else if(id != NOTEXTUREID)
    {
        LOG_WARNING("Attempted with unbound textureId #%u, returning null-object.") << id;
    }
#endif
    return reinterpret_cast<Uri*>(emptyUri);
}

textureid_t Textures_Id(Texture* tex)
{
    LOG_AS("Textures::id");

    if(tex)
    {
        return Texture_PrimaryBind(tex);
    }
#if _DEBUG
    LOG_WARNING("Attempted with invalid reference [%p], returning invalid id.") << de::dintptr(tex);
#endif
    return NOTEXTUREID;
}

textureschemeid_t Textures_Scheme(textureid_t id)
{
    LOG_AS("Textures::scheme");

    TextureRepository::Node* node = directoryNodeForBindId(id);
    if(node)
    {
        return schemeIdForDirectoryNode(*node);
    }
#if _DEBUG
    if(id != NOTEXTUREID)
    {
        LOG_WARNING("Attempted with unbound textureId #%u, returning null-object.") << id;
    }
#endif
    return TS_ANY;
}

AutoStr* Textures_ComposePath(textureid_t id)
{
    LOG_AS("Textures::composePath");

    TextureRepository::Node* node = directoryNodeForBindId(id);
    if(node)
    {
        QByteArray path = node->composePath().toUtf8();
        return AutoStr_FromTextStd(path.constData());
    }

    LOG_WARNING("Attempted with unbound textureId #%u, returning null-object.") << id;
    return AutoStr_NewStd();
}

Uri* Textures_ComposeUri(textureid_t id)
{
    LOG_AS("Textures::composeUri");

    TextureRepository::Node* node = directoryNodeForBindId(id);
    if(node)
    {
        return reinterpret_cast<Uri*>(new de::Uri(composeUriForDirectoryNode(*node)));
    }

#if _DEBUG
    if(id != NOTEXTUREID)
    {
        LOG_WARNING("Attempted with unbound textureId #%u, returning null-object.") << id;
    }
#endif
    return Uri_New();
}

Uri* Textures_ComposeUrn(textureid_t id)
{
    LOG_AS("Textures::composeUrn");

    TextureRepository::Node* node = directoryNodeForBindId(id);
    TextureRecord const* record = (node? reinterpret_cast<TextureRecord*>(node->userPointer()) : NULL);
    de::Uri* uri = new de::Uri();

    if(record)
    {
        Str const* schemeName = Textures_SchemeName(schemeIdForDirectoryNode(*node));
        AutoStr* path = AutoStr_NewStd();

        Str_Reserve(path, Str_Length(schemeName) +1/*delimiter*/ + M_NumDigits(DDMAXINT));
        Str_Appendf(path, "%s:%i", Str_Text(schemeName), record->uniqueId);

        uri->setScheme("urn").setPath(Str_Text(path));

        return reinterpret_cast<Uri*>(uri);
    }

#if _DEBUG
    if(id != NOTEXTUREID)
    {
        LOG_WARNING("Attempted with unbound textureId #%u, returning null-object.") << id;
    }
#endif
    return reinterpret_cast<Uri*>(uri);
}

int Textures_Iterate2(textureschemeid_t schemeId,
    int (*callback)(Texture* tex, void* parameters), void* parameters)
{
    if(!callback) return 0;

    textureschemeid_t from, to;
    if(VALID_TEXTURESCHEMEID(schemeId))
    {
        from = to = schemeId;
    }
    else
    {
        from = TEXTURESCHEME_FIRST;
        to   = TEXTURESCHEME_LAST;
    }

    for(uint i = uint(from); i <= uint(to); ++i)
    {
        TextureRepository& directory = schemeById(textureschemeid_t(i));

        DENG2_FOR_EACH_CONST(TextureRepository::Nodes, nodeIt, directory.leafNodes())
        {
            TextureRecord* record = reinterpret_cast<TextureRecord*>((*nodeIt)->userPointer());
            if(!record || !record->texture) continue;

            int result = callback(record->texture, parameters);
            if(result) return result;
        }
    }
    return 0;
}

int Textures_Iterate(textureschemeid_t schemeId,
    int (*callback)(Texture* tex, void* parameters))
{
    return Textures_Iterate2(schemeId, callback, NULL/*no parameters*/);
}

int Textures_IterateDeclared2(textureschemeid_t schemeId,
    int (*callback)(textureid_t textureId, void* parameters), void* parameters)
{
    if(!callback) return 0;

    textureschemeid_t from, to;
    if(VALID_TEXTURESCHEMEID(schemeId))
    {
        from = to = schemeId;
    }
    else
    {
        from = TEXTURESCHEME_FIRST;
        to   = TEXTURESCHEME_LAST;
    }

    for(uint i = uint(from); i <= uint(to); ++i)
    {
        TextureRepository& directory = schemeById(textureschemeid_t(i));

        DENG2_FOR_EACH_CONST(TextureRepository::Nodes, nodeIt, directory.leafNodes())
        {
            TextureRepository::Node& node = **nodeIt;
            TextureRecord* record = reinterpret_cast<TextureRecord*>(node.userPointer());
            if(!record) continue;

            // If we have bound a texture it can provide the id.
            textureid_t textureId = NOTEXTUREID;
            if(record->texture)
                textureId = Texture_PrimaryBind(record->texture);

            // Otherwise look it up.
            if(!validTextureId(textureId))
                textureId = findBindIdForDirectoryNode(node);

            // Sanity check.
            DENG2_ASSERT(validTextureId(textureId));

            int result = callback(textureId, parameters);
            if(result) return result;
        }
    }
    return 0;
}

int Textures_IterateDeclared(textureschemeid_t schemeId,
    int (*callback)(textureid_t textureId, void* parameters))
{
    return Textures_IterateDeclared2(schemeId, callback, NULL/*no parameters*/);
}

static int printVariantInfo(TextureVariant* variant, void* parameters)
{
    uint* variantIdx = (uint*)parameters;
    DENG2_ASSERT(variantIdx);

    Con_Printf("Variant #%i: GLName:%u\n", *variantIdx,
               TextureVariant_GLName(variant));

    float s, t;
    TextureVariant_Coords(variant, &s, &t);
    Con_Printf("  Source:%s Masked:%s Prepared:%s Uploaded:%s\n  Coords:(s:%g t:%g)\n",
               TexSource_Name(TextureVariant_Source(variant)),
               TextureVariant_IsMasked(variant)  ? "yes":"no",
               TextureVariant_IsPrepared(variant)? "yes":"no",
               TextureVariant_IsUploaded(variant)? "yes":"no", s, t);

    Con_Printf("  Specification: ");
    GL_PrintTextureVariantSpecification(TextureVariant_Spec(variant));

    ++(*variantIdx);
    return 0; // Continue iteration.
}

static void printTextureInfo(Texture* tex)
{
    Uri* uri = Textures_ComposeUri(Textures_Id(tex));
    AutoStr* path = Uri_ToString(uri);

    Con_Printf("Texture \"%s\" [%p] x%u uid:%u origin:%s\nSize: %d x %d\n",
               F_PrettyPath(Str_Text(path)), (void*) tex, Texture_VariantCount(tex),
               (uint) Textures_Id(tex), Texture_IsCustom(tex)? "addon" : "game",
               Texture_Width(tex), Texture_Height(tex));

    uint variantIdx = 0;
    Texture_IterateVariants(tex, printVariantInfo, (void*)&variantIdx);

    Uri_Delete(uri);
}

static void printTextureOverview(TextureRepository::Node& node, bool printSchemeName)
{
    TextureRecord* record = reinterpret_cast<TextureRecord*>(node.userPointer());
    textureid_t texId = findBindIdForDirectoryNode(node);
    int numUidDigits = MAX_OF(3/*uid*/, M_NumDigits(Textures_Size()));
    Uri* uri = record->texture? Textures_ComposeUri(texId) : Uri_New();
    AutoStr* path = printSchemeName? Uri_ToString(uri) : Str_PercentDecode(AutoStr_FromTextStd(Str_Text(Uri_Path(uri))));
    AutoStr* resourcePath = Uri_ToString(Textures_ResourcePath(texId));

    Con_FPrintf(!record->texture? CPF_LIGHT : CPF_WHITE,
                "%-*s %*u %-6s x%u %s\n", printSchemeName? 22 : 14, F_PrettyPath(Str_Text(path)),
                numUidDigits, texId, !record->texture? "unknown" : Texture_IsCustom(record->texture)? "addon" : "game",
                Texture_VariantCount(record->texture), resourcePath? F_PrettyPath(Str_Text(resourcePath)) : "N/A");

    Uri_Delete(uri);
}

/**
 * @todo A horridly inefficent algorithm. This should be implemented in TextureRepository
 * itself and not force users of this class to implement this sort of thing themselves.
 * However this is only presently used for the texture search/listing console commands
 * so is not hugely important right now.
 */
static TextureRepository::Node** collectDirectoryNodes(textureschemeid_t schemeId,
    de::String like, uint* count, TextureRepository::Node** storage)
{
    textureschemeid_t fromId, toId;

    if(VALID_TEXTURESCHEMEID(schemeId))
    {
        // Only consider textures in this scheme.
        fromId = toId = schemeId;
    }
    else
    {
        // Consider textures in any scheme.
        fromId = TEXTURESCHEME_FIRST;
        toId   = TEXTURESCHEME_LAST;
    }

    int idx = 0;
    for(uint i = uint(fromId); i <= uint(toId); ++i)
    {
        TextureRepository& directory = schemeById(textureschemeid_t(i));

        DENG2_FOR_EACH_CONST(TextureRepository::Nodes, nodeIt, directory.leafNodes())
        {
            TextureRepository::Node& node = **nodeIt;
            if(!like.isEmpty())
            {
                de::String path = node.composePath();
                if(!path.beginsWith(like)) continue; // Continue iteration.
            }

            if(storage)
            {
                // Store mode.
                storage[idx++] = &node;
            }
            else
            {
                // Count mode.
                ++idx;
            }
        }
    }

    if(storage)
    {
        storage[idx] = NULL; // Terminate.
        if(count) *count = idx;
        return storage;
    }

    if(idx == 0)
    {
        if(count) *count = 0;
        return NULL;
    }

    storage = static_cast<TextureRepository::Node**>(M_Malloc(sizeof *storage * (idx+1)));
    if(!storage)
    {
        throw de::Error("Textures::collectDirectoryNodes",
                        de::String("Failed on allocation of %1 bytes for new collection.")
                            .arg((unsigned long) (sizeof* storage * (idx+1)) ));
    }
    return collectDirectoryNodes(schemeId, like, count, storage);
}

static int composeAndCompareDirectoryNodePaths(void const* a, void const* b)
{
    // Decode paths before determining a lexicographical delta.
    TextureRepository::Node const& nodeA = **(TextureRepository::Node const**)a;
    TextureRepository::Node const& nodeB = **(TextureRepository::Node const**)b;
    QByteArray pathAUtf8 = nodeA.composePath().toUtf8();
    QByteArray pathBUtf8 = nodeB.composePath().toUtf8();
    AutoStr* pathA = Str_PercentDecode(AutoStr_FromTextStd(pathAUtf8));
    AutoStr* pathB = Str_PercentDecode(AutoStr_FromTextStd(pathBUtf8));
    return Str_CompareIgnoreCase(pathA, Str_Text(pathB));
}

/**
 * @defgroup printTextureFlags  Print Texture Flags
 * @ingroup flags
 */
///@{
#define PTF_TRANSFORM_PATH_NO_SCHEME        0x1 ///< Do not print the scheme.
///@}

#define DEFAULT_PRINTTEXTUREFLAGS           0

/**
 * @param flags  @ref printTextureFlags
 */
static size_t printTextures3(textureschemeid_t schemeId, const char* like, int flags)
{
    const bool printSchemeName = !(flags & PTF_TRANSFORM_PATH_NO_SCHEME);
    uint count = 0;
    TextureRepository::Node** foundTextures = collectDirectoryNodes(schemeId, like, &count, NULL);

    if(!foundTextures) return 0;

    if(!printSchemeName)
        Con_FPrintf(CPF_YELLOW, "Known textures in scheme '%s'", Str_Text(Textures_SchemeName(schemeId)));
    else // Any scheme.
        Con_FPrintf(CPF_YELLOW, "Known textures");

    if(like && like[0])
        Con_FPrintf(CPF_YELLOW, " like \"%s\"", like);
    Con_FPrintf(CPF_YELLOW, ":\n");

    // Print the result index key.
    int numFoundDigits = MAX_OF(3/*idx*/, M_NumDigits((int)count));
    int numUidDigits   = MAX_OF(3/*uid*/, M_NumDigits((int)Textures_Size()));

    Con_Printf(" %*s: %-*s %*s origin x# path\n", numFoundDigits, "idx",
        printSchemeName? 22 : 14, printSchemeName? "scheme:name" : "name",
        numUidDigits, "uid");
    Con_PrintRuler();

    // Sort and print the index.
    qsort(foundTextures, (size_t)count, sizeof(*foundTextures), composeAndCompareDirectoryNodePaths);

    uint idx = 0;
    for(TextureRepository::Node** iter = foundTextures; *iter; ++iter)
    {
        TextureRepository::Node& node = **iter;
        Con_Printf(" %*u: ", numFoundDigits, idx++);
        printTextureOverview(node, printSchemeName);
    }

    M_Free(foundTextures);
    return count;
}

static void printTextures2(textureschemeid_t schemeId, const char* like, int flags)
{
    size_t printTotal = 0;
    // Do we care which scheme?
    if(schemeId == TS_ANY && like && like[0])
    {
        printTotal = printTextures3(schemeId, like, flags & ~PTF_TRANSFORM_PATH_NO_SCHEME);
        Con_PrintRuler();
    }
    // Only one scheme to print?
    else if(VALID_TEXTURESCHEMEID(schemeId))
    {
        printTotal = printTextures3(schemeId, like, flags | PTF_TRANSFORM_PATH_NO_SCHEME);
        Con_PrintRuler();
    }
    else
    {
        // Collect and sort in each scheme separately.
        for(int i = TEXTURESCHEME_FIRST; i <= TEXTURESCHEME_LAST; ++i)
        {
            size_t printed = printTextures3((textureschemeid_t)i, like, flags | PTF_TRANSFORM_PATH_NO_SCHEME);
            if(printed != 0)
            {
                printTotal += printed;
                Con_PrintRuler();
            }
        }
    }
    Con_Printf("Found %lu %s.\n", (unsigned long) printTotal, printTotal == 1? "Texture" : "Textures");
}

static void printTextures(textureschemeid_t schemeId, const char* like)
{
    printTextures2(schemeId, like, DEFAULT_PRINTTEXTUREFLAGS);
}

D_CMD(ListTextures)
{
    DENG2_UNUSED(src);

    if(!Textures_Size())
    {
        Con_Message("There are currently no textures defined/loaded.\n");
        return true;
    }

    textureschemeid_t schemeId = TS_ANY;
    char const* like = 0;
    de::Uri uri;

    // "listtextures [scheme] [path]"
    if(argc > 2)
    {
        uri.setScheme(argv[1]).setPath(argv[2]);

        schemeId = Textures_ParseSchemeName(uri.schemeCStr());
        if(!VALID_TEXTURESCHEMEID(schemeId))
        {
            Con_Printf("Invalid scheme \"%s\".\n", uri.schemeCStr());
            return false;
        }
        like = uri.pathCStr();
    }
    // "listtextures [scheme:name]" (i.e., a partial URI)
    else if(argc > 1)
    {
        uri = uri.setUri(argv[1], RC_NULL);

        if(!uri.scheme().isEmpty())
        {
            schemeId = Textures_ParseSchemeName(uri.schemeCStr());
            if(!VALID_TEXTURESCHEMEID(schemeId))
            {
                Con_Printf("Invalid scheme \"%s\".\n", uri.schemeCStr());
                return false;
            }

            if(!uri.path().isEmpty())
                like = uri.pathCStr();
        }
        else
        {
            schemeId = Textures_ParseSchemeName(uri.pathCStr());
            if(!VALID_TEXTURESCHEMEID(schemeId))
            {
                schemeId = TS_ANY;
                like = argv[1];
            }
        }
    }

    printTextures(schemeId, like);

    return true;
}

D_CMD(InspectTexture)
{
    DENG2_UNUSED(src);
    DENG2_UNUSED(argc);

    // Path is assumed to be in a human-friendly, non-encoded representation.
    Str path; Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, argv[1]));

    de::Uri search = de::Uri(Str_Text(&path), RC_NULL);
    Str_Free(&path);

    if(!search.scheme().isEmpty())
    {
        textureschemeid_t schemeId = Textures_ParseSchemeName(search.schemeCStr());
        if(!VALID_TEXTURESCHEMEID(schemeId))
        {
            Con_Printf("Invalid scheme \"%s\".\n", search.schemeCStr());
            return false;
        }
    }

    Texture* tex = Textures_ToTexture(Textures_ResolveUri(reinterpret_cast<Uri*>(&search)));
    if(tex)
    {
        printTextureInfo(tex);
    }
    else
    {
        AutoStr* path = Uri_ToString(reinterpret_cast<Uri*>(&search));
        Con_Printf("Unknown texture \"%s\".\n", Str_Text(path));
    }

    return true;
}

#if _DEBUG
D_CMD(PrintTextureStats)
{
    DENG2_UNUSED(src);
    DENG2_UNUSED(argc);
    DENG2_UNUSED(argv);

    if(!Textures_Size())
    {
        Con_Message("There are currently no textures defined/loaded.\n");
        return true;
    }

    Con_FPrintf(CPF_YELLOW, "Texture Statistics:\n");
    for(uint i = uint(TEXTURESCHEME_FIRST); i <= uint(TEXTURESCHEME_LAST); ++i)
    {
        textureschemeid_t schemeId = textureschemeid_t(i);
        TextureRepository& directory = schemeById(schemeId);

        uint size = directory.size();
        Con_Printf("Scheme: %s (%u %s)\n", Str_Text(Textures_SchemeName(schemeId)), size, size==1? "texture":"textures");
        directory.debugPrintHashDistribution();
        directory.debugPrint();
    }
    return true;
}
#endif
