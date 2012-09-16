/**
 * @file quadtreevisual.h
 * Graphical Quadtree visual. @ingroup render
 *
 * @author Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_QUADTREE_VISUAL_H
#define LIBDENG_RENDER_QUADTREE_VISUAL_H

#include "quadtree.h"

/**
 * Render a visual for @a quadtree to assist in debugging (etc...).
 *
 * This visualizer assumes that the caller has already configured the GL render
 * state (projection matrices, scale, etc...) as desired prior to calling. This
 * function guarantees to restore the previous GL state if any changes are made
 * to it.
 *
 * @note Internally the visual uses fixed unit dimensions [1x1] for cells and
 *       therefore the caller should scale the appropriate matrix to scale this
 *       visual as desired.
 *
 * @param quadtree      QuadtreeBase-derived class instance.
 */
void Rend_QuadtreeDebug(QuadtreeBase& quadtree);

#endif /// LIBDENG_RENDER_QUADTREE_VISUAL_H
