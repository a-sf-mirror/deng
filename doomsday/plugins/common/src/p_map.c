/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
 * p_map.c: Common map routines.
 *
 * Compiles for jDoom/jHeretic/jHexen
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "d_net.h"
#include "g_common.h"
#include "dmu_lib.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

#if __JHEXEN__
#define USE_PUZZLE_ITEM_SPECIAL     129
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

#if __DOOM64TC__
static void CheckMissileImpact(mobj_t *mobj);
#endif

#if __JHERETIC__
static void  CheckMissileImpact(mobj_t *mobj);
#elif __JHEXEN__
static void  P_FakeZMovement(mobj_t *mo);
static void  CheckForPushSpecial(line_t *line, int side, mobj_t *mobj);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float tmbbox[4];
mobj_t *tmthing;

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
boolean floatok;

float tmfloorz;
float tmceilingz;
int tmfloorpic;

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

mobj_t *linetarget; // who got hit (or NULL)

float attackrange;

#if __JHEXEN__
mobj_t *PuffSpawned;
mobj_t *BlockingMobj;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static float tm[3];
static float tmheight;
static line_t *tmhitline;
static float tmdropoffz;
static float bestslidefrac, secondslidefrac;
static line_t *bestslideline, *secondslideline;

static mobj_t *slidemo;

static float tmmove[3];
static mobj_t *shootthing;

// Height if not aiming up or down.
static float shootz;

static int la_damage;
static float aimslope;

// slopes to top and bottom of target
static float topslope, bottomslope;

static mobj_t *usething;

static mobj_t *bombsource, *bombspot;
static int bombdamage;
static int bombdistance;

static boolean crushchange;
static boolean nofit;

static float startPos[3]; // start position for trajectory line checks
static float endPos[3]; // end position for trajectory checks

#if __JHEXEN__
static mobj_t *tsthing;
static boolean DamageSource;
static mobj_t *onmobj; // generic global onmobj...used for landing on pods/players

static mobj_t *PuzzleItemUser;
static int PuzzleItemType;
static boolean PuzzleActivated;
#endif

#if !__JHEXEN__
static int tmunstuck; // $unstuck: used to check unsticking
#endif

// CODE --------------------------------------------------------------------

/**
 * Unlinks a mobj from the world so that it can be moved.
 */
void P_UnsetMobjPosition(mobj_t *mo)
{
    P_UnlinkMobj(mo);
}

/**
 * Called after a move to link the mobj back into the world.
 */
void P_SetMobjPosition(mobj_t *mo)
{
    int         flags = 0;

    if(!(mo->flags & MF_NOSECTOR))
        flags |= DDLINK_SECTOR;

    if(!(mo->flags & MF_NOBLOCKMAP))
        flags |= DDLINK_BLOCKMAP;

    P_LinkMobj(mo, flags);
}

float P_GetGravity(void)
{
    if(IS_NETGAME && cfg.netGravity != -1)
        return (float) cfg.netGravity / 100;

    return *((float*) DD_GetVariable(DD_GRAVITY));
}

static boolean PIT_StompThing(mobj_t *mo, void *data)
{
    int         stompAnyway;
    float       blockdist;

    if(!(mo->flags & MF_SHOOTABLE))
        return true;

    blockdist = mo->radius + tmthing->radius;
    if(fabs(mo->pos[VX] - tm[VX]) >= blockdist ||
       fabs(mo->pos[VY] - tm[VY]) >= blockdist)
        return true; // Didn't hit it.

    if(mo == tmthing)
        return true; // Don't clip against self.

    stompAnyway = *(int*) data;

    // Should we stomp anyway? unless self.
    if(mo != tmthing && stompAnyway)
    {
        P_DamageMobj2(mo, tmthing, tmthing, 10000, true);
        return true;
    }

#if __DOOM64TC__ || __WOLFTC__
    // monsters don't stomp things
    if(!tmthing->player)
        return false;
#elif __JDOOM__
    // monsters don't stomp things except on boss level
    if(!tmthing->player && gamemap != 30)
        return false;
#endif

    if(!(tmthing->flags2 & MF2_TELESTOMP))
        return false; // Not allowed to stomp things

    // Do stomp damage (unless self)
    if(mo != tmthing)
        P_DamageMobj2(mo, tmthing, tmthing, 10000, true);

    return true;
}

