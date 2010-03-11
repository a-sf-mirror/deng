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

/**
 * p_map.c: Common map routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <stdio.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "gamemap.h"
#include "d_net.h"
#include "g_common.h"
#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_terraintype.h"
#include "p_tick.h"
#include "p_actor.h"
#include "p_player.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#if __JHEXEN__
#define USE_PUZZLE_ITEM_SPECIAL     129
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

#if __JDOOM64__
static void CheckMissileImpact(mobj_t* mobj);
#endif

#if __JHERETIC__
static void  CheckMissileImpact(mobj_t* mobj);
#elif __JHEXEN__
static void  P_FakeZMovement(mobj_t* mo);
static void  checkForPushSpecial(linedef_t* line, int side, mobj_t* mobj);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

float GameMap_Gravity(map_t* map)
{
    assert(map);
    if(IS_NETGAME && cfg.netGravity != -1)
        return (float) cfg.netGravity / 100;
    return map->gravity;
}

/**
 * Checks the reject matrix to find out if the two sectors are visible
 * from each other.
 */
static boolean checkReject(map_t* map, subsector_t* a, subsector_t* b)
{
    if(map->_rejectMatrix != NULL)
    {
        uint s1, s2, pnum, bytenum, bitnum;
        sector_t* sec1 = DMU_GetPtrp(a, DMU_SECTOR);
        sector_t* sec2 = DMU_GetPtrp(b, DMU_SECTOR);

        // Determine subsector entries in REJECT table.
        s1 = DMU_ToIndex(sec1);
        s2 = DMU_ToIndex(sec2);
        pnum = s1 * Map_NumSectors(map) + s2;
        bytenum = pnum >> 3;
        bitnum = 1 << (pnum & 7);

        // Check in REJECT table.
        if(map->_rejectMatrix[bytenum] & bitnum)
        {
            // Can't possibly be connected.
            return false;
        }
    }

    return true;
}

/**
 * Look from eyes of t1 to any part of t2 (start from middle of t1).
 *
 * @param from          The mobj doing the looking.
 * @param to            The mobj being looked at.
 *
 * @return              @c true if a straight line between t1 and t2 is
 *                      unobstructed.
 */
boolean P_CheckSight(const mobj_t* from, const mobj_t* to)
{
    assert(from);
    assert(to);
    {
    map_t* map = Thinker_Map((thinker_t*) from);
    float fPos[3];

    if(map != Thinker_Map((thinker_t*) to))
        return false; // Definetly not.

    // If either is unlinked, they can't see each other.
    if(!from->subsector || !to->subsector)
        return false;

    if(to->dPlayer && (to->dPlayer->flags & DDPF_CAMERA))
        return false; // Cameramen don't exist!

    // Check for trivial rejection.
    if(!checkReject(map, from->subsector, to->subsector))
        return false;

    fPos[VX] = from->pos[VX];
    fPos[VY] = from->pos[VY];
    fPos[VZ] = from->pos[VZ];

    if(!P_MobjIsCamera(from))
        fPos[VZ] += from->height + -(from->height / 4);

    return Map_CheckLineSight(map, fPos, to->pos, 0, to->height, LS_PASSMIDDLE);
    }
}

boolean PIT_StompThing(mobj_t* mo, void* data)
{
    map_t* map = Thinker_Map((thinker_t*) mo);
    int stompAnyway;
    float blockdist;

    if(!(mo->flags & MF_SHOOTABLE))
        return true;

    blockdist = mo->radius + map->tmThing->radius;
    if(fabs(mo->pos[VX] - map->tm[VX]) >= blockdist ||
       fabs(mo->pos[VY] - map->tm[VY]) >= blockdist)
        return true; // Didn't hit it.

    if(mo == map->tmThing)
        return true; // Don't clip against self.

    stompAnyway = *(int*) data;

    // Should we stomp anyway? unless self.
    if(mo != map->tmThing && stompAnyway)
    {
        P_DamageMobj(mo, map->tmThing, map->tmThing, 10000, true);
        return true;
    }

#if __JDOOM64__
    // monsters don't stomp things
    if(!map->tmThing->player)
        return false;
#elif __JDOOM__
    // Monsters don't stomp things except on a boss map.
    if(!map->tmThing->player && gameMap != 29)
        return false;
#endif

    if(!(map->tmThing->flags2 & MF2_TELESTOMP))
        return false; // Not allowed to stomp things.

    // Do stomp damage (unless self).
    if(mo != map->tmThing)
        P_DamageMobj(mo, map->tmThing, map->tmThing, 10000, true);

    return true;
}

boolean P_TeleportMove(mobj_t* thing, float x, float y, boolean alwaysStomp)
{
    assert(thing);
    {
    int stomping;
    subsector_t* newSSec;
    float box[4];
    map_t* map = Thinker_Map((thinker_t*) thing);

    // Kill anything occupying the position.
    map->tmThing = thing;
    stomping = alwaysStomp;

    map->tm[VX] = x;
    map->tm[VY] = y;

    map->tmBBox[BOXTOP]    = map->tm[VY] + map->tmThing->radius;
    map->tmBBox[BOXBOTTOM] = map->tm[VY] - map->tmThing->radius;
    map->tmBBox[BOXRIGHT]  = map->tm[VX] + map->tmThing->radius;
    map->tmBBox[BOXLEFT]   = map->tm[VX] - map->tmThing->radius;

    newSSec = Map_PointInSubsector(map, map->tm[VX], map->tm[VY]);

    map->ceilingLine = map->floorLine = NULL;
#if !__JHEXEN__
    map->blockLine = NULL;
    map->tmUnstuck = thing->dPlayer && thing->dPlayer->mo == thing;
#endif

    // The base floor / ceiling is from the subsector that contains the
    // point. Any contacted lines the step closer together will adjust them.
    map->tmFloorZ = map->tmDropoffZ = DMU_GetFloatp(newSSec, DMU_FLOOR_HEIGHT);
    map->tmCeilingZ = DMU_GetFloatp(newSSec, DMU_CEILING_HEIGHT);
#if __JHEXEN__
    map->tmFloorMaterial = DMU_GetPtrp(newSSec, DMU_FLOOR_MATERIAL);
#endif

    P_EmptyIterList(GameMap_SpecHits(map));

    box[BOXLEFT]   = map->tmBBox[BOXLEFT]   - MAXRADIUS;
    box[BOXRIGHT]  = map->tmBBox[BOXRIGHT]  + MAXRADIUS;
    box[BOXBOTTOM] = map->tmBBox[BOXBOTTOM] - MAXRADIUS;
    box[BOXTOP]    = map->tmBBox[BOXTOP]    + MAXRADIUS;

    // Stomp on any things contacted.
    VALIDCOUNT++;
    if(!Map_MobjsBoxIterator(map, box, PIT_StompThing, &stomping))
        return false;

    // The move is ok, so link the thing into its new position.
    P_MobjUnsetPosition(thing);

    thing->floorZ = map->tmFloorZ;
    thing->ceilingZ = map->tmCeilingZ;
#if !__JHEXEN__
    thing->dropOffZ = map->tmDropoffZ;
#endif
    thing->pos[VX] = x;
    thing->pos[VY] = y;

    P_MobjSetPosition(thing);
    P_MobjClearSRVO(thing);

    return true;
    }
}

/**
 * Checks to see if a start->end trajectory line crosses a blocking line.
 * Returns false if it does.
 *
 * tmBBox holds the bounding box of the trajectory. If that box does not
 * touch the bounding box of the line in question, then the trajectory is
 * not blocked. If the start is on one side of the line and the end is on
 * the other side, then the trajectory is blocked.
 *
 * Currently this assumes an infinite line, which is not quite correct.
 * A more correct solution would be to check for an intersection of the
 * trajectory and the line, but that takes longer and probably really isn't
 * worth the effort.
 *
 * @param data          Unused.
 */
boolean PIT_CrossLine(linedef_t* ld, void* data)
{
    map_t* map = P_CurrentMap();
    int flags = DMU_GetIntp(ld, DMU_FLAGS);

    if((flags & DDLF_BLOCKING) ||
       (P_ToXLine(ld)->flags & ML_BLOCKMONSTERS) ||
       (!DMU_GetPtrp(ld, DMU_FRONT_SECTOR) || !DMU_GetPtrp(ld, DMU_BACK_SECTOR)))
    {
        float bbox[4];

        DMU_GetFloatpv(ld, DMU_BOUNDING_BOX, bbox);

        if(!(map->tmBBox[BOXLEFT]   > bbox[BOXRIGHT] ||
             map->tmBBox[BOXRIGHT]  < bbox[BOXLEFT] ||
             map->tmBBox[BOXTOP]    < bbox[BOXBOTTOM] ||
             map->tmBBox[BOXBOTTOM] > bbox[BOXTOP]))
        {
            if(DMU_PointOnLineDefSide(map->startPos[VX], map->startPos[VY], ld) !=
               DMU_PointOnLineDefSide(map->endPos[VX], map->endPos[VY], ld))
                // Line blocks trajectory.
                return false;
        }
    }

    // Line doesn't block trajectory.
    return true;
}

/**
 * This routine checks for Lost Souls trying to be spawned across 1-sided
 * lines, impassible lines, or "monsters can't cross" lines.
 *
 * Draw an imaginary line between the PE and the new Lost Soul spawn spot.
 * If that line crosses a 'blocking' line, then disallow the spawn. Only
 * search lines in the blocks of the blockmap where the bounding box of the
 * trajectory line resides. Then check bounding box of the trajectory vs
 * the bounding box of each blocking line to see if the trajectory and the
 * blocking line cross. Then check the PE and LS to see if they are on
 * different sides of the blocking line. If so, return true otherwise
 * false.
 */
boolean P_CheckSides(mobj_t* actor, float x, float y)
{
    assert(actor);
    {
    map_t* map = Thinker_Map((thinker_t*) actor);

    map->startPos[VX] = actor->pos[VX];
    map->startPos[VY] = actor->pos[VY];
    map->startPos[VZ] = actor->pos[VZ];

    map->endPos[VX] = x;
    map->endPos[VY] = y;
    map->endPos[VZ] = DDMINFLOAT; // Initialize with *something*.

    // The bounding box of the trajectory
    map->tmBBox[BOXLEFT]   = (map->startPos[VX] < map->endPos[VX]? map->startPos[VX] : map->endPos[VX]);
    map->tmBBox[BOXRIGHT]  = (map->startPos[VX] > map->endPos[VX]? map->startPos[VX] : map->endPos[VX]);
    map->tmBBox[BOXTOP]    = (map->startPos[VY] > map->endPos[VY]? map->startPos[VY] : map->endPos[VY]);
    map->tmBBox[BOXBOTTOM] = (map->startPos[VY] < map->endPos[VY]? map->startPos[VY] : map->endPos[VY]);

    VALIDCOUNT++;
    return !Map_LineDefsBoxIterator(map, map->tmBBox, PIT_CrossLine, NULL);
    }
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
/**
 * $unstuck: used to test intersection between thing and line assuming NO
 * movement occurs -- used to avoid sticky situations.
 */
static int untouched(linedef_t* ld)
{
    map_t* map = P_CurrentMap();
    float x, y, box[4], bbox[4], radius;

    DMU_GetFloatpv(ld, DMU_BOUNDING_BOX, bbox);

    x = map->tmThing->pos[VX];
    y = map->tmThing->pos[VY];
    radius = map->tmThing->radius;

    if(((box[BOXRIGHT]  = x + radius) <= bbox[BOXLEFT]) ||
       ((box[BOXLEFT]   = x - radius) >= bbox[BOXRIGHT]) ||
       ((box[BOXTOP]    = y + radius) <= bbox[BOXBOTTOM]) ||
       ((box[BOXBOTTOM] = y - radius) >= bbox[BOXTOP]) ||
       DMU_BoxOnLineSide(box, ld) != -1)
        return true;

    return false;
}
#endif

boolean PIT_CheckThing(mobj_t* thing, void* data)
{
    map_t* map = Thinker_Map((thinker_t*) thing);
    int damage;
    float blockdist;
    boolean solid;
#if !__JHEXEN__
    boolean overlap = false;
#endif

    // Don't clip against self.
    if(thing == map->tmThing)
        return true;

#if __JHEXEN__
    // Don't clip on something we are stood on.
    if(thing == map->tmThing->onMobj)
        return true;
#endif

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)) ||
       P_MobjIsCamera(thing) || P_MobjIsCamera(map->tmThing))
        return true;

#if !__JHEXEN__
    // Player only.
    if(map->tmThing->player && map->tm[VZ] != DDMAXFLOAT &&
       (cfg.moveCheckZ || (map->tmThing->flags2 & MF2_PASSMOBJ)))
    {
        if((thing->pos[VZ] > map->tm[VZ] + map->tmHeight) ||
           (thing->pos[VZ] + thing->height < map->tm[VZ]))
            return true; // Under or over it.

        overlap = true;
    }
