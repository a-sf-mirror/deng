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

/*
 *  Movement, collision handling.
 *  Shooting and aiming.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#include "m_random.h"
#include "doomdef.h"
#include "d_config.h"
#include "p_local.h"
#include "g_common.h"
#include "s_sound.h"
#include "doomstat.h"
#include "r_state.h"

#include "Common/dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

fixed_t tmbbox[4];
mobj_t *tmthing;
int     tmflags;
fixed_t tmx, tmy, tmz, tmheight;
line_t *tmhitline;

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
boolean floatok;

fixed_t tmfloorz;
fixed_t tmceilingz;
fixed_t tmdropoffz;

// killough $dropoff_fix
boolean felldown;

// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls
line_t *ceilingline;

// Used to prevent player getting stuck in monster
// Based on solution derived by Lee Killough
// See also floorline and static int untouched

line_t *floorline; // $unstuck: Highest touched floor
line_t *blockline; // $unstuck: blocking linedef

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid

line_t **spechit;
static int spechit_max;
int     numspechit;

fixed_t bestslidefrac;
fixed_t secondslidefrac;

line_t *bestslideline;
line_t *secondslideline;

mobj_t *slidemo;

fixed_t tmxmove;
fixed_t tmymove;

mobj_t *linetarget; // who got hit (or NULL)
mobj_t *shootthing;

// Height if not aiming up or down
// ???: use slope for monsters?
fixed_t shootz;

int     la_damage;
fixed_t attackrange;

fixed_t aimslope;

// slopes to top and bottom of target
fixed_t topslope;
fixed_t bottomslope;

mobj_t *usething;

mobj_t *bombsource;
mobj_t *bombspot;
int     bombdamage;

boolean crushchange;
boolean nofit;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// DJS - from prBoom
static int pe_x; // Pain Elemental position for Lost Soul checks
static int pe_y; // Pain Elemental position for Lost Soul checks
static int ls_x; // Lost Soul position for Lost Soul checks
static int ls_y; // Lost Soul position for Lost Soul checks

static int tmunstuck; // $unstuck: used to check unsticking

// CODE --------------------------------------------------------------------

boolean PIT_StompThing(mobj_t *thing, void *data)
{
    int stompAnyway;
    fixed_t blockdist;

    if(!(thing->flags & MF_SHOOTABLE))
        return true;

    blockdist = thing->radius + tmthing->radius;

    if(abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist)
    {
        // didn't hit it
        return true;
    }

    // don't clip against self
    if(thing == tmthing)
        return true;

    stompAnyway = *(int*) data;

    // Should we stomp anyway? unless self
    if(thing != tmthing && stompAnyway)
    {
        P_DamageMobj2(thing, tmthing, tmthing, 10000, true);
        return true;
    }

    // monsters don't stomp things except on boss level
    if(!tmthing->player && gamemap != 30)
        return false;

    // Do stomp damage (unless self)
    if(thing != tmthing)
        P_DamageMobj2(thing, tmthing, tmthing, 10000, true);

    return true;
}

boolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y, boolean alwaysstomp)
{
    int     xl;
    int     xh;
    int     yl;
    int     yh;
    int     bx;
    int     by;
    int     stomping;

    subsector_t *newsubsec;

    // kill anything occupying the position
    tmthing = thing;
    tmflags = thing->flags;

    stomping = alwaysstomp;

    tmx = x;
    tmy = y;

    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsubsec = R_PointInSubsector(x, y);

    // $unstuck: floorline used with tmunstuck
    blockline = floorline = ceilingline = NULL;

    // $unstuck
    tmunstuck = thing->dplayer && thing->dplayer->mo == thing;

    // The base floor/ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.
    tmfloorz = tmdropoffz =
        P_GetFixedp(newsubsec, DMU_FLOOR_HEIGHT);

    tmceilingz =
        P_GetFixedp(newsubsec, DMU_CEILING_HEIGHT);

    validCount++;
    numspechit = 0;

    // stomp on any things contacted
    P_PointToBlock(tmbbox[BOXLEFT] - MAXRADIUS, tmbbox[BOXBOTTOM] - MAXRADIUS,
                   &xl, &yl);
    P_PointToBlock(tmbbox[BOXRIGHT] + MAXRADIUS, tmbbox[BOXTOP] + MAXRADIUS,
                   &xh, &yh);

    for(bx = xl; bx <= xh; bx++)
        for(by = yl; by <= yh; by++)
            if(!P_BlockThingsIterator(bx, by, PIT_StompThing, &stomping))
                return false;

    // the move is ok,
    // so link the thing into its new position
    P_UnsetThingPosition(thing);

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
    thing->dropoffz = tmdropoffz;   // killough $unstuck
    thing->x = x;
    thing->y = y;

    P_SetThingPosition(thing);
    P_ClearThingSRVO(thing);

    return true;
}

/*
 * Checks to see if a PE->LS trajectory line crosses a blocking
 * line. Returns false if it does.
 *
 * tmbbox holds the bounding box of the trajectory. If that box
 * does not touch the bounding box of the line in question,
 * then the trajectory is not blocked. If the PE is on one side
 * of the line and the LS is on the other side, then the
 * trajectory is blocked.
 *
 * Currently this assumes an infinite line, which is not quite
 * correct. A more correct solution would be to check for an
 * intersection of the trajectory and the line, but that takes
 * longer and probably really isn't worth the effort.
 *
 * @param : *data is unused
 */
