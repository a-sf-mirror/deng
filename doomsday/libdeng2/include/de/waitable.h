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

#ifndef LIBDENG2_WAITABLE_H
#define LIBDENG2_WAITABLE_H

#include <de/deng.h>

namespace de
{
    class Time;

    /**
     * Waitable implements a semaphore, which allows creating resources
     * that can be waited on.
     */
    class PUBLIC_API Waitable
    {
    public:
        /// wait() failed due to timing out before the resource is secured. @ingroup errors
        DEFINE_ERROR(TimeOutError);
        
        /// wait() or waitTime() failed to secure the resource. @ingroup errors
        DEFINE_ERROR(WaitError);
    
    public:
        Waitable(duint initialValue = 0);
        virtual ~Waitable();

        /// Wait until the resource becomes available.
        void wait();

        /// Wait for the specified period of time to secure the
        /// resource.  If timeout occurs, an exception is thrown.
        void wait(const Time& timeOut);
        
        /// Mark the resource as available by incrementing the
        /// semaphore value.
        void post();
        
    private:
        /// Pointer to the internal semaphore data.
        void* semaphore_;
    };
}

#endif /* LIBDENG2_WAITABLE_H */
