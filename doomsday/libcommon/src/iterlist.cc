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

#include "common/Iterlist"

using namespace de;

IterList::~IterList()
{
    if(_count > 0)
        std::free(_list);
}

dint IterList::add(void* obj)
{
    assert(obj);
    if(++_count > _max)
    {
         _max = (_max? _max * 2 : 8);
         _list = reinterpret_cast<void**>(std::realloc(_list, sizeof(void*) * _max));
    }
    _list[_count - 1] = obj;
    return _count - 1;
}

void* IterList::pop()
{
    if(_count > 0)
        return _list[--_count];
    return NULL;
}

void* IterList::iterator()
{
    if(!_count)
        return NULL;

    if(_forward)
    {
        if(_rover < _count - 1)
            return _list[++_rover];
        return NULL;
    }

    if(_rover > 0)
        return _list[--_rover];
    return NULL;
}

void IterList::resetIterator(bool forward)
{
    _forward = forward;
    if(_forward)
        _rover = -1;
    else
        _rover = _count;
}

void IterList::clear()
{
    _count = _max = _rover = 0;
}

dint IterList::size() const
{
    return _count;
}
