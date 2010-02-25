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

#include "de/App"
#include "de/Log"
#include "de/Map"
#include "de/Sector"

#include <sstream>

using namespace de;

namespace
{
// @fixme Not thread safe.
bool noFit;

/**
 * Adjusts Thing::floorZ, Thing::ceilingZ, and possibly Thing::origin.z.
 * This is called for all nearby Things whenever a Sector plane changes height.
 * If the Thing doesn't fit, it's z origin will be set to the lowest value and false
 * will be returned.
 */
bool heightClip(Thing* thing)
{
    bool onfloor;

    // During demo playback the player gets preferential treatment.
    if(thing->hasObject() && &thing->object().user() == &ddPlayers[consolePlayer].shared && playback)
        return true;

    onfloor = (thing->origin.z <= thing->floorZ);

    P_CheckPosXYZ(thing, thing->origin.x, thing->origin.y, thing->origin.z);
    thing->floorZ = tmpFloorZ;
    thing->ceilingZ = tmpCeilingZ;

    if(onfloor)
    {
        thing->origin.z = thing->floorZ;
    }
    else
    {
        // Don't adjust a floating mobj unless forced to.
        if(thing->origin.z + thing->height > thing->ceilingZ)
            thing->origin.z = thing->ceilingZ - thing->height;
    }

    // On clientside, players are represented by two mobjs: the real mobj,
    // created by the Game, is the one that is visible and modified in this
    // function. We'll need to sync the hidden client mobj (that receives
    // all the changes from the server) to match the changes.
    if(isClient && thing->hasObject())
    {
        Cl_UpdatePlayerPos(P_GetDDPlayerIdx(thing->object().user()));
    }

    if(thing->ceilingZ - thing->floorZ < thing->height)
        return false;
    return true;
}

bool PIT_SectorPlanesChanged(Thing* thing, void* data)
{
     // Always keep checking.
    if(heightClip(thing))
        return true;
    noFit = true;
    return true;
}
}

Sector::Sector(dfloat lightIntensity, const Vector3f& lightColor)
 : _floorPlane(0), _ceilingPlane(0), lightSource(0), thingList(0)
   blockCount(0), changedBlockCount(0), blocks(0),
   _lightIntensity(clamp(dfloat(0), lightIntensity, dfloat(1))),
   _lightColor(Vector3f(clamp(dfloat(0), lightColor.x, dfloat(1)),
                        clamp(dfloat(0), lightColor.y, dfloat(1)),
                        clamp(dfloat(0), lightColor.z, dfloat(1)))))
{}

Sector::~Sector()
{
    subsectors.clear();
    reverbSubsectors.clear();
    lineDefs.clear();

    if(blocks) Z_Free(blocks);
}

void Sector::clearFrameFlags()
{
    // Clear all flags that can be cleared before each frame.
    frameFlags &= ~FRAME_CLEARMASK;
}

void Sector::markDependantSurfacesForDecorationUpdate()
{
    FOR_EACH(i, lineDefs, LineDefSet::iterator)
    {
        LineDef* lineDef = *i;
        if(!lineDef->hasBack())
        {
            lineDef->front().middle().update();
            continue;
        }
        if(&lineDef->backSector() != &lineDef->frontSector())
        {
            lineDef->front().bottom().update();
            lineDef->front().top().update();
            lineDef->back().bottom().update();
            lineDef->back().top().update();
        }
    }
}

bool Sector::planesChanged()
{
    noFit = false;
    // We'll use validCount to make sure mobjs are only checked once.
    validCount++;
    iterateThingsTouching(PIT_SectorPlanesChanged);
    return noFit;
}

dfloat Sector::lightIntensity() const
{
    if(mapFullBright)
        return 1.0f;
    return _lightIntensity;
}

