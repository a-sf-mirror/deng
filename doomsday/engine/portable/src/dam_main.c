/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * dam_main.c: Doomsday Archived Map (DAM), map management.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_dam.h"
#include "de_misc.h"
#include "de_refresh.h"
#include "de_defs.h"
#include "de_edit.h"
#include "de_play.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static int mapLumpTypeForName(const char* name)
{
    struct maplumpinfo_s {
        int         type;
        const char *name;
    } mapLumpInfos[] =
    {
        {ML_THINGS,     "THINGS"},
        {ML_LINEDEFS,   "LINEDEFS"},
        {ML_SIDEDEFS,   "SIDEDEFS"},
        {ML_VERTEXES,   "VERTEXES"},
        {ML_SEGS,       "SEGS"},
        {ML_SSECTORS,   "SSECTORS"},
        {ML_NODES,      "NODES"},
        {ML_SECTORS,    "SECTORS"},
        {ML_REJECT,     "REJECT"},
        {ML_BLOCKMAP,   "BLOCKMAP"},
        {ML_BEHAVIOR,   "BEHAVIOR"},
        {ML_SCRIPTS,    "SCRIPTS"},
        {ML_LIGHTS,     "LIGHTS"},
        {ML_MACROS,     "MACROS"},
        {ML_LEAFS,      "LEAFS"},
        {ML_INVALID,    NULL}
    };

    int         i;

    if(!name)
        return ML_INVALID;

    for(i = 0; mapLumpInfos[i].type > ML_INVALID; ++i)
    {
        if(!strcmp(name, mapLumpInfos[i].name))
            return mapLumpInfos[i].type;
    }

    return ML_INVALID;
}

/**
 * Allocate a new list node.
 */
static listnode_t* allocListNode(void)
{
    listnode_t         *node = Z_Calloc(sizeof(listnode_t), PU_STATIC, 0);
    return node;
}

/**
 * Free all memory acquired for the given list node.
 */
static void freeListNode(listnode_t *node)
{
    if(node)
        Z_Free(node);
}

/**
 * Allocate memory for a new map lump info record.
 */
static maplumpinfo_t* allocMapLumpInfo(void)
{
    maplumpinfo_t *info = Z_Calloc(sizeof(maplumpinfo_t), PU_STATIC, 0);
    return info;
}

/**
 * Free all memory acquired for the given map lump info record.
 */
static void freeMapLumpInfo(maplumpinfo_t *info)
{
    if(info)
        Z_Free(info);
}

/**
 * Free a list of maplumpinfo records.
 */
static void freeMapLumpInfoList(listnode_t* headPtr)
{
    listnode_t*         node, *np;

    node = headPtr;
    while(node)
    {
        np = node->next;

        if(node->data)
            freeMapLumpInfo(node->data);
        freeListNode(node);

        node = np;
    }
}

/**
 * Create a new map lump info record.
 */
static maplumpinfo_t* createMapLumpInfo(int lumpNum, int lumpClass)
{
    maplumpinfo_t*      info = allocMapLumpInfo();

    info->lumpNum = lumpNum;
    info->lumpClass = lumpClass;
    info->length = W_LumpLength(lumpNum);
    info->format = NULL;
    info->startOffset = 0;

    return info;
}

/**
 * Link a maplumpinfo record to an archivedmap record.
 */
static void addLumpInfoToList(listnode_t** headPtr, maplumpinfo_t* info)
{
    listnode_t*         node = allocListNode();

    node->data = info;

    node->next = *headPtr;
    *headPtr = node;
}

/**
 * Find the lumps associated with this map dataset and link them to the
 * archivedmap record.
 *
 * \note Some obscure PWADs have these lumps in a non-standard order,
 * so we need to go resort to finding them automatically.
 *
 * @param headPtr       The list to link the created maplump records to.
 * @param startLump     The lump number to begin our search with.
 *
 * @return              The number of collected lumps.
 */
static uint collectMapLumps(listnode_t** headPtr, int startLump)
{
    int                 i;
    uint                numCollectedLumps = 0;

    VERBOSE(Con_Message("collectMapLumps: Locating lumps...\n"));

    if(startLump > 0 && startLump < numLumps)
    {
        // Keep checking lumps to see if its a map data lump.
        for(i = startLump; i < numLumps; ++i)
        {
            int                 lumpType;
            const char*         lumpName;

            // Lookup the lump name in our list of known map lump names.
            lumpName = W_LumpName(i);
            lumpType = mapLumpTypeForName(lumpName);

            if(lumpType != ML_INVALID)
            {   // Its a known map lump.
                maplumpinfo_t *info = createMapLumpInfo(i, lumpType);

                addLumpInfoToList(headPtr, info);
                numCollectedLumps++;
                continue;
            }

            // Stop looking, we *should* have found them all.
            break;
        }
    }

    return numCollectedLumps;
}

