/**
 * @file sidedef.h
 * Map SideDef. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_MAP_SIDEDEF
#define LIBDENG_MAP_SIDEDEF

#include "r_data.h"
#include "p_dmu.h"

/**
 * Update the SideDef's map space surface base origins according to the points
 * defined by the associated LineDef's vertices and the plane heights of the
 * Sector on this side. If no LineDef is presently associated this is a no-op.
 *
 * @param sideDef  SideDef instance.
 */
void SideDef_UpdateBaseOrigins(SideDef* sideDef);

/**
 * Update the SideDef's map space surface tangents according to the points
 * defined by the associated LineDef's vertices. If no LineDef is presently
 * associated this is a no-op.
 *
 * @param sideDef  SideDef instance.
 */
void SideDef_UpdateSurfaceTangents(SideDef* sideDef);

/**
 * Return the active blending mode for the specified @a section of this sidedef.
 *
 * @param sideDef  SideDef instance.
 * @param section  Section of the sidedef caller is interested in.
 * @return  Blendmode to use for this section.
 */
blendmode_t SideDef_SurfaceBlendMode(SideDef* side, SideDefSection section);

/**
 * Retrieve surface color(s) for the specified @a section of this sidedef
 * according to the current blending flags.
 *
 * @note Depending on the blend flags one or other color may be disabled, in
 *       which case @c NULL is written to the relevant specified address.
 *
 * @param sideDef  SideDef instance.
 * @param section  Section of the sidedef caller is interested in.
 * @param bottomColor  If not @c NULL, the bottom color will be written here.
 * @param topColor  If not @c NULL, the top color will be written here.
 */
void SideDef_SurfaceColors(SideDef* sideDef, SideDefSection section,
    const float** bottomColor, const float** topColor);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param sidedef  SideDef instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int SideDef_GetProperty(const SideDef* sideDef, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param sidedef  SideDef instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int SideDef_SetProperty(SideDef* sideDef, const setargs_t* args);

#endif /// LIBDENG_MAP_SIDEDEF
