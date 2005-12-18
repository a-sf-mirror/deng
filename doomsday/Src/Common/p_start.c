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
 * Compiles for jDoom/jHeretic/jHexen
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "doomdef.h"
#  include "doomstat.h"
#  include "p_local.h"
#  include "d_config.h"
#  include "g_game.h"
#  include "s_sound.h"
#  include "r_common.h"
# include "hu_stuff.h"
#elif __JHERETIC__
#  include "jHeretic/Doomdef.h"
#  include "jHeretic/P_local.h"
#  include "h_config.h"
#elif __JHEXEN__
#  include "jHexen/h2def.h"
#  include "jHexen/p_local.h"
#  include "x_config.h"
#elif __JSTRIFE__
#  include "jStrife/h2def.h"
#  include "jStrife/p_local.h"
#  include "jStrife/d_config.h"
#endif

#include "d_net.h"

// MACROS ------------------------------------------------------------------

// Maximum number of different player starts.
#if __JDOOM__ || __JHERETIC__
#  define MAX_START_SPOTS 4
#else
#  define MAX_START_SPOTS 8
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void P_SpawnMapThing(thing_t * mthing);

#if __JHERETIC__
void P_TurnGizmosAwayFromDoors();
void P_MoveThingsOutOfWalls();
char *P_GetLevelName(int episode, int map);
char *P_GetShortLevelName(int episode, int map);
#elif __JHEXEN__
void P_TurnTorchesToFaceWalls();
#endif

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_SpawnThings(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int actual_leveltime;
extern boolean bossKilled;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int numvertexes;
int numsegs;
int numsectors;
int numsubsectors;
int numnodes;
int numlines;
int numsides;
int numthings;

// If true we are in the process of setting up a level
boolean levelSetup;

// Maintain single and multi player starting spots.
thing_t deathmatchstarts[MAX_DM_STARTS];
thing_t *deathmatch_p;

thing_t playerstarts[MAXSTARTS], *playerstart_p;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void P_RegisterPlayerStart(thing_t * mthing)
{
    // Enough room?
    if(playerstart_p - playerstarts >= MAXSTARTS)
        return;
    *playerstart_p++ = *mthing;
}

/*
 * Gives all the players in the game a playerstart.
 * Only needed in co-op games (start spots are random in deathmatch).
 */
void P_DealPlayerStarts(void)
{
    int     i, k, num = playerstart_p - playerstarts;
    player_t *pl;
    thing_t *mt;
    int     spotNumber;

    if(!num)
        Con_Error("No playerstarts!\n");

    // First assign one start per player, only accepting perfect matches.
    for(i = 0, pl = players; i < MAXPLAYERS; i++, pl++)
    {
        if(!pl->plr->ingame)
            continue;

        // The number of the start spot this player will use.
        spotNumber = i % MAX_START_SPOTS;
        pl->startspot = -1;

        for(k = 0, mt = playerstarts; k < num; k++, mt++)
        {
            if(spotNumber == mt->type - 1)
            {
                // This is a match.
                pl->startspot = k;
                // Keep looking.
            }
        }

        // If still without a start spot, assign one randomly.
        if(pl->startspot == -1)
        {
            // It's likely that some players will get the same start
            // spots.
            pl->startspot = M_Random() % num;
        }
    }

    if(IS_NETGAME)
    {
        Con_Printf("Player starting spots:\n");
        for(i = 0, pl = players; i < MAXPLAYERS; i++, pl++)
        {
            if(!pl->plr->ingame)
                continue;
            Con_Printf("- pl%i: color %i, spot %i\n", i, cfg.PlayerColor[i],
                       pl->startspot);
        }
    }
}

/*
 * Returns false if the player cannot be respawned
 * at the given thing_t spot because something is occupying it
 * FIXME: Quite a mess!
 */
boolean P_CheckSpot(int playernum, thing_t * mthing, boolean doTeleSpark)
{
    fixed_t x;
    fixed_t y;
    unsigned an;
    mobj_t *mo;

#if __JDOOM__ || __JSTRIFE__
    int     i;
#endif
#if __JHERETIC__ || __JHEXEN__
    boolean using_dummy = false;
    thing_t faraway;
#endif

#if __JDOOM__ || __JSTRIFE__
    if(!players[playernum].plr->mo)
    {
        // first spawn of level, before corpses
        for(i = 0; i < playernum; i++)
        {
            if(players[i].plr->mo &&
               players[i].plr->mo->x == mthing->x << FRACBITS &&
               players[i].plr->mo->y == mthing->y << FRACBITS)
                return false;
        }
        return true;
    }
#endif

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

#if __JHERETIC__ || __JHEXEN__
    if(!players[playernum].plr->mo)
    {
        // The player doesn't have a mobj. Let's create a dummy.
        faraway.x = faraway.y = DDMAXSHORT;
        P_SpawnPlayer(&faraway, playernum);
        using_dummy = true;
    }
    players[playernum].plr->mo->flags2 &= ~MF2_PASSMOBJ;
#endif

    if(!P_CheckPosition(players[playernum].plr->mo, x, y))
    {
#if __JHERETIC__ || __JHEXEN__
        players[playernum].plr->mo->flags2 |= MF2_PASSMOBJ;
        if(using_dummy)
        {
            P_RemoveMobj(players[playernum].plr->mo);
            players[playernum].plr->mo = NULL;
        }
#endif
        return false;
    }

#if __JHERETIC__
    players[playernum].plr->mo->flags2 |= MF2_PASSMOBJ;
#endif

#if __JHERETIC__ || __JHEXEN__
    if(using_dummy)
    {
        P_RemoveMobj(players[playernum].plr->mo);
        players[playernum].plr->mo = NULL;
    }
#endif

#if __JDOOM__
    G_QueueBody(players[playernum].plr->mo);
#endif

    if(doTeleSpark)
    {
        // spawn a teleport fog
        an = (ANG45 * (mthing->angle / 45)) >> ANGLETOFINESHIFT;

#if __JDOOM__ || __JHEXEN__ || __JSTRIFE__
        mo = P_SpawnMobj(x + 20 * finecosine[an], y + 20 * finesine[an],
                         P_GetFixedp(DMU_SUBSECTOR, R_PointInSubsector(x, y), DMU_FLOOR_HEIGHT),
                         MT_TFOG);
#else                           // __JHERETIC__
        mo = P_SpawnTeleFog(x + 20 * finecosine[an], y + 20 * finesine[an]);
#endif

        // don't start sound on first frame
        if(players[consoleplayer].plr->viewz != 1)
        {
#if __JHEXEN__ || __JSTRIFE__
            S_StartSound(SFX_TELEPORT, mo);
#else
            S_StartSound(sfx_telept, mo);
#endif
        }
    }

    return true;
}

/*
 * Try to spawn close to the mapspot. Returns false if no clear spot
 * was found.
 */
boolean P_FuzzySpawn(thing_t * spot, int playernum, boolean doTeleSpark)
{
    thing_t place;

    int     i, k, x, y;
    int     offset = 33;        // Player radius = 16

    // Try some spots in the vicinity.
    for(i = 0; i < 9; i++)
    {
        memcpy(&place, spot, sizeof(*spot));
        if(i != 0)
        {
            k = (i == 4 ? 0 : i);
            // Move a bit.
            x = k % 3 - 1;
            y = k / 3 - 1;
            place.x += x * offset;
            place.y += y * offset;
        }
        if(P_CheckSpot(playernum, &place, doTeleSpark))
        {
            // This is good!
            P_SpawnPlayer(&place, playernum);
            return true;
        }
    }

    // No success. Just spawn at the specified spot.
    P_SpawnPlayer(spot, playernum);
    return false;
}

/*
 * Spawns all players, using the method appropriate for current game mode.
 * Called during level setup.
 */
void P_SpawnPlayers(void)
{
    int     i;

    // If deathmatch, randomly spawn the active players.
    if(deathmatch)
    {
        for(i = 0; i < MAXPLAYERS; i++)
            if(players[i].plr->ingame)
            {
                players[i].plr->mo = NULL;
                G_DeathMatchSpawnPlayer(i);
            }
    }
    else
    {
#ifdef __JDOOM__
        if(!IS_NETGAME)
        {
            thing_t *it;

            // Spawn all unused player starts. This will create 'zombies'.
            // FIXME: Also in netgames?
            for(it = playerstarts; it != playerstart_p; it++)
                if(players[0].startspot != it - playerstarts && it->type == 1)
                {
                    P_SpawnPlayer(it, 0);
                }
        }
#endif
        // Spawn everybody at their assigned places.
        // Might get messy if there aren't enough starts.
        for(i = 0; i < MAXPLAYERS; i++)
            if(players[i].plr->ingame)
            {
                ddplayer_t *ddpl = players[i].plr;

                if(!P_FuzzySpawn
                   (&playerstarts[players[i].startspot], i, false))
                {
                    // Gib anything at the spot.
                    P_Telefrag(ddpl->mo);
                }
            }
    }
}

#if __JHEXEN__ || __JSTRIFE__
/*
 * Hexen specific. Returns the correct start for the player. The start
 * is in the given group, or zero if no such group exists.
 */
thing_t *P_GetPlayerStart(int group, int pnum)
{
    thing_t *mt, *g0choice = playerstarts;

    for(mt = playerstarts; mt < playerstart_p; mt++)
    {
        if(mt->arg1 == group && mt->type - 1 == pnum)
            return mt;
        if(!mt->arg1 && mt->type - 1 == pnum)
            g0choice = mt;
    }
    // Return the group zero choice.
    return g0choice;
}
#endif

/*
 * Compose the name of the map lump identifier.
 */
void P_GetMapLumpName(int episode, int map, char *lumpName)
{
#ifdef __JDOOM__
    if(gamemode == commercial)
    {
        sprintf(lumpName, "MAP%02i", map);
    }
    else
    {
        sprintf(lumpName, "E%iM%i", episode, map);
    }
#endif

#ifdef __JHERETIC__
    sprintf(lumpName, "E%iM%i", episode, map);
#endif

#if __JHEXEN__ || __JSTRIFE__
    sprintf(lumpName, "MAP%02i", map);
#endif
}

/*
 * Locate the lump indices where the data of the specified map
 * resides.
 */
void P_LocateMapLumps(int episode, int map, int *lumpIndices)
{
    char lumpName[40];
    char glLumpName[40];

    // Find map name.
    P_GetMapLumpName(episode, map, lumpName);
    sprintf(glLumpName, "GL_%s", lumpName);
    Con_Message("SetupLevel: %s\n", lumpName);

    // Let's see if a plugin is available for loading the data.
    if(!Plug_DoHook(HOOK_LOAD_MAP_LUMPS, W_GetNumForName(lumpName),
                    (void*) lumpIndices))
    {
        // The plugin failed.
        lumpIndices[0] = W_GetNumForName(lumpName);
        lumpIndices[1] = W_CheckNumForName(glLumpName);
    }
}

// -------------------------------------------------------------
// Here follows common code used while loading a new map
// -------------------------------------------------------------

/*
 * Loads map and glnode data for the requested episode and map
 */
void P_SetupLevel(int episode, int map, int playermask, skill_t skill)
{
    int     i;
    int     lumpNumbers[2];
#if __JDOOM__
    char    levelId[9];
#elif __JHERETIC__
    int     parm;
    char    levelId[16];
#elif __JHEXEN__
    int     parm;
    char    levelId[9];
#endif

#if !__JHEXEN__
    char    *lname, *lauthor;
#endif
    int     setupflags = DDSLF_POLYGONIZE | DDSLF_FIX_SKY | DDSLF_REVERB;

    // It begins
    levelSetup = true;

    // Reset our local global element counters.
    // Any local data should have been free'd by now (PU_LEVEL)
    numvertexes = 0;
    numsegs = 0;
    numsectors = 0;
    numsubsectors = 0;
    numnodes = 0;
    numlines = 0;
    numsides = 0;
    numthings = 0;

#if __JHEXEN__
    po_NumPolyobjs = 0;
#endif

    // Let the engine know that we are about to start setting up a
    // level.
    R_SetupLevel(NULL, DDSLF_INITIALIZE);

#if !__JHEXEN__
    totalkills = totalitems = totalsecret = 0;
#endif

    for(i = 0; i < MAXPLAYERS; i++)
    {
        players[i].killcount = players[i].secretcount = players[i].itemcount = 0;
    }
    // Initial height of PointOfView; will be set by player think.
    players[consoleplayer].plr->viewz = 1;

    S_LevelChange();

#if __JHEXEN__
    S_StartMusic("chess", true);    // Waiting-for-level-load song
#endif

    Z_FreeTags(PU_LEVEL, PU_PURGELEVEL - 1);

#if __JDOOM__
    wminfo.maxfrags = 0;
    wminfo.partime = 180;

    // Clear brain data. This is only used in Doom II map30, though.
    memset(braintargets, 0, sizeof(braintargets));
    numbraintargets = 0;
    braintargeton = 0;

    // Only used with 666/7 specials
    bossKilled = false;
#endif

    P_InitThinkers();

    leveltime = actual_leveltime = 0;

    // Locate the lumps where the map data resides in.
    P_LocateMapLumps(episode, map, lumpNumbers);

#if __JDOOM__ || __JHEXEN__
    P_GetMapLumpName(episode, map, levelId);
#elif __JHERETIC__
    strcpy(levelId, W_LumpName(lumpNumbers[0]));
#endif

    P_LoadBlockMap(lumpNumbers[0] + ML_BLOCKMAP);

    P_LoadMap(lumpNumbers[0], lumpNumbers[1], levelId);

    // Now the map data has been loaded we can update the
    // global data struct counters
    numvertexes = DD_GetInteger(DD_VERTEX_COUNT);
    numsegs = DD_GetInteger(DD_SEG_COUNT);
    numsectors = DD_GetInteger(DD_SECTOR_COUNT);
    numsubsectors = DD_GetInteger(DD_SUBSECTOR_COUNT);
    numnodes = DD_GetInteger(DD_NODE_COUNT);
    numlines = DD_GetInteger(DD_LINE_COUNT);
    numsides = DD_GetInteger(DD_SIDE_COUNT);
    numthings = DD_GetInteger(DD_THING_COUNT);

    if(lumpNumbers[1] > lumpNumbers[0])
        // We have GL nodes!
        setupflags |= DDSLF_DONT_CLIP;

    bodyqueslot = 0;
    deathmatch_p = deathmatchstarts;
    playerstart_p = playerstarts;

#if __JHERETIC__
    P_InitAmbientSound();
    P_InitMonsters();
    P_OpenWeapons();
#endif
    Con_Message("Spawn things\n");
    P_SpawnThings();
    Con_Message("Spawned things\n");
#if __JHERETIC__
    P_CloseWeapons();
#endif

    // It's imperative that this is called!
    // - dlBlockLinks initialized
    // - necessary GL data generated
    // - sky fix
    // - map info setup
#if __JHEXEN__
    // Server can't be initialized before PO_Init is done, but PO_Init
    // can't be done until SetupLevel is called...
    R_SetupLevel(levelId, setupflags | DDSLF_NO_SERVER);

    // Initialize polyobjs.
    Con_Message("PO init\n");
    PO_Init(lumpNumbers[0] + ML_THINGS);   // Initialize the polyobjs

    // Now we can init the server.
    Con_Message("Init server\n");
    R_SetupLevel(levelId, DDSLF_SERVER_ONLY);

    Con_Message("Load ACS scripts\n");
    P_LoadACScripts(lumpNumbers[0] + ML_BEHAVIOR); // ACS object code
#else

    R_SetupLevel(levelId, setupflags);

#endif

    P_DealPlayerStarts();
    P_SpawnPlayers();

#if __JHERETIC__ || __JHEXEN__
    //
    // if deathmatch, randomly spawn the active players
    //
    TimerGame = 0;
    if(deathmatch)
    {
        parm = ArgCheck("-timer");
        if(parm && parm < Argc() - 1)
        {
            TimerGame = atoi(Argv(parm + 1)) * 35 * 60;
        }
    }
#endif
#if __JDOOM__
    // clear special respawning que
    iquehead = iquetail = 0;
#endif

    // set up world state
    P_SpawnSpecials();

    // preload graphics
    if(precache)
    {
        R_PrecacheLevel();
        R_PrecachePSprites();
    }

#if __JHEXEN__
    // Check if the level is a lightning level.
    P_InitLightning();

    SN_StopAllSequences();
#endif

    // How about some music?
    S_LevelMusic();

#if !__JHEXEN__
    // Print some info. These also appear on the screen.
    lname = (char *) DD_GetVariable(DD_MAP_NAME);
    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);

#if __JDOOM__
    // Plutonia and TNT are special cases.
    if(gamemission == pack_plut)
    {
        lname = mapnamesp[map - 1];
        lauthor = PLUT_AUTHOR;
    }
    else if(gamemission == pack_tnt)
    {
        lname = mapnamest[map - 1];
        lauthor = TNT_AUTHOR;
    }
#endif

    if(lname || lauthor)
    {
        Con_Printf("\n");
        if(lname)
            Con_FPrintf(CBLF_LIGHT | CBLF_BLUE, "%s\n", lname);
        if(lauthor)
            Con_FPrintf(CBLF_LIGHT | CBLF_BLUE, "Author: %s\n", lauthor);
        Con_Printf("\n");
    }
#endif // end if !__JHEXEN__

// Games specific level finalization
#if __JDOOM__
    // Adjust slime lower wall textures (a hack!).
    // This will hide the ugly green bright line that would otherwise be
    // visible due to texture repeating and interpolation.
 #ifdef TODO_MAP_UPDATE
    for(i = 0; i < numlines; i++)
    {
        int     k;
        int     lumpnum = R_TextureNumForName("NUKE24");

        for(k = 0; k < 2; k++)
            if(lines[i].sidenum[k] != NO_INDEX)
            {
                side_t *sdef = &sides[lines[i].sidenum[k]];

                if(sdef->bottomtexture == lumpnum && sdef->midtexture == 0)
                    sdef->rowoffset += FRACUNIT;
            }
    }
#endif

#elif __JHERETIC__
    // Do some fine tuning with mobj placement and orientation.
    P_MoveThingsOutOfWalls();
    P_TurnGizmosAwayFromDoors();

#elif __JHEXEN__
    // Load colormap and set the fullbright flag
    i = P_GetMapFadeTable(gamemap);
    if(i == W_GetNumForName("COLORMAP"))
    {
        // We don't want fog in this case.
        GL_UseFog(false);
    }
    else
    {
        // Probably fog ... don't use fullbright sprites
        if(i == W_GetNumForName("FOGMAP"))
        {
            // Tell the renderer to turn on the fog.
            GL_UseFog(true);
        }
    }

    P_TurnTorchesToFaceWalls();

    // Print a message in the console about this level.
    Con_Message("Map %d (%d): %s\n", P_GetMapWarpTrans(map), map,
                P_GetMapName(map));
#endif

    // Someone may want to do something special now that the level has been
    // fully set up.
    R_SetupLevel(levelId, DDSLF_FINALIZE);

    // It ends
    levelSetup = false;
}


