/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/App"
#include "de/Vector"
#include "de/Polyobj"
#include "de/LineDef"
#include "de/Map"

using namespace de;

namespace {
struct PIT_CheckThingBlockingParams {
    bool blocked;
    LineDef* lineDef;
    Polyobj* polyobj;
};

bool checkThingBlocking(LineDef* lineDef, Polyobj* po);

void rotatePoint(dangle angle, ddouble* x, ddouble* y, ddouble startSpotX, ddouble startSpotY)
{
    dint an = angle >> ANGLETOFINESHIFT;

    ddouble curX = *x, curY = *y;
    ddouble gxt, gyt;

    gxt = curX * fix2flt(fineCosine[an]);
    gyt = curY * fix2flt(finesine[an]);
    *x = gxt - gyt + startSpotX;

    gxt = curX * fix2flt(finesine[an]);
    gyt = curY * fix2flt(fineCosine[an]);
    *y = gyt + gxt + startSpotY;
}
}

Polyobj::Polyobj(const LineDefs& lineDefs, dint tag, dint sequenceType,
    dfloat anchorX, dfloat anchorY)
 :  idx(0), _lineDefs(lineDefs), tag(tag), seqType(sequenceType),
    origin(Vector2f(anchorX, anchorY))
{}

Polyobj::~Polyobj()
{
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef* lineDef = reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);
        delete lineDef->halfEdges[0]->twin;
        delete lineDef->halfEdges[0];
    }
    _lineDefs.clear();
    _segs.clear();
    _originalPts.clear();
    _prevPts.clear();
}

void Polyobj::changed()
{
    // Shadow bias must be told.
    // @todo Polyobj should return the map its linked in.
    Map& map = App::currentMap();
    FOR_EACH(i, _segs, Segs::iterator)
    {
        Seg& seg = *i;
        if(seg.bsuf[Seg::MIDDLE])
            SB_SurfaceMoved(map, seg.bsuf[Seg::MIDDLE]);
    }
}

void Polyobj::updateAABounds()
{
    bool first = true;
    FOR_EACH(i, _lineDefs, LineDefs::const_iterator)
    {
        const LineDef* lineDef = reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);
        if(first)
        {
            _aaBounds = lineDef->aaBounds();
            first = false;
            continue;
        }
        _aaBounds.include(lineDef->aaBounds());
    }
}

void Polyobj::createSegs()
{
    _segs.reserve(_lineDefs.size());
    FOR_EACH(i, _lineDefs, LineDefs::const_iterator)
    {
        const LineDef* lineDef = reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);
        HalfEdge& halfEdge = *lineDef->halfEdges[0];

        /// @todo Polyobj should return the map its linked in.
        Map& map = App::app().currentMap();
        SideDef* sideDef = map.sideDefs()[lineDef->buildData.sideDefs[LineDef::FRONT]->buildData.index - 1];

        _segs.push_back(Seg(halfEdge, sideDef, false));

        Seg* seg = &_segs[i - _lineDefs.begin()];
        halfEdge.data = reinterpret_cast<void*>(seg);
    }
}

