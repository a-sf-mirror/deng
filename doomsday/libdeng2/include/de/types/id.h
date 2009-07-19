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

#ifndef LIBDENG2_ID_H
#define LIBDENG2_ID_H

#include <de/deng.h>

namespace de
{
    class String;
    
    /**
     * Unique identifier number. Zero is not a valid identifier, as it reserved
     * for the "no identifier" special case.
     */
    class Id
    {
    public:
        typedef duint Type;

        /// The special "no identifier".
        static const Type NONE;
        
    public:
        Id();
        ~Id();

        operator Type () const { return id_; }

        /// Converts the Id to a text string.
        operator std::string () const;
        
        /// Converts the Id to a text string.
        String asText() const;
        
    private:
        Type id_;
        static Type generator_;
    };
    
    std::ostream& operator << (std::ostream& os, const Id& id);
}

#endif /* LIBDENG2_ID_H */
