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

#ifndef LIBDENG2_WRITER_H
#define LIBDENG2_WRITER_H

#include "../deng.h"
#include "../IByteArray"
#include "../ByteOrder"

namespace de
{
    class IWritable;
    class String;
    class Block;
    class FixedByteArray;
    
    /**
     * Provides a protocol for writing data in a specific byte order 
     * into a byte array object (anything with a IByteArray interface).
     *
     * @ingroup data
     */
    class PUBLIC_API Writer
    {
    public:
        /**
         * Constructs a new writer.
         *
         * @param destination  Byte array to write to.
         * @param byteOrder    Byte order to use. The byte order defaults to network 
         *                     (big-endian) byte order.
         * @param offset       Offset in @a destination where to start writing.
         */
        Writer(IByteArray& destination, const ByteOrder& byteOrder = bigEndianByteOrder, 
            IByteArray::Offset offset = 0);
            
        /**
         * Constructs a new writer that uses the current offset of @a other as its 
         * zero offset.
         *
         * @param other      Writer.
         * @param byteOrder  Byte order. Defaults to big-endian.
         */
        Writer(const Writer& other, const ByteOrder& byteOrder = bigEndianByteOrder);

        //@{ Write a number to the destination buffer, in the chosen byte order.
        Writer& operator << (const dchar& byte);
        Writer& operator << (const duchar& byte);
        Writer& operator << (const dint16& word);
        Writer& operator << (const duint16& word);
        Writer& operator << (const dint32& dword);
        Writer& operator << (const duint32& dword);
        Writer& operator << (const dint64& qword);
        Writer& operator << (const duint64& qword);
        Writer& operator << (const dfloat& value);
        Writer& operator << (const ddouble& value);
        //@}

        /// Write a string to the destination buffer.
        Writer& operator << (const String& text);
        
        /// Writes a sequence bytes to the destination buffer.
        Writer& operator << (const IByteArray& byteArray);
    
        /**
         * Writes a fixed-size sequence of bytes to the destination buffer.
         * The size of the sequence is not included in the written data.
         * When reading, the reader must know the size beforehand.
         * @see Reader::operator >> (FixedByteArray& fixedByteArray)
         *
         * @param fixedByteArray  Data to write.
         *
         * @return  Reference to the Writer.
         */
        Writer& operator << (const FixedByteArray& fixedByteArray);
        
        /// Writes the Block @a block into the destination buffer.
        Writer& operator << (const Block& block);
        
        /// Writes a writable object into the destination buffer.
        Writer& operator << (const IWritable& writable);

        /**
         * Returns the destination byte array used by the writer.
         */
        const IByteArray& destination() const {
            return destination_;
        }

        /**
         * Returns the destination byte array used by the writer.
         */
        IByteArray& destination() {
            return destination_;
        }

        /**
         * Returns the offset used by the writer.
         */
        IByteArray::Offset offset() const {
            return offset_;
        }
        
        void setOffset(IByteArray::Offset offset) {
            offset_ = offset;
        }

        /**
         * Returns the byte order of the writer.
         */
        const ByteOrder& byteOrder() const {
            return convert_;
        }

        /**
         * Moves the writer offset forward by a number of bytes.
         *
         * @param count  Number of bytes to move forward.
         */
        void seek(dint count);
        
    private:
        IByteArray& destination_;
        IByteArray::Offset offset_;
        const IByteArray::Offset fixedOffset_;
        const ByteOrder& convert_;
    };
}

#endif /* LIBDENG2_WRITER_H */
