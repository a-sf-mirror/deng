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

#include "de/Block"

using namespace de;

Block::Block(Size initialSize)
{
    data_.resize(initialSize);
}

Block::Block(const IByteArray& other)
{
    // Read the other's data directly into our data buffer.
    data_.resize(other.size());
    other.get(0, &data_[0], other.size());
}

Block::Block(const IByteArray& other, Offset at, Size count) : IByteArray()
{
    // Read the other's data directly into our data buffer.
    data_.resize(count);
    other.get(at, &data_[0], count);
}

Block::Size Block::size() const
{
    return data_.size();
}

void Block::get(Offset at, Byte* values, Size count) const
{
    if(at + count > size())
    {
        throw OffsetError("Block::get", "out of range");
    }

    for(Data::const_iterator i = data_.begin() + at; count > 0; ++i, --count)
    {
        *values++ = *i;
    }
}

void Block::set(Offset at, const Byte* values, Size count)
{
    if(at > size())
    {
        throw OffsetError("Block::set", "out of range");
    }

    data_.insert(data_.begin() + at, values, values + count);
}

const Block::Byte* Block::data() const
{
    return &data_[0];
}

void Block::clear()
{
    data_.clear();
}