static boolean PIT_CrossLine (line_t* ld, void *data)
{
    if(!(P_GetIntp(ld, DMU_FLAGS) & ML_TWOSIDED) ||
      (P_GetIntp(ld, DMU_FLAGS) & (ML_BLOCKING | ML_BLOCKMONSTERS)))
    {
        fixed_t* bbox = P_GetPtrp(ld, DMU_BOUNDING_BOX);

        if(!(tmbbox[BOXLEFT] > bbox[BOXRIGHT] ||
             tmbbox[BOXRIGHT] < bbox[BOXLEFT] ||
             tmbbox[BOXTOP] < bbox[BOXBOTTOM] ||
             tmbbox[BOXBOTTOM] > bbox[BOXTOP]))
        {
            if(P_PointOnLineSide(pe_x, pe_y, ld) != P_PointOnLineSide(ls_x, ls_y, ld))
                // line blocks trajectory
                return(false);
        }
    }

    // line doesn't block trajectory
    return(true);
}

/*
 * $unstuck: used to test intersection between thing and line
 * assuming NO movement occurs -- used to avoid sticky situations.
 */
static int untouched(line_t *ld)
{
    fixed_t x, y, box[4];
    fixed_t* bbox = P_GetPtrp(ld, DMU_BOUNDING_BOX);

    /*return (box[BOXRIGHT] =
        (x = tmthing->x) + tmthing->radius) <= ld->bbox[BOXLEFT] ||
        (box[BOXLEFT] = x - tmthing->radius) >= ld->bbox[BOXRIGHT] ||
        (box[BOXTOP] =
         (y = tmthing->y) + tmthing->radius) <= ld->bbox[BOXBOTTOM] ||
        (box[BOXBOTTOM] = y - tmthing->radius) >= ld->bbox[BOXTOP] ||
        P_BoxOnLineSide(box, ld) != -1; */

    if(((box[BOXRIGHT] = (x = tmthing->x) + tmthing->radius) <= bbox[BOXLEFT]) ||
       ((box[BOXLEFT] = x - tmthing->radius) >= bbox[BOXRIGHT]) ||
       ((box[BOXTOP] = (y = tmthing->y) + tmthing->radius) <= bbox[BOXBOTTOM]) ||
       ((box[BOXBOTTOM] = y - tmthing->radius) >= bbox[BOXTOP]) ||
       P_BoxOnLineSide(box, ld) != -1)
        return true;
    else
        return false;
}

/*
 * Adjusts tmfloorz and tmceilingz as lines are contacted
 */
boolean PIT_CheckLine(line_t *ld, void *data)
{
    fixed_t dx = P_GetFixedp(ld, DMU_DX);
    fixed_t dy = P_GetFixedp(ld, DMU_DY);
    fixed_t* bbox = P_GetPtrp(ld, DMU_BOUNDING_BOX);

    if(tmbbox[BOXRIGHT] <= bbox[BOXLEFT] ||
       tmbbox[BOXLEFT] >= bbox[BOXRIGHT] ||
       tmbbox[BOXTOP] <= bbox[BOXBOTTOM] ||
       tmbbox[BOXBOTTOM] >= bbox[BOXTOP])
        return true;

    if(P_BoxOnLineSide(tmbbox, ld) != -1)
        return true;

    // A line has been hit
    tmthing->wallhit = true;

    // A Hit event will be sent to special lines.
    if(P_XLine(ld)->special)
        tmhitline = ld;

    // The moving thing's destination position will cross
    // the given line.
    // If this should not be allowed, return false.
    // If the line is special, keep track of it
    // to process later if the move is proven ok.
    // NOTE: specials are NOT sorted by order,
    // so two special lines that are only 8 pixels apart
    // could be crossed in either order.

    // $unstuck: allow player to move out of 1s wall, to prevent sticking
    if(!P_GetPtrp(ld, DMU_BACK_SECTOR)) // one sided line
    {
        blockline = ld;
        return tmunstuck && !untouched(ld) &&
            FixedMul(tmx - tmthing->x, dy) > FixedMul(tmy - tmthing->y, dx);
    }

    if(!(tmthing->flags & MF_MISSILE))
    {
        // explicitly blocking everything?
        if(P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKING)
            // killough $unstuck: allow escape
            return tmunstuck && !untouched(ld);

        // block monsters only?
        if(!tmthing->player &&
           P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKMONSTERS)
            return false;
    }

    // set openrange, opentop, openbottom
    P_LineOpening(ld);

    // adjust floor / ceiling heights
    if(opentop < tmceilingz)
    {
        tmceilingz = opentop;
        ceilingline = ld;
        blockline = ld;
    }

    if(openbottom > tmfloorz)
    {
        tmfloorz = openbottom;
        // killough $unstuck: remember floor linedef
        floorline = ld;
        blockline = ld;
    }
    /*
       if (openbottom > tmfloorz)
       tmfloorz = openbottom;   */

    if(lowfloor < tmdropoffz)
        tmdropoffz = lowfloor;

    // if contacted a special line, add it to the list
    if(P_XLine(ld)->special)
    {
        if(numspechit >= spechit_max)
        {
             spechit_max = spechit_max ? spechit_max * 2 : 8;
             spechit = realloc(spechit, sizeof *spechit*spechit_max);
        }
        spechit[numspechit++] = ld;
    }

    tmthing->wallhit = false;
    return true;
}