void Polyobj::initalize()
{
    // @fixme Polyobj should return the map its linked in.
    Map& map = App::currentMap();

    FOR_EACH(i, _segs, Segs::iterator)
    {
        Seg& seg = *i;

        biassurface_t* bsuf = SB_CreateSurface(map);
        bsuf->size = 4;
        bsuf->illum = Z_Calloc(sizeof(vertexillum_t) * bsuf->size, PU_STATIC, 0);

        for(duint j = 0; j < bsuf->size; ++j)
            SB_InitVertexIllum(&bsuf->illum[j]);

        seg.bsuf[SEG_MIDDLE] = bsuf;
    }

    Vector2f average;

    {bool first = true;
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        const LineDef& lineDef = *reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);

        if(first)
        {
            average = lineDef.vtx1().pos;
            first = false;
        }
        else
        {
            average += lineDef.vtx1().pos;
        }

        MSurface& surface = lineDef.front().middle();
        surface._inFlags[MSurface::NORADIO] = true;
        surface.normal.x = (lineDef.vtx2().pos.y - lineDef.vtx1().pos.y) / lineDef.length;
        surface.normal.y = (lineDef.vtx1().pos.x - lineDef.vtx2().pos.x) / lineDef.length;
        surface.normal.z = 0;
    }}

    average.x /= _lineDefs.size();
    average.y /= _lineDefs.size();

    subsector = &map.pointInSubsector(average);
    if(subsector->polyobj)
    {
        LOG_MESSAGE("Warning: Polyobj::initialize: Multiple Polyobjs in a single Subsector"
                    " (face %i, sector %i). Previous Polyobj overridden.")
            << P_ObjectRecord(DMU_SUBSECTOR, subsector)->id
            << P_ObjectRecord(DMU_SECTOR, subsector->sector())->id;
    }
    subsector->polyobj = this;

    // Until now the LineDefs for this Polyobj have been excluded from the
    // LineDefBlockmap. From this point forward it can collide with Things.
    linkInLineDefBlockmap();
    changed();
}

bool Polyobj::translate(const Vector2f& delta)
{
    /// Move to the new location.
    unlinkInLineDefBlockmap();
    {EdgePoints::iterator pointItr = _prevPts.begin();
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef* lineDef = reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);

        lineDef->_aaBounds.move(delta);
        lineDef->vtx1().pos += delta;

        (*pointItr) += delta; // Previous points are unique for each linedef.
        pointItr++;
    }}
    origin += delta;
    linkInLineDefBlockmap();

    /// Check if we now in collision with any Thing we shouldn't be.
    bool blocked = false;
    validCount++;
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef* lineDef = reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);
        if(checkThingBlocking(lineDef, this))
            blocked = true;
    }

    if(blocked)
    {
        /// Undo the move.
        Vector2d inverseDelta = -delta;

        unlinkInLineDefBlockmap();
        {EdgePoints::iterator pointItr = _prevPts.begin();
        FOR_EACH(i, _lineDefs, LineDefs::iterator)
        {
            LineDef* lineDef = reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);

            lineDef->_aaBounds.move(inverseDelta);
            lineDef->vtx1().pos += inverseDelta;

            (*pointItr) += inverseDelta;
            pointItr++;
        }
        }
        origin += inverseDelta;
        linkInLineDefBlockmap();
        return false;
    }

    changed();
    return true;
}

bool Polyobj::rotate(dangle delta)
{
    /// Move to the new location.
    unlinkInLineDefBlockmap();
    {EdgePoints::const_iterator origPointItr = _originalPts.begin();
    EdgePoints::iterator pointItr = _prevPts.begin();
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef& lineDef = *reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);
        SideDef& sideDef = lineDef.front();
        MSurface& surface = sideDef.middle();

        (*pointItr) = lineDef.vtx1().pos;
        lineDef.vtx1().pos = *origPointItr;

        Vector2d newPos = lineDef.vtx1().pos;
        rotatePoint(angle + delta, &newPos.x, &newPos.y, origin.x, origin.y);
        lineDef.vtx1().pos = newPos;
        lineDef.angle += delta;

        surface.normal.x = (lineDef.vtx2().pos.y - lineDef.vtx1().pos.y) / lineDef.length;
        surface.normal.y = (lineDef.vtx1().pos.x - lineDef.vtx2().pos.x) / lineDef.length;

        origPointItr++;
        pointItr++;
    }}
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef* lineDef = reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);
        lineDef->updateAABounds();
    }
    angle += delta;
    linkInLineDefBlockmap();

    /// Check if we are now in collision with any Thing we shouldn't be.
    bool blocked = false;
    validCount++;
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef* lineDef = reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);
        if(checkThingBlocking(lineDef, this))
            blocked = true;
    }

    if(blocked)
    {
        /// Undo the move.
        unlinkInLineDefBlockmap();
        {EdgePoints::iterator pointItr = _prevPts.begin();
        FOR_EACH(i, _lineDefs, LineDefs::iterator)
        {
            LineDef& lineDef = *reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);
            SideDef& sideDef = lineDef.front();
            MSurface& surface = sideDef.middle();

            lineDef.vtx1().pos = *pointItr;
            lineDef.angle -= delta;

            surface.normal.y = (lineDef.vtx1().pos.x - lineDef.vtx2().pos.x) / lineDef.length;
            surface.normal.x = (lineDef.vtx2().pos.y - lineDef.vtx1().pos.y) / lineDef.length;

            pointItr++;
        }}
        FOR_EACH(i, _lineDefs, LineDefs::iterator)
        {
            LineDef& lineDef = *reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);
            lineDef.updateAABounds();
        }
        angle -= delta;
        linkInLineDefBlockmap();
        return false;
    }

    changed();
    return true;
}

