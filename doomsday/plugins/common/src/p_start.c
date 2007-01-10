/**
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 *
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2006 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © Raven Software, Corp.
 *\author Copyright © 1993-1996 by id Software, Inc.
 */

/* $Id$
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

#include <math.h>

#if  __DOOM64TC__
#  include "doom64tc.h"
#  include "r_common.h"
#  include "hu_stuff.h"
#elif __WOLFTC__
#  include "wolftc.h"
#  include "r_common.h"
#  include "hu_stuff.h"
#elif __JDOOM__
#  include "jdoom.h"
#  include "r_common.h"
#  include "hu_stuff.h"
#elif __JHERETIC__
#  include "jheretic.h"
#  include "r_common.h"
#  include "hu_stuff.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "p_mapsetup.h"
#include "d_net.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

#if __JDOOM__ || __JHERETIC__
#  define TELEPORTSOUND   sfx_telept
#  define MAX_START_SPOTS 4 // Maximum number of different player starts.
#else
#  define TELEPORTSOUND   SFX_TELEPORT
#  define MAX_START_SPOTS 8
#endif

// Time interval for item respawning.
#define ITEMQUESIZE     128

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Maintain single and multi player starting spots.
thing_t  deathmatchstarts[MAX_DM_STARTS];
thing_t *deathmatch_p;

thing_t *playerstarts;
int      numPlayerStarts = 0;

thing_t *things;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int numPlayerStartsMax = 0;

// CODE --------------------------------------------------------------------

/*
 * Initializes various playsim related data
 */
void P_Init(void)
{
#if __JHEXEN__
    P_InitMapInfo();
#endif

    P_InitSwitchList();
    P_InitPicAnims();

    P_InitTerrainTypes();
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    P_InitLava();
#endif

#if __JDOOM__
    // Maximum health and armor points.
    maxhealth = 100;
    healthlimit = 200;

    godmodehealth = 100;
    megaspherehealth = 200;

# if __DOOM64TC_
    soulspherehealth = 50;
# else
    soulspherehealth = 100;
# endif
    soulspherelimit = 200;

    armorpoints[0] = 100;
    armorpoints[1] = armorpoints[2] = armorpoints[3] = 200;
    armorclass[0] = 1;
    armorclass[1] = armorclass[2] = armorclass[3] = 2;

    GetDefInt("Player|Max Health", &maxhealth);
    GetDefInt("Player|Health Limit", &healthlimit);
    GetDefInt("Player|God Health", &godmodehealth);

    GetDefInt("Player|Green Armor", &armorpoints[0]);
    GetDefInt("Player|Blue Armor", &armorpoints[1]);
    GetDefInt("Player|IDFA Armor", &armorpoints[2]);
    GetDefInt("Player|IDKFA Armor", &armorpoints[3]);

    GetDefInt("Player|Green Armor Class", &armorclass[0]);
    GetDefInt("Player|Blue Armor Class", &armorclass[1]);
    GetDefInt("Player|IDFA Armor Class", &armorclass[2]);
    GetDefInt("Player|IDKFA Armor Class", &armorclass[3]);

    GetDefInt("MegaSphere|Give|Health", &megaspherehealth);

    GetDefInt("SoulSphere|Give|Health", &soulspherehealth);
    GetDefInt("SoulSphere|Give|Health Limit", &soulspherelimit);
#endif
}

int P_RegisterPlayerStart(thing_t * mthing)
{
    // Enough room?
    if(++numPlayerStarts > numPlayerStartsMax)
    {
        // Allocate more memory.
        numPlayerStartsMax *= 2;
        if(numPlayerStartsMax < numPlayerStarts)
            numPlayerStartsMax = numPlayerStarts;

        playerstarts =
            Z_Realloc(playerstarts, sizeof(thing_t) * numPlayerStartsMax, PU_LEVEL);
    }

    // Copy the properties of this thing
    memcpy(&playerstarts[numPlayerStarts -1], mthing, sizeof(thing_t));
    return numPlayerStarts; // == index + 1
}

void P_FreePlayerStarts(void)
{
    deathmatch_p = deathmatchstarts;

    if(playerstarts)
        Z_Free(playerstarts);
    playerstarts = NULL;

    numPlayerStarts = numPlayerStartsMax = 0;
}

/*
 * Gives all the players in the game a playerstart.
 * Only needed in co-op games (start spots are random in deathmatch).
 */
