/*
 * The Doomsday Engine Project
 *
 * Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_ITERLIST_H
#define LIBCOMMON_ITERLIST_H

#include "common.h"

/**
 * Object iteration list.
 * Can be traversed through iteration but otherwise act like a LIFO stack.
 */
class IterList
{
public:
    IterList();

    ~IterList();

    /**
     * Add the given object to iterlist.
     *
     * @param obj       Ptr to the object to be added to the list.
     * @return          The index of the object within 'list' once added,
     *                  ELSE @c -1.
     */
    de::dint add(void* obj);

    /**
     * Pop the top of the iterlist and return the next element.
     *
     * @return          Ptr to the next object in 'list'.
     */
    void* pop();

    /**
     * Returns the next element in the iterlist.
     *
     * @return          The next object in the iterlist.
     */
    void* iterator();

    /**
     * Returns the iterlist iterator to the beginning (the end).
     *
     * @param forward   @c true = iteration will move forwards.
     */
    void resetIterator(bool forward);

    /**
     * Empty the iterlist.
     */
    void clear();

    /**
     * Return the size of the iterlist.
     *
     * @return          The size of the iterlist.
     */
    de::dint size() const;

private:
    void** _list;
    de::dint _max;
    de::dint _count;

    /// Used during iteration to track the current position of the iterator.
    de::dint _rover;

    /// If @c true iteration moves forward.
    bool _forward;
};

#endif /* LIBCOMMON_ITERLIST_H */
