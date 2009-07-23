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

#ifndef LIBDENG2_ENUMERATOR_H
#define LIBDENG2_ENUMERATOR_H

#include <de/deng.h>

namespace de
{
    /**
     * Provides unique 32-bit unsigned integer numbers.
     *
     * @see Id
     *
     * @ingroup types
     */
    class PUBLIC_API Enumerator
    {
    public:
        typedef duint32 Type;
        
        enum { 
            /// Zero is reserved as a special case.
            NONE = 0 
        };
        
    public:
        Enumerator();
        
        /**
         * Returns the next unique 32-bit unsigned integer. Never returns zero.
         */
        Type get();
        
    private:
        Type current_;
    };
}

#endif /* LIBDENG2_ENUMERATOR_H */
