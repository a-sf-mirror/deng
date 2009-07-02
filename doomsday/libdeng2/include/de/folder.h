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

#ifndef LIBDENG2_FOLDER_H
#define LIBDENG2_FOLDER_H

#include <de/deng.h>
#include <de/Error>
#include <de/File>

#include <map>

namespace de
{
    /**
     * A folder contains a set of files. It is used for building a tree of files
     * in the file system (de::FS). This is the base class for all types of folders.
     */
    class PUBLIC_API Folder : public File
    {
    public:
        DEFINE_ERROR(DuplicateNameError);
        
    public:
        Folder(const std::string& name = "");
        
        virtual ~Folder();
    
        /**
         * Populates the folder with a set of File instances. Subclasses will
         * implement this method to enumerate their contents. Every populated file
         * will also be added to the file system's main index. This may be called
         * more than once during the life time of the folder, for example when it's 
         * necessary to synchronize the file instances with the contents of a hard 
         * drive folder.
         */
        virtual void populate() = 0;

        /**
         * Destroys the contents of the folder. All contained file objects are deleted.
         */
        virtual void clear();

        /**
         * Adds an object to the folder. The object must be an instance of a class
         * derived from File.
         *
         * @param fileObject  Object to add to the folder. The folder takes 
         *      ownership of this object. Cannot be NULL.
         *
         * @return Reference to @c fileObject, for convenience.
         */
        template <typename T>
        T& add(T* fileObject) {
            assert(fileObject != 0);
            add(dynamic_cast<File*>(fileObject));
            return *fileObject;
        }
        
        /**
         * Adds a file instance to the contents of the folder. 
         *
         * @param file  File to add. The folder takes ownership of this instance.
         *
         * @return Reference to the file, for convenience.
         */
        virtual File& add(File* file);

        /**
         * Removes a file from the folder, by name. The file is not deleted. The
         * ownership of the file is given to the caller.
         *
         * @return The removed file object. Ownership of the object is given to
         * the caller.
         */
        File* remove(const std::string& name);

        template <typename T>
        T* remove(T* fileObject) {
            assert(fileObject != 0);
            remove(dynamic_cast<File*>(fileObject));
            return fileObject;
        }

        /**
         * Removes a file from the folder. The file is not deleted. The ownership 
         * of the file is given to the caller.
         *
         * @return The removed file object. Ownership of the object is given to
         * the caller.
         */
        virtual File* remove(File* file);
                    
    private:
        /// A map of file names to file instances.
        typedef std::map<std::string, File*> Contents;
        Contents contents_;
    };
}

#endif /* LIBDENG2_FOLDER_H */
