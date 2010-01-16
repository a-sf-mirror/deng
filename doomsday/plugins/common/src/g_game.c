/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * g_game.c: Top-level (common) game routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

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
#include "p_saveg.h"
#include "g_controls.h"
#include "g_eventsequence.h"
#include "p_mapsetup.h"
#include "p_user.h"
#include "p_actor.h"
#include "p_tick.h"
#include "am_map.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_log.h"
#include "hu_msg.h"
#include "hu_pspr.h"
#include "g_common.h"
#include "g_update.h"
#include "d_net.h"
#include "x_hair.h"
#include "p_player.h"
#include "r_common.h"
#include "p_mapspec.h"
#include "f_infine.h"
#include "p_start.h"
#include "p_inventory.h"
#include "p_spec.h"
#if __JHERETIC__ || __JHEXEN__
#  include "hu_inventory.h"
#  include "p_inventory.h"
#endif

// MACROS ------------------------------------------------------------------

#define READONLYCVAR        CVF_READ_ONLY|CVF_NO_MAX|CVF_NO_MIN|CVF_NO_ARCHIVE

// TYPES -------------------------------------------------------------------

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
struct missileinfo_s {
    mobjtype_t  type;
    float       speed[2];
}
MonsterMissileInfo[] =
{
#if __JDOOM__ || __JDOOM64__
    {MT_BRUISERSHOT, {15, 20}},
    {MT_HEADSHOT, {10, 20}},
    {MT_TROOPSHOT, {10, 20}},
# if __JDOOM64__
    {MT_BRUISERSHOTRED, {15, 20}},
    {MT_NTROSHOT, {20, 40}},
# endif
#elif __JHERETIC__
    {MT_IMPBALL, {10, 20}},
    {MT_MUMMYFX1, {9, 18}},
    {MT_KNIGHTAXE, {9, 18}},
    {MT_REDAXE, {9, 18}},
    {MT_BEASTBALL, {12, 20}},
    {MT_WIZFX1, {18, 24}},
    {MT_SNAKEPRO_A, {14, 20}},
    {MT_SNAKEPRO_B, {14, 20}},
    {MT_HEADFX1, {13, 20}},
    {MT_HEADFX3, {10, 18}},
    {MT_MNTRFX1, {20, 26}},
    {MT_MNTRFX2, {14, 20}},
    {MT_SRCRFX1, {20, 28}},
    {MT_SOR2FX1, {20, 28}},
#endif
    {-1, {-1, -1}}                  // Terminator
};
#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdListMaps);

void    G_PlayerReborn(int player);
void    G_InitNew(skillmode_t skill, int episode, int map);
void    G_DoInitNew(void);
void    G_DoReborn(int playernum);
void    G_DoLoadMap(void);
void    G_DoNewGame(void);
void    G_DoLoadGame(void);
void    G_DoPlayDemo(void);
void    G_DoCompleted(void);
void    G_DoVictory(void);
void    G_DoWorldDone(void);
void    G_DoSaveGame(void);
void    G_DoScreenShot(void);
boolean G_ValidateMap(int *episode, int *map);

#if __JHEXEN__ || __JSTRIFE__
void    G_DoTeleportNewMap(void);
void    G_DoSingleReborn(void);
void    H2_PageTicker(void);
void    H2_AdvanceDemo(void);
#endif

void    G_StopDemo(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

game_config_t cfg; // The global cfg.

int debugSound; // Debug flag for displaying sound info.

skillmode_t gameSkill;
int gameEpisode;
int gameMap;
int nextMap; // If non zero this will be the next map.

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
boolean respawnMonsters;
wbstartstruct_t wmInfo; // Params for world map / intermission.
#endif

boolean paused;
boolean sendPause; // Send a pause event next tic.
boolean userGame; // Ok to save / end game.
boolean deathmatch; // Only if started as net death.
player_t players[MAXPLAYERS];

boolean singledemo; // Quit after playing a demo from cmdline.

boolean precache = true; // If @c true, load all graphics at start.

int saveGameSlot;
char saveDescription[HU_SAVESTRINGSIZE];

filename_t saveName;

static gamestate_t gameState = GS_DEMOSCREEN;

cvar_t gamestatusCVars[] = {
   {"game-state", READONLYCVAR, CVT_INT, &gameState, 0, 0},
   {"game-paused", READONLYCVAR, CVT_INT, &paused, 0, 0},
   {"game-skill", READONLYCVAR, CVT_INT, &gameSkill, 0, 0},
   {NULL}
};

ccmd_t gameCmds[] = {
    { "listmaps",    "",     CCmdListMaps },
    { NULL }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static skillmode_t dSkill;
static int dEpisode;
static int dMap;

#if __JHEXEN__
static int gameLoadSlot;
#endif

static gameaction_t gameAction;

static gamemap_t* theMap = NULL; // temp.

// CODE --------------------------------------------------------------------

void G_Register(void)
{
    int i;

    for(i = 0; gamestatusCVars[i].name; ++i)
        Con_AddVariable(gamestatusCVars + i);

    for(i = 0; gameCmds[i].name; ++i)
        Con_AddCommand(gameCmds + i);
}

void G_SetGameAction(gameaction_t action)
{
    if(gameAction != action)
        gameAction = action;
}

gameaction_t G_GetGameAction(void)
{
    return gameAction;
}

/**
 * Common Pre Engine Initialization routine.
 * Game-specfic pre init actions should be placed in eg D_PreInit() (for jDoom)
 */
void G_CommonPreInit(void)
{
    filename_t file;
    int i;

    // Make sure game.dll isn't newer than Doomsday...
    if(gi.version < DOOMSDAY_VERSION)
        Con_Error(GAME_NICENAME " requires at least Doomsday " DOOMSDAY_VERSION_TEXT
                  "!\n");

    verbose = ArgExists("-verbose");

    // Setup the players.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].plr = DD_GetPlayer(i);
        players[i].plr->extraData = (void *) &players[i];
    }

    dd_snprintf(file, FILENAME_T_MAXLEN, CONFIGFILE);
    DD_SetConfigFile(file);

    dd_snprintf(file, FILENAME_T_MAXLEN, DEFSFILE);
    DD_SetDefsFile(file);

    R_SetDataPath( DATAPATH );

    G_RegisterBindClasses();
    G_RegisterPlayerControls();
    P_RegisterMapObjs();

    // Add the cvars and ccmds to the console databases.
    G_ConsoleRegistration();    // Main command list.
    D_NetConsoleRegistration(); // For network.
    G_Register();               // Read-only game status cvars (for playsim).
    G_ControlRegister();        // For controls/input.
    AM_Register();              // For the automap.
    Hu_MenuRegister();          // For the menu.
    HU_Register();              // For the HUD displays.
    Hu_LogRegister();           // For the player message logs.
    Chat_Register();
    Hu_MsgRegister();           // For the game messages.
    ST_Register();              // For the hud/statusbar.
    X_Register();               // For the crosshair.

    DD_AddStartupWAD( STARTUPPK3 );
    G_DetectIWADs();
}

#if __JHEXEN__
/**
 * @todo all this swapping colors around is rather silly, why not simply
 * reorder the translation tables at load time?
 */
void R_GetTranslation(int plrClass, int plrColor, int* tclass, int* tmap)
{
    *tclass = 1;

    if(plrColor == 0)
        *tmap = 1;
    else if(plrColor == 1)
        *tmap = 0;
    else
        *tmap = plrColor;

    // Fighter's colors are a bit different.
    if(plrClass == PCLASS_FIGHTER && *tmap > 1)
        *tclass = 0;
}

void R_SetTranslation(mobj_t* mo)
{
    if(!(mo->flags & MF_TRANSLATION))
    {   // No translation.
        mo->tmap = mo->tclass = 0;
    }
    else
    {
        int                 tclass, tmap;

        tmap = (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;

        if(mo->player)
        {
            tclass = 1;

            if(mo->player->class == PCLASS_FIGHTER)
            {   // Fighter's colors are a bit different.
                if(tmap == 0)
                    tmap = 2;
                else if(tmap == 2)
                    tmap = 0;
                else
                    tclass = 0;
            }

            mo->tclass = tclass;
        }
        else
            tclass = mo->special1;

        mo->tmap = tmap;
        mo->tclass = tclass;
    }
}
#endif

void R_LoadColorPalettes(void)
{
#define PALLUMPNAME         "PLAYPAL"
#define PALENTRIES          (256)
#define PALID               (0)

    lumpnum_t lump = W_GetNumForName(PALLUMPNAME);
    byte data[PALENTRIES*3];

    W_ReadLumpSection(lump, data, 0 + PALID * (PALENTRIES * 3), PALENTRIES * 3);

    R_CreateColorPalette("R8G8B8", PALLUMPNAME, data, PALENTRIES);

    /**
     * Create the translation tables to map the green color ramp to gray,
     * brown, red.
     *
     * @note Assumes a given structure of the PLAYPAL. Could be read from a
     * lump instead?
     */
#if __JDOOM__ || __JDOOM64__
    {
    byte* translationtables = (byte*) DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);
    int i;

    // Translate just the 16 green colors.
    for(i = 0; i < 256; ++i)
    {
        if(i >= 0x70 && i <= 0x7f)
        {
            // Map green ramp to gray, brown, red.
            translationtables[i] = 0x60 + (i & 0xf);
            translationtables[i + 256] = 0x40 + (i & 0xf);
            translationtables[i + 512] = 0x20 + (i & 0xf);
        }
        else
        {
            // Keep all other colors as is.
            translationtables[i] = translationtables[i + 256] =
                translationtables[i + 512] = i;
        }
    }
    }
#elif __JHERETIC__
    {
    byte* translationtables = (byte*) DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);
    int i;

    // Fill out the translation tables.
    for(i = 0; i < 256; ++i)
    {
        if(i >= 225 && i <= 240)
        {
            translationtables[i] = 114 + (i - 225); // yellow
            translationtables[i + 256] = 145 + (i - 225); // red
            translationtables[i + 512] = 190 + (i - 225); // blue
        }
        else
        {
            translationtables[i] = translationtables[i + 256] =
                translationtables[i + 512] = i;
        }
    }
    }
#else // __JHEXEN__
    {
    byte* translationtables = (byte*) DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);
    int i;

    for(i = 0; i < 3 * 7; ++i)
    {
        char name[9];
        lumpnum_t lump;

        dd_snprintf(name, 9, "TRANTBL%X", i);

        if((lump = W_CheckNumForName(name)) != -1)
        {
            W_ReadLumpSection(lump, &translationtables[i * 256], 0, 256);
        }
    }
    }
