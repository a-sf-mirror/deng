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

#ifndef LIBDENG2_THREAD_H
#define LIBDENG2_THREAD_H

#include <de/deng.h>

namespace de
{
    /**
     * The Thread class runs its own thread of execution.  The run() method 
     * should be overridden by subclasses to perform a background task.
     *
     * This is an abstract class because no implementation has been
     * specified for run().
     */
    class Thread
    {
    public:
        Thread();

        /// If not already stopped, the thread is forcibly killed in
        /// the destructor.
        virtual ~Thread();

        /// Start executing the thread.
        virtual void start();

        /// Signals the thread to stop.
        virtual void stop();

        /// This method is executed when the thread is started.
        virtual void run() = 0;
        
        /// @return True, if the thread should stop itself as soon as
        /// possible.
        bool shouldStopNow() const;
    
    private:
        /// This is set to true when the thread should stop.
        volatile bool stopNow_;
        
        /// Pointer to the internal thread data.
        void* thread_;
    };
}

#endif /* LIBDENG2_THREAD_H */
