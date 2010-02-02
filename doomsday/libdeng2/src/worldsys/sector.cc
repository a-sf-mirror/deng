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
 * p_sector.c: World sectors.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// @fixme Not thread safe.
static boolean noFit;

// CODE --------------------------------------------------------------------

/**
 * Takes a valid mobj and adjusts the mobj->floorZ, mobj->ceilingZ, and
 * possibly mobj->z. This is called for all nearby mobjs whenever a sector
 * changes height. If the mobj doesn't fit, the z will be set to the lowest
 * value and false will be returned.
 */
static boolean heightClip(mobj_t* mo)
{
    boolean onfloor;

    // During demo playback the player gets preferential treatment.
    if(mo->dPlayer == &ddPlayers[consolePlayer].shared && playback)
        return true;

    onfloor = (mo->pos[VZ] <= mo->floorZ);

    P_CheckPosXYZ(mo, mo->pos[VX], mo->pos[VY], mo->pos[VZ]);
    mo->floorZ = tmpFloorZ;
    mo->ceilingZ = tmpCeilingZ;

    if(onfloor)
    {
        mo->pos[VZ] = mo->floorZ;
    }
    else
    {
        // Don't adjust a floating mobj unless forced to.
        if(mo->pos[VZ] + mo->height > mo->ceilingZ)
            mo->pos[VZ] = mo->ceilingZ - mo->height;
    }

    // On clientside, players are represented by two mobjs: the real mobj,
    // created by the Game, is the one that is visible and modified in this
    // function. We'll need to sync the hidden client mobj (that receives
    // all the changes from the server) to match the changes.
    if(isClient && mo->dPlayer)
    {
        Cl_UpdatePlayerPos(P_GetDDPlayerIdx(mo->dPlayer));
    }

    if(mo->ceilingZ - mo->floorZ < mo->height)
        return false;
    return true;
}

/**
 * After modifying a sectors floor or ceiling height, call this routine
 * to adjust the positions of all mobjs that touch the sector.
 *
 * If anything doesn't fit anymore, true will be returned.
 */
static int PIT_SectorPlanesChanged(void* obj, void* data)
{
    mobj_t* mo = (mobj_t*) obj;

    // Always keep checking.
    if(heightClip(mo))
        return true;

    noFit = true;
    return true;
}

/**
 * Called whenever a sector's planes are moved. This will update the mobjs
 * inside the sector and do crushing.
 */
boolean Sector_PlanesChanged(sector_t* sector)
{
    noFit = false;

    // We'll use validCount to make sure mobjs are only checked once.
    validCount++;
    Sector_IterateMobjsTouching(sector, PIT_SectorPlanesChanged, 0);

    return noFit;
}

float Sector_LightLevel(sector_t* sec)
{
    assert(sec);

    if(mapFullBright)
        return 1.0f;

    return sec->lightLevel;
}

/**
 * Is the point inside the sector, according to the edge lines of the
 * subsector. Uses the well-known algorithm described here:
 * http://www.alienryderflex.com/polygon/
 *
 * @param               X coordinate to test.
 * @param               Y coordinate to test.
 * @param               Sector to test.
 *
 * @return              @c true, if the point is inside the sector.
 */
boolean Sector_PointInside(const sector_t* sector, float x, float y)
{
    assert(sector);
    {
    boolean isOdd = false;
    uint i;

    for(i = 0; i < sector->lineDefCount; ++i)
    {
        linedef_t* line = sector->lineDefs[i];
        vertex_t* vtx[2];

        // Skip lines that aren't sector boundaries.
        if(LINE_SELFREF(line))
            continue;

        vtx[0] = line->L_v1;
        vtx[1] = line->L_v2;
        // It shouldn't matter whether the line faces inward or outward.
        if((vtx[0]->pos[VY] < y && vtx[1]->pos[VY] >= y) ||
           (vtx[1]->pos[VY] < y && vtx[0]->pos[VY] >= y))
        {
            if(vtx[0]->pos[VX] +
               (((y - vtx[0]->pos[VY]) / (vtx[1]->pos[VY] - vtx[0]->pos[VY])) *
                (vtx[1]->pos[VX] - vtx[0]->pos[VX])) < x)
            {
                // Toggle oddness.
                isOdd = !isOdd;
            }
        }
    }

    // The point is inside if the number of crossed nodes is odd.
    return isOdd;
    }
}

/**
 * Is the point inside the sector, according to the edge lines of the
 * subsector. Uses the well-known algorithm described here:
 * http://www.alienryderflex.com/polygon/
 *
 * More accurate than Sector_PointInside.
 *
 * @param               X coordinate to test.
 * @param               Y coordinate to test.
 * @param               Sector to test.
 *
 * @return              @c true, if the point is inside the sector.
 */
boolean Sector_PointInside2(const sector_t* sector, float x, float y)
{
    assert(sector);
    {
    // @todo Subsector should return the map its linked in.
    const subsector_t* subsector = Map_PointInSubsector2(P_CurrentMap(), x, y);

    if(subsector->sector != sector)
        return false; // Wrong sector.

    return Subsector_PointInside(subsector, x, y);
    }
}

/**
 * @pre Sector bounds must be setup before this is called!
 */
void Sector_Bounds(sector_t* sec, float* min, float* max)
{
    assert(sec);

    if(min)
    {
        min[VX] = sec->bBox[BOXLEFT];
        min[VY] = sec->bBox[BOXBOTTOM];
    }
    if(max)
    {
        max[VX] = sec->bBox[BOXRIGHT];
        max[VY] = sec->bBox[BOXTOP];
    }
}

