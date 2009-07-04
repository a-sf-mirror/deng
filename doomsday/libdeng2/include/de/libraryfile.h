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

#ifndef LIBDENG2_LIBRARYFILE_H
#define LIBDENG2_LIBRARYFILE_H

#include <de/File>

namespace de
{
    /**
     * LibraryFile provides a way to load a shared library.
     *
     * @see Library
     *
     * @ingroup fs
     */
    class LibraryFile : public File
    {
    public:
        /**
         * Constructs a new LibraryFile instance.
         *
         * @param source  Library file. Ownership transferred to LibraryFile.
         */
        LibraryFile(File* source);
        
        virtual ~LibraryFile();

    public:
        /**
         * Determines whether a file appears suitable for use with LibraryFile.
         *
         * @param file  File whose content to recognize.
         */
        static bool recognize(const File& file);
        
    private:
        File* source_;
    };
}

#endif /* LIBDENG2_LIBRARYFILE_H */