#endif

    blockdist = thing->radius + map->tmThing->radius;
    if(fabs(thing->pos[VX] - map->tm[VX]) >= blockdist ||
       fabs(thing->pos[VY] - map->tm[VY]) >= blockdist)
        return true; // Didn't hit thing.

#if __JHEXEN__
    // Stop here if we are a client.
    if(IS_CLIENT)
        return false;
#endif

#if !__JHEXEN__
    if(!map->tmThing->player && (map->tmThing->flags2 & MF2_PASSMOBJ))
#else
    map->blockingMobj = thing;
    if(map->tmThing->flags2 & MF2_PASSMOBJ)
#endif
    {   // Check if a mobj passed over/under another object.
#if __JHERETIC__
        if((map->tmThing->type == MT_IMP || map->tmThing->type == MT_WIZARD) &&
           (thing->type == MT_IMP || thing->type == MT_WIZARD))
        {
            return false; // Don't let imps/wizards fly over other imps/wizards.
        }
#elif __JHEXEN__
        if(map->tmThing->type == MT_BISHOP && thing->type == MT_BISHOP)
            return false; // Don't let bishops fly over other bishops.
#endif

        if(!(thing->flags & MF_SPECIAL))
        {
            if(map->tmThing->pos[VZ] > thing->pos[VZ] + thing->height ||
               map->tmThing->pos[VZ] + map->tmThing->height < thing->pos[VZ])
                return true; // Over/under thing.
        }
    }

    // Check for skulls slamming into things.
    if((map->tmThing->flags & MF_SKULLFLY) && (thing->flags & MF_SOLID))
    {
#if __JHEXEN__
        map->blockingMobj = NULL;
        if(map->tmThing->type == MT_MINOTAUR)
        {
            // Slamming minotaurs shouldn't move non-creatures.
            if(!(thing->flags & MF_COUNTKILL))
            {
                return false;
            }
        }
        else if(map->tmThing->type == MT_HOLY_FX)
        {
            if((thing->flags & MF_SHOOTABLE) && thing != map->tmThing->target)
            {
                if(IS_NETGAME && !deathmatch && thing->player)
                    return true; // don't attack other co-op players

                if((thing->flags2 & MF2_REFLECTIVE) &&
                   (thing->player || (thing->flags2 & MF2_BOSS)))
                {
                    map->tmThing->tracer = map->tmThing->target;
                    map->tmThing->target = thing;
                    return true;
                }

                if(thing->flags & MF_COUNTKILL || thing->player)
                {
                    map->tmThing->tracer = thing;
                }

                if(P_Random() < 96)
                {
                    damage = 12;
                    if(thing->player || (thing->flags2 & MF2_BOSS))
                    {
                        damage = 3;
                        // Ghost burns out faster when attacking players/bosses.
                        map->tmThing->health -= 6;
                    }

                    P_DamageMobj(thing, map->tmThing, map->tmThing->target, damage, false);
                    if(P_Random() < 128)
                    {
                        GameMap_SpawnMobj3fv(map, MT_HOLY_PUFF, map->tmThing->pos, P_Random() << 24, 0);
                        S_StartSound(SFX_SPIRIT_ATTACK, map->tmThing);
                        if((thing->flags & MF_COUNTKILL) && P_Random() < 128 &&
                           !S_IsPlaying(SFX_PUPPYBEAT, thing))
                        {
                            if((thing->type == MT_CENTAUR) ||
                               (thing->type == MT_CENTAURLEADER) ||
                               (thing->type == MT_ETTIN))
                            {
                                S_StartSound(SFX_PUPPYBEAT, thing);
                            }
                        }
                    }
                }

                if(thing->health <= 0)
                    map->tmThing->tracer = NULL;
            }
            return true;
        }
#endif
#if __JDOOM__
        if(map->tmThing->damage == DDMAXINT) // @kludge to support old save games.
            damage = map->tmThing->info->damage;
        else
#endif
            damage = map->tmThing->damage;

        damage *= (P_Random() % 8) + 1;
        P_DamageMobj(thing, map->tmThing, map->tmThing, damage, false);

        map->tmThing->flags &= ~MF_SKULLFLY;
        map->tmThing->mom[MX] = map->tmThing->mom[MY] = map->tmThing->mom[MZ] = 0;

#if __JHERETIC__ || __JHEXEN__
        P_MobjChangeState(map->tmThing, P_GetState(map->tmThing->type, SN_SEE));
#else
        P_MobjChangeState(map->tmThing, P_GetState(map->tmThing->type, SN_SPAWN));
#endif

        return false; // Stop moving.
    }

#if __JHEXEN__
    // Check for blasted thing running into another
    if((map->tmThing->flags2 & MF2_BLASTED) && (thing->flags & MF_SHOOTABLE))
    {
        if(!(thing->flags2 & MF2_BOSS) && (thing->flags & MF_COUNTKILL))
        {
            thing->mom[MX] += map->tmThing->mom[MX];
            thing->mom[MY] += map->tmThing->mom[MY];

            if(thing->dPlayer)
                thing->dPlayer->flags |= DDPF_FIXMOM;

            if((thing->mom[MX] + thing->mom[MY]) > 3)
            {
                damage = (map->tmThing->info->mass / 100) + 1;
                P_DamageMobj(thing, map->tmThing, map->tmThing, damage, false);

                damage = (thing->info->mass / 100) + 1;
                P_DamageMobj(map->tmThing, thing, thing, damage >> 2, false);
            }

            return false;
        }
    }
#endif

    // Missiles can hit other things.
    if(map->tmThing->flags & MF_MISSILE)
    {
#if __JHEXEN__
        // Check for a non-shootable mobj.
        if(thing->flags2 & MF2_NONSHOOTABLE)
            return true;
#else
        // Check for passing through a ghost.
        if((thing->flags & MF_SHADOW) && (map->tmThing->flags2 & MF2_THRUGHOST))
            return true;
#endif

        // See if it went over / under.
        if(map->tmThing->pos[VZ] > thing->pos[VZ] + thing->height)
            return true; // Overhead.

        if(map->tmThing->pos[VZ] + map->tmThing->height < thing->pos[VZ])
            return true; // Underneath.

#if __JHEXEN__
        if(map->tmThing->flags2 & MF2_FLOORBOUNCE)
        {
            if(map->tmThing->target == thing || !(thing->flags & MF_SOLID))
                return true;
            else
                return false;
        }

        if(map->tmThing->type == MT_LIGHTNING_FLOOR ||
           map->tmThing->type == MT_LIGHTNING_CEILING)
        {
            if((thing->flags & MF_SHOOTABLE) && thing != map->tmThing->target)
            {
                if(thing->info->mass != DDMAXINT)
                {
                    thing->mom[MX] += map->tmThing->mom[MX] / 16;
                    thing->mom[MY] += map->tmThing->mom[MY] / 16;
                    if(thing->dPlayer)
                        thing->dPlayer->flags |= DDPF_FIXMOM;
                }

                if((!thing->player && !(thing->flags2 & MF2_BOSS)) ||
                   !(map->time & 1))
                {
                    if(thing->type == MT_CENTAUR ||
                       thing->type == MT_CENTAURLEADER)
                    {   // Lightning does more damage to centaurs.
                        P_DamageMobj(thing, map->tmThing, map->tmThing->target, 9, false);
                    }
                    else
                    {
                        P_DamageMobj(thing, map->tmThing, map->tmThing->target, 3, false);
                    }

                    if(!(S_IsPlaying(SFX_MAGE_LIGHTNING_ZAP, map->tmThing)))
                    {
                        S_StartSound(SFX_MAGE_LIGHTNING_ZAP, map->tmThing);
                    }

                    if((thing->flags & MF_COUNTKILL) && P_Random() < 64 &&
                       !S_IsPlaying(SFX_PUPPYBEAT, thing))
                    {
                        if((thing->type == MT_CENTAUR) ||
                           (thing->type == MT_CENTAURLEADER) ||
                           (thing->type == MT_ETTIN))
                        {
                            S_StartSound(SFX_PUPPYBEAT, thing);
                        }
                    }
                }

                map->tmThing->health--;
                if(map->tmThing->health <= 0 || thing->health <= 0)
                {
                    return false;
                }

                if(map->tmThing->type == MT_LIGHTNING_FLOOR)
                {
                    if(map->tmThing->lastEnemy &&
                       !map->tmThing->lastEnemy->tracer)
                    {
                        map->tmThing->lastEnemy->tracer = thing;
                    }
                }
                else if(!map->tmThing->tracer)
                {
                    map->tmThing->tracer = thing;
                }
            }

            return true; // Lightning zaps through all sprites.
        }
        else if(map->tmThing->type == MT_LIGHTNING_ZAP)
        {
            mobj_t *lmo;

            if((thing->flags & MF_SHOOTABLE) && thing != map->tmThing->target)
            {
                lmo = map->tmThing->lastEnemy;
                if(lmo)
                {
                    if(lmo->type == MT_LIGHTNING_FLOOR)
                    {
                        if(lmo->lastEnemy &&
                           !lmo->lastEnemy->tracer)
                        {
                            lmo->lastEnemy->tracer = thing;
                        }
                    }
                    else if(!lmo->tracer)
                    {
                        lmo->tracer = thing;
                    }

                    if(!(map->time & 3))
                    {
                        lmo->health--;
                    }
                }
            }
        }
        else if(map->tmThing->type == MT_MSTAFF_FX2 && thing != map->tmThing->target)
        {
            if(!thing->player && !(thing->flags2 & MF2_BOSS))
            {
                switch(thing->type)
                {
                case MT_FIGHTER_BOSS:   // these not flagged boss
                case MT_CLERIC_BOSS:    // so they can be blasted
                case MT_MAGE_BOSS:
                    break;
                default:
                    P_DamageMobj(thing, map->tmThing, map->tmThing->target, 10, false);
                    return true;
                    break;
                }
            }
        }
#endif

        // Don't hit same species as originator.
#if __JDOOM__ || __JDOOM64__
        if(map->tmThing->target &&
           (map->tmThing->target->type == thing->type ||
           (map->tmThing->target->type == MT_KNIGHT && thing->type == MT_BRUISER) ||
           (map->tmThing->target->type == MT_BRUISER && thing->type == MT_KNIGHT)))
#else
        if(map->tmThing->target && map->tmThing->target->type == thing->type)
#endif
        {
            if(thing == map->tmThing->target)
                return true;

#if __JHEXEN__
            if(!thing->player)
                return false; // Hit same species as originator, explode, no damage
#else
            if(!monsterInfight && thing->type != MT_PLAYER)
            {
                // Explode, but do no damage.
                // Let players missile other players.
                return false;
            }
#endif
        }

        if(!(thing->flags & MF_SHOOTABLE))
        {
            return !(thing->flags & MF_SOLID); // Didn't do any damage.
        }

        if(map->tmThing->flags2 & MF2_RIP)
        {
#if __JHEXEN__
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE))
#else
            if(!(thing->flags & MF_NOBLOOD))
#endif
            {   // Ok to spawn some blood.
                P_RipperBlood(map->tmThing);
            }
#if __JHERETIC__
            S_StartSound(SFX_RIPSLOP, map->tmThing);
#endif
#if __JDOOM__
            if(map->tmThing->damage == DDMAXINT) //// \kludge to support old save games.
                damage = map->tmThing->info->damage;
            else
#endif
                damage = map->tmThing->damage;

            damage *= (P_Random() & 3) + 2;

            P_DamageMobj(thing, map->tmThing, map->tmThing->target, damage, false);

            if((thing->flags2 & MF2_PUSHABLE) &&
               !(map->tmThing->flags2 & MF2_CANNOTPUSH))
            {   // Push thing
                thing->mom[MX] += map->tmThing->mom[MX] / 4;
                thing->mom[MY] += map->tmThing->mom[MY] / 4;
                if(thing->dPlayer)
                    thing->dPlayer->flags |= DDPF_FIXMOM;
            }
            P_EmptyIterList(GameMap_SpecHits(map));
            return true;
        }

        // Do damage
#if __JDOOM__
        if(map->tmThing->damage == DDMAXINT) //// \kludge to support old save games.
            damage = map->tmThing->info->damage;
        else
#endif
            damage = map->tmThing->damage;

        damage *= (P_Random() % 8) + 1;
#if __JDOOM__ || __JDOOM64__
        P_DamageMobj(thing, map->tmThing, map->tmThing->target, damage, false);
#else
        if(damage)
        {
# if __JHERETIC__
            if(!(thing->flags & MF_NOBLOOD) && P_Random() < 192)
# else //__JHEXEN__
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE) &&
               !(map->tmThing->type == MT_TELOTHER_FX1) &&
               !(map->tmThing->type == MT_TELOTHER_FX2) &&
               !(map->tmThing->type == MT_TELOTHER_FX3) &&
               !(map->tmThing->type == MT_TELOTHER_FX4) &&
               !(map->tmThing->type == MT_TELOTHER_FX5) && (P_Random() < 192))
# endif
                P_SpawnBloodSplatter(map, map->tmThing->pos[VX], map->tmThing->pos[VY], map->tmThing->pos[VZ], thing);

            P_DamageMobj(thing, map->tmThing, map->tmThing->target, damage, false);
        }
