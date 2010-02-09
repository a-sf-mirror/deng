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

#include "de/App"
#include "de/Map"
#include "de/LineDef"

using namespace de;

dfloat LineDef::lightLevelDelta() const
{
    return dfloat((1.0f / 255) * ((vtx2().pos.y - vtx1().pos.y) / length * 18));
}

Vector2f LineDef::unitVector() const
{
    dfloat length = M_ApproxDistancef(delta.x, delta.y);
    if(length != 0)
        return Vector2f(delta.x / length, delta.y / length);
    return Vector2f(0, 0);
}

dint LineDef::pointOnSide(dfloat x, dfloat y) const
{
    return !P_PointOnLineSide(x, y, vtx1().pos.x, vtx1().pos.y, delta.x, delta.y);
}

void LineDef::updateAABounds()
{
    bool edge;

    edge = (vtx1().pos.x < vtx2().pos.x);
    bBox[BOXLEFT]  = vtx(edge^1).pos.x;
    bBox[BOXRIGHT] = vtx(edge).pos.x;

    edge = (vtx1().pos.y < vtx2().pos.y);
    bBox[BOXBOTTOM] = vtx(edge^1).pos.y;
    bBox[BOXTOP]    = vtx(edge).pos.y;

    // Update the lineDef's slopetype.
    delta = vtx2().pos - vtx1().pos;
    if(fequal(delta.x, 0))
    {
        slopeType = ST_VERTICAL;
    }
    else if(fequal(delta.y, 0))
    {
        slopeType = ST_HORIZONTAL;
    }
    else
    {
        if(delta.y / delta.x > 0)
        {
            slopeType = ST_POSITIVE;
        }
        else
        {
            slopeType = ST_NEGATIVE;
        }
    }
}

dint LineDef::boxOnSide(dfloat xl, dfloat xh, dfloat yl, dfloat yh) const
{
    dint a = 0, b = 0;

    switch(slopeType)
    {
    default: // Shut up compiler.
      case ST_HORIZONTAL:
        a = yh > vtx1().pos.y;
        b = yl > vtx1().pos.y;
        if(delta.x < 0)
        {
            a ^= 1;
            b ^= 1;
        }
        break;

      case ST_VERTICAL:
        a = xh < vtx1().pos.x;
        b = xl < vtx1().pos.x;
        if(delta.y < 0)
        {
            a ^= 1;
            b ^= 1;
        }
        break;

      case ST_POSITIVE:
        a = pointOnSide(xl, yh);
        b = pointOnSide(xh, yl);
        break;

    case ST_NEGATIVE:
        a = pointOnSide(xh, yh);
        b = pointOnSide(xl, yl);
        break;
    }

    if(a == b)
        return a;

    return -1;
}

dint LineDef::boxOnSide(const Vector2f& bottomLeft, const Vector2f& topRight) const
{
    return boxOnSide(bottomLeft.x, topRight.x, bottomLeft.y, topRight.y);
}

#if 0
void LineDef::constructDivline(const LineDef& lineDef, divline_t* divline)
{
    assert(lineDef);
    assert(divline);

    const Vertex* vtx = lineDef.vtx1();

    divline->pos[VX] = FLT2FIX(vtx->pos[VX]);
    divline->pos[VY] = FLT2FIX(vtx->pos[VY]);
    divline->dX = FLT2FIX(lineDef->dX);
    divline->dY = FLT2FIX(lineDef->dY);
}

bool LineDef::setProperty(const setargs_t* args)
{
    switch(args->prop)
    {
    /**
     * \todo Re-implement me (conversion from objectrecord_t).
    case DMU_FRONT_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SEC, &LINE_FRONTSECTOR(lin), args, 0);
        break;

    case DMU_BACK_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SEC, &LINE_BACKSECTOR(lin), args, 0);
        break;*/

    /**
     * \todo Re-implement me (need to update all seg->sideDef ptrs).
    case DMU_SIDEDEF0:
        DMU_SetValue(DMT_LINEDEF_SIDE, &LINE_FRONTSIDE(lin), args, 0);
        break;
    case DMU_SIDEDEF1:
        DMU_SetValue(DMT_LINEDEF_SIDE, &LINE_BACKSIDE(lin), args, 0);
        break;*/
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_LINEDEF_VALIDCOUNT, &validCount, args, 0);
        break;
    case DMU_FLAGS:
        {
        SideDef& s;

        DMU_SetValue(DMT_LINEDEF_FLAGS, &flags, args, 0);

        s = LINE_FRONTSIDE(lin);
        s.SW_topsurface.update();
        s.SW_bottomsurface.update();
        s.SW_middlesurface.update();
        if((s = LINE_BACKSIDE(lin)))
        {
            s.SW_topsurface.update();
            s.SW_bottomsurface.update();
            s.SW_middlesurface.update();
        }
        break;
        }
    default:
        LOG_ERROR("LineDef::setProperty: Property %s is not writable.") << DMU_Str(args->prop);
    }

    return true; // Continue iteration.
}