void P_DealPlayerStarts(int group)
{
    int     i, k;
    player_t *pl;
    thing_t *mt;
    int     spotNumber;

    if(!numPlayerStarts)
        Con_Error("No playerstarts!\n");

    // First assign one start per player, only accepting perfect matches.
    for(i = 0, pl = players; i < MAXPLAYERS; i++, pl++)
    {
        if(!pl->plr->ingame)
            continue;

        // The number of the start spot this player will use.
        spotNumber = i % MAX_START_SPOTS;
        pl->startspot = -1;

        for(k = 0, mt = playerstarts; k < numPlayerStarts; k++, mt++)
        {
            if(spotNumber == mt->type - 1)
            {
                // This is a match.
#if __JHEXEN__
                if(mt->arg1 == group) // Group must match too.
                    pl->startspot = k;
#else
                pl->startspot = k;
#endif
                // Keep looking.
            }
        }

        // If still without a start spot, assign one randomly.
        if(pl->startspot == -1)
        {
            // It's likely that some players will get the same start
            // spots.
            pl->startspot = M_Random() % numPlayerStarts;
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
 */
boolean P_CheckSpot(int playernum, thing_t *mthing, boolean doTeleSpark)
{
    fixed_t     pos[3];
    ddplayer_t *ddplyr = players[playernum].plr;
    boolean     using_dummy = false;

    pos[VX] = mthing->x << FRACBITS;
    pos[VY] = mthing->y << FRACBITS;

    if(!ddplyr->mo)
    {
        // The player doesn't have a mobj. Let's create a dummy.
        G_DummySpawnPlayer(playernum);
        using_dummy = true;
    }

    ddplyr->mo->flags2 &= ~MF2_PASSMOBJ;

    if(!P_CheckPosition(ddplyr->mo, pos[VX], pos[VY]))
    {
        ddplyr->mo->flags2 |= MF2_PASSMOBJ;

        if(using_dummy)
        {
            P_RemoveMobj(ddplyr->mo);
            ddplyr->mo = NULL;
        }
        return false;
    }
    ddplyr->mo->flags2 |= MF2_PASSMOBJ;

    if(using_dummy)
    {
        P_RemoveMobj(ddplyr->mo);
        ddplyr->mo = NULL;
    }

#if __JDOOM__
    G_QueueBody(ddplyr->mo);
#endif

    if(doTeleSpark) // spawn a teleport fog
    {
        mobj_t *mo;
        fixed_t an = (ANG45 * (mthing->angle / 45)) >> ANGLETOFINESHIFT;

        mo = P_SpawnTeleFog(pos[VX] + 20 * finecosine[an],
                            pos[VY] + 20 * finesine[an]);

        // don't start sound on first frame
        if(players[consoleplayer].plr->viewz != 1)
            S_StartSound(TELEPORTSOUND, mo);
    }

    return true;
}

/*
 * Try to spawn close to the mapspot. Returns false if no clear spot
 * was found.
 */
boolean P_FuzzySpawn(thing_t * spot, int playernum, boolean doTeleSpark)
{
    int     i, k, x, y;
    int     offset = 33;        // Player radius = 16
    thing_t place;

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
 * Spawns all THINGS that belong in the map.
 *
 * Polyobject anchors etc are still handled in PO_Init()
 */
void P_SpawnThings(void)
{
    int i;
    thing_t *th;
#if __JDOOM__
    boolean spawn;
#elif __JHEXEN__
    int     playerCount;
    int     deathSpotsCount;
#endif

    for(i = 0; i < numthings; i++)
    {
        th = &things[i];
#if __JDOOM__
        // Do not spawn cool, new stuff if !commercial
        spawn = true;
        if(gamemode != commercial)
        {
            switch(th->type)
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
        if(!spawn)
            continue;
#endif
        P_SpawnMapThing(th);
    }

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
        if(players[i].plr->ingame)
            playerCount++;
    }

    deathSpotsCount = deathmatch_p - deathmatchstarts;
    if(deathSpotsCount < playerCount)
    {
        Con_Error("P_LoadThings: Player count (%d) exceeds deathmatch "
                  "spots (%d)", playerCount, deathSpotsCount);
    }
#endif

    // We're finished with the temporary thing list
    Z_Free(things);
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
            // Spawn all unused player starts. This will create 'zombies'.
            // FIXME: Also in netgames?
            for(i = 0; i < numPlayerStarts; ++i)
                if(players[0].startspot != i && playerstarts[i].type == 1)
                {
                    P_SpawnPlayer(&playerstarts[i], 0);
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

/*
 *  Spawns the given player at a dummy place.
 */
void G_DummySpawnPlayer(int playernum)
{
    thing_t faraway;

    faraway.x = faraway.y = DDMAXSHORT;
    faraway.angle = (short) 0;
    P_SpawnPlayer(&faraway, playernum);
}

/*
 * Spawns a player at one of the random death match spots
 * called at level load and each death
 */
void G_DeathMatchSpawnPlayer(int playernum)
{
    int     i = 0, j;
    int     selections;
    boolean using_dummy = false;
    ddplayer_t *pl = players[playernum].plr;

    // Spawn player initially at a distant location.
    if(!pl->mo)
    {
        G_DummySpawnPlayer(playernum);
        using_dummy = true;
    }

    // Now let's find an available deathmatch start.
    selections = deathmatch_p - deathmatchstarts;
    if(selections < 2)
        Con_Error("Only %i deathmatch spots, 2 required", selections);

    for(j = 0; j < 20; j++)
    {
        i = P_Random() % selections;
        if(P_CheckSpot(playernum, &deathmatchstarts[i], true))
        {
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
            deathmatchstarts[i].type = playernum + 1;
#endif
            break;
        }
    }

    if(using_dummy)
    {
        // Destroy the dummy.
        P_RemoveMobj(pl->mo);
        pl->mo = NULL;
    }

    P_SpawnPlayer(&deathmatchstarts[i], playernum);

#if __JDOOM__ || __JHERETIC__
    // Gib anything at the spot.
    P_Telefrag(players[playernum].plr->mo);
#endif
}

/*
 * Returns the correct start for the player. The start
 * is in the given group, or zero if no such group exists.
 *
 * With jDoom groups arn't used at all.
 */
thing_t *P_GetPlayerStart(int group, int pnum)
{
#if __JDOOM__ || __JHERETIC__
    return &playerstarts[players[pnum].startspot];
#else
    int i;

    thing_t *mt, *g0choice = NULL;

    for(i = 0; i < numPlayerStarts; ++i)
    {
        mt = &playerstarts[i];

        if(mt->arg1 == group && mt->type - 1 == pnum)
            return mt;
        if(!mt->arg1 && mt->type - 1 == pnum)
            g0choice = mt;
    }
    // Return the group zero choice.
    return g0choice;
#endif
}

#if __JHERETIC__ || __JHEXEN__
fixed_t P_PointLineDistance(line_t *line, fixed_t x, fixed_t y,
                            fixed_t *offset)
{
    float   a[2], b[2], c[2], d[2], len;

    P_GetFloatpv(line, DMU_VERTEX1_XY, a);
    P_GetFloatpv(line, DMU_VERTEX2_XY, b);

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

#if __JHERETIC__
/*
 * Only affects torches, which are often placed inside walls in the
 * original maps. The DOOM engine allowed these kinds of things but
 * a Z-buffer doesn't.
 */
void P_MoveThingsOutOfWalls(void)
{
#define MAXLIST 200
    sector_t   *sec;
    mobj_t     *iter;
    uint        i, l;
    int         k, t;
    line_t     *closestline = NULL, *li;
    fixed_t     closestdist = 0, dist, off, linelen, minrad;
    mobj_t     *tlist[MAXLIST];

    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        memset(tlist, 0, sizeof(tlist));

        // First all the things to process.
        for(k = 0, iter = P_GetPtrp(sec, DMU_THINGS);
            k < MAXLIST - 1 && iter; iter = iter->snext)
        {
            // Wall torches are most often seen inside walls.
            if(iter->type == MT_MISC10)
                tlist[k++] = iter;
        }

        // Move the things out of walls.
        for(t = 0; (iter = tlist[t]) != NULL; ++t)
        {
            uint sectorLineCount = P_GetIntp(sec, DMU_LINE_COUNT);

            minrad = iter->radius / 2;
            closestline = NULL;

            for(l = 0; l < sectorLineCount; ++l)
            {
                li = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | l);
                if(P_GetPtrp(li, DMU_BACK_SECTOR))
                    continue;
                linelen =
                    P_ApproxDistance(P_GetFixedp(li, DMU_DX),
                                     P_GetFixedp(li, DMU_DY));
                dist = P_PointLineDistance(li, iter->pos[VX], iter->pos[VY], &off);
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
                dy = -P_GetFloatp(li, DMU_DX);
                dx = P_GetFloatp(li, DMU_DY);
                len = sqrt(dx * dx + dy * dy);
                dx *= offlen / len;
                dy *= offlen / len;
                P_UnsetThingPosition(iter);
                iter->pos[VX] += FRACUNIT * dx;
                iter->pos[VY] += FRACUNIT * dy;
                P_SetThingPosition(iter);
            }
        }
    }
}

/*
 * Fails in some places, but works most of the time.
 */
void P_TurnGizmosAwayFromDoors(void)
{
#define MAXLIST 200
    sector_t   *sec;
    mobj_t     *iter;
    uint        i, l;
    int         k, t;
    line_t     *closestline = NULL, *li;
    xline_t    *xli;
    fixed_t     closestdist = 0, dist, off, linelen;    //, minrad;
    mobj_t     *tlist[MAXLIST];

    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        memset(tlist, 0, sizeof(tlist));

        // First all the things to process.
        for(k = 0, iter = P_GetPtrp(sec, DMU_THINGS);
            k < MAXLIST - 1 && iter; iter = iter->snext)
        {
            if(iter->type == MT_KEYGIZMOBLUE || iter->type == MT_KEYGIZMOGREEN
               || iter->type == MT_KEYGIZMOYELLOW)
                tlist[k++] = iter;
        }

        // Turn to face away from the nearest door.
        for(t = 0; (iter = tlist[t]) != NULL; ++t)
        {
            closestline = NULL;
            for(l = 0; l < numlines; ++l)
            {
                li = P_ToPtr(DMU_LINE, l);

                if(P_GetPtrp(li, DMU_BACK_SECTOR))
                    continue;

                xli = P_XLine(li);

                // It must be a special line with a back sector.
                if((xli->special != 32 && xli->special != 33 && xli->special != 34
                    && xli->special != 26 && xli->special != 27 &&
                    xli->special != 28))
                    continue;

                linelen =
                    P_ApproxDistance(P_GetFixedp(li, DMU_DX),
                                     P_GetFixedp(li, DMU_DY));

                dist = abs(P_PointLineDistance(li, iter->pos[VX], iter->pos[VY], &off));
                if(!closestline || dist < closestdist)
                {
                    closestdist = dist;
                    closestline = li;
                }
            }
            if(closestline)
            {
                iter->angle =
                    R_PointToAngle2(P_GetFixedp(closestline, DMU_VERTEX1_X),
                                    P_GetFixedp(closestline, DMU_VERTEX1_Y),
                                    P_GetFixedp(closestline, DMU_VERTEX2_X),
                                    P_GetFixedp(closestline, DMU_VERTEX2_Y)) - ANG90;
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
void P_TurnTorchesToFaceWalls(void)
{
#define MAXLIST 200
    sector_t   *sec;
    mobj_t     *iter;
    uint        i, l;
    int         k, t;
    line_t     *closestline = NULL, *li;
    fixed_t     closestdist = 0, dist, off, linelen, minrad;
    mobj_t     *tlist[MAXLIST];

    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        memset(tlist, 0, sizeof(tlist));

        // First all the things to process.
        for(k = 0, iter = P_GetPtrp(sec, DMU_THINGS);
            k < MAXLIST - 1 && iter; iter = iter->snext)
        {
            if(iter->type == MT_ZWALLTORCH ||
               iter->type == MT_ZWALLTORCH_UNLIT)
                tlist[k++] = iter;
        }

        // Turn to face away from the nearest wall.
        for(t = 0; (iter = tlist[t]) != NULL; t++)
        {
            uint sectorLineCount = P_GetIntp(sec, DMU_LINE_COUNT);

            minrad = iter->radius;
            closestline = NULL;
            for(l = 0; l < sectorLineCount; ++l)
            {
                li = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | l);
                if(P_GetPtrp(li, DMU_BACK_SECTOR))
                    continue;
                linelen =
                    P_ApproxDistance(P_GetFixedp(li, DMU_DX),
                                     P_GetFixedp(li, DMU_DY));
                dist = P_PointLineDistance(li, iter->pos[VX], iter->pos[VY], &off);
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
                    R_PointToAngle2(P_GetFixedp(closestline, DMU_VERTEX1_X),
                                    P_GetFixedp(closestline, DMU_VERTEX1_Y),
                                    P_GetFixedp(closestline, DMU_VERTEX2_X),
                                    P_GetFixedp(closestline, DMU_VERTEX2_Y)) - ANG90;
            }
        }
    }
}
#endif
