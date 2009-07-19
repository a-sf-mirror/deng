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

#ifndef LIBDENG2_USER_H
#define LIBDENG2_USER_H

#include <de/deng.h>

namespace de
{
    /**
     * A player in a game world. The game plugin is responsible for creating
     * concrete User instances. The game plugin can extend the User class with any
     * extra information it needs.
     *
     * @ingroup world
     */ 
    class User
    {
    public:
        /// Identifier for the user.
        typedef duint Id;
        
    public:
        User();
        
        virtual ~User();
                
    private:
    };
};

#endif /* LIBDENG2_USER_H */