#endif

#undef PALID
#undef PALENTRIES
#undef PALLUMPNAME
}

void R_InitRefresh(void)
{
    VERBOSE(Con_Message("R_InitRefresh: Loading data for referesh.\n"))

    R_LoadColorPalettes();
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    P_InitPicAnims();
#endif

    {
    float mul = 1.4f;
    DD_SetVariable(DD_PSPRITE_LIGHTLEVEL_MULTIPLIER, &mul);
    }
}

/**
 * Common Post Engine Initialization routine.
 * Game-specific post init actions should be placed in eg D_PostInit()
 * (for jDoom) and NOT here.
 */
void G_CommonPostInit(void)
{
    VERBOSE(G_PrintMapList());

    R_InitRefresh();

    // Init the save system and create the game save directory
    SV_Init();

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    XG_ReadTypes();
    XG_Register(); // Register XG classnames.
#endif

    R_SetViewSize(cfg.screenBlocks);
    R_SetBorderGfx(borderLumps);

    Con_Message("P_Init: Init Playloop state.\n");
    P_Init();

    Con_Message("Hu_LoadData: Setting up heads up display.\n");
    Hu_LoadData();
#if __JHERETIC__ || __JHEXEN__
    Hu_InventoryInit();
#endif

    Con_Message("ST_Init: Init status bar.\n");
    ST_Init();

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    Cht_Init();
#endif

    Con_Message("Hu_MenuInit: Init miscellaneous info.\n");
    Hu_MenuInit();
    Hu_MsgInit();

    // From this point on, the shortcuts are always active.
    DD_Execute(true, "activatebcontext shortcut");

    Con_Message("AM_Init: Init automap.\n");
    AM_Init();
}

/**
 * Retrieve the current game state.
 *
 * @return              The current game state.
 */
gamestate_t G_GetGameState(void)
{
    return gameState;
}

#if _DEBUG
static const char* getGameStateStr(gamestate_t state)
{
    struct statename_s {
        gamestate_t     state;
        const char*     name;
    } stateNames[] =
    {
        {GS_MAP, "GS_MAP"},
        {GS_INTERMISSION, "GS_INTERMISSION"},
        {GS_FINALE, "GS_FINALE"},
        {GS_DEMOSCREEN, "GS_DEMOSCREEN"},
        {GS_WAITING, "GS_WAITING"},
        {GS_INFINE, "GS_INFINE"},
        {-1, NULL}
    };
    uint i;

    for(i = 0; stateNames[i].name; ++i)
        if(stateNames[i].state == state)
            return stateNames[i].name;

    return NULL;
}
#endif

gamemap_t* P_CurrentGameMap(void)
{
    return theMap;
}

/**
 * Called when the gameui binding context is active. Triggers the menu.
 */
int G_UIResponder(event_t* ev)
{
    // Handle "Press any key to continue" messages.
    if(Hu_MsgResponder(ev))
        return true;

    if(!Hu_MenuIsActive())
    {
        // Any key/button down pops up menu if in demos.
        if(G_GetGameAction() == GA_NONE && !singledemo &&
           (Get(DD_PLAYBACK) || FI_IsMenuTrigger(ev)))
        {
            if(ev->state == EVS_DOWN &&
               (ev->type == EV_KEY || ev->type == EV_MOUSE_BUTTON ||
                ev->type == EV_JOY_BUTTON))
            {
                Hu_MenuCommand(MCMD_OPEN);
                return true;
            }
        }
    }

    return false;
}

/**
 * Change the game's state.
 *
 * @param state         The state to change to.
 */
