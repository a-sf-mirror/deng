/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Archive"
#include "de/ISerializable"
#include "de/ByteSubArray"
#include "de/FixedByteArray"
#include "de/Reader"
#include "de/Writer"
#include "de/LittleEndianByteOrder"
#include "de/Block"
#include "de/File"
#include "de/Date"
#include "de/Zeroed"

#include <cstring>
#include <zlib.h>

using namespace de;

// Marker signatures.
#define SIG_LOCAL_FILE_HEADER   0x04034b50
#define SIG_CENTRAL_FILE_HEADER 0x02014b50
#define SIG_END_OF_CENTRAL_DIR  0x06054b50
#define SIG_DIGITAL_SIGNATURE   0x05054b50

// Maximum tolerated size of the comment.
#define MAXIMUM_COMMENT_SIZE    2048

// This is the length of the central directory end record (without the
// comment, but with the signature).
#define CENTRAL_END_SIZE        22

// Deflate minimum compression. Worse than this will be stored uncompressed.
#define REQUIRED_DEFLATE_PERCENTAGE .98            

// File header flags.
#define ZFH_ENCRYPTED           0x1
#define ZFH_COMPRESSION_OPTS    0x6
#define ZFH_DESCRIPTOR          0x8
#define ZFH_COMPRESS_PATCHED    0x20    // Not supported.

// Compression methods.
enum Compression {
    NO_COMPRESSION = 0,     // Supported format.
    SHRUNK,
    REDUCED_1,
    REDUCED_2,
    REDUCED_3,
    REDUCED_4,
    IMPLODED,
    DEFLATED = 8,           // The only supported compression (via zlib).
    DEFLATED_64,
    PKWARE_DCL_IMPLODED
};

/**
 * MS-DOS time.
 * - 0..4:   Second/2
 * - 5..10:  Minute 
 * - 11..15: Hour
 */
struct DOSTime {
    duint16 seconds;
    duint16 minutes;
    duint16 hours;
    
    DOSTime(duint16 h, duint16 m, duint16 s) : seconds(s), minutes(m), hours(h) {}
    DOSTime(const duint16& i) {
        seconds = (i & 0x1f) * 2;
        minutes = (i >> 5) & 0x3f;
        hours = i >> 11;
    }
    operator duint16() const {
        return (hours << 11) | ((minutes & 0x3f) << 5) | ((seconds/2) & 0x1f);
    }
};

/**
 * MS-DOS date.
 * - 0..4:  Day of the month (1-31)
 * - 5..8:  Month (1=Jan)
 * - 9..15: Year since 1980
 */
struct DOSDate {
    duint16 dayOfMonth;
    duint16 month;
    duint16 year;

    DOSDate(duint16 y, duint16 m, duint16 d) : dayOfMonth(d), month(m), year(y) {}
    DOSDate(const duint16& i) {
        dayOfMonth = i & 0x1f;
        month = (i >> 5) & 0xf;
        year = i >> 9;
    }
    operator duint16() const {
        return (year << 9) | ((month & 0xf) << 5) | (dayOfMonth & 0x1f);
    }
};

struct LocalFileHeader : public ISerializable {
    Uint32 signature;
    Uint16 requiredVersion;
    Uint16 flags;
    Uint16 compression;
    Uint16 lastModTime;
    Uint16 lastModDate;
    Uint32 crc32;
    Uint32 compressedSize;
    Uint32 size;
    Uint16 fileNameSize;
    Uint16 extraFieldSize;
    
    void operator >> (Writer& to) const {
        to  << signature
            << requiredVersion
            << flags
            << compression
            << lastModTime
            << lastModDate
            << crc32
            << compressedSize
            << size
            << fileNameSize
            << extraFieldSize;
    }
    void operator << (Reader& from) {
        from >> signature
             >> requiredVersion
             >> flags
             >> compression
             >> lastModTime
             >> lastModDate
             >> crc32
             >> compressedSize
             >> size
             >> fileNameSize
             >> extraFieldSize;
    }
};

