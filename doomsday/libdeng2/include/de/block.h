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

#ifndef LIBDENG2_BLOCK_H
#define LIBDENG2_BLOCK_H

#include <de/IByteArray>

#include <vector>

namespace de
{
    /**
     * Block is a simple data buffer that implements the byte array interface.
     *
     * @ingroup data
     */
    class PUBLIC_API Block : public IByteArray
    {
    public:
        Block(Size initialSize = 0);
        Block(const IByteArray& other);
        
        /**
         * Construct a new block and copy its contents from the specified
         * location at another array.
         *
         * @param other  Source data.
         * @param at     Offset without source data.
         * @param count  Number of bytes to copy. Also the size of the new block.
         */
        Block(const IByteArray& other, Offset at, Size count);
        
        Size size() const;
        void get(Offset at, Byte* values, Size count) const;
        void set(Offset at, const Byte* values, Size count);

        /**
         * Gives const access to the data directly.
         *
         * @return Pointer to the beginning of the image data.
         */
        const Byte* data() const;
        
        /// Empties the data of the block.         
        void clear();
        
    private:
        typedef std::vector<Byte> Data;
        Data data_;
    };
}
    
#endif /* LIBDENG2_BLOCK_H */
