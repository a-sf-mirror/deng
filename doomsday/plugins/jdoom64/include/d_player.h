/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * d_player.h: Player data structures.
 */

#ifndef __D_PLAYER_H__
#define __D_PLAYER_H__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

// The player data structure depends on a number of other structs: items
// (internal inventory), animation states (closely tied to the sprites used
// to represent them, unfortunately).
#include "d_items.h"
#include "p_pspr.h"

// In addition, the player is just a special case of the generic moving
// object/actor.
#include "p_mobj.h"

#include "g_controls.h"

#ifdef __GNUG__
#pragma interface
#endif

/**
 * Player states.
 */
typedef enum {
    PST_LIVE, // Playing or camping.
    PST_DEAD, // Dead on the ground, view follows killer.
    PST_REBORN // Ready to restart/respawn???
} playerstate_t;

/**
 * Flags for cheats currently activated by player:
 */
#define CF_NOCLIP           (1) // No clipping, walk through barriers.
#define CF_GODMODE          (2) // No damage, no health loss.
#define CF_NOMOMENTUM       (4) // Not really a cheat, just a debug aid.

typedef struct player_s {
    ddplayer_t*     plr; // Pointer to the engine's player data.
    playerstate_t   playerState;
    playerclass_t   class; // player class type
    playerbrain_t   brain;

    // This is only used between maps, mo->health is used during maps.
    int             health;
    int             armorPoints;
    // Armor type is 0-2.
    int             armorType;

    // Power ups. invinc and invis are tic counters.
    int             powers[NUM_POWER_TYPES];
    boolean         keys[NUM_KEY_TYPES];
    boolean         backpack;

    int             frags[MAXPLAYERS];
    weapontype_t    readyWeapon;

    // Is wp_nochange if not changing.
    weapontype_t    pendingWeapon;

    struct playerweapon_s {
        boolean         owned;
    } weapons[NUM_WEAPON_TYPES];
    struct playerammo_s {
        int             owned;
        int             max;
    } ammo[NUM_AMMO_TYPES];

    // True if button down last tic.
    int             attackDown;
    int             useDown;

    // Bit flags, for cheats and debug. See cheat_t, above.
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

    // True if secret map has been done.
    boolean         didSecret;

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
} player_t;

#endif
