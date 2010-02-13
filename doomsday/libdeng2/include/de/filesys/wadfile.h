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

#ifndef LIBDENG2_WADFILE_H
#define LIBDENG2_WADFILE_H

#include "../File"
#include "../String"

namespace de
{
    class WAD;
    
    /**
     * Accesses data of a file within a WAD.
     *
     * @ingroup fs
     */
    class WADFile : public File
    {
    public:
        /**
         * Constructs a WAD file.
         *
         * @param name       Name of the file.
         * @param wad        WAD where the contents of the file are located.
         * @param entryPath  Path of the file's entry within the archive.
         */
        WADFile(const String& name, WAD& wad, const String& entryPath);
        
        ~WADFile();

        /// Returns the WAD of the file (non-modifiable).
        const WAD& wad() const { return _wad; }

        // Implements IByteArray.
        void get(Offset at, Byte* values, Size count) const;
        
    private:
        WAD& _wad;
        
        /// Name of the entry within the WAD.
        String _entryName;
    };
}

#endif /* LIBDENG2_WADFILE_H */
