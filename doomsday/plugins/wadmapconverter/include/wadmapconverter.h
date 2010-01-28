/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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
 * wadconverter.h: Doomsday plugin for converting DOOM, Hexen and DOOM64
 * format game data.
 */

#ifndef WADCONVERTER_H
#define WADCONVERTER_H

#include "Map"

namespace wadconverter
{
    void LoadANIMATED(void);
    void LoadANIMDEFS(void);
}

extern char* sc_String;
extern int sc_Number;
extern int sc_LineNumber;
extern char sc_ScriptName[];

namespace wadconverter
{
    void SC_Open(const char* name);
    void SC_OpenLump(lumpnum_t lump);
    void SC_OpenFile(const char* name);
    void SC_OpenFileCLib(const char* name);
    void SC_Close(void);
    boolean SC_GetString(void);
    void SC_MustGetString(void);
    void SC_MustGetStringName(char* name);
    boolean SC_GetNumber(void);
    void SC_MustGetNumber(void);
    void SC_UnGet(void);
    void SC_SkipToStartOfNextLine(void);
    void SC_ScriptError(char* message);
    boolean SC_Compare(char* text);
}

#endif /* WADCONVERTER_H */
