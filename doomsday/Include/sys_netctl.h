/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sys_netctl.h: Network Control Connections
 */

#ifndef __DOOMSDAY_NETWORK_CONTROL_H__
#define __DOOMSDAY_NETWORK_CONTROL_H__

boolean N_InitControl(boolean inServerMode, ushort userPort);
void 	N_ShutdownControl(void);
void 	N_Listen(void);
boolean	N_OpenControlSocket(const char *address, ushort port);
void 	N_AskInfo(boolean quit);
boolean	N_GetHostInfo(int index, struct serverinfo_s *info);
int		N_GetHostCount(void);

#endif 

