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

#include "de/Log"
#include "de/Seg"
#include "de/LineDef"

using namespace de;

Seg::Seg(HalfEdge& _halfEdge, SideDef* sideDef, bool back)
 :  halfEdge(&_halfEdge), _sideDef(sideDef), onBack(back)
{
    if(_sideDef)
    {
        const LineDef& lineDef = _sideDef->lineDef();
        const Vertex* vtx = lineDef.buildData.v[onBack];
        offset = vtx->pos.distance(halfEdge->vertex->pos);
    }

    Vector2d diff = halfEdge->twin->vertex->pos - halfEdge->vertex->pos;
    angle = bamsAtan2(dint(diff.y), dint(diff.x)) << FRACBITS;

    // Calculate the length of the segment. We need this for
    // the texture coordinates. -jk
    length = diff.length();
    if(length == 0)
        length = 0.01f; // Hmm...

    // Calculate the surface normals.
    if(_sideDef)
    {
        MSurface& surface = _sideDef->middle();

        surface.normal.x = (halfEdge->twin->vertex->pos.y - halfEdge->vertex->pos.y) / length;
        surface.normal.y = (halfEdge->vertex->pos.x - halfEdge->twin->vertex->pos.x) / length;
        surface.normal.z = 0;

        // All surfaces of this seg have the same normal.
        _sideDef->top().normal = surface.normal;
        _sideDef->bottom().normal = surface.normal;
    }
}

#if 0
bool Seg::setProperty(const setargs_t* args)
{
    throw UnknownPropertyError("Seg::setProperty", "Property " + DMU_Str(args->prop) + " not known.");
}

bool Seg::getProperty(setargs_t* args) const
{
    switch(args->prop)
    {
    case DMU_VERTEX0:
        {
        objectrecord_t* r = P_ObjectRecord(DMU_VERTEX, halfEdge->HE_v1);
        DMU_GetValue(DMT_SEG_VERTEX1, &r, args, 0);
        break;
        }
    case DMU_VERTEX1:
        {
        objectrecord_t* r = P_ObjectRecord(DMU_VERTEX, halfEdge->HE_v2);
        DMU_GetValue(DMT_SEG_VERTEX2, &r, args, 0);
        break;
        }
    case DMU_LENGTH:
        DMU_GetValue(DMT_SEG_LENGTH, &length, args, 0);
        break;
    case DMU_OFFSET:
        DMU_GetValue(DMT_SEG_OFFSET, &offset, args, 0);
        break;
    case DMU_SIDEDEF:
        {
        objectrecord_t* r = P_ObjectRecord(DMU_SIDEDEF, sideDef);
        DMU_GetValue(DMT_SEG_SIDEDEF, &r, args, 0);
        break;
        }
    case DMU_LINEDEF:
        {
        void* ptr = NULL;
        if(sideDef)
            ptr = P_ObjectRecord(DMU_LINEDEF, sideDef->lineDef);
        DMU_GetValue(DMT_SEG_LINEDEF, &ptr, args, 0);
        break;
        }
    case DMU_FRONT_SECTOR:
        {
        Subsector* subsector = (Subsector*) halfEdge->face->data;
        objectrecord_t* r = (subsector->sector && sideDef)?
            P_ObjectRecord(DMU_SECTOR, subsector->sector) : NULL;
        DMU_GetValue(DMT_SEG_FRONTSECTOR, &r, args, 0);
        break;
        }
    case DMU_BACK_SECTOR:
        {
        void* ptr = NULL;

        if(halfEdge->twin)
        {
            Subsector* subsector = (Subsector*) halfEdge->twin->face->data;

            /**
             * The sector and the sidedef are checked due to the possibility
             * of the "back-sided window effect", which the games are not
             * currently aware of.
             */
            if(subsector->sector && ((Seg*) halfEdge->twin->data)->sideDef)
                ptr = P_ObjectRecord(DMU_SECTOR, subsector->sector);
        }

        DMU_GetValue(DMT_SEG_BACKSECTOR, &ptr, args, 0);
        break;
        }
    case DMU_ANGLE:
        DMU_GetValue(DMT_SEG_ANGLE, &angle, args, 0);
        break;
    default:
        throw UnknownPropertyError("Seg::getProperty", "Property " + DMU_Str(args->prop) + " not known.");
    }

    return true; // Continue iteration.
}
#endif
