/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * p_setup.c: Common map setup routines.
 * Management of extended map data objects (eg xlines) is done here
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "jDoom/doomdef.h"
#  include "jDoom/doomstat.h"
#  include "jDoom/d_config.h"
#  include "jDoom/p_local.h"
#  include "jDoom/s_sound.h"
#  include "hu_stuff.h"
#elif __JHERETIC__
#  include <math.h>
#  include <ctype.h>              // has isspace
#  include "jHeretic/Doomdef.h"
#  include "jHeretic/P_local.h"
#  include "jHeretic/Soundst.h"
#  include "jHeretic/S_sound.h"
#elif __JHEXEN__
#  include <math.h>
#  include "h2def.h"
#  include "jHexen/p_local.h"
#elif __JSTRIFE__
#  include "h2def.h"
#  include "jStrife/p_local.h"
#endif

#include "Common/dmu_lib.h"
#include "r_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    InitMapInfo(void);
void    P_SpawnThings(void);

#if __JHERETIC__
void P_TurnGizmosAwayFromDoors();
void P_MoveThingsOutOfWalls();
#elif __JHEXEN__
void P_TurnTorchesToFaceWalls();
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_ResetWorldState(void);
static void P_FinalizeLevel(void);
static void P_PrintMapBanner(int episode, int map);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#if __JDOOM__
extern int actual_leveltime;
extern boolean bossKilled;
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int numvertexes;
int numsegs;
int numsectors;
int numsubsectors;
int numnodes;
int numlines;
int numsides;
int numthings;

// Our private map data structures
xsector_t *xsectors;
int        numxsectors;
xline_t   *xlines;
int        numxlines;

// If true we are in the process of setting up a level
boolean levelSetup;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Converts a line to an xline.
 */
xline_t* P_XLine(line_t* line)
{
    // Is it a dummy?
    if(P_IsDummy(line))
        return P_DummyData(line);
    else
        return &xlines[P_ToIndex(DMU_LINE, line)];
}

/*
 * Converts a sector to an xsector.
 */
xsector_t* P_XSector(sector_t* sector)
{
    // Is it a dummy?
    if(P_IsDummy(sector))
        return P_DummyData(sector);
    else
        return &xsectors[P_ToIndex(DMU_SECTOR, sector)];
}

/*
 * Given a subsector - find its parent xsector.
 */
xsector_t* P_XSectorOfSubsector(subsector_t* sub)
{
    sector_t* sec = P_GetPtrp(DMU_SUBSECTOR, sub, DMU_SECTOR);

    // Is it a dummy?
    if(P_IsDummy(sec))
        return P_DummyData(sec);
    else
        return &xsectors[P_ToIndex(DMU_SECTOR, sec)];
}

/*
 * Create and initialize our private thing data array
 */
void P_SetupForThings(int num)
{
    int oldNum = numthings;

    numthings += num;

    if(oldNum > 0)
        things = Z_Realloc(things, numthings * sizeof(thing_t), PU_LEVEL);
    else
        things = Z_Malloc(numthings * sizeof(thing_t), PU_LEVEL, 0);

    memset(things + oldNum, 0, num * sizeof(thing_t));
}

/*
 * Create and initialize our private line data array
 */
void P_SetupForLines(int num)
{
    int oldNum = numlines;

    numlines += num;

    if(oldNum > 0)
        xlines = Z_Realloc(xlines, numlines * sizeof(xline_t), PU_LEVEL);
    else
        xlines = Z_Malloc(numlines * sizeof(xline_t), PU_LEVEL, 0);

    memset(xlines + oldNum, 0, num * sizeof(xline_t));
}

void P_SetupForSides(int num)
{
    // Nothing to do
}

/*
 * Create and initialize our private sector data array
 */
void P_SetupForSectors(int num)
{
    int oldNum = numsectors;

    numsectors += num;

    if(oldNum > 0)
        xsectors = Z_Realloc(xsectors, numsectors * sizeof(xsector_t), PU_LEVEL);
    else
        xsectors = Z_Malloc(numsectors * sizeof(xsector_t), PU_LEVEL, 0);

    memset(xsectors + oldNum, 0, num * sizeof(xsector_t));
}

/*
 * Loads map and glnode data for the requested episode and map
 */
