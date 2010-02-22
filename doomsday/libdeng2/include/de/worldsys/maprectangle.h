/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_MAPRECTANGLE_H
#define LIBDENG2_MAPRECTANGLE_H

#include "../Rectangle"
#include "../Vector"

namespace de
{
    /**
     * Template for 2D rectangles in the map coordinate space.
     *
     * Derived from Rectangle but adapts the interface so that
     * - bottomLeft is the minimum (X and Y)
     * - topRight is the maximum (X and Y)
     * All functionality is deferred to Rectangle.
     *
     * @ingroup types
     */
    template <typename VectorType>
    class MapRectangle : public Rectangle<VectorType>
    {
    private:
        /**
         * Redefine topLeft and bottomRight as private to prevent
         * users from mistakenly modifying them directly, under the
         * assumption that they are in the local coordinate space.
         */
        Rectangle::topLeft;
        Rectangle::bottomRight;

    public:
        MapRectangle() : Rectangle::Rectangle() {};
        MapRectangle(const Corner& bl, const Corner& tr) : Rectangle(bl, tr) {};

        /// Conversion operator to a float maprectangle.
        operator MapRectangle<Vector2f> () const {
            return MapRectangle<Vector2f>(Rectangle::topLeft, Rectangle::bottomRight);
        }
        /// Conversion operator to a double maprectangle.
        operator MapRectangle<Vector2d> () const {
            return MapRectangle<Vector2d>(Rectangle::topLeft, Rectangle::bottomRight);
        }

        String asText() const { 
            return "[" + topLeft().asText() + ", " + bottomRight().asText() + "]";
        }
        Corner topRight() const {
            return Rectangle::bottomRight;
        }
        Corner bottomLeft() const {
            return Rectangle::topLeft;
        }
        Corner topLeft() const {
            return Rectangle::bottomLeft();
        }
        Corner bottomRight() const {
            return Rectangle::topRight();
        }
        Corner midTop() const {
            return Corner((topLeft.x + bottomRight.x)/2.0, Rectangle::bottomRight.y);
        }
        Corner midBottom() const {
            return Corner((topLeft.x + bottomRight.x)/2.0, Rectangle::topLeft.y);
        }
    };

    // Common types.
    typedef MapRectangle<Vector2i> MapRectanglei;
    typedef MapRectangle<Vector2f> MapRectanglef;
    typedef MapRectangle<Vector2d> MapRectangled;
}

#endif /* LIBDENG2_MAPRECTANGLE_H */
