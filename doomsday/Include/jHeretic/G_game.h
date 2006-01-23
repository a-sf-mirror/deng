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

#ifndef __JHERETIC_GAME_H__
#define __JHERETIC_GAME_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "jHeretic/Doomdef.h"
#include "jHeretic/h_event.h"
#include "jHeretic/h_player.h"

//
// GAME
//
void            G_Register(void);

void            G_DeathMatchSpawnPlayer(int playernum);

void            G_InitNew(skill_t skill, int episode, int map);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void            G_DeferedInitNew(skill_t skill, int episode, int map);

void            G_DeferedPlayDemo(char *demo);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel or W_EnterWorld.
void            G_LoadGame(char *name);

void            G_DoLoadGame(void);

// Called by M_Responder.
void            G_SaveGame(int slot, char *description);

// Only called by startup code.
void            G_RecordDemo(skill_t skill, int numplayers, int episode,
                             int map, char *name);

void            G_PlayDemo(char *name);
void            G_TimeDemo(char *name);
void            G_StopDemo(void);
void            G_DemoEnds(void);
void            G_DemoAborted(void);

void            G_DoReborn(int playernum);
void            G_ExitLevel(void);
void            G_SecretExitLevel(void);

void            G_WorldDone(void);

void            G_Ticker(void);
boolean         G_Responder(event_t *ev);

void            G_ScreenShot(void);

void            G_ReadDemoTiccmd(player_t *pl);
void            G_WriteDemoTiccmd(ticcmd_t * cmd);
void            G_SpecialButton(player_t *pl);
#endif
