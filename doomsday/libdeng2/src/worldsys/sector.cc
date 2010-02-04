/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#include "de/Sector"
#include "de/Log"

using namespace de;

namespace de
{
    // @fixme Not thread safe.
    static bool noFit;
}

/**
 * Takes a valid mobj and adjusts the mobj->floorZ, mobj->ceilingZ, and
 * possibly mobj->z. This is called for all nearby mobjs whenever a sector
 * changes height. If the mobj doesn't fit, the z will be set to the lowest
 * value and false will be returned.
 */
static bool heightClip(mobj_t* mo)
{
    bool onfloor;

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
static dint PIT_SectorPlanesChanged(void* obj, void* data)
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
bool Sector::planesChanged()
{
    noFit = false;
    // We'll use validCount to make sure mobjs are only checked once.
    validCount++;
    Sector_IterateMobjsTouching(sector, PIT_SectorPlanesChanged, 0);
    return noFit;
}

dfloat Sector::lightLevel()
{
    if(mapFullBright)
        return 1.0f;
    return _lightLevel;
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
bool Sector::pointInside(dfloat x, dfloat y) const
{
    bool isOdd = false;
    for(duint i = 0; i < sector->lineDefCount; ++i)
    {
        LineDef* line = sector->lineDefs[i];
        Vertex* vtx[2];

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
bool Sector::pointInside2(dfloat x, dfloat y) const
{
    // @todo Subsector should return the map its linked in.
    const Subsector* subsector = Map_PointInSubsector2(P_CurrentMap(), x, y);
    if(subsector->sector != this)
        return false; // Wrong sector.
    return subsector->pointInside(x, y);
}

/**
 * @pre Sector bounds must be setup before this is called!
 */
void Sector::bounds(dfloat* min, dfloat* max) const
{
    if(min)
    {
        min[VX] = bBox[BOXLEFT];
        min[VY] = bBox[BOXBOTTOM];
    }
    if(max)
    {
        max[VX] = bBox[BOXRIGHT];
        max[VY] = bBox[BOXTOP];
    }
}

/**
 * @pre Lines in sector must be setup before this is called!
 */
void Sector::updateBounds()
{
    dfloat* bbox = bBox;

    if(!(sec->lineDefCount > 0))
    {
        memset(bBox, 0, sizeof(bBox));
        return;
    }

    bbox[BOXLEFT]   = DDMAXFLOAT;
    bbox[BOXRIGHT]  = DDMINFLOAT;
    bbox[BOXBOTTOM] = DDMAXFLOAT;
    bbox[BOXTOP]    = DDMINFLOAT;

    for(duint i = 1; i < lineDefCount; ++i)
    {
        LineDef* li = lineDefs[i];
        Vertex* vtx;

        if(li->polyobjOwned)
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
    approxArea = ((bbox[BOXRIGHT] - bbox[BOXLEFT]) / 128) *
        ((bbox[BOXTOP] - bbox[BOXBOTTOM]) / 128);
}

bool Sector::setProperty(const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_COLOR:
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[0], args, 0);
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[1], args, 1);
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[2], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[0], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[1], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_SetValue(DMT_SECTOR_RGB, &rgb[2], args, 0);
        break;
    case DMU_LIGHT_LEVEL:
        DMU_SetValue(DMT_SECTOR_LIGHTLEVEL, &lightLevel, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_SECTOR_VALIDCOUNT, &validCount, args, 0);
        break;
    default:
        LOG_ERROR("Sector::setProperty: Property %s is not writable.")
            << DMU_Str(args->prop);
    }

    return true; // Continue iteration.
}

bool Sector::getProperty(setargs_t* args) const
{
    switch(args->prop)
    {
    case DMU_LIGHT_LEVEL:
        DMU_GetValue(DMT_SECTOR_LIGHTLEVEL, &_lightLevel, args, 0);
        break;
    case DMU_COLOR:
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[0], args, 0);
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[1], args, 1);
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[2], args, 2);
        break;
    case DMU_COLOR_RED:
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[0], args, 0);
        break;
    case DMU_COLOR_GREEN:
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[1], args, 0);
        break;
    case DMU_COLOR_BLUE:
        DMU_GetValue(DMT_SECTOR_RGB, &rgb[2], args, 0);
        break;
    case DMU_SOUND_ORIGIN:
    {
        const ddmobj_base_t* dmo = &soundOrg;
        DMU_GetValue(DMT_SECTOR_SOUNDORG, &dmo, args, 0);
        break;
    }
    case DMU_LINEDEF_COUNT:
    {
        dint val = (dint) lineDefCount;
        DMU_GetValue(DDVT_INT, &val, args, 0);
        break;
    }
    case DMU_MOBJS:
        DMU_GetValue(DMT_SECTOR_MOBJLIST, &mobjList, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_SECTOR_VALIDCOUNT, &validCount, args, 0);
        break;
    default:
        LOG_ERROR("Sector::getProperty: No property %s.")
            << DMU_Str(args->prop);
    }

    return true; // Continue iteration.
}

bool Sector::iterateMobjsTouching(dint (*func) (void*, void*), void* data)
{
    assert(func);

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
    LineDef* li;
    nodeindex_t root, nix;
    Map* map;

    if(!func)
        return true;

    // First process the mobjs that obviously are in the sector.
    for(mo = mobjList; mo; mo = mo->sNext)
    {
        if(mo->validCount == validCount)
            continue;

        mo->validCount = validCount;
        *end++ = mo;
    }

    // Then check the sector's lines.
    map = P_CurrentMap();
    for(i = 0; i < lineDefCount; ++i)
    {
        linknode_t* ln;

        li = lineDefs[i];

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
