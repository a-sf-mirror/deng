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

void SideDef::colorTints(sidedefsection_t section, const dfloat** topColor, const dfloat** bottomColor)
{
    // Select the colors for this surface.
    switch(section)
    {
    case MIDDLE:
        if(flags & SDF_BLEND_MIDTOTOP)
        {
            *topColor = SW_toprgba;
            *bottomColor = SW_middlergba;
        }
        else if(flags & SDF_BLEND_MIDTOBOTTOM)
        {
            *topColor = SW_middlergba;
            *bottomColor = SW_bottomrgba;
        }
        else
        {
            *topColor = SW_middlergba;
            *bottomColor = NULL;
        }
        break;

    case TOP:
        if(flags & SDF_BLEND_TOPTOMID)
        {
            *topColor = SW_toprgba;
            *bottomColor = SW_middlergba;
        }
        else
        {
            *topColor = SW_toprgba;
            *bottomColor = NULL;
        }
        break;

    case BOTTOM:
        // Select the correct colors for this surface.
        if(flags & SDF_BLEND_BOTTOMTOMID)
        {
            *topColor = SW_middlergba;
            *bottomColor = SW_bottomrgba;
        }
        else
        {
            *topColor = SW_bottomrgba;
            *bottomColor = NULL;
        }
        break;

    default:
        break;
    }
}

bool SideDef::setProperty(const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_FLAGS:
        DMU_SetValue(DMT_SIDEDEF_FLAGS, &flags, args, 0);
        break;
    default:
        LOG_ERROR("SideDef::setProperty: Property %s is not writable.")
            << DMU_Str(args->prop);
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
        DMU_GetValue(DMT_SIDEDEF_FLAGS, &flags, args, 0);
        break;
    default:
        LOG_ERROR("SideDef_GetProperty: Has no property %s.")
            << DMU_Str(args->prop);
    }
    return true; // Continue iteration.
}