boolean P_TeleportMove(mobj_t *thing, float x, float y, boolean alwaysstomp)
{
    int         stomping;
    subsector_t *newsubsec;
    float       box[4];

    // kill anything occupying the position
    tmthing = thing;

    stomping = alwaysstomp;

    tm[VX] = x;
    tm[VY] = y;

    tmbbox[BOXTOP]    = tm[VY] + tmthing->radius;
    tmbbox[BOXBOTTOM] = tm[VY] - tmthing->radius;
    tmbbox[BOXRIGHT]  = tm[VX] + tmthing->radius;
    tmbbox[BOXLEFT]   = tm[VX] - tmthing->radius;

    newsubsec = R_PointInSubsector(tm[VX], tm[VY]);

    // $unstuck: floorline used with tmunstuck
    ceilingline = NULL;
#if !__JHEXEN__
    blockline = floorline = NULL;

    // $unstuck
    tmunstuck = thing->dplayer && thing->dplayer->mo == thing;
#endif

    // the base floor / ceiling is from the subsector that contains the
    // point.  Any contacted lines the step closer together will adjust them
    tmfloorz = tmdropoffz = P_GetFloatp(newsubsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFloatp(newsubsec, DMU_CEILING_HEIGHT);
    tmfloorpic = P_GetIntp(newsubsec, DMU_FLOOR_MATERIAL);

    VALIDCOUNT++;
    P_EmptyIterList(spechit);

    box[BOXLEFT]   = tmbbox[BOXLEFT]   - MAXRADIUS;
    box[BOXRIGHT]  = tmbbox[BOXRIGHT]  + MAXRADIUS;
    box[BOXBOTTOM] = tmbbox[BOXBOTTOM] - MAXRADIUS;
    box[BOXTOP]    = tmbbox[BOXTOP]    + MAXRADIUS;

    // Stomp on any things contacted.
    if(!P_MobjsBoxIterator(box, PIT_StompThing, &stomping))
        return false;

    // The move is ok, so link the thing into its new position.
    P_UnsetMobjPosition(thing);

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
#if !__JHEXEN__
    thing->dropoffz = tmdropoffz;
#endif
    thing->pos[VX] = x;
    thing->pos[VY] = y;

    P_SetMobjPosition(thing);
    P_ClearThingSRVO(thing);

    return true;
}

/**
 * Checks to see if a start->end trajectory line crosses a blocking line.
 * Returns false if it does.
 *
 * tmbbox holds the bounding box of the trajectory. If that box does not
 * touch the bounding box of the line in question, then the trajectory is
 * not blocked. If the start is on one side of the line and the end is on
 * the other side, then the trajectory is blocked.
 *
 * Currently this assumes an infinite line, which is not quite correct.
 * A more correct solution would be to check for an intersection of the
 * trajectory and the line, but that takes longer and probably really isn't
 * worth the effort.
 *
 * @param data      Unused
 */
static boolean PIT_CrossLine(line_t *ld, void *data)
{
    if(!(P_GetIntp(ld, DMU_FLAGS) & ML_TWOSIDED) ||
      (P_GetIntp(ld, DMU_FLAGS) & (ML_BLOCKING | ML_BLOCKMONSTERS)))
    {
        float       bbox[4];

        P_GetFloatpv(ld, DMU_BOUNDING_BOX, bbox);

        if(!(tmbbox[BOXLEFT]   > bbox[BOXRIGHT] ||
             tmbbox[BOXRIGHT]  < bbox[BOXLEFT] ||
             tmbbox[BOXTOP]    < bbox[BOXBOTTOM] ||
             tmbbox[BOXBOTTOM] > bbox[BOXTOP]))
        {
            if(P_PointOnLineSide(startPos[VX], startPos[VY], ld) !=
               P_PointOnLineSide(endPos[VX], endPos[VY], ld))
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
boolean P_CheckSides(mobj_t *actor, float x, float y)
{
    memcpy(startPos, actor->pos, sizeof(startPos));

    endPos[VX] = x;
    endPos[VY] = y;
    endPos[VZ] = DDMINFLOAT; // just initialize with *something*

    // Here is the bounding box of the trajectory

    tmbbox[BOXLEFT]   = (startPos[VX] < endPos[VX]? startPos[VX] : endPos[VX]);
    tmbbox[BOXRIGHT]  = (startPos[VX] > endPos[VX]? startPos[VX] : endPos[VX]);
    tmbbox[BOXTOP]    = (startPos[VY] > endPos[VY]? startPos[VY] : endPos[VY]);
    tmbbox[BOXBOTTOM] = (startPos[VY] < endPos[VY]? startPos[VY] : endPos[VY]);

    return !P_AllLinesBoxIterator(tmbbox, PIT_CrossLine, 0);
}

/**
 * $unstuck: used to test intersection between thing and line assuming NO
 * movement occurs -- used to avoid sticky situations.
 */
static int untouched(line_t *ld)
{
    float       x, y, box[4];
    float       bbox[4];
    float       radius;

    P_GetFloatpv(ld, DMU_BOUNDING_BOX, bbox);

    x = tmthing->pos[VX];
    y = tmthing->pos[VY];
    radius = tmthing->radius;

    if(((box[BOXRIGHT]  = x + radius) <= bbox[BOXLEFT]) ||
       ((box[BOXLEFT]   = x - radius) >= bbox[BOXRIGHT]) ||
       ((box[BOXTOP]    = y + radius) <= bbox[BOXBOTTOM]) ||
       ((box[BOXBOTTOM] = y - radius) >= bbox[BOXTOP]) ||
       P_BoxOnLineSide(box, ld) != -1)
        return true;
    else
        return false;
}

static boolean PIT_CheckThing(mobj_t *thing, void *data)
{
    int         damage;
    float       blockdist;
    boolean     solid;
#if !__JHEXEN__
    boolean     overlap = false;
#endif

    // Don't clip against self.
    if(thing == tmthing)
        return true;

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)) ||
       P_IsCamera(thing) || P_IsCamera(tmthing))
        return true;

#if !__JHEXEN__
    // Player only.
    if(tmthing->player && tm[VZ] != DDMAXFLOAT &&
       (cfg.moveCheckZ || (tmthing->flags2 & MF2_PASSMOBJ)))
    {
        if((thing->pos[VZ] > tm[VZ] + tmheight) ||
           (thing->pos[VZ] + thing->height < tm[VZ]))
            return true; // Under or over it.

        overlap = true;
    }
#endif

    blockdist = thing->radius + tmthing->radius;
    if(fabs(thing->pos[VX] - tm[VX]) >= blockdist ||
       fabs(thing->pos[VY] - tm[VY]) >= blockdist)
        return true; // Didn't hit thing.

#if __JHEXEN__
    // Stop here if we are a client.
    if(IS_CLIENT)
        return false;
#endif

#if !__JHEXEN__
    if(!tmthing->player && (tmthing->flags2 & MF2_PASSMOBJ))
#else
    BlockingMobj = thing;
    if(tmthing->flags2 & MF2_PASSMOBJ)
#endif
    {   // check if a mobj passed over/under another object
#if __JHERETIC__
        if((tmthing->type == MT_IMP || tmthing->type == MT_WIZARD) &&
           (thing->type == MT_IMP || thing->type == MT_WIZARD))
        {
            return false; // Don't let imps/wizards fly over other imps/wizards.
        }
#elif __JHEXEN__
        if(tmthing->type == MT_BISHOP && thing->type == MT_BISHOP)
            return false; // Don't let bishops fly over other bishops.
#endif
        if(tmthing->pos[VZ] > thing->pos[VZ] + thing->height &&
           !(thing->flags & MF_SPECIAL))
        {
            return true;
        }
        else if(tmthing->pos[VZ] + tmthing->height < thing->pos[VZ] &&
                !(thing->flags & MF_SPECIAL))
        {
            return true; // Under thing.
        }
    }

    // Check for skulls slamming into things.
    if(tmthing->flags & MF_SKULLFLY)
    {
#if __JHEXEN__
        if(tmthing->type == MT_MINOTAUR)
        {
            // Slamming minotaurs shouldn't move non-creatures.
            if(!(thing->flags & MF_COUNTKILL))
            {
                return false;
            }
        }
        else if(tmthing->type == MT_HOLY_FX)
        {
            if((thing->flags & MF_SHOOTABLE) && thing != tmthing->target)
            {
                if(IS_NETGAME && !deathmatch && thing->player)
                    return true; // don't attack other co-op players

                if((thing->flags2 & MF2_REFLECTIVE) &&
                   (thing->player || (thing->flags2 & MF2_BOSS)))
                {
                    tmthing->tracer = tmthing->target;
                    tmthing->target = thing;
                    return true;
                }

                if(thing->flags & MF_COUNTKILL || thing->player)
                {
                    tmthing->tracer = thing;
                }

                if(P_Random() < 96)
                {
                    damage = 12;
                    if(thing->player || (thing->flags2 & MF2_BOSS))
                    {
                        damage = 3;
                        // Ghost burns out faster when attacking players/bosses.
                        tmthing->health -= 6;
                    }

                    P_DamageMobj(thing, tmthing, tmthing->target, damage);
                    if(P_Random() < 128)
                    {
                        P_SpawnMobj3fv(MT_HOLY_PUFF, tmthing->pos);
                        S_StartSound(SFX_SPIRIT_ATTACK, tmthing);
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
                    tmthing->tracer = NULL;
            }
            return true;
        }
#endif
        if(tmthing->damage == DDMAXINT) //// \kludge to support old save games.
            damage = tmthing->info->damage;
        else
            damage = tmthing->damage;

        damage *= (P_Random() % 8) + 1;
        P_DamageMobj(thing, tmthing, tmthing, damage);

        tmthing->flags &= ~MF_SKULLFLY;
        tmthing->mom[MX] = tmthing->mom[MY] = tmthing->mom[MZ] = 0;

        P_SetMobjState(tmthing, tmthing->info->spawnstate);

        return false; // Stop moving.
    }

#if __JHEXEN__
    // Check for blasted thing running into another
    if((tmthing->flags2 & MF2_BLASTED) && (thing->flags & MF_SHOOTABLE))
    {
        if(!(thing->flags2 & MF2_BOSS) && (thing->flags & MF_COUNTKILL))
        {
            thing->mom[MX] += tmthing->mom[MX];
            thing->mom[MY] += tmthing->mom[MY];

            if(thing->dplayer)
                thing->dplayer->flags |= DDPF_FIXMOM;

            if((thing->mom[MX] + thing->mom[MY]) > 3)
            {
                damage = (tmthing->info->mass / 100) + 1;
                P_DamageMobj(thing, tmthing, tmthing, damage);

                damage = (thing->info->mass / 100) + 1;
                P_DamageMobj(tmthing, thing, thing, damage >> 2);
            }

            return false;
        }
    }
#endif

    // Missiles can hit other things.
    if(tmthing->flags & MF_MISSILE)
    {
#if __JHEXEN__
        // Check for a non-shootable mobj.
        if(thing->flags2 & MF2_NONSHOOTABLE)
            return true;
#else
        // Check for passing through a ghost.
        if((thing->flags & MF_SHADOW) && (tmthing->flags2 & MF2_THRUGHOST))
            return true;
#endif

        // See if it went over / under.
        if(tmthing->pos[VZ] > thing->pos[VZ] + thing->height)
            return true; // Overhead.

        if(tmthing->pos[VZ] + tmthing->height < thing->pos[VZ])
            return true; // Underneath.

#if __JHEXEN__
        if(tmthing->flags2 & MF2_FLOORBOUNCE)
        {
            if(tmthing->target == thing || !(thing->flags & MF_SOLID))
                return true;
            else
                return false;
        }

        if(tmthing->type == MT_LIGHTNING_FLOOR ||
           tmthing->type == MT_LIGHTNING_CEILING)
        {
            if((thing->flags & MF_SHOOTABLE) && thing != tmthing->target)
            {
                if(thing->info->mass != DDMAXINT)
                {
                    thing->mom[MX] += tmthing->mom[MX] / 16;
                    thing->mom[MY] += tmthing->mom[MY] / 16;
                    if(thing->dplayer)
                        thing->dplayer->flags |= DDPF_FIXMOM;
                }

                if((!thing->player && !(thing->flags2 & MF2_BOSS)) ||
                   !(leveltime & 1))
                {
                    if(thing->type == MT_CENTAUR ||
                       thing->type == MT_CENTAURLEADER)
                    {   // Lightning does more damage to centaurs.
                        P_DamageMobj(thing, tmthing, tmthing->target, 9);
                    }
                    else
                    {
                        P_DamageMobj(thing, tmthing, tmthing->target, 3);
                    }

                    if(!(S_IsPlaying(SFX_MAGE_LIGHTNING_ZAP, tmthing)))
                    {
                        S_StartSound(SFX_MAGE_LIGHTNING_ZAP, tmthing);
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

                tmthing->health--;
                if(tmthing->health <= 0 || thing->health <= 0)
                {
                    return false;
                }

                if(tmthing->type == MT_LIGHTNING_FLOOR)
                {
                    if(tmthing->lastenemy &&
                       !tmthing->lastenemy->tracer)
                    {
                        tmthing->lastenemy->tracer = thing;
                    }
                }
                else if(!tmthing->tracer)
                {
                    tmthing->tracer = thing;
                }
            }

            return true; // Lightning zaps through all sprites.
        }
        else if(tmthing->type == MT_LIGHTNING_ZAP)
        {
            mobj_t *lmo;

            if((thing->flags & MF_SHOOTABLE) && thing != tmthing->target)
            {
                lmo = tmthing->lastenemy;
                if(lmo)
                {
                    if(lmo->type == MT_LIGHTNING_FLOOR)
                    {
                        if(lmo->lastenemy &&
                           !lmo->lastenemy->tracer)
                        {
                            lmo->lastenemy->tracer = thing;
                        }
                    }
                    else if(!lmo->tracer)
                    {
                        lmo->tracer = thing;
                    }

                    if(!(leveltime & 3))
                    {
                        lmo->health--;
                    }
                }
            }
        }
        else if(tmthing->type == MT_MSTAFF_FX2 && thing != tmthing->target)
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
                    P_DamageMobj(thing, tmthing, tmthing->target, 10);
                    return true;
                    break;
                }
            }
        }
#endif

        // Don't hit same species as originator.
#if __JDOOM__
        if(tmthing->target &&
           (tmthing->target->type == thing->type ||
           (tmthing->target->type == MT_KNIGHT && thing->type == MT_BRUISER) ||
           (tmthing->target->type == MT_BRUISER && thing->type == MT_KNIGHT)))
#else
        if(tmthing->target && tmthing->target->type == thing->type)
#endif
        {
            if(thing == tmthing->target)
                return true;

#if __JHEXEN__
            if(!thing->player)
                return false; // Hit same species as originator, explode, no damage
#else
            if(!monsterinfight && thing->type != MT_PLAYER)
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

        if(tmthing->flags2 & MF2_RIP)
        {
#if __JHEXEN__
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE))
#else
            if(!(thing->flags & MF_NOBLOOD))
#endif
            {   // Ok to spawn some blood.
                P_RipperBlood(tmthing);
            }
#if __JHERETIC__
            S_StartSound(sfx_ripslop, tmthing);
#endif

            if(tmthing->damage == DDMAXINT) //// \kludge to support old save games.
                damage = tmthing->info->damage;
            else
                damage = tmthing->damage;

            damage *= (P_Random() & 3) + 2;

            P_DamageMobj(thing, tmthing, tmthing->target, damage);

            if((thing->flags2 & MF2_PUSHABLE) &&
               !(tmthing->flags2 & MF2_CANNOTPUSH))
            {   // Push thing
                thing->mom[MX] += tmthing->mom[MX] / 4;
                thing->mom[MY] += tmthing->mom[MY] / 4;
                if(thing->dplayer)
                    thing->dplayer->flags |= DDPF_FIXMOM;
            }
            P_EmptyIterList(spechit);
            return true;
        }

        // Do damage
#if __JDOOM__
        if(tmthing->damage == DDMAXINT) //// \kludge to support old save games.
            damage = tmthing->info->damage;
        else
#endif
            damage = tmthing->damage;

        damage *= (P_Random() % 8) + 1;
#if __JDOOM__
        P_DamageMobj(thing, tmthing, tmthing->target, damage);
#else
        if(damage)
        {
# if __JHERETIC__
            if(!(thing->flags & MF_NOBLOOD) && P_Random() < 192)
# else //__JHEXEN__
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE) &&
               !(tmthing->type == MT_TELOTHER_FX1) &&
               !(tmthing->type == MT_TELOTHER_FX2) &&
               !(tmthing->type == MT_TELOTHER_FX3) &&
               !(tmthing->type == MT_TELOTHER_FX4) &&
               !(tmthing->type == MT_TELOTHER_FX5) && (P_Random() < 192))
# endif
                P_SpawnBloodSplatter(tmthing->pos[VX], tmthing->pos[VY], tmthing->pos[VZ], thing);

            P_DamageMobj(thing, tmthing, tmthing->target, damage);
        }
#endif
        // Don't traverse anymore.
        return false;
    }

    if((thing->flags2 & MF2_PUSHABLE) && !(tmthing->flags2 & MF2_CANNOTPUSH))
    {   // Push thing
        thing->mom[MX] += tmthing->mom[MX] / 4;
        thing->mom[MY] += tmthing->mom[MY] / 4;
        if(thing->dplayer)
            thing->dplayer->flags |= DDPF_FIXMOM;
    }

    // Check for special pickup.
    if(thing->flags & MF_SPECIAL)
    {
        solid = thing->flags & MF_SOLID;
        if(tmthing->flags & MF_PICKUP)
        {
            // can remove thing
            P_TouchSpecialMobj(thing, tmthing);
        }

        return !solid;
    }

