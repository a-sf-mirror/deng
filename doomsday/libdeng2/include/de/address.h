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

#ifndef LIBDENG2_ADDRESS_H
#define LIBDENG2_ADDRESS_H

#include <de/deng.h>

namespace de
{
    /**
     * IP address.
     *
     * @ingroup net 
     */
    class PUBLIC_API Address
    {
    public:
        /// The address cannot be resolved successfully. @ingroup errors
        DEFINE_ERROR(ResolveError);
    
    public:
        Address();
        Address(const char* address, duint16 port = 0);

        /**
         * Resolve the given address.  If the address string contains a
         * port (after a colon), it will always be used instead of
         * the @c port parameter.  When creating an address for
         * listening, set @c address to NULL.
         */
        void set(const char* address, duint16 port);

        duint32 ip() const { return ip_; }
        duint16 port() const { return port_; }

    private:
        duint32 ip_;
        duint16 port_;
    };
}

#endif /* LIBDENG2_ADDRESS_H */
