/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_LOCKABLE_H
#define LIBDENG2_LOCKABLE_H

#include <de/deng.h>

namespace de
{
    /**
     * Lockable implements a mutex that can be used to synchronize access
     * to a resource.  All classes of lockable resources should be derived
     * from Lockable.
     */
    class PUBLIC_API Lockable
    {
    public:
        Lockable();
        virtual ~Lockable();

        /// Acquire the lock.  Blocks until the operation succeeds.
        void lock();

        /// Release the lock.
        void unlock();

        /// Returns true, if the lock is currently locked.
        bool isLocked() const;
        
        /// Asserts that the lock is currently locked.  Should be used by
        /// subclasses to assert that a lock has been acquired when it
        /// must be.
        void assertLocked() const;
        
    private:
        /// Pointer to the internal mutex data.
        void* mutex_;
        
        bool isLocked_;
    };
}

#endif /* LIBDENG2_LOCKABLE_H */
