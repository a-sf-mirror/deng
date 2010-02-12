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

    dfloat curX = *x, curY = *y;
    dfloat gxt, gyt;

    gxt = curX * FIX2FLT(fineCosine[an]);
    gyt = curY * FIX2FLT(finesine[an]);
    *x = gxt - gyt + startSpotX;

    gxt = curX * FIX2FLT(finesine[an]);
    gyt = curY * FIX2FLT(fineCosine[an]);
    *y = gyt + gxt + startSpotY;
}
}

Polyobj::~Polyobj()
{
    for(duint i = 0; i < _numLineDefs; ++i)
    {
        LineDef* lineDef = reinterpret_cast<LineDef*>(((objectrecord_t*) lineDefs[i])->obj);
        delete lineDef->halfEdges[0]->twin;
        delete lineDef->halfEdges[0];
    }
    Z_Free(lineDefs);
    Z_Free(segs);
    _originalPts.clear();
    _prevPts.clear();
}

void Polyobj::changed()
{
    // @todo Polyobj should return the map its linked in.
    Map& map = App::currentMap();

    // Shadow bias must be told.
    for(duint i = 0; i < _numSegs; ++i)
    {
        Seg* seg = &segs[i];
        if(seg->bsuf[SEG_MIDDLE])
            SB_SurfaceMoved(map, seg->bsuf[SEG_MIDDLE]);
    }
}

void Polyobj::updateAABounds()
{
    const LineDef& lineDef = *reinterpret_cast<LineDef*>(((objectrecord_t*) lineDefs[0])->obj);

    V2_InitBox(box, lineDef.vtx1().pos);
    for(duint i = 0; i < _numLineDefs; ++i)
    {
        lineDef = *reinterpret_cast<LineDef*>(((objectrecord_t*) po->lineDefs[i])->obj);
        V2_AddToBox(box, lineDef.vtx1().pos);
    }
}

