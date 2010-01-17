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
 * p_setup.c: Common map setup routines.
 *
 * Management of extended map data objects (eg xlines) is done here
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <ctype.h>  // has isspace
#include <string.h>

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
#include "p_actor.h"
#include "dmu_lib.h"
#include "r_common.h"
#include "p_mapsetup.h"
#include "am_map.h"
#include "p_tick.h"
#include "p_start.h"
#include "hu_stuff.h"

// MACROS ------------------------------------------------------------------

#if __JDOOM64__
# define TOLIGHTIDX(c) (!((c) >> 8)? 0 : ((c) - 0x100) + 1)
#endif

// TYPES -------------------------------------------------------------------

typedef struct {
    int             gameModeBits;
    mobjtype_t      type;
} mobjtype_precachedata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void     P_FinalizeMap(gamemap_t* map);
static void     P_PrintMapBanner(int episode, int map);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Converts a line to an xline.
 */
xlinedef_t* P_ToXLine(linedef_t* line)
{
    if(!line)
        return NULL;

    // Is it a dummy?
    if(DMU_IsDummy(line))
    {
        return DMU_DummyExtraData(line);
    }
    else
    {
        gamemap_t* map = P_CurrentGameMap();
        return &map->_xLineDefs[DMU_ToIndex(line)];
    }
}

/**
 * Converts a sector to an xsector.
 */
xsector_t* P_ToXSector(sector_t* sector)
{
    if(!sector)
        return NULL;

    // Is it a dummy?
    if(DMU_IsDummy(sector))
    {
        return DMU_DummyExtraData(sector);
    }
    else
    {
        gamemap_t* map = P_CurrentGameMap();
        return &map->_xSectors[DMU_ToIndex(sector)];
    }
}

/**
 * Given a subsector - find its parent xsector.
 */
xsector_t* P_ToXSectorOfSubsector(subsector_t* subsector)
{
    sector_t* sec;

    if(!subsector)
        return NULL;

    sec = DMU_GetPtrp(subsector, DMU_SECTOR);

    // Is it a dummy?
    if(DMU_IsDummy(sec))
    {
        return DMU_DummyExtraData(sec);
    }
    else
    {
        gamemap_t* map = P_CurrentGameMap();
        return &map->_xSectors[DMU_ToIndex(sec)];
    }
}

/**
 * Given the index of an xline, return it.
 *
 * @note: This routine cannot be used with dummy lines!
 *
 * @param               Index of the xline to return.
 *
 * @return              Ptr to xlinedef_t.
 */
xlinedef_t* GameMap_XLineDef(gamemap_t* map, uint index)
{
    assert(map);
    if(index >= numlines)
        return NULL;
    return &map->_xLineDefs[index];
}

/**
 * Given the index of an xsector, return it.
 *
 * @note: This routine cannot be used with dummy sectors!
 *
 * @param               Index of the xsector to return.
 *
 * @return              Ptr to xsector_t.
 */
xsector_t* GameMap_XSector(gamemap_t* map, uint index)
{
    assert(map);
    if(index >= numsectors)
        return NULL;
    return &map->_xSectors[index];
}

/**
 * Doomsday calls this (before any data is read) for each type of map object
 * at the start of the map load process. This is to allow us (the game) to
 * do any initialization we need. For example if we maintain our own data
 * for lines (the xlines) we'll do all allocation and init here.
 *
 * @param type          (DMU object type) The id of the data type being setup.
 * @param num           The number of elements of "type" Doomsday is creating.
 */
void P_SetupForMapData(int type, uint num)
{
    switch(type)
    {
    case DMU_SECTOR:
        {
        gamemap_t* map = P_CurrentGameMap();
        if(num > 0)
            map->_xSectors = Z_Calloc(num * sizeof(xsector_t), PU_MAP, 0);
        else
            map->_xSectors = NULL;
        break;
        }
    case DMU_LINEDEF:
        {
        gamemap_t* map = P_CurrentGameMap();
        if(num > 0)
            map->_xLineDefs = Z_Calloc(num * sizeof(xlinedef_t), PU_MAP, 0);
        else
            map->_xLineDefs = NULL;
        break;
        }

    default:
        break;
    }
}