#if !__JHEXEN__
    if(overlap && thing->flags & MF_SOLID)
    {
        // How are we positioned?
        if(tm[VZ] > thing->pos[VZ] + thing->height - 24)
        {
            tmthing->onmobj = thing;
            if(thing->pos[VZ] + thing->height > tmfloorz)
                tmfloorz = thing->pos[VZ] + thing->height;
            return true;
        }
    }
#endif

    return !(thing->flags & MF_SOLID);
}

/**
 * Adjusts tmfloorz and tmceilingz as lines are contacted
 */
static boolean PIT_CheckLine(line_t *ld, void *data)
{
    float       bbox[4];

    P_GetFloatpv(ld, DMU_BOUNDING_BOX, bbox);

    if(tmbbox[BOXRIGHT]  <= bbox[BOXLEFT] ||
       tmbbox[BOXLEFT]   >= bbox[BOXRIGHT] ||
       tmbbox[BOXTOP]    <= bbox[BOXBOTTOM] ||
       tmbbox[BOXBOTTOM] >= bbox[BOXTOP])
        return true;

    if(P_BoxOnLineSide(tmbbox, ld) != -1)
        return true;

    // A line has been hit
#if !__JHEXEN__
    tmthing->wallhit = true;

    // A Hit event will be sent to special lines.
    if(P_ToXLine(ld)->special)
        tmhitline = ld;
#endif

    /**
     * The moving thing's destination position will cross the given line.
     * If this should not be allowed, return false.
     * If the line is special, keep track of it to process later if the
     * move is proven ok.
     * \note Specials are NOT sorted by order, so two special lines that
     * are only 8 pixels apart could be crossed in either order.
     */

    // $unstuck: allow player to move out of 1s wall, to prevent sticking
    if(!P_GetPtrp(ld, DMU_BACK_SECTOR)) // One sided line.
    {
#if __JHEXEN__
        if(tmthing->flags2 & MF2_BLASTED)
            P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
        CheckForPushSpecial(ld, 0, tmthing);
        return false;
#else
        float       dx = P_GetFloatp(ld, DMU_DX);
        float       dy = P_GetFloatp(ld, DMU_DY);

        blockline = ld;
        return tmunstuck && !untouched(ld) &&
            ((tm[VX] - tmthing->pos[VX]) * dy) >
            ((tm[VY] - tmthing->pos[VY]) * dx);
#endif
    }

    // \fixme Will never pass this test due to above. Is the previous check
    // supposed to qualify player mobjs only?
#if __JHERETIC__
    if(!P_GetPtrp(ld, DMU_BACK_SECTOR)) // one sided line
    {   // One sided line
        if(tmthing->flags & MF_MISSILE)
        {   // Missiles can trigger impact specials
            if(P_ToXLine(ld)->special)
                P_AddObjectToIterList(spechit, ld);
        }
        return false;
    }
#endif

    if(!(tmthing->flags & MF_MISSILE))
    {
        // Explicitly blocking everything?
        if(P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKING)
        {
#if __JHEXEN__
            if(tmthing->flags2 & MF2_BLASTED)
                P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
            CheckForPushSpecial(ld, 0, tmthing);
            return false;
#else
            // killough $unstuck: allow escape.
            return tmunstuck && !untouched(ld);
#endif
        }

        // Block monsters only?
#if __JHEXEN__
        if(!tmthing->player && tmthing->type != MT_CAMERA &&
           (P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKMONSTERS))
#elif __JHERETIC__
        if(!tmthing->player && tmthing->type != MT_POD &&
           (P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKMONSTERS))
#else
        if(!tmthing->player &&
           (P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKMONSTERS))
#endif
        {
#if __JHEXEN__
            if(tmthing->flags2 & MF2_BLASTED)
                P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
#endif
            return false;
        }
    }

#if __DOOM64TC__
    if((tmthing->flags & MF_MISSILE))
    {
        if(P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKALL)  // explicitly blocking everything
            return tmunstuck && !untouched(ld);  // killough $unstuck: allow escape
    }
#endif

    // Set openrange, opentop, openbottom.
    P_LineOpening(ld);

    // Adjust floor / ceiling heights.
    if(opentop < tmceilingz)
    {
        tmceilingz = opentop;
        ceilingline = ld;
#if !__JHEXEN__
        blockline = ld;
#endif
    }

    if(openbottom > tmfloorz)
    {
        tmfloorz = openbottom;
#if !__JHEXEN__
        // killough $unstuck: remember floor linedef
        floorline = ld;
        blockline = ld;
#endif
    }

    if(lowfloor < tmdropoffz)
        tmdropoffz = lowfloor;

    // If contacted a special line, add it to the list.
    if(P_ToXLine(ld)->special)
        P_AddObjectToIterList(spechit, ld);

#if !__JHEXEN__
    tmthing->wallhit = false;
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
 *  tmdropoffz
 *   the lowest point contacted
 *   (monsters won't move to a drop off)
 *  speciallines[]
 *  numspeciallines
 */
boolean P_CheckPosition3f(mobj_t *thing, float x, float y, float z)
{
    sector_t   *newsec;
    float       box[4];

    tmthing = thing;

#if !__JHEXEN__
    thing->onmobj = NULL;
    thing->wallhit = false;

    tmhitline = NULL;
    tmheight = thing->height;
#endif

    tm[VX] = x;
    tm[VY] = y;
    tm[VZ] = z;

    tmbbox[BOXTOP]    = tm[VY] + tmthing->radius;
    tmbbox[BOXBOTTOM] = tm[VY] - tmthing->radius;
    tmbbox[BOXRIGHT]  = tm[VX] + tmthing->radius;
    tmbbox[BOXLEFT]   = tm[VX] - tmthing->radius;

    newsec = P_GetPtrp(R_PointInSubsector(tm[VX], tm[VY]), DMU_SECTOR);

    // $unstuck: floorline used with tmunstuck.
    ceilingline = NULL;
#if !__JHEXEN__
    blockline = floorline = NULL;

    // $unstuck
    tmunstuck =
        ((thing->dplayer && thing->dplayer->mo == thing)? true : false);
#endif

    // The base floor/ceiling is from the subsector that contains the point.
    // Any contacted lines the step closer together will adjust them.
    tmfloorz = tmdropoffz = P_GetFloatp(newsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFloatp(newsec, DMU_CEILING_HEIGHT);
    tmfloorpic = P_GetIntp(newsec, DMU_FLOOR_MATERIAL);

    VALIDCOUNT++;
    P_EmptyIterList(spechit);

#if __JHEXEN__
    if((tmthing->flags & MF_NOCLIP) && !(tmthing->flags & MF_SKULLFLY))
        return true;
#else
    if((tmthing->flags & MF_NOCLIP))
        return true;
#endif

    // Check things first, possibly picking things up the bounding box is
    // extended by MAXRADIUS because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap into adjacent blocks by
    // up to MAXRADIUS units.
    box[BOXLEFT]   = tmbbox[BOXLEFT]   - MAXRADIUS;
    box[BOXRIGHT]  = tmbbox[BOXRIGHT]  + MAXRADIUS;
    box[BOXBOTTOM] = tmbbox[BOXBOTTOM] - MAXRADIUS;
    box[BOXTOP]    = tmbbox[BOXTOP]    + MAXRADIUS;

    // The camera goes through all objects.
    if(!P_IsCamera(thing))
    {
#if __JHEXEN__
        BlockingMobj = NULL;
#endif
        if(!P_MobjsBoxIterator(box, PIT_CheckThing, 0))
            return false;

#ifdef _DEBUG
        if(thing->onmobj)
            Con_Message("thing->onmobj = %p (solid:%i)\n", thing->onmobj,
                        (thing->onmobj->flags & MF_SOLID)!=0);
#endif
    }

     // Check lines.
#if __JHEXEN__
    if(tmthing->flags & MF_NOCLIP)
        return true;

    BlockingMobj = NULL;
#endif

    return P_AllLinesBoxIterator(box, PIT_CheckLine, 0);
}

boolean P_CheckPosition3fv(mobj_t *thing, const float pos[3])
{
    return P_CheckPosition3f(thing, pos[VX], pos[VY], pos[VZ]);
}

boolean P_CheckPosition2f(mobj_t *thing, float x, float y)
{
    return P_CheckPosition3f(thing, x, y, DDMAXFLOAT);
}

/**
 * Attempt to move to a new position, crossing special lines unless
 * MF_TELEPORT is set.
 *
 * killough $dropoff_fix
 */
#if __JHEXEN__
static boolean P_TryMove2(mobj_t *thing, float x, float y)
#else
static boolean P_TryMove2(mobj_t *thing, float x, float y, boolean dropoff)
#endif
{
    float       oldpos[3];
    int         side, oldside;
    line_t     *ld;

    // $dropoff_fix: felldown
    floatok = false;
#if !__JHEXEN__
    felldown = false;
#endif
#if __JHEXEN__
    if(!P_CheckPosition2f(thing, x, y))
#else
    if(!P_CheckPosition3f(thing, x, y, thing->pos[VZ]))
#endif
    {
#if __JHEXEN__
        if(!BlockingMobj || BlockingMobj->player || !thing->player)
        {
            goto pushline;
        }
        else if(BlockingMobj->pos[VZ] + BlockingMobj->height - thing->pos[VZ] > 24 ||
                (P_GetFloatp(BlockingMobj->subsector, DMU_CEILING_HEIGHT) -
                 (BlockingMobj->pos[VZ] + BlockingMobj->height) < thing->height) ||
                (tmceilingz - (BlockingMobj->pos[VZ] + BlockingMobj->height) <
                 thing->height))
        {
            goto pushline;
        }
#else
# if __JHERETIC__
        CheckMissileImpact(thing);
# endif
        // Would we hit another thing or a solid wall?
        if(!thing->onmobj || thing->wallhit)
            return false;
#endif
    }

    // GMJ 02/02/02
    if(!(thing->flags & MF_NOCLIP))
    {
#if __JHEXEN__
        if(tmceilingz - tmfloorz < thing->height)
        {   // Doesn't fit.
            goto pushline;
        }
        floatok = true;
        if(!(thing->flags & MF_TELEPORT) &&
           tmceilingz - thing->pos[VZ] < thing->height &&
           thing->type != MT_LIGHTNING_CEILING && !(thing->flags2 & MF2_FLY))
        {   // Mobj must lower itself to fit.
            goto pushline;
        }
#else
        // killough 7/26/98: reformatted slightly
        // killough 8/1/98: Possibly allow escape if otherwise stuck.
        if(tmceilingz - tmfloorz < thing->height || // Doesn't fit.
           // Mobj must lower to fit.
           (floatok = true, !(thing->flags & MF_TELEPORT) &&
            !(thing->flags2 & MF2_FLY) &&
            tmceilingz - thing->pos[VZ] < thing->height) ||
           // Too big a step up.
           (!(thing->flags & MF_TELEPORT) &&
            !(thing->flags2 & MF2_FLY) &&
# if __JHERETIC__
            thing->type != MT_MNTRFX2 && // The Minotaur floor fire (MT_MNTRFX2) can step up any amount
# endif
            tmfloorz - thing->pos[VZ] > 24))
        {
# if __JHERETIC__
            CheckMissileImpact(thing);
# endif
            return tmunstuck && !(ceilingline && untouched(ceilingline)) &&
                !(floorline && untouched(floorline));
        }
# if __JHERETIC__
        if((thing->flags & MF_MISSILE) && tmfloorz > thing->pos[VZ])
        {
            CheckMissileImpact(thing);
        }
# endif
#endif
        if(thing->flags2 & MF2_FLY)
        {
            if(thing->pos[VZ] + thing->height > tmceilingz)
            {
                thing->mom[MZ] = -8;
#if __JHEXEN__
                goto pushline;
#else
                return false;
#endif
            }
            else if(thing->pos[VZ] < tmfloorz &&
                    tmfloorz - tmdropoffz > 24)
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
           tmfloorz - thing->pos[VZ] > 24)
        {
            goto pushline;
        }
#endif

#if __JHEXEN__
        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)) &&
           (tmfloorz - tmdropoffz > 24) &&
           !(thing->flags2 & MF2_BLASTED))
        {   // Can't move over a dropoff unless it's been blasted.
            return false;
        }
#else
        // killough 3/15/98: Allow certain objects to drop off.
        // killough 7/24/98, 8/1/98:
        // Prevent monsters from getting stuck hanging off ledges.
        // killough 10/98: Allow dropoffs in controlled circumstances.
        // killough 11/98: Improve symmetry of clipping on stairs.
        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)))
        {
            // Dropoff height limit.
            if(cfg.avoidDropoffs)
            {
                if(tmfloorz - tmdropoffz > 24)
                    return false;
            }
            else
            {
                if(!dropoff)
                {
                   if(thing->floorz - tmfloorz > 24 ||
                      thing->dropoffz - tmdropoffz > 24)
                      return false;
                }
                else
                {   // Set felldown if drop > 24.
                    felldown = !(thing->flags & MF_NOGRAVITY) &&
                        thing->pos[VZ] - tmfloorz > 24;
                }
            }
        }
