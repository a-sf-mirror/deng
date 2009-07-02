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

#ifndef LIBDENG2_NATIVEFOLDER_H
#define LIBDENG2_NATIVEFOLDER_H

#include <de/Folder>
#include <de/String>

namespace de
{
    /**
     * NativeFolder reads from and writes to directories in the native file system.
     */
    class PUBLIC_API NativeFolder : public Folder
    {
    public:
        DEFINE_ERROR(NotFoundError);
        
    public:
        /**
         * Constructs a NativeFolder that accesses a directory in the native file system.
         *
         * @param name  Name of the folder object.
         * @param nativePath  Path of the native directory.
         */
        NativeFolder(const std::string& name, const std::string& nativePath);
        
        virtual ~NativeFolder();
        
        void populate();
                
    private:
        const String nativePath_;
    };
}

#endif /* LIBDENG2_NATIVEFOLDER_H */