#endif
        // Don't traverse anymore.
        return false;
    }

    if((thing->flags2 & MF2_PUSHABLE) && !(map->tmThing->flags2 & MF2_CANNOTPUSH))
    {   // Push thing
        thing->mom[MX] += map->tmThing->mom[MX] / 4;
        thing->mom[MY] += map->tmThing->mom[MY] / 4;
        if(thing->dPlayer)
            thing->dPlayer->flags |= DDPF_FIXMOM;
    }

    // \kludge: Always treat blood as a solid.
    if(map->tmThing->type == MT_BLOOD)
        solid = true;
    else
        solid = (thing->flags & MF_SOLID) && !(thing->flags & MF_NOCLIP) &&
            (map->tmThing->flags & MF_SOLID);
    // \kludge: end.

    // Check for special pickup.
    if((thing->flags & MF_SPECIAL) && (map->tmThing->flags & MF_PICKUP))
    {
        P_TouchSpecialMobj(thing, map->tmThing); // Can remove thing.
    }
#if !__JHEXEN__
    else if(overlap && solid)
    {
        // How are we positioned?
        if(map->tm[VZ] > thing->pos[VZ] + thing->height - 24)
        {
            map->tmThing->onMobj = thing;
            if(thing->pos[VZ] + thing->height > map->tmFloorZ)
                map->tmFloorZ = thing->pos[VZ] + thing->height;
            return true;
        }
    }
#endif

    return !solid;
}

/**
 * Adjusts tmFloorZ and tmCeilingZ as lines are contacted.
 */
boolean PIT_CheckLine(linedef_t* ld, void* data)
{
    float bbox[4];
    xlinedef_t* xline;
    map_t* map = P_CurrentMap();

    DMU_GetFloatpv(ld, DMU_BOUNDING_BOX, bbox);

    if(map->tmBBox[BOXRIGHT]  <= bbox[BOXLEFT] ||
       map->tmBBox[BOXLEFT]   >= bbox[BOXRIGHT] ||
       map->tmBBox[BOXTOP]    <= bbox[BOXBOTTOM] ||
       map->tmBBox[BOXBOTTOM] >= bbox[BOXTOP])
        return true;

    if(DMU_BoxOnLineSide(map->tmBBox, ld) != -1)
        return true;

    // A line has been hit
    xline = P_ToXLine(ld);
#if !__JHEXEN__
    map->tmThing->wallHit = true;

    // A Hit event will be sent to special lines.
    if(xline->special)
        map->tmHitLine = ld;
#endif

    if(!DMU_GetPtrp(ld, DMU_BACK_SECTOR)) // One sided line.
    {
#if __JHEXEN__
        if(map->tmThing->flags2 & MF2_BLASTED)
            P_DamageMobj(map->tmThing, NULL, NULL, map->tmThing->info->mass >> 5, false);
        checkForPushSpecial(ld, 0, map->tmThing);
        return false;
#else
        float d1[2];

        DMU_GetFloatpv(ld, DMU_DXY, d1);

        /**
         * $unstuck: allow player to move out of 1s wall, to prevent
         * sticking. The moving thing's destination position will cross the
         * given line.
         * If this should not be allowed, return false.
         * If the line is special, keep track of it to process later if the
         * move is proven ok.
         *
         * \note Specials are NOT sorted by order, so two special lines that
         * are only 8 units apart could be crossed in either order.
         */

        map->blockLine = ld;
        return map->tmUnstuck && !untouched(ld) &&
            ((map->tm[VX] - map->tmThing->pos[VX]) * d1[1]) >
            ((map->tm[VY] - map->tmThing->pos[VY]) * d1[0]);
#endif
    }

    // \fixme Will never pass this test due to above. Is the previous check
    // supposed to qualify player mobjs only?
#if __JHERETIC__
    if(!DMU_GetPtrp(ld, DMU_BACK_SECTOR)) // one sided line
    {   // One sided line
        if(map->tmThing->flags & MF_MISSILE)
        {   // Missiles can trigger impact specials
            if(xline->special)
                P_AddObjectToIterList(GameMap_SpecHits(map), ld);
        }
        return false;
    }
#endif

    if(!(map->tmThing->flags & MF_MISSILE))
    {
        // Explicitly blocking everything?
        if(DMU_GetIntp(ld, DMU_FLAGS) & DDLF_BLOCKING)
        {
#if __JHEXEN__
            if(map->tmThing->flags2 & MF2_BLASTED)
                P_DamageMobj(map->tmThing, NULL, NULL, map->tmThing->info->mass >> 5, false);
            checkForPushSpecial(ld, 0, map->tmThing);
            return false;
#else
            // $unstuck: allow escape.
            return map->tmUnstuck && !untouched(ld);
#endif
        }

        // Block monsters only?
#if __JHEXEN__
        if(!map->tmThing->player && map->tmThing->type != MT_CAMERA &&
           (xline->flags & ML_BLOCKMONSTERS))
#elif __JHERETIC__
        if(!map->tmThing->player && map->tmThing->type != MT_POD &&
           (xline->flags & ML_BLOCKMONSTERS))
#else
        if(!map->tmThing->player &&
            (xline->flags & ML_BLOCKMONSTERS))
#endif
        {
#if __JHEXEN__
            if(map->tmThing->flags2 & MF2_BLASTED)
                P_DamageMobj(map->tmThing, NULL, NULL, map->tmThing->info->mass >> 5, false);
#endif
            return false;
        }
    }

#if __JDOOM64__
    if((map->tmThing->flags & MF_MISSILE))
    {
        if(xline->flags & ML_BLOCKALL) // Explicitly blocking everything.
            return map->tmUnstuck && !untouched(ld);  // $unstuck: allow escape.
    }
#endif

    // Set OPENRANGE, OPENTOP, OPENBOTTOM.
    DMU_LineOpening(ld);

    // Adjust floor / ceiling heights.
    if(OPENTOP < map->tmCeilingZ)
    {
        map->tmCeilingZ = OPENTOP;
        map->ceilingLine = ld;
#if !__JHEXEN__
        map->blockLine = ld;
#endif
    }

    if(OPENBOTTOM > map->tmFloorZ)
    {
        map->tmFloorZ = OPENBOTTOM;
        map->floorLine = ld;
#if !__JHEXEN__
        map->blockLine = ld;
#endif
    }

    if(LOWFLOOR < map->tmDropoffZ)
        map->tmDropoffZ = LOWFLOOR;

    // If contacted a special line, add it to the list.
    if(P_ToXLine(ld)->special)
        P_AddObjectToIterList(GameMap_SpecHits(map), ld);

#if !__JHEXEN__
    map->tmThing->wallHit = false;
#endif
    return true; // Continue iteration.
}

/**
 * This is purely informative, nothing is modified (except things picked up).
 *
 * in:
 *  a mobj_t (can be valid or invalid)
 *  a position to be checked
 *   (doesn't need to be related to the mobj_t->x,y)
 *
 * during:
 *  special things are touched if MF_PICKUP early out on solid lines?
 *
 * out:
 *  newsubsec
 *  floorz
 *  ceilingz
 *  tmDropoffZ
 *   the lowest point contacted
 *   (monsters won't move to a drop off)
 *  speciallines[]
 *  numspeciallines
 */
boolean P_CheckPosition3f(mobj_t* thing, float x, float y, float z)
{
    sector_t* newSec;
    float box[4];
    map_t* map = Thinker_Map((thinker_t*) thing);

    map->tmThing = thing;

#if !__JHEXEN__
    thing->onMobj = NULL;
    thing->wallHit = false;

    map->tmHitLine = NULL;
    map->tmHeight = thing->height;
#endif

    map->tm[VX] = x;
    map->tm[VY] = y;
    map->tm[VZ] = z;

    map->tmBBox[BOXTOP]    = map->tm[VY] + map->tmThing->radius;
    map->tmBBox[BOXBOTTOM] = map->tm[VY] - map->tmThing->radius;
    map->tmBBox[BOXRIGHT]  = map->tm[VX] + map->tmThing->radius;
    map->tmBBox[BOXLEFT]   = map->tm[VX] - map->tmThing->radius;

    newSec = DMU_GetPtrp(Map_PointInSubsector(map, map->tm[VX], map->tm[VY]), DMU_SECTOR);

    map->ceilingLine = map->floorLine = NULL;
#if !__JHEXEN__
    map->blockLine = NULL;
    map->tmUnstuck =
        ((thing->dPlayer && thing->dPlayer->mo == thing)? true : false);
#endif

    // The base floor/ceiling is from the subsector that contains the point.
    // Any contacted lines the step closer together will adjust them.
    map->tmFloorZ = map->tmDropoffZ = DMU_GetFloatp(newSec, DMU_FLOOR_HEIGHT);
    map->tmCeilingZ = DMU_GetFloatp(newSec, DMU_CEILING_HEIGHT);
#if __JHEXEN__
    map->tmFloorMaterial = DMU_GetPtrp(newSec, DMU_FLOOR_MATERIAL);
#endif

    P_EmptyIterList(GameMap_SpecHits(map));

#if __JHEXEN__
    if((map->tmThing->flags & MF_NOCLIP) && !(map->tmThing->flags & MF_SKULLFLY))
        return true;
#else
    if((map->tmThing->flags & MF_NOCLIP))
        return true;
#endif

    // Check things first, possibly picking things up the bounding box is
    // extended by MAXRADIUS because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap into adjacent blocks by
    // up to MAXRADIUS units.
    box[BOXLEFT]   = map->tmBBox[BOXLEFT]   - MAXRADIUS;
    box[BOXRIGHT]  = map->tmBBox[BOXRIGHT]  + MAXRADIUS;
    box[BOXBOTTOM] = map->tmBBox[BOXBOTTOM] - MAXRADIUS;
    box[BOXTOP]    = map->tmBBox[BOXTOP]    + MAXRADIUS;

    VALIDCOUNT++;

    // The camera goes through all objects.
    if(!P_MobjIsCamera(thing))
    {
#if __JHEXEN__
        map->blockingMobj = NULL;
#endif
        if(!Map_MobjsBoxIterator(map, box, PIT_CheckThing, 0))
            return false;

/* #if _DEBUG
if(thing->onMobj)
    Con_Message("thing->onMobj = %p (solid:%i)\n", thing->onMobj, (thing->onMobj->flags & MF_SOLID)!=0);
#endif */
    }

     // Check lines.
#if __JHEXEN__
    if(map->tmThing->flags & MF_NOCLIP)
        return true;

    map->blockingMobj = NULL;
#endif

    box[BOXLEFT]   = map->tmBBox[BOXLEFT];
    box[BOXRIGHT]  = map->tmBBox[BOXRIGHT];
    box[BOXBOTTOM] = map->tmBBox[BOXBOTTOM];
    box[BOXTOP]    = map->tmBBox[BOXTOP];

    return Map_LineDefsBoxIterator(map, box, PIT_CheckLine, 0);
}

boolean P_CheckPosition3fv(mobj_t* thing, const float pos[3])
{
    return P_CheckPosition3f(thing, pos[VX], pos[VY], pos[VZ]);
}

boolean P_CheckPosition2f(mobj_t* thing, float x, float y)
{
    return P_CheckPosition3f(thing, x, y, DDMAXFLOAT);
}

/**
 * Attempt to move to a new position, crossing special lines unless
 * MF_TELEPORT is set. $dropoff_fix
 */