#endif

#if __DOOM64TC__
        //// \fixme DJS - FIXME! Mother demon fire attack.
        if(!(thing->flags & MF_TELEPORT) /*&& thing->type != MT_SPAWNFIRE*/
            && tmfloorz - thing->pos[VZ] > 24)
        { // Too big a step up
            CheckMissileImpact(thing);
            return false;
        }
#endif

#if __JHEXEN__
        // must stay within a sector of a certain floor type?
        if((thing->flags2 & MF2_CANTLEAVEFLOORPIC) &&
           (tmfloorpic != P_GetIntp(thing->subsector, DMU_FLOOR_MATERIAL) ||
            tmfloorz - thing->pos[VZ] != 0))
        {
            return false;
        }
#endif

#if !__JHEXEN__
        // killough $dropoff: prevent falling objects from going up too many steps.
        if(!thing->player && (thing->intflags & MIF_FALLING) &&
           tmfloorz - thing->pos[VZ] > (thing->mom[MX] * thing->mom[MX]) +
                                       (thing->mom[MY] * thing->mom[MY]))
        {
            return false;
        }
#endif
    }

    // The move is ok, so link the thing into its new position.
    P_UnsetMobjPosition(thing);

    memcpy(oldpos, thing->pos, sizeof(oldpos));
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
#if __JHEXEN__
    thing->floorpic = tmfloorpic;
#else
    // killough $dropoff_fix: keep track of dropoffs.
    thing->dropoffz = tmdropoffz;
