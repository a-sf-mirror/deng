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

static boolean convertMap(const char* mapID)
{
    boolean converted = false;

    Con_Message("convertMap: Attempting conversion of \"%s\".\n", mapID);

    // Nope. See if there is a converter available.
    if(Plug_CheckForHook(HOOK_MAP_CONVERT))
    {
        // Pass the lump list around the map converters, hopefully
        // one of them will recognise the format and convert it.
        if(Plug_DoHook(HOOK_MAP_CONVERT, 0, (void*) mapID))
            converted = true;
    }

    Con_Message("convertMap: %s.\n", (converted? "Successful" : "Failed"));
    return converted;
}

ddstring_t* DAM_ComposeArchiveMapFilepath(const char* mapID)
{
    ddstring_t* s = Str_New();

    Str_Init(s);
    Str_Appendf(s, "%s.pk3", mapID);

    return s;
}

/**
 * Generate a 'unique' identifier for the map.  This identifier
 * contains information about the map tag (E3M3), the WAD that
 * contains the map (DOOM.IWAD), and the game mode (doom-ultimate).
 *
 * The entire ID string will be in lowercase letters.
 */
const char* DAM_GenerateUniqueMapName(const char* mapID)
{
    static char uid[255];
    filename_t base;
    int lump = W_GetNumForName(mapID);

    M_ExtractFileBase(base, W_LumpSourceFile(lump), FILENAME_T_MAXLEN);

    dd_snprintf(uid, 255, "%s|%s|%s|%s", mapID,
                base, (W_IsFromIWAD(lump) ? "iwad" : "pwad"),
                (char*) gx.GetVariable(DD_GAME_MODE));

    strlwr(uid);
    return uid;
}

/**
 * Attempt to load the map associated with the specified identifier.
 */
boolean DAM_TryMapConversion(const char* mapID)
{
    // Load it in. Try a JIT conversion with the help of a plugin.
    // Destroy DMU obj records for map-owned objects.
    P_DestroyObjectRecordsByType(DMU_VERTEX);
    P_DestroyObjectRecordsByType(DMU_LINEDEF);
    P_DestroyObjectRecordsByType(DMU_SIDEDEF);
    P_DestroyObjectRecordsByType(DMU_PLANE);
    P_DestroyObjectRecordsByType(DMU_SECTOR);
    P_DestroyObjectRecordsByType(DMU_SEG);
    P_DestroyObjectRecordsByType(DMU_SUBSECTOR);
    P_DestroyObjectRecordsByType(DMU_NODE);

    return convertMap(mapID);
}

map_t* DAM_LoadMap(const char* mapID)
{
    ddstring_t* s = DAM_ComposeArchiveMapFilepath(mapID);
    map_t* map = P_CreateMap(mapID);

    // Destroy DMU obj records for map-owned objects.
    P_DestroyObjectRecordsByType(DMU_VERTEX);
    P_DestroyObjectRecordsByType(DMU_LINEDEF);
    P_DestroyObjectRecordsByType(DMU_SIDEDEF);
    P_DestroyObjectRecordsByType(DMU_PLANE);
    P_DestroyObjectRecordsByType(DMU_SECTOR);
    P_DestroyObjectRecordsByType(DMU_SEG);
    P_DestroyObjectRecordsByType(DMU_SUBSECTOR);
    P_DestroyObjectRecordsByType(DMU_NODE);

    DAM_MapRead(map, Str_Text(s));

    Str_Delete(s);
    return map;
}
