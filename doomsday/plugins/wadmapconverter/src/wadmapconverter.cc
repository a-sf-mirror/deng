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

typedef struct maplumpinfo_s {
    int         lumpNum;
    size_t      length;
} maplumpinfo_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

int ConvertMapHook(int hookType, int parm, void *data);
int LoadResources(int hookType, int param, void* data);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

wadmap_t theMap, *wadmap = &theMap;
boolean verbose;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_INIT, LoadResources);
    Plug_AddHook(HOOK_UPDATE, LoadResources);
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

    int i;

    if(!name)
        return ML_INVALID;

    for(i = 0; mapLumpInfos[i].type > ML_INVALID; ++i)
    {
        if(!strcmp(name, mapLumpInfos[i].name))
            return mapLumpInfos[i].type;
    }

    return ML_INVALID;
}

int LoadResources(int hookType, int param, void* data)
{
    Con_Message("WadMapConverter::LoadResources: Processing...\n");

    LoadANIMATED();
    LoadANIMDEFS();

    return true;
}

/**
 * This function is called when Doomsday is asked to load a map that is not
 * presently available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map
 * editing interface to recreate the map in native format.
 */
int ConvertMapHook(int hookType, int param, void* data)
{
    lumpnum_t i, numLumps, totalLumps, startLump;
    lumpnum_t* lumpList;
    char* mapID = (char*) data;

    if((startLump = W_CheckNumForName(mapID)) == -1)
        return false;

    // Add the marker lump to the list of lumps for this map.
    lumpList = (lumpnum_t*) malloc(sizeof(lumpnum_t));
    lumpList[0] = startLump;
    numLumps = 1;

    /**
     * Find the lumps associated with this map dataset and link them to the
     * archivedmap record.
     *
     * \note Some obscure PWADs have these lumps in a non-standard order,
     * so we need to go resort to finding them automatically.
     */
    totalLumps = W_NumLumps();

    VERBOSE(Con_Message("WadMapConverter::Convert: Locating lumps...\n"));

    // Keep checking lumps to see if its a map data lump.
    for(i = startLump + 1; i < totalLumps; ++i)
    {
        const char* lumpName;
        int lumpType;

        // Lookup the lump name in our list of known map lump names.
        lumpName = W_LumpName(i);

        // @todo Do more validity checking.
        lumpType = mapLumpTypeForName(lumpName);

        if(lumpType != ML_INVALID)
        {   // Its a known map lump.
            lumpList = (lumpnum_t*) realloc(lumpList, sizeof(lumpnum_t) * ++numLumps);
            lumpList[numLumps - 1] = i;
            continue;
        }

        // Stop looking, we *should* have found them all.
        break;
    }

    verbose = ArgExists("-verbose");

    Con_Message("WadMapConverter::Convert: Attempting map conversion...\n");
    memset(wadmap, 0, sizeof(*wadmap));

    if(!IsSupportedFormat(lumpList, numLumps))
    {
        Con_Message("WadMapConverter::Convert: Unknown map format, aborting.\n");
        free(lumpList);
        return false; // Cannot convert.
    }

    // A supported format.
    Con_Message("WadMapConverter::Convert: %s map format.\n",
                (wadmap->format == MF_DOOM64? "DOOM64" :
                 wadmap->format == MF_HEXEN? "Hexen" : "DOOM"));

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