boolean PIT_CheckThing(mobj_t *thing, void *data)
{
    fixed_t blockdist;
    boolean solid, overlap = false;
    int     damage;

    // don't clip against self
    if(thing == tmthing)
        return true;

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)) ||
       P_IsCamera(thing) || P_IsCamera(tmthing))
        return true;

    blockdist = thing->radius + tmthing->radius;

    if(tmthing->player && tmz != DDMAXINT && cfg.moveCheckZ)
    {
        if(thing->z > tmz + tmheight || thing->z + thing->height < tmz)
            return true;        // under or over it
        overlap = true;
    }
    if(abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist)
    {
        // didn't hit it
        return true;
    }

    // check for skulls slamming into things
    if(tmthing->flags & MF_SKULLFLY)
    {
        damage = ((P_Random() % 8) + 1) * tmthing->info->damage;

        P_DamageMobj(thing, tmthing, tmthing, damage);

        tmthing->flags &= ~MF_SKULLFLY;
        tmthing->momx = tmthing->momy = tmthing->momz = 0;

        P_SetMobjState(tmthing, tmthing->info->spawnstate);

        return false;           // stop moving
    }

    // missiles can hit other things
    if(tmthing->flags & MF_MISSILE)
    {
        // see if it went over / under
        if(tmthing->z > thing->z + thing->height)
            return true;        // overhead

        if(tmthing->z + tmthing->height < thing->z)
            return true;        // underneath

        // Don't hit same species as originator.
        if(tmthing->target &&
           (tmthing->target->type == thing->type ||
            (tmthing->target->type == MT_KNIGHT && thing->type == MT_BRUISER)
            || (tmthing->target->type == MT_BRUISER &&
                thing->type == MT_KNIGHT)))
        {
            if(thing == tmthing->target)
                return true;

            if(!monsterinfight && thing->type != MT_PLAYER)
            {
                // Explode, but do no damage.
                // Let players missile other players.
                return false;
            }
        }

        if(!(thing->flags & MF_SHOOTABLE))
        {
            // didn't do any damage
            return !(thing->flags & MF_SOLID);
        }

        // damage / explode
        damage = ((P_Random() % 8) + 1) * tmthing->info->damage;
        P_DamageMobj(thing, tmthing, tmthing->target, damage);

        // don't traverse any more
        return false;
    }

    // check for special pickup
    if(thing->flags & MF_SPECIAL)
    {
        solid = thing->flags & MF_SOLID;
        if(tmflags & MF_PICKUP)
        {
            // can remove thing
            P_TouchSpecialThing(thing, tmthing);
        }
        return !solid;
    }

    if(overlap && thing->flags & MF_SOLID)
    {
        // How are we positioned?
        if(tmz > thing->z + thing->height - 24 * FRACUNIT)
        {
            tmthing->onmobj = thing;
            if(thing->z + thing->height > tmfloorz)
                tmfloorz = thing->z + thing->height;
            return true;
        }
    }

    return !(thing->flags & MF_SOLID);
}

/*
 * This routine checks for Lost Souls trying to be spawned
 * across 1-sided lines, impassible lines, or "monsters can't
 * cross" lines. Draw an imaginary line between the PE
 * and the new Lost Soul spawn spot. If that line crosses
 * a 'blocking' line, then disallow the spawn. Only search
 * lines in the blocks of the blockmap where the bounding box
 * of the trajectory line resides. Then check bounding box
 * of the trajectory vs. the bounding box of each blocking
 * line to see if the trajectory and the blocking line cross.
 * Then check the PE and LS to see if they're on different
 * sides of the blocking line. If so, return true, otherwise
 * false.
 */
boolean P_CheckSides(mobj_t* actor, int x, int y)
{
    int bx, by, xl, xh, yl, yh;

    pe_x = actor->x;
    pe_y = actor->y;
    ls_x = x;
    ls_y = y;

    // Here is the bounding box of the trajectory

    tmbbox[BOXLEFT]   = pe_x < x ? pe_x : x;
    tmbbox[BOXRIGHT]  = pe_x > x ? pe_x : x;
    tmbbox[BOXTOP]    = pe_y > y ? pe_y : y;
    tmbbox[BOXBOTTOM] = pe_y < y ? pe_y : y;

    // Determine which blocks to look in for blocking lines
    P_PointToBlock(tmbbox[BOXLEFT], tmbbox[BOXBOTTOM], &xl, &yl);
    P_PointToBlock(tmbbox[BOXRIGHT], tmbbox[BOXTOP], &xh, &yh);

    // xl->xh, yl->yh determine the mapblock set to search

    for(bx = xl ; bx <= xh ; bx++)
        for(by = yl ; by <= yh ; by++)
            if(!P_BlockLinesIterator(bx,by,PIT_CrossLine, 0))
                return true;

    return false;
}

/*
 * This is purely informative, nothing is modified
 * (except things picked up).
 *
 * in:
 *  a mobj_t (can be valid or invalid)
 *  a position to be checked
 *   (doesn't need to be related to the mobj_t->x,y)
 *
 * during:
 *  special things are touched if MF_PICKUP
 *  early out on solid lines?
 *
 * out:
 *  newsubsec
 *  floorz
 *  ceilingz
 *  tmdropoffz
 *   the lowest point contacted
 *   (monsters won't move to a drop off)
 *  speciallines[]
 *  numspeciallines
 */