#if __JDOOM64__
static void getSurfaceColor(uint idx, float rgba[4])
{
    if(!idx)
    {
        rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1;
    }
    else
    {
        rgba[0] = P_GetObjectRecordFloat(MO_LIGHT, idx-1, MO_COLORR);
        rgba[1] = P_GetObjectRecordFloat(MO_LIGHT, idx-1, MO_COLORG);
        rgba[2] = P_GetObjectRecordFloat(MO_LIGHT, idx-1, MO_COLORB);
        rgba[3] = 1;
    }
}

typedef struct applysurfacecolorparams_s {
    sector_t*       frontSec;
    float           topColor[4];
    float           bottomColor[4];
} applysurfacecolorparams_t;

int applySurfaceColor(void* obj, void* context)
{
#define LDF_NOBLENDTOP      32
#define LDF_NOBLENDBOTTOM   64
#define LDF_BLEND           128

#define LTF_SWAPCOLORS      4

    linedef_t*          li = (linedef_t*) obj;
    applysurfacecolorparams_t* params =
        (applysurfacecolorparams_t*) context;
    byte                dFlags =
        P_GetObjectRecordByte(MO_XLINEDEF, DMU_ToIndex(li), MO_DRAWFLAGS);
    byte                tFlags =
        P_GetObjectRecordByte(MO_XLINEDEF, DMU_ToIndex(li), MO_TEXFLAGS);

    if((dFlags & LDF_BLEND) &&
       params->frontSec == DMU_GetPtrp(li, DMU_FRONT_SECTOR))
    {
        sidedef_t*          side = DMU_GetPtrp(li, DMU_SIDEDEF0);

        if(side)
        {
            int                 flags;
            float*              top, *bottom;

            top = (tFlags & LTF_SWAPCOLORS)? params->bottomColor :
                params->topColor;
            bottom = (tFlags & LTF_SWAPCOLORS)? params->topColor :
                params->bottomColor;

            DMU_SetFloatpv(side, DMU_TOP_COLOR, top);
            DMU_SetFloatpv(side, DMU_BOTTOM_COLOR, bottom);

            flags = DMU_GetIntp(side, DMU_FLAGS);
            if(!(dFlags & LDF_NOBLENDTOP))
                flags |= SDF_BLEND_TOPTOMID;
            if(!(dFlags & LDF_NOBLENDBOTTOM))
                flags |= SDF_BLEND_BOTTOMTOMID;

            DMU_SetIntp(side, DMU_FLAGS, flags);
        }
    }

    if((dFlags & LDF_BLEND) &&
       params->frontSec == DMU_GetPtrp(li, DMU_BACK_SECTOR))
    {
        sidedef_t*          side = DMU_GetPtrp(li, DMU_SIDEDEF1);

        if(side)
        {
            int                 flags;
            float*              top, *bottom;

            top = /*(tFlags & LTF_SWAPCOLORS)? params->bottomColor :*/
                params->topColor;
            bottom = /*(tFlags & LTF_SWAPCOLORS)? params->topColor :*/
                params->bottomColor;

            DMU_SetFloatpv(side, DMU_TOP_COLOR, top);
            DMU_SetFloatpv(side, DMU_BOTTOM_COLOR, bottom);

            flags = DMU_GetIntp(side, DMU_FLAGS);
            if(!(dFlags & LDF_NOBLENDTOP))
                flags |= SDF_BLEND_TOPTOMID;
            if(!(dFlags & LDF_NOBLENDBOTTOM))
                flags |= SDF_BLEND_BOTTOMTOMID;

            DMU_SetIntp(side, DMU_FLAGS, flags);
        }
    }

    return 1; // Continue iteration
}
#endif

static boolean checkMapSpotSpawnFlags(const mapspot_t* spot)
{
#if __JHEXEN__
    static unsigned int classFlags[] = {
        MSF_FIGHTER,
        MSF_CLERIC,
        MSF_MAGE
    };
#endif

    int                 spawnMask;

    // Don't spawn things flagged for Multiplayer if we're not in a netgame.
    if(!IS_NETGAME && (spot->flags & MSF_NOTSINGLE))
        return false;

    // Don't spawn things flagged for Not Deathmatch if we're deathmatching.
    if(deathmatch && (spot->flags & MSF_NOTDM))
        return false;

    // Don't spawn things flagged for Not Coop if we're coop'in.
    if(IS_NETGAME && !deathmatch && (spot->flags & MSF_NOTCOOP))
        return false;

    // Check for appropriate skill level.
    if(gameSkill == SM_BABY || gameSkill == SM_EASY)
        spawnMask = MSF_EASY;
    else if(gameSkill == SM_HARD
#if !__JDOOM64__
        || gameSkill == SM_NIGHTMARE
#endif
        )
        spawnMask = MSF_HARD;
    else
        spawnMask = MSF_MEDIUM;

    if(!(spot->flags & spawnMask))
        return false;

#if __JHEXEN__
    // Check current character classes with spawn flags.
    if(IS_NETGAME == false)
    {   // Single player.
        if((spot->flags & classFlags[cfg.playerClass[0]]) == 0)
        {   // Not for current class.
            return false;
        }
    }
    else if(deathmatch == false)
    {   // Cooperative.
        int                 i;

        spawnMask = 0;
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
            {
                spawnMask |= classFlags[cfg.playerClass[i]];
            }
        }

        // No players are in the game when a dedicated server is started.
        // In this case, we'll be generous and spawn stuff for all the
        // classes.
        if(!spawnMask)
        {
            spawnMask |= MSF_FIGHTER | MSF_CLERIC | MSF_MAGE;
        }

        if((spot->flags & spawnMask) == 0)
        {
            return false;
        }
    }