/**
 * @pre Lines in sector must be setup before this is called!
 */
void Sector_UpdateBounds(sector_t* sec)
{
    uint i;
    float* bbox;
    vertex_t* vtx;

    assert(sec);

    bbox = sec->bBox;

    if(!(sec->lineDefCount > 0))
    {
        memset(sec->bBox, 0, sizeof(sec->bBox));
        return;
    }

    bbox[BOXLEFT] = DDMAXFLOAT;
    bbox[BOXRIGHT] = DDMINFLOAT;
    bbox[BOXBOTTOM] = DDMAXFLOAT;
    bbox[BOXTOP] = DDMINFLOAT;

    for(i = 1; i < sec->lineDefCount; ++i)
    {
        linedef_t* li = sec->lineDefs[i];

        if(li->inFlags & LF_POLYOBJ)
            continue;

        vtx = li->L_v1;

        if(vtx->pos[VX] < bbox[BOXLEFT])
            bbox[BOXLEFT]   = vtx->pos[VX];
        if(vtx->pos[VX] > bbox[BOXRIGHT])
            bbox[BOXRIGHT]  = vtx->pos[VX];
        if(vtx->pos[VY] < bbox[BOXBOTTOM])
            bbox[BOXBOTTOM] = vtx->pos[VY];
        if(vtx->pos[VY] > bbox[BOXTOP])
            bbox[BOXTOP]    = vtx->pos[VY];
    }

    // This is very rough estimate of sector area.
    sec->approxArea = ((bbox[BOXRIGHT] - bbox[BOXLEFT]) / 128) *
        ((bbox[BOXTOP] - bbox[BOXBOTTOM]) / 128);
}

/**
 * Update the sector, property is selected by DMU_* name.
 */
boolean Sector_SetProperty(sector_t *sec, const setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_COLOR:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 1);
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_SetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 0);
        break;
    case DMU_LIGHT_LEVEL:
        DMU_SetValue(DMT_SECTOR_LIGHTLEVEL, &sec->lightLevel, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_SECTOR_VALIDCOUNT, &sec->validCount, args, 0);
        break;
    default:
        Con_Error("Sector_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a sector property, selected by DMU_* name.
 */
boolean Sector_GetProperty(const sector_t *sec, setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_LIGHT_LEVEL:
        DMU_GetValue(DMT_SECTOR_LIGHTLEVEL, &sec->lightLevel, args, 0);
        break;
    case DMU_COLOR:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 1);
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[0], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[1], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SECTOR_RGB, &sec->rgb[2], args, 0);
        break;
    case DMU_SOUND_ORIGIN:
    {
        const ddmobj_base_t* dmo = &sec->soundOrg;
        DMU_GetValue(DMT_SECTOR_SOUNDORG, &dmo, args, 0);
        break;
    }
    case DMU_LINEDEF_COUNT:
    {
        int val = (int) sec->lineDefCount;
        DMU_GetValue(DDVT_INT, &val, args, 0);
        break;
    }
    case DMU_MOBJS:
        DMU_GetValue(DMT_SECTOR_MOBJLIST, &sec->mobjList, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_SECTOR_VALIDCOUNT, &sec->validCount, args, 0);
        break;
    default:
        Con_Error("Sector_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Increment validCount before using this. 'func' is called for each mobj
 * that is (even partly) inside the sector. This is not a 3D test, the
 * mobjs may actually be above or under the sector.
 *
 * (Lovely name; actually this is a combination of SectorMobjs and
 * a bunch of LineMobjs iterations.)
 */
boolean Sector_IterateMobjsTouching(sector_t* sector, int (*func) (void*, void*), void* data)
{
    assert(sector);
    assert(func);
    {
/**
 * Linkstore is list of pointers gathered when iterating stuff.
 * This is pretty much the only way to avoid *all* potential problems
 * caused by callback routines behaving badly (moving or destroying
 * mobjs). The idea is to get a snapshot of all the objects being
 * iterated before any callbacks are called. The hardcoded limit is
 * a drag, but I'd like to see you iterating 2048 mobjs/lines in one block.
 */
#define MAXLINKED           2048
#define DO_LINKS(it, end)   for(it = linkstore; it < end; it++) \
                                if(!func(*it, data)) return false;
    uint i;
    void* linkstore[MAXLINKED];
    void** end = linkstore, **it;
    mobj_t* mo;
    linedef_t* li;
    nodeindex_t root, nix;
    map_t* map;

    if(!sector)
        return true;
    if(!func)
        return true;

    // First process the mobjs that obviously are in the sector.
    for(mo = sector->mobjList; mo; mo = mo->sNext)
    {
        if(mo->validCount == validCount)
            continue;

        mo->validCount = validCount;
        *end++ = mo;
    }

    // Then check the sector's lines.
    map = P_CurrentMap();
    for(i = 0; i < sector->lineDefCount; ++i)
    {
        linknode_t* ln;

        li = sector->lineDefs[i];

        // @fixme LineDef should tell us which map it belongs to.
        ln = map->lineNodes->nodes;

        // Iterate all mobjs on the line.
        root = map->lineLinks[P_ObjectRecord(DMU_LINEDEF, li)->id - 1];
        for(nix = ln[root].next; nix != root; nix = ln[nix].next)
        {
            mo = (mobj_t *) ln[nix].ptr;
            if(mo->validCount == validCount)
                continue;

            mo->validCount = validCount;
            *end++ = mo;
        }
    }

    DO_LINKS(it, end);
    return true;

#undef MAXLINKED
#undef DO_LINKS
    }
}
