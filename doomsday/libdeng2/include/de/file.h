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

#ifndef LIBDENG2_FILE_H
#define LIBDENG2_FILE_H

#include <de/IByteArray>
#include <de/String>

namespace de
{
    class FS;
    class Folder;
    
    /**
     * Base class for all files stored in the file system (de::FS).
     * 
     * Note that folders are also files.
     */
    class PUBLIC_API File : public IByteArray
    {
    public:
        DEFINE_ERROR(ReadOnlyError);
        
    public:
        File(const std::string& name = "");
        
        virtual ~File();

        /// Returns a reference to the application's file system.
        FS& fileSystem();
        
        /// Returns the name of the file.
        const String& name() const {
            return name_;
        }

        /**
         * Sets the parent folder of this file.
         */
        void setParent(Folder* parent) { parent_ = parent; }

        /**
         * Returns the parent folder. May be NULL.
         */
        Folder* parent() const { return parent_; }
        
        /**
         * Forms the complete path of this file object.
         *
         * @return Path of the object. This is not a native path, but instead 
         *      intended for de::FS.
         */
        const String path() const;
        
        Size size() const;
		void get(Offset at, Byte* values, Size count) const;
		void set(Offset at, const Byte* values, Size count);
        
    private:
        /// The parent folder.
        Folder* parent_;
        
        /// Name of the file.
        String name_;
    }; 
}

#endif /* LIBDENG2_FILE_H */
