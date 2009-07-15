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

#include <de/deng.h>
#include <de/IByteArray>

namespace de
{
    class ISerializable;
    class String;
    
    /**
     * Writer provides a protocol for writing network byte ordered data
     * into a byte array object (anything with a IByteArray interface).
     */
    class PUBLIC_API Writer
    {
    public:
        Writer(IByteArray& destination, IByteArray::Offset offset = 0);

        //@{ Write a number to the destination buffer, in network byte order.
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
        
        /// Writes a sequence bytes to the destination buffer. As with 
        /// strings, the length of the array is included in the data.
        Writer& operator << (const IByteArray& byteArray);
        
        /// Writes a serializable object into the destination buffer.
        Writer& operator << (const ISerializable& serializable);

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
        
    private:
        IByteArray& destination_;
        IByteArray::Offset offset_;
    };
}

#endif /* LIBDENG2_WRITER_H */
