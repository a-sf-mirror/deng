/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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

#include "de/reader.h"
#include "de/byteorder.h"
#include "de/block.h"
#include "de/iserializable.h"

#ifdef UNIX
#   include <netinet/in.h>
#endif

#include <cstring> // for memcpy

using namespace de;

Reader::Reader(const IByteArray& source, IByteArray::Offset offset)
    : source_(source), offset_(offset)
{}

Reader& Reader::operator >> (dchar& byte)
{
    return *this >> reinterpret_cast<duchar&>(byte);
}

Reader& Reader::operator >> (duchar& byte)
{
    source_.get(offset_, &byte, 1);
    ++offset_;
    return *this;
}

Reader& Reader::operator >> (dint16& word)
{
    return *this >> reinterpret_cast<duint16&>(word);
}

Reader& Reader::operator >> (duint16& word)
{
    source_.get(offset_, reinterpret_cast<IByteArray::Byte*>(&word), 2);
    offset_ += 2;
    word = ntohs(word);
    
    return *this;
}

Reader& Reader::operator >> (dint32& dword)
{
    return *this >> reinterpret_cast<duint32&>(dword);
}

Reader& Reader::operator >> (duint32& dword)
{
    source_.get(offset_, reinterpret_cast<IByteArray::Byte*>(&dword), 4);
    offset_ += 4;
    dword = ntohl(dword);
    
    return *this;
}

Reader& Reader::operator >> (dint64& qword)
{
    return *this >> reinterpret_cast<duint64&>(qword);
}

Reader& Reader::operator >> (duint64& qword)
{
    source_.get(offset_, reinterpret_cast<IByteArray::Byte*>(&qword), 8);
    offset_ += 8;
    qword = bigEndianToNative(qword);

    return *this;
}

Reader& Reader::operator >> (dfloat& value)
{
    return *this >> *reinterpret_cast<duint32*>(&value);
}

Reader& Reader::operator >> (ddouble& value)
{
    return *this >> *reinterpret_cast<duint64*>(&value);
}

Reader& Reader::operator >> (std::string& text)
{
    duint size = 0;
    *this >> size;

    text.clear();
    text.reserve(size);
    for(unsigned i = 0; i < size; ++i)
    {
        IByteArray::Byte ch = 0;
        *this >> ch;
        text.append(1, ch);
    }
    
    return *this;
}

Reader& Reader::operator >> (IByteArray& byteArray)
{
    duint size = 0;
    *this >> size;
    
    std::auto_ptr<IByteArray::Byte> data(new IByteArray::Byte[size]);
    source_.get(offset_, data.get(), size);
    offset_ += size;
    
    byteArray.set(0, data.get(), size);
    
    return *this;
}

Reader& Reader::operator >> (ISerializable& serializable)
{
    duint size = 0;
    *this >> size;
    
    Block serial(size);
    source_.get(offset_, const_cast<IByteArray::Byte*>(serial.data()), size);
    offset_ += size;
    
    serializable << serial;
    return *this;
}
