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

#include "de/bytesubarray.h"

using namespace de;

ByteSubArray::ByteSubArray(IByteArray& mainArray, Offset at, duint size)
    : mainArray_(&mainArray), constMainArray_(&mainArray), at_(at), size_(size)
{}

ByteSubArray::ByteSubArray(const IByteArray& mainArray, Offset at, duint size)
    : mainArray_(0), constMainArray_(&mainArray), at_(at), size_(size)
{}

duint ByteSubArray::size() const
{
    return size_;
}

void ByteSubArray::get(Offset at, Byte* values, duint count) const
{
    constMainArray_->get(at_ + at, values, count);
}

void ByteSubArray::set(Offset at, const Byte* values, duint count)
{
    if(!mainArray_)
    {
        throw NonModifiableError("ByteSubArray::set", "Array is non-modifiable.");
    }
    
    mainArray_->set(at_ + at, values, count);
}
