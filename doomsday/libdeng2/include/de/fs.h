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

#ifndef LIBDENG2_FS_H
#define LIBDENG2_FS_H

#include <de/deng.h>
#include <de/Error>
#include <de/Folder>

#include <map>

/**
 * @defgroup fs File System
 *
 * The file system (de::FS) governs a tree of files and folders, and provides the means to
 * access all data in libdeng2.
 */

namespace de
{
    /**
     * The file system maintains a tree of files and folders. It provides a way
     * to quickly and efficiently locate files anywhere in the tree. It also
     * maintains semantic information about the structure and content of the 
     * file tree, allowing others to know how to treat the files and folders.
     *
     * In practice, the file system consists of a tree of File and Folder instances.
     * These instances are generated by the Feed objects attached to the folders.
     * For instance, a DirectoryFeed will generate the appropriate File and Folder
     * instances for a directory in the native file system. 
     *
     * The file system can be repopulated at any time to resynchronize it with the
     * source data. Repopulation is nondestructive as long as the source data has not
     * changed. Repopulation is needed for instance when native files get deleted 
     * in the directory a folder is feeding on. The feeds are responsible for
     * deciding when instances get out-of-date and need to be deleted (pruning). 
     * Pruning occurs when a folder that is already populated with files is repopulated.
     *
     * @ingroup fs
     */
    class PUBLIC_API FS
    {
    public:
        /// No index is found for the specified type. @ingroup errors
        DEFINE_ERROR(UnknownTypeError);
        
        typedef std::multimap<std::string, File*> Index;
        typedef std::pair<std::string, File*> IndexEntry;
        
    public:
        FS();
        
        virtual ~FS();

        void printIndex();
        
        Folder& root() { return root_; }
        
        /**
         * Refresh the file system. Populates all folders with files from the feeds.
         */
        void refresh();
        
        /**
         * Retrieves a folder in the file system. The folder gets created if it
         * does not exist. Any missing parent folders will also be created.
         *
         * @param path  Path of the folder. Relative to the root folder.
         */
        Folder& getFolder(const String& path);
        
        /**
         * Creates an interpreter for the data in a file. 
         *
         * @param sourceData  File with the source data.
         *
         * @return  If the format of the source data was recognized, returns a new File
         *      that can be used for accessing the data. Ownership of the @c sourceData 
         *      will be transferred to the new interpreter File instance.
         *      If the format was not recognized, @c sourceData is returned as is.
         */
        File* interpret(File* sourceData);
        
        /**
         * Provides access to the main index of the file system. This can be used for
         * efficiently looking up files based on name. @note The file names are
         * indexed in lower case.
         */
        const Index& nameIndex() const { return index_; }
        
        /**
         * Retrieves the index of files of a particular type.
         *
         * @param typeIdentifier  Type identifier to look for. Use the TYPE_NAME() macro. 
         *
         * @return  A subset of the main index containing only the entries of the given type.
         *
         * For example, to look up the index for NativeFile instances:
         * @code
         * FS::Index& nativeFileIndex = App::the().fileSystem().indexFor(TYPE_NAME(NativeFile));
         * @endcode
         */
        const Index& indexFor(const std::string& typeIdentifier) const;
        
        /**
         * Adds a file to the main index.
         *
         * @param file  File to index.
         */
        void index(File& file);
        
        /**
         * Removes a file from the main index.
         *
         * @param file  File to remove from the index.
         */
        void deindex(File& file);

    private:  
        /// The main index to all files in the file system.
        Index index_;
        
        /// Index of file types. Each entry in the index is another index of names 
        /// to file instances.
        typedef std::map<std::string, Index> TypeIndex;
        TypeIndex typeIndex_;

        /// The root folder of the entire file system.
        Folder root_;
    };
}

#endif /* LIBDENG2_FS_H */