#endif

    return true;
}

static void P_LoadMapObjs(gamemap_t* map)
{
    uint i;

    for(i = 0; i < numlines; ++i)
    {
        xlinedef_t* xl = &map->_xLineDefs[i];

        xl->origID = P_GetObjectRecordInt(MO_XLINEDEF, i, MO_ORIGINALID);
        xl->flags = P_GetObjectRecordShort(MO_XLINEDEF, i, MO_FLAGS);
#if __JHEXEN__
        xl->special = P_GetObjectRecordByte(MO_XLINEDEF, i, MO_TYPE);
        xl->arg1 = P_GetObjectRecordByte(MO_XLINEDEF, i, MO_ARG0);
        xl->arg2 = P_GetObjectRecordByte(MO_XLINEDEF, i, MO_ARG1);
        xl->arg3 = P_GetObjectRecordByte(MO_XLINEDEF, i, MO_ARG2);
        xl->arg4 = P_GetObjectRecordByte(MO_XLINEDEF, i, MO_ARG3);
        xl->arg5 = P_GetObjectRecordByte(MO_XLINEDEF, i, MO_ARG4);
#else
# if __JDOOM64__
        xl->special = P_GetObjectRecordByte(MO_XLINEDEF, i, MO_TYPE);
# else
        xl->special = P_GetObjectRecordShort(MO_XLINEDEF, i, MO_TYPE);
# endif
        xl->tag = P_GetObjectRecordShort(MO_XLINEDEF, i, MO_TAG);
#endif
    }

    for(i = 0; i < numsectors; ++i)
    {
        xsector_t* xsec = &map->_xSectors[i];

        xsec->origID = P_GetObjectRecordInt(MO_XSECTOR, i, MO_ORIGINALID);
        xsec->special = P_GetObjectRecordShort(MO_XSECTOR, i, MO_TYPE);
        xsec->tag = P_GetObjectRecordShort(MO_XSECTOR, i, MO_TAG);

#if __JDOOM64__
        {
        applysurfacecolorparams_t params;
        float rgba[4];
        sector_t* sec = P_ToPtr(DMU_SECTOR, i);

        getSurfaceColor(TOLIGHTIDX(
            P_GetObjectRecordShort(MO_XSECTOR, i, MO_FLOORCOLOR)), rgba);
        DMU_SetFloatpv(sec, DMU_FLOOR_COLOR, rgba);

        getSurfaceColor(TOLIGHTIDX(
            P_GetObjectRecordShort(MO_XSECTOR, i, MO_CEILINGCOLOR)), rgba);
        DMU_SetFloatpv(sec, DMU_CEILING_COLOR, rgba);

        // Now set the side surface colors.
        params.frontSec = sec;
        getSurfaceColor(TOLIGHTIDX(
            P_GetObjectRecordShort(MO_XSECTOR, i, MO_WALLTOPCOLOR)),
                          params.topColor);
        getSurfaceColor(TOLIGHTIDX(
            P_GetObjectRecordShort(MO_XSECTOR, i, MO_WALLBOTTOMCOLOR)),
                          params.bottomColor);

        DMU_Iteratep(sec, DMU_LINEDEF, applySurfaceColor, &params);
        }
#endif
    }

    map->numSpawnSpots = P_NumObjectRecords(MO_THING);

    if(map->numSpawnSpots > 0)
        map->_spawnSpots = Z_Malloc(map->numSpawnSpots * sizeof(mapspot_t), PU_MAP, 0);
    else
        map->_spawnSpots = NULL;

    for(i = 0; i < map->numSpawnSpots; ++i)
    {
        mapspot_t* spot = &map->_spawnSpots[i];

        spot->pos[VX] = P_GetObjectRecordFloat(MO_THING, i, MO_X);
        spot->pos[VY] = P_GetObjectRecordFloat(MO_THING, i, MO_Y);
        spot->pos[VZ] = P_GetObjectRecordFloat(MO_THING, i, MO_Z);

        spot->doomEdNum = P_GetObjectRecordInt(MO_THING, i, MO_DOOMEDNUM);
        spot->flags = P_GetObjectRecordInt(MO_THING, i, MO_FLAGS);
        spot->angle = P_GetObjectRecordAngle(MO_THING, i, MO_ANGLE);

#if __JHEXEN__
        spot->tid = P_GetObjectRecordShort(MO_THING, i, MO_ID);
        spot->special = P_GetObjectRecordByte(MO_THING, i, MO_SPECIAL);
        spot->arg1 = P_GetObjectRecordByte(MO_THING, i, MO_ARG0);
        spot->arg2 = P_GetObjectRecordByte(MO_THING, i, MO_ARG1);
        spot->arg3 = P_GetObjectRecordByte(MO_THING, i, MO_ARG2);
        spot->arg4 = P_GetObjectRecordByte(MO_THING, i, MO_ARG3);
        spot->arg5 = P_GetObjectRecordByte(MO_THING, i, MO_ARG4);
#endif

#if __JHERETIC__
        // Ambient sound origin?
        if(spot->doomEdNum >= 1200 && spot->doomEdNum < 1300)
        {
            GameMap_AddAmbientSfx(map, spot->doomEdNum - 1200);
            continue;
        }
#elif __JHEXEN__
        // Sound sequence origin?
        if(spot->doomEdNum >= 1400 && spot->doomEdNum < 1410)
        {
            subsector_t* ssec = P_PointInSubSector(spot->pos[VX], spot->pos[VY]);
            xsector_t* xsector = P_ToXSector(DMU_GetPtrp(ssec, DMU_SECTOR));

            xsector->seqType = spot->doomEdNum - 1400;
            continue;
        }
        // Polyobject origin?
        if(spot->doomEdNum >= 3000 && spot->doomEdNum < 3003)
            continue; // Not spawned.
#endif

        switch(spot->doomEdNum)
        {
        default: // A spot that should auto-spawn one (or more) mobjs.
            {
            mobjtype_t type;

            if(!checkMapSpotSpawnFlags(spot))
                continue;

            // Find which type to spawn.
            if((type = P_DoomEdNumToMobjType(spot->doomEdNum)) != MT_NONE)
            {   // A known type; spawn it!
                mobj_t* mo;
/*#if _DEBUG
Con_Message("spawning x:[%g, %g, %g] angle:%i ednum:%i flags:%i\n",
            spot->pos[VX], spot->pos[VY], spot->pos[VZ], spot->angle,
            spot->doomedNum, spot->flags);
#endif*/

                if((mo = GameMap_SpawnMobj3fv(map, type, spot->pos, spot->angle, spot->flags)))
                {
                    if(mo->tics > 0)
                        mo->tics = 1 + (P_Random() % mo->tics);

#if __JHEXEN__
                    mo->tid = spot->tid;
                    mo->special = spot->special;
                    mo->args[0] = spot->arg1;
                    mo->args[1] = spot->arg2;
                    mo->args[2] = spot->arg3;
                    mo->args[3] = spot->arg4;
                    mo->args[4] = spot->arg5;
#endif

#if __JHEXEN__
                    if(mo->flags2 & MF2_FLOATBOB)
                        mo->special1 = FLT2FIX(spot->pos[VZ]);
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
                    if(mo->flags & MF_COUNTKILL)
                        map->totalKills++;
                    if(mo->flags & MF_COUNTITEM)
                        map->totalItems++;
#endif
                }
            }
            else
            {
                Con_Message("spawnMapThing: Warning, unknown thing num %i "
                            "at [%g, %g, %g].\n", spot->doomEdNum,
                            spot->pos[VX], spot->pos[VY], spot->pos[VZ]);
            }
            break;
            }
        case 11: // Player start (deathmatch).
            GameMap_AddPlayerStart(map, 0, 0, true, spot->pos[VX], spot->pos[VY],
                                   spot->pos[VZ], spot->angle, spot->flags);
            break;

        case 1: // Player starts 1 through 4.
        case 2:
        case 3:
        case 4:
            {
#if __JHEXEN__
            byte entryPoint = spot->arg1;
#else
            byte entryPoint = 0;
#endif

            GameMap_AddPlayerStart(map, spot->doomEdNum, entryPoint, false,
                                   spot->pos[VX], spot->pos[VY], spot->pos[VZ],
                                   spot->angle, spot->flags);
            break;
            }

#if __JHERETIC__
        case 56: // Boss spot.
            GameMap_AddBossSpot(map, spot->pos[VX], spot->pos[VY], spot->angle);
            break;

        case 2002:
            if(gameMode != shareware)
                GameMap_AddMaceSpot(map, spot->pos[VX], spot->pos[VY], spot->angle);
            break;
#endif

#if __JHEXEN__
        case 9100: // Player starts 5 through 8.
        case 9101:
        case 9102:
        case 9103:
            GameMap_AddPlayerStart(map, 5 + spot->doomEdNum - 9100, spot->arg1,
                                   false, spot->pos[VX], spot->pos[VY],
                                   spot->pos[VZ], spot->angle, spot->flags);
            break;
#endif
        }
    }

    if(deathmatch)
    {
        uint numDMStarts = GameMap_NumPlayerStarts(map, true);
        uint playerCount = 0;
        int i;
        
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
                playerCount++;
        }

        if(numDMStarts < playerCount)
        {
            Con_Error("P_SetupMap: Player count (%d) exceeds deathmatch "
                      "spots (%d).", playerCount, numDMStarts);
        }
    }
}

