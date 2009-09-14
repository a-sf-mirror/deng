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
 * wadmapconverter.c: Doomsday Plugin for converting DOOM-like format maps.
 *
 * The purpose of a wadmapconverter plugin is to transform a map into
 * Doomsday's native map format by use of the public map editing interface.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "doomsday.h"
#include "dd_api.h"

#include "wadmapconverter.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

enum {
    ML_INVALID = -1,
    ML_LABEL, // A separator, name, ExMx or MAPxx
    ML_THINGS, // Monsters, items..
    ML_LINEDEFS, // LineDefs, from editing
    ML_SIDEDEFS, // SideDefs, from editing
    ML_VERTEXES, // Vertices, edited and BSP splits generated
    ML_SEGS, // LineSegs, from LineDefs split by BSP
    ML_SSECTORS, // SubSectors, list of LineSegs
    ML_NODES, // BSP nodes
    ML_SECTORS, // Sectors, from editing
    ML_REJECT, // LUT, sector-sector visibility
    ML_BLOCKMAP, // LUT, motion clipping, walls/grid element
    ML_BEHAVIOR, // ACS Scripts (compiled).
    ML_SCRIPTS, // ACS Scripts (source).
    ML_LIGHTS, // Surface color tints.
    ML_MACROS, // DOOM64 format, macro scripts.
    ML_LEAFS, // DOOM64 format, segs (close subsectors).
    NUM_MAPLUMPS
};

typedef struct maplumpformat_s {
    int         hversion;
    char*       formatName;
    int         lumpClass;
} maplumpformat_t;

typedef struct maplumpinfo_s {
    int         lumpNum;
    maplumpformat_t* format;
    int         lumpClass;
    int         startOffset;
    uint        elements;
    size_t      length;
} maplumpinfo_t;

typedef struct listnode_s {
    void*       data;
    struct listnode_s* next;
} listnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int ConvertMapHook(int hookType, int parm, void *data);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

map_t theMap, *map = &theMap;
boolean verbose;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_MAP_CONVERT, ConvertMapHook);
}

#ifdef WIN32
/**
 * Windows calls this when the DLL is loaded.
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch(fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // Register our hooks.
        DP_Initialize();
        break;

    default:
        break;
    }

    return TRUE;
}
#endif

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
    listnode_t         *node = calloc(1, sizeof(listnode_t));
    return node;
}

/**
 * Free all memory acquired for the given list node.
 */
static void freeListNode(listnode_t* node)
{
    if(node)
        free(node);
}

/**
 * Allocate memory for a new map lump info record.
 */
static maplumpinfo_t* allocMapLumpInfo(void)
{
    maplumpinfo_t *info = calloc(1, sizeof(maplumpinfo_t));
    return info;
}

/**
 * Free all memory acquired for the given map lump info record.
 */
static void freeMapLumpInfo(maplumpinfo_t *info)
{
    if(info)
        free(info);
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
    int                 i, numLumps = W_NumLumps();
    uint                numCollectedLumps = 0;

    VERBOSE(Con_Message("collectMapLumps: Locating lumps...\n"));

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

    return numCollectedLumps;
}

/**
 * This function is called when Doomsday is asked to load a map that is not
 * presently available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map
 * editing interface to recreate the map in native format.
 */
int ConvertMapHook(int hookType, int param, void *data)
{
    int                 numLumps, startLump;
    int*                lumpList;
    char*               mapID = (char*) data;
    boolean             result = false;

    startLump = W_CheckNumForName(mapID);

    if(startLump == -1)
        return false;

    {   // We've not yet attempted to load this map.
    int                 i;
    listnode_t*         ln, *headPtr = NULL;

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

    verbose = ArgExists("-verbose");

    Con_Message("WadMapConverter::Convert: Attempting map conversion...\n");
    memset(map, 0, sizeof(*map));

    if(!IsSupportedFormat(lumpList, numLumps))
    {
        Con_Message("WadMapConverter::Convert: Unknown map format, aborting.\n");
        free(lumpList);
        return false; // Cannot convert.
    }

    // A supported format.
    Con_Message("WadMapConverter::Convert: %s map format.\n",
                (map->format == MF_DOOM64? "DOOM64" :
                 map->format == MF_HEXEN? "Hexen" : "DOOM"));

    // Load it in.
    if(!LoadMap(lumpList, numLumps))
    {
        Con_Message("WadMapConverter::Convert: Internal error, load failed.\n");
        free(lumpList);
        return false; // Something went horribly wrong...
    }
    free(lumpList);

    AnalyzeMap();
    return TransferMap();
}