struct CentralFileHeader : public ISerializable {
    Uint32 signature;
    Uint16 version;
    Uint16 requiredVersion;
    Uint16 flags;
    Uint16 compression;
    Uint16 lastModTime;
    Uint16 lastModDate;
    Uint32 crc32;
    Uint32 compressedSize;
    Uint32 size;
    Uint16 fileNameSize;
    Uint16 extraFieldSize;
    Uint16 commentSize;
    Uint16 diskStart;
    Uint16 internalAttrib;
    Uint32 externalAttrib;
    Uint32 relOffset;

    /*
     * Followed by:
     * file name (variable size)
     * extra field (variable size)
     * file comment (variable size)
     */

    void operator >> (Writer& to) const {
        to  << signature
            << version
            << requiredVersion
            << flags
            << compression
            << lastModTime
            << lastModDate
            << crc32
            << compressedSize
            << size
            << fileNameSize
            << extraFieldSize
            << commentSize
            << diskStart
            << internalAttrib
            << externalAttrib
            << relOffset;
    }
    void operator << (Reader& from) {
        from >> signature
             >> version
             >> requiredVersion
             >> flags
             >> compression
             >> lastModTime
             >> lastModDate
             >> crc32
             >> compressedSize
             >> size
             >> fileNameSize
             >> extraFieldSize
             >> commentSize
             >> diskStart
             >> internalAttrib
             >> externalAttrib
             >> relOffset;        
    }
};

struct CentralEnd : public ISerializable {
    Uint16 disk;
    Uint16 centralStartDisk;
    Uint16 diskEntryCount;
    Uint16 totalEntryCount;
    Uint32 size;
    Uint32 offset;
    Uint16 commentSize;
    
    void operator >> (Writer& to) const {
        to  << disk 
            << centralStartDisk
            << diskEntryCount
            << totalEntryCount
            << size
            << offset
            << commentSize;
    }
    void operator << (Reader& from) {
        from >> disk 
             >> centralStartDisk
             >> diskEntryCount
             >> totalEntryCount
             >> size
             >> offset
             >> commentSize;
    }
};

Archive::Archive() : source_(0)
{}

