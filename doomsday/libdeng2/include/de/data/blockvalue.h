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

#ifndef LIBDENG2_BLOCKVALUE_H
#define LIBDENG2_BLOCKVALUE_H

#include "../Value"
#include "../Block"

namespace de
{
    /**
     * The BlockValue class is a subclass of Value that holds a data block.
     *
     * @ingroup data
     */
    class PUBLIC_API BlockValue : public Value
    {
    public:
        BlockValue();
        
        /// Copies the content of an existing block.
        BlockValue(const Block& block);

        /// Converts the BlockValue to a plain byte array (non-modifiable).
        operator const IByteArray& () const;

        /// Converts the BlockValue to a plain byte array.
        operator IByteArray& ();

        /// Empties the block value.
        void clear();

        Value* duplicate() const;
        Text asText() const;
        dsize size() const;
        bool isTrue() const;
        void sum(const Value& value);
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    private:
        Block value_;
    };
}

#endif /* LIBDENG2_BLOCKVALUE_H */
