/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ADDRESSEDBLOCK_H
#define LIBDENG2_ADDRESSEDBLOCK_H

#include <de/Block>
#include <de/Address>

namespace de
{
    /**
     * Block with an Address.
     */
    class PUBLIC_API AddressedBlock : public Block
    {
    public:
        AddressedBlock(const Address& addr, Size initialSize = 0);
        AddressedBlock(const Address& addr, const IByteArray& other);    
        AddressedBlock(const Address& addr, const IByteArray& other, Offset at, Size count);        

        /**
         * Returns the address associated with the block.
         */
        const Address& address() const {
            return address_;
        }
        
    private:
        Address address_;
    };    
}

#endif /* LIBDENG2_ADDRESSEDBLOCK_H */