boolean P_CheckPosition2(mobj_t *thing, fixed_t x, fixed_t y, fixed_t z)
{
    int     xl, xh, yl, yh, bx, by;
    sector_t *newsec;

    tmthing = thing;
    tmflags = thing->flags;

    thing->onmobj = NULL;
    thing->wallhit = false;

    tmhitline = NULL;

    tmx = x;
    tmy = y;
    tmz = z;
    tmheight = thing->height;

    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsec = P_GetPtrp(R_PointInSubsector(x, y), DMU_SECTOR);

    // $unstuck: floorline used with tmunstuck
    blockline = floorline = ceilingline = NULL;

    // $unstuck
    tmunstuck = thing->dplayer && thing->dplayer->mo == thing;

    // The base floor / ceiling is from the subsector
    // that contains the point. Any contacted lines the
    // step closer together will adjust them.
    tmfloorz = tmdropoffz = P_GetFixedp(newsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFixedp(newsec, DMU_CEILING_HEIGHT);

    validCount++;
    numspechit = 0;

    if(tmflags & MF_NOCLIP)
        return true;

    // Check things first, possibly picking things up.
    // The bounding box is extended by MAXRADIUS
    // because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap
    // into adjacent blocks by up to MAXRADIUS units.
    P_PointToBlock(tmbbox[BOXLEFT] - MAXRADIUS, tmbbox[BOXBOTTOM] - MAXRADIUS,
                   &xl, &yl);
    P_PointToBlock(tmbbox[BOXRIGHT] + MAXRADIUS, tmbbox[BOXTOP] + MAXRADIUS,
                   &xh, &yh);

    for(bx = xl; bx <= xh; bx++)
        for(by = yl; by <= yh; by++)
            if(!P_BlockThingsIterator(bx, by, PIT_CheckThing, 0))
                return false;

    // check lines
    P_PointToBlock(tmbbox[BOXLEFT], tmbbox[BOXBOTTOM], &xl, &yl);
    P_PointToBlock(tmbbox[BOXRIGHT], tmbbox[BOXTOP], &xh, &yh);

    for(bx = xl; bx <= xh; bx++)
        for(by = yl; by <= yh; by++)
            if(!P_BlockLinesIterator(bx, by, PIT_CheckLine, 0))
                return false;

    return true;
}

boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y)
{
    return P_CheckPosition2(thing, x, y, DDMAXINT);
}

/*
 * Attempt to move to a new position,
 * crossing special lines unless MF_TELEPORT is set.
 *
 * killough $dropoff_fix
 */
boolean P_TryMove2(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff)
{
    fixed_t oldx;
    fixed_t oldy;
    int     side;
    int     oldside;
    line_t *ld;

    // $dropoff_fix: felldown
    floatok = felldown = false;

    if(!P_CheckPosition2(thing, x, y, thing->z))
    {
        // Would we hit another thing or a solid wall?
        if(!thing->onmobj || thing->wallhit)
            return false;
    }

    // GMJ 02/02/02
    if(!(thing->flags & MF_NOCLIP))
    {
        // killough 7/26/98: reformatted slightly
        // killough 8/1/98: Possibly allow escape if otherwise stuck

        if(tmceilingz - tmfloorz < thing->height || // doesn't fit
           // mobj must lower to fit
           (floatok = true, !(thing->flags & MF_TELEPORT) &&
            tmceilingz - thing->z < thing->height) ||
           // too big a step up
           (!(thing->flags & MF_TELEPORT) &&
            tmfloorz - thing->z > 24 * FRACUNIT))
        {
            return tmunstuck && !(ceilingline && untouched(ceilingline)) &&
                !(floorline && untouched(floorline));
        }

        // killough 3/15/98: Allow certain objects to drop off
        // killough 7/24/98, 8/1/98:
        // Prevent monsters from getting stuck hanging off ledges
        // killough 10/98: Allow dropoffs in controlled circumstances
        // killough 11/98: Improve symmetry of clipping on stairs

        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)))
        {
            // Dropoff height limit
            if(!dropoff && tmfloorz - tmdropoffz > 24 * FRACUNIT)
                return false;
            else
            {
                // set felldown if drop > 24
                felldown = !(thing->flags & MF_NOGRAVITY) &&
                    thing->z - tmfloorz > 24 * FRACUNIT;
            }
        }

        // killough $dropoff: prevent falling objects from going up too many steps
        if(!thing->player && thing->intflags & MIF_FALLING &&
           tmfloorz - thing->z > FixedMul(thing->momx,
                                          thing->momx) + FixedMul(thing->momy,
                                                                  thing->momy))
        {
            return false;
        }
    }

    // the move is ok, so link the thing into its new position
    P_UnsetThingPosition(thing);

    oldx = thing->x;
    oldy = thing->y;
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;

    // killough $dropoff_fix: keep track of dropoffs
    thing->dropoffz = tmdropoffz;
    thing->x = x;
    thing->y = y;

    P_SetThingPosition(thing);

    // if any special lines were hit, do the effect
    if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        while(numspechit--)
        {
            // see if the line was crossed
            ld = spechit[numspechit];

            if(P_XLine(ld)->special)
            {
                side = P_PointOnLineSide(thing->x, thing->y, ld);
                oldside = P_PointOnLineSide(oldx, oldy, ld);
                if(side != oldside)
                    P_CrossSpecialLine(P_ToIndex(DMU_LINE, ld), oldside, thing);
            }
        }
    }

    return true;
}

boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff)
{
    // killough $dropoff_fix
    boolean res = P_TryMove2(thing, x, y, dropoff);

    if(!res && tmhitline)
    {
        // Move not possible, see if the thing hit a line and send a Hit
        // event to it.
        XL_HitLine(tmhitline, P_PointOnLineSide(thing->x, thing->y, tmhitline),
                   thing);
    }
    return res;
}