void G_ChangeGameState(gamestate_t state)
{
    boolean gameUIActive = false;
    boolean gameActive = true;

    if(state < 0 || state >= NUM_GAME_STATES)
        Con_Error("G_ChangeGameState: Invalid state %i.\n", (int) state);

    if(gameState != state)
    {
#if _DEBUG
// Log gamestate changes in debug builds, with verbose.
VERBOSE(Con_Message("G_ChangeGameState: New state %s.\n",
                    getGameStateStr(state)));
#endif

        gameState = state;
    }

    // Update the state of the gameui binding context.
    switch(gameState)
    {
        case GS_FINALE:
        case GS_DEMOSCREEN:
        case GS_WAITING:
        case GS_INFINE:
            gameActive = false;
        case GS_INTERMISSION:
            gameUIActive = true;
            break;

        default:
            break;
    }

    if(gameUIActive)
    {
        DD_Execute(true, "activatebcontext gameui");
        B_SetContextFallback("gameui", G_UIResponder);
    }

    DD_Executef(true, "%sactivatebcontext game", gameActive? "" : "de");
}

/**
 * Begin the titlescreen animation sequence.
 */
void G_StartTitle(void)
{
    char               *name = "title";
    void               *script;

    G_StopDemo();
    userGame = false;

    // The title script must always be defined.
    if(!Def_Get(DD_DEF_FINALE, name, &script))
    {
        Con_Error("G_StartTitle: Script \"%s\" not defined.\n", name);
    }

    FI_Start(script, FIMODE_LOCAL);
}

void G_DoLoadMap(void)
{
#if __JHEXEN__
    static int firstFragReset = 1;
#endif

    gamemap_t* map = NULL;
    char mapID[9];
    int i;

    // If we're the server, let clients know the map will change.
    NetSv_SendGameState(GSF_CHANGE_MAP, DDSP_ALL_PLAYERS);

#if __JHEXEN__
    SN_StopAllSequences();
#endif

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];

        if(plr->plr->inGame && plr->playerState == PST_DEAD)
            plr->playerState = PST_REBORN;

#if __JHEXEN__ || __JSTRIFE__
        if(!IS_NETGAME || (IS_NETGAME != 0 && deathmatch != 0) ||
            firstFragReset == 1)
        {
            memset(plr->frags, 0, sizeof(plr->frags));
            firstFragReset = 0;
        }
#else
        memset(plr->frags, 0, sizeof(plr->frags));
#endif
        // Set all player mobjs to NULL, clear control state toggles etc.
        plr->plr->mo = NULL;
        G_ResetLookOffset(i);
    }

    if(theMap)
        P_DestroyGameMap(theMap);
    theMap = NULL;

    P_GetMapLumpName(mapID, gameEpisode, gameMap);
    map = theMap = P_CreateGameMap(mapID, gameEpisode, gameMap);

    P_SetupMap(map, gameSkill);

    Set(DD_DISPLAYPLAYER, CONSOLEPLAYER); // View the guy you are playing.
    G_SetGameAction(GA_NONE);

    Z_CheckHeap();

    // Clear cmd building stuff.
    G_ResetMousePos();
    sendPause = paused = false;

    G_ControlReset(-1); // Clear all controls for all local players.

    // Start a briefing, if there is one.
    if(!FI_Briefing(gameEpisode, gameMap))
    {   // No briefing, start the map.
        G_ChangeGameState(GS_MAP);
        S_MapMusic();
    }
}

/**
 * Get info needed to make ticcmd_ts for the players.
 * Return false if the event should be checked for bindings.
 */
boolean G_Responder(event_t *ev)
{
    // With the menu active, none of these should respond to input events.
    if(!Hu_MenuIsActive() && !Hu_IsMessageActive())
    {
        // Try Infine.
        if(FI_Responder(ev))
            return true;

        // Try the chatmode responder.
        if(Chat_Responder(ev))
            return true;

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
        // Check for cheats?
        if(G_GetGameState() == GS_MAP)
        {
            if(G_EventSequenceResponder(ev))
                return true;
        }
#endif
    }

    // Try the edit responder.
    if(M_EditResponder(ev))
        return true;

    // We may wish to eat the event depending on type...
    if(G_AdjustControlState(ev))
        return true;

    // The event wasn't used.
    return false;
}

/**
 * The core of the timing loop. Game state, game actions etc occur here.
 *
 * @param ticLength     How long this tick is, in seconds.
 */
void G_Ticker(timespan_t ticLength)
{
    static trigger_t fixed = {1.0 / TICSPERSEC};

    gameaction_t currentAction;
    int i;

    // Always tic:
    Hu_FogEffectTicker(ticLength);
    Hu_MenuTicker(ticLength);
    Hu_MsgTicker(ticLength);

    if(IS_CLIENT && !Get(DD_GAME_READY))
        return;

    // Do player reborns if needed.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];

        if(plr->plr->inGame && plr->playerState == PST_REBORN &&
           !P_MobjIsCamera(plr->plr->mo))
            G_DoReborn(i);

        // Player has left?
        if(plr->playerState == PST_GONE)
        {
            plr->playerState = PST_REBORN;
            if(plr->plr->mo)
            {
                if(!IS_CLIENT)
                {
                    gamemap_t* map = P_CurrentGameMap();
                    P_SpawnTeleFog(map, plr->plr->mo->pos[VX],
                                   plr->plr->mo->pos[VY],
                                   plr->plr->mo->angle + ANG180);
                }

                // Let's get rid of the mobj.
#ifdef _DEBUG
Con_Message("G_Ticker: Removing player %i's mobj.\n", i);
#endif
                P_MobjRemove(plr->plr->mo, true);
                plr->plr->mo = NULL;
            }
        }
    }

    // Do things to change the game state.
    while((currentAction = G_GetGameAction()) != GA_NONE)
    {
        switch(currentAction)
        {
#if __JHEXEN__ || __JSTRIFE__
        case GA_INITNEW:
            G_DoInitNew();
            break;

        case GA_SINGLEREBORN:
            G_DoSingleReborn();
            break;
#endif

        case GA_LEAVEMAP:
#if __JHEXEN__ || __JSTRIFE__
            //Draw_TeleportIcon();
            G_DoTeleportNewMap();
#else
            G_DoWorldDone();
#endif
            break;

        case GA_LOADMAP:
            G_DoLoadMap();
            break;

        case GA_NEWGAME:
            G_DoNewGame();
            break;

        case GA_LOADGAME:
            G_DoLoadGame();
            break;

        case GA_SAVEGAME:
            G_DoSaveGame();
            break;

        case GA_COMPLETED:
            G_DoCompleted();
            break;

        case GA_VICTORY:
            G_SetGameAction(GA_NONE);
            break;

        case GA_SCREENSHOT:
            G_DoScreenShot();
            G_SetGameAction(GA_NONE);
            break;

        case GA_NONE:
        default:
            break;
        }
    }

    // Update the viewer's look angle
    //G_LookAround(CONSOLEPLAYER);

    if(!IS_CLIENT)
    {
        // Enable/disable sending of frames (delta sets) to clients.
        Set(DD_ALLOW_FRAMES, G_GetGameState() == GS_MAP);

        // Tell Doomsday when the game is paused (clients can't pause
        // the game.)
        Set(DD_CLIENT_PAUSED, P_IsPaused());
    }

    // Must be called on every tick.
    P_RunPlayers(ticLength);

    // The following is restricted to fixed 35 Hz ticks.
    if(M_RunTrigger(&fixed, ticLength))
    {
        // Do main actions.
        switch(G_GetGameState())
        {
        case GS_MAP:
            P_DoTick();
            HU_UpdatePsprites();

            // Active briefings once again (they were disabled when loading
            // a saved game).
            briefDisabled = false;

            if(IS_DEDICATED)
                break;

            ST_Ticker();
            AM_Ticker();
            Hu_Ticker();
            break;

        case GS_INTERMISSION:
#if __JDOOM__ || __JDOOM64__
            WI_Ticker();
#else
            IN_Ticker();
#endif
            break;

        default:
            break;
        }

        // Update view window size.
        R_ViewWindowTicker();

        // InFine ticks whenever it's active.
        FI_Ticker();

        // Servers will have to update player information and do such stuff.
        if(!IS_CLIENT)
            NetSv_Ticker();
    }
}

