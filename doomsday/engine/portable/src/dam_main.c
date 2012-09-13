/**
 * @file dam_main.c
 * Doomsday Archived Map (DAM), map management. @ingroup map
 *
 * @authors Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
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

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_dam.h"
#include "de_defs.h"
#include "de_edit.h"
#include "de_filesys.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_render.h"

static void freeArchivedMap(archivedmap_t* dam);

// Should we be caching successfully loaded maps?
byte mapCache = true;

static const char* mapCacheDir = "mapcache/";

static archivedmap_t** archivedMaps = NULL;
static uint numArchivedMaps = 0;

static void clearArchivedMaps(void)
{
    if(archivedMaps)
    {
        uint i;
        for(i = 0; i < numArchivedMaps; ++i)
        {
            freeArchivedMap(archivedMaps[i]);
        }
        Z_Free(archivedMaps);
        archivedMaps = NULL;
    }
    numArchivedMaps = 0;
}

void DAM_Register(void)
{
    C_VAR_BYTE("map-cache", &mapCache, 0, 0, 1);
}

void DAM_Init(void)
{
    // Allow re-init.
    clearArchivedMaps();
}

void DAM_Shutdown(void)
{
    clearArchivedMaps();
}

/**
 * Allocate memory for a new archived map record.
 */
static archivedmap_t* allocArchivedMap(void)
{
    archivedmap_t* dam = Z_Calloc(sizeof(*dam), PU_APPSTATIC, 0);
    if(!dam)
        Con_Error("allocArchivedMap: Failed on allocation of %lu bytes for new ArchivedMap.",
                  (unsigned long) sizeof(*dam));
    return dam;
}

/// Free all memory acquired for an archived map record.
static void freeArchivedMap(archivedmap_t* dam)
{
    assert(dam);
    Uri_Delete(dam->uri);
    Str_Free(&dam->cachedMapPath);
    Z_Free(dam);
}

/// Create a new archived map record.
static archivedmap_t* createArchivedMap(const Uri* uri, const ddstring_t* cachedMapPath)
{
    const char* mapId = Str_Text(Uri_Path(uri));
    archivedmap_t* dam = allocArchivedMap();

    VERBOSE(
        AutoStr* path = Uri_ToString(uri);
        Con_Message("createArchivedMap: Add record for map '%s'.\n", Str_Text(path));
        )

    dam->uri = Uri_NewCopy(uri);
    dam->lastLoadAttemptFailed = false;
    Str_Init(&dam->cachedMapPath);
    Str_Set(&dam->cachedMapPath, Str_Text(cachedMapPath));

    if(DAM_MapIsValid(Str_Text(&dam->cachedMapPath), F_CheckLumpNumForName2(mapId, true)))
        dam->cachedMapFound = true;

    return dam;
}

/**
 * Search the list of maps for one matching the specified identifier.
 *
 * @param uri  Identifier of the map to be searched for.
 * @return  The found map else @c NULL.
 */
static archivedmap_t* findArchivedMap(const Uri* uri)
{
    if(numArchivedMaps)
    {
        archivedmap_t** p = archivedMaps;
        while(*p)
        {
            archivedmap_t *dam = *p;
            if(Uri_Equality(dam->uri, uri))
            {
                return dam;
            }
            p++;
        }
    }
    return 0;
}

/**
 * Add an archived map to the list of maps.
 */
static void addArchivedMap(archivedmap_t* dam)
{
    archivedMaps = Z_Realloc(archivedMaps, sizeof(archivedmap_t*) * (++numArchivedMaps + 1), PU_APPSTATIC);
    archivedMaps[numArchivedMaps - 1] = dam;
    archivedMaps[numArchivedMaps] = NULL; // Terminate.
}

/// Calculate the identity key for maps loaded from this path.
static ushort calculateIdentifierForMapPath(const char* path)
{
    if(path && path[0])
    {
        ushort identifier = 0;
        size_t i;
        for(i = 0; path[i]; ++i)
            identifier ^= path[i] << ((i * 3) % 11);
        return identifier;
    }
    Con_Error("calculateMapIdentifier: Empty/NULL path given.");
    return 0; // Unreachable.
}

AutoStr* DAM_ComposeCacheDir(const char* sourcePath)
{
    const Str* gameIdentityKey;
    ushort mapPathIdentifier;
    Str mapFileName;
    AutoStr* path;

    if(!sourcePath || !sourcePath[0]) return NULL;

    gameIdentityKey = Game_IdentityKey(theGame);
    mapPathIdentifier = calculateIdentifierForMapPath(sourcePath);
    Str_InitStd(&mapFileName);
    F_FileName(&mapFileName, sourcePath);

    // Compose the final path.
    path = AutoStr_NewStd();
    Str_Appendf(path, "%s%s/%s-%04X/", mapCacheDir, Str_Text(gameIdentityKey),
        Str_Text(&mapFileName), mapPathIdentifier);
    F_ExpandBasePath(path, path);

    Str_Free(&mapFileName);
    return path;
}

static boolean loadMap(GameMap** map, archivedmap_t* dam)
{
    *map = (GameMap*) Z_Calloc(sizeof(**map), PU_MAPSTATIC, 0);
    if(!*map)
        Con_Error("loadMap: Failed on allocation of %lu bytes for new Map.", (unsigned long) sizeof(**map));
    return DAM_MapRead(*map, Str_Text(&dam->cachedMapPath));
}

