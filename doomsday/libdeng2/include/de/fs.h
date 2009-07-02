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
#include <de/CompoundFolder>

#include <map>

namespace de
{
    /**
     * The file system maintains a tree of files and folders. It provides a way
     * to quickly and efficiently locate files anywhere in the tree. It also
     * maintains semantic information about the structure and content of the 
     * file tree, allowing others to know how to treat the files and folders.
     */
    class PUBLIC_API FS
    {
    public:
        FS();
        
        virtual ~FS();

        void printIndex();
        
        void index(File& file);
        
        void deindex(File& file);
        
    private:  
        /// The root folder of the entire file system.
        CompoundFolder root_;
        
        /// The main index to all files in the file system.
        typedef std::multimap<std::string, File*> Index;
        typedef std::pair<std::string, File*> IndexEntry;
        Index index_;
    };
}

#endif /* LIBDENG2_FS_H */
