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

#ifndef LIBDENG2_READER_HH
#define LIBDENG2_READER_HH

#include <de/deng.h>
#include <de/IByteArray>

#include <string>

namespace de
{
    class ISerializable;
    
    /**   
     * Reader provides a protocol for reading network byte ordered data
     * from a byte array object (anything with a IByteArray interface).
     */
    class PUBLIC_API Reader
    {
    public:
        Reader(const IByteArray& source, IByteArray::Offset offset = 0);

        //@{ Read a number from the source buffer, in network byte order.
        Reader& operator >> (dchar& byte);
        Reader& operator >> (duchar& byte);
        Reader& operator >> (dint16& word);
        Reader& operator >> (duint16& word);
        Reader& operator >> (dint32& dword);
        Reader& operator >> (duint32& dword);
        Reader& operator >> (dint64& qword);
        Reader& operator >> (duint64& qword);
        Reader& operator >> (dfloat& value);
        Reader& operator >> (ddouble& value);
        //@}

        /// Reads a string from the source buffer.
        Reader& operator >> (std::string& text);
        
        /// Reads a sequence bytes from the source buffer. As with 
        /// strings, the length of the array is included in the data.
        Reader& operator >> (IByteArray& byteArray);
        
        /// Reads a serializable object from the source buffer.
        Reader& operator >> (ISerializable& serializable);
    
        /** 
         * Returns the source byte array of the reader.
         */
        const IByteArray& source() const { 
            return source_; 
        }

        /**
         * Returns the offset used by the reader.
         */
        IByteArray::Offset offset() const {
            return offset_;
        }
        
        void setOffset(IByteArray::Offset offset) {
            offset_ = offset;
        }
    
    private:
        const IByteArray& source_;
        IByteArray::Offset offset_;
    };
}

#endif /* LIBDENG2_READER_HH */