#if __JHEXEN__
static boolean P_TryMove2(mobj_t* thing, float x, float y)
#else
static boolean P_TryMove2(mobj_t* thing, float x, float y, boolean dropoff)
#endif
{
    float oldpos[3];
    int side, oldSide;
    linedef_t* ld;
    map_t* map = Thinker_Map((thinker_t*) thing);

    // $dropoff_fix: fellDown.
    map->floatOk = false;
#if !__JHEXEN__
    map->fellDown = false;
#endif

#if __JHEXEN__
    if(!P_CheckPosition2f(thing, x, y))
#else
    if(!P_CheckPosition3f(thing, x, y, thing->pos[VZ]))
#endif
    {
#if __JHEXEN__
        if(!map->blockingMobj || map->blockingMobj->player || !thing->player)
        {
            goto pushline;
        }
        else if(map->blockingMobj->pos[VZ] + map->blockingMobj->height - thing->pos[VZ] > 24 ||
                (DMU_GetFloatp(map->blockingMobj->subsector, DMU_CEILING_HEIGHT) -
                 (map->blockingMobj->pos[VZ] + map->blockingMobj->height) < thing->height) ||
                (map->tmCeilingZ - (map->blockingMobj->pos[VZ] + map->blockingMobj->height) <
                 thing->height))
        {
            goto pushline;
        }
#else
# if __JHERETIC__
        CheckMissileImpact(thing);
# endif
        // Would we hit another thing or a solid wall?
        if(!thing->onMobj || thing->wallHit)
            return false;
#endif
    }

    if(!(thing->flags & MF_NOCLIP))
    {
#if __JHEXEN__
        if(map->tmCeilingZ - map->tmFloorZ < thing->height)
        {   // Doesn't fit.
            goto pushline;
        }

        map->floatOk = true;

        if(!(thing->flags & MF_TELEPORT) &&
           map->tmCeilingZ - thing->pos[VZ] < thing->height &&
           thing->type != MT_LIGHTNING_CEILING && !(thing->flags2 & MF2_FLY))
        {   // Mobj must lower itself to fit.
            goto pushline;
        }
#else
        // Possibly allow escape if otherwise stuck.
        boolean ret = (map->tmUnstuck &&
            !(map->ceilingLine && untouched(map->ceilingLine)) &&
            !(map->floorLine   && untouched(map->floorLine)));

        if(map->tmCeilingZ - map->tmFloorZ < thing->height)
            return ret; // Doesn't fit.

        // Mobj must lower to fit.
        map->floatOk = true;
        if(!(thing->flags & MF_TELEPORT) && !(thing->flags2 & MF2_FLY) &&
           map->tmCeilingZ - thing->pos[VZ] < thing->height)
            return ret;

        // Too big a step up.
        if(!(thing->flags & MF_TELEPORT) &&
            !(thing->flags2 & MF2_FLY)
# if __JHERETIC__
            && thing->type != MT_MNTRFX2 // The Minotaur floor fire (MT_MNTRFX2) can step up any amount
# endif
            )
        {
            if(map->tmFloorZ - thing->pos[VZ] > 24)
            {
# if __JHERETIC__
                CheckMissileImpact(thing);
# endif
                return ret;
            }
        }
# if __JHERETIC__
        if((thing->flags & MF_MISSILE) && map->tmFloorZ > thing->pos[VZ])
        {
            CheckMissileImpact(thing);
        }
# endif
#endif
        if(thing->flags2 & MF2_FLY)
        {
            if(thing->pos[VZ] + thing->height > map->tmCeilingZ)
            {
                thing->mom[MZ] = -8;
#if __JHEXEN__
                goto pushline;
#else
                return false;
#endif
            }
            else if(thing->pos[VZ] < map->tmFloorZ &&
                    map->tmFloorZ - map->tmDropoffZ > 24)
            {
                thing->mom[MZ] = 8;
#if __JHEXEN__
                goto pushline;
#else
                return false;
#endif
            }
        }

#if __JHEXEN__
        if(!(thing->flags & MF_TELEPORT)
           // The Minotaur floor fire (MT_MNTRFX2) can step up any amount
           && thing->type != MT_MNTRFX2 && thing->type != MT_LIGHTNING_FLOOR &&
           map->tmFloorZ - thing->pos[VZ] > 24)
        {
            goto pushline;
        }
#endif

#if __JHEXEN__
        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)) &&
           (map->tmFloorZ - map->tmDropoffZ > 24) &&
           !(thing->flags2 & MF2_BLASTED))
        {   // Can't move over a dropoff unless it's been blasted.
            return false;
        }
#else
        /**
         * Allow certain objects to drop off.
         * Prevent monsters from getting stuck hanging off ledges.
         * Allow dropoffs in controlled circumstances.
         * Improve symmetry of clipping on stairs.
         */
        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)))
        {
            // Dropoff height limit.
            if(cfg.avoidDropoffs)
            {
                if(map->tmFloorZ - map->tmDropoffZ > 24)
                    return false; // Don't stand over dropoff.
            }
            else
            {
                float floorZ = map->tmFloorZ;

                if(thing->onMobj)
                {
                    // Thing is stood on something so use our z position
                    // as the floor.
                    floorZ = (thing->pos[VZ] > map->tmFloorZ ?
                        thing->pos[VZ] : map->tmFloorZ);
                }

                if(!dropoff)
                {
                   if(thing->floorZ - floorZ > 24 ||
                      thing->dropOffZ - map->tmDropoffZ > 24)
                      return false;
                }
                else
                {   // Set fellDown if drop > 24.
                    map->fellDown = !(thing->flags & MF_NOGRAVITY) &&
                        thing->pos[VZ] - floorZ > 24;
                }
            }
        }
#endif

#if __JDOOM64__
        // @fixme DJS - FIXME! Mother demon fire attack.
        if(!(thing->flags & MF_TELEPORT) /*&& thing->type != MT_SPAWNFIRE*/
            && map->tmFloorZ - thing->pos[VZ] > 24)
        { // Too big a step up
            CheckMissileImpact(thing);
            return false;
        }
#endif

#if __JHEXEN__
        // Must stay within a sector of a certain floor type?
        if((thing->flags2 & MF2_CANTLEAVEFLOORPIC) &&
           (map->tmFloorMaterial != DMU_GetPtrp(thing->subsector, DMU_FLOOR_MATERIAL) ||
            map->tmFloorZ - thing->pos[VZ] != 0))
        {
            return false;
        }
#endif

#if !__JHEXEN__
        // $dropoff: prevent falling objects from going up too many steps.
        if(!thing->player && (thing->intFlags & MIF_FALLING) &&
           map->tmFloorZ - thing->pos[VZ] > (thing->mom[MX] * thing->mom[MX]) +
                                       (thing->mom[MY] * thing->mom[MY]))
        {
            return false;
        }
#endif
    }

    // The move is ok, so link the thing into its new position.
    P_MobjUnsetPosition(thing);

    oldpos[VX] = thing->pos[VX];
    oldpos[VY] = thing->pos[VY];
    oldpos[VZ] = thing->pos[VZ];

    thing->floorZ = map->tmFloorZ;
    thing->ceilingZ = map->tmCeilingZ;
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    thing->dropOffZ = map->tmDropoffZ; // $dropoff_fix: keep track of dropoffs.
#endif

    thing->pos[VX] = x;
    thing->pos[VY] = y;

    P_MobjSetPosition(thing);

    if(thing->flags2 & MF2_FLOORCLIP)
    {
        thing->floorClip = 0;

        if(thing->pos[VZ] == DMU_GetFloatp(thing->subsector, DMU_FLOOR_HEIGHT))
        {
            const terraintype_t* tt = P_MobjGetFloorTerrainType(thing);

            if(tt->flags & TTF_FLOORCLIP)
            {
                thing->floorClip = 10;
            }
        }
    }

    // If any special lines were hit, do the effect.
    if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        while((ld = P_PopIterList(GameMap_SpecHits(map))) != NULL)
        {
            // See if the line was crossed.
            if(P_ToXLine(ld)->special)
            {
                side = DMU_PointOnLineDefSide(thing->pos[VX], thing->pos[VY], ld);
                oldSide = DMU_PointOnLineDefSide(oldpos[VX], oldpos[VY], ld);
                if(side != oldSide)
                {
#if __JHEXEN__
                    if(thing->player)
                    {
                        P_ActivateLine(ld, thing, oldSide, SPAC_CROSS);
                    }
                    else if(thing->flags2 & MF2_MCROSS)
                    {
                        P_ActivateLine(ld, thing, oldSide, SPAC_MCROSS);
                    }
                    else if(thing->flags2 & MF2_PCROSS)
                    {
                        P_ActivateLine(ld, thing, oldSide, SPAC_PCROSS);
                    }
#else
                    P_ActivateLine(ld, thing, oldSide, SPAC_CROSS);
#endif
                }
            }
        }
    }

    return true;

#if __JHEXEN__
  pushline:
    if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        if(map->tmThing->flags2 & MF2_BLASTED)
        {
            P_DamageMobj(map->tmThing, NULL, NULL, map->tmThing->info->mass >> 5, false);
        }

        P_IterListResetIterator(GameMap_SpecHits(map), false);
        while((ld = P_IterListIterator(GameMap_SpecHits(map))) != NULL)
        {
            // See if the line was crossed.
            side = DMU_PointOnLineDefSide(thing->pos[VX], thing->pos[VY], ld);
            checkForPushSpecial(ld, side, thing);
        }
    }
    return false;
#endif
}

#if __JHEXEN__
boolean P_TryMove(mobj_t* thing, float x, float y)
#else
boolean P_TryMove(mobj_t* thing, float x, float y, boolean dropoff,
                  boolean slide)
#endif
{
#if __JHEXEN__
    return P_TryMove2(thing, x, y);
#else
    // $dropoff_fix
    map_t* map = Thinker_Map((thinker_t*) thing);
    boolean res = P_TryMove2(thing, x, y, dropoff);

    if(!res && map->tmHitLine)
    {
        // Move not possible, see if the thing hit a line and send a Hit
        // event to it.
        XL_HitLine(map->tmHitLine, DMU_PointOnLineDefSide(thing->pos[VX], thing->pos[VY], map->tmHitLine),
                   thing);
    }

    if(res && slide)
        thing->wallRun = true;

    return res;
#endif
}

/**
 * @fixme DJS - This routine has gotten way too big, split if(in->isaline)
 * to a seperate routine?
 */
