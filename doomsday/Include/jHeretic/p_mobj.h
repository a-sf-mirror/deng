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
 * Map Objects, MObj, definition and handling.
 */

#ifndef __P_MOBJ__
#define __P_MOBJ__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

// We need the WAD data structure for Map things,
// from the THINGS lump.
#include "Doomdata.h"

// States are tied to finite states are
//  tied to animation frames.
// Needs precompiled tables/data structures.
#include "info.h"

#ifdef __GNUG__
#pragma interface
#endif

/*
 * NOTES: mobj_t
 *
 * mobj_ts are used to tell the refresh where to draw an image,
 * tell the world simulation when objects are contacted,
 * and tell the sound driver how to position a sound.
 *
 * The refresh uses the next and prev links to follow
 * lists of things in sectors as they are being drawn.
 * The sprite, frame, and angle elements determine which patch_t
 * is used to draw the sprite if it is visible.
 * The sprite and frame values are allmost allways set
 * from state_t structures.
 * The statescr.exe utility generates the states.h and states.c
 * files that contain the sprite/frame numbers from the
 * statescr.txt source file.
 * The xyz origin point represents a point at the bottom middle
 * of the sprite (between the feet of a biped).
 * This is the default origin position for patch_ts grabbed
 * with lumpy.exe.
 * A walking creature will have its z equal to the floor
 * it is standing on.
 *
 * The sound code uses the x,y, and subsector fields
 * to do stereo positioning of any sound effited by the mobj_t.
 *
 * The play simulation uses the blocklinks, x,y,z, radius, height
 * to determine when mobj_ts are touching each other,
 * touching lines in the map, or hit by trace lines (gunshots,
 * lines of sight, etc).
 * The mobj_t->flags element has various bit flags
 * used by the simulation.
 *
 * Every mobj_t is linked into a single sector
 * based on its origin coordinates.
 * The subsector_t is found with R_PointInSubsector(x,y),
 * and the sector_t can be found with subsector->sector.
 * The sector links are only used by the rendering code,
 * the play simulation does not care about them at all.
 *
 * Any mobj_t that needs to be acted upon by something else
 * in the play world (block movement, be shot, etc) will also
 * need to be linked into the blockmap.
 * If the thing has the MF_NOBLOCK flag set, it will not use
 * the block links. It can still interact with other things,
 * but only as the instigator (missiles will run into other
 * things, but nothing can run into a missile).
 * Each block in the grid is 128*128 units, and knows about
 * every line_t that it contains a piece of, and every
 * interactable mobj_t that has its origin contained.
 *
 * A valid mobj_t is a mobj_t that has the proper subsector_t
 * filled in for its xy coordinates and is linked into the
 * sector from which the subsector was made, or has the
 * MF_NOSECTOR flag set (the subsector_t needs to be valid
 * even if MF_NOSECTOR is set), and is linked into a blockmap
 * block or has the MF_NOBLOCKMAP flag set.
 * Links should only be modified by the P_[Un]SetThingPosition()
 * functions.
 * Do not change the MF_NO? flags while a thing is valid.
 *
 * Any questions?
 */

//
// Misc. mobj flags
//
#define MF_SPECIAL      1          // call P_SpecialThing when touched
#define MF_SOLID        2
#define MF_SHOOTABLE    4
#define MF_NOSECTOR     8          // don't use the sector links
                                 // (invisible but touchable)
#define MF_NOBLOCKMAP   16         // don't use the blocklinks
                                 // (inert but displayable)
#define MF_AMBUSH       32
#define MF_JUSTHIT      64         // try to attack right back
#define MF_JUSTATTACKED 128        // take at least one step before attacking
#define MF_SPAWNCEILING 256        // hang from ceiling instead of floor
#define MF_NOGRAVITY    512        // don't apply gravity every tic

// movement flags
#define MF_DROPOFF      0x400      // allow jumps from high places
#define MF_PICKUP       0x800      // for players to pick up items
#define MF_NOCLIP       0x1000     // player cheat
#define MF_SLIDE        0x2000     // keep info about sliding along walls
#define MF_FLOAT        0x4000     // allow moves to any height, no gravity
#define MF_TELEPORT     0x8000     // don't cross lines or look at heights
#define MF_MISSILE      0x10000    // don't hit same species, explode on block