static boolean convertMap(gamemap_t** map, const char* file, const char* mapID,
                          int* lumpList, int numLumps)
{
    boolean             converted = false;

    Con_Message("convertMap: Attempting conversion of \"%s\".\n",
                mapID);

    // Nope. See if there is a converter available.
    if(Plug_CheckForHook(HOOK_MAP_CONVERT))
    {
        // Pass the lump list around the map converters, hopefully
        // one of them will recognise the format and convert it.
        if(Plug_DoHook(HOOK_MAP_CONVERT, numLumps, (void*) lumpList))
        {
            converted = true;
            *map = MPE_GetLastBuiltMap();
        }
    }

    Con_Message("convertMap: %s.\n", (converted? "Successful" : "Failed"));
    return converted;
}

/**
 * Attempt to load the map associated with the specified identifier.
 */
boolean DAM_AttemptMapLoad(const char* mapID)
{
    int                 numLumps;
    int*                lumpList;
    lumpnum_t           startLump;
    boolean             loadedOK = false;

    VERBOSE2(
    Con_Message("DAM_AttemptMapLoad: Loading \"%s\"...\n", mapID));

    startLump = W_CheckNumForName(mapID);

    {   // We've not yet attempted to load this map.
    int                 i;
    listnode_t*         ln, *headPtr = NULL;

    if(startLump == -1)
        return false;

    // Add the marker lump to the list of lumps for this map.
    addLumpInfoToList(&headPtr, createMapLumpInfo(startLump, ML_LABEL));

    // Find the rest of the map data lumps associated with this map.
    collectMapLumps(&headPtr, startLump + 1);

    // Count the lumps for this map
    numLumps = 0;
    ln = headPtr;
    while(ln)
    {
        numLumps++;
        ln = ln->next;
    }

    // Allocate an array of the lump indices.
    lumpList = malloc(sizeof(int) * numLumps);
    ln = headPtr;
    i = 0;
    while(ln)
    {
        maplumpinfo_t      *info = (maplumpinfo_t*) ln->data;

        lumpList[i++] = info->lumpNum;
        ln = ln->next;
    }

    freeMapLumpInfoList(headPtr);
    }

    // Load it in.
    {
    gamemap_t      *map = NULL;
    ded_mapinfo_t  *mapInfo;

    // Try a JIT conversion with the help of a plugin.
    if(convertMap(&map, W_LumpSourceFile(startLump), mapID, lumpList, numLumps))
        loadedOK = true;

    if(loadedOK)
    {
        ded_sky_t*          skyDef = NULL;

        // Do any initialization/error checking work we need to do.
        // Must be called before we go any further.
        P_InitUnusedMobjList();

        // Must be called before any mobjs are spawned.
        R_InitLinks(map);

        R_BuildSectorLinks(map);

        // Init blockmap for searching subsectors.
        P_BuildSubsectorBlockMap(map);

        // Init the watched object lists.
        memset(&map->watchedPlaneList, 0, sizeof(map->watchedPlaneList));
        memset(&map->movingSurfaceList, 0, sizeof(map->movingSurfaceList));
        memset(&map->decoratedSurfaceList, 0, sizeof(map->decoratedSurfaceList));

        strncpy(map->mapID, mapID, 8);
        strncpy(map->uniqueID, P_GenerateUniqueMapID(mapID),
                sizeof(map->uniqueID));

        // See what mapinfo says about this map.
        mapInfo = Def_GetMapInfo(map->mapID);
        if(!mapInfo)
            mapInfo = Def_GetMapInfo("*");

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

        // \todo should be called from P_LoadMap() but R_InitMap requires the
        // currentMap to be set first.
        P_SetCurrentMap(map);

        R_InitSectorShadows();

        {
        uint                startTime = Sys_GetRealTime();

        R_InitSkyFix();

        // How much time did we spend?
        VERBOSE(Con_Message
                ("  InitialSkyFix: Done in %.2f seconds.\n",
                 (Sys_GetRealTime() - startTime) / 1000.0f));
        }
    }
    }

    free(lumpList);
    return loadedOK;
}