boolean PTR_ShootTraverse(intercept_t* in)
{
    map_t* map = P_CurrentMap();
    int divisor;
    float pos[3], frac, slope, dist, thingTopSlope, thingBottomSlope, cTop,
        cBottom, d[3], step, stepv[3], tracePos[3], cFloor, cCeil;
    linedef_t* li;
    mobj_t* th;
    divline_t* trace = (divline_t*) DD_GetVariable(DD_TRACE_ADDRESS);
    sector_t* frontSec = NULL, *backSec = NULL;
    subsector_t* contact, *originSub;
    xlinedef_t* xline;
    boolean lineWasHit;

    tracePos[VX] = FIX2FLT(trace->pos[VX]);
    tracePos[VY] = FIX2FLT(trace->pos[VY]);
    tracePos[VZ] = map->shootZ;

    if(in->type == ICPT_LINE)
    {
        li = in->d.lineDef;
        xline = P_ToXLine(li);

        frontSec = DMU_GetPtrp(li, DMU_FRONT_SECTOR);
        backSec = DMU_GetPtrp(li, DMU_BACK_SECTOR);

        if(!backSec && DMU_PointOnLineDefSide(tracePos[VX], tracePos[VY], li))
            return true; // Continue traversal.

        if(xline->special)
            P_ActivateLine(li, map->shootThing, 0, SPAC_IMPACT);

        if(!backSec)
            goto hitline;

#if __JDOOM64__
        if(xline->flags & ML_BLOCKALL) // jd64
            goto hitline;
#endif

        // Crosses a two sided line.
        DMU_LineOpening(li);
        dist = map->attackRange * in->frac;

        if(DMU_GetFloatp(frontSec, DMU_FLOOR_HEIGHT) !=
           DMU_GetFloatp(backSec, DMU_FLOOR_HEIGHT))
        {
            slope = (OPENBOTTOM - tracePos[VZ]) / dist;
            if(slope > map->aimSlope)
                goto hitline;
        }

        if(DMU_GetFloatp(frontSec, DMU_CEILING_HEIGHT) !=
           DMU_GetFloatp(backSec, DMU_CEILING_HEIGHT))
        {
            slope = (OPENTOP - tracePos[VZ]) / dist;
            if(slope < map->aimSlope)
                goto hitline;
        }

        // Shot continues...
        return true;

        // Hit a line.
      hitline:

        // Position a bit closer.
        frac = in->frac - (4 / map->attackRange);
        pos[VX] = tracePos[VX] + (FIX2FLT(trace->dX) * frac);
        pos[VY] = tracePos[VY] + (FIX2FLT(trace->dY) * frac);
        pos[VZ] = tracePos[VZ] + (map->aimSlope * (frac * map->attackRange));

        if(backSec)
        {
            // Is it a sky hack wall? If the hitpoint is beyond the visible
            // surface, no puff must be shown.
            if((DMU_GetIntp(DMU_GetPtrp(frontSec, DMU_CEILING_MATERIAL),
                            DMU_FLAGS) & MATF_SKYMASK) &&
               (pos[VZ] > DMU_GetFloatp(frontSec, DMU_CEILING_HEIGHT) ||
                pos[VZ] > DMU_GetFloatp(backSec, DMU_CEILING_HEIGHT)))
                return false;

            if((DMU_GetIntp(DMU_GetPtrp(backSec, DMU_FLOOR_MATERIAL),
                            DMU_FLAGS) & MATF_SKYMASK) &&
               (pos[VZ] < DMU_GetFloatp(frontSec, DMU_FLOOR_HEIGHT) ||
                pos[VZ] < DMU_GetFloatp(backSec, DMU_FLOOR_HEIGHT)))
                return false;
        }

        lineWasHit = true;

        // This is the subsector where the trace originates.
        originSub = Map_PointInSubsector(map, tracePos[VX], tracePos[VY]);

        d[VX] = pos[VX] - tracePos[VX];
        d[VY] = pos[VY] - tracePos[VY];
        d[VZ] = pos[VZ] - tracePos[VZ];

        if(!INRANGE_OF(d[VZ], 0, .0001f)) // Epsilon
        {
            contact = Map_PointInSubsector(map, pos[VX], pos[VY]);
            step = P_ApproxDistance3(d[VX], d[VY], d[VZ]);
            stepv[VX] = d[VX] / step;
            stepv[VY] = d[VY] / step;
            stepv[VZ] = d[VZ] / step;

            cFloor = DMU_GetFloatp(contact, DMU_FLOOR_HEIGHT);
            cCeil = DMU_GetFloatp(contact, DMU_CEILING_HEIGHT);
            // Backtrack until we find a non-empty sector.
            while(cCeil <= cFloor && contact != originSub)
            {
                d[VX] -= 8 * stepv[VX];
                d[VY] -= 8 * stepv[VY];
                d[VZ] -= 8 * stepv[VZ];
                pos[VX] = tracePos[VX] + d[VX];
                pos[VY] = tracePos[VY] + d[VY];
                pos[VZ] = tracePos[VZ] + d[VZ];
                contact = Map_PointInSubsector(map, pos[VX], pos[VY]);
            }

            // Should we backtrack to hit a plane instead?
            cTop = cCeil - 4;
            cBottom = cFloor + 4;
            divisor = 2;

            // We must not hit a sky plane.
            if(pos[VZ] > cTop &&
               (DMU_GetIntp(DMU_GetPtrp(contact, DMU_CEILING_MATERIAL),
                            DMU_FLAGS) & MATF_SKYMASK))
                return false;

            if(pos[VZ] < cBottom &&
               (DMU_GetIntp(DMU_GetPtrp(contact, DMU_FLOOR_MATERIAL),
                            DMU_FLAGS) & MATF_SKYMASK))
                return false;

            // Find the approximate hitpoint by stepping back and
            // forth using smaller and smaller steps.
            while((pos[VZ] > cTop || pos[VZ] < cBottom) && divisor <= 128)
            {
                // We aren't going to hit a line any more.
                lineWasHit = false;

                // Take a step backwards.
                pos[VX] -= d[VX] / divisor;
                pos[VY] -= d[VY] / divisor;
                pos[VZ] -= d[VZ] / divisor;

                // Divisor grows.
                divisor *= 2;

                // Move forward until limits breached.
                while((d[VZ] > 0 && pos[VZ] <= cTop) ||
                      (d[VZ] < 0 && pos[VZ] >= cBottom))
                {
                    pos[VX] += d[VX] / divisor;
                    pos[VY] += d[VY] / divisor;
                    pos[VZ] += d[VZ] / divisor;
                }
            }
        }

        // Spawn bullet puffs.
        P_SpawnPuff(map, pos[VX], pos[VY], pos[VZ], P_Random() << 24);

#if !__JHEXEN__
        if(lineWasHit && xline->special)
        {
            // Extended shoot events only happen when the bullet actually
            // hits the line.
            XL_ShootLine(li, 0, map->shootThing);
        }
/*
if(lineWasHit)
    Con_Message("Hit line [%i,%i]\n", DMU_GetIntp(li, DMU_SIDEDEF0), DMU_GetIntp(li, DMU_SIDEDEF1));
*/
#endif
        // Don't go any farther.
        return false;
    }

    // Shot a mobj.
    th = in->d.mo;
    if(th == map->shootThing)
        return true; // Can't shoot self.

    if(!(th->flags & MF_SHOOTABLE))
        return true; // Corpse or something.

#if __JHERETIC__
    // Check for physical attacks on a ghost.
    if((th->flags & MF_SHADOW) && map->shootThing->player->readyWeapon == WT_FIRST)
        return true;
#endif

    // Check angles to see if the thing can be aimed at
    dist = map->attackRange * in->frac;
    {
    float dz = th->pos[VZ];

    if(!(th->player && (th->player->plr->flags & DDPF_CAMERA)))
        dz += th->height;
    dz -= tracePos[VZ];

    thingTopSlope = dz / dist;
    }

    if(thingTopSlope < map->aimSlope)
        return true; // Shot over the thing.

    thingBottomSlope = (th->pos[VZ] - tracePos[VZ]) / dist;
    if(thingBottomSlope > map->aimSlope)
        return true; // Shot under the thing.

    // Hit thing.

    // Position a bit closer.
    frac = in->frac - (10 / map->attackRange);

    pos[VX] = tracePos[VX] + (FIX2FLT(trace->dX) * frac);
    pos[VY] = tracePos[VY] + (FIX2FLT(trace->dY) * frac);
    pos[VZ] = tracePos[VZ] + (map->aimSlope * (frac * map->attackRange));

    // Spawn bullet puffs or blood spots, depending on target type.
#if __JHERETIC__
    if(map->puffType == MT_BLASTERPUFF1)
    {   // Make blaster big puff.
        mobj_t* mo;

        if((mo = GameMap_SpawnMobj3fv(map, MT_BLASTERPUFF2, pos, P_Random() << 24, 0)))
            S_StartSound(SFX_BLSHIT, mo);
    }
    else
        P_SpawnPuff(map, pos[VX], pos[VY], pos[VZ], P_Random() << 24);
#elif __JHEXEN__
    P_SpawnPuff(map, pos[VX], pos[VY], pos[VZ], P_Random() << 24);
#endif

    if(map->lineAttackDamage)
    {
        int damageDone;
#if __JDOOM__ || __JDOOM64__
        angle_t attackAngle = R_PointToAngle2(map->shootThing->pos[VX],
            map->shootThing->pos[VY], pos[VX], pos[VY]);
#endif

#if __JHEXEN__
        if(map->puffType == MT_FLAMEPUFF2)
        {   // Cleric FlameStrike does fire damage.
            damageDone = P_DamageMobj(th, &map->lavaInflictor, map->shootThing,
                                      map->lineAttackDamage, false);
        }
        else
#endif
        {
            damageDone = P_DamageMobj(th, map->shootThing, map->shootThing,
                                      map->lineAttackDamage, false);
        }

#if __JHEXEN__
        if(!(in->d.mo->flags2 & MF2_INVULNERABLE))
#endif
        {
            if(!(in->d.mo->flags & MF_NOBLOOD))
            {
                if(damageDone > 0)
                {   // Damage was inflicted, so shed some blood.
#if __JDOOM__ || __JDOOM64__
                    P_SpawnBlood(map, pos[VX], pos[VY], pos[VZ],
                        map->lineAttackDamage, attackAngle + ANG180);
#else
# if __JHEXEN__
                    if(map->puffType == MT_AXEPUFF || map->puffType == MT_AXEPUFF_GLOW)
                    {
                        P_SpawnBloodSplatter2(map, pos[VX], pos[VY], pos[VZ], in->d.mo);
                    }
                    else
# endif
                    if(P_Random() < 192)
                        P_SpawnBloodSplatter(map, pos[VX], pos[VY], pos[VZ], in->d.mo);
#endif
                }
            }
#if __JDOOM__ || __JDOOM64__
            else
                P_SpawnPuff(map, pos[VX], pos[VY], pos[VZ], P_Random() << 24);
#endif
        }
    }

    // Don't go any farther.
    return false;
}

/**
 * Sets linetarget and aimSlope when a target is aimed at.
 */
boolean PTR_AimTraverse(intercept_t* in)
{
    map_t* map = P_CurrentMap();
    float slope, thingTopSlope, thingBottomSlope, dist;
    sector_t* backSec, *frontSec;
    mobj_t* th;
    linedef_t* li;

    if(in->type == ICPT_LINE)
    {
        float fFloor, bFloor;
        float fCeil, bCeil;

        li = in->d.lineDef;

        if(!(frontSec = DMU_GetPtrp(li, DMU_FRONT_SECTOR)) ||
           !(backSec  = DMU_GetPtrp(li, DMU_BACK_SECTOR)))
        {
            float tracePos[3];
            divline_t* trace = (divline_t*) DD_GetVariable(DD_TRACE_ADDRESS);

            tracePos[VX] = FIX2FLT(trace->pos[VX]);
            tracePos[VY] = FIX2FLT(trace->pos[VY]);
            tracePos[VZ] = map->shootZ;

            if(DMU_PointOnLineDefSide(tracePos[VX], tracePos[VY], li))
                return true; // Continue traversal.

            return false; // Stop.
        }

        // Crosses a two sided line.
        // A two sided line will restrict the possible target ranges.
        DMU_LineOpening(li);

        if(OPENBOTTOM >= OPENTOP)
            return false; // Stop.

        dist = map->attackRange * in->frac;

        fFloor = DMU_GetFloatp(frontSec, DMU_FLOOR_HEIGHT);
        fCeil = DMU_GetFloatp(frontSec, DMU_CEILING_HEIGHT);

        bFloor = DMU_GetFloatp(backSec, DMU_FLOOR_HEIGHT);
        bCeil = DMU_GetFloatp(backSec, DMU_CEILING_HEIGHT);

        if(fFloor != bFloor)
        {
            slope = (OPENBOTTOM - map->shootZ) / dist;
            if(slope > map->bottomSlope)
                map->bottomSlope = slope;
        }

        if(fCeil != bCeil)
        {
            slope = (OPENTOP - map->shootZ) / dist;
            if(slope < map->topSlope)
                map->topSlope = slope;
        }

        if(map->topSlope <= map->bottomSlope)
            return false; // Stop.

        return true; // Shot continues...
    }

    // Shot a mobj.
    th = in->d.mo;
    if(th == map->shootThing)
        return true; // Can't shoot self.

    if(!(th->flags & MF_SHOOTABLE))
        return true; // Corpse or something?

#if __JHERETIC__
    if(th->type == MT_POD)
        return true; // Can't auto-aim at pods.
#endif

#if __JDOOM__ || __JHEXEN__ || __JDOOM64__
    if(th->player && IS_NETGAME && !deathmatch)
        return true; // Don't aim at fellow co-op players.
#endif

    // Check angles to see if the thing can be aimed at.
    dist = map->attackRange * in->frac;
    {
    float posZ = th->pos[VZ];

    if(!(th->player && (th->player->plr->flags & DDPF_CAMERA)))
        posZ += th->height;

    thingTopSlope = (posZ - map->shootZ) / dist;

    if(thingTopSlope < map->bottomSlope)
        return true; // Shot over the thing.

    // Too far below?
    // $addtocfg $limitautoaimZ:
#if __JHEXEN__
    if(posZ < map->shootZ - map->attackRange / 1.2f)
        return true;
#endif
    }

    thingBottomSlope = (th->pos[VZ] - map->shootZ) / dist;
    if(thingBottomSlope > map->topSlope)
        return true; // Shot under the thing.

    // Too far above?
    // $addtocfg $limitautoaimZ:
#if __JHEXEN__
    if(th->pos[VZ] > map->shootZ + map->attackRange / 1.2f)
        return true;
#endif

    // This thing can be hit!
    if(thingTopSlope > map->topSlope)
        thingTopSlope = map->topSlope;

    if(thingBottomSlope < map->bottomSlope)
        thingBottomSlope = map->bottomSlope;

    map->aimSlope = (thingTopSlope + thingBottomSlope) / 2;
    map->lineTarget = th;

    return false; // Don't go any farther.
}

float P_AimLineAttack(mobj_t* t1, angle_t angle, float distance)
{
    assert(t1);
    {
    map_t* map = Thinker_Map((thinker_t*) t1);
    uint an;
    float pos[2];

    an = angle >> ANGLETOFINESHIFT;
    map->shootThing = t1;

    pos[VX] = t1->pos[VX] + distance * FIX2FLT(finecosine[an]);
    pos[VY] = t1->pos[VY] + distance * FIX2FLT(finesine[an]);

    // Determine the z trace origin.
    map->shootZ = t1->pos[VZ];
#if __JHEXEN__
    if(t1->player &&
      (t1->player->class == PCLASS_FIGHTER ||
       t1->player->class == PCLASS_CLERIC ||
       t1->player->class == PCLASS_MAGE))
#else
    if(t1->player && t1->type == MT_PLAYER)
#endif
    {
        if(!(t1->player->plr->flags & DDPF_CAMERA))
            map->shootZ += (cfg.plrViewHeight - 5);
    }
    else
        map->shootZ += (t1->height / 2) + 8;

#if __JDOOM__ || __JDOOM64__
    map->topSlope = 60;
    map->bottomSlope = -map->topSlope;
#else
    map->topSlope = 100;
    map->bottomSlope = -100;
#endif

    map->attackRange = distance;
    map->lineTarget = NULL;

    Map_PathTraverse(map, t1->pos[VX], t1->pos[VY], pos[VX], pos[VY],
                   PT_ADDLINEDEFS | PT_ADDMOBJS, PTR_AimTraverse);

    if(map->lineTarget)
    {   // While autoaiming, we accept this slope.
        if(!t1->player || !cfg.noAutoAim)
            return map->aimSlope;
    }

    if(t1->player && cfg.noAutoAim)
    {
        // The slope is determined by lookdir.
        return tan(LOOKDIR2RAD(t1->dPlayer->lookDir)) / 1.2;
    }

    return 0;
    }
}

/**
 * If damage == 0, it is just a test trace that will leave lineTarget set.
 */