void P_SetupLevel(int episode, int map, int playermask, skill_t skill)
{
    int     setupflags = DDSLF_POLYGONIZE | DDSLF_FIX_SKY | DDSLF_REVERB;
    int     lumpNumbers[2];
    char    levelId[9];

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

    // Map thing data for the level IS stored game-side.
    // However, Doomsday tells us how many things there are.
    numthings = 0;

    // The engine manages polyobjects, so reset the count.
    DD_SetInteger(DD_POLYOBJ_COUNT, 0);

    P_ResetWorldState();

    // Let the engine know that we are about to start setting up a
    // level.
    R_SetupLevel(NULL, DDSLF_INITIALIZE);

    // Initialize The Logical Sound Manager.
    S_LevelChange();

#if __JHEXEN__
    S_StartMusic("chess", true);    // Waiting-for-level-load song
#endif

    Z_FreeTags(PU_LEVEL, PU_PURGELEVEL - 1);

    P_InitThinkers();

    // Locate the lumps where the map data resides in.
    P_LocateMapLumps(episode, map, lumpNumbers);

    P_GetMapLumpName(episode, map, levelId);

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

#if __JHERETIC__
    P_InitAmbientSound();
    P_InitMonsters();
    P_OpenWeapons();
#endif
    Con_Message("Spawn things\n");
    P_SpawnThings();

    // killough 3/26/98: Spawn icon landings:
#if __JDOOM__
    if(gamemode == commercial)
        P_SpawnBrainTargets();
#endif

#if __JHERETIC__
    P_CloseWeapons();
#endif

    if(lumpNumbers[1] > lumpNumbers[0])
        setupflags |= DDSLF_DONT_CLIP; // We have GL nodes!

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

    // set up world state
    P_SpawnSpecials();

    // preload graphics
    if(precache)
    {
        R_PrecacheLevel();
        R_PrecachePSprites();
    }

    S_LevelMusic();

    P_FinalizeLevel();

    // Someone may want to do something special now that
    // the level has been fully set up.
    R_SetupLevel(levelId, DDSLF_FINALIZE);

    P_PrintMapBanner(episode, map);

    // It ends
    levelSetup = false;
}

/*
 * Called during level setup when beginning to load a new map.
 */
static void P_ResetWorldState(void)
{
    int i;
#if __JHERETIC__ || _JHEXEN__
    int     parm;
#endif

#if __JDOOM__
    wminfo.maxfrags = 0;
    wminfo.partime = 180;

    // Only used with 666/7 specials
    bossKilled = false;

    // clear special respawning que
    iquehead = iquetail = 0;
#endif

#if !__JHEXEN__
    totalkills = totalitems = totalsecret = 0;
#endif

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

    // Initial height of PointOfView; will be set by player think.
    players[consoleplayer].plr->viewz = 1;

    for(i = 0; i < MAXPLAYERS; i++)
    {
        players[i].killcount = players[i].secretcount = players[i].itemcount = 0;
    }

    bodyqueslot = 0;
    deathmatch_p = deathmatchstarts;
    playerstart_p = playerstarts;

    leveltime = actual_leveltime = 0;
}

/*
 * Do any level finalization including any game-specific stuff.
 */
static void P_FinalizeLevel(void)
{
#if __JDOOM__
    int i;

    // Adjust slime lower wall textures (a hack!).
    // This will hide the ugly green bright line that would otherwise be
    // visible due to texture repeating and interpolation.

    for(i = 0; i < numlines; i++)
    {
        side_t* side;
        int     lumpnum = R_TextureNumForName("NUKE24");
        fixed_t yoff;

        side = P_GetPtr(DMU_LINE, i, DMU_SIDE0);
        yoff = P_GetFixedp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_Y);

        if(P_GetIntp(DMU_SIDE, side, DMU_BOTTOM_TEXTURE) == lumpnum &&
           P_GetIntp(DMU_SIDE, side, DMU_MIDDLE_TEXTURE) == 0)
            P_SetFixedp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_Y, yoff + FRACUNIT);

        side = P_GetPtr(DMU_LINE, i, DMU_SIDE1);
        yoff = P_GetFixedp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_Y);

        if(P_GetIntp(DMU_SIDE, side, DMU_BOTTOM_TEXTURE) == lumpnum &&
           P_GetIntp(DMU_SIDE, side, DMU_MIDDLE_TEXTURE) == 0)
            P_SetFixedp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_Y, yoff + FRACUNIT);
    }

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

    // Check if the level is a lightning level.
    P_InitLightning();

    SN_StopAllSequences();
#endif
}

/*
 * Prints a banner to the console containing information
 * pertinent to the current map (eg map name, author...).
 */
static void P_PrintMapBanner(int episode, int map)
{
#if !__JHEXEN__
    char    *lname, *lauthor;

    // Retrieve the name and author strings from the engine.
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
#else
    char lname[64];
    boolean lauthor = false;

    sprintf(lname, "Map %d (%d): %s", P_GetMapWarpTrans(map), map,
            P_GetMapName(map));
#endif

    Con_Printf("\n");
    if(lname)
        Con_FPrintf(CBLF_LIGHT | CBLF_BLUE, "%s\n", lname);

    if(lauthor)
        Con_FPrintf(CBLF_LIGHT | CBLF_BLUE, "Author: %s\n", lauthor);
    Con_Printf("\n");
}
