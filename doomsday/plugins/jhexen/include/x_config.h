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
 * x_config.h:
 */

#ifndef __JHEXEN_CONFIG_H__
#define __JHEXEN_CONFIG_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

enum {
    HUD_MANA,
    HUD_HEALTH,
    HUD_CURRENTITEM
};

// Hud Unhide Events (the hud will unhide on these events if enabled).
typedef enum {
    HUE_FORCE = -1,
    HUE_ON_DAMAGE,
    HUE_ON_PICKUP_HEALTH,
    HUE_ON_PICKUP_ARMOR,
    HUE_ON_PICKUP_POWER,
    HUE_ON_PICKUP_WEAPON,
    HUE_ON_PICKUP_AMMO,
    HUE_ON_PICKUP_KEY,
    HUE_ON_PICKUP_INVITEM,
    NUMHUDUNHIDEEVENTS
} hueevent_t;

// This struct should be cleaned up. Currently some of the data isn't
// even used any more.

// WARNING: Do not use the boolean type. Its size can be either 1 or 4 bytes
//          depending on build settings.

typedef struct {
    float           playerMoveSpeed;
    float           lookSpeed;
    float           turnSpeed;
    int             quakeFly;
    byte            fastMonsters;
    int             useMLook, useJLook;
    int             screenBlocks;
    int             setBlocks;
    byte            hudShown[4]; // HUD data visibility.
    float           hudScale;
    float           hudColor[4];
    float           hudIconAlpha;
    float           hudTimer; // Number of seconds until the hud/statusbar auto-hides.
    byte            hudUnHide[NUMHUDUNHIDEEVENTS]; // when the hud/statusbar unhides.
    byte            usePatchReplacement;
    int             showFPS, lookSpring;
    int             mlookInverseY;
    int             echoMsg;
    int             translucentIceCorpse;

    byte            overrideHubMsg; // skip the transition hub message when 1
    int             cameraNoClip;
    float           bobView, bobWeapon;
    byte            askQuickSaveLoad;
    int             jumpEnabled;
    float           jumpPower;
    int             airborneMovement;
    int             useMouse, noAutoAim, alwaysRun;
    byte            povLookAround;
    int             jLookDeltaMode;

    int             xhair;
    float           xhairSize;
    byte            xhairVitality;
    float           xhairColor[4];

    int             statusbarScale;
    float           statusbarOpacity;
    float           statusbarCounterAlpha;

    int             msgCount;
    float           msgScale;
    float           msgUptime;
    int             msgBlink;
    int             msgAlign;
    byte            msgShow;
    float           msgColor[3];
    byte            weaponAutoSwitch;
    byte            noWeaponAutoSwitchIfFiring;
    byte            ammoAutoSwitch;
    int             weaponOrder[NUM_WEAPON_TYPES];
    byte            weaponNextMode; // if true use the weaponOrder for next/previous.
    float           filterStrength;
    byte            counterCheat;
    float           counterCheatScale;

    // Automap stuff.
/*    int             automapPos;
    float           automapWidth;
    float           automapHeight; */
    float           automapMobj[3];
    float           automapL0[3];
    float           automapL1[3];
    float           automapL2[3];
    float           automapL3[3];
    float           automapBack[3];
    float           automapOpacity;
    float           automapLineAlpha;
    byte            automapRotate;
    byte            automapHudDisplay;
    int             automapCustomColors;
    byte            automapShowDoors;
    float           automapDoorGlow;
    byte            automapBabyKeys;
    float           automapZoomSpeed;
    float           automapPanSpeed;
    byte            automapPanResetOnOpen;
    float           automapOpenSeconds;

    int             messagesOn;
    char*           chatMacros[10];
    byte            chatBeep;
    int             snd3D;
    float           sndReverbFactor;
    byte            reverbDebug;

    int             dclickUse;
    int             plrViewHeight;
    byte            mapTitle, hideIWADAuthor;
    float           menuScale;
    int             menuEffects;
    int             hudFog;
    float           menuGlitter;
    float           menuShadow;
    float           flashColor[3];
    int             flashSpeed;
    byte            turningSkull;
    float           menuColor[3];
    float           menuColor2[3];
    byte            menuSlam;
    byte            menuHotkeys;

    byte            netMap, netClass, netColor, netSkill;
    byte            netEpisode; // Unused in Hexen.
    byte            netDeathmatch, netNoMonsters, netRandomClass;
    byte            netJumping;
    byte            netMobDamageModifier; // Multiplier for non-player mobj damage.
    byte            netMobHealthModifier; // Health modifier for non-player mobjs.
    int             netGravity; // Multiplayer custom gravity.
    byte            netNoMaxZRadiusAttack; // Radius attacks are infinitely tall.
    byte            netNoMaxZMonsterMeleeAttack; // Melee attacks are infinitely tall.

    playerclass_t   playerClass[MAXPLAYERS];
    byte            playerColor[MAXPLAYERS];

    float           inventoryTimer; // Number of seconds until the invetory auto-hides.
    byte            inventoryWrap;
    byte            inventoryUseNext;
    byte            inventoryUseImmediate;
    int             inventorySlotMaxVis;
    byte            inventorySlotShowEmpty;
    byte            inventorySelectMode;
} game_config_t;

extern game_config_t cfg;

#endif
