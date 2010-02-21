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

#include "de/Log"
#include "de/Vector"
#include "de/Vertex"
#include "de/LineDef"

using namespace de;

namespace {
/**
 * @return          The width (world units) of the shadow edge.
 *                  It is scaled depending on the length of the edge.
 */
dfloat shadowEdgeWidth(const Vector2f& edge)
{
    const dfloat normalWidth = 20; //16;
    const dfloat maxWidth = 60;

    // A long edge?
    ddouble length = edge.length();
    if(length > 600)
    {
        ddouble w = length - 600;
        if(w > 1000)
            w = 1000;
        return normalWidth + dfloat(w / 1000 * maxWidth);
    }

    return normalWidth;
}

/**
 * Line1 and line2 are the (dx,dy)s for two lines, connected at the
 * origin (0,0).  Dist1 and dist2 are the distances from these lines.
 * The returned point (in 'point') is dist1 away from line1 and dist2
 * from line2, while also being the nearest point to the origin (in
 * case the lines are parallel).
 */
void cornerNormalPoint(const Vector2d& line1, ddouble dist1,
                       const Vector2d& line2, ddouble dist2, Vector2d* point,
                       Vector2d* lp)
{
    // Calculate normals for both lines.
    ddouble len1 = line1.length();
    Vector2d norm1 = Vector2d(-line1.y, line1.x);
    norm1.x /= len1; norm1.y /= len1;
    norm1 *= dist1;

    ddouble len2 = line2.length();
    Vector2d norm2 = Vector2d(line2.y, -line2.x);
    norm2.x /= len2; norm2.y /= len2;
    norm2 *= dist2;

    // Do we need to calculate the extended points, too?  Check that
    // the extension does not bleed too badly outside the legal shadow
    // area.
    if(lp)
    {
        (*lp)    = line2;
        (*lp).x /= len2; (*lp).y /= len2;
        (*lp)   *= dist2;
    }

    // Are the lines parallel?  If so, they won't connect at any
    // point, and it will be impossible to determine a corner point.
    if(line1.isParallel(line2))
    {
        // Just use a normal as the point.
        if(point) *point = norm1;
        return;
    }

    // Find the intersection of normal-shifted lines.  That'll be our
    // corner point.
    if(point)
        V2_Intersection(norm1, line1, norm2, line2, point);
}
}

void MVertex::updateShadowOffsets()
{
    if(!(numLineOwners > 0))
        return;

    lineowner_t& own = *lineOwners;
    do
    {
        const LineDef& lineDefB = *own.lineDef;
        const LineDef& lineDefA = *own.next().lineDef;

        Vector2f left, right;
        if(reinterpret_cast<MVertex*>(lineDefB.vtx1().data) == this)
            right = lineDefB.direction;
        else
            right = -lineDefB.direction;

        if(reinterpret_cast<MVertex*>(lineDefA.vtx1().data) == this)
            left = -lineDefA.direction;
        else
            left = lineDefA.direction;

        // The left side is always flipped.
        left = -left;

        cornerNormalPoint(left, shadowEdgeWidth(left),
                          right, shadowEdgeWidth(right),
                          &own.shadowOffsets.inner, &own.shadowOffsets.extended);

        own = own.next();
    } while(&own != lineOwners);
}

#if 0
bool MVertex::setProperty(const setargs_t* args)
{
    // Vertices are not writable through DMU.
    throw WriteError("Vertex::setProperty", "Not writable.");
}

bool MVertex::getProperty(setargs_t* args) const
{
    switch(args->prop)
    {
    case DMU_X:
        {
        dfloat pos = vtx->pos[VX];
        DMU_GetValue(DMT_VERTEX_POS, &pos, args, 0);
        break;
        }
    case DMU_Y:
        {
        dfloat pos = vtx->pos[VY];
        DMU_GetValue(DMT_VERTEX_POS, &pos, args, 0);
        break;
        }
    case DMU_XY:
        {
        dfloat pos[2];
        pos[VX] = vtx->pos[VX];
        pos[VY] = vtx->pos[VY];
        DMU_GetValue(DMT_VERTEX_POS, &pos[VX], args, 0);
        DMU_GetValue(DMT_VERTEX_POS, &pos[VY], args, 1);
        break;
        }
    default:
        throw UnknownPropertyError("Vertex::getProperty", "Property " + DMU_Str(args->prop) + " not known.");
    }

    return true; // Continue iteration.
}
#endif
