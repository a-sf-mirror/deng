/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * blockmapvisual.h: Blockmap visualizer; a debugging aid.
 */

#ifndef DOOMSDAY_RENDER_BLOCKMAPVISUAL_H
#define DOOMSDAY_RENDER_BLOCKMAPVISUAL_H

#define BLOCKMAPVISUAL_MOBJS        0
#define BLOCKMAPVISUAL_LINEDEFS     1
#define BLOCKMAPVISUAL_SUBSECTORS   2

void            Rend_BlockmapVisual(struct gamemap_s* map, byte mode);

#endif /* DOOMSDAY_RENDER_BLOCKMAPVISUAL_H */