/**
 * @todo This should be done in the map converter plugin, not here.
 */
static void interpretLinedefFlags(gamemap_t* map)
{
#define ML_BLOCKING             1 // Solid, is an obstacle.
#define ML_TWOSIDED             4 // Backside will not be present at all if not two sided.
#define ML_DONTPEGTOP           8 // Upper texture unpegged.
#define ML_DONTPEGBOTTOM        16 // Lower texture unpegged.
#if __JDOOM64__
#define MLT_MIRRORH             64 // Mirror textures horizontally.
#define MLT_MIRRORV             128 // Mirror textures vertically.
#endif
    uint i;

    // Interpret the archived map linedef flags and update accordingly.
    for(i = 0; i < numlines; ++i)
    {
        xlinedef_t* xline = &map->_xLineDefs[i];
        int flags = 0;

        /**
         * Zero unused flags if ML_INVALID is set.
         *
         * @note "This has been found to be necessary because of errors
         * in Ultimate DOOM's E2M7, where around 1000 linedefs have
         * the value 0xFE00 masked into the flags value.
         * There could potentially be many more maps with this problem,
         * as it is well-known that Hellmaker wads set all bits in
         * mapthings that it does not understand."
         * Thanks to Quasar for the heads up.
         */
#if !__JHEXEN__
        /**
         * @fixme This applies only to DOOM format maps but the game doesn't
         * know what format the map is in (and shouldn't really) but the
         * engine doesn't know if the game wants to do this...
         */
# if !__JDOOM64__
        /**
         * @note DJS - Can't do this with Doom64TC as it has used the
         * ML_INVALID bit for another purpose... doh!
         */

        if(xline->flags & ML_INVALID)
            xline->flags &= VALIDMASK;
# endif
#endif

        if(xline->flags & ML_BLOCKING)
        {
            flags |= DDLF_BLOCKING;
            xline->flags &= ~ML_BLOCKING;
        }

        if(xline->flags & ML_TWOSIDED)
        {
            xline->flags &= ~ML_TWOSIDED;
        }

        if(xline->flags & ML_DONTPEGTOP)
        {
            flags |= DDLF_DONTPEGTOP;
            xline->flags &= ~ML_DONTPEGTOP;
        }

        if(xline->flags & ML_DONTPEGBOTTOM)
        {
            flags |= DDLF_DONTPEGBOTTOM;
            xline->flags &= ~ML_DONTPEGBOTTOM;
        }

        if(xline->flags & ML_MAPPED)
        {
            int p;

            // Update the automap(s) with all immediately visible lines.
            for(p = 0; p < MAXPLAYERS; ++p)
                AM_UpdateLinedef(AM_MapForPlayer(p), i, true);
            xline->flags &= ~ML_MAPPED;
        }

        DMU_SetInt(DMU_LINEDEF, i, DMU_FLAGS, flags);
    }

#undef ML_BLOCKING
#undef ML_TWOSIDED
#undef ML_DONTPEGTOP
#undef ML_DONTPEGBOTTOM
#if __JDOOM64__
#undef MLT_MIRRORH
#undef MLT_MIRRORV
#endif
}

