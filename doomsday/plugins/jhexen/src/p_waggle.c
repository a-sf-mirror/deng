/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * p_waggle.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "gamemap.h"
#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_mapsetup.h"
#include "p_iterlist.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void T_FloorWaggle(waggle_t* waggle)
{
    float fh;

    switch(waggle->state)
    {
    default:
    case WS_STABLE:
        if(waggle->ticker != -1)
        {
            if(!--waggle->ticker)
            {
                waggle->state = WS_REDUCE;
            }
        }
        break;

    case WS_EXPAND:
        if((waggle->scale += waggle->scaleDelta) >= waggle->targetScale)
        {
            waggle->scale = waggle->targetScale;
            waggle->state = WS_STABLE;
        }
        break;

    case WS_REDUCE:
        if((waggle->scale -= waggle->scaleDelta) <= 0)
        {   // Remove.
            DMU_SetFloatp(waggle->sector, DMU_FLOOR_HEIGHT,
                        waggle->originalHeight);
            P_ChangeSector(waggle->sector, true);
            P_ToXSector(waggle->sector)->specialData = NULL;
            ActionScriptInterpreter_TagFinished(ActionScriptInterpreter, P_ToXSector(waggle->sector)->tag);
            Map_RemoveThinker(Thinker_Map((thinker_t*) waggle), (thinker_t*) waggle);
            return;
        }
        break;
    }

    waggle->accumulator += waggle->accDelta;
    fh = waggle->originalHeight +
        P_FloatBobOffset(((int) waggle->accumulator) & 63) * waggle->scale;
    DMU_SetFloatp(waggle->sector, DMU_FLOOR_HEIGHT, fh);
    DMU_SetFloatp(waggle->sector, DMU_FLOOR_TARGET_HEIGHT, fh);
    DMU_SetFloatp(waggle->sector, DMU_FLOOR_SPEED, 0);
    P_ChangeSector(waggle->sector, true);
}

boolean EV_StartFloorWaggle(map_t* map, int tag, int height, int speed,
                            int offset, int timer)
{
    assert(map);
    {
    boolean retCode = false;
    sector_t* sec = NULL;
    waggle_t* waggle;
    iterlist_t* list;

    list = GameMap_SectorIterListForTag(map, tag, false);
    if(!list)
        return retCode;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_ToXSector(sec)->specialData)
            continue; // Already moving, so keep going...

        retCode = true;

        waggle = Z_Calloc(sizeof(*waggle), PU_MAP, 0);
        waggle->thinker.function = T_FloorWaggle;
        Map_ThinkerAdd(map, (thinker_t*) waggle);

        P_ToXSector(sec)->specialData = waggle;
        waggle->sector = sec;
        waggle->originalHeight = DMU_GetFloatp(sec, DMU_FLOOR_HEIGHT);
        waggle->accumulator = offset;
        waggle->accDelta = FIX2FLT(speed << 10);
        waggle->scale = 0;
        waggle->targetScale = FIX2FLT(height << 10);
        waggle->scaleDelta =
            FIX2FLT(FLT2FIX(waggle->targetScale) / (TICSPERSEC + ((3 * TICSPERSEC) * height) / 255));
        waggle->ticker = timer ? timer * 35 : -1;
        waggle->state = WS_EXPAND;
    }

    return retCode;
    }
}