/*
 * Takes a valid thing and adjusts the thing->floorz,
 * thing->ceilingz, and possibly thing->z.
 * This is called for all nearby monsters
 * whenever a sector changes height.
 * If the thing doesn't fit,
 * the z will be set to the lowest value
 * and false will be returned.
 */
boolean P_ThingHeightClip(mobj_t *thing)
{
    boolean onfloor;

    onfloor = (thing->z == thing->floorz);
    P_CheckPosition2(thing, thing->x, thing->y, thing->z);

    // what about stranding a monster partially off an edge?

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;

    // killough $dropoff_fix: remember dropoffs
    thing->dropoffz = tmdropoffz;

    if(onfloor)
    {
        // walking monsters rise and fall with the floor
        thing->z = thing->floorz;
        // killough $dropoff_fix:
        // Possibly upset balance of objects hanging off ledges
        if(thing->intflags & MIF_FALLING && thing->gear >= MAXGEAR)
            thing->gear = 0;
    }
    else
    {
        // don't adjust a floating monster unless forced to
        if(thing->z + thing->height > thing->ceilingz)
            thing->z = thing->ceilingz - thing->height;
    }

    if((thing->ceilingz - thing->floorz) >= thing->height)
        return true;
    else
        return false;
}

/*
 * Allows the player to slide along any angled walls by
 * adjusting the xmove / ymove so that the NEXT move will
 * slide along the wall.
 */