void GameMap_Precache(gamemap_t* map)
{
    assert(map);

    // Preload graphics.
    R_PrecacheMap();
    R_PrecachePSprites();

#if __JDOOM__
    {
    static const mobjtype_precachedata_t types[] = {
        { GM_ANY,   MT_SKULL },
        // Missiles:
        { GM_ANY,   MT_BRUISERSHOT },
        { GM_ANY,   MT_TROOPSHOT },
        { GM_ANY,   MT_HEADSHOT },
        { GM_ANY,   MT_ROCKET },
        { GM_NOTSHAREWARE, MT_PLASMA },
        { GM_NOTSHAREWARE, MT_BFG },
        { GM_DOOM2, MT_ARACHPLAZ },
        { GM_DOOM2, MT_FATSHOT },
        // Potentially dropped weapons:
        { GM_ANY,   MT_CLIP },
        { GM_ANY,   MT_SHOTGUN },
        { GM_ANY,   MT_CHAINGUN },
        // Misc effects:
        { GM_DOOM2, MT_FIRE },
        { GM_ANY,   MT_TRACER },
        { GM_ANY,   MT_SMOKE },
        { GM_DOOM2, MT_FATSHOT },
        { GM_ANY,   MT_BLOOD },
        { GM_ANY,   MT_PUFF },
        { GM_ANY,   MT_TFOG }, // Teleport FX.
        { GM_ANY,   MT_EXTRABFG },
        { GM_ANY,   MT_ROCKETPUFF },
        { 0,        0}
    };
    uint i;

    for(i = 0; types[i].type != 0; ++i)
        if(types[i].gameModeBits & gameModeBits)
            R_PrecacheMobjNum(types[i].type);

    if(IS_NETGAME)
        R_PrecacheMobjNum(MT_IFOG);
    }
#endif
}