/*
 * Spawns all THINGS that belong in the map.
 * Should really be moved to Common/p_start.c
 *
 * Polyobject anchors etc are still handled in PO_Init()
 */
static void P_SpawnThings(void)
{
    int i;
    thing_t *th;
#if __JDOOM__
    boolean spawn;
#elif __JHEXEN__
    int     playerCount;
    int     deathSpotsCount;
#endif

#ifdef TODO_MAP_UPDATE
    th = things;
    for(i = 0; i < numthings; i++, th++)
    {
#if __JDOOM__
        // Do not spawn cool, new stuff if !commercial
        spawn = true;
        if(gamemode != commercial)
        {
            switch (SHORT(th->type))
            {
            case 68:            // Arachnotron
            case 64:            // Archvile
            case 88:            // Boss Brain
            case 89:            // Boss Shooter
            case 69:            // Hell Knight
            case 67:            // Mancubus
            case 71:            // Pain Elemental
            case 74:            // MegaSphere
            case 65:            // Former Human Commando
            case 66:            // Revenant
            case 84:            // Wolf SS
                spawn = false;
                break;
            }
        }
        if(spawn == false)
            break;
#endif
Con_Message("spawn %i of %i\n",i, numthings);
        P_SpawnMapThing(th);
    }
#endif

#if __JHEXEN__
    // FIXME: This stuff should be moved!
    P_CreateTIDList();
    P_InitCreatureCorpseQueue(false);   // false = do NOT scan for corpses

    if(!deathmatch)
    {                           // Don't need to check deathmatch spots
        return;
    }

    playerCount = 0;
    for(i = 0; i < MAXPLAYERS; i++)
    {
        playerCount += players[i].plr->ingame;
    }

    deathSpotsCount = deathmatch_p - deathmatchstarts;
    if(deathSpotsCount < playerCount)
    {
        Con_Error("P_LoadThings: Player count (%d) exceeds deathmatch "
                  "spots (%d)", playerCount, deathSpotsCount);
    }
#endif

    // We're finished with the temporary thing list
    //Z_Free(things);
}

