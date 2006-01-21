/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "doomstat.h"
#include "s_sound.h"
#include "p_local.h"
#include "r_state.h"

#include "Common/dmu_lib.h"
#include "Common/p_setup.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

int EV_Teleport(line_t *line, int side, mobj_t *thing)
{
    int     i;
    int     tag;
    mobj_t *m;
    mobj_t *fog;
    unsigned an;
    thinker_t *thinker;
    sector_t *sector;
    fixed_t oldx;
    fixed_t oldy;
    fixed_t oldz;

    // don't teleport missiles
    if(thing->flags & MF_MISSILE)
        return 0;

    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if(side == 1)
        return 0;

    tag = P_XLine(line)->tag;

    for(i = 0; i < numsectors; i++)
    {
        if(xsectors[i].tag == tag)
        {
            thinker = thinkercap.next;
            for(thinker = thinkercap.next; thinker != &thinkercap;
                thinker = thinker->next)
            {
                // not a mobj
                if(thinker->function != P_MobjThinker)
                    continue;

                m = (mobj_t *) thinker;

                // not a teleportman
                if(m->type != MT_TELEPORTMAN)
                    continue;

                sector = P_GetPtrp(DMU_SUBSECTOR, m->subsector, DMU_SECTOR);
                // wrong sector
                if(P_ToIndex(DMU_SECTOR, sector) != i)
                    continue;

                oldx = thing->x;
                oldy = thing->y;
                oldz = thing->z;

                if(!P_TeleportMove(thing, m->x, m->y, false))
                    return 0;
                // In Final Doom things teleported to their destination
                // but the height wasn't set to the floor.
                if(gamemission != pack_tnt && gamemission != pack_plut)
                    thing->z = thing->floorz;

                if(thing->player)
                    thing->dplayer->viewz =
                        thing->z + thing->dplayer->viewheight;

                // spawn teleport fog at source and destination
                fog = P_SpawnMobj(oldx, oldy, oldz, MT_TFOG);
                S_StartSound(sfx_telept, fog);
                an = m->angle >> ANGLETOFINESHIFT;
                fog =
                    P_SpawnMobj(m->x + 20 * finecosine[an],
                                m->y + 20 * finesine[an], thing->z, MT_TFOG);

                // emit sound, where?
                S_StartSound(sfx_telept, fog);

                thing->angle = m->angle;
                thing->momx = thing->momy = thing->momz = 0;

                // don't move for a bit
                if(thing->player)
                {
                    thing->reactiontime = 18;
                    thing->dplayer->clAngle = thing->angle;
                    thing->dplayer->clLookDir = 0;
                    thing->dplayer->lookdir = 0;
                    thing->dplayer->flags |=
                        DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
                }
                return 1;
            }
        }
    }
    return 0;
}