/**
 * Called when a player leaves a map.
 *
 * Jobs include; striping keys, inventory and powers from the player
 * and configuring other player-specific properties ready for the next
 * map.
 *
 * @param player        Id of the player to configure.
 */
void G_PlayerLeaveMap(int player)
{
#if __JHERETIC__ || __JHEXEN__
    uint i;
    int flightPower;
#endif
    player_t* p = &players[player];
    boolean newCluster;

#if __JHEXEN__
    newCluster = (P_GetMapCluster(gameMap) != P_GetMapCluster(p->leaveMap));
#else
    newCluster = true;
#endif

#if __JHERETIC__ || __JHEXEN__
    // Remember if flying.
    flightPower = p->powers[PT_FLIGHT];
#endif

#if __JHERETIC__
    // Empty the inventory of excess items
    for(i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        inventoryitemtype_t type = IIT_FIRST + i;
        uint count = P_InventoryCount(player, type);

        if(count)
        {
            uint j;

            if(type != IIT_FLY)
                count--;

            for(j = 0; j < count; ++j)
                P_InventoryTake(player, type, true);
        }
    }
#endif

#if __JHEXEN__
    if(newCluster)
    {
        uint count = P_InventoryCount(player, IIT_FLY);

        for(i = 0; i < count; ++i)
            P_InventoryTake(player, IIT_FLY, true);
    }
#endif

    // Remove their powers.
    p->update |= PSF_POWERS;
    memset(p->powers, 0, sizeof(p->powers));

#if __JHEXEN__
    if(!newCluster && !deathmatch)
        p->powers[PT_FLIGHT] = flightPower; // Restore flight.
#endif

    // Remove their keys.
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    p->update |= PSF_KEYS;
    memset(p->keys, 0, sizeof(p->keys));
#else
    if(!deathmatch && newCluster)
        p->keys = 0;
#endif

    // Misc
#if __JHERETIC__
    p->rain1 = NULL;
    p->rain2 = NULL;
#endif

    // Un-morph?
#if __JHERETIC__ || __JHEXEN__
    p->update |= PSF_MORPH_TIME;
    if(p->morphTics)
    {
        p->readyWeapon = p->plr->mo->special1; // Restore weapon.
        p->morphTics = 0;
    }
#endif

    p->plr->lookDir = 0;
    p->plr->mo->flags &= ~MF_SHADOW; // Cancel invisibility.
    p->plr->extraLight = 0; // Cancel gun flashes.
    p->plr->fixedColorMap = 0; // Cancel IR goggles.

    // Clear filter.
    p->plr->flags &= ~DDPF_VIEW_FILTER;
    p->plr->flags |= DDPF_FILTER; // Server: Send the change to the client.
    p->damageCount = 0; // No palette changes.
    p->bonusCount = 0;

#if __JHEXEN__
    p->poisonCount = 0;
#endif

    Hu_LogEmpty(p - players);
}

/**
 * Safely clears the player data structures.
 */
void ClearPlayer(player_t *p)
{
    ddplayer_t *ddplayer = p->plr;
    int         playeringame = ddplayer->inGame;
    int         flags = ddplayer->flags;
    int         start = p->startSpot;
    fixcounters_t counter, acked;

    // Restore counters.
    counter = ddplayer->fixCounter;
    acked = ddplayer->fixAcked;

    memset(p, 0, sizeof(*p));
    // Restore the pointer to ddplayer.
    p->plr = ddplayer;
#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    P_InventoryEmpty(p - players);
    P_InventorySetReadyItem(p - players, IIT_NONE);
#endif
    // Also clear ddplayer.
    memset(ddplayer, 0, sizeof(*ddplayer));
    // Restore the pointer to this player.
    ddplayer->extraData = p;
    // Restore the playeringame data.
    ddplayer->inGame = playeringame;
    ddplayer->flags = flags & ~(DDPF_INTERYAW | DDPF_INTERPITCH);
    // Don't clear the start spot.
    p->startSpot = start;
    // Restore counters.
    ddplayer->fixCounter = counter;
    ddplayer->fixAcked = acked;

    ddplayer->fixCounter.angles++;
    ddplayer->fixCounter.pos++;
    ddplayer->fixCounter.mom++;

/*    ddplayer->fixAcked.angles =
        ddplayer->fixAcked.pos =
        ddplayer->fixAcked.mom = -1;
#ifdef _DEBUG
    Con_Message("ClearPlayer: fixacked set to -1 (counts:%i, %i, %i)\n",
                ddplayer->fixCounter.angles,
                ddplayer->fixCounter.pos,
                ddplayer->fixCounter.mom);
#endif*/
}

/**
 * Called after a player dies (almost everything is cleared and then
 * re-initialized).
 */
void G_PlayerReborn(int player)
{
    player_t       *p;
    int             frags[MAXPLAYERS];
    int             killcount, itemcount, secretcount;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    int             i;
#endif
#if __JHERETIC__
    boolean         secret = false;
    int             spot;
#elif __JHEXEN__ || __JSTRIFE__
    uint            worldTimer;
#endif

    if(player < 0 || player >= MAXPLAYERS)
        return; // Wha?

    p = &players[player];

    memcpy(frags, p->frags, sizeof(frags));
    killcount = p->killCount;
    itemcount = p->itemCount;
    secretcount = p->secretCount;
#if __JHEXEN__ || __JSTRIFE__
    worldTimer = p->worldTimer;
#endif

#if __JHERETIC__
    if(p->didSecret)
        secret = true;
    spot = p->startSpot;
#endif

    // Clears (almost) everything.
    ClearPlayer(p);

#if __JHERETIC__
    p->startSpot = spot;
#endif

    memcpy(p->frags, frags, sizeof(p->frags));
    p->killCount = killcount;
    p->itemCount = itemcount;
    p->secretCount = secretcount;
#if __JHEXEN__ || __JSTRIFE__
    p->worldTimer = worldTimer;
    p->colorMap = cfg.playerColor[player];
#endif
#if __JHEXEN__
    p->class = cfg.playerClass[player];
#endif
    p->useDown = p->attackDown = true; // Don't do anything immediately.
    p->playerState = PST_LIVE;
    p->health = maxHealth;

#if __JDOOM__ || __JDOOM64__
    p->readyWeapon = p->pendingWeapon = WT_SECOND;
    p->weapons[WT_FIRST].owned = true;
    p->weapons[WT_SECOND].owned = true;

    // Initalize the player's ammo counts.
    memset(p->ammo, 0, sizeof(p->ammo));
    p->ammo[AT_CLIP].owned = 50;

    // See if the Values specify anything.
    P_InitPlayerValues(p);

#elif __JHERETIC__
    p->readyWeapon = p->pendingWeapon = WT_SECOND;
    p->weapons[WT_FIRST].owned = true;
    p->weapons[WT_SECOND].owned = true;
    p->ammo[AT_CRYSTAL].owned = 50;

    if(gameMap == 9 || secret)
    {
        p->didSecret = true;
    }

#else
    p->readyWeapon = p->pendingWeapon = WT_FIRST;
    p->weapons[WT_FIRST].owned = true;
    p->viewShake = 0;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    // Reset maxammo.
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
        p->ammo[i].max = maxAmmo[i];
#endif

    // We'll need to update almost everything.
#if __JHERETIC__
    p->update |=
        PSF_STATE | PSF_HEALTH | PSF_ARMOR_TYPE | PSF_ARMOR_POINTS |
        PSF_INVENTORY | PSF_POWERS | PSF_KEYS | PSF_OWNED_WEAPONS | PSF_AMMO |
        PSF_MAX_AMMO | PSF_PENDING_WEAPON | PSF_READY_WEAPON;
#else
    p->update |= PSF_REBORN;
#endif

    p->plr->flags &= ~DDPF_DEAD;
}