#if __JHERETIC__ || __JHEXEN__
fixed_t P_PointLineDistance(line_t *line, fixed_t x, fixed_t y,
                            fixed_t *offset)
{
    float   a[2], b[2], c[2], d[2], len;

    a[VX] = FIX2FLT(line->v1->x);
    a[VY] = FIX2FLT(line->v1->y);

    b[VX] = FIX2FLT(line->v2->x);
    b[VY] = FIX2FLT(line->v2->y);

    c[VX] = FIX2FLT(x);
    c[VY] = FIX2FLT(y);

    d[VX] = b[VX] - a[VX];
    d[VY] = b[VY] - a[VY];
    len = sqrt(d[VX] * d[VX] + d[VY] * d[VY]);  // Accurate.

    if(offset)
        *offset =
            FRACUNIT * ((a[VY] - c[VY]) * (a[VY] - b[VY]) -
                        (a[VX] - c[VX]) * (b[VX] - a[VX])) / len;
    return FRACUNIT * ((a[VY] - c[VY]) * (b[VX] - a[VX]) -
                       (a[VX] - c[VX]) * (b[VY] - a[VY])) / len;
}
#endif

#if __JDOOM__
/*
 * Returns true if the specified ep/map exists in a WAD.
 */
boolean P_MapExists(int episode, int map)
{
    char    buf[20];

    P_GetMapLumpName(episode, map, buf);
    return W_CheckNumForName(buf) >= 0;
}
#endif