Archive::Archive(const IByteArray& archive) : source_(&archive)
{
    Reader reader(archive, littleEndianByteOrder);
    
    // Locate the central directory. Start from the earliest location where 
    // the signature might be.
    duint centralEndPos = 0;
    for(duint pos = CENTRAL_END_SIZE; pos < MAXIMUM_COMMENT_SIZE; pos++)
    {
        reader.setOffset(archive.size() - pos);
        duint32 signature;
        reader >> signature;
        if(signature == SIG_END_OF_CENTRAL_DIR)
        {
            // This is it!
            centralEndPos = archive.size() - pos;
            break;
        }
    }
    if(!centralEndPos)
    {
        /// @throw MissingCentralDirectoryError The ZIP central directory was not found
        /// in the end of the source data.
        throw MissingCentralDirectoryError("Archive::Archive", 
            "Could not locate the central directory of the archive");
    }
    
    // The central directory end record is at the signature we found.
    CentralEnd summary;
    reader >> summary;
    
    const duint entryCount = summary.totalEntryCount;
    
    // The ZIP must have only one part, all entries in the same archive.
    if(entryCount != summary.diskEntryCount)
    {
        /// @throw MultiPartError  ZIP archives in more than one part are not supported
        /// by the implementation.
        throw MultiPartError("Archive::Archive", "Multipart archives are not supported");
    }
    
    // Read all the entries of the central directory.
    reader.setOffset(summary.offset);
    for(duint index = 0; index < entryCount; ++index)
    {
        CentralFileHeader header;
        reader >> header;
        
        // Check the signature.
        if(header.signature != SIG_CENTRAL_FILE_HEADER)
        {
            /// @throw FormatError  Invalid signature in a central directory entry.
            throw FormatError("Archive::Archive", "Corrupt central directory");
        }
            
        String fileName(ByteSubArray(archive, reader.offset(), header.fileNameSize));
        
        // Advance the cursor past the variable sized fields.
        reader.seek(header.fileNameSize + header.extraFieldSize + header.commentSize);

        // Skip folders.
        if(fileName.endsWith("/") && !header.size)
        {
            continue;
        }
        
        // Check for unsupported features.
        if(header.compression != NO_COMPRESSION && header.compression != DEFLATED)
        {
            /// @throw UnknownCompressionError  Deflation is the only compression 
            /// algorithm supported by the implementation.
            throw UnknownCompressionError("Archive::Archive",
                "Entry '" + fileName + "' uses an unsupported compression algorithm");
        }
        if(header.flags & ZFH_ENCRYPTED)
        {
            /// @throw EncryptionError  Archive is encrypted, which is not supported
            /// by the implementation.
            throw EncryptionError("Archive::Archive",
                "Entry '" + fileName + "' is encrypted and thus cannot be read");
        }
        
        // Make an index entry for this.
        Entry entry;
        entry.size = header.size;
        entry.sizeInArchive = header.compressedSize;
        entry.compression = header.compression;
        entry.crc32 = header.crc32;
        entry.localHeaderOffset = header.relOffset;
        
        // Unpack the last modified time from the ZIP entry header.
        DOSDate lastModDate(header.lastModDate);
        DOSTime lastModTime(header.lastModTime);
        Date modifiedAt;
        modifiedAt.seconds = lastModTime.seconds;
        modifiedAt.minutes = lastModTime.minutes;
        modifiedAt.hours = lastModTime.hours;
        modifiedAt.dayOfMonth = lastModDate.dayOfMonth;
        modifiedAt.month = lastModDate.month;
        modifiedAt.year = lastModDate.year + 1980;    
        entry.modifiedAt = modifiedAt.asTime();
        
        // Read the local file header, which contains the correct extra 
        // field size (Info-ZIP!).
        IByteArray::Offset posInCentral = reader.offset();
        
        reader.setOffset(header.relOffset);
        LocalFileHeader localHeader;
        reader >> localHeader;
        
        entry.offset = reader.offset() + header.fileNameSize + localHeader.extraFieldSize;
            
        // Add it to our index.
        index_[fileName] = entry;

        // Back to the central directory.
        reader.setOffset(posInCentral);
        
        // Collect subdirectories.
        /*
        std::string iter = fileName;
        while(true)
        {
            std::string entryDir = String::fileName(String::fileNamePath(iter));
            if(entryDir.empty())
            {
                break;
            }
            std::string parentDir = String::fileNamePath(String::fileNamePath(iter));
            iter = String::fileNamePath(iter);
            subDirs_[parentDir].insert(entryDir);
        }
        */
    }
}

Archive::~Archive()
{
    clear();
}

bool Archive::has(const String& path) const
{
    return index_.find(path) != index_.end();
}

File::Status Archive::status(const String& path) const
{
    Index::const_iterator found = index_.find(path);
    if(found == index_.end())
    {
        /// @throw NotFoundError  @a path was not found in the archive.
        throw NotFoundError("Archive::fileStatus",
            "Entry '" + path + "' cannot not found in the archive");
    }

    File::Status info(
        // Is it a folder?
        (!found->second.size && path.endsWith("/"))? File::Status::FOLDER : File::Status::FILE,
        found->second.size,
        found->second.modifiedAt);
    return info;
}