#endif
    thing->pos[VX] = x;
    thing->pos[VY] = y;
    P_SetMobjPosition(thing);

    if(thing->flags2 & MF2_FLOORCLIP)
    {
        if(thing->pos[VZ] == P_GetFloatp(thing->subsector, DMU_FLOOR_HEIGHT) &&
           P_GetMobjFloorType(thing) >= FLOOR_LIQUID)
        {
            thing->floorclip = 10;
        }
        else
        {
            thing->floorclip = 0;
        }
    }

    // If any special lines were hit, do the effect.
    if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        while((ld = P_PopIterList(spechit)) != NULL)
        {
            // See if the line was crossed.
            if(P_ToXLine(ld)->special)
            {
                side = P_PointOnLineSide(thing->pos[VX], thing->pos[VY], ld);
                oldside = P_PointOnLineSide(oldpos[VX], oldpos[VY], ld);
                if(side != oldside)
                {
#if __JHEXEN__
                    if(thing->player)
                    {
                        P_ActivateLine(ld, thing, oldside, SPAC_CROSS);
                    }
                    else if(thing->flags2 & MF2_MCROSS)
                    {
                        P_ActivateLine(ld, thing, oldside, SPAC_MCROSS);
                    }
                    else if(thing->flags2 & MF2_PCROSS)
                    {
                        P_ActivateLine(ld, thing, oldside, SPAC_PCROSS);
                    }
#else
                    P_ActivateLine(ld, thing, oldside, SPAC_CROSS);
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
        if(tmthing->flags2 & MF2_BLASTED)
        {
            P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
        }

        P_IterListResetIterator(spechit, false);
        while((ld = P_IterListIterator(spechit)) != NULL)
        {
            // See if the line was crossed.
            side = P_PointOnLineSide(thing->pos[VX], thing->pos[VY], ld);
            CheckForPushSpecial(ld, side, thing);
        }
    }
    return false;
#endif
}

#if __JHEXEN__
boolean P_TryMove(mobj_t *thing, float x, float y)
#else
boolean P_TryMove(mobj_t *thing, float x, float y, boolean dropoff,
                  boolean slide)
#endif
{
#if __JHEXEN__
    return P_TryMove2(thing, x, y);
#else
    // killough $dropoff_fix
    boolean res = P_TryMove2(thing, x, y, dropoff);

    if(!res && tmhitline)
    {
        // Move not possible, see if the thing hit a line and send a Hit
        // event to it.
        XL_HitLine(tmhitline, P_PointOnLineSide(thing->pos[VX], thing->pos[VY], tmhitline),
                   thing);
    }

    if(res && slide)
        thing->wallrun = true;

    return res;
#endif
}

/**
 * \fixme DJS - This routine has gotten way too big, split if(in->isaline)
 * to a seperate routine?
 */
static boolean PTR_ShootTraverse(intercept_t *in)
{
    float       pos[3];
    float       frac;
    line_t     *li;
    mobj_t     *th;
    float       slope;
    float       dist;
    float       thingtopslope, thingbottomslope;
    divline_t  *trace = (divline_t *) DD_GetVariable(DD_TRACE_ADDRESS);
    sector_t   *frontsector = NULL;
    sector_t   *backsector = NULL;
#if __JHEXEN__
    extern mobj_t LavaInflictor;
#endif
    xline_t    *xline;
    boolean     lineWasHit;
    subsector_t *contact, *originSub;
    float       ctop, cbottom, d[3], step, stepv[3], tracepos[3];
    float       cfloor, cceil;
    int         divisor;

    tracepos[VX] = FIX2FLT(trace->pos[VX]);
    tracepos[VY] = FIX2FLT(trace->pos[VY]);
    tracepos[VZ] = shootz;

    if(in->isaline)
    {
        li = in->d.line;
        xline = P_ToXLine(li);
        if(xline->special)
            P_ActivateLine(li, shootthing, 0, SPAC_IMPACT);

#if __DOOM64TC__
        if(P_GetIntp(li, DMU_FLAGS) & ML_BLOCKALL) // d64tc
            goto hitline;
#endif

        if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
            goto hitline;

        // Crosses a two sided line.
        P_LineOpening(li);

        dist = attackrange * in->frac;
        frontsector = P_GetPtrp(li, DMU_FRONT_SECTOR);
        backsector = P_GetPtrp(li, DMU_BACK_SECTOR);

        slope = (openbottom - tracepos[VZ]) / dist;
        if(slope > aimslope)
            goto hitline;

        slope = (opentop - tracepos[VZ]) / dist;
        if(slope < aimslope)
            goto hitline;

        // Shot continues...
        return true;

        // Hit a line.
      hitline:
        lineWasHit = true;

        // Position a bit closer.
        frac = in->frac - (4 / attackrange);
        pos[VX] = tracepos[VX] + (FIX2FLT(trace->dx) * frac);
        pos[VY] = tracepos[VY] + (FIX2FLT(trace->dy) * frac);
        pos[VZ] = tracepos[VZ] + (aimslope * (frac * attackrange));

        // Is it a sky hack wall? If the hitpoint is above the visible line
        // no puff must be shown.
        frontsector = P_GetPtrp(li, DMU_FRONT_SECTOR);
        backsector = P_GetPtrp(li, DMU_BACK_SECTOR);

        if(backsector &&
           P_GetIntp(frontsector, DMU_CEILING_MATERIAL) == skyMaskMaterial &&
           P_GetIntp(backsector, DMU_CEILING_MATERIAL) == skyMaskMaterial &&
           (pos[VZ] > P_GetFloatp(frontsector, DMU_CEILING_HEIGHT) ||
            pos[VZ] > P_GetFloatp(backsector, DMU_CEILING_HEIGHT)))
            return false;

        // This is the subsector where the trace originates.
        originSub = R_PointInSubsector(tracepos[VX], tracepos[VY]);

        d[VX] = pos[VX] - tracepos[VX];
        d[VY] = pos[VY] - tracepos[VY];
        d[VZ] = pos[VZ] - tracepos[VZ];

        if(d[VZ] != 0)
        {
            contact = R_PointInSubsector(pos[VX], pos[VY]);
            step = P_ApproxDistance3(d[VX], d[VY], d[VZ]);
            stepv[VX] = d[VX] / step;
            stepv[VY] = d[VY] / step;
            stepv[VZ] = d[VZ] / step;

            cfloor = P_GetFloatp(contact, DMU_FLOOR_HEIGHT);
            cceil = P_GetFloatp(contact, DMU_CEILING_HEIGHT);
            // Backtrack until we find a non-empty sector.
            while(cceil <= cfloor && contact != originSub)
            {
                d[VX] -= 8 * stepv[VX];
                d[VY] -= 8 * stepv[VY];
                d[VZ] -= 8 * stepv[VZ];
                pos[VX] = tracepos[VX] + d[VX];
                pos[VY] = tracepos[VY] + d[VY];
                pos[VZ] = tracepos[VZ] + d[VZ];
                contact = R_PointInSubsector(pos[VX], pos[VY]);
            }

            // Should we backtrack to hit a plane instead?
            ctop = cceil - 4;
            cbottom = cfloor + 4;
            divisor = 2;

            // We must not hit a sky plane.
            if((pos[VZ] > ctop &&
                P_GetIntp(contact, DMU_CEILING_MATERIAL) == skyMaskMaterial) ||
               (pos[VZ] < cbottom &&
                P_GetIntp(contact, DMU_FLOOR_MATERIAL) == skyMaskMaterial))
                return false;

            // Find the approximate hitpoint by stepping back and
            // forth using smaller and smaller steps.
            while((pos[VZ] > ctop || pos[VZ] < cbottom) && divisor <= 128)
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
                while((d[VZ] > 0 && pos[VZ] <= ctop) ||
                      (d[VZ] < 0 && pos[VZ] >= cbottom))
                {
                    pos[VX] += d[VX] / divisor;
                    pos[VY] += d[VY] / divisor;
                    pos[VZ] += d[VZ] / divisor;
                }
            }
        }

        // Spawn bullet puffs.
        P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);

#if !__JHEXEN__
        if(lineWasHit && xline->special)
        {
            // Extended shoot events only happen when the bullet actually
            // hits the line.
            XL_ShootLine(li, 0, shootthing);
        }
/*
if(lineWasHit)
    Con_Message("Hit line [%i,%i]\n", P_GetIntp(li, DMU_SIDE0), P_GetIntp(li, DMU_SIDE1));
*/
#endif
        // Don't go any farther.
        return false;
    }

    // Shot a mobj.
    th = in->d.mo;
    if(th == shootthing)
        return true; // Can't shoot self.

    if(!(th->flags & MF_SHOOTABLE))
        return true; // Corpse or something.

#if __JHERETIC__
    // Check for physical attacks on a ghost.
    if((th->flags & MF_SHADOW) && shootthing->player->readyweapon == WT_FIRST)
        return true;
#endif

    // Check angles to see if the thing can be aimed at
    dist = attackrange * in->frac;
    {
    float       dz = th->pos[VZ];

    if(!(th->player && (th->player->plr->flags & DDPF_CAMERA)))
        dz += th->height;
    dz -= tracepos[VZ];

    thingtopslope = dz / dist;
    }

    if(thingtopslope < aimslope)
        return true; // Shot over the thing.

    thingbottomslope = (th->pos[VZ] - tracepos[VZ]) / dist;
    if(thingbottomslope > aimslope)
        return true; // Shot under the thing.

    // Hit thing.

    // Position a bit closer.
    frac = in->frac - (10 / attackrange);

    pos[VX] = tracepos[VX] + (FIX2FLT(trace->dx) * frac);
    pos[VY] = tracepos[VY] + (FIX2FLT(trace->dy) * frac);
    pos[VZ] = tracepos[VZ] + (aimslope * (frac * attackrange));

    // Spawn bullet puffs or blood spots, depending on target type.
#if __JHERETIC__
    if(PuffType == MT_BLASTERPUFF1)
    {
        // Make blaster big puff.
        mobj_t *mo = P_SpawnMobj3fv(MT_BLASTERPUFF2, pos);
        S_StartSound(sfx_blshit, mo);
    }
    else
        P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);
#elif __JHEXEN__
    P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);
#endif

    if(la_damage)
    {
#if __JHEXEN__
        if(!(in->d.mo->flags2 & MF2_INVULNERABLE))
#endif
        {
            if(!(in->d.mo->flags & MF_NOBLOOD))
            {
#if __JDOOM__
                P_SpawnBlood(pos[VX], pos[VY], pos[VZ], la_damage);
#elif __JHEXEN__
                if(PuffType == MT_AXEPUFF || PuffType == MT_AXEPUFF_GLOW)
                {
                    P_SpawnBloodSplatter2(pos[VX], pos[VY], pos[VZ], in->d.mo);
                }
#else
                if(P_Random() < 192)
                    P_SpawnBloodSplatter(pos[VX], pos[VY], pos[VZ], in->d.mo);
#endif
            }
#if __JDOOM__
            else
                P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);
#endif
        }

#if __JHEXEN__
        if(PuffType == MT_FLAMEPUFF2)
        {   // Cleric FlameStrike does fire damage.
            P_DamageMobj(th, &LavaInflictor, shootthing, la_damage);
        }
        else
#endif
        {
            P_DamageMobj(th, shootthing, shootthing, la_damage);
        }
    }

    // Don't go any farther.
    return false;
}

/**
 * Sets linetaget and aimslope when a target is aimed at.
 */
static boolean PTR_AimTraverse(intercept_t *in)
{
    line_t     *li;
    mobj_t     *th;
    float       slope, thingtopslope, thingbottomslope;
    float       dist;
    sector_t   *backsector, *frontsector;

    if(in->isaline)
    {
        float   ffloor, bfloor;
        float   fceil, bceil;

        li = in->d.line;

        if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
            return false; // Stop.

        // Crosses a two sided line.
        // A two sided line will restrict the possible target ranges.
        P_LineOpening(li);

        if(openbottom >= opentop)
            return false; // Stop.

        dist = attackrange * in->frac;

        frontsector = P_GetPtrp(li, DMU_FRONT_SECTOR);
        ffloor = P_GetFloatp(frontsector, DMU_FLOOR_HEIGHT);
        fceil = P_GetFloatp(frontsector, DMU_CEILING_HEIGHT);

        backsector = P_GetPtrp(li, DMU_BACK_SECTOR);
        bfloor = P_GetFloatp(backsector, DMU_FLOOR_HEIGHT);
        bceil = P_GetFloatp(backsector, DMU_CEILING_HEIGHT);

        if(ffloor != bfloor)
        {
            slope = (openbottom - shootz) / dist;
            if(slope > bottomslope)
                bottomslope = slope;
        }

        if(fceil != bceil)
        {
            slope = (opentop - shootz) / dist;
            if(slope < topslope)
                topslope = slope;
        }

        if(topslope <= bottomslope)
            return false; // Stop.

        return true; // Shot continues...
    }

    // Shot a mobj.
    th = in->d.mo;
    if(th == shootthing)
        return true; // Can't shoot self.

    if(!(th->flags & MF_SHOOTABLE))
        return true; // Corpse or something?

#if __JHERETIC__
    if(th->type == MT_POD)
        return true; // Can't auto-aim at pods.
#endif

#if __JDOOM__ || __JHEXEN__
    if(th->player && IS_NETGAME && !deathmatch)
        return true; // Don't aim at fellow co-op players.
#endif

    // Check angles to see if the thing can be aimed at.
    dist = attackrange * in->frac;
    {
        float       posz = th->pos[VZ];

        if(!(th->player && (th->player->plr->flags & DDPF_CAMERA)))
            posz += th->height;

        thingtopslope = (posz - shootz) / dist;

        if(thingtopslope < bottomslope)
            return true; // Shot over the thing.

    // Too far below?
// $addtocfg $limitautoaimZ:
#if __JHEXEN__
        if(posz < shootz - attackrange / 1.2f)
            return true;
#endif
    }

    thingbottomslope = (th->pos[VZ] - shootz) / dist;
    if(thingbottomslope > topslope)
        return true; // Shot under the thing.

    // Too far above?
// $addtocfg $limitautoaimZ:
#if __JHEXEN__
    if(th->pos[VZ] > shootz + attackrange / 1.2f)
        return true;
#endif

    // this thing can be hit!
    if(thingtopslope > topslope)
        thingtopslope = topslope;

    if(thingbottomslope < bottomslope)
        thingbottomslope = bottomslope;

    aimslope = (thingtopslope + thingbottomslope) / 2;
    linetarget = th;

    return false; // Don't go any farther.
}

