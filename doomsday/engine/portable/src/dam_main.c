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

static boolean convertMap(gamemap_t** map, const char* file, const char* mapID)
{
    boolean             converted = false;

    Con_Message("convertMap: Attempting conversion of \"%s\".\n",
                mapID);

    // Nope. See if there is a converter available.
    if(Plug_CheckForHook(HOOK_MAP_CONVERT))
    {
        // Pass the lump list around the map converters, hopefully
        // one of them will recognise the format and convert it.
        if(Plug_DoHook(HOOK_MAP_CONVERT, 0, (void*) mapID))
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
    lumpnum_t           startLump;
    boolean             loadedOK = false;

    VERBOSE2(
    Con_Message("DAM_AttemptMapLoad: Loading \"%s\"...\n", mapID));

    startLump = W_CheckNumForName(mapID);

    if(startLump == -1)
        return false;

    // Load it in.
    {
    gamemap_t      *map = NULL;
    ded_mapinfo_t  *mapInfo;

    // Try a JIT conversion with the help of a plugin.
    if(convertMap(&map, W_LumpSourceFile(startLump), mapID))
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

    return loadedOK;
}
