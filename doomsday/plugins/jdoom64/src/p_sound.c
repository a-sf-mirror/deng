/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

// HEADER FILES ------------------------------------------------------------

#include "jdoom64.h"

#include "dmu_lib.h"
#include "g_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Starts playing the music for the specified map.
 */
void S_MapMusic(uint episode, uint map)
{
    ddmapinfo_t mapInfo;
    char mapId[9];

    P_GetMapLumpName(mapId, episode, map);
    if(Def_Get(DD_DEF_MAP_INFO, mapId, &mapInfo))
    {
        S_StartMusicNum(mapInfo.music, true);
    }
}

/**
 * Doom-like sector sounds: when a new sound starts, stop any old ones
 * from the same origin.
 *
 * @param sec           Sector in which the sound should be played.
 * @param id            ID number of the sound to be played.
 */
void S_SectorSound(sector_t* sec, int id)
{
    mobj_t* origin = (mobj_t*) DMU_GetPtrp(sec, DMU_SOUND_ORIGIN);

    S_StopSound(0, origin);
    S_StartSound(id, origin);
}

/**
 * Doom-like sector sounds: when a new sound starts, stop any old ones
 * from the same origin.
 *
 * @param sec           Sector in which the sound should be played.
 * @param origin        Origin of the sound (center/floor/ceiling).
 * @param id            ID number of the sound to be played.
 */
void S_PlaneSound(sector_t* sec, int plane, int id)
{
    mobj_t* origin = (mobj_t*) DMU_GetPtrp(sec, DMU_SOUND_ORIGIN);

    S_StopSound(0, origin);
    S_StartSound(id, origin);
}