void Archive::read(const std::string& path, IBlock& uncompressedData) const
{
    Index::const_iterator found = index_.find(path);
    if(found == index_.end())
    {
        /// @throw NotFoundError @a path was not found in the archive.
        throw NotFoundError("Archive::readEntry",
            "Entry '" + path + "' cannot not found in the archive");
    }
    
    const Entry& entry = found->second;
    
    if(!entry.size)
    {
        // Nothing to do.
        return;
    }
    
    // Do we have a decompressed copy of this entry already?
    if(entry.data)
    {
        uncompressedData.copyFrom(*entry.data, 0, entry.data->size());
        return;
    }

    // Must have a source archive for reading.
    assert(source_ != NULL);
    
    if(entry.compression == NO_COMPRESSION)
    {
        // Just read it, then.
        uncompressedData.copyFrom(*source_, entry.offset, entry.size);
    }
    else
    {
        // Prepare the output buffer for the decompressed data.
        uncompressedData.resize(entry.size);

        // Take a copy of the compressed data for zlib.
        Block compressedData(*source_, entry.offset, entry.sizeInArchive);

        z_stream stream;
        std::memset(&stream, 0, sizeof(stream));
        stream.next_in = const_cast<IByteArray::Byte*>(compressedData.data());
        stream.avail_in = entry.sizeInArchive;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.next_out = const_cast<IByteArray::Byte*>(uncompressedData.data());
        stream.avail_out = entry.size;

        if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        {
            /// @throw InflateError Problem with zlib: inflateInit2 failed.
            throw InflateError("Archive::readEntry",
                "Inflation failed because initialization failed");
        }

        // Do the inflation in one call.
        dint result = inflate(&stream, Z_FINISH);

        if(stream.total_out != entry.size)
        {
            /// @throw InflateError The actual decompressed size is not equal to the
            /// size listed in the central directory.
            throw InflateError("Archive::readEntry", 
                "Failure due to " + 
                std::string((result == Z_DATA_ERROR ? "corrupt data in archive" :
                "zlib error")) + ": " + stream.msg);
        }

        // We're done.
        inflateEnd(&stream);
    }
}

void Archive::add(const String& path, const IByteArray& data)
{
    // Get rid of the earlier entry with this path.
    if(has(path))
    {
        remove(path);
    }
    Entry entry;
    entry.size = data.size();
    entry.modifiedAt = Time();
    entry.data = new Block(data);
    entry.compression = NO_COMPRESSION; // Will be updated.
    entry.crc32 = crc32(0L, Z_NULL, 0);
    entry.crc32 = crc32(entry.crc32, entry.data->data(), entry.data->size());
    // The rest of the data gets updated when the archive is written.
    index_[path] = entry;
}

void Archive::remove(const String& path)
{
    Index::iterator found = index_.find(path);
    if(found != index_.end())
    {
        if(found->second.data)
        {
            delete found->second.data;
        }
        index_.erase(found);
        return;
    }
    /// @throw NotFoundError  The path does not exist in the archive.
    throw NotFoundError("Archive::remove", "Entry '" + path + "' not found");
}

void Archive::clear()
{
    // Free uncompressed data.
    for(Index::iterator i = index_.begin(); i != index_.end(); ++i)
    {
        if(i->second.data)
        {
            delete i->second.data;
        }
    }
    index_.clear();
}