void P_HitSlideLine(line_t *ld)
{
    int     side;
    angle_t lineangle, moveangle, deltaangle;
    fixed_t movelen, newlen;

    if(P_GetIntp(ld, DMU_SLOPE_TYPE) == ST_HORIZONTAL)
    {
        tmymove = 0;
        return;
    }

    if(P_GetIntp(ld, DMU_SLOPE_TYPE) == ST_VERTICAL)
    {
        tmxmove = 0;
        return;
    }

    side = P_PointOnLineSide(slidemo->x, slidemo->y, ld);

    lineangle = R_PointToAngle2(0, 0,
                                P_GetFixedp(ld, DMU_DX),
                                P_GetFixedp(ld, DMU_DY));

    if(side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2(0, 0, tmxmove, tmymove);
    deltaangle = moveangle - lineangle;

    if(deltaangle > ANG180)
        deltaangle += ANG180;

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_ApproxDistance(tmxmove, tmymove);
    newlen = FixedMul(movelen, finecosine[deltaangle]);

    tmxmove = FixedMul(newlen, finecosine[lineangle]);
    tmymove = FixedMul(newlen, finesine[lineangle]);
}

boolean PTR_SlideTraverse(intercept_t * in)
{
    line_t *li;

    if(!in->isaline)
        Con_Error("PTR_SlideTraverse: not a line?");

    li = in->d.line;

    if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
    {
        if(P_PointOnLineSide(slidemo->x, slidemo->y, li))
        {
            // don't hit the back side
            return true;
        }
        goto isblocking;
    }

    // set openrange, opentop, openbottom
    P_LineOpening(li);

    if(openrange < slidemo->height)
        goto isblocking;        // doesn't fit

    if(opentop - slidemo->z < slidemo->height)
        goto isblocking;        // mobj is too high

    if(openbottom - slidemo->z > 24 * FRACUNIT)
        goto isblocking;        // too big a step up

    // this line doesn't block movement
    return true;

    // the line does block movement,
    // see if it is closer than best so far
  isblocking:
    if(in->frac < bestslidefrac)
    {
        secondslidefrac = bestslidefrac;
        secondslideline = bestslideline;
        bestslidefrac = in->frac;
        bestslideline = li;
    }

    return false;               // stop
}

/*
 * The momx / momy move is bad, so try to slide
 * along a wall.
 * Find the first line hit, move flush to it,
 * and slide along it
 *
 * This is a kludgy mess.
 */
void P_SlideMove(mobj_t *mo)
{
    fixed_t leadx, leady;
    fixed_t trailx, traily;
    fixed_t newx, newy;
    int     hitcount;

    slidemo = mo;
    hitcount = 0;

  retry:
    if(++hitcount == 3)
        goto stairstep; // don't loop forever

    // trace along the three leading corners
    if(mo->momx > 0)
    {
        leadx = mo->x + mo->radius;
        trailx = mo->x - mo->radius;
    }
    else
    {
        leadx = mo->x - mo->radius;
        trailx = mo->x + mo->radius;
    }

    if(mo->momy > 0)
    {
        leady = mo->y + mo->radius;
        traily = mo->y - mo->radius;
    }
    else
    {
        leady = mo->y - mo->radius;
        traily = mo->y + mo->radius;
    }

    bestslidefrac = FRACUNIT + 1;

    P_PathTraverse(leadx, leady, leadx + mo->momx, leady + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(trailx, leady, trailx + mo->momx, leady + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(leadx, traily, leadx + mo->momx, traily + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);

    // move up to the wall
    if(bestslidefrac == FRACUNIT + 1)
    {
        // the move most have hit the middle, so stairstep
      stairstep:
        // killough $dropoff_fix
        if(!P_TryMove(mo, mo->x, mo->y + mo->momy, true))
            P_TryMove(mo, mo->x + mo->momx, mo->y, true);
        return;
    }

    // fudge a bit to make sure it doesn't hit
    bestslidefrac -= 0x800;
    if(bestslidefrac > 0)
    {
        newx = FixedMul(mo->momx, bestslidefrac);
        newy = FixedMul(mo->momy, bestslidefrac);

        // killough $dropoff_fix
        if(!P_TryMove(mo, mo->x + newx, mo->y + newy, true))
            goto stairstep;
    }

    // Now continue along the wall.
    // First calculate remainder.
    bestslidefrac = FRACUNIT - (bestslidefrac + 0x800);

    if(bestslidefrac > FRACUNIT)
        bestslidefrac = FRACUNIT;

    if(bestslidefrac <= 0)
        return;

    tmxmove = FixedMul(mo->momx, bestslidefrac);
    tmymove = FixedMul(mo->momy, bestslidefrac);

    P_HitSlideLine(bestslideline);  // clip the moves

    mo->momx = tmxmove;
    mo->momy = tmymove;

    // killough $dropoff_fix
    if(!P_TryMove(mo, mo->x + tmxmove, mo->y + tmymove, true))
    {
        goto retry;
    }
}

/*
 * Sets linetaget and aimslope when a target is aimed at.
 */
boolean PTR_AimTraverse(intercept_t * in)
{
    line_t *li;
    mobj_t *th;
    fixed_t slope, thingtopslope, thingbottomslope;
    fixed_t dist;
    sector_t *backsector, *frontsector;

    if(in->isaline)
    {
        li = in->d.line;

        if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
            return false;       // stop

        // Crosses a two sided line.
        // A two sided line will restrict
        // the possible target ranges.
        P_LineOpening(li);

        if(openbottom >= opentop)
            return false;       // stop

        dist = FixedMul(attackrange, in->frac);

        frontsector = P_GetPtrp(li, DMU_FRONT_SECTOR);
        backsector = P_GetPtrp(li, DMU_BACK_SECTOR);

        if(P_GetFixedp(frontsector, DMU_FLOOR_HEIGHT) !=
           P_GetFixedp(backsector, DMU_FLOOR_HEIGHT))
        {
            slope = FixedDiv(openbottom - shootz, dist);
            if(slope > bottomslope)
                bottomslope = slope;
        }

        if(P_GetFixedp(frontsector, DMU_CEILING_HEIGHT) !=
           P_GetFixedp(backsector, DMU_CEILING_HEIGHT))
        {
            slope = FixedDiv(opentop - shootz, dist);
            if(slope < topslope)
                topslope = slope;
        }

        if(topslope <= bottomslope)
            return false;       // stop

        return true;            // shot continues
    }

    // shoot a thing
    th = in->d.thing;
    if(th == shootthing)
        return true;            // can't shoot self

    if(!(th->flags & MF_SHOOTABLE))
        return true;            // corpse or something

    // check angles to see if the thing can be aimed at
    dist = FixedMul(attackrange, in->frac);
    thingtopslope = FixedDiv(th->z + th->height - shootz, dist);

    if(thingtopslope < bottomslope)
        return true;            // shot over the thing

    thingbottomslope = FixedDiv(th->z - shootz, dist);

    if(thingbottomslope > topslope)
        return true;            // shot under the thing

    // this thing can be hit!
    if(thingtopslope > topslope)
        thingtopslope = topslope;

    if(thingbottomslope < bottomslope)
        thingbottomslope = bottomslope;

    aimslope = (thingtopslope + thingbottomslope) / 2;
    linetarget = th;

    return false;               // don't go any farther
}

boolean PTR_ShootTraverse(intercept_t * in)
{
    fixed_t x, y, z;
    fixed_t frac;
    sector_t *backsector, *frontsector;
    line_t *li;
    xline_t *xline;

    mobj_t *th;

    fixed_t slope;
    fixed_t dist;
    fixed_t thingtopslope;
    fixed_t thingbottomslope;

    divline_t *trace = (divline_t *) DD_GetVariable(DD_TRACE_ADDRESS);

    // These were added for the plane-hitpoint algo.
    // FIXME: This routine is getting rather bloated.
    boolean lineWasHit;
    subsector_t *contact, *originSub;
    fixed_t ctop, cbottom, dx, dy, dz, step, stepx, stepy, stepz;
    fixed_t cfloor, cceil;
    int     divisor;

    if(in->isaline)
    {
        li = in->d.line;
        xline = &xlines[P_ToIndex(DMU_LINE, li)];

        if(xline->special)
            P_ShootSpecialLine(shootthing, li);

        if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
            goto hitline;

        // crosses a two sided line
        P_LineOpening(li);

        dist = FixedMul(attackrange, in->frac);
        frontsector = P_GetPtrp(li, DMU_FRONT_SECTOR);
        backsector = P_GetPtrp(li, DMU_BACK_SECTOR);

        if(P_GetFixedp(frontsector, DMU_FLOOR_HEIGHT) !=
           P_GetFixedp(backsector, DMU_FLOOR_HEIGHT))
        {
            slope = FixedDiv(openbottom - shootz, dist);
            if(slope > aimslope)
                goto hitline;
        }

        if(P_GetFixedp(frontsector, DMU_CEILING_HEIGHT) !=
           P_GetFixedp(backsector, DMU_CEILING_HEIGHT))
        {
            slope = FixedDiv(opentop - shootz, dist);
            if(slope < aimslope)
                goto hitline;
        }

        // shot continues
        return true;

        // hit line
      hitline:
        lineWasHit = true;

        // Position a bit closer.
        frac = in->frac - FixedDiv(4 * FRACUNIT, attackrange);
        x = trace->x + FixedMul(trace->dx, frac);
        y = trace->y + FixedMul(trace->dy, frac);
        z = shootz + FixedMul(aimslope, FixedMul(frac, attackrange));

        // Is it a sky hack wall? If the hitpoint is above the visible
        // line, no puff must be shown.
        if(backsector &&
           P_GetIntp(frontsector, DMU_CEILING_TEXTURE) == skyflatnum &&
           P_GetIntp(backsector, DMU_CEILING_TEXTURE) == skyflatnum &&
           (z > P_GetFixedp(frontsector, DMU_CEILING_HEIGHT) ||
            z > P_GetFixedp(backsector, DMU_CEILING_HEIGHT) ))
            return false;

        // This is subsector where the trace originates.
        originSub = R_PointInSubsector(trace->x, trace->y);

        dx = x - trace->x;
        dy = y - trace->y;
        dz = z - shootz;

        if(dz != 0)
        {
            contact = R_PointInSubsector(x, y);
            step = P_ApproxDistance3(dx, dy, dz);
            stepx = FixedDiv(dx, step);
            stepy = FixedDiv(dy, step);
            stepz = FixedDiv(dz, step);

            cfloor = P_GetFixedp(contact, DMU_FLOOR_HEIGHT);
            cceil = P_GetFixedp(contact, DMU_CEILING_HEIGHT);
            // Backtrack until we find a non-empty sector.
            while(cceil <= cfloor && contact != originSub)
            {
                dx -= 8 * stepx;
                dy -= 8 * stepy;
                dz -= 8 * stepz;
                x = trace->x + dx;
                y = trace->y + dy;
                z = shootz + dz;
                contact = R_PointInSubsector(x, y);
            }

            // Should we backtrack to hit a plane instead?
            ctop = cceil - 4 * FRACUNIT;
            cbottom = cfloor + 4 * FRACUNIT;
            divisor = 2;

            // We must not hit a sky plane.
            if((z > ctop &&
                P_GetIntp(contact, DMU_CEILING_TEXTURE) == skyflatnum) ||
               (z < cbottom &&
                P_GetIntp(contact, DMU_FLOOR_TEXTURE) == skyflatnum))
                return false;

            // Find the approximate hitpoint by stepping back and
            // forth using smaller and smaller steps.
            while((z > ctop || z < cbottom) && divisor <= 128)
            {
                // We aren't going to hit a line any more.
                lineWasHit = false;

                // Take a step backwards.
                x -= dx / divisor;
                y -= dy / divisor;
                z -= dz / divisor;

                // Divisor grows.
                divisor <<= 1;

                // Move forward until limits breached.
                while((dz > 0 && z <= ctop) || (dz < 0 && z >= cbottom))
                {
                    x += dx / divisor;
                    y += dy / divisor;
                    z += dz / divisor;
                }
            }
        }

        // Spawn bullet puffs.
        P_SpawnPuff(x, y, z);

        if(lineWasHit && xline->special)
        {
            // Extended shoot events only happen when the bullet actually
            // hits the line.
            XL_ShootLine(li, 0, shootthing);
        }

        // don't go any farther
        return false;
    }

    // shoot a thing
    th = in->d.thing;
    if(th == shootthing)
        return true;            // can't shoot self

    if(!(th->flags & MF_SHOOTABLE))
        return true;            // corpse or something

    // check angles to see if the thing can be aimed at
    dist = FixedMul(attackrange, in->frac);
    thingtopslope = FixedDiv(th->z + th->height - shootz, dist);

    if(thingtopslope < aimslope)
        return true;            // shot over the thing

    thingbottomslope = FixedDiv(th->z - shootz, dist);

    if(thingbottomslope > aimslope)
        return true;            // shot under the thing

    // hit thing
    // position a bit closer
    frac = in->frac - FixedDiv(10 * FRACUNIT, attackrange);

    x = trace->x + FixedMul(trace->dx, frac);
    y = trace->y + FixedMul(trace->dy, frac);
    z = shootz + FixedMul(aimslope, FixedMul(frac, attackrange));

    // Spawn bullet puffs or blod spots,
    // depending on target type.
    if(in->d.thing->flags & MF_NOBLOOD)
        P_SpawnPuff(x, y, z);
    else
        P_SpawnBlood(x, y, z, la_damage);

    if(la_damage)
        P_DamageMobj(th, shootthing, shootthing, la_damage);

    // don't go any farther
    return false;

}

fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance)
{
    fixed_t x2;
    fixed_t y2;

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;

    x2 = t1->x + (distance >> FRACBITS) * finecosine[angle];
    y2 = t1->y + (distance >> FRACBITS) * finesine[angle];
    shootz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;

    topslope = 60 * FRACUNIT;
    bottomslope = -topslope;

    attackrange = distance;
    linetarget = NULL;

    P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS,
                   PTR_AimTraverse);

    if(linetarget)
    {
        if(!t1->player || !cfg.noAutoAim)
            return aimslope;
    }

    if(t1->player)
    {
        // The slope is determined by lookdir.
        return FRACUNIT * (tan(LOOKDIR2RAD(t1->dplayer->lookdir)) / 1.2);
    }

    return 0;
}

/*
 * If damage == 0, it is just a test trace that will leave linetarget set.
 */
void P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope,
                  int damage)
{
    fixed_t x2;
    fixed_t y2;

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;
    la_damage = damage;
    x2 = t1->x + (distance >> FRACBITS) * finecosine[angle];
    y2 = t1->y + (distance >> FRACBITS) * finesine[angle];
    shootz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;
    if(t1->player)
    {
        // Players shoot at eye height.
        shootz = t1->z + (cfg.plrViewHeight - 5) * FRACUNIT;
    }
    attackrange = distance;
    aimslope = slope;

    P_PathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS,
                   PTR_ShootTraverse);
}

