/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/nativefile.h"

using namespace de;

NativeFile::NativeFile(const std::string& name, const std::string& nativePath, const Mode& mode)
    : File(name), path_(nativePath), mode_(mode)
{
    std::cout << "NativeFile: " << name << ": " << nativePath << "\n";
}

NativeFile::~NativeFile()
{}

duint NativeFile::size() const
{
    return 0;
}

void NativeFile::get(Offset at, Byte* values, duint count) const
{}

void NativeFile::set(Offset at, const Byte* values, duint count)
{}
