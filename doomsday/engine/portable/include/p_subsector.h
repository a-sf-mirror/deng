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

#ifndef DOOMSDAY_MAP_SUBSECTOR_H
#define DOOMSDAY_MAP_SUBSECTOR_H

#include "r_data.h"
#include "p_dmu.h"

subsector_t*    P_CreateSubsector(void);
void            P_DestroySubsector(subsector_t* subsector);

void            Subsector_SpreadObjs(subsector_t* subsector);
void            Subsector_UpdateMidPoint(subsector_t* subsector);

boolean         Subsector_GetProperty(const subsector_t* subsector, setargs_t* args);
boolean         Subsector_SetProperty(subsector_t* subsector, const setargs_t* args);

#endif /* DOOMSDAY_MAP_SUBSECTOR */