boolean PTR_UseTraverse(intercept_t * in)
{
    int     side;

    if(!P_XLine(in->d.line)->special)
    {
        P_LineOpening(in->d.line);
        if(openrange <= 0)
        {
            S_StartSound(sfx_noway, usething);

            // can't use through a wall
            return false;
        }
        // not a special line, but keep checking
        return true;
    }

    side = 0;
    if(P_PointOnLineSide(usething->x, usething->y, in->d.line) == 1)
        side = 1;

    //  return false;       // don't use back side

    P_UseSpecialLine(usething, in->d.line, side);

    // can use multiple line specials in a row with the PassThru flag
    if(P_GetIntp(in->d.line, DMU_FLAGS) & ML_PASSUSE)
        return true;

    // can't use for than one special line in a row
    return false;
}

/*
 * Looks for special lines in front of the player to activate.
 */
void P_UseLines(player_t *player)
{
    int     angle;
    fixed_t x1;
    fixed_t y1;
    fixed_t x2;
    fixed_t y2;

    usething = player->plr->mo;

    angle = player->plr->mo->angle >> ANGLETOFINESHIFT;

    x1 = player->plr->mo->x;
    y1 = player->plr->mo->y;
    x2 = x1 + (USERANGE >> FRACBITS) * finecosine[angle];
    y2 = y1 + (USERANGE >> FRACBITS) * finesine[angle];

    P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse);
}

