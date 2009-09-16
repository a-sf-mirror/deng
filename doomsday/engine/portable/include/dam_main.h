/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * dam_main.h: Doomsday Archived Map (DAM) reader
 */

#ifndef __DOOMSDAY_ARCHIVED_MAP_MAIN_H__
#define __DOOMSDAY_ARCHIVED_MAP_MAIN_H__

#include "m_string.h"

boolean         DAM_TryMapConversion(const char* mapID);
ddstring_t*     DAM_ComposeArchiveMapFilepath(const char* mapID);
#endif
