

// HEADER FILES ------------------------------------------------------------

#include "jHeretic/Doomdef.h"
#include "jHeretic/P_local.h"
#include "jHeretic/Sounds.h"

#include "Common/dmu_lib.h"
#include "Common/p_mapsetup.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

mobj_t *P_SpawnTeleFog(int x, int y)
{
    subsector_t *ss = R_PointInSubsector(x, y);

    return P_SpawnMobj(x, y, P_GetFixedp(ss, DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT) +
                             TELEFOGHEIGHT, MT_TFOG);
}

boolean P_Teleport(mobj_t *thing, fixed_t x, fixed_t y, angle_t angle)
{
    fixed_t oldx;
    fixed_t oldy;
    fixed_t oldz;
    fixed_t aboveFloor;
    fixed_t fogDelta;
    player_t *player;
    unsigned an;
    mobj_t *fog;

    oldx = thing->x;
    oldy = thing->y;
    oldz = thing->z;
    aboveFloor = thing->z - thing->floorz;
    if(!P_TeleportMove(thing, x, y, false))
    {
        return (false);
    }
    if(thing->player)
    {
        player = thing->player;
        if(player->powers[pw_flight] && aboveFloor)
        {
            thing->z = thing->floorz + aboveFloor;
            if(thing->z + thing->height > thing->ceilingz)
            {
                thing->z = thing->ceilingz - thing->height;
            }
            player->plr->viewz = thing->z + player->plr->viewheight;
        }
        else
        {
            thing->z = thing->floorz;
            player->plr->viewz = thing->z + player->plr->viewheight;
            player->plr->clLookDir = 0;
            player->plr->lookdir = 0;
        }
        player->plr->clAngle = angle;
        player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    }
    else if(thing->flags & MF_MISSILE)
    {
        thing->z = thing->floorz + aboveFloor;
        if(thing->z + thing->height > thing->ceilingz)
        {
            thing->z = thing->ceilingz - thing->height;
        }
    }
    else
    {
        thing->z = thing->floorz;
    }
    // Spawn teleport fog at source and destination
    fogDelta = thing->flags & MF_MISSILE ? 0 : TELEFOGHEIGHT;
    fog = P_SpawnMobj(oldx, oldy, oldz + fogDelta, MT_TFOG);
    S_StartSound(sfx_telept, fog);
    an = angle >> ANGLETOFINESHIFT;
    fog =
        P_SpawnMobj(x + 20 * finecosine[an], y + 20 * finesine[an],
                    thing->z + fogDelta, MT_TFOG);
    S_StartSound(sfx_telept, fog);
    if(thing->player && !thing->player->powers[pw_weaponlevel2])
    {                           // Freeze player for about .5 sec
        thing->reactiontime = 18;
    }
    thing->angle = angle;
    if(thing->flags2 & MF2_FOOTCLIP &&
       P_GetThingFloorType(thing) != FLOOR_SOLID)
    {
        thing->flags2 |= MF2_FEETARECLIPPED;
    }
    else if(thing->flags2 & MF2_FEETARECLIPPED)
    {
        thing->flags2 &= ~MF2_FEETARECLIPPED;
    }
    if(thing->flags & MF_MISSILE)
    {
        angle >>= ANGLETOFINESHIFT;
        thing->momx = FixedMul(thing->info->speed, finecosine[angle]);
        thing->momy = FixedMul(thing->info->speed, finesine[angle]);
    }
    else
    {
        thing->momx = thing->momy = thing->momz = 0;
    }
    P_ClearThingSRVO(thing);
    return (true);
}

boolean EV_Teleport(line_t *line, int side, mobj_t *thing)
{
    int     i;
    int     tag;
    mobj_t *m;
    thinker_t *thinker;
    sector_t *sector;

    // don't teleport missiles
    if(thing->flags2 & MF_MISSILE)
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
                // Not a mobj
                if(thinker->function != P_MobjThinker)
                    continue;

                m = (mobj_t *) thinker;

                // Not a teleportman
                if(m->type != MT_TELEPORTMAN)
                    continue;

                sector = P_GetPtrp(m->subsector, DMU_SECTOR);
                // wrong sector
                if(P_ToIndex(sector) != i)
                    continue;

                return (P_Teleport(thing, m->x, m->y, m->angle));
            }
        }
    }
    return (false);
}
