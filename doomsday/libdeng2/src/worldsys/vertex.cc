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

#include "de/Vertex"
#include "de/Log"

using namespace de;

bool Vertex::setProperty(const setargs_t* args)
{
    // Vertices are not writable through DMU.
    LOG_ERROR("Vertex::SetProperty: Not writable.");
    return true; // Continue iteration.
}

bool Vertex::getProperty(setargs_t* args) const
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
        LOG_Error("Vertex::getProperty: No property %s.")
            << DMU_Str(args->prop);
    }

    return true; // Continue iteration.
}