#if __JDOOM__ || __JDOOM64__
void G_QueueBody(mobj_t* mo)
{
    assert(mo);
    {
    gamemap_t* map = P_CurrentGameMap();

    // Flush an old corpse if needed.
    if(map->bodyQueueSlot >= BODYQUEUESIZE)
        P_MobjRemove(map->bodyQueue[map->bodyQueueSlot % BODYQUEUESIZE], false);

    map->bodyQueue[map->bodyQueueSlot % BODYQUEUESIZE] = mo;
    map->bodyQueueSlot++;
    }
}
#endif

void G_DoReborn(int plrNum)
{
    if(plrNum < 0 || plrNum >= MAXPLAYERS)
        return; // Wha?

    // Clear the currently playing script, if any.
    FI_Reset();

    if(!IS_NETGAME)
    {
        // We've just died, don't do a briefing now.
        briefDisabled = true;

#if __JHEXEN__
        if(SV_HxRebornSlotAvailable())
        {   // Use the reborn code if the slot is available
            G_SetGameAction(GA_SINGLEREBORN);
        }
        else
        {   // Start a new game if there's no reborn info
            G_SetGameAction(GA_NEWGAME);
        }
#else
        // Reload the map from scratch.
        G_SetGameAction(GA_LOADMAP);
#endif
    }
    else
    {   // In a net game.
        P_RebornPlayer(plrNum);
    }
}

#if __JHEXEN__
void G_StartNewInit(void)
{
    int i;

    SV_HxInitBaseSlot();
    SV_HxClearRebornSlot();

    if(ActionScriptInterpreter)
        P_DestroyActionScriptInterpreter(ActionScriptInterpreter);

    // Default the player start spot group to 0
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];
        plr->rebornPosition = 0;
    }
}

void G_StartNewGame(skillmode_t skill)
{
    int realMap = 1;

    G_StartNewInit();
# if __JHEXEN__
    realMap = P_TranslateMap(1);
# endif

    if(realMap == -1)
    {
        realMap = 1;
    }

    G_InitNew(dSkill, 1, realMap);
}

void G_DoTeleportNewMap(void)
{
    player_t* plr = &players[CONSOLEPLAYER]; // @fixme This is not right.

    // Clients trust the server in these things.
    if(IS_CLIENT)
    {
        G_SetGameAction(GA_NONE);
        return;
    }

    SV_MapTeleport(plr->leaveMap, plr->leavePosition);
    G_ChangeGameState(GS_MAP);
    G_SetGameAction(GA_NONE);
    plr->rebornPosition = plr->leavePosition;

    // Is there a briefing before this map?
    FI_Briefing(gameEpisode, gameMap);
}
#endif

/**
 * Leave the current map and start intermission routine.
 * (if __JHEXEN__ the intermission will only be displayed when exiting a
 * hub and in DeathMatch games)
 *
 * @param map           ID of the map we are leaving.
 * @param position      Position id (maps with multiple entry/exit points).
 * @param secret        @c true = if this is a secret exit point.
 */
void G_LeaveMap(int player, int map, int position, boolean secret)
{
    assert(player >= 0 && player < MAXPLAYERS);

    if(cyclingMaps && mapCycleNoExit)
        return;

#if __JHEXEN__
    if(shareware && map > 4)
    {
        // Not possible in the 4-map demo.
        P_SetMessage(&players[CONSOLEPLAYER], "PORTAL INACTIVE -- DEMO", false);
        return;
    }
#endif

#if __JHEXEN__
    players[player].leaveMap = map;
    players[player].leavePosition = position;
#else
    players[player].secretExit = secret;
  #if __JDOOM__
      // If no Wolf3D maps, no secret exit!
      if(secret && (gameMode == commercial) &&
         W_CheckNumForName("map31") < 0)
          players[player].secretExit = false;
  #endif
#endif

    G_SetGameAction(GA_COMPLETED);
}

/**
 * @return              @c true, if the game has been completed.
 */
boolean G_IfVictory(void)
{
#if __JDOOM64__
    if(gameMap == 28)
    {
        G_SetGameAction(GA_VICTORY);
        return true;
    }
#elif __JDOOM__
    if((gameMap == 8) && (gameMode != commercial))
    {
        G_SetGameAction(GA_VICTORY);
        return true;
    }

#elif __JHERETIC__
    if(gameMap == 8)
    {
        G_SetGameAction(GA_VICTORY);
        return true;
    }

#elif __JHEXEN__
    {
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];

        if(plr->leaveMap == -1 && plr->leavePosition == -1)
        {
            G_SetGameAction(GA_VICTORY);
            return true;
        }
    }
    }
#endif

    return false;
}

void G_DoCompleted(void)
{
    int i;

    G_SetGameAction(GA_NONE);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            AM_Open(AM_MapForPlayer(i), false, true);

            G_PlayerLeaveMap(i); // take away cards and stuff

            // Update this client's stats.
            NetSv_SendPlayerState(i, DDSP_ALL_PLAYERS,
                                  PSF_FRAGS | PSF_COUNTERS, true);
        }
    }

    GL_SetFilter(false);

#if __JHEXEN__
    SN_StopAllSequences();
#endif

    // Go to an intermission?
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {
    ddmapinfo_t minfo;
    char mapID[8];

    P_GetMapLumpName(mapID, gameEpisode, gameMap);

    if(Def_Get(DD_DEF_MAP_INFO, mapID, &minfo) && (minfo.flags & MIF_NO_INTERMISSION))
    {
        G_WorldDone();
        return;
    }
    }
#elif __JHEXEN__
    if(!deathmatch)
    {
        G_WorldDone();
        return;
    }
#endif

    // Has the player completed the game?
    if(G_IfVictory())
        return; // Victorious!

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
# if __JDOOM__
    if(gameMode != commercial && gameMap == 9)
    {
        for(i = 0; i < MAXPLAYERS; ++i)
            players[i].didSecret = true;
    }