#define MF_DROPPED      0x20000    // dropped by a demon, not level spawned
#define MF_SHADOW       0x40000    // use fuzzy draw (shadow demons / invis)
#define MF_NOBLOOD      0x80000    // don't bleed when shot (use puff)
#define MF_CORPSE       0x100000   // don't stop moving halfway off a step
#define MF_INFLOAT      0x200000   // floating to a height for a move, don't
                                 // auto float to target's height

#define MF_COUNTKILL    0x400000   // count towards intermission kill total
#define MF_COUNTITEM    0x800000   // count towards intermission item total

#define MF_SKULLFLY     0x1000000  // skull in flight
#define MF_NOTDMATCH    0x2000000  // don't spawn in death match (key cards)

#define MF_TRANSLATION  0xc000000  // if 0x4 0x8 or 0xc, use a translation
#define MF_TRANSSHIFT   26         // table for player colormaps

#define MF_LOCAL            0x10000000  // Won't be sent to clients.
#define MF_BRIGHTSHADOW     0x20000000
#define MF_BRIGHTEXPLODE    0x40000000  // Make this brightshadow when exploding.
#define MF_VIEWALIGN        0x80000000

// --- mobj.flags2 ---

#define MF2_LOGRAV          0x00000001  // alternate gravity setting
#define MF2_WINDTHRUST      0x00000002  // gets pushed around by the wind
                                     // specials
#define MF2_FLOORBOUNCE     0x00000004  // bounces off the floor
#define MF2_THRUGHOST       0x00000008  // missile will pass through ghosts
#define MF2_FLY             0x00000010  // fly mode is active
#define MF2_FOOTCLIP        0x00000020  // if feet are allowed to be clipped
#define MF2_SPAWNFLOAT      0x00000040  // spawn random float z
#define MF2_NOTELEPORT      0x00000080  // does not teleport
#define MF2_RIP             0x00000100  // missile rips through solid
                                     // targets
#define MF2_PUSHABLE        0x00000200  // can be pushed by other moving
                                     // mobjs
#define MF2_SLIDE           0x00000400  // slides against walls
#define MF2_ONMOBJ          0x00000800  // mobj is resting on top of another
                                     // mobj
#define MF2_PASSMOBJ        0x00001000  // Enable z block checking.  If on,
                                     // this flag will allow the mobj to
                                     // pass over/under other mobjs.
#define MF2_CANNOTPUSH      0x00002000  // cannot push other pushable mobjs
#define MF2_FEETARECLIPPED  0x00004000  // a mobj's feet are now being cut
#define MF2_BOSS            0x00008000  // mobj is a major boss
#define MF2_FIREDAMAGE      0x00010000  // does fire damage
#define MF2_NODMGTHRUST     0x00020000  // does not thrust target when
                                     // damaging
#define MF2_TELESTOMP       0x00040000  // mobj can stomp another
#define MF2_FLOATBOB        0x00080000  // use float bobbing z movement
#define MF2_DONTDRAW        0X00100000  // don't generate a vissprite


// Map Object definition.
typedef struct mobj_s {
    // Defined in dd_share.h; required mobj elements.
    DD_BASE_MOBJ_ELEMENTS() mobjinfo_t *info;   // &mobjinfo[mobj->type]
    int             damage;        // For missiles
    int             flags;
    int             flags2;        // Heretic flags
    int             special1;      // Special info
    int             special2;      // Special info
    int             health;
    int             movedir;       // 0-7
    int             movecount;     // when 0, select a new dir
    struct mobj_s  *target;        // thing being chased/attacked (or NULL)
    // also the originator for missiles
    int             reactiontime;  // if non 0, don't attack yet
    // used by player to freeze a bit after
    // teleporting
    int             threshold;     // if >0, the target will be chased
    // no matter what (even if shot)
    struct player_s *player;       // only valid if type == MT_PLAYER
    int             lastlook;      // player number last looked for

    thing_t         spawnpoint;    // for nightmare respawn
    int             turntime;      // $visangle-facetarget
    int             corpsetics;    // $vanish: how long has this been dead?
} mobj_t;

#endif
