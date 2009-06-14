/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/*
 * sys_timer.h: Timing Subsystem
 */

#ifndef __DOOMSDAY_SYSTEM_TIMER_H__
#define __DOOMSDAY_SYSTEM_TIMER_H__

#include "dd_types.h"

void            Sys_InitTimer(void);
void            Sys_ShutdownTimer(void);
int             Sys_GetTime(void);
double          Sys_GetTimef(void);
double          Sys_GetSeconds(void);
uint            Sys_GetRealTime(void);
double          Sys_GetRealSeconds(void);
void            Sys_TicksPerSecond(float num);

#endif