# endif

    wmInfo.didSecret = players[CONSOLEPLAYER].didSecret;
    wmInfo.last = gameMap - 1;

# if __JDOOM64__
    if(players[CONSOLEPLAYER].secretExit)
    {
        switch(gameMap)
        {
        case 1: wmInfo.next = 31; break;
        case 4: wmInfo.next = 28; break;
        case 12: wmInfo.next = 29; break;
        case 18: wmInfo.next = 30; break;
        case 32: wmInfo.next = 0; break;
        }
    }
    else
    {
        switch(gameMap)
        {
        case 24: wmInfo.next = 27; break;
        case 32: wmInfo.next = 0; break;
        case 29: wmInfo.next = 4; break;
        case 30: wmInfo.next = 12; break;
        case 31: wmInfo.next = 18; break;
        case 25: wmInfo.next = 0; break;
        case 26: wmInfo.next = 0; break;
        case 27: wmInfo.next = 0; break;
        default: wmInfo.next = gameMap;
        }
    }
# elif __JDOOM__
    // wmInfo.next is 0 biased, unlike gameMap
    if(gameMode == commercial)
    {
        if(players[CONSOLEPLAYER].secretExit)
        {
            switch(gameMap)
            {
            case 15:
                wmInfo.next = 30;
                break;
            case 31:
                wmInfo.next = 31;
                break;
            }
        }
        else
        {
            switch(gameMap)
            {
            case 31:
            case 32:
                wmInfo.next = 15;
                break;
            default:
                wmInfo.next = gameMap;
            }
        }
    }
    else
    {
        if(players[CONSOLEPLAYER].secretExit)
            wmInfo.next = 8; // Go to secret map.
        else if(gameMap == 9)
        {
            // Returning from secret map.
            switch(gameEpisode)
            {
            case 1:
                wmInfo.next = 3;
                break;
            case 2:
                wmInfo.next = 5;
                break;
            case 3:
                wmInfo.next = 6;
                break;
            case 4:
                wmInfo.next = 2;
                break;
            }
        }
        else
            wmInfo.next = gameMap; // Go to next map.
    }
# else /* __JHERETIC__ */
    if(players[CONSOLEPLAYER].secretExit == true)
    {
        wmInfo.next = 9;
    }
    else if(gameMap == 9)
    {   // Finished secret map.
        static int afterSecret[5] = { 7, 5, 5, 5, 4 };

        wmInfo.next = afterSecret[gameEpisode - 1];
    }
    else
        wmInfo.next = gameMap; // Go to next map.
# endif

    // Is there an overide for wmInfo.next? (eg from an XG line)
    if(nextMap > 0)
    {
        wmInfo.next = nextMap - 1;   // wmInfo is zero based
        nextMap = 0;
    }

# if __JDOOM__ || __JDOOM64__
    {
    gamemap_t* map = P_CurrentGameMap();
    wmInfo.maxKills = map->totalKills;
    wmInfo.maxItems = map->totalItems;
    wmInfo.maxSecret = map->totalSecret;
    }

    G_PrepareWIData();
# endif
#endif

#if __JHERETIC__
    // @fixme is this necessary at this time?
    NetSv_SendGameState(0, DDSP_ALL_PLAYERS);
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    NetSv_Intermission(IMF_BEGIN, 0, 0);
#else /* __JHEXEN__ */
    NetSv_Intermission(IMF_BEGIN, players[CONSOLEPLAYER].leaveMap, players[CONSOLEPLAYER].leavePosition);
#endif

    G_ChangeGameState(GS_INTERMISSION);

#if __JDOOM__ || __JDOOM64__
    WI_Start(&wmInfo);
#elif __JHERETIC__
    IN_Start(&wmInfo);
#else /* __JHEXEN__ */
    IN_Start();
#endif
}

#if __JDOOM__ || __JDOOM64__
void G_PrepareWIData(void)
{
    ddmapinfo_t minfo;
    char mapID[8];
    wbstartstruct_t* info = &wmInfo;
    int i;

#if !__JDOOM64__
    info->epsd = gameEpisode - 1;
#endif
    info->maxFrags = 0;

    P_GetMapLumpName(mapID, gameEpisode, gameMap);

    // See if there is a par time definition.
    if(Def_Get(DD_DEF_MAP_INFO, mapID, &minfo) && minfo.parTime > 0)
        info->parTime = 35 * (int) minfo.parTime;
    else
        info->parTime = -1; // Unknown.

    info->pNum = CONSOLEPLAYER;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* p = &players[i];
        gamemap_t* map = P_CurrentGameMap();
        wbplayerstruct_t* pStats = &info->plyr[i];

        pStats->inGame = p->plr->inGame;
        pStats->kills = p->killCount;
        pStats->items = p->itemCount;
        pStats->secret = p->secretCount;
        pStats->time = map->time;
        memcpy(pStats->frags, p->frags, sizeof(pStats->frags));
    }
}
#endif

boolean G_DebriefingEnabled(void)
{
    // If we're already in the INFINE state, don't start a finale.
    if(briefDisabled)
        return false;
#if __JHEXEN__
    if(cfg.overrideHubMsg && G_GetGameState() == GS_MAP &&
       !(players[CONSOLEPLAYER].leaveMap == -1 && players[CONSOLEPLAYER].leavePosition == -1) &&
       P_GetMapCluster(gameMap) != P_GetMapCluster(players[CONSOLEPLAYER].leaveMap))
        return false;
#endif
    if(G_GetGameState() == GS_INFINE || IS_CLIENT || Get(DD_PLAYBACK))
        return false;

    return true;
}

void G_WorldDone(void)
{
#if __JDOOM__ || __JDOOM64__
    if(players[CONSOLEPLAYER].secretExit)
        players[CONSOLEPLAYER].didSecret = true;
#endif

    // Clear the currently playing script, if any.
    // @note FI_Reset() changes the game state so we must determine
    // whether the debrief is enabled first.
    {
    boolean doDebrief = G_DebriefingEnabled();

    FI_Reset();

    if(doDebrief && FI_Debriefing(gameEpisode, gameMap))
        return;

    // We have either just returned from a debriefing or there wasn't one.
    briefDisabled = false;
    }

    G_SetGameAction(GA_LEAVEMAP);
}

void G_DoWorldDone(void)
{
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    gameMap = wmInfo.next + 1;
#endif

    G_DoLoadMap();
    G_SetGameAction(GA_NONE);
}

#if __JHEXEN__
/**
 * Called by G_Ticker based on gameaction.  Loads a game from the reborn
 * save slot.
 */
void G_DoSingleReborn(void)
{
    G_SetGameAction(GA_NONE);
    SV_LoadGame(SV_HxGetRebornSlot());
}
#endif

/**
 * Can be called by the startup code or the menu task.
 */
#if __JHEXEN__ || __JSTRIFE__
void G_LoadGame(int slot)
{
    gameLoadSlot = slot;
    G_SetGameAction(GA_LOADGAME);
}
#else
void G_LoadGame(const char* name)
{
    M_TranslatePath(saveName, name, FILENAME_T_MAXLEN);
    G_SetGameAction(GA_LOADGAME);
}
#endif

/**
 * Called by G_Ticker based on gameaction.
 */