float P_AimLineAttack(mobj_t *t1, angle_t angle, float distance)
{
    uint        an;
    float       pos[2];

    an = angle >> ANGLETOFINESHIFT;
    shootthing = t1;

    pos[VX] = t1->pos[VX] + distance * FIX2FLT(finecosine[an]);
    pos[VY] = t1->pos[VY] + distance * FIX2FLT(finesine[an]);

    // Determine the z trace origin.
    shootz = t1->pos[VZ];
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
            shootz += (cfg.plrViewHeight - 5);
    }
    else
        shootz += (t1->height / 2) + 8;

#if __JDOOM__
    topslope = 60;
    bottomslope = -topslope;
#else
    topslope = 100;
    bottomslope = -100;
#endif

    attackrange = distance;
    linetarget = NULL;

    P_PathTraverse(t1->pos[VX], t1->pos[VY], pos[VX], pos[VY],
                   PT_ADDLINES | PT_ADDMOBJS, PTR_AimTraverse);

    if(linetarget)
    {   // While autoaiming, we accept this slope.
        if(!t1->player || !cfg.noAutoAim)
            return aimslope;
    }

    if(t1->player && cfg.noAutoAim)
    {
        // The slope is determined by lookdir.
        return tan(LOOKDIR2RAD(t1->dplayer->lookdir)) / 1.2;
    }

    return 0;
}

/**
 * If damage == 0, it is just a test trace that will leave linetarget set.
 */
void P_LineAttack(mobj_t *t1, angle_t angle, float distance, float slope,
                  int damage)
{
    uint        an;
    float       targetPos[2];

    an = angle >> ANGLETOFINESHIFT;
    shootthing = t1;
    la_damage = damage;

    targetPos[VX] = t1->pos[VX] + distance * FIX2FLT(finecosine[an]);
    targetPos[VY] = t1->pos[VY] + distance * FIX2FLT(finesine[an]);

    // Determine the z trace origin.
    shootz = t1->pos[VZ];
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
            shootz += cfg.plrViewHeight - 5;
    }
    else
        shootz += (t1->height / 2) + 8;

    shootz -= t1->floorclip;
    attackrange = distance;
    aimslope = slope;

    if(P_PathTraverse(t1->pos[VX], t1->pos[VY], targetPos[VX], targetPos[VY],
                      PT_ADDLINES | PT_ADDMOBJS, PTR_ShootTraverse))
    {
#if __JHEXEN__
        switch(PuffType)
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
            P_SpawnPuff(targetPos[VX], targetPos[VY], shootz + (slope * distance));
            break;

        default:
            break;
        }
#endif
    }
}

/**
 * "bombsource" is the creature that caused the explosion at "bombspot".
 */
static boolean PIT_RadiusAttack(mobj_t *thing, void *data)
{
    float       dx, dy, dz, dist;

    if(!(thing->flags & MF_SHOOTABLE))
        return true;

    // Boss spider and cyborg take no damage from concussion.
#if __JHERETIC__
    if(thing->type == MT_MINOTAUR || thing->type == MT_SORCERER1 ||
       thing->type == MT_SORCERER2)
        return true;
#elif __JDOOM__
    if(thing->type == MT_CYBORG || thing->type == MT_SPIDER)
        return true;
#else
    if(!DamageSource && thing == bombsource) // Don't damage the source of the explosion.
        return true;
#endif

    dx = fabs(thing->pos[VX] - bombspot->pos[VX]);
    dy = fabs(thing->pos[VY] - bombspot->pos[VY]);
    dz = fabs(thing->pos[VZ] - bombspot->pos[VZ]);

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

    if(dist >= bombdistance)
        return true; // Out of range.

    // Must be in direct path.
    if(P_CheckSight(thing, bombspot))
    {
        int     damage;

        damage = (bombdamage * (bombdistance - dist) / bombdistance) + 1;
#if __JHEXEN__
        if(thing->player)
            damage /= 4;
#endif
        P_DamageMobj(thing, bombspot, bombsource, damage);
    }

    return true;
}

/**
 * Source is the creature that caused the explosion at spot.
 */
#if __JHEXEN__
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance,
                    boolean damageSource)
#else
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance)
#endif
{
    float       dist;
    float       box[4];

    dist = distance + MAXRADIUS;

    box[BOXLEFT]   = spot->pos[VX] - dist;
    box[BOXRIGHT]  = spot->pos[VX] + dist;
    box[BOXBOTTOM] = spot->pos[VY] - dist;
    box[BOXTOP]    = spot->pos[VY] + dist;

    bombspot = spot;
    bombdamage = damage;
    bombdistance = distance;

#if __JHERETIC__
    if(spot->type == MT_POD && spot->target)
        bombsource = spot->target;
    else
#endif
        bombsource = source;

#if __JHEXEN__
    DamageSource = damageSource;
#endif

    P_MobjsBoxIterator(box, PIT_RadiusAttack, 0);
}

