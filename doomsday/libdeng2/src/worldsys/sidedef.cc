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

#include "de/SideDef"
#include "de/Log"

using namespace de;

SideDef::SideDef(Sector* sector, dshort flags, Material* middleMaterial,
    const Vector2f& middleOffset, const Vector3f& middleTintColor,
    dfloat middleOpacity, Material* topMaterial, const Vector2f& topOffset,
    const Vector3f& topTintColor, Material* middleMaterial,
    Material* bottomMaterial, const Vector2f& bottomOffset, const Vector3f& bottomTintColor)
 : _flags(flags), _lineDef(0), _sector(sector),
   _middle(Vector3f(0, 0, 0), middleMaterial, middleOffset, middleOpacity, BM_NORMAL, middleTintColor),
   _top(Vector3f(0, 0, 0), topMaterial, topOffset, 1, BM_NORMAL, topTintColor),
   _bottom(Vector3f(0, 0, 0), bottomMaterial, bottomOffset, 1, BM_NORMAL, bottomTintColor)
{
    middle().owner = reinterpret_cast<void*>(this);
    top().owner = reinterpret_cast<void*>(this);
    bottom().owner = reinterpret_cast<void*>(this);
}

SideDef::~SideDef()
{}

SideDef::setLineDef(LineDef* lineDef)
{
    _lineDef = lineDef;
}

void SideDef::colorTints(Section section, const dfloat** topColor, const dfloat** bottomColor)
{
    // Select the colors for this surface.
    switch(section)
    {
    case MIDDLE:
        if(_flags[BLEND_MIDDLE2TOP])
        {
            *topColor = top().rgba;
            *bottomColor = middle().rgba;
        }
        else if(_flags[BLEND_MIDDLE2BOTTOM])
        {
            *topColor = middle().rgba;
            *bottomColor = bottom().rgba;
        }
        else
        {
            *topColor = middle().rgba;
            *bottomColor = NULL;
        }
        break;

    case TOP:
        if(_flags[BLEND_TOP2MIDDLE])
        {
            *topColor = top().rgba;
            *bottomColor = middle().rgba;
        }
        else
        {
            *topColor = top().rgba;
            *bottomColor = NULL;
        }
        break;

    case BOTTOM:
        // Select the correct colors for this surface.
        if(_flags[BLEND_BOTTOM2MIDDLE])
        {
            *topColor = middle().rgba;
            *bottomColor = bottom().rgba;
        }
        else
        {
            *topColor = bottom().rgba;
            *bottomColor = NULL;
        }
        break;

    default:
        break;
    }
}

#if 0
bool SideDef::setProperty(const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_FLAGS:
        DMU_SetValue(DMT_SIDEDEF_FLAGS, &_flags, args, 0);
        break;
    default:
        throw UnknownPropertyError("SideDef::setProperty", "Property " + DMU_Str(args->prop) + " not known.");
    }

    return true; // Continue iteration.
}

bool SideDef::getProperty(setargs_t* args) const
{
    switch(args->prop)
    {
    case DMU_SECTOR:
        {
        objectrecord_t* r = P_ObjectRecord(DMU_SECTOR, sector);
        DMU_GetValue(DMT_SIDEDEF_SECTOR, &r, args, 0);
        break;
        }
    case DMU_LINEDEF:
        {
        objectrecord_t* r = P_ObjectRecord(DMU_LINEDEF, lineDef);
        DMU_GetValue(DMT_SIDEDEF_LINEDEF, &r, args, 0);
        break;
        }
    case DMU_FLAGS:
        DMU_GetValue(DMT_SIDEDEF_FLAGS, &_flags, args, 0);
        break;
    default:
        throw UnknownPropertyError("SideDef::getProperty", "Property " + DMU_Str(args->prop) + " not known.");
    }
    return true; // Continue iteration.
}
#endif