void G_DoLoadGame(void)
{
    G_StopDemo();
    FI_Reset();
    G_SetGameAction(GA_NONE);

#if __JHEXEN__ || __JSTRIFE__
    GL_DrawPatch(100, 68, W_GetNumForName("loadicon"));

    SV_LoadGame(gameLoadSlot);
    if(!IS_NETGAME)
    {                           // Copy the base slot to the reborn slot
        SV_HxUpdateRebornSlot();
    }
#else
    SV_LoadGame(saveName);
#endif
}

/**
 * Called by the menu task.
 *
 * @param description       A 24 byte text string.
 */
void G_SaveGame(int slot, const char* description)
{
    saveGameSlot = slot;
    strncpy(saveDescription, description, HU_SAVESTRINGSIZE);
    G_SetGameAction(GA_SAVEGAME);
}

/**
 * Called by G_Ticker based on gameaction.
 */
void G_DoSaveGame(void)
{
#if __JHEXEN__ || __JSTRIFE__
    GL_DrawPatch(100, 68, W_GetNumForName("SAVEICON"));

    SV_SaveGame(saveGameSlot, saveDescription);
#else
    filename_t              name;

    SV_GetSaveGameFileName(name, saveGameSlot, FILENAME_T_MAXLEN);
    SV_SaveGame(name, saveDescription);
#endif

    G_SetGameAction(GA_NONE);
    saveDescription[0] = 0;

    P_SetMessage(&players[CONSOLEPLAYER], TXT_GAMESAVED, false);
}

#if __JHEXEN__ || __JSTRIFE__
void G_DeferredNewGame(skillmode_t skill)
{
    dSkill = skill;
    G_SetGameAction(GA_NEWGAME);
}

void G_DoInitNew(void)
{
    SV_HxInitBaseSlot();
    G_InitNew(dSkill, dEpisode, dMap);
    G_SetGameAction(GA_NONE);
}
#endif

/**
 * Can be called by the startup code or the menu task, CONSOLEPLAYER,
 * DISPLAYPLAYER, playeringame[] should be set.
 */
void G_DeferedInitNew(skillmode_t skill, int episode, int map)
{
    dSkill = skill;
    dEpisode = episode;
    dMap = map;

#if __JHEXEN__ || __JSTRIFE__
    G_SetGameAction(GA_INITNEW);
#else
    G_SetGameAction(GA_NEWGAME);
#endif
}

void G_DoNewGame(void)
{
    G_StopDemo();
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    if(!IS_NETGAME)
    {
        deathmatch = false;
        respawnMonsters = false;
        noMonstersParm = ArgExists("-nomonsters")? true : false;
    }
    G_InitNew(dSkill, dEpisode, dMap);
#else
    G_StartNewGame(dSkill);
#endif
    G_SetGameAction(GA_NONE);
}

/**
 * Start a new game.
 */
void G_InitNew(skillmode_t skill, int episode, int map)
{
    int i;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    int speed;
#endif

    // Close any open automaps.
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame)
            AM_Open(AM_MapForPlayer(i), false, true);

    // If there are any InFine scripts running, they must be stopped.
    FI_Reset();

    if(paused)
    {
        paused = false;
    }

    if(skill < SM_BABY)
        skill = SM_BABY;
    if(skill > NUM_SKILL_MODES - 1)
        skill = NUM_SKILL_MODES - 1;

    // Make sure that the episode and map numbers are good.
    G_ValidateMap(&episode, &map);

    M_ResetRandom();

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ || __JSTRIFE__
    respawnMonsters = respawnParm;
#endif

#if __JDOOM__ || __JHERETIC__
    // Is respawning enabled at all in nightmare skill?
    if(skill == SM_NIGHTMARE)
        respawnMonsters = cfg.respawnMonstersNightmare;
#endif

//// \kludge Doom/Heretic Fast Monters/Missiles
#if __JDOOM__ || __JDOOM64__
    // Fast monsters?
    if(fastParm
# if __JDOOM__
        || (skill == SM_NIGHTMARE && gameSkill != SM_NIGHTMARE)
# endif
        )
    {
        for(i = S_SARG_RUN1; i <= S_SARG_RUN8; ++i)
            STATES[i].tics = 1;
        for(i = S_SARG_ATK1; i <= S_SARG_ATK3; ++i)
            STATES[i].tics = 4;
        for(i = S_SARG_PAIN; i <= S_SARG_PAIN2; ++i)
            STATES[i].tics = 1;
    }
    else
    {
        for(i = S_SARG_RUN1; i <= S_SARG_RUN8; ++i)
            STATES[i].tics = 2;
        for(i = S_SARG_ATK1; i <= S_SARG_ATK3; ++i)
            STATES[i].tics = 8;
        for(i = S_SARG_PAIN; i <= S_SARG_PAIN2; ++i)
            STATES[i].tics = 2;
    }
#endif

    // Fast missiles?
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
# if __JDOOM64__
    speed = fastParm;
# elif __JDOOM__
    speed = (fastParm || (skill == SM_NIGHTMARE && gameSkill != SM_NIGHTMARE));
# else
    speed = skill == SM_NIGHTMARE;
# endif

    for(i = 0; MonsterMissileInfo[i].type != -1; ++i)
    {
        MOBJINFO[MonsterMissileInfo[i].type].speed =
            MonsterMissileInfo[i].speed[speed];
    }
#endif
// <-- KLUDGE

    if(!IS_CLIENT)
    {
        // Force players to be initialized upon first map load.
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            player_t           *plr = &players[i];

            plr->playerState = PST_REBORN;
#if __JHEXEN__ || __JSTRIFE__
            plr->worldTimer = 0;
#else
            plr->didSecret = false;
#endif
        }
    }

    userGame = true; // Will be set false if a demo.
    paused = false;
    gameEpisode = episode;
    gameMap = map;
    gameSkill = skill;

    NetSv_UpdateGameConfig();

    G_DoLoadMap();

#if __JHEXEN__
    // Initialize the sky.
    P_InitSky(map);
#endif
}

/**
 * Return the index of this map.
 */
int G_GetMapNumber(int episode, int map)
{
#if __JHEXEN__ || __JSTRIFE__
    return P_TranslateMap(map);
#elif __JDOOM64__
    return map;
#else
  #if __JDOOM__
    if(gameMode == commercial)
        return map;
    else
  #endif
    {
        return map + episode * 9; // maps per episode.
    }
#endif
}

/**
 * Compose the name of the map lump identifier.
 */
void P_GetMapLumpName(char lumpName[9], int episode, int map)
{
#if __JDOOM64__
    sprintf(lumpName, "MAP%02i", map);
#elif __JDOOM__
    if(gameMode == commercial)
        sprintf(lumpName, "MAP%02i", map);
    else
        sprintf(lumpName, "E%iM%i", episode, map);
#elif  __JHERETIC__
    sprintf(lumpName, "E%iM%i", episode, map);
#else
    sprintf(lumpName, "MAP%02i", map);
#endif
}

/**
 * Returns true if the specified ep/map exists in a WAD.
 */
boolean P_MapExists(int episode, int map)
{
    char mapID[9];

    P_GetMapLumpName(mapID, episode, map);
    return W_CheckNumForName(mapID) >= 0;
}

