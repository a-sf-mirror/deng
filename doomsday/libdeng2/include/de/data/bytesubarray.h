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

#ifndef LIBDENG2_BYTESUBARRAY_H
#define LIBDENG2_BYTESUBARRAY_H

#include "../IByteArray"

namespace de
{
    /**
     * Accesses a portion of a IByteArray object.
     *
     * @ingroup data
     */
    class LIBDENG2_API ByteSubArray : public IByteArray
    {
    public:
        /// set() is attempted on a nonmodifiable array. @ingroup errors
        DEFINE_ERROR(NonModifiableError);
        
    public:
        /**
         * Constructs a modifiable sub-array which refers to the @a mainArray.
         */
        ByteSubArray(IByteArray& mainArray, Offset at, Size size);
        
        /**
         * Constructs a non-modifiable sub-array which refers to the @a mainArray.
         */
        ByteSubArray(const IByteArray& mainArray, Offset at, Size size);
        
        virtual ~ByteSubArray() {}

        Size size() const;
        void get(Offset at, Byte* values, Size count) const;
        void set(Offset at, const Byte* values, Size count);
        
    private:
        IByteArray* mainArray_;
        const IByteArray* constMainArray_;
        Offset at_;
        Size size_;
    };
}

#endif /* LIBDENG2_BYTESUBARRAY_H */
