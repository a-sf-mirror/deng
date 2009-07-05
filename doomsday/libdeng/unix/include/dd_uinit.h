/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * dd_uinit.h: Unix Initialization.
 */

#ifndef __DOOMSDAY_UINIT_H__
#define __DOOMSDAY_UINIT_H__

#include "dd_pinit.h"

typedef struct {
    boolean         userDirOk;

    //void*           hInstGame; // Instance handle to the game library.
    //lt_dlhandle     hInstPlug[MAX_PLUGS]; // Instance handle to all other libs.
    GETGAMEAPI      GetGameAPI;
} application_t;

extern uint windowIDX; // Main window.
extern application_t app;

void            DD_Shutdown(void);

#endif
