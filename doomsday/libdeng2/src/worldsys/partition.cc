/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Partition"

using namespace de;

bool Partition::pointOnSide(ddouble x, ddouble y) const
{
    if(fequal(direction.x, 0))
    {
        if(x <= point.x)
            return direction.y > 0? 1 : 0;
        return direction.y < 0? 1 : 0;
    }

    if(fequal(direction.y, 0))
    {
        if(y <= point.y)
            return direction.x < 0? 1 : 0;
        return direction.x > 0? 1 : 0;
    }

    // Try to quickly decide by looking at the signs.
    Vector2d delta = point - Vector2d(x, y);
    if(direction.x < 0)
    {
        if(direction.y < 0)
        {
            if(delta.x < 0)
            {
                if(delta.y >= 0)
                    return 0;
            }
            else if(delta.y < 0)
                return 1;
        }
        else
        {
            if(delta.x < 0)
            {
                if(delta.y < 0)
                    return 1;
            }
            else if(delta.y >= 0)
                return 0;
        }
    }
    else
    {
        if(direction.y < 0)
        {
            if(delta.x < 0)
            {
                if(delta.y < 0)
                    return 0;
            }
            else if(delta.y >= 0)
                return 1;
        }
        else
        {
            if(delta.x < 0)
            {
                if(delta.y >= 0)
                    return 1;
            }
            else if(delta.y < 0)
                return 0;
        }
    }

    if(delta.y * direction.x < direction.y * delta.x)
        return 0; // front side

    return 1; // back side
}