static void loadRejectMatrix(gamemap_t* map)
{
    lumpnum_t lumpNum = W_GetNumForName(map->mapID) + 9 /*ML_REJECT*/;
    if(lumpNum != -1)
    {
        map->_rejectMatrix = Z_Malloc(W_LumpLength(lumpNum), PU_STATIC, 0);
        W_ReadLump(lumpNum, map->_rejectMatrix);
    }
}

typedef struct setupmapparams_s {
    gamemap_t*      map;
    skillmode_t     skill;
} setupmapparams_t;

int P_SetupMapWorker(void* ptr)
{
    const setupmapparams_t* p = ptr;
    gamemap_t* map = p->map;

    // It begins...
    map->inSetup = true;

    // Let the engine know that we are about to start setting up a map.
    R_SetupMap(DDSMM_INITIALIZE, 0);

    if(!P_LoadMap(map->mapID))
    {
        return 0; // Failed.
    }

    DD_InitThinkers();
    P_LoadMapObjs(map);

#if __JDOOM__
    if(gameMode == commercial)
        P_SpawnBrainTargets(map);
#endif

#if __JHERETIC__
    if(map->maceSpotCount)
    {
        // Sometimes doesn't show up if not in deathmatch.
        if(!(!deathmatch && P_Random() < 64))
        {
            const mapspot_t* spot = &map->_maceSpots[P_Random() % map->maceSpotCount];

            GameMap_SpawnMobj3f(map, MT_WMACE, spot->pos[VX], spot->pos[VY], 0,
                          spot->angle, MSF_Z_FLOOR);
        }
    }
#endif

#if __JHEXEN__
    P_CreateTIDList(map);
    P_InitCreatureCorpseQueue(map);
    GameMap_InitPolyobjs(map);
#endif

    interpretLinedefFlags(map);

#if __JHERETIC__
    GameMap_InitAmbientSfx(map);
#endif

    // @fixme Should be handled by the map converter.
    loadRejectMatrix(map);

    GameMap_DealPlayerStarts(map, 0);
    GameMap_SpawnPlayers(map);

    // Set up world state.
    GameMap_SpawnSpecials(map);

    if(precache)
        GameMap_Precache(map);

    P_FinalizeMap(map);

    // Someone may want to do something special now that the map has been
    // fully set up.
    R_SetupMap(DDSMM_FINALIZE, 0);

    // It ends.
    map->inSetup = false;

    Con_BusyWorkerEnd();
    return 1; // Success.
}