#if __JHERETIC__
char *P_GetShortLevelName(int episode, int map)
{
    char   *name = P_GetLevelName(episode, map);
    char   *ptr;

    // Remove the "ExMx:" from the beginning.
    ptr = strchr(name, ':');
    if(!ptr)
        return name;
    name = ptr + 1;
    while(*name && isspace(*name))
        name++;                 // Skip any number of spaces.
    return name;
}

char   *P_GetLevelName(int episode, int map)
{
    char    id[10];
    ddmapinfo_t info;

    // Compose the level identifier.
    sprintf(id, "E%iM%i", episode, map);

    // Get the map info definition.
    if(!Def_Get(DD_DEF_MAP_INFO, id, &info))
    {
        // There is no map information for this map...
        return "";
    }
    return info.name;
}

/*
 * Only affects torches, which are often placed inside walls in the
 * original maps. The DOOM engine allowed these kinds of things but
 * a Z-buffer doesn't.
 */
void P_MoveThingsOutOfWalls()
{
#define MAXLIST 200
    sector_t *sec;
    mobj_t *iter;
    int     i, k, t;
    line_t *closestline = NULL, *li;
    fixed_t closestdist, dist, off, linelen, minrad;
    mobj_t *tlist[MAXLIST];

    for(sec = sectors, i = 0; i < numsectors; i++, sec++)
    {
        memset(tlist, 0, sizeof(tlist));
        // First all the things to move.
        for(k = 0, iter = sec->thinglist; iter; iter = iter->snext)
            // Wall torches are most often seen inside walls.
            if(iter->type == MT_MISC10)
                tlist[k++] = iter;
        // Move the things out of walls.
        for(t = 0; (iter = tlist[t]) != NULL; t++)
        {
            minrad = iter->radius / 2;
            closestline = NULL;
            for(k = 0; k < sec->linecount; k++)
            {
                li = sec->Lines[k];
                if(li->backsector)
                    continue;
                linelen =
                    P_ApproxDistance(li->v2->x - li->v1->x,
                                     li->v2->y - li->v1->y);
                dist = P_PointLineDistance(li, iter->x, iter->y, &off);
                if(off > -minrad && off < linelen + minrad &&
                   (!closestline || dist < closestdist) && dist >= 0)
                {
                    closestdist = dist;
                    closestline = li;
                }
            }
            if(closestline && closestdist < minrad)
            {
                float   dx, dy, offlen = FIX2FLT(minrad - closestdist);
                float   len;

                li = closestline;
                dy = -FIX2FLT(li->v2->x - li->v1->x);
                dx = FIX2FLT(li->v2->y - li->v1->y);
                len = sqrt(dx * dx + dy * dy);
                dx *= offlen / len;
                dy *= offlen / len;
                P_UnsetThingPosition(iter);
                iter->x += FRACUNIT * dx;
                iter->y += FRACUNIT * dy;
                P_SetThingPosition(iter);
            }
        }
    }
}