void Archive::operator >> (Writer& to) const
{
    /// ZIP archives will use little-endian byte order regardless of the byte order
    /// employed by @a to.
    Writer writer(to, littleEndianByteOrder);
    
    // First write the local headers.
    for(Index::const_iterator i = index_.begin(); i != index_.end(); ++i)
    {
        Entry& entry = const_cast<Entry&>(i->second);
        
        // This is where the local file header is located.
        entry.localHeaderOffset = writer.offset();
        
        LocalFileHeader header;
        header.signature = SIG_LOCAL_FILE_HEADER;
        header.requiredVersion = 20;
        header.compression = entry.compression;
        Date at(entry.modifiedAt);
        header.lastModTime = DOSTime(at.hours, at.minutes, at.seconds);
        header.lastModDate = DOSDate(at.year - 1980, at.month, at.dayOfMonth);
        header.crc32 = entry.crc32;
        header.compressedSize = entry.sizeInArchive;
        header.size = entry.size;
        header.fileNameSize = i->first.size();

        // Can we use the data already in the source archive?
        if(source_ && !entry.data)
        {
            // Yes, we can.
            writer << header << FixedByteArray(i->first);
            IByteArray::Offset newOffset = writer.offset();
            writer << FixedByteArray(*source_, entry.offset, entry.sizeInArchive);
            
            // Written to new location.
            entry.offset = newOffset;
        }
        else 
        {
            assert(entry.data != NULL);

            // Let's try and compress.
            Block archived(REQUIRED_DEFLATE_PERCENTAGE * entry.data->size());
            
            z_stream stream;
            std::memset(&stream, 0, sizeof(stream));
            stream.next_in = const_cast<IByteArray::Byte*>(entry.data->data());
            stream.avail_in = entry.data->size();
            stream.zalloc = Z_NULL;
            stream.zfree = Z_NULL;
            stream.next_out = const_cast<IByteArray::Byte*>(archived.data());
            stream.avail_out = archived.size();
    
            if(deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            {
                /// @throw DeflateError  zlib error: could not initialize deflate operation.
                throw DeflateError("Archive::operator >>", "Deflate init failed");
            }
            
            deflate(&stream, Z_FINISH);
            int result = deflateEnd(&stream);
            if(result == Z_OK)
            {                
                // Compression was ok.
                header.compression = entry.compression = DEFLATED;
                header.compressedSize = entry.sizeInArchive = stream.total_out;
                writer << header << FixedByteArray(i->first);
                entry.offset = writer.offset();                
                writer << FixedByteArray(archived, 0, entry.sizeInArchive);
            }
            else
            {
                // We won't compress.
                header.compression = entry.compression = NO_COMPRESSION;
                header.compressedSize = entry.sizeInArchive = entry.data->size();
                writer << header << FixedByteArray(i->first);
                entry.offset = writer.offset();
                writer << FixedByteArray(*entry.data);
            }            
        }
    }
    
    CentralEnd summary;
    summary.diskEntryCount = index_.size();
    summary.totalEntryCount = index_.size();

    // This is where the central directory begins.
    summary.offset = writer.offset();
    
    // Write the central directory.
    for(Index::const_iterator i = index_.begin(); i != index_.end(); ++i)
    {
        const Entry& entry = i->second;
        
        CentralFileHeader header;
        header.signature = SIG_CENTRAL_FILE_HEADER;
        header.version = 20;
        header.requiredVersion = 20;
        Date at(entry.modifiedAt);
        header.lastModTime = DOSTime(at.hours, at.minutes, at.seconds);
        header.lastModDate = DOSDate(at.year - 1980, at.month, at.dayOfMonth);
        header.compression = entry.compression;
        header.crc32 = entry.crc32;
        header.compressedSize = entry.sizeInArchive;
        header.size = entry.size;
        header.fileNameSize = i->first.size();
        header.relOffset = entry.localHeaderOffset;
        
        writer << header << FixedByteArray(i->first);
    }    
    
    // Size of the central directory.
    summary.size = writer.offset() - summary.offset;
    
    // End of central directory.
    writer << duint32(SIG_END_OF_CENTRAL_DIR) << summary;
    
    // No signature data.
    writer << duint32(SIG_DIGITAL_SIGNATURE) << duint16(0);
    
    // Since we used our own writer, seek the writer that was given to us 
    // by the amount of data we wrote.
    to.seek(writer.offset());
}

bool Archive::recognize(const File& file)
{
    // For now, just check the name.
    String ext = String::fileNameExtension(file.name()).lower();
    return (ext == ".pack" || ext == ".demo" || ext == ".save" || ext == ".addon" ||
            ext == ".box" || ext == ".pk3" || ext == ".zip");
}