boolean GameMap_Load(gamemap_t* map, skillmode_t skill)
{
    assert(map);

    // Pre-init.
#if __JHERETIC__ || __JHEXEN__
    GameMap_InitLava(map);
#endif

    // Create the various line lists (spechits, anims, buttons etc).
    map->_spechit = P_CreateIterList();
    map->_linespecials = P_CreateIterList();

#if __JDOOM__ || __JDOOM64__
    // Only used with 666/7 specials
    map->bossKilled = false;
#endif

#if __JDOOM__
    // Brain info
    map->brain.numTargets = 0;
    map->brain.numTargetsAlloc = -1;
    map->brain.targetOn = 0;
    map->brain.easy = 0; // Always init easy to 0.
#endif

#if __JHERETIC__
    map->maceSpotCount = 0;
    map->_maceSpots = NULL;
    map->bossSpotCount = 0;
    map->_bossSpots = NULL;
#endif

    GameMap_PurgeDeferredSpawns(map);

#if !__JHEXEN__
    map->totalKills = map->totalItems = map->totalSecret = 0;
#endif

#if __JDOOM__ || __JDOOM64__
    map->bodyQueueSlot = 0;
#endif

    GameMap_ClearPlayerStarts(map);

    map->time = map->actualTime = 0;

    // Load the map!
    {
    setupmapparams_t p;
    int result;

    p.map = map;
    p.skill = skill;

    // @todo Use progress bar mode and update progress during the setup.
    result = Con_Busy(BUSYF_ACTIVITY | /*BUSYF_PROGRESS_BAR |*/ (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                      "Loading map...", P_SetupMapWorker, &p);

    R_SetupMap(DDSMM_AFTER_BUSY, 0);
    return result;
    }
}

static void loadActionScripts(const char* mapID)
{
#if __JHEXEN__
    /**
     * @todo The map converter should convert/decompile the Hexen ACS bytecode
     * into DE script source file(s) and then loaded here.
     * For now we will be content to interpret the Hexen ACS bytecode.
     */
    lumpnum_t lumpNum = W_GetNumForName(mapID) + 11 /*ML_BEHAVIOR*/;
    if(lumpNum != -1)
    {
        if(!ActionScriptInterpreter)
            P_CreateActionScriptInterpreter();
        ActionScriptInterpreter_Load(ActionScriptInterpreter, 0, lumpNum);
    }
#endif
}

/**
 * Loads map and glnode data for the requested episode and map.
 */
void P_SetupMap(gamemap_t* map, skillmode_t skill)
{
    assert(map);
    {
    int i;

    DD_Executef(true, "texreset raw"); // Delete raw images to save memory.
#if __JHEXEN__
    S_StartMusic("chess", true); // Waiting-for-map-load song.
#endif
#if __JDOOM__ || __JDOOM64__
    wmInfo.maxFrags = 0;
    wmInfo.parTime = -1;
#endif

    if(!GameMap_Load(map, skill))
        Con_Error("P_SetupMap: Failed loading \"%s\".\n", map->mapID);

    loadActionScripts(map->mapID);

    P_PrintMapBanner(map->episodeNum, map->mapNum);

    // @todo startTic should not be set until the first player enters the map.
    map->startTic = (int) GAMETIC;

    timerGame = 0;
    if(deathmatch)
    {
        int parm = ArgCheck("-timer");
        if(parm && parm < Argc() - 1)
            timerGame = atoi(Argv(parm + 1)) * 35 * 60;
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        automapid_t am = AM_MapForPlayer(i);
        player_t* plr = &players[i];

        plr->killCount = plr->secretCount = plr->itemCount = 0;
        // Initial height of PointOfView; will be set by player think.
        plr->viewZ = 1;

        AM_SetCheatLevel(am, 0);
        AM_RevealMap(am, false);
    }

    AM_InitForMap();

#if __JHEXEN__
    {
    int i;
    // Load colormap and set the fullbright flag
    i = P_GetMapFadeTable(gameMap);
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
    }
#endif
    }
}

/**
 * Do any map finalization including any game-specific stuff.
 */
static void P_FinalizeMap(gamemap_t* map)
{
#if __JDOOM__ || __JDOOM64__
    // Adjust slime lower wall textures (a hack!).
    // This will hide the ugly green bright line that would otherwise be
    // visible due to texture repeating and interpolation.
    {
    uint i;
    material_t* mat = P_MaterialForName(MN_TEXTURES, "NUKE24");

    for(i = 0; i < numlines; ++i)
    {
        linedef_t* line = DMU_ToPtr(DMU_LINEDEF, i);
        uint k;

        for(k = 0; k < 2; ++k)
        {
            sidedef_t* sidedef = DMU_GetPtrp(line, k == 0? DMU_SIDEDEF0 : DMU_SIDEDEF1);

            if(sidedef)
            {
                material_t* bottomMat = DMU_GetPtrp(sidedef, DMU_BOTTOM_MATERIAL);

                if(bottomMat == mat &&
                   DMU_GetPtrp(sidedef, DMU_MIDDLE_MATERIAL) == NULL)
                {
                    float yoff = DMU_GetFloatp(sidedef, DMU_BOTTOM_MATERIAL_OFFSET_Y);
                    DMU_SetFloatp(sidedef, DMU_BOTTOM_MATERIAL_OFFSET_Y, yoff + 1.0f);
                }
            }
        }
    }
    }

#elif __JHEXEN__
    // Check if the map should have lightening.
    P_InitLightning();

    SN_StopAllSequences();
#endif

    // Do some fine tuning with mobj placement and orientation.
    P_MoveThingsOutOfWalls(map);
#if __JHERETIC__
    P_TurnGizmosAwayFromDoors(map);
#endif
}

const char* P_GetMapNiceName(void)
{
    const char* lname, *ptr;

    lname = (char*) DD_GetVariable(DD_MAP_NAME);
#if __JHEXEN__
    // In jHexen we can look in the MAPINFO for the map name.
    if(!lname)
        lname = P_GetMapName(gameMap);
#endif

    if(!lname || !lname[0])
        return NULL;

    // Skip the ExMx part.
    ptr = strchr(lname, ':');
    if(ptr)
    {
        lname = ptr + 1;
        while(*lname && isspace(*lname))
            lname++;
    }

    return lname;
}

const char* P_GetMapAuthor(boolean surpressIWADAuthors)
{
    const char* author = (const char*) DD_GetVariable(DD_MAP_AUTHOR);

    if(!author || !author[0])
        return NULL;

    if(surpressIWADAuthors)
    {
        char lumpName[9];

        P_GetMapLumpName(lumpName, gameEpisode, gameMap);
        if(W_IsFromIWAD(W_GetNumForName(lumpName)))
            return NULL;

        // @kludge We need DED Reader 2.0 to handle this the Right Way...
        {
# if __JDOOM__
        static const char* iwadAuthors[] = {
            "id Software",
            "id Software",
            "Team TNT",
            "Dario Casali and Milo Casali"
        };

        if(!stricmp(author, iwadAuthors[gameMission]))
            return NULL;
# elif __JDOOM64__
        if(!stricmp(author, "Midway"))
            return NULL;
# else /* __JHERETIC__ || __JHEXEN__ */
        if(!stricmp(author, "raven software"))
            return NULL;
# endif
        }
    }

    return author;
}

#if !__JHEXEN__
static boolean isIWADMap(int episode, int map)
{
    char lumpName[9];

    P_GetMapLumpName(lumpName, episode, map);
    return W_IsFromIWAD(W_GetNumForName(lumpName));
}
#endif

/**
 * Prints a banner to the console containing information pertinent to the
 * current map (e.g. map name, author...).
 */
static void P_PrintMapBanner(int episode, int map)
{
    const char* lname;

    Con_Printf("\n");
    lname = P_GetMapNiceName();
    if(lname)
    {
        char name[64];

#if __JHEXEN__
        dd_snprintf(name, 64, "Map %d (%d): %s", P_GetMapWarpTrans(map),
                    map, lname);
#else
        dd_snprintf(name, 64, "Map %d: %s", map, lname);
#endif

        Con_FPrintf(CBLF_LIGHT | CBLF_BLUE, "%s\n", name);
    }

#if !__JHEXEN__
    {
    static const char* unknownAuthorStr = "Unknown";
    const char* lauthor;

    lauthor = P_GetMapAuthor(!isIWADMap(episode, map));
    if(!lauthor)
        lauthor = unknownAuthorStr;

    Con_FPrintf(CBLF_LIGHT | CBLF_BLUE, "Author: %s\n", lauthor);
    }
#endif

    Con_Printf("\n");
}
