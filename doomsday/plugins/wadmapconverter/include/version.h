/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef WADCONVERTER_VERSION_H
#define WADCONVERTER_VERSION_H

/**
 * @file version.h
 *
 * Version numbering.
 */

#ifndef WADCONVERTER_VER_ID
#  ifdef _DEBUG
#    define WADCONVERTER_VER_ID "+D Doomsday"
#  else
#    define WADCONVERTER_VER_ID "Doomsday"
#  endif
#endif

// Used to derive filepaths.
#define PLUGIN_NAME         "dpwadconverter"

// Presented to the user in dialogs, messages etc.
#define PLUGIN_NICENAME     "WAD Converter"
#define PLUGIN_DETAILS      "Doomsday plugin for loading WAD format data."

#define PLUGIN_VERSION_TEXT "1.0.5"
#define PLUGIN_VERSION_TEXTLONG "Version" PLUGIN_VERSION_TEXT " " __DATE__ " (" WADCONVERTER_VER_ID ")"
#define PLUGIN_VERSION_NUMBER 1,0,5,0 // For WIN32 version info.

#endif /* WADCONVERTER_VERSION_H */