static boolean PTR_UseTraverse(intercept_t *in)
{
    int         side;

    if(!P_ToXLine(in->d.line)->special)
    {
        P_LineOpening(in->d.line);
        if(openrange <= 0)
        {
            if(usething->player)
                S_StartSound(PCLASS_INFO(usething->player->class)->failUseSound,
                             usething);

            return false; // Can't use through a wall.
        }

#if __JHEXEN__
        if(usething->player)
        {
            float       pheight = usething->pos[VZ] + usething->height/2;

            if((opentop < pheight) || (openbottom > pheight))
                S_StartSound(PCLASS_INFO(usething->player->class)->failUseSound,
                             usething);
        }
#endif
        // Not a special line, but keep checking.
        return true;
    }

    side = 0;
    if(P_PointOnLineSide(usething->pos[VX], usething->pos[VY], in->d.line) == 1)
        side = 1;

#if !__JDOOM__
    if(side == 1)
        return false;       // don't use back side
#endif

    P_ActivateLine(in->d.line, usething, side, SPAC_USE);

#if !__JHEXEN__
    // Can use multiple line specials in a row with the PassThru flag.
    if(P_GetIntp(in->d.line, DMU_FLAGS) & ML_PASSUSE)
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
void P_UseLines(player_t *player)
{
    uint            an;
    float           pos[3];
    mobj_t         *mo;

    if(IS_CLIENT)
    {
#ifdef _DEBUG
        Con_Message("P_UseLines: Sending a use request for player %i.\n",
                    player - players);
#endif
        NetCl_PlayerActionRequest(player, GPA_USE);
        return;
    }

    usething = mo = player->plr->mo;

    an = mo->angle >> ANGLETOFINESHIFT;

    pos[VX] = mo->pos[VX];
    pos[VY] = mo->pos[VY];
    pos[VZ] = mo->pos[VZ];

    pos[VX] += USERANGE * FIX2FLT(finecosine[an]);
    pos[VY] += USERANGE * FIX2FLT(finesine[an]);

    P_PathTraverse(mo->pos[VX], mo->pos[VY], pos[VX], pos[VY],
                   PT_ADDLINES, PTR_UseTraverse);
}

/**
 * Takes a valid thing and adjusts the thing->floorz, thing->ceilingz,
 * and possibly thing->pos[VZ].
 *
 * This is called for all nearby monsters whenever a sector changes height
 * If the thing doesn't fit, the z will be set to the lowest value and
 * false will be returned
 *
 * @param thing         The mobj whoose position to adjust.
 * @return boolean      @c true, if the thing did fit.
 */
static boolean P_ThingHeightClip(mobj_t *thing)
{
    boolean         onfloor;

    if(P_IsCamera(thing))
        return false; // Don't height clip cameras.

    onfloor = (thing->pos[VZ] == thing->floorz);
    P_CheckPosition3fv(thing, thing->pos);

    // What about stranding a monster partially off an edge?

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
#if __JHEXEN__
    thing->floorpic = tmfloorpic;
#else
    // killough $dropoff_fix: remember dropoffs.
    thing->dropoffz = tmdropoffz;
#endif

    if(onfloor)
    {
#if __JHEXEN__
        if((thing->pos[VZ] - thing->floorz < 9) ||
           (thing->flags & MF_NOGRAVITY))
        {
            thing->pos[VZ] = thing->floorz;
        }
#else
        // Walking monsters rise and fall with the floor.
        thing->pos[VZ] = thing->floorz;

        // killough $dropoff_fix:
        // Possibly upset balance of objects hanging off ledges.
        if((thing->intflags & MIF_FALLING) && thing->gear >= MAXGEAR)
            thing->gear = 0;
#endif
    }
    else
    {
        // Don't adjust a floating monster unless forced to.
        if(thing->pos[VZ] + thing->height > thing->ceilingz)
            thing->pos[VZ] = thing->ceilingz - thing->height;
    }

    if((thing->ceilingz - thing->floorz) >= thing->height)
        return true;
    else
        return false;
}

/**
 * Allows the player to slide along any angled walls by adjusting the
 * xmove / ymove so that the NEXT move will slide along the wall.
 *
 * @param ld            The line being slid along.
 */
static void P_HitSlideLine(line_t *ld)
{
    int         side;
    angle_t     lineangle, moveangle, deltaangle;
    float       movelen, newlen;

    if(P_GetIntp(ld, DMU_SLOPE_TYPE) == ST_HORIZONTAL)
    {
        tmmove[MY] = 0;
        return;
    }
    else if(P_GetIntp(ld, DMU_SLOPE_TYPE) == ST_VERTICAL)
    {
        tmmove[MX] = 0;
        return;
    }

    side = P_PointOnLineSide(slidemo->pos[VX], slidemo->pos[VY], ld);

    lineangle =
        R_PointToAngle2(0, 0, P_GetFloatp(ld, DMU_DX), P_GetFloatp(ld, DMU_DY));

    if(side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2(0, 0, tmmove[MX], tmmove[MY]);
    deltaangle = moveangle - lineangle;

    if(deltaangle > ANG180)
        deltaangle += ANG180;

    lineangle  >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_ApproxDistance(tmmove[MX], tmmove[MY]);
    newlen = movelen * FIX2FLT(finecosine[deltaangle]);

    tmmove[MX] = newlen * FIX2FLT(finecosine[lineangle]);
    tmmove[MY] = newlen * FIX2FLT(finesine[lineangle]);
}

static boolean PTR_SlideTraverse(intercept_t *in)
{
    line_t     *li;

    if(!in->isaline)
        Con_Error("PTR_SlideTraverse: not a line?");

    li = in->d.line;
    if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
    {
        if(P_PointOnLineSide(slidemo->pos[VX], slidemo->pos[VY], li))
            return true; // Don't hit the back side.

        goto isblocking;
    }

#if __DOOM64TC__
    if(P_GetIntp(li, DMU_FLAGS) & ML_BLOCKALL) // d64tc
        goto isblocking;
#endif

    P_LineOpening(li); // Set openrange, opentop, openbottom...

    if(openrange < slidemo->height)
        goto isblocking; // Doesn't fit.

    if(opentop - slidemo->pos[VZ] < slidemo->height)
        goto isblocking; // mobj is too high.

    if(openbottom - slidemo->pos[VZ] > 24)
        goto isblocking; // Too big a step up.

    // This line doesn't block movement.
    return true;

    // The line does block movement, see if it is closer than best so far.
  isblocking:
    if(in->frac < bestslidefrac)
    {
        secondslidefrac = bestslidefrac;
        secondslideline = bestslideline;
        bestslidefrac = in->frac;
        bestslideline = li;
    }

    return false; // Stop.
}

/**
 * \fixme The momx / momy move is bad, so try to slide along a wall.
 * Find the first line hit, move flush to it, and slide along it
 *
 * This is a kludgy mess.
 *
 * @param mo            The mobj to attempt the slide move.
 */
void P_SlideMove(mobj_t *mo)
{
    int         hitcount;
    float       leadpos[3], trailpos[3], newPos[3];

    slidemo = mo;
    hitcount = 0;

  retry:
    if(++hitcount == 3)
        goto stairstep; // don't loop forever

    // trace along the three leading corners
    memcpy(leadpos, mo->pos, sizeof(leadpos));
    memcpy(trailpos, mo->pos, sizeof(trailpos));
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

    bestslidefrac = 1;

    P_PathTraverse(leadpos[VX], leadpos[VY],
                   leadpos[VX] + mo->mom[MX], leadpos[VY] + mo->mom[MY],
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(trailpos[VX], leadpos[VY],
                   trailpos[VX] + mo->mom[MX], leadpos[VY] + mo->mom[MY],
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(leadpos[VX], trailpos[VY],
                   leadpos[VX] + mo->mom[MX], trailpos[VY] + mo->mom[MY],
                   PT_ADDLINES, PTR_SlideTraverse);

    // Move up to the wall.
    if(bestslidefrac == 1)
    {
        // The move must have hit the middle, so stairstep.
      stairstep:
        // killough $dropoff_fix
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
        return;
    }

    // Fudge a bit to make sure it doesn't hit.
    bestslidefrac -= (1.0f / 32);
    if(bestslidefrac > 0)
    {
        newPos[VX] = mo->mom[MX] * bestslidefrac;
        newPos[VY] = mo->mom[MY] * bestslidefrac;
        newPos[VZ] = DDMAXFLOAT; // Just initialize with *something*.

        // killough $dropoff_fix
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
    bestslidefrac = 1 - (bestslidefrac + (1.0f / 32));
    if(bestslidefrac > 1)
        bestslidefrac = 1;
    if(bestslidefrac <= 0)
        return;

    tmmove[MX] = mo->mom[MX] * bestslidefrac;
    tmmove[MY] = mo->mom[MY] * bestslidefrac;

    P_HitSlideLine(bestslideline); // Clip the move.

    mo->mom[MX] = tmmove[MX];
    mo->mom[MY] = tmmove[MY];

    // killough $dropoff_fix
#if __JHEXEN__
    if(!P_TryMove(mo, mo->pos[VX] + tmmove[MX], mo->pos[VY] + tmmove[MY]))
#else
    if(!P_TryMove(mo, mo->pos[VX] + tmmove[MX], mo->pos[VY] + tmmove[MY], true, true))
#endif
    {
        goto retry;
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

/*
 * @param thing         The thing to check against height changes.
 * @param data          Unused.
 */
static boolean PIT_ChangeSector(mobj_t *thing, void *data)
{
    mobj_t     *mo;

    // Don't check things that aren't blocklinked (supposedly immaterial).
    if(thing->flags & MF_NOBLOCKMAP)
        return true;

    if(P_ThingHeightClip(thing))
        return true; // Keep checking...

    // Crunch bodies to giblets.
#if __JDOOM__
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
            P_RemoveMobj(thing);
        }
        else
        {
            if(thing->state != &states[S_GIBS1])
            {
                P_SetMobjState(thing, S_GIBS1);
                thing->height = 0;
                thing->radius = 0;
                S_StartSound(SFX_PLAYER_FALLING_SPLAT, thing);
            }
        }
#else
# if __DOOM64TC__
        //// \fixme kaiser - the def file is too fucked up..
        //// DJS - FIXME!
        P_SetMobjState(thing, S_HEADCANDLES + 3);
        S_StartSound(sfx_slop, thing);
# elif __JDOOM__
        P_SetMobjState(thing, S_GIBS);
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
        P_RemoveMobj(thing);
        return true; // Keep checking...
    }

    if(!(thing->flags & MF_SHOOTABLE))
        return true; // Keep checking...

    nofit = true;
    if(crushchange && !(leveltime & 3))
    {
        P_DamageMobj(thing, NULL, NULL, 10);
#if __JDOOM__
        if(!(thing->flags & MF_NOBLOOD))
#elif __JHEXEN__
        if(!(thing->flags & MF_NOBLOOD) &&
           !(thing->flags2 & MF2_INVULNERABLE))
#endif
        {
            // Spray blood in a random direction.
            mo = P_SpawnMobj3f(MT_BLOOD, thing->pos[VX], thing->pos[VY],
                               thing->pos[VZ] + (thing->height /2));

            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 12);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 12);
        }
    }

    return true; // Keep checking (crush other things)...
}

/**
 * @param sector        The sector to check.
 * @param crunch        @c true = crush any things in the sector.
 */
boolean P_ChangeSector(sector_t *sector, boolean crunch)
{
    nofit = false;
    crushchange = crunch;

    VALIDCOUNT++;
    P_SectorTouchingMobjsIterator(sector, PIT_ChangeSector, 0);

    return nofit;
}

/*
 * The following routines originate from the Heretic src.
 */

#if __JHERETIC__ || __JHEXEN__
/**
 * @param mobj          The mobj whoose position to test.
 * @return boolean      @c true, if the mobj is not blocked by anything.
 */
boolean P_TestMobjLocation(mobj_t *mobj)
{
    int         flags;

    flags = mobj->flags;
    mobj->flags &= ~MF_PICKUP;

    if(P_CheckPosition2f(mobj, mobj->pos[VX], mobj->pos[VY]))
    {
        // XY is ok, now check Z
        mobj->flags = flags;
        if((mobj->pos[VZ] < mobj->floorz) ||
           (mobj->pos[VZ] + mobj->height > mobj->ceilingz))
        {
            return false; // Bad Z
        }

        return true;
    }

    mobj->flags = flags;
    return false;
}
#endif

#if __DOOM64TC__ || __JHERETIC__
static void CheckMissileImpact(mobj_t *mobj)
{
    line_t     *ld;
    int         size = P_IterListSize(spechit);

    if(!size || !(mobj->flags & MF_MISSILE) || !mobj->target)
        return;

    if(!mobj->target->player)
        return;

    P_IterListResetIterator(spechit, false);
    while((ld = P_IterListIterator(spechit)) != NULL)
        P_ActivateLine(ld, mobj->target, 0, SPAC_IMPACT);
}
#endif

/*
 * The following routines originate from the Hexen src
 */

#if __JHEXEN__
static boolean PIT_ThrustStompThing(mobj_t *thing, void *data)
{
    float       blockdist;

    if(!(thing->flags & MF_SHOOTABLE))
        return true;

    blockdist = thing->radius + tsthing->radius;
    if(fabs(thing->pos[VX] - tsthing->pos[VX]) >= blockdist ||
       fabs(thing->pos[VY] - tsthing->pos[VY]) >= blockdist ||
       (thing->pos[VZ] > tsthing->pos[VZ] + tsthing->height))
        return true; // Didn't hit it.

    if(thing == tsthing)
        return true; // Don't clip against self.

    P_DamageMobj(thing, tsthing, tsthing, 10001);
    tsthing->args[1] = 1; // Mark thrust thing as bloody.

    return true;
}

void PIT_ThrustSpike(mobj_t *actor)
{
    float       bbox[4];
    float       radius;

    tsthing = actor;
    radius = actor->info->radius + MAXRADIUS;

    bbox[BOXLEFT]   = bbox[BOXRIGHT] = actor->pos[VX];
    bbox[BOXBOTTOM] = bbox[BOXTOP]  = actor->pos[VY];

    bbox[BOXLEFT]   -= radius;
    bbox[BOXRIGHT]  += radius;
    bbox[BOXBOTTOM] -= radius;
    bbox[BOXTOP]    += radius;

    // Stomp on any things contacted.
    P_MobjsBoxIterator(bbox, PIT_ThrustStompThing, 0);
}

static boolean PIT_CheckOnmobjZ(mobj_t *thing, void *data)
{
    float       blockdist;

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)))
        return true; // Can't hit thing.

    blockdist = thing->radius + tmthing->radius;
    if(fabs(thing->pos[VX] - tm[VX]) >= blockdist ||
       fabs(thing->pos[VY] - tm[VY]) >= blockdist)
        return true; // Didn't hit thing.

    if(thing == tmthing)
        return true; // Don't clip against self.

    if(tmthing->pos[VZ] > thing->pos[VZ] + thing->height)
        return true;
    else if(tmthing->pos[VZ] + tmthing->height < thing->pos[VZ])
        return true; // Under thing.

    if(thing->flags & MF_SOLID)
        onmobj = thing;

    return (!(thing->flags & MF_SOLID));
}

