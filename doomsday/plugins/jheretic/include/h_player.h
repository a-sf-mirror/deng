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
 * h_player.h:
 */

#ifndef __JHERETIC_PLAYER_H__
#define __JHERETIC_PLAYER_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "h_items.h"
#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "p_mobj.h"

#include "g_controls.h"

#ifdef __GNUG__
#pragma interface
#endif

//
// Player states.
//
typedef enum {
    // Playing or camping.
    PST_LIVE,
    // Dead on the ground, view follows killer.
    PST_DEAD,
    // Ready to restart/respawn???
    PST_REBORN
} playerstate_t;

//
// Player internal flags, for cheats and debug.
//
typedef enum {
    // No clipping, walk through barriers.
    CF_NOCLIP = 1,
    // No damage, no health loss.
    CF_GODMODE = 2,
    // Not really a cheat, just a debug aid.
    CF_NOMOMENTUM = 4
} cheat_t;

typedef struct player_s {
    ddplayer_t*     plr; // Pointer to the engine's player data.
    playerstate_t   playerState;
    playerclass_t   class; // Player class.
    playerbrain_t   brain;

    // This is only used between levels,
    // mo->health is used during levels.
    int             health;
    int             armorPoints;
    // Armor type is 0-2.
    int             armorType;

    // Power ups. invinc and invis are tic counters.
    int             powers[NUM_POWER_TYPES];
    boolean         keys[NUM_KEY_TYPES];
    boolean         backpack;

    signed int      frags[MAXPLAYERS];

    weapontype_t    readyWeapon;
    weapontype_t    pendingWeapon; // Is wp_nochange if not changing.

    struct playerweapon_s {
        boolean         owned;
    } weapons[NUM_WEAPON_TYPES];
    struct playerammo_s {
        int             owned;
        int             max;
    } ammo[NUM_AMMO_TYPES];

    // true if button down last tic
    int             attackDown;
    int             useDown;

    // Bit flags, for cheats and debug, see cheat_t, above.
    int             cheats;

    // Refired shots are less accurate.
    int             refire;

    // For intermission stats.
    int             killCount;
    int             itemCount;
    int             secretCount;

    // For screen flashing (red or bright).
    int             damageCount;
    int             bonusCount;

    // Who did damage (NULL for floors/ceilings).
    mobj_t*         attacker;

    // Player skin colorshift, 0-3 for which color to draw player.
    int             colorMap;

    // Overlay view sprites (gun, etc).
    pspdef_t        pSprites[NUMPSPRITES];

    // True if secret level has been done.
    boolean         didSecret;
    boolean         secretExit;

    int             jumpTics; // The player can jump if this counter is zero.
    int             airCounter;
    int             rebornWait; // The player can be reborn if this counter is zero.
    boolean         centering; // The player's view pitch is centering back to zero.
    int             update, startSpot;

    float           viewOffset[3]; // Relative to position of the player mobj.
    float           viewZ; // Focal origin above r.z.
    float           viewHeight; // Base height above floor for viewZ.
    float           viewHeightDelta;
    float           bob; // Bounded/scaled total momentum.

    // Target view to a mobj (NULL=disabled).
    mobj_t*         viewLock; // $democam
    int             lockFull;

    int             flyHeight;

    //
    // DJS - Here follows Heretic specific player_t properties
    //
    int             flameCount; // For flame thrower duration.

    int             morphTics; // player is a chicken if > 0.
    int             chickenPeck; // chicken peck countdown.
    mobj_t*         rain1; // Active rain maker 1.
    mobj_t*         rain2; // Active rain maker 2.
} player_t;

//
// INTERMISSION
// Structure passed e.g. to IN_Start(wb)
//
/*typedef struct {
    boolean         inGame; // Whether the player is in game.

    // Player stats, kills, collected items etc.
    int             kills;
    int             items;
    int             secret;
    int             time;
    int             frags[MAXPLAYERS];
    int             score; // Current score on entry, modified on return.
} wbplayerstruct_t;*/

typedef struct {
/*    int             epsd; // Episode # (0-2)*/
    boolean         didSecret; // If true, splash the secret level.
    int             last, next; // Previous and next levels, origin 0.
/*    int             maxKills;
    int             maxItems;
    int             maxSecret;
    int             maxFrags;
    int             parTime;
    int             pNum; // Index of this player in game.
    wbplayerstruct_t plyr[MAXPLAYERS];*/
} wbstartstruct_t;
#endif
