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

#ifndef LIBDENG2_PARTITION_H
#define LIBDENG2_PARTITION_H

#include "../Vector"

namespace de
{
    /**
     * An infinite line of the form point + direction vectors.
     */
    struct Partition
    {
        Vector2d point;
        Vector2d direction;

        Partition() : point(0, 0), direction(0, 0) {};
        Partition(const Vector2d& _point, const Vector2d& _direction)
            : point(_point), direction(_direction) {};
        Partition(ddouble x, ddouble y, ddouble dX, ddouble dY)
            : point(x, y), direction(dX, dY) {};

        /**
         * Which side of the partition does the point lie?
         * @return              @c false = front.
         */
        bool pointOnSide(ddouble x, ddouble y) const;
        bool pointOnSide(const Vector2d& otherPoint) const { return pointOnSide(otherPoint.x, otherPoint.y); }
    };
}

#endif /* LIBDENG2_PARTITION_H */
