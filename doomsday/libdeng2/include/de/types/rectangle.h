/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_RECTANGLE_H
#define LIBDENG2_RECTANGLE_H

#include "../Vector"

namespace de
{
    /**
     * Template for 2D rectangles.
     * The members are public for convenient access.
     *
     * @ingroup types
     */
    template <typename VectorType>
    class Rectangle
    {
    public:
        typedef VectorType Corner;
        typedef typename Corner::ValueType Type;
    
    public:
        Rectangle() {}
        Rectangle(const Corner& tl, const Corner& br) : topLeft(tl), bottomRight(br) {}
        Type width() const { return abs(bottomRight.x - topLeft.x); }
        Type height() const { return abs(bottomRight.y - topLeft.y); }
        Vector2<Type> size() const { return Vector2<Type>(width(), height()); }        
        void setWidth(Type w) { bottomRight.x = topLeft.x + w; }
        void setHeight(Type h) { bottomRight.y = topLeft.y + h; }
        void setSize(const Vector2<Type>& s) { setWidth(s.x); setHeight(s.y); }
        void include(const Corner& point) {
            topLeft = topLeft.min(point);
            bottomRight = bottomRight.max(point);
        }
        bool contains(const Corner& point) const {
            return point >= topLeft && point <= bottomRight;
        }
        std::string asText() const { 
            return "[" + topLeft.asText() + ", " + bottomRight.asText() + "]";
        }
        Corner topRight() const {
            return Corner(bottomRight.x, topLeft.y);
        }
        Corner bottomLeft() const {
            return Corner(topLeft.x, bottomRight.y);
        }
        Corner midLeft() const {
            return Corner(topLeft.x, (topLeft.y + bottomRight.y)/2.0);
        }
        Corner midRight() const {
            return Corner(bottomRight.x, (topLeft.y + bottomRight.y)/2.0);
        }
        Corner midTop() const {
            return Corner((topLeft.x + bottomRight.x)/2.0, topLeft.y);
        }
        Corner midBottom() const {
            return Corner((topLeft.x + bottomRight.x)/2.0, bottomRight.y);
        }
        Corner middle() const {
            return Corner((topLeft.x + bottomRight.x)/2.0, (topLeft.y + bottomRight.y)/2.0);
        }
    
    public:
        Corner topLeft;
        Corner bottomRight;
    };

    // Common types.
    typedef Rectangle<Vector2i> Rectanglei;
    typedef Rectangle<Vector2ui> Rectangleui;
    typedef Rectangle<Vector2f> Rectanglef;
}

#endif /* LIBDENG2_RECTANGLE_H */