void P_LineAttack(mobj_t* t1, angle_t angle, float distance, float slope,
                  int damage)
{
    assert(t1);
    {
    map_t* map = Thinker_Map((thinker_t*) t1);
    uint an;
    float targetPos[2];

    an = angle >> ANGLETOFINESHIFT;
    map->shootThing = t1;
    map->lineAttackDamage = damage;

    targetPos[VX] = t1->pos[VX] + distance * FIX2FLT(finecosine[an]);
    targetPos[VY] = t1->pos[VY] + distance * FIX2FLT(finesine[an]);

    // Determine the z trace origin.
    map->shootZ = t1->pos[VZ];
#if __JHEXEN__
    if(t1->player &&
      (t1->player->class == PCLASS_FIGHTER ||
       t1->player->class == PCLASS_CLERIC ||
       t1->player->class == PCLASS_MAGE))
#else
    if(t1->player && t1->type == MT_PLAYER)
#endif
    {
        if(!(t1->player->plr->flags & DDPF_CAMERA))
            map->shootZ += cfg.plrViewHeight - 5;
    }
    else
        map->shootZ += (t1->height / 2) + 8;

    map->shootZ -= t1->floorClip;
    map->attackRange = distance;
    map->aimSlope = slope;

    if(Map_PathTraverse(map, t1->pos[VX], t1->pos[VY], targetPos[VX], targetPos[VY],
                      PT_ADDLINEDEFS | PT_ADDMOBJS, PTR_ShootTraverse))
    {
#if __JHEXEN__
        switch(map->puffType)
        {
        case MT_PUNCHPUFF:
            S_StartSound(SFX_FIGHTER_PUNCH_MISS, t1);
            break;

        case MT_HAMMERPUFF:
        case MT_AXEPUFF:
        case MT_AXEPUFF_GLOW:
            S_StartSound(SFX_FIGHTER_HAMMER_MISS, t1);
            break;

        case MT_FLAMEPUFF:
            P_SpawnPuff(map, targetPos[VX], targetPos[VY],
                        map->shootZ + (slope * distance), P_Random() << 24);
            break;

        default:
            break;
        }
#endif
    }
    }
}

/**
 * "bombSource" is the creature that caused the explosion at "bombSpot".
 */
boolean PIT_RadiusAttack(mobj_t* thing, void* data)
{
    map_t* map = Thinker_Map((thinker_t*) thing);
    float dx, dy, dz, dist;

    if(!(thing->flags & MF_SHOOTABLE))
        return true;

    // Boss spider and cyborg take no damage from concussion.
#if __JHERETIC__
    if(thing->type == MT_MINOTAUR || thing->type == MT_SORCERER1 ||
       thing->type == MT_SORCERER2)
        return true;
#elif __JDOOM__ || __JDOOM64__
    if(thing->type == MT_CYBORG)
        return true;
# if __JDOOM__
    if(thing->type == MT_SPIDER)
        return true;
# endif
#endif

#if __JHEXEN__
    if(!map->damageSource && thing == map->bombSource) // Don't damage the source of the explosion.
        return true;
#endif

    dx = fabs(thing->pos[VX] - map->bombSpot->pos[VX]);
    dy = fabs(thing->pos[VY] - map->bombSpot->pos[VY]);
    dz = fabs((thing->pos[VZ] + thing->height / 2) - map->bombSpot->pos[VZ]);

    dist = (dx > dy? dx : dy);

#if __JHEXEN__
    if(!cfg.netNoMaxZRadiusAttack)
        dist = (dz > dist? dz : dist);
#else
    if(!(cfg.netNoMaxZRadiusAttack || (thing->info->flags2 & MF2_INFZBOMBDAMAGE)))
        dist = (dz > dist? dz : dist);
#endif

    dist = (dist - thing->radius);

    if(dist < 0)
        dist = 0;

    if(dist >= map->bombDistance)
        return true; // Out of range.

    // Must be in direct path.
    if(P_CheckSight(thing, map->bombSpot))
    {
        int damage = (map->bombDamage * (map->bombDistance - dist) / map->bombDistance) + 1;

#if __JHEXEN__
        if(thing->player)
            damage /= 4;
#endif
        P_DamageMobj(thing, map->bombSpot, map->bombSource, damage, false);
    }

    return true;
}

/**
 * Source is the creature that caused the explosion at spot.
 */
#if __JHEXEN__
void P_RadiusAttack(mobj_t* spot, mobj_t* source, int damage, int distance,
                    boolean canDamageSource)
#else
void P_RadiusAttack(mobj_t* spot, mobj_t* source, int damage, int distance)
#endif
{
    map_t* map = Thinker_Map((thinker_t*) spot);
    float dist, box[4];

    dist = distance + MAXRADIUS;

    box[BOXLEFT]   = spot->pos[VX] - dist;
    box[BOXRIGHT]  = spot->pos[VX] + dist;
    box[BOXBOTTOM] = spot->pos[VY] - dist;
    box[BOXTOP]    = spot->pos[VY] + dist;

    map->bombSpot = spot;
    map->bombDamage = damage;
    map->bombDistance = distance;

#if __JHERETIC__
    if(spot->type == MT_POD && spot->target)
        map->bombSource = spot->target;
    else
#endif
        map->bombSource = source;

#if __JHEXEN__
    map->damageSource = canDamageSource;
#endif
    VALIDCOUNT++;
    Map_MobjsBoxIterator(map, box, PIT_RadiusAttack, 0);
}

boolean PTR_UseTraverse(intercept_t* in)
{
    map_t* map = P_CurrentMap();
    xlinedef_t* xline;
    int side;

    if(in->type != ICPT_LINE)
        return true; // Continue iteration.

    xline = P_ToXLine(in->d.lineDef);

    if(!xline->special)
    {
        DMU_LineOpening(in->d.lineDef);
        if(OPENRANGE <= 0)
        {
            if(map->useThing->player)
                S_StartSound(PCLASS_INFO(map->useThing->player->class)->failUseSound, map->useThing);

            return false; // Can't use through a wall.
        }

#if __JHEXEN__
        if(map->useThing->player)
        {
            float pheight = map->useThing->pos[VZ] + map->useThing->height/2;

            if((OPENTOP < pheight) || (OPENBOTTOM > pheight))
                S_StartSound(PCLASS_INFO(map->useThing->player->class)->failUseSound, map->useThing);
        }
#endif
        // Not a special line, but keep checking.
        return true;
    }

    side = 0;
    if(1 == DMU_PointOnLineDefSide(map->useThing->pos[VX], map->useThing->pos[VY], in->d.lineDef))
        side = 1;

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    if(side == 1)
        return false;       // don't use back side
#endif

    P_ActivateLine(in->d.lineDef, map->useThing, side, SPAC_USE);

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    // Can use multiple line specials in a row with the PassThru flag.
    if(xline->flags & ML_PASSUSE)
        return true;
#endif
    // Can't use more than one special line in a row.
    return false;
}

/**
 * Looks for special lines in front of the player to activate.
 *
 * @param player        The player to test.
 */
void P_UseLines(player_t* player)
{
    assert(player);
    {
    map_t* map = Thinker_Map((thinker_t*) player->plr->mo);
    uint an;
    float pos[3];
    mobj_t* mo;

    if(IS_CLIENT)
    {
#ifdef _DEBUG
        Con_Message("P_UseLines: Sending a use request for player %i.\n",
                    player - players);
#endif
        NetCl_PlayerActionRequest(player, GPA_USE);
        return;
    }

    map->useThing = mo = player->plr->mo;

    an = mo->angle >> ANGLETOFINESHIFT;

    pos[VX] = mo->pos[VX];
    pos[VY] = mo->pos[VY];
    pos[VZ] = mo->pos[VZ];

    pos[VX] += USERANGE * FIX2FLT(finecosine[an]);
    pos[VY] += USERANGE * FIX2FLT(finesine[an]);

    Map_PathTraverse(map, mo->pos[VX], mo->pos[VY], pos[VX], pos[VY],
                   PT_ADDLINEDEFS, PTR_UseTraverse);
    }
}

/**
 * Takes a valid thing and adjusts the thing->floorZ, thing->ceilingZ,
 * and possibly thing->pos[VZ].
 *
 * This is called for all nearby monsters whenever a sector changes height
 * If the thing doesn't fit, the z will be set to the lowest value and
 * false will be returned
 *
 * @param thing         The mobj whoose position to adjust.
 * @return              @c true, if the thing did fit.
 */
static boolean P_ThingHeightClip(mobj_t* thing)
{
    map_t* map = Thinker_Map((thinker_t*) thing);
    boolean onfloor;

    if(P_MobjIsCamera(thing))
        return false; // Don't height clip cameras.

    onfloor = (thing->pos[VZ] == thing->floorZ)? true : false;
    P_CheckPosition3fv(thing, thing->pos);

    thing->floorZ = map->tmFloorZ;
    thing->ceilingZ = map->tmCeilingZ;
#if !__JHEXEN__
    thing->dropOffZ = map->tmDropoffZ; // $dropoff_fix: remember dropoffs.
#endif

    if(onfloor)
    {
#if __JHEXEN__
        if((thing->pos[VZ] - thing->floorZ < 9) ||
           (thing->flags & MF_NOGRAVITY))
        {
            thing->pos[VZ] = thing->floorZ;
        }
#else
        // Walking monsters rise and fall with the floor.
        thing->pos[VZ] = thing->floorZ;

        // $dropoff_fix: Possibly upset balance of objects hanging off ledges.
        if((thing->intFlags & MIF_FALLING) && thing->gear >= MAXGEAR)
            thing->gear = 0;
#endif
    }
    else
    {
        // Don't adjust a floating monster unless forced to.
        if(thing->pos[VZ] + thing->height > thing->ceilingZ)
            thing->pos[VZ] = thing->ceilingZ - thing->height;
    }

    if(thing->ceilingZ - thing->floorZ >= thing->height)
        return true;

    return false;
}

/**
 * Allows the player to slide along any angled walls by adjusting the
 * xmove / ymove so that the NEXT move will slide along the wall.
 *
 * @param ld            The line being slid along.
 */
static void P_HitSlideLine(linedef_t* ld)
{
    map_t* map = P_CurrentMap();
    int side;
    unsigned int an;
    angle_t lineAngle, moveAngle, deltaAngle;
    float moveLen, newLen, d1[2];
    slopetype_t slopeType = DMU_GetIntp(ld, DMU_SLOPE_TYPE);

    if(slopeType == ST_HORIZONTAL)
    {
        map->tmMove[MY] = 0;
        return;
    }
    else if(slopeType == ST_VERTICAL)
    {
        map->tmMove[MX] = 0;
        return;
    }

    side = DMU_PointOnLineDefSide(map->slideMo->pos[VX], map->slideMo->pos[VY], ld);
    DMU_GetFloatpv(ld, DMU_DXY, d1);
    lineAngle = R_PointToAngle2(0, 0, d1[0], d1[1]);
    moveAngle = R_PointToAngle2(0, 0, map->tmMove[MX], map->tmMove[MY]) + 10;

    if(side == 1)
        lineAngle += ANG180;
    deltaAngle = moveAngle - lineAngle;
    if(deltaAngle > ANG180)
        deltaAngle += ANG180;

    moveLen = P_ApproxDistance(map->tmMove[MX], map->tmMove[MY]);
    an = deltaAngle >> ANGLETOFINESHIFT;
    newLen = moveLen * FIX2FLT(finecosine[an]);

    an = lineAngle >> ANGLETOFINESHIFT;
    map->tmMove[MX] = newLen * FIX2FLT(finecosine[an]);
    map->tmMove[MY] = newLen * FIX2FLT(finesine[an]);
}

boolean PTR_SlideTraverse(intercept_t* in)
{
    map_t* map = P_CurrentMap();
    linedef_t* li;

    if(in->type != ICPT_LINE)
        Con_Error("PTR_SlideTraverse: Not a line?");

    li = in->d.lineDef;

    if(!DMU_GetPtrp(li, DMU_FRONT_SECTOR) || !DMU_GetPtrp(li, DMU_BACK_SECTOR))
    {
        if(DMU_PointOnLineDefSide(map->slideMo->pos[VX], map->slideMo->pos[VY], li))
            return true; // Don't hit the back side.

        goto isblocking;
    }

#if __JDOOM64__
    if(P_ToXLine(li)->flags & ML_BLOCKALL) // jd64
        goto isblocking;
#endif

    DMU_LineOpening(li); // Set OPENRANGE, OPENTOP, OPENBOTTOM...

    if(OPENRANGE < map->slideMo->height)
        goto isblocking; // Doesn't fit.

    if(OPENTOP - map->slideMo->pos[VZ] < map->slideMo->height)
        goto isblocking; // mobj is too high.

    if(OPENBOTTOM - map->slideMo->pos[VZ] > 24)
        goto isblocking; // Too big a step up.

    // This line doesn't block movement.
    return true;

    // The line does block movement, see if it is closer than best so far.
  isblocking:
    if(in->frac < map->bestSlideFrac)
    {
        map->secondSlideFrac = map->bestSlideFrac;
        map->secondSlideLine = map->bestSlideLine;
        map->bestSlideFrac = in->frac;
        map->bestSlideLine = li;
    }

    return false; // Stop.
}

