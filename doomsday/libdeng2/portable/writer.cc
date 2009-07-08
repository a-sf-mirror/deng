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

#include "de/writer.h"
#include "de/byteorder.h"
#include "de/block.h"
#include "de/iserializable.h"

#ifdef UNIX
#   include <netinet/in.h>
#endif

#include <cstring> // for memcpy

using namespace de;

Writer::Writer(IByteArray& destination, IByteArray::Offset offset)
    : destination_(destination), offset_(offset)
{}

Writer& Writer::operator << (const dchar& byte)
{
    destination_.set(offset_,
                     reinterpret_cast<const IByteArray::Byte*>(&byte), 1);
    ++offset_;
    return *this;
}

Writer& Writer::operator << (const duchar& byte)
{
    destination_.set(offset_, &byte, 1);
    ++offset_;
    return *this;
}

Writer& Writer::operator << (const dint16& word)
{
    return *this << static_cast<duint16>(word);
}   

Writer& Writer::operator << (const duint16& word)
{
    duint16 netWord = htons(word);  
    destination_.set(offset_, reinterpret_cast<IByteArray::Byte*>(&netWord), 2);
    offset_ += 2;
    return *this;
}

Writer& Writer::operator << (const dint32& dword)
{
    return *this << static_cast<duint32>(dword);
}   

Writer& Writer::operator << (const duint32& dword)
{
    duint32 netDword = htonl(dword);
    destination_.set(offset_, reinterpret_cast<IByteArray::Byte*>(&netDword), 4);
    offset_ += 4;
    return *this;
}

Writer& Writer::operator << (const dint64& qword)
{
    return *this << static_cast<duint64>(qword);
}   

Writer& Writer::operator << (const duint64& qword)
{
    duint64 netQword = nativeToBigEndian(qword);
    destination_.set(offset_, reinterpret_cast<IByteArray::Byte*>(&netQword), 8);
    offset_ += 8;
    return *this;
}

Writer& Writer::operator << (const dfloat& value)
{
    return *this << *reinterpret_cast<const duint32*>(&value);
}

Writer& Writer::operator << (const ddouble& value)
{
    return *this << *reinterpret_cast<const duint64*>(&value);
}

Writer& Writer::operator << (const std::string& text)
{
    // First write the length of the text.
    duint size = text.length();
    *this << size;

    destination_.set(offset_,
                     reinterpret_cast<const IByteArray::Byte*>(text.c_str()),
                     size);
    offset_ += size;
    return *this;
}

Writer& Writer::operator << (const IByteArray& byteArray)
{
    // First write the length of the array.
    duint size = byteArray.size();
    *this << size;

    // Read the entire contents of the array.
    std::auto_ptr<IByteArray::Byte> data(new IByteArray::Byte[size]);
    byteArray.get(0, data.get(), size);
    
    destination_.set(offset_, data.get(), size);
    offset_ += size;
    return *this;
}

Writer& Writer::operator << (const ISerializable& serializable)
{
    serializable >> *this;
    return *this;
}
