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

#ifndef __D_PLAYER__
#define __D_PLAYER__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "d_items.h"
#include "p_pspr.h"
#include "p_mobj.h"
#include "g_controls.h"

//
// Player states.
//
typedef enum {
    PST_LIVE, // Playing or camping.
    PST_DEAD, // Dead on the ground, view follows killer.
    PST_REBORN // Ready to restart/respawn???
} playerstate_t;

//
// Player internal flags, for cheats and debug.
//
typedef enum {
    CF_NOCLIP = 0x1, // No clipping, walk through barriers.
    CF_GODMODE = 0x2, // No damage, no health loss.
    CF_NOMOMENTUM = 0x4 // Not really a cheat, just a debug aid.
} cheat_t;

typedef struct player_s {
    ddplayer_t*     plr; // Pointer to the engine's player data.
    playerstate_t   playerState;
    playerclass_t   class; // Player class type.
    playerbrain_t   brain;

    int             health; // This is only used between levels, mo->health is used during levels.
    int             armorPoints;
    int             armorType; // Armor type is 0-2.
    int             powers[NUM_POWER_TYPES]; // Power ups. invinc and invis are tic counters.
    boolean         keys[NUM_KEY_TYPES];
    boolean         backpack;
    int             frags[MAXPLAYERS];
    weapontype_t    readyWeapon;
    weapontype_t    pendingWeapon; // Is wp_nochange if not changing.
    struct playerweapon_s {
        boolean         owned;
    } weapons[NUM_WEAPON_TYPES];
    struct playerammo_s {
        int             owned;
        int             max;
    } ammo[NUM_AMMO_TYPES];

    int             attackDown; // True if button down last tic.
    int             useDown;

    int             cheats; // Bit flags, for cheats and debug, see cheat_t, above.
    int             refire; // Refired shots are less accurate.

    // For intermission stats:
    int             killCount;
    int             itemCount;
    int             secretCount;

    // For screen flashing (red or bright):
    int             damageCount;
    int             bonusCount;

    mobj_t*         attacker; // Who did damage (NULL for floors/ceilings).
    int             colorMap; // Player skin colorshift, 0-3 for which color to draw player.
    pspdef_t        pSprites[NUMPSPRITES]; // Overlay view sprites (gun, etc).

    boolean         didSecret; // True if secret level has been done.
    boolean         secretExit;

    int             jumpTics; // The player can jump if this counter is zero.
    int             airCounter;
    int             flyHeight;
    int             rebornWait; // The player can be reborn if this counter is zero.
    boolean         centering; // The player's view pitch is centering back to zero.
    int             update, startSpot;

    float           viewOffset[3]; // Relative to position of the player mobj.
    float           viewZ; // Focal origin above r.z.
    float           viewHeight; // Base height above floor for viewZ.
    float           viewHeightDelta;
    float           bob; // Bounded/scaled total momentum.

    // Target view to a mobj (NULL=disabled):
    mobj_t*         viewLock; // $democam
    int             lockFull;
} player_t;

//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
typedef struct {
    boolean         inGame; // Whether the player is in game.

    // Player stats, kills, collected items etc.
    int             kills;
    int             items;
    int             secret;
    int             time;
    int             frags[MAXPLAYERS];
    int             score; // Current score on entry, modified on return.
} wbplayerstruct_t;

typedef struct {
    int             epsd; // Episode # (0-2)
    boolean         didSecret; // If true, splash the secret level.
    int             last, next; // Previous and next levels, origin 0.
    int             maxKills;
    int             maxItems;
    int             maxSecret;
    int             maxFrags;
    int             parTime;
    int             pNum; // Index of this player in game.
    wbplayerstruct_t plyr[MAXPLAYERS];
} wbstartstruct_t;

#endif