/**
 * @fixme The momx / momy move is bad, so try to slide along a wall.
 * Find the first line hit, move flush to it, and slide along it
 *
 * This is a kludgy mess.
 *
 * @param mo            The mobj to attempt the slide move.
 */
void P_SlideMove(mobj_t* mo)
{
    assert(mo);
    {
    map_t* map = Thinker_Map((thinker_t*) mo);
    int hitcount = 3;

    map->slideMo = mo;

    do
    {
        float leadpos[3], trailpos[3], newPos[3];

        if(!--hitcount == 3)
            goto stairstep; // Don't loop forever.

        // Trace along the three leading corners.
        leadpos[VX] = trailpos[VX] = mo->pos[VX];
        leadpos[VY] = trailpos[VY] = mo->pos[VY];
        leadpos[VZ] = trailpos[VZ] = mo->pos[VZ];

        if(mo->mom[MX] > 0)
        {
            leadpos[VX] += mo->radius;
            trailpos[VX] -= mo->radius;
        }
        else
        {
            leadpos[VX] -= mo->radius;
            trailpos[VX] += mo->radius;
        }

        if(mo->mom[MY] > 0)
        {
            leadpos[VY] += mo->radius;
            trailpos[VY] -= mo->radius;
        }
        else
        {
            leadpos[VY] -= mo->radius;
            trailpos[VY] += mo->radius;
        }

        map->bestSlideFrac = 1;

        Map_PathTraverse(map, leadpos[VX], leadpos[VY],
                       leadpos[VX] + mo->mom[MX], leadpos[VY] + mo->mom[MY],
                       PT_ADDLINEDEFS, PTR_SlideTraverse);
        Map_PathTraverse(map, trailpos[VX], leadpos[VY],
                       trailpos[VX] + mo->mom[MX], leadpos[VY] + mo->mom[MY],
                       PT_ADDLINEDEFS, PTR_SlideTraverse);
        Map_PathTraverse(map, leadpos[VX], trailpos[VY],
                       leadpos[VX] + mo->mom[MX], trailpos[VY] + mo->mom[MY],
                       PT_ADDLINEDEFS, PTR_SlideTraverse);

        // Move up to the wall.
        if(map->bestSlideFrac == 1)
        {
            // The move must have hit the middle, so stairstep.
          stairstep:
            // $dropoff_fix
#if __JHEXEN__
            if(!P_TryMove(mo, mo->pos[VX], mo->pos[VY] + mo->mom[MY]))
            {
                if(P_TryMove(mo, mo->pos[VX] + mo->mom[MX], mo->pos[VY]))
                {
                    // If not set to zero, the mobj will appear stuttering against
                    // the blocking surface/thing.
                    mo->mom[MY] = 0;
                }
                else
                {
                    // If not set to zero, the mobj will appear stuttering against
                    // the blocking surface/thing.
                    mo->mom[MX] = mo->mom[MY] = 0;
                }
            }
#else
            if(!P_TryMove(mo, mo->pos[VX], mo->pos[VY] + mo->mom[MY], true, true))
            {
                if(P_TryMove(mo, mo->pos[VX] + mo->mom[MX], mo->pos[VY], true, true))
                {
                    // If not set to zero, the mobj will appear stuttering against
                    // the blocking surface/thing.
                    mo->mom[MY] = 0;
                }
                else
                {
                    // If not set to zero, the mobj will appear stuttering against
                    // the blocking surface/thing.
                    mo->mom[MX] = mo->mom[MY] = 0;
                }
            }
#endif
            else
            {
                // If not set to zero, the mobj will appear stuttering against
                // the blocking surface/thing.
                mo->mom[MX] = 0;
            }

            break;
        }

        // Fudge a bit to make sure it doesn't hit.
        map->bestSlideFrac -= (1.0f / 32);
        if(map->bestSlideFrac > 0)
        {
            newPos[VX] = mo->mom[MX] * map->bestSlideFrac;
            newPos[VY] = mo->mom[MY] * map->bestSlideFrac;
            newPos[VZ] = DDMAXFLOAT; // Just initialize with *something*.

            // $dropoff_fix: Allow objects to drop off ledges
#if __JHEXEN__
            if(!P_TryMove(mo, mo->pos[VX] + newPos[VX], mo->pos[VY] + newPos[VY]))
                goto stairstep;
#else
            if(!P_TryMove(mo, mo->pos[VX] + newPos[VX], mo->pos[VY] + newPos[VY],
                          true, true))
                goto stairstep;
#endif
        }

        // Now continue along the wall.
        // First calculate remainder.
        map->bestSlideFrac = 1 - (map->bestSlideFrac + (1.0f / 32));
        if(map->bestSlideFrac > 1)
            map->bestSlideFrac = 1;
        if(map->bestSlideFrac <= 0)
            break;

        map->tmMove[MX] = mo->mom[MX] * map->bestSlideFrac;
        map->tmMove[MY] = mo->mom[MY] * map->bestSlideFrac;

        P_HitSlideLine(map->bestSlideLine); // Clip the move.

        mo->mom[MX] = map->tmMove[MX];
        mo->mom[MY] = map->tmMove[MY];

    // $dropoff_fix: Allow objects to drop off ledges:
#if __JHEXEN__
    } while(!P_TryMove(mo, mo->pos[VX] + map->tmMove[MX], mo->pos[VY] + map->tmMove[MY]));
#else
    } while(!P_TryMove(mo, mo->pos[VX] + map->tmMove[MX], mo->pos[VY] + map->tmMove[MY], true, true));
#endif
    }
}

/**
 * SECTOR HEIGHT CHANGING
 * After modifying a sectors floor or ceiling height, call this routine
 * to adjust the positions of all things that touch the sector.
 *
 * If anything doesn't fit anymore, true will be returned.
 * If crunch is true, they will take damage as they are being crushed.
 * If Crunch is false, you should set the sector height back the way it
 * was and call P_ChangeSector again to undo the changes.
 */

/**
 * @param thing         The thing to check against height changes.
 * @param data          Unused.
 */
int PIT_ChangeSector(void* ptr, void* data)
{
    mobj_t* thing = (mobj_t*) ptr;
    map_t* map = Thinker_Map((thinker_t*) thing);
    mobj_t* mo;

    if(P_ThingHeightClip(thing))
        return true; // Keep checking...

    // Crunch bodies to giblets.
#if __JDOOM__ || __JDOOM64__
    if(thing->health <= 0 && !(thing->flags & MF_NOBLOOD))
#elif __JHEXEN__
    if(thing->health <= 0 && (thing->flags & MF_CORPSE))
#else
    if(thing->health <= 0)
#endif
    {
#if __JHEXEN__
        if(thing->flags & MF_NOBLOOD)
        {
            P_MobjRemove(thing, false);
        }
        else
        {
            if(thing->state != &STATES[S_GIBS1])
            {
                P_MobjChangeState(thing, S_GIBS1);
                thing->height = 0;
                thing->radius = 0;
                S_StartSound(SFX_PLAYER_FALLING_SPLAT, thing);
            }
        }
#else
# if __JDOOM64__
        S_StartSound(SFX_SLOP, thing);
# endif

# if __JDOOM__ || __JDOOM64__
        P_MobjChangeState(thing, S_GIBS);
# endif
        thing->flags &= ~MF_SOLID;
        thing->height = 0;
        thing->radius = 0;
#endif
        return true; // Keep checking...
    }

    // Crunch dropped items.
#if __JHEXEN__
    if(thing->flags2 & MF2_DROPPED)
#else
    if(thing->flags & MF_DROPPED)
#endif
    {
        P_MobjRemove(thing, false);
        return true; // Keep checking...
    }

    if(!(thing->flags & MF_SHOOTABLE))
        return true; // Keep checking...

    map->noFit = true;
    if(map->crushChange > 0 && !(map->time & 3))
    {
        P_DamageMobj(thing, NULL, NULL, 10, false);
#if __JDOOM__ || __JDOOM64__
        if(!(thing->flags & MF_NOBLOOD))
#elif __JHEXEN__
        if(!(thing->flags & MF_NOBLOOD) &&
           !(thing->flags2 & MF2_INVULNERABLE))
#endif
        {
            // Spray blood in a random direction.
            if((mo = GameMap_SpawnMobj3f(map, MT_BLOOD, thing->pos[VX], thing->pos[VY],
                                   thing->pos[VZ] + (thing->height /2),
                                   P_Random() << 24, 0)))
            {
                mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 12);
                mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 12);
            }
        }
    }

    return true; // Keep checking (crush other things)...
}

/**
 * @param sector        The sector to check.
 * @param crunch        @c true = crush any things in the sector.
 */
boolean P_ChangeSector(sector_t* sector, boolean crunch)
{
    assert(sector);
    {
    map_t* map = P_CurrentMap();

    map->noFit = false;
    map->crushChange = crunch;

    VALIDCOUNT++;
    DMU_Iteratep(sector, DMU_MOBJS, PIT_ChangeSector, 0);

    return map->noFit;
    }
}

/**
 * The following routines originate from the Heretic src!
 */

#if __JHERETIC__ || __JHEXEN__
/**
 * @param mobj          The mobj whoose position to test.
 * @return boolean      @c true, if the mobj is not blocked by anything.
 */
boolean P_TestMobjLocation(mobj_t* mobj)
{
    int                 flags;

    flags = mobj->flags;
    mobj->flags &= ~MF_PICKUP;

    if(P_CheckPosition2f(mobj, mobj->pos[VX], mobj->pos[VY]))
    {
        // XY is ok, now check Z
        mobj->flags = flags;
        if((mobj->pos[VZ] < mobj->floorZ) ||
           (mobj->pos[VZ] + mobj->height > mobj->ceilingZ))
        {
            return false; // Bad Z
        }

        return true;
    }

    mobj->flags = flags;
    return false;
}
#endif

#if __JDOOM64__ || __JHERETIC__
static void CheckMissileImpact(mobj_t* mobj)
{
    map_t* map = Thinker_Map((thinker_t*) mobj);
    int size;
    linedef_t* ld;

    if(!mobj->target || !mobj->target->player ||
       !(mobj->flags & MF_MISSILE))
        return;

    if(!(size = P_IterListSize(GameMap_SpecHits(map))))
        return;

    P_IterListResetIterator(GameMap_SpecHits(map), false);
    while((ld = P_IterListIterator(GameMap_SpecHits(map))) != NULL)
        P_ActivateLine(ld, mobj->target, 0, SPAC_IMPACT);
}
#endif

#if __JHEXEN__
boolean PIT_ThrustStompThing(mobj_t* thing, void* data)
{
    map_t* map = Thinker_Map((thinker_t*) thing);
    float blockdist;

    if(!(thing->flags & MF_SHOOTABLE))
        return true;
    if(thing == map->tsThing)
        return true; // Don't clip against self.

    blockdist = thing->radius + map->tsThing->radius;
    if(fabs(thing->pos[VX] - map->tsThing->pos[VX]) >= blockdist ||
       fabs(thing->pos[VY] - map->tsThing->pos[VY]) >= blockdist ||
       (thing->pos[VZ] > map->tsThing->pos[VZ] + map->tsThing->height))
        return true; // Didn't hit it.

    P_DamageMobj(thing, map->tsThing, map->tsThing, 10001, false);
    map->tsThing->args[1] = 1; // Mark thrust thing as bloody.

    return true;
}

void PIT_ThrustSpike(mobj_t* actor)
{
    map_t* map = Thinker_Map((thinker_t*) actor);
    float bbox[4], radius;

    map->tsThing = actor;
    radius = actor->info->radius + MAXRADIUS;

    bbox[BOXLEFT]   = bbox[BOXRIGHT] = actor->pos[VX];
    bbox[BOXBOTTOM] = bbox[BOXTOP]   = actor->pos[VY];

    bbox[BOXLEFT]   -= radius;
    bbox[BOXRIGHT]  += radius;
    bbox[BOXBOTTOM] -= radius;
    bbox[BOXTOP]    += radius;

    // Stomp on any things contacted.
    VALIDCOUNT++;
    Map_MobjsBoxIterator(map, bbox, PIT_ThrustStompThing, 0);
}

boolean PIT_CheckOnmobjZ(mobj_t* thing, void* data)
{
    map_t* map = Thinker_Map((thinker_t*) thing);
    float blockdist;

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)))
        return true; // Can't hit thing.

    if(thing == map->tmThing)
        return true; // Don't clip against self.

    blockdist = thing->radius + map->tmThing->radius;
    if(fabs(thing->pos[VX] - map->tm[VX]) >= blockdist ||
       fabs(thing->pos[VY] - map->tm[VY]) >= blockdist)
        return true; // Didn't hit thing.

    if(map->tmThing->pos[VZ] > thing->pos[VZ] + thing->height)
        return true; // Over thing.
    else if(map->tmThing->pos[VZ] + map->tmThing->height < thing->pos[VZ])
        return true; // Under thing.

    if(thing->flags & MF_SOLID)
        map->onMobj = thing;

    return !(thing->flags & MF_SOLID);
}