static boolean convertMap(GameMap** map, archivedmap_t* dam)
{
    boolean converted = false;

    VERBOSE(
        AutoStr* path = Uri_ToString(dam->uri);
        Con_Message("convertMap: Attempting conversion of '%s'.\n", Str_Text(path));
        );

    // Any converters available?
    if(Plug_CheckForHook(HOOK_MAP_CONVERT))
    {
        // Ask each converter in turn whether the map format is recognised
        // and if so, to interpret/transfer it to us via the map edit interface.
        if(DD_CallHooks(HOOK_MAP_CONVERT, 0, (void*) dam->uri))
        {
            // Transfer went OK.
            // Were we able to produce a valid map?
            converted = MPE_GetLastBuiltMapResult();
            if(converted)
            {
                *map = MPE_GetLastBuiltMap();
            }
        }
    }

    if(!converted || verbose >= 2)
        Con_Message("convertMap: %s.\n", (converted? "Successful" : "Failed"));

    return converted;
}

/**
 * Attempt to load the map associated with the specified identifier.
 */
boolean DAM_AttemptMapLoad(const Uri* uri)
{
    boolean loadedOK = false;
    archivedmap_t* dam;

    assert(uri);

    VERBOSE2(
        AutoStr* path = Uri_ToString(uri);
        Con_Message("DAM_AttemptMapLoad: Loading '%s'...\n", Str_Text(path));
        )

    dam = findArchivedMap(uri);
    if(!dam)
    {
        // We've not yet attempted to load this map.
        const char* mapId = Str_Text(Uri_Path(uri));
        lumpnum_t markerLump;
        AutoStr* cachedMapDir;
        Str cachedMapPath;

        markerLump = F_CheckLumpNumForName2(mapId, true /*quiet please*/);
        if(0 > markerLump) return false;

        // Compose the cache directory path and ensure it exists.
        cachedMapDir = DAM_ComposeCacheDir(F_LumpSourceFile(markerLump));
        F_MakePath(Str_Text(cachedMapDir));

        // Compose the full path to the cached map data file.
        Str_InitStd(&cachedMapPath);
        F_FileName(&cachedMapPath, F_LumpName(markerLump));
        Str_Append(&cachedMapPath, ".dcm");
        Str_Prepend(&cachedMapPath, Str_Text(cachedMapDir));

        // Create an archived map record for this.
        dam = createArchivedMap(uri, &cachedMapPath);
        addArchivedMap(dam);

        Str_Free(&cachedMapPath);
    }

    // Load it in.
    if(!dam->lastLoadAttemptFailed)
    {
        GameMap* map = NULL;
        ded_mapinfo_t* mapInfo;

        P_SetCurrentMap(0);
        //Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);

        if(mapCache && dam->cachedMapFound)
        {
            // Attempt to load the cached map data.
            if(loadMap(&map, dam))
                loadedOK = true;
        }
        else
        {
            // Try a JIT conversion with the help of a plugin.
            if(convertMap(&map, dam))
                loadedOK = true;
        }

        if(loadedOK)
        {
            ded_sky_t* skyDef = NULL;
            vec2d_t min, max;
            uint i;

            // Do any initialization/error checking work we need to do.
            // Must be called before we go any further.
            P_InitUnusedMobjList();

            // Must be called before any mobjs are spawned.
            GameMap_InitNodePiles(map);

            // Prepare the client-side data.
            if(isClient)
            {
                GameMap_InitClMobjs(map);
            }

            Rend_DecorInit();

            // Init blockmap for searching BSP leafs.
            GameMap_Bounds(map, min, max);
            GameMap_InitBspLeafBlockmap(map, min, max);
            for(i = 0; i < map->numBspLeafs; ++i)
            {
                GameMap_LinkBspLeaf(map, GameMap_BspLeaf(map, i));
            }

            map->uri = Uri_NewCopy(dam->uri);
            strncpy(map->uniqueId, P_GenerateUniqueMapId(Str_Text(Uri_Path(map->uri))), sizeof(map->uniqueId));

            // See what mapinfo says about this map.
            mapInfo = Def_GetMapInfo(map->uri);
            if(!mapInfo)
            {
                Uri* mapUri = Uri_NewWithPath2("*", RC_NULL);
                mapInfo = Def_GetMapInfo(mapUri);
                Uri_Delete(mapUri);
            }

            if(mapInfo)
            {
                skyDef = Def_GetSky(mapInfo->skyID);
                if(!skyDef)
                    skyDef = &mapInfo->sky;
            }

            R_SetupSky(skyDef);

            // Setup accordingly.
            if(mapInfo)
            {
                map->globalGravity = mapInfo->gravity;
                map->ambientLightLevel = mapInfo->ambient * 255;
            }
            else
            {
                // No map info found, so set some basic stuff.
                map->globalGravity = 1.0f;
                map->ambientLightLevel = 0;
            }

            map->effectiveGravity = map->globalGravity;

            // @todo should be called from P_LoadMap() but R_InitMap requires the
            //       theMap to be set first.
            P_SetCurrentMap(map);

            R_InitFakeRadioForMap();

            { uint startTime = Sys_GetRealTime();
            GameMap_InitSkyFix(map);
            VERBOSE2( Con_Message("Initial sky fix done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
            }
        }
    }

    if(!loadedOK)
        dam->lastLoadAttemptFailed = true;

    return loadedOK;
}
