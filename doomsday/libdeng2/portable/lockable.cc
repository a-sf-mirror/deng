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

#include "de/lockable.h"
#include "de/error.h"
#include "sdl.h"

using namespace de;

Lockable::Lockable() : isLocked_(false)
{
    mutex_ = SDL_CreateMutex();
}
        
Lockable::~Lockable()
{
    if(isLocked_)
    {
        unlock();
    }
    SDL_DestroyMutex(static_cast<SDL_mutex*>(mutex_));
}

void Lockable::lock()
{
    // Acquire the lock.  Blocks until the operation succeeds.
    if(SDL_LockMutex(static_cast<SDL_mutex*>(mutex_)) < 0)
    {
        throw Error("Lockable::lock", "SDL_LockMutex failed");
    }

    isLocked_ = true;
}

void Lockable::unlock()
{
    // Release the lock.
    SDL_UnlockMutex(static_cast<SDL_mutex*>(mutex_));

    isLocked_ = false;
}

void Lockable::assertLocked() const
{
    assert(isLocked_);
}

