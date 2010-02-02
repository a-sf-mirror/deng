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

#ifndef LIBDENG2_MSURFACE_H
#define LIBDENG2_MSURFACE_H

namespace de
{
    bool            Surface_GetProperty(const surface_t* suf, setargs_t* args);
    bool            Surface_SetProperty(surface_t* suf, const setargs_t* args);
    void            Surface_Update(surface_t* suf);

    bool            Surface_SetMaterial(surface_t* suf, material_t* mat, boolean fade);
    bool            Surface_SetMaterialOffsetX(surface_t* suf, dfloat x);
    bool            Surface_SetMaterialOffsetY(surface_t* suf, dfloat y);
    bool            Surface_SetMaterialOffsetXY(surface_t* suf, dfloat x, dfloat y);
    bool            Surface_SetColorR(surface_t* suf, dfloat r);
    bool            Surface_SetColorG(surface_t* suf, dfloat g);
    bool            Surface_SetColorB(surface_t* suf, dfloat b);
    bool            Surface_SetColorA(surface_t* suf, dfloat a);
    bool            Surface_SetColorRGBA(surface_t* suf, dfloat r, dfloat g, dfloat b, dfloat a);
    bool            Surface_SetBlendMode(surface_t* suf, blendmode_t blendMode);
}

#endif /* LIBDENG2_MSURFACE_H */