/*
 * Fails in some places, but works most of the time.
 */
void P_TurnGizmosAwayFromDoors()
{
#define MAXLIST 200
    sector_t *sec;
    mobj_t *iter;
    int     i, k, t;
    line_t *closestline = NULL, *li;
    fixed_t closestdist, dist, off, linelen;    //, minrad;
    mobj_t *tlist[MAXLIST];

    for(sec = sectors, i = 0; i < numsectors; i++, sec++)
    {
        memset(tlist, 0, sizeof(tlist));

        // First all the things to process.
        for(k = 0, iter = sec->thinglist; k < MAXLIST - 1 && iter;
            iter = iter->snext)
        {
            if(iter->type == MT_KEYGIZMOBLUE || iter->type == MT_KEYGIZMOGREEN
               || iter->type == MT_KEYGIZMOYELLOW)
                tlist[k++] = iter;
        }

        // Turn to face away from the nearest door.
        for(t = 0; (iter = tlist[t]) != NULL; t++)
        {
            closestline = NULL;
            for(k = 0; k < numlines; k++)
            {
                li = lines + k;
                // It must be a special line with a back sector.
                if(!li->backsector ||
                   (li->special != 32 && li->special != 33 && li->special != 34
                    && li->special != 26 && li->special != 27 &&
                    li->special != 28))
                    continue;
                linelen =
                    P_ApproxDistance(li->v2->x - li->v1->x,
                                     li->v2->y - li->v1->y);
                dist = abs(P_PointLineDistance(li, iter->x, iter->y, &off));
                if(!closestline || dist < closestdist)
                {
                    closestdist = dist;
                    closestline = li;
                }
            }
            if(closestline)
            {
                iter->angle =
                    R_PointToAngle2(closestline->v1->x, closestline->v1->y,
                                    closestline->v2->x,
                                    closestline->v2->y) - ANG90;
            }
        }
    }
}
#endif