void Polyobj::initalize()
{
    // @fixme Polyobj should return the map its linked in.
    Map& map = App::currentMap();

    for(duint i = 0; i < _numSegs; ++i)
    {
        biassurface_t* bsuf = SB_CreateSurface(map);
        bsuf->size = 4;
        bsuf->illum = Z_Calloc(sizeof(vertexillum_t) * bsuf->size, PU_STATIC, 0);

        for(duint j = 0; j < bsuf->size; ++j)
            SB_InitVertexIllum(&bsuf->illum[j]);

        Seg* seg = &segs[i];
        seg->bsuf[SEG_MIDDLE] = bsuf;
    }

    const LineDef& lineDef = *reinterpret_cast<LineDef*>(((objectrecord_t*) lineDefs[0])->obj);
    Vector2d avg = lineDef.vtx1().pos;

    for(duint i = 1; i < _numLineDefs; ++i)
    {
        const LineDef& lineDef = *reinterpret_cast<LineDef*>(((objectrecord_t*) lineDefs[i])->obj);
        MSurface& surface = lineDef.front().middle();

        avg += lineDef.vtx1().pos;

        surface.inFlags |= SUIF_NO_RADIO; 
        surface.normal.y = (lineDef.vtx1().pos.x - lineDef.vtx2().pos.x) / lineDef.length;
        surface.normal.x = (lineDef.vtx2().pos.y - lineDef.vtx1().pos.y) / lineDef.length;
        surface.normal.z = 0;
    }

    avg.x /= _numLineDefs;
    avg.y /= _numLineDefs;

    subsector = &map.pointInSubsector(avg);
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

bool Polyobj::translate(const Vector2f delta)
{
    /// Move to the new location.
    unlinkInLineDefBlockmap();
    {EdgePoints::iterator pointItr = _prevPts.begin();
    for(duint i = 0; i < _numLineDefs; ++i, pointItr++)
    {
        LineDef& lineDef = *reinterpret_cast<LineDef*>(((objectrecord_t*) lineDefs[i])->obj);

        lineDef.bBox[BOXTOP]    += delta.y;
        lineDef.bBox[BOXBOTTOM] += delta.y;
        lineDef.bBox[BOXLEFT]   += delta.x;
        lineDef.bBox[BOXRIGHT]  += delta.x;

        lineDef.vtx1().pos += delta;

        (*pointItr) += delta; // Previous points are unique for each linedef.
    }}
    origin += delta;
    linkInLineDefBlockmap();

    /// Check if we now in collision with any Thing we shouldn't be.
    bool blocked = false;
    validCount++;
    for(duint i = 0; i < _numLineDefs; ++i)
    {
        LineDef* lineDef = reinterpret_cast<LineDef*>(((objectrecord_t*) lineDefs[i])->obj);
        if(checkThingBlocking(lineDef, this))
            blocked = true;
    }

    if(blocked)
    {
        /// Undo the move.
        unlinkInLineDefBlockmap();
        {EdgePoints::iterator pointItr = _prevPts.begin();
        for(duint i = 0; i < _numLineDefs; ++i, pointItr++)
        {
            LineDef& lineDef = *reinterpret_cast<LineDef*>(((objectrecord_t*) lineDefs[i])->obj);

            lineDef.bBox[BOXTOP]    -= delta.y;
            lineDef.bBox[BOXBOTTOM] -= delta.y;
            lineDef.bBox[BOXLEFT]   -= delta.x;
            lineDef.bBox[BOXRIGHT]  -= delta.x;

            lineDef.vtx1().pos -= delta;

            (*pointItr).pos -= delta.x;
        }
        }
        origin -= delta;
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
    for(duint i = 0; i < _numLineDefs; ++i, origPointItr++, prevPointItr++)
    {
        LineDef& lineDef = *reinterpret_cast<LineDef*>(((objectrecord_t*) lineDefs[i])->obj);
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
    }}
    for(duint i = 0; i < _numLineDefs; ++i)
    {
        LineDef& lineDef = *reinterpret_cast<LineDef*>(((objectrecord_t*) lineDefs[i])->obj);
        lineDef.updateAABounds();
    }
    angle += delta;
    linkInLineDefBlockmap();

    /// Check if we are now in collision with any Thing we shouldn't be.
    bool blocked = false;
    validCount++;
    for(duint i = 0; i < _numLineDefs; ++i)
    {
        LineDef& lineDef = *reinterpret_cast<LineDef*>(((objectrecord_t*) lineDefs[i])->obj);
        if(checkThingBlocking(&lineDef, this))
            blocked = true;
    }

    if(blocked)
    {
        /// Undo the move.
        unlinkInLineDefBlockmap();
        {EdgePoints::iterator pointItr = _prevPts.begin();
        for(duint i = 0; i < _numLineDefs; ++i, pointItr++)
        {
            LineDef& lineDef = *reinterpret_cast<LineDef*>(((objectrecord_t*) lineDefs[i])->obj);
            SideDef& sideDef = lineDef.front();
            MSurface& surface = sideDef.middle();

            lineDef.vtx1().pos = *pointItr;
            lineDef.angle -= delta;

            surface.normal.y = (lineDef.vtx1().pos.x - lineDef.vtx2().pos.x) / lineDef.length;
            surface.normal.x = (lineDef.vtx2().pos.y - lineDef.vtx1().pos.y) / lineDef.length;
        }}
        for(duint i = 0; i < _numLineDefs; ++i)
        {
            LineDef& lineDef = *reinterpret_cast<LineDef*>(((objectrecord_t*) po->lineDefs[i])->obj);
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
    for(duint i = 0; i < _numLineDefs; ++i)
        lineDefBlockmap.unlink(reinterpret_cast<LineDef*>((objectrecord_t*) lineDefs[i])->obj);
}

void Polyobj::linkInLineDefBlockmap()
{
    // @todo Polyobj should return the map its linked in.
    LineDefBlockmap& lineDefBlockmap = App::currentMap().lineDefBlockmap();
    for(duint i = 0; i < _numLineDefs; ++i)
        lineDefBlockmap.link(reinterpret_cast<LineDef*>((objectrecord_t*) lineDefs[i])->obj);
}

bool PTR_CheckThingBlocking(Thing* thing, void* paramaters)
{
    PIT_CheckThingBlockingParams* params = reinterpret_cast<PIT_CheckThingBlockingParams*>(paramaters);

    if((thing->ddFlags & DDMF_SOLID) ||
       (thing->dPlayer && !(thing->dPlayer->flags & DDPF_CAMERA)))
    {
        dfloat tmbbox[4];

        tmbbox[BOXTOP]    = thing->origin.y + thing->radius;
        tmbbox[BOXBOTTOM] = thing->origin.y - thing->radius;
        tmbbox[BOXLEFT]   = thing->origin.x - thing->radius;
        tmbbox[BOXRIGHT]  = thing->origin.x + thing->radius;

        if(!(tmbbox[BOXRIGHT]  <= params->lineDef->aaBounds[BOXLEFT] ||
             tmbbox[BOXLEFT]   >= params->lineDef->aaBounds[BOXRIGHT] ||
             tmbbox[BOXTOP]    <= params->lineDef->aaBounds[BOXBOTTOM] ||
             tmbbox[BOXBOTTOM] >= params->lineDef->aaBounds[BOXTOP]))
        {
            if(params->lineDef->boxOnSide(tmbbox) == -1)
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
    PIT_CheckThingBlockingParams params;
    params.blocked = false;
    params.lineDef = lineDef;
    params.polyobj = polyobj;

    Vector2f bbox[2];
    bbox[0][0] = lineDef->bBox[BOXLEFT]   - DDMOBJ_RADIUS_MAX;
    bbox[0][1] = lineDef->bBox[BOXBOTTOM] - DDMOBJ_RADIUS_MAX;
    bbox[1][0] = lineDef->bBox[BOXRIGHT]  + DDMOBJ_RADIUS_MAX;
    bbox[1][1] = lineDef->bBox[BOXTOP]    + DDMOBJ_RADIUS_MAX;

    // @fixme Polyobj should return the map its linked in.
    ThingBlockmap& thingBlockmap = App::currentMap().thingBlockmap();

    duint blockBox[4];
    thingBlockmap.boxToBlocks(blockBox, bbox);
    thingBlockmap.iterate(blockBox, PTR_CheckThingBlocking, &params);

    return params.blocked;
}

bool Polyobj::iterateLineDefs(bool (*callback) (LineDef*, void*), bool retObjRecord, void* paramaters = 0)
{
    for(duint i = 0; i < _numLineDefs; ++i)
    {
        LineDef* lineDef = ((objectrecord_t*) lineDefs[i])->obj;

        if(lineDef->validCount == validCount)
            continue;

        lineDef->validCount = validCount;

        LineDef* ptr;
        if(retObjRecord)
            ptr = reinterpret_cast<LineDef*>(lineDefs[i]);
        else
            ptr = lineDef;

        if(!callback(ptr, paramaters))
            return false;
    }
    return true;
}
