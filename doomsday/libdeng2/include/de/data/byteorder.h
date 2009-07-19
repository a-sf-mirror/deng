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

#ifndef LIBDENG2_BYTEORDER_H
#define LIBDENG2_BYTEORDER_H

#include <de/deng.h>

namespace de
{
    /// Swaps the bytes in a 64-bit unsigned integer.
    duint64 swap64(const duint64& n);    
    
#ifdef __BIG_ENDIAN__ 

    /// Convert a little-endian unsigned short (16-bit) integer to native byte order.
    inline dushort littleEndianToNative(const dushort& n) {
        return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
    }

    /// Convert a little-endian unsigned long (32-bit) integer to native byte order.
    inline duint littleEndianToNative(const duint& n) {
        return (((n & 0xff) << 24) | ((n & 0xff00) << 8) |
            ((n & 0xff0000) >> 8) | ((n & 0xff000000) >> 24));
    }

    /// Convert a big-endian unsigned long long (64-bit) integer to native byte order.
    inline duint64 bigEndianToNative(const duint64& n) {
        return n;
    }

    /// Convert a native unsigned long long (64-bit) integer to big endian byte order.
    inline duint64 nativeToBigEndian(const duint64& n) {
        return n;
    }

#else // Little-endian.

    /// Convert a little-endian unsigned short (16-bit) integer to native byte order.
    inline dushort littleEndianToNative(const dushort& n) {
        return n;
    }

    /// Convert a little-endian unsigned long (32-bit) integer to native byte order.
    inline duint littleEndianToNative(const duint& n) {
        return n;
    }

    /// Convert a big-endian unsigned long long (64-bit) integer to native byte order.
    inline duint64 bigEndianToNative(const duint64& n) {
        return swap64(n);
    }

    /// Convert a native unsigned long long (64-bit) integer to big endian byte order.
    inline duint64 nativeToBigEndian(const duint64& n) {
        return swap64(n);
    }

#endif // __BIG_ENDIAN__ 

    /// Convert a little-endian signed short (16-bit) integer to native byte order.
    inline dint16 littleEndianToNative(const dint16& n) {
        return static_cast<dint16>(
            littleEndianToNative(reinterpret_cast<const duint16&>(n)));
    }

    /// Convert a little-endian signed long (32-bit) integer to native byte order.
    inline dint32 littleEndianToNative(const dint32& n) {
        return static_cast<dint32>(
            littleEndianToNative(reinterpret_cast<const duint32&>(n)));
    }

    /// Convert a big-endian signed long long (64-bit) integer to native byte order.
    inline dint64 bigEndianToNative(const dint64& n) {
        return static_cast<dint64>(
            bigEndianToNative(reinterpret_cast<const duint64&>(n)));
    }

    /// Convert a native signed long long (64-bit) integer to big endian byte order.
    inline dint64 nativeToBigEndian(const dint64& n) {
        return static_cast<dint64>(
            nativeToBigEndian(reinterpret_cast<const duint64&>(n)));
    }
}

#endif /* LIBDENG2_BYTEORDER_H */
