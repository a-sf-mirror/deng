/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2009-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2010 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/WADFile"
#include "de/WAD"
#include "de/Block"

using namespace de;

WADFile::WADFile(const String& name, WAD& wad, const String& entryName)
    : File(name), _wad(wad), _entryName(entryName)
{}

WADFile::~WADFile()
{
    FOR_AUDIENCE(Deletion, i) i->fileBeingDeleted(*this);
    audienceForDeletion.clear();
    
    deindex();
}

void WADFile::get(Offset at, Byte* values, Size count) const
{
    wad().entryBlock(_entryName).get(at, values, count);
}
