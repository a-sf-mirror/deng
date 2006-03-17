/* $Id: h_api.c 2678 2006-01-23 14:44:26Z danij $
 * Copyright (C) 2003, 2004 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * x_api.c : Doomsday API setup and interaction - jHexen specific
 */

// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "x_config.h"
#include "AcFnLink.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

/*
 *  jHexen's entry points
 */

// Initialization
void    H2_PreInit(void);
void    H2_PostInit(void);
void    R_InitTranslationTables(void);

// Timeing loop
void    H2_Ticker(void);

// Drawing
void    D_Display(void);
void    H2_EndFrame(void);
void    M_Drawer(void);
void    G_Drawer(void);

// Input responders
int     D_PrivilegedResponder(event_t *event);
boolean G_Responder(event_t *ev);
int     D_PrivilegedResponder(event_t *event);

// Ticcmd
void    G_BuildTiccmd(ticcmd_t *cmd, float elapsedTime);
void    G_MergeTiccmd(ticcmd_t *dest, ticcmd_t *src);

// Map Data
void    P_SetupForThings(int num);
void    P_SetupForLines(int num);
void    P_SetupForSides(int num);
void    P_SetupForSectors(int num);

// Map Objects
fixed_t P_GetMobjFriction(struct mobj_t *mo);
void    P_MobjThinker(mobj_t *mobj);
void    P_BlasterMobjThinker(mobj_t *mobj);

// Misc
void    H2_ConsoleBg(int *width, int *height);

// Game state changes
void    G_UpdateState(int step);

// Network
int     D_NetServerStarted(int before);
int     D_NetServerClose(int before);
int     D_NetConnect(int before);
int     D_NetDisconnect(int before);

long int D_NetPlayerEvent(int plrNumber, int peType, void *data);
int     D_NetWorldEvent(int type, int parm, void *data);

// Handlers
void    D_HandlePacket(int fromplayer, int type, void *data, int length);
int     P_HandleMapDataProperty(int id, int dtype, int prop, int type, void *data);
int     P_HandleMapDataPropertyValue(int id, int dtype, int prop, int type, void *data);

// Shutdown
void    H2_Shutdown(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern char gameModeString[];
extern char gameConfigString[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The interface to the Doomsday engine.
game_export_t gx;
game_import_t gi;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

char *G_Get(int id)
{
    switch (id)
    {
    case DD_GAME_NAME:
        return GAMENAMETEXT;

    case DD_GAME_ID:
        return GAMENAMETEXT " " VERSION_TEXT;

    case DD_GAME_MODE:
        return gameModeString;

    case DD_GAME_CONFIG:
        return gameConfigString;

    case DD_VERSION_SHORT:
        return VERSION_TEXT;

    case DD_VERSION_LONG:
        return VERSIONTEXT
            "\n"GAMENAMETEXT" is based on Hexen v1.1 by Raven Software.";

    case DD_ACTION_LINK:
        return (char *) actionlinks;

    case DD_PSPRITE_BOB_X:
        if(players[consoleplayer].morphTics > 0)
            return 0;
        return (char *) (FRACUNIT +
                         FixedMul(FixedMul
                                  (FRACUNIT * cfg.bobWeapon,
                                   players[consoleplayer].bob),
                                  finecosine[(128 * leveltime) & FINEMASK]));

    case DD_PSPRITE_BOB_Y:
        if(players[consoleplayer].morphTics > 0)
            return (char *) (32 * FRACUNIT);
        return (char *) (32 * FRACUNIT +
                         FixedMul(FixedMul
                                  (FRACUNIT * cfg.bobWeapon,
                                   players[consoleplayer].bob),
                                  finesine[(128 *
                                            leveltime) & FINEMASK & (FINEANGLES
                                                                     / 2 -
                                                                     1)]));

    case DD_ALT_MOBJ_THINKER:
        return (char *) P_BlasterMobjThinker;

    case DD_XGFUNC_LINK:
        return 0;

    }
    // ID not recognized, return NULL.
    return 0;
}

/*
 * Takes a copy of the engine's entry points and exported data. Returns
 * a pointer to the structure that contains our entry points and exports.
 */
game_export_t *GetGameAPI(game_import_t * imports)
{
    // Take a copy of the imports, but only copy as much data as is
    // allowed and legal.
    memset(&gi, 0, sizeof(gi));
    memcpy(&gi, imports, MIN_OF(sizeof(game_import_t), imports->apiSize));

    // Clear all of our exports.
    memset(&gx, 0, sizeof(gx));

    // Fill in the data for the exports.
    gx.apiSize = sizeof(gx);
    gx.PreInit = H2_PreInit;
    gx.PostInit = H2_PostInit;
    gx.Shutdown = H2_Shutdown;
    gx.BuildTicCmd = (void (*)(void*, float)) G_BuildTiccmd;
    gx.MergeTicCmd = (void (*)(void*, void*)) G_MergeTiccmd;
    gx.Ticker = H2_Ticker;
    gx.G_Drawer = G_Drawer;
    gx.MN_Drawer = M_Drawer;
    gx.PrivilegedResponder = (boolean (*)(event_t *)) D_PrivilegedResponder;
    gx.FallbackResponder = M_Responder;
    gx.G_Responder = G_Responder;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = (fixed_t (*)(void *)) P_GetMobjFriction;
    gx.EndFrame = H2_EndFrame;
    gx.ConsoleBackground = H2_ConsoleBg;
    gx.UpdateState = G_UpdateState;
#undef Get
    gx.Get = G_Get;

    gx.NetServerStart = D_NetServerStarted;
    gx.NetServerStop = D_NetServerClose;
    gx.NetConnect = D_NetConnect;
    gx.NetDisconnect = D_NetDisconnect;
    gx.NetPlayerEvent = D_NetPlayerEvent;
    gx.NetWorldEvent = D_NetWorldEvent;
    gx.HandlePacket = D_HandlePacket;

    // The structure sizes.
    gx.ticcmd_size = sizeof(ticcmd_t);

    gx.SetupForThings = P_SetupForThings;
    gx.SetupForLines = P_SetupForLines;
    gx.SetupForSides = P_SetupForSides;
    gx.SetupForSectors = P_SetupForSectors;

    // These two really need better names. Ideas?
    gx.HandleMapDataProperty = P_HandleMapDataProperty;
    gx.HandleMapDataPropertyValue = P_HandleMapDataPropertyValue;
    return &gx;
}