mobj_t *P_CheckOnMobj(mobj_t *thing)
{
    subsector_t *newsubsec;
    float       pos[3], box[4];
    mobj_t      oldmo;

    memcpy(pos, thing->pos, sizeof(pos));

    tmthing = thing;

    //// \fixme Do this properly!
    oldmo = *thing; // Save the old mobj before the fake zmovement

    P_FakeZMovement(tmthing);

    tm[VX] = pos[VX];
    tm[VY] = pos[VY];
    tm[VZ] = pos[VZ];

    tmbbox[BOXTOP]    = pos[VY] + tmthing->radius;
    tmbbox[BOXBOTTOM] = pos[VY] - tmthing->radius;
    tmbbox[BOXRIGHT]  = pos[VX] + tmthing->radius;
    tmbbox[BOXLEFT]   = pos[VX] - tmthing->radius;

    newsubsec = R_PointInSubsector(pos[VX], pos[VY]);
    ceilingline = NULL;

    // The base floor/ceiling is from the subsector that contains the
    // point.  Any contacted lines the step closer together will adjust them.

    tmfloorz = tmdropoffz = P_GetFloatp(newsubsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFloatp(newsubsec, DMU_CEILING_HEIGHT);
    tmfloorpic = P_GetIntp(newsubsec, DMU_FLOOR_MATERIAL);

    VALIDCOUNT++;
    P_EmptyIterList(spechit);

    if(tmthing->flags & MF_NOCLIP)
        return NULL;

    // Check things first, possibly picking things up the bounding box is extended
    // by MAXRADIUS because mobj_ts are grouped into mapblocks based on their origin
    // point, and can overlap into adjacent blocks by up to MAXRADIUS.

    box[BOXLEFT]   = tmbbox[BOXLEFT]   - MAXRADIUS;
    box[BOXRIGHT]  = tmbbox[BOXRIGHT]  + MAXRADIUS;
    box[BOXBOTTOM] = tmbbox[BOXBOTTOM] - MAXRADIUS;
    box[BOXTOP]    = tmbbox[BOXTOP]    + MAXRADIUS;

    if(!P_MobjsBoxIterator(box, PIT_CheckOnmobjZ, 0))
    {
        *tmthing = oldmo;
        return onmobj;
    }

    *tmthing = oldmo;

    return NULL;
}

/**
 * Fake the zmovement so that we can check if a move is legal
 */
static void P_FakeZMovement(mobj_t *mo)
{
    float       dist;
    float       delta;

    if(P_IsCamera(mo))
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

    if(mo->player && (mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorz) &&
       (leveltime & 2))
    {
        mo->pos[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * leveltime >> 2) & FINEMASK]);
    }

    // Clip movement.
    if(mo->pos[VZ] <= mo->floorz) // Hit the floor.
    {
        mo->pos[VZ] = mo->floorz;
        if(mo->mom[MZ] < 0)
            mo->mom[MZ] = 0;

        if(mo->flags & MF_SKULLFLY)
            mo->mom[MZ] = -mo->mom[MZ]; // The skull slammed into something

        if(mo->info->crashstate && (mo->flags & MF_CORPSE))
            return;
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -(P_GetGravity() / 32) * 2;
        else
            mo->mom[MZ] -= P_GetGravity() / 32;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -P_GetGravity() * 2;
        else
            mo->mom[MZ] -= P_GetGravity();
    }

    if(mo->pos[VZ] + mo->height > mo->ceilingz) // Hit the ceiling.
    {
        mo->pos[VZ] = mo->ceilingz - mo->height;

        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        if(mo->flags & MF_SKULLFLY)
            mo->mom[MZ] = -mo->mom[MZ]; // The skull slammed into something.
    }
}

static void CheckForPushSpecial(line_t *line, int side, mobj_t *mobj)
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

static boolean PTR_BounceTraverse(intercept_t *in)
{
    line_t     *li;

    if(!in->isaline)
        Con_Error("PTR_BounceTraverse: not a line?");

    li = in->d.line;
    if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
    {
        if(P_PointOnLineSide(slidemo->pos[VX], slidemo->pos[VY], li))
            return true; // Don't hit the back side.
        goto bounceblocking;
    }

    P_LineOpening(li); // Set openrange, opentop, openbottom...

    if(openrange < slidemo->height)
        goto bounceblocking; // Doesn't fit.

    if(opentop - slidemo->pos[VZ] < slidemo->height)
        goto bounceblocking; // Mobj is too high...

    return true; // this line doesn't block movement...

    // the line does block movement, see if it is closer than best so far
  bounceblocking:
    if(in->frac < bestslidefrac)
    {
        secondslidefrac = bestslidefrac;
        secondslideline = bestslideline;
        bestslidefrac = in->frac;
        bestslideline = li;
    }

    return false; // Stop.
}

void P_BounceWall(mobj_t *mo)
{
    int         side;
    float       movelen, leadPos[3];
    angle_t     lineangle, moveangle, deltaangle;

    slidemo = mo;

    // trace along the three leading corners
    memcpy(leadPos, mo->pos, sizeof(leadPos));
    if(mo->mom[MX] > 0)
        leadPos[VX] += mo->radius;
    else
        leadPos[VX] -= mo->radius;

    if(mo->mom[MY] > 0)
        leadPos[VY] += mo->radius;
    else
        leadPos[VY] -= mo->radius;

    bestslidefrac = 1;
    P_PathTraverse(leadPos[VX], leadPos[VY],
                   leadPos[VX] + mo->mom[MX], leadPos[VY] + mo->mom[MY],
                   PT_ADDLINES, PTR_BounceTraverse);

    if(!bestslideline)
        return; // We don't want to crash.

    side = P_PointOnLineSide(mo->pos[VX], mo->pos[VY], bestslideline);
    lineangle = R_PointToAngle2(0, 0,
                                P_GetFloatp(bestslideline, DMU_DX),
                                P_GetFloatp(bestslideline, DMU_DY));

    if(side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2(0, 0, mo->mom[MX], mo->mom[MY]);
    deltaangle = (2 * lineangle) - moveangle;

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_ApproxDistance(mo->mom[MX], mo->mom[MY]);
    movelen *= 0.75f; // friction

    if(movelen < 1)
        movelen = 2;

    mo->mom[MX] = movelen * FIX2FLT(finecosine[deltaangle]);
    mo->mom[MY] = movelen * FIX2FLT(finesine[deltaangle]);
}

static boolean PTR_PuzzleItemTraverse(intercept_t *in)
{
    mobj_t     *mobj;
    int         sound;

    if(in->isaline)
    {   // Check line.
        if(P_ToXLine(in->d.line)->special != USE_PUZZLE_ITEM_SPECIAL)
        {
            P_LineOpening(in->d.line);
            if(openrange <= 0)
            {
                sound = SFX_NONE;
                if(PuzzleItemUser->player)
                {
                    switch(PuzzleItemUser->player->class)
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

                S_StartSound(sound, PuzzleItemUser);
                return false; // Can't use through a wall.
            }

            return true; // Continue searching...
        }

        if(P_PointOnLineSide(PuzzleItemUser->pos[VX],
                             PuzzleItemUser->pos[VY], in->d.line) == 1)
            return false; // Don't use back sides.

        if(PuzzleItemType != P_ToXLine(in->d.line)->arg1)
            return false; // Item type doesn't match.

        P_StartACS(P_ToXLine(in->d.line)->arg2, 0, &P_ToXLine(in->d.line)->arg3,
                   PuzzleItemUser, in->d.line, 0);
        P_ToXLine(in->d.line)->special = 0;
        PuzzleActivated = true;

        return false; // Stop searching.
    }

    // Check thing.
    mobj = in->d.mo;
    if(mobj->special != USE_PUZZLE_ITEM_SPECIAL)
        return true; // Wrong special...

    if(PuzzleItemType != mobj->args[0])
        return true; // Item type doesn't match...

    P_StartACS(mobj->args[1], 0, &mobj->args[2], PuzzleItemUser, NULL, 0);
    mobj->special = 0;
    PuzzleActivated = true;

    return false; // Stop searching.
}

/**
 * See if the specified player can use the specified puzzle item on a
 * thing or line(s) at their current world location.
 *
 * @param player        The player using the puzzle item.
 * @param itemType      The type of item to try to use.
 * @return boolean      true if the puzzle item was used.
 */
boolean P_UsePuzzleItem(player_t *player, int itemType)
{
    int         angle;
    float       pos1[3], pos2[3];

    PuzzleItemType = itemType;
    PuzzleItemUser = player->plr->mo;
    PuzzleActivated = false;

    angle = player->plr->mo->angle >> ANGLETOFINESHIFT;

    memcpy(pos1, player->plr->mo->pos, sizeof(pos1));
    memcpy(pos2, player->plr->mo->pos, sizeof(pos2));

    pos2[VX] += FIX2FLT(USERANGE * finecosine[angle]);
    pos2[VY] += FIX2FLT(USERANGE * finesine[angle]);

    P_PathTraverse(pos1[VX], pos1[VY], pos2[VX], pos2[VY],
                   PT_ADDLINES | PT_ADDMOBJS, PTR_PuzzleItemTraverse);

    return PuzzleActivated;
}
#endif
