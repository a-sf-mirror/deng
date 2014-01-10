/** @file clplanemover.cpp  Clientside plane mover (thinker).
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "client/clplanemover.h"

#include "client/cl_def.h"
#include "client/cl_player.h"

#include "world/world.h"
#include "world/map.h"
#include "world/p_players.h"

using namespace de;

void ClPlaneMover_Thinker(ClPlaneMover *mover)
{
    LOG_AS("ClPlaneMover_Thinker");

    // Can we think yet?
    if(!Cl_GameReady()) return;

    Map &map = App_World().map(); /// @todo Do not assume mover is from the CURRENT map.

#ifdef DENG_DEBUG
    if(map.clPlaneIndex(mover) < 0)
    {
        LOG_MAP_WARNING("Running a mover that is not in activemovers!");
    }
#endif

    int const dmuPlane = (mover->planeIndex == 0? DMU_FLOOR_OF_SECTOR : DMU_CEILING_OF_SECTOR);

    // The move is cancelled if the consolePlayer becomes obstructed.
    bool const freeMove = ClPlayer_IsFreeToMove(consolePlayer);
    float const fspeed = mover->speed;

    // How's the gap?
    bool remove = false;
    coord_t const original = P_GetDouble(DMU_SECTOR, mover->sectorIndex, dmuPlane | DMU_HEIGHT);
    if(de::abs(fspeed) > 0 && de::abs(mover->destination - original) > de::abs(fspeed))
    {
        // Do the move.
        P_SetDouble(DMU_SECTOR, mover->sectorIndex, dmuPlane | DMU_HEIGHT, original + fspeed);
    }
    else
    {
        // We have reached the destination.
        P_SetDouble(DMU_SECTOR, mover->sectorIndex, dmuPlane | DMU_HEIGHT, mover->destination);

        // This thinker can now be removed.
        remove = true;
    }

    LOGDEV_MAP_XVERBOSE_DEBUGONLY("plane height %f in sector #%i",
            P_GetDouble(DMU_SECTOR, mover->sectorIndex, dmuPlane | DMU_HEIGHT)
            << mover->sectorIndex);

    // Let the game know of this.
    if(gx.SectorHeightChangeNotification)
    {
        gx.SectorHeightChangeNotification(mover->sectorIndex);
    }

    // Make sure the client didn't get stuck as a result of this move.
    if(freeMove != ClPlayer_IsFreeToMove(consolePlayer))
    {
        LOG_MAP_VERBOSE("move blocked in sector #%i, undoing move") << mover->sectorIndex;

        // Something was blocking the way! Go back to original height.
        P_SetDouble(DMU_SECTOR, mover->sectorIndex, dmuPlane | DMU_HEIGHT, original);

        if(gx.SectorHeightChangeNotification)
        {
            gx.SectorHeightChangeNotification(mover->sectorIndex);
        }
    }
    else
    {
        // Can we remove this thinker?
        if(remove)
        {
            LOG_MAP_VERBOSE("finished in sector #%i") << mover->sectorIndex;

            // It stops.
            P_SetDouble(DMU_SECTOR, mover->sectorIndex, dmuPlane | DMU_SPEED, 0);

            map.deleteClPlane(mover);
        }
    }
}