void Polyobj::unlinkInLineDefBlockmap()
{
    // @todo Polyobj should return the map its linked in.
    LineDefBlockmap& lineDefBlockmap = App::currentMap().lineDefBlockmap();
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
        lineDefBlockmap.unlink(reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);
}

void Polyobj::linkInLineDefBlockmap()
{
    // @todo Polyobj should return the map its linked in.
    LineDefBlockmap& lineDefBlockmap = App::currentMap().lineDefBlockmap();
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
        lineDefBlockmap.link(reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);
}

bool PTR_CheckThingBlocking(Thing* thing, void* paramaters)
{
    PIT_CheckThingBlockingParams* params = reinterpret_cast<PIT_CheckThingBlockingParams*>(paramaters);

    if((thing->ddFlags & DDMF_SOLID) ||
       (thing->object().user() && !(thing->object().user()->flags & DDPF_CAMERA)))
    {
        const MapRectanglef thingBounds = thing->aaBounds();

        if(thingBounds.intersects(params->lineDef->aaBounds()))
        {
            if(params->lineDef->boxOnSide(thingBounds) == -1)
            {
#pragma message("Warning: Polyobj::rotate - Callback not implemented yet.")
                //if(po_callback)
                //    po_callback(thing, P_ObjectRecord(DMU_LINEDEF, params->lineDef), params->polyobj);

                params->blocked = true;
            }
        }
    }

    return true; // Continue iteration.
}

static bool checkThingBlocking(LineDef* lineDef, Polyobj* polyobj)
{
    MapRectanglef aaBounds = lineDef->aaBounds();
    Vector2f border = Vector2f(dfloat(Thing::MAXRADIUS), dfloat(Thing::MAXRADIUS));
    aaBounds.move(-border);
    aaBounds.setSize(aaBounds.size() + border * 2);

    // @fixme Polyobj should return the map its linked in.
    ThingBlockmap& thingBlockmap = App::currentMap().thingBlockmap();
    duint blockBox[4];
    thingBlockmap.boxToBlocks(blockBox, aaBounds);

    PIT_CheckThingBlockingParams params;
    params.blocked = false;
    params.lineDef = lineDef;
    params.polyobj = polyobj;
    thingBlockmap.iterate(blockBox, PTR_CheckThingBlocking, &params);

    return params.blocked;
}

bool Polyobj::iterateLineDefs(bool (*callback) (LineDef*, void*), bool retObjRecord, void* paramaters)
{
    FOR_EACH(i, _lineDefs, LineDefs::iterator)
    {
        LineDef* lineDef = reinterpret_cast<LineDef*>(reinterpret_cast<objectrecord_t*>(*i)->obj);

        if(lineDef->validCount == validCount)
            continue;

        lineDef->validCount = validCount;

        void* ptr;
        if(retObjRecord)
            ptr = *i;
        else
            ptr = lineDef;

        if(!callback(reinterpret_cast<LineDef*>(ptr), paramaters))
            return false;
    }
    return true;
}