#if __JHEXEN__
/*
 * Pretty much the same as P_TurnGizmosAwayFromDoors()
 * TODO: Merge them together
 */
void P_TurnTorchesToFaceWalls()
{
#define MAXLIST 200
    sector_t *sec;
    mobj_t *iter;
    int     i, k, t;
    line_t *closestline = NULL, *li;
    fixed_t closestdist, dist, off, linelen, minrad;
    mobj_t *tlist[MAXLIST];

    for(sec = sectors, i = 0; i < numsectors; i++, sec++)
    {
        memset(tlist, 0, sizeof(tlist));

        // First all the things to process.
        for(k = 0, iter = sec->thinglist; k < MAXLIST - 1 && iter;
            iter = iter->snext)
        {
            if(iter->type == MT_ZWALLTORCH ||
               iter->type == MT_ZWALLTORCH_UNLIT)
                tlist[k++] = iter;
        }

        // Turn to face away from the nearest wall.
        for(t = 0; (iter = tlist[t]) != NULL; t++)
        {
            minrad = iter->radius;
            closestline = NULL;
            for(k = 0; k < sec->linecount; k++)
            {
                li = sec->Lines[k];
                if(li->backsector)
                    continue;
                linelen =
                    P_ApproxDistance(li->v2->x - li->v1->x,
                                     li->v2->y - li->v1->y);
                dist = P_PointLineDistance(li, iter->x, iter->y, &off);
                if(off > -minrad && off < linelen + minrad &&
                   (!closestline || dist < closestdist) && dist >= 0)
                {
                    closestdist = dist;
                    closestline = li;
                }
            }
            if(closestline && closestdist < minrad)
            {
                iter->angle =
                    R_PointToAngle2(closestline->v1->x, closestline->v1->y,
                                    closestline->v2->x,
                                    closestline->v2->y) - ANG90;
            }
        }
    }
}
#endif
