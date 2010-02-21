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

#ifndef LIBDENG2_LINE_H
#define LIBDENG2_LINE_H

#include "../ISerializable"
#include "../Writer"
#include "../Reader"
#include "../String"
#include "../Vector"

#include <sstream>

namespace de
{
    /**
     * Template for 2D lines of the form point + direction vectors.
     * The members are public for convenient access.
     * The used value type must be serializable.
     *
     * @ingroup types
     */
    template <typename Type>
    class Line2 : public ISerializable
    {
    public:
        typedef Type ValueType;

    public:
        Line2(Type x = 0, Type y = 0, Type dX = 0, Type dY = 0)
            : point(x, y), direction(dX, dY) {};
        Line2(const Vector2<Type>& _point, const Vector2<Type>& _direction)
            : point(_point), direction(_direction) {};

        operator String () const {
            return asText();
        }
        String asText() const { 
            std::ostringstream s;
            s << *this;
            return s.str();
        }

        /**
         * Which side of the line does the point lie?
         * @return              @c false = front.
         */
        bool side(const Vector2<Type>& otherPoint) const {
            Vector2<Type> diff = otherPoint - point;
            Vector2<Type> perp = Vector2<Type>(-diff.y, diff.x);
            return sign(Vector2<Type>(direction - point).dotProduct(perp)) == -1;
        }

        bool side(Type x, Type y) const {
            return side(Vector2<Type>(x, y));
        }

        // Implements ISerializable.
        void operator >> (Writer& to) const {
            to << point << direction;
        }
        void operator << (Reader& from) {
            from >> point >> direction;
        }

    public:
        Vector2<Type> point;
        Vector2<Type> direction;
    };

    template <typename Type> 
    std::ostream& operator << (std::ostream& os, const Line2<Type>& line2)
    {
        os << "(" << line2.point << " | " << line2.direction << ")";
        return os;
    }

    //@{
    /// @ingroup types
    typedef Line2<dint> Line2i;     ///< 2D line with integer components.
    typedef Line2<dfloat> Line2f;   ///< 2D line with floating point components.
    typedef Line2<ddouble> Line2d;  ///< 2D line with high-precision floating point components.
    //@}
}

#endif /* LIBDENG2_LINE_H */