bool Sector::pointInside(dfloat x, dfloat y) const
{
    bool isOdd = false;
    FOR_EACH(i, lineDefs, LineDefSet::const_iterator)
    {
        const LineDef& lineDef = *(*i);

        // Skip lines that aren't sector boundaries.
        if(lineDef.isSelfreferencing())
            continue;

        const Vertex& vtx1 = lineDef.vtx1();
        const Vertex& vtx2 = lineDef.vtx2();
        // It shouldn't matter whether the line faces inward or outward.
        if((vtx1.pos.y < y && vtx2.pos.y >= y) ||
           (vtx2.pos.y < y && vtx1.pos.y >= y))
        {
            if(vtx1.pos.x +
               (((y - vtx1.pos.y) / (vtx2.pos.y - vtx1.pos.y)) *
                (vtx2.pos.x - vtx1.pos.x)) < x)
            {
                // Toggle oddness.
                isOdd = !isOdd;
            }
        }
    }
    // The point is inside if the number of crossed nodes is odd.
    return isOdd;
}

bool Sector::pointInside2(dfloat x, dfloat y) const
{
    // @todo Subsector should return the map its linked in.
    const Subsector& subsector = App::currentMap().pointInSubsector(x, y);
    if(&subsector.sector() != this)
        return false; // Wrong sector.
    return subsector.pointInside(x, y);
}

void Sector::updateAABounds()
{
    if(!(lineDefs.size() > 0))
    {
        _aaBounds = MapRectangled(Vector2d(0, 0), Vector2d(0, 0));
        return;
    }

    bool first = true;
    FOR_EACH(i, lineDefs, LineDefSet::const_iterator)
    {
        const LineDef* lineDef = (*i);
        if(lineDef->polyobjOwned)
            continue;

        if(first)
        {
            _aaBounds = MapRectangled(lineDef->vtx1().pos, lineDef->vtx1().pos);
            first = false;
            continue;
        }

        _aaBounds.include(lineDef->vtx1().pos);
    }

    // This is very rough estimate of sector area.
    approxArea = (_aaBounds.width() / 128) * (_aaBounds.height() / 128);
}

#if 0
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
        DMU_SetValue(DMT_SECTOR_LIGHTLEVEL, &_lightLevel, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_SECTOR_VALIDCOUNT, &validCount, args, 0);
        break;
    default:
        throw UnknownPropertyError("Sector::setProperty", "Property " + DMU_Str(args->prop) + " not found");
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
        throw UnknownPropertyError("Sector::getProperty", "Property " + DMU_Str(args->prop) + " not found");
    }

    return true; // Continue iteration.
}
#endif

bool Sector::iterateThingsTouching(bool (*callback) (Thing*, void*), void* paramaters)
{
    assert(callback);

/**
 * Linkstore is list of pointers gathered when iterating stuff.
 * This is pretty much the only way to avoid *all* potential problems
 * caused by callback routines behaving badly (moving or destroying
 * Things). The idea is to get a snapshot of all the objects being
 * iterated before any callbacks are called. The hardcoded limit is
 * a drag, but I'd like to see you iterating 2048 Things in one block.
 */
#define MAXLINKED           2048

    Thing* linkstore[MAXLINKED];
    Thing** end = linkstore;   

    // First process the Things that obviously are in this Sector.
    for(Thing* thing = thingList; thing; thing = thing->sNext)
    {
        if(thing->validCount == validCount)
            continue;
        thing->validCount = validCount;
        *end++ = thing;
    }

    /// @fixme Sector should tell us which map it belongs to.
    Map& map = App::currentMap();

    // Then check the sector's lines.
    FOR_EACH(i, lineDefs, LineDefSet::iterator)
    {
        LineDef* li = *i;
        NodePile::LinkNode* ln = map.lineDefNodes->nodes;

        // Iterate all Things linked to the LineDef.
        NodePile::Index root = map.lineDefLinks[P_ObjectRecord(DMU_LINEDEF, li)->id - 1];
        for(NodePile::Index nix = ln[root].next; nix != root; nix = ln[nix].next)
        {
            Thing* thing = reinterpret_cast<Thing*>(ln[nix].ptr);
            if(thing->validCount == validCount)
                continue;

            thing->validCount = validCount;
            *end++ = thing;
        }
    }

    for(Thing** it = linkstore; it < end; it++)
        if(!callback(*it, paramaters))
            return false;
    return true;

#undef MAXLINKED
}
