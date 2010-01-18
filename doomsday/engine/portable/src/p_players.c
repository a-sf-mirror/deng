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
 * p_players.c: Players
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

player_t ddPlayers[DDMAXPLAYERS];
player_t* viewPlayer = &ddPlayers[0];
int consolePlayer;
int displayPlayer;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Determine which console is used by the given local player. Local players
 * are numbered starting from zero.
 */
int P_LocalToConsole(int localPlayer)
{
    int                 i, count;

    for(i = 0, count = 0; i < DDMAXPLAYERS; ++i)
    {
        int console = (i + consolePlayer) % DDMAXPLAYERS;
        player_t*           plr = &ddPlayers[console];
        ddplayer_t*         ddpl = &plr->shared;

        if(ddpl->flags & DDPF_LOCAL)
        {
            if(count++ == localPlayer)
                return console;
        }
    }

    // No match!
    return -1;
}

/**
 * Determine the local player number used by a particular console. Local
 * players are numbered starting from zero.
 */
int P_ConsoleToLocal(int playerNum)
{
    int                 i, count = 0;
    player_t*           plr = &ddPlayers[playerNum];
    int                 console = consolePlayer;

    if(playerNum == consolePlayer)
    {
        return 0;
    }

    if(!(plr->shared.flags & DDPF_LOCAL))
        return -1; // Not local at all.

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        int console = (i + consolePlayer) % DDMAXPLAYERS;
        player_t*           plr = &ddPlayers[console];

        if(console == playerNum)
        {
            return count;
        }

        if(plr->shared.flags & DDPF_LOCAL)
            count++;
    }
    return -1;
}

/**
 * Given a ptr to ddplayer_t, return it's logical index.
 */
int P_GetDDPlayerIdx(ddplayer_t* ddpl)
{
    if(ddpl)
    {
        uint                i;

        for(i = 0; i < DDMAXPLAYERS; ++i)
            if(&ddPlayers[i].shared == ddpl)
                return i;
    }

    return -1;
}

/**
 * Do we THINK the given (camera) player is currently in the void.
 * The method used to test this is to compare the position of the mobj
 * each time it is linked into a subsector.
 *
 * \note Cannot be 100% accurate so best not to use it for anything critical...
 *
 * @param player        The player to test.
 *
 * @return              @c true, If the player is thought to be in the void.
 */
boolean P_IsInVoid(player_t* player)
{
    ddplayer_t* ddpl;

    if(!player)
        return false;

    ddpl = &player->shared;

    // Cameras are allowed to move completely freely (so check z height
    // above/below ceiling/floor).
    if(ddpl->flags & DDPF_CAMERA)
    {
        if(ddpl->mo->subsector)
        {
            map_t* map = Thinker_Map((thinker_t*) ddpl->mo);
            sector_t* sec = ((const subsector_t*) ((objectrecord_t*) ddpl->mo->subsector)->obj)->sector;

            if((IS_SKYSURFACE(&sec->SP_ceilsurface) &&
                ddpl->mo->pos[VZ] < map->skyFixCeiling - 4) ||
               (IS_SKYSURFACE(&sec->SP_floorsurface) &&
                ddpl->mo->pos[VZ] > map->skyFixFloor + 4))
                return false;
        }

        return ddpl->inVoid;
    }

    return false;
}