/*
 * "bombsource" is the creature that caused the explosion at "bombspot".
 */
boolean PIT_RadiusAttack(mobj_t *thing, void *data)
{
    fixed_t dx;
    fixed_t dy;
    fixed_t dz;
    fixed_t dist;

    if(!(thing->flags & MF_SHOOTABLE))
        return true;

    // Boss spider and cyborg
    // take no damage from concussion.
    if(thing->type == MT_CYBORG || thing->type == MT_SPIDER)
        return true;

    dx = abs(thing->x - bombspot->x);
    dy = abs(thing->y - bombspot->y);
    dz = abs(thing->z - bombspot->z);

    dist = dx > dy ? dx : dy;
    dist = dz > dist ? dz : dist;
    dist = (dist - thing->radius) >> FRACBITS;

    if(dist < 0)
        dist = 0;

    if(dist >= bombdamage)
        return true;            // out of range

    if(P_CheckSight(thing, bombspot))
    {
        // must be in direct path
        P_DamageMobj(thing, bombspot, bombsource, bombdamage - dist);
    }

    return true;
}

/*
 * Source is the creature that caused the explosion at spot.
 */
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage)
{
    int     x, y, xl, xh, yl, yh;
    fixed_t dist;

    dist = (damage + MAXRADIUS) << FRACBITS;
    P_PointToBlock(spot->x - dist, spot->y - dist, &xl, &yl);
    P_PointToBlock(spot->x + dist, spot->y + dist, &xh, &yh);

    bombspot = spot;
    bombsource = source;
    bombdamage = damage;

    for(y = yl; y <= yh; y++)
        for(x = xl; x <= xh; x++)
            P_BlockThingsIterator(x, y, PIT_RadiusAttack, 0);
}

/*
 * After modifying a sectors floor or ceiling height,
 * call this routine to adjust the positions
 * of all things that touch the sector.
 *
 * If anything doesn't fit anymore, true will be returned.
 * If crunch is true, they will take damage
 *  as they are being crushed.
 * If Crunch is false, you should set the sector height back
 *  the way it was and call P_ChangeSector again
 *  to undo the changes.
 *
 */
boolean PIT_ChangeSector(mobj_t *thing, void *data)
{
    mobj_t *mo;

    // Don't check things that aren't blocklinked (supposedly immaterial).
    if(thing->flags & MF_NOBLOCKMAP)
        return true;

    if(P_ThingHeightClip(thing))
    {
        // keep checking
        return true;
    }

    // crunch bodies to giblets
    if(!(thing->flags & MF_NOBLOOD) && thing->health <= 0)
    {
        P_SetMobjState(thing, S_GIBS);

        thing->flags &= ~MF_SOLID;
        thing->height = 0;
        thing->radius = 0;

        // keep checking
        return true;
    }

    // crunch dropped items
    if(thing->flags & MF_DROPPED)
    {
        P_RemoveMobj(thing);

        // keep checking
        return true;
    }

    if(!(thing->flags & MF_SHOOTABLE))
    {
        // assume it is bloody gibs or something
        return true;
    }

    nofit = true;

    if(crushchange && !(leveltime & 3))
    {
        P_DamageMobj(thing, NULL, NULL, 10);

        if(!(thing->flags & MF_NOBLOOD))
        {
            // spray blood in a random direction
            mo = P_SpawnMobj(thing->x, thing->y, thing->z + thing->height / 2,
                             MT_BLOOD);

            mo->momx = (P_Random() - P_Random()) << 12;
            mo->momy = (P_Random() - P_Random()) << 12;
        }
    }

    // keep checking (crush other things)
    return true;
}

boolean P_ChangeSector(sector_t *sector, boolean crunch)
{
    nofit = false;
    crushchange = crunch;

    validCount++;
    P_SectorTouchingThingsIterator(sector, PIT_ChangeSector, 0);

    return nofit;
}
