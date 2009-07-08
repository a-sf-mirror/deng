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
 
#ifndef LIBDENG2_ISERIALIZABLE_H
#define LIBDENG2_ISERIALIZABLE_H

#include <de/deng.h>

namespace de
{
    class Writer;
    class Reader;
    
    /**
     * ISerializable is an interface that classes can implement if they
     * want to support operations where serialization is needed.
     * Serialization means that an object is converted into a byte array
     * so that all the relevant information about the object is included.
     * The original object can then be restored by deserializing the byte
     * array.
     */
    class PUBLIC_API ISerializable
    {
    public:
        virtual ~ISerializable() {}

        /**
         * Serialize the object using the provided Writer.
         *
         * @param to  Writer using which the data is written.
         *
         * @return  Reference to the Writer @c to.
         */
        virtual Writer& operator >> (Writer& to) const = 0;

        /**
         * Restore the object from the provided Reader.
         *
         * @param from  Reader where the data is read from.
         *
         * @return  Reference to the Reader @c from.
         */
        virtual Reader& operator << (Reader& from) = 0;
    };
}

#endif /* LIBDENG2_ISERIALIZABLE_H */
