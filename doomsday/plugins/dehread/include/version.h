/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * version.h: Version numbering, naming etc.
 */

#ifndef __DEHREAD_VERSION_H__
#define __DEHREAD_VERSION_H__

#ifndef DEHREAD_VER_ID
#  ifdef _DEBUG
#    define DEHREAD_VER_ID "+D Doomsday"
#  else
#    define DEHREAD_VER_ID "Doomsday"
#  endif
#endif

// Used to derive filepaths.
#define PLUGIN_NAME         "dpdehread"

// Presented to the user in dialogs, messages etc.
#define PLUGIN_NICENAME     "Deh Reader"
#define PLUGIN_DETAILS      "Deh Reader is based on ZDoom's Deh patch reader."

#define PLUGIN_VERSION_TEXT "1.2.6"
#define PLUGIN_VERSION_TEXTLONG "Version" PLUGIN_VERSION_TEXT " " __DATE__ " (" DEHREAD_VER_ID ")"
#define PLUGIN_VERSION_NUMBER 1,2,6,0 // For WIN32 version info.

#endif