bool LineDef::getProperty(setargs_t *args) const
{
    switch(args->prop)
    {
    case DMU_VERTEX0:
        {
        Vertex* v = vtx1();
        objectrecord_t* r = P_ObjectRecord(DMU_VERTEX, v);
        DMU_GetValue(DMT_LINEDEF_VERTEX1, &r, args, 0);
        break;
        }
    case DMU_VERTEX1:
        {
        Vertex* v = vtx2();
        objectrecord_t* r = P_ObjectRecord(DMU_VERTEX, v);
        DMU_GetValue(DMT_LINEDEF_VERTEX2, &r, args, 0);
        break;
        }
    case DMU_DX:
        DMU_GetValue(DMT_LINEDEF_DX, &dX, args, 0);
        break;
    case DMU_DY:
        DMU_GetValue(DMT_LINEDEF_DY, &dY, args, 0);
        break;
    case DMU_DXY:
        DMU_GetValue(DMT_LINEDEF_DX, &dX, args, 0);
        DMU_GetValue(DMT_LINEDEF_DY, &dY, args, 1);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DDVT_FLOAT, &length, args, 0);
        break;
    case DMU_ANGLE:
        DMU_GetValue(DDVT_ANGLE, &angle, args, 0);
        break;
    case DMU_SLOPE_TYPE:
        DMU_GetValue(DMT_LINEDEF_SLOPETYPE, &slopeType, args, 0);
        break;
    case DMU_FRONT_SECTOR:
        {
        Sector* sec = (LINE_FRONTSIDE(lin)? LINE_FRONTSECTOR(lin) : NULL);
        objectrecord_t* r = P_ObjectRecord(DMU_SECTOR, sec);
        DMU_GetValue(DMT_LINEDEF_SEC, &r, args, 0);
        break;
        }
    case DMU_BACK_SECTOR:
        {
        Sector* sec = (LINE_BACKSIDE(lin)? LINE_BACKSECTOR(lin) : NULL);
        objectrecord_t* r = P_ObjectRecord(DMU_SECTOR, sec);
        DMU_GetValue(DMT_LINEDEF_SEC, &r, args, 0);
        break;
        }
    case DMU_FLAGS:
        DMU_GetValue(DMT_LINEDEF_FLAGS, &flags, args, 0);
        break;
    case DMU_SIDEDEF0:
        {
        SideDef* side = LINE_FRONTSIDE(lin);
        objectrecord_t* r = P_ObjectRecord(DMU_SIDEDEF, side);
        DMU_GetValue(DMT_LINEDEF_FRONTSIDEDEF, &r, args, 0);
        break;
        }
    case DMU_SIDEDEF1:
        {
        SideDef* side = LINE_BACKSIDE(lin);
        objectrecord_t* r = P_ObjectRecord(DMU_SIDEDEF, side);
        DMU_GetValue(DMT_LINEDEF_BACKSIDEDEF, &r, args, 0);
        break;
        }
    case DMU_POLYOBJ:
        {
        bool polyLinked = (polyobjOwned != 0);
        DMU_GetValue(DDVT_BOOL, &polyLinked, args, 0);
        break;
        }
    case DMU_BOUNDING_BOX:
        if(args->valueType == DDVT_PTR)
        {
            const dfloat* bbox = bBox;
            DMU_GetValue(DDVT_PTR, &bbox, args, 0);
        }
        else
        {
            DMU_GetValue(DMT_LINEDEF_BBOX, &bBox[0], args, 0);
            DMU_GetValue(DMT_LINEDEF_BBOX, &bBox[1], args, 1);
            DMU_GetValue(DMT_LINEDEF_BBOX, &bBox[2], args, 2);
            DMU_GetValue(DMT_LINEDEF_BBOX, &bBox[3], args, 3);
        }
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_LINEDEF_VALIDCOUNT, &validCount, args, 0);
        break;
    default:
        LOG_ERROR("LineDef::getProperty: No property %s.") << DMU_Str(args->prop);
    }

    return true; // Continue iteration.
}
#endif

bool LineDef::iterateThings(bool (*callback) (Thing*, void*), void* paramaters)
{
    assert(callback);

/**
 * Linkstore is list of pointers gathered when iterating stuff.
 * This is pretty much the only way to avoid *all* potential problems
 * caused by callback routines behaving badly (moving or destroying
 * Things). The idea is to get a snapshot of all the objects being
 * iterated before any callbacks are called. The hardcoded limit is
 * a drag, but I'd like to see you iterating 2048 Things/LineDefs in one block.
 */
#define MAXLINKED           2048

    void* linkstore[MAXLINKED];
    void** end = linkstore;

    Map& map = App::currentMap(); // @fixme LineDef should tell us which map it belongs to.

    NodePile::Index root = map.lineDefLinks[P_ObjectRecord(DMU_LINEDEF, this)->id - 1];
    NodePile::LinkNode* ln = map.lineDefNodes->nodes;
    for(NodePile::Index nix = ln[root].next; nix != root; nix = ln[nix].next)
        *end++ = ln[nix].ptr;

    for(void** it = linkstore; it < end; it++)
        if(!callback(reinterpret_cast<Thing*>(*it), paramaters))
            return false;

    return true;

#undef MAXLINKED
}