/**
 * Returns true if the specified (episode, map) pair can be used.
 * Otherwise the values are adjusted so they are valid.
 */
boolean G_ValidateMap(int* episode, int* map)
{
    boolean ok = true;

    if(*episode < 1)
    {
        *episode = 1;
        ok = false;
    }

    if(*map < 1)
    {
        *map = 1;
        ok = false;
    }

#if __JDOOM64__
    if(*map > 99)
    {
        *map = 99;
        ok = false;
    }
#elif __JDOOM__
    if(gameMode == shareware)
    {
        // only start episode 1 on shareware
        if(*episode > 1)
        {
            *episode = 1;
            ok = false;
        }
    }
    else
    {
        // Allow episodes 1-9.
        if(*episode > 9)
        {
            *episode = 9;
            ok = false;
        }
    }

    if(gameMode == commercial)
    {
        if(*map > 99)
        {
            *map = 99;
            ok = false;
        }
    }
    else
    {
        if(*map > 9)
        {
            *map = 9;
            ok = false;
        }
    }

#elif __JHERETIC__
    //  Allow episodes 1-9.
    if(*episode > 9)
    {
        *episode = 9;
        ok = false;
    }

    if(*map > 9)
    {
        *map = 9;
        ok = false;
    }

    if(gameMode == shareware) // Shareware version checks
    {
        if(*episode > 1)
        {
            *episode = 1;
            ok = false;
        }
    }
    else if(gameMode == extended) // Extended version checks
    {
        if(*episode == 6)
        {
            if(*map > 3)
            {
                *map = 3;
                ok = false;
            }
        }
        else if(*episode > 5)
        {
            *episode = 5;
            ok = false;
        }
    }
    else // Registered version checks
    {
        if(*episode == 4)
        {
            if(*map != 1)
            {
                *map = 1;
                ok = false;
            }
        }
        else if(*episode > 3)
        {
            *episode = 3;
            ok = false;
        }
    }
#elif __JHEXEN__ || __JSTRIFE__
    if(*map > 99)
    {
        *map = 99;
        ok = false;
    }
#endif

    // Check that the map truly exists.
    if(!P_MapExists(*episode, *map))
    {
        // (1,1) should exist always?
        *episode = 1;
        *map = 1;
        ok = false;
    }

    return ok;
}

#if __JHERETIC__
char* P_GetShortMapName(int episode, int map)
{
    char* name = P_GetMapName(episode, map);
    char* ptr;

    // Remove the "ExMx:" from the beginning.
    ptr = strchr(name, ':');
    if(!ptr)
        return name;

    name = ptr + 1;
    while(*name && isspace(*name))
        name++; // Skip any number of spaces.

    return name;
}

char* P_GetMapName(int episode, int map)
{
    char mapID[10], *ptr;
    ddmapinfo_t info;

    // Compose the map identifier.
    P_GetMapLumpName(mapID, episode, map);

    // Get the map info definition.
    if(!Def_Get(DD_DEF_MAP_INFO, mapID, &info))
    {
        // There is no map information for this map...
        return "";
    }

    if(Def_Get(DD_DEF_TEXT, info.name, &ptr) != -1)
        return ptr;

    return info.name;
}
#endif

/**
 * Print a list of maps and the WAD files where they are from.
 */
void G_PrintFormattedMapList(int episode, const char** files, int count)
{
    const char* current = NULL;
    char mapID[20];
    int i, k, rangeStart = 0, len;

    for(i = 0; i < count; ++i)
    {
        if(!current && files[i])
        {
            current = files[i];
            rangeStart = i;
        }
        else if(current && (!files[i] || stricmp(current, files[i])))
        {
            // Print a range.
            len = i - rangeStart;
            Con_Printf("  "); // Indentation.
            if(len <= 2)
            {
                for(k = rangeStart + 1; k <= i; ++k)
                {
                    P_GetMapLumpName(mapID, episode, k);
                    Con_Printf("%s%s", mapID, k != i ? "," : "");
                }
            }
            else
            {
                P_GetMapLumpName(mapID, episode, rangeStart + 1);
                Con_Printf("%s-", mapID);
                P_GetMapLumpName(mapID, episode, i);
                Con_Printf("%s", mapID);
            }
            Con_Printf(": %s\n", M_PrettyPath(current));

            // Moving on to a different file.
            current = files[i];
            rangeStart = i;
        }
    }
}

/**
 * Print a list of loaded maps and which WAD files are they located in.
 * The maps are identified using the "ExMy" and "MAPnn" markers.
 */
void G_PrintMapList(void)
{
    const char* sourceList[100];
    lumpnum_t lump;
    int episode, map, numEpisodes, maxMapsPerEpisode;
    char mapID[20];

#if __JDOOM__
    if(gameMode == registered)
    {
        numEpisodes = 3;
        maxMapsPerEpisode = 9;
    }
    else if(gameMode == retail)
    {
        numEpisodes = 4;
        maxMapsPerEpisode = 9;
    }
    else
    {
        numEpisodes = 1;
        maxMapsPerEpisode = 99;
    }
#elif __JHERETIC__
    if(gameMode == extended)
        numEpisodes = 6;
    else if(gameMode == registered)
        numEpisodes = 3;
    else
        numEpisodes = 1;
    maxMapsPerEpisode = 9;
#else
    numEpisodes = 1;
    maxMapsPerEpisode = 99;
#endif

    for(episode = 1; episode <= numEpisodes; ++episode)
    {
        memset((void*) sourceList, 0, sizeof(sourceList));

        // Find the name of each map (not all may exist).
        for(map = 1; map <= maxMapsPerEpisode; ++map)
        {
            P_GetMapLumpName(mapID, episode, map);

            // Does the lump exist?
            if((lump = W_CheckNumForName(mapID)) >= 0)
            {
                // Get the name of the WAD.
                sourceList[map - 1] = W_LumpSourceFile(lump);
            }
        }

        G_PrintFormattedMapList(episode, sourceList, 99);
    }
}

/**
 * Stops both playback and a recording. Called at critical points like
 * starting a new game, or ending the game in the menu.
 */
void G_StopDemo(void)
{
    DD_Execute(true, "stopdemo");
}

void G_DemoEnds(void)
{
    G_ChangeGameState(GS_WAITING);

    if(singledemo)
        Sys_Quit();

    FI_DemoEnds();
}

void G_DemoAborted(void)
{
    G_ChangeGameState(GS_WAITING);
    FI_DemoEnds();
}

void G_ScreenShot(void)
{
    G_SetGameAction(GA_SCREENSHOT);
}

void G_DoScreenShot(void)
{
    int                 i;
    filename_t          name;
    char*               numPos;

    // Use game mode as the file name base.
    sprintf(name, "%s-", (char *) G_GetVariable(DD_GAME_MODE));
    numPos = name + strlen(name);

    // Find an unused file name.
    for(i = 0; i < 1e6; ++i) // Stop eventually...
    {
        sprintf(numPos, "%03i.tga", i);
        if(!M_FileExists(name))
            break;
    }

    M_ScreenShot(name, 24);
    Con_Message("Wrote %s.\n", name);
}

DEFCC(CCmdListMaps)
{
    Con_Message("Loaded maps:\n");
    G_PrintMapList();
    return true;
}
