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

#include "de/WAD"
#include "de/ByteSubArray"
#include "de/FixedByteArray"
#include "de/Reader"
#include "de/LittleEndianByteOrder"
#include "de/Block"
#include "de/File"
#include "de/Date"
#include "de/Zeroed"

#include <cstring>

using namespace de;

WAD::WAD() : _source(0)
{}

WAD::WAD(const IByteArray& wad) : _source(&wad)
{
    Reader reader(wad, littleEndianByteOrder);

    // Move past the identification signature and read the total number of lumps
    // plus the lump directory offset.
    reader.setOffset(4);
    duint16 numLumps, lumpDirectoryOffset;
    reader >> numLumps;
    reader >> lumpDirectoryOffset;

    // Attempt to locate the lump directory.
    if(!(lumpDirectoryOffset < wad.size()))
        /// @throw MissingLumpDirectoryError The WAD lump directory was not found
        /// in the source data.
        throw MissingLumpDirectoryError("WAD::WAD", 
            "Could not locate the lump directory in WAD");

    if(!(lumpDirectoryOffset + 16 * numLumps < wad.size()))
        /// @throw TruncatedLumpDirectoryError The WAD lump directory is incomplete
        /// in the source data.
        throw TruncatedLumpDirectoryError("WAD::WAD",
            "Incomplete/truncated lump directory in WAD");
        
    // Read all the entries of the lump directory.
    reader.setOffset(lumpDirectoryOffset);
    for(duint16 index = 0; index < numLumps; ++index)
    {
        duint16 lumpOffset;
        reader >> lumpOffset;

        duint16 lumpSize;
        reader >> lumpSize;

        // Make an index entry for this.
        Entry entry;
        entry.size = lumpSize;
        entry.offset = lumpOffset;

        // Add it to our index.
        de::String lumpName;
        de::FixedByteArray byteSeq(lumpName, 0, 8);
        reader >> byteSeq;

        /**
         * The Hexen demo on Mac uses the 0x80 on some lumps, maybe has
         * significance?
         * \todo: Ensure that this doesn't break other IWADs. The 0x80-0xff
         * range isn't normally used in lump names, right??
         * \todo: The DOOM64 IWAD uses the high range with the first character
         * to signify it as compressed.
         */
        for(duint i = 0; i < 8; ++i)
        {
            dbyte c;
            byteSeq.get(i, &c, 1);
            c &= 0x7f;
            byteSeq.set(i, &c, 1);
        }

        _index[lumpName] = entry;

        // Reader is now positioned at the start of the next directory entry.
    }
}

WAD::~WAD()
{
    clear();
}

void WAD::cache(bool detachFromSource)
{
    if(!_source)
    {
        // Nothing to read from.
        return;
    }    
    FOR_EACH(i, _index, Index::iterator)
    {
        Entry& entry = i->second;
        if(!entry.data)
        {
            entry.data = new Block(*_source, entry.offset, entry.size);
        }
    }
    if(detachFromSource)
    {
        _source = 0;
    }
}

bool WAD::has(const String& name) const
{
    return _index.find(name) != _index.end();
}

void WAD::listFiles(Names& names) const
{
    names.clear();

    FOR_EACH(i, _index, Index::const_iterator)
    {
        names.insert(i->first);
    }
}

File::Status WAD::status(const String& name) const
{
    Index::const_iterator found = _index.find(name);
    if(found == _index.end())
    {
        /// @throw NotFoundError  @a name was not found in the WAD.
        throw NotFoundError("WAD::fileStatus",
            "Entry '" + name + "' cannot be found in the WAD");
    }

    /// @fixme Should populate the modified time with that of the WAD file.
    File::Status info(
        File::Status::FILE,
        found->second.size);
    return info;
}

const Block& WAD::entryBlock(const String& name) const
{
    Index::const_iterator found = _index.find(name);
    if(found == _index.end())
    {
        /// @throw NotFoundError  @a name was not found in the WAD.
        throw NotFoundError("WAD::block",
            "Entry '" + name + "' cannot be found in the WAD");
    }
    
    // We'll need to modify the entry.
    Entry& entry = const_cast<Entry&>(found->second);
    if(entry.data)
    {
        // Got it.
        return *entry.data;
    }
    std::auto_ptr<Block> cached(new Block);
    read(name, *cached.get());
    entry.data = cached.release();
    return *entry.data;
}

void WAD::read(const String& name, IBlock& data) const
{
    Index::const_iterator found = _index.find(name);
    if(found == _index.end())
    {
        /// @throw NotFoundError @a name was not found in the archive.
        throw NotFoundError("WAD::readEntry",
            "Entry '" + name + "' cannot be found in the WAD");
    }
    
    const Entry& entry = found->second;
    
    if(!entry.size)
    {
        // Nothing to do.
        return;
    }
    
    // Do we have a cached copy of this entry already?
    if(entry.data)
    {
        data.copyFrom(*entry.data, 0, entry.data->size());
        return;
    }
    
    // Just read it, then.
    assert(_source != NULL);
    data.copyFrom(*_source, entry.offset, entry.size);
}

void WAD::clear()
{
    // Free cached data.
    for(Index::iterator i = _index.begin(); i != _index.end(); ++i)
    {
        delete i->second.data;
    }
    _index.clear();
}

bool WAD::recognize(const File& file)
{
    String ext = file.name().fileNameExtension().lower();
    if(ext != ".wad")
        return false;

    // Ensure the file is large enough for us to read what should be the WAD
    // header, including the identification marker/signature.
    if(file.info().hasMember(String("size")) && file.info().value<dsize>(String("size")) > 12)
    {
        // Read the identification bytes to see if this may actually be a WAD.
        Reader reader(file, littleEndianByteOrder);

        reader.setOffset(0);
        de::String identification;
        de::FixedByteArray byteSeq(identification, 0, 4);
        reader >> byteSeq;

        if(!identification.compareWithCase("JWAD") ||
           !identification.compareWithCase("IWAD") ||
           !identification.compareWithCase("PWAD"))
            return true; // It at least looks like a WAD.
    }
    return false;
}
