/**
 * @file rend_misc.c
 * Miscellaneous renderer routines. @ingroup render
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

#ifndef LIBDENG_REND_MISC_H
#define LIBDENG_REND_MISC_H

#include "p_mapdata.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Draws a textured triangle using the currently bound gl texture.
 * Used to draw mobj angle direction arrow.
 *
 * @param pos3f         Coordinates of the center of the base of the
 *                      triangle (in "world" coordinates [VX, VY, VZ]).
 * @param a             Angle to point the triangle in.
 * @param s             Scale of the triangle.
 * @param color         Color to make the box (uniform vertex color).
 * @param alpha         Alpha to make the box (uniform vertex color).
 */
void Rend_RenderArrow(coord_t const pos[3], float a, float s, float const color[3], float alpha);

/**
 * Draws a textured cube using the currently bound gl texture.
 * Used to draw mobj bounding boxes.
 *
 * @param pos           Coordinates of the center of the box (in "world"
 *                      coordinates [VX, VY, VZ]).
 * @param w             Width of the box.
 * @param l             Length of the box.
 * @param h             Height of the box.
 * @param color         Color to make the box (uniform vertex color).
 * @param alpha         Alpha to make the box (uniform vertex color).
 * @param br            Border amount to overlap box faces.
 * @param alignToBase   If @c true, align the base of the box
 *                      to the Z coordinate.
 */
void Rend_RenderBoundingBox(coord_t const pos[3], coord_t w, coord_t l, coord_t h,
    float a, float const color[3], float alpha, float br, boolean alignToBase);

/**
 * Renders bounding boxes for all mobj's (linked in sec->mobjList, except
 * the console player) in all sectors that are currently marked as vissible.
 *
 * Depth test is disabled to show all mobjs that are being rendered, regardless
 * if they are actually vissible (hidden by previously drawn map geometry).
 */
void Rend_RenderBoundingBoxes(void);

/**
 * To be called upon renderer reset/restart to clear the GL data used for the
 * visualization of object bounding boxes.
 */
void Rend_ClearBoundingBoxGLData(void);

/**
 * Draw the surface normals, primarily for debug.
 */
void Rend_RenderSurfaceVectors(void);

/**
 * Draw the various vertex debug aids.
 */
void Rend_RenderVertexes(void);

/**
 * Draw the debugging aids for visualizing sound origins.
 */
void Rend_RenderSoundOrigins(void);

void Rend_LightVertex(ColorRawf* color, const rvertex_t* vertex, float lightLevel,
    const float* ambientColor);

void Rend_LightVertices(uint numVerts, ColorRawf* colors, const rvertex_t* verts,
    float lightLevel, const float* ambientColor);

void Rend_SetUniformColor(ColorRawf* colors, uint num, float glow);

void Rend_SetAlpha(ColorRawf* colors, uint num, float alpha);

void Rend_ApplyTorchLight(float* color, float distance);

void Rend_VertexColorsApplyTorchLight(ColorRawf* colors, const rvertex_t* verts, size_t numVerts);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_REND_MISC_H */