mobj_t* P_CheckOnMobj(mobj_t* thing)
{
    assert(thing);
    {
    map_t* map = Thinker_Map((thinker_t*) thing);
    subsector_t* newSSec;
    float pos[3], box[4];
    mobj_t oldMo;

    pos[VX] = thing->pos[VX];
    pos[VY] = thing->pos[VY];
    pos[VZ] = thing->pos[VZ];

    map->tmThing = thing;

    // @fixme Do this properly!
    memcpy(&oldMo, thing, sizeof(oldMo)); // Save the old mobj before the fake z movement.

    P_FakeZMovement(map->tmThing);

    map->tm[VX] = pos[VX];
    map->tm[VY] = pos[VY];
    map->tm[VZ] = pos[VZ];

    map->tmBBox[BOXTOP]    = pos[VY] + map->tmThing->radius;
    map->tmBBox[BOXBOTTOM] = pos[VY] - map->tmThing->radius;
    map->tmBBox[BOXRIGHT]  = pos[VX] + map->tmThing->radius;
    map->tmBBox[BOXLEFT]   = pos[VX] - map->tmThing->radius;

    newSSec = Map_PointInSubsector(map, pos[VX], pos[VY]);
    map->ceilingLine = map->floorLine = NULL;

    // The base floor/ceiling is from the subsector that contains the
    // point. Any contacted lines the step closer together will adjust them.

    map->tmFloorZ = map->tmDropoffZ = DMU_GetFloatp(newSSec, DMU_FLOOR_HEIGHT);
    map->tmCeilingZ = DMU_GetFloatp(newSSec, DMU_CEILING_HEIGHT);
    map->tmFloorMaterial = DMU_GetPtrp(newSSec, DMU_FLOOR_MATERIAL);

    P_EmptyIterList(GameMap_SpecHits(map));

    if(map->tmThing->flags & MF_NOCLIP)
        return NULL;

    // Check things first, possibly picking things up the bounding box is
    // extended by MAXRADIUS because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap into adjacent blocks by
    // up to MAXRADIUS.

    box[BOXLEFT]   = map->tmBBox[BOXLEFT]   - MAXRADIUS;
    box[BOXRIGHT]  = map->tmBBox[BOXRIGHT]  + MAXRADIUS;
    box[BOXBOTTOM] = map->tmBBox[BOXBOTTOM] - MAXRADIUS;
    box[BOXTOP]    = map->tmBBox[BOXTOP]    + MAXRADIUS;

    VALIDCOUNT++;
    if(!Map_MobjsBoxIterator(map, box, PIT_CheckOnmobjZ, 0))
    {
        memcpy(map->tmThing, &oldMo, sizeof(mobj_t));
        return map->onMobj;
    }

    memcpy(map->tmThing, &oldMo, sizeof(mobj_t));

    return NULL;
    }
}

/**
 * Fake the zmovement so that we can check if a move is legal.
 */
static void P_FakeZMovement(mobj_t* mo)
{
    map_t* map = Thinker_Map((thinker_t*) mo);
    float dist, delta;

    if(P_MobjIsCamera(mo))
        return;

    // Adjust height.
    mo->pos[VZ] += mo->mom[MZ];
    if((mo->flags & MF_FLOAT) && mo->target)
    {   // Float down towards target if too close.
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist = P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                    mo->pos[VY] - mo->target->pos[VY]);

            delta = mo->target->pos[VZ] + (mo->height / 2) - mo->pos[VZ];

            if(delta < 0 && dist < -(delta * 3))
                mo->pos[VZ] -= FLOATSPEED;
            else if(delta > 0 && dist < (delta * 3))
                mo->pos[VZ] += FLOATSPEED;
        }
    }

    if(mo->player && (mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) &&
       (map->time & 2))
    {
        mo->pos[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * map->time >> 2) & FINEMASK]);
    }

    // Clip movement.
    if(mo->pos[VZ] <= mo->floorZ) // Hit the floor.
    {
        mo->pos[VZ] = mo->floorZ;
        if(mo->mom[MZ] < 0)
            mo->mom[MZ] = 0;

        if(mo->flags & MF_SKULLFLY)
            mo->mom[MZ] = -mo->mom[MZ]; // The skull slammed into something

        if(P_GetState(mo->type, SN_CRASH) && (mo->flags & MF_CORPSE))
            return;
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -(GameMap_Gravity(Thinker_Map((thinker_t*) mo)) / 32) * 2;
        else
            mo->mom[MZ] -= GameMap_Gravity(Thinker_Map((thinker_t*) mo)) / 32;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -GameMap_Gravity(Thinker_Map((thinker_t*) mo)) * 2;
        else
            mo->mom[MZ] -= GameMap_Gravity(Thinker_Map((thinker_t*) mo));
    }

    if(mo->pos[VZ] + mo->height > mo->ceilingZ) // Hit the ceiling.
    {
        mo->pos[VZ] = mo->ceilingZ - mo->height;

        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        if(mo->flags & MF_SKULLFLY)
            mo->mom[MZ] = -mo->mom[MZ]; // The skull slammed into something.
    }
}

static void checkForPushSpecial(linedef_t* line, int side, mobj_t* mobj)
{
    if(P_ToXLine(line)->special)
    {
        if(mobj->flags2 & MF2_PUSHWALL)
        {
            P_ActivateLine(line, mobj, side, SPAC_PUSH);
        }
        else if(mobj->flags2 & MF2_IMPACT)
        {
            P_ActivateLine(line, mobj, side, SPAC_IMPACT);
        }
    }
}

boolean PTR_BounceTraverse(intercept_t* in)
{
    map_t* map = P_CurrentMap();
    linedef_t* li;

    if(in->type != ICPT_LINE)
        Con_Error("PTR_BounceTraverse: Not a line?");

    li = in->d.lineDef;

    if(!DMU_GetPtrp(li, DMU_FRONT_SECTOR) || !DMU_GetPtrp(li, DMU_BACK_SECTOR))
    {
        if(DMU_PointOnLineDefSide(map->slideMo->pos[VX], map->slideMo->pos[VY], li))
            return true; // Don't hit the back side.

        goto bounceblocking;
    }

    DMU_LineOpening(li); // Set OPENRANGE, OPENTOP, OPENBOTTOM...

    if(OPENRANGE < map->slideMo->height)
        goto bounceblocking; // Doesn't fit.

    if(OPENTOP - map->slideMo->pos[VZ] < map->slideMo->height)
        goto bounceblocking; // Mobj is too high...

    return true; // This line doesn't block movement...

    // the line does block movement, see if it is closer than best so far.
  bounceblocking:
    if(in->frac < map->bestSlideFrac)
    {
        map->secondSlideFrac = map->bestSlideFrac;
        map->secondSlideLine = map->bestSlideLine;
        map->bestSlideFrac = in->frac;
        map->bestSlideLine = li;
    }

    return false; // Stop.
}

void P_BounceWall(mobj_t* mo)
{
    assert(mo);
    {
    map_t* map = Thinker_Map((thinker_t*) mo);
    int side;
    unsigned int an;
    float moveLen, leadPos[3], d1[2];
    angle_t lineAngle, moveAngle, deltaAngle;

    map->slideMo = mo;

    // Trace along the three leading corners.
    leadPos[VX] = mo->pos[VX];
    leadPos[VY] = mo->pos[VY];
    leadPos[VZ] = mo->pos[VZ];

    if(mo->mom[MX] > 0)
        leadPos[VX] += mo->radius;
    else
        leadPos[VX] -= mo->radius;

    if(mo->mom[MY] > 0)
        leadPos[VY] += mo->radius;
    else
        leadPos[VY] -= mo->radius;

    map->bestSlideFrac = 1;
    Map_PathTraverse(map, leadPos[VX], leadPos[VY],
                   leadPos[VX] + mo->mom[MX], leadPos[VY] + mo->mom[MY],
                   PT_ADDLINEDEFS, PTR_BounceTraverse);

    if(!map->bestSlideLine)
        return;

    side = DMU_PointOnLineDefSide(mo->pos[VX], mo->pos[VY], map->bestSlideLine);
    DMU_GetFloatpv(map->bestSlideLine, DMU_DXY, d1);
    lineAngle = R_PointToAngle2(0, 0, d1[0], d1[1]);
    if(side == 1)
        lineAngle += ANG180;

    moveAngle = R_PointToAngle2(0, 0, mo->mom[MX], mo->mom[MY]);
    deltaAngle = (2 * lineAngle) - moveAngle;

    moveLen = P_ApproxDistance(mo->mom[MX], mo->mom[MY]);
    moveLen *= 0.75f; // Friction.

    if(moveLen < 1)
        moveLen = 2;

    an = deltaAngle >> ANGLETOFINESHIFT;
    mo->mom[MX] = moveLen * FIX2FLT(finecosine[an]);
    mo->mom[MY] = moveLen * FIX2FLT(finesine[an]);
    }
}

boolean PTR_PuzzleItemTraverse(intercept_t* in)
{
    switch(in->type)
    {
    case ICPT_LINE: // Linedef.
        {
        map_t* map = P_CurrentMap();
        linedef_t* line = in->d.lineDef;
        xlinedef_t* xline = P_ToXLine(line);

        if(xline->special != USE_PUZZLE_ITEM_SPECIAL)
        {
            DMU_LineOpening(line);

            if(OPENRANGE <= 0)
            {
                sfxenum_t sound = SFX_NONE;

                if(map->puzzleItemUser->player)
                {
                    switch(map->puzzleItemUser->player->class)
                    {
                    case PCLASS_FIGHTER:
                        sound = SFX_PUZZLE_FAIL_FIGHTER;
                        break;

                    case PCLASS_CLERIC:
                        sound = SFX_PUZZLE_FAIL_CLERIC;
                        break;

                    case PCLASS_MAGE:
                        sound = SFX_PUZZLE_FAIL_MAGE;
                        break;

                    default:
                        sound = SFX_NONE;
                        break;
                    }
                }

                S_StartSound(sound, map->puzzleItemUser);
                return false; // Can't use through a wall.
            }

            return true; // Continue searching...
        }

        if(DMU_PointOnLineDefSide(map->puzzleItemUser->pos[VX],
                                  map->puzzleItemUser->pos[VY], line) == 1)
            return false; // Don't use back sides.

        if(map->puzzleItemType != xline->arg1)
            return false; // Item type doesn't match.

        ActionScriptInterpreter_Start(ActionScriptInterpreter, (actionscriptid_t) xline->arg2, 0, &xline->arg3, map->puzzleItemUser, line, 0);
        xline->special = 0;
        map->puzzleActivated = true;

        return false; // Stop searching.
        }

    case ICPT_MOBJ: // Mobj.
        {
        mobj_t* mo = in->d.mo;
        map_t* map = Thinker_Map((thinker_t*) mo);

        if(mo->special != USE_PUZZLE_ITEM_SPECIAL)
            return true; // Wrong special...

        if(map->puzzleItemType != mo->args[0])
            return true; // Item type doesn't match...

        ActionScriptInterpreter_Start(ActionScriptInterpreter, (actionscriptid_t) mo->args[1], 0, &mo->args[2], map->puzzleItemUser, NULL, 0);
        mo->special = 0;
        map->puzzleActivated = true;

        return false; // Stop searching.
        }
    default:
        Con_Error("PTR_PuzzleItemTraverse: Unknown intercept type %i.", in->type);
    }

    // Unreachable.
    return false;
}

/**
 * See if the specified player can use the specified puzzle item on a
 * thing or line(s) at their current world location.
 *
 * @param player        The player using the puzzle item.
 * @param itemType      The type of item to try to use.
 * @return boolean      true if the puzzle item was used.
 */
boolean P_UsePuzzleItem(player_t* player, int itemType)
{
    assert(player);
    {
    map_t* map = Thinker_Map((thinker_t*) player->plr->mo);
    int angle;
    float pos1[3], pos2[3];

    map->puzzleItemType = itemType;
    map->puzzleItemUser = player->plr->mo;
    map->puzzleActivated = false;

    angle = player->plr->mo->angle >> ANGLETOFINESHIFT;

    memcpy(pos1, player->plr->mo->pos, sizeof(pos1));
    memcpy(pos2, player->plr->mo->pos, sizeof(pos2));

    pos2[VX] += FIX2FLT(USERANGE * finecosine[angle]);
    pos2[VY] += FIX2FLT(USERANGE * finesine[angle]);

    Map_PathTraverse(map, pos1[VX], pos1[VY], pos2[VX], pos2[VY],
                   PT_ADDLINEDEFS | PT_ADDMOBJS, PTR_PuzzleItemTraverse);

    if(!map->puzzleActivated)
    {
        P_SetYellowMessage(player, TXT_USEPUZZLEFAILED, false);
    }

    return map->puzzleActivated;
    }
}
#endif
