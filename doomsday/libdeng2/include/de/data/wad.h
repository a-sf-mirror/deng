/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
 * Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifndef LIBDENG2_WAD_H
#define LIBDENG2_WAD_H

#include "../deng.h"
#include "../IByteArray"
#include "../IWritable"
#include "../String"
#include "../Time"
#include "../File"

#include <vector>
#include <set>
#include <map>

namespace de
{
    class IBlock;
    class Block;
    
    /**
     * Provides read access to data inside a WAD.
     * 
     * @see WADFeed, WADFile
     *
     * @ingroup data
     */
    class LIBDENG2_API WAD
    {
    public:
        /// Base class for format-related errors. @ingroup errors
        DEFINE_ERROR(FormatError);
        
        /// The lump directory of the WAD cannot be located. Maybe it's not 
        /// a WAD after all? @ingroup errors
        DEFINE_SUB_ERROR(FormatError, MissingLumpDirectoryError);
        
        /// The lump directory of the WAD is incomplete/truncated. Maybe it's not
        /// a WAD after all? @ingroup errors
        DEFINE_SUB_ERROR(FormatError, TruncatedLumpDirectoryError);

        /// The requested entry does not exist in the WAD. @ingroup errors
        DEFINE_ERROR(NotFoundError);
                
        typedef std::set<String> Names;
        
    public:
        /**
         * Constructs an empty WAD.
         */
        WAD();
        
        /**
         * Constructs a new WAD instance. The lump directory index contained in
         * @a WAD is read during construction.
         *
         * @param data  Data of the source WAD. No copy of the
         *              data is made, so the caller must make sure the
         *              byte array remains in existence for the lifetime
         *              of the WAD instance.
         */
        WAD(const IByteArray& data);
        
        virtual ~WAD();

        /**
         * Loads a copy of the data into memory for all the entries that don't
         * already have cached data stored. 
         *
         * @param detachFromSource  If @c true, the WAD becomes a standalone 
         *                          WAD that no longer needs the source byte
         *                          array to remain in existence.
         */
        void cache(bool detachFromSource = true);

        /**
         * Determines whether the WAD contains an entry.
         *
         * @param name  Name of the entry.
         *
         * @return  @c true or @c false.
         */
        bool has(const String& name) const;

        /**
         * List the files in the WAD.
         *
         * @param names   Entry names collected to this set.
         */
        void listFiles(Names& names) const;

        /**
         * Returns information about the specified name. 
         *
         * @param name  Name of the entry within the WAD.
         *
         * @return Type, size, and other metadata about the entry.
         */
        File::Status status(const String& name) const;

        /**
         * Returns the data of an entry for read-only access. 
         * The data is cached if a cached copy doesn't already exist. 
         *
         * @param name  Entry name.
         */
        const Block& entryBlock(const String& name) const;

        /**
         * Reads an entry from the WAD. The data for the entry is kept cached
         * after the call.
         *
         * @param name  Name of the entry within the WAD.
         * @param data  Data is written here.
         */
        void read(const String& name, IBlock& data) const;
        
        /**
         * Clears the index of the WAD. All entries are deleted.
         */
        void clear();
        
    public:
        /**
         * Determines whether a File looks like it could be accessed using WAD.
         *
         * @param file  File to check.
         *
         * @return @c true, if the file looks like a WAD.
         */
        static bool recognize(const File& file);

    private:
        struct Entry {
            dsize offset;           ///< Offset from the start of the WAD file.
            dsize size;             ///< Size within the WAD.
            mutable Block* data;    ///< Cached copy of the data. Can be @c NULL.
            
            Entry() : offset(0), size(0), data(0) {}
        };
        typedef std::map<String, Entry> Index;

    private:
        /// Source data provided at construction.
        const IByteArray* _source;
        
        /// Index maps entry paths to their metadata.
        Index _index;
    };
}

#endif /* LIBDENG2_WAD_H */
