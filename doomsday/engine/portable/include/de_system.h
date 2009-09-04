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

/**
 * de_system.h: Operating System Dependent Subsystems
 */

#ifndef __DOOMSDAY_SYSTEM__
#define __DOOMSDAY_SYSTEM__

#include "sys_system.h"
#include "sys_console.h"
#include "sys_direc.h"
#include "sys_file.h"
#include "sys_input.h"
#include "sys_network.h"
#include "sys_sock.h"
#include "sys_master.h"
#include "sys_timer.h"
#include "sys_opengl.h"

// Use SDL for window management under *nix
#ifdef UNIX
#  define USING_SDL_WINDOW
#  include "../../unix/include/sys_path.h"
#endif

#include "sys_window.h"

#endif
