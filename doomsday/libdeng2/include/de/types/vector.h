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

#ifndef LIBDENG2_VECTOR_H
#define LIBDENG2_VECTOR_H

#include "../math.h"
#include "../ISerializable"
#include "../Writer"
#include "../Reader"
#include "../String"

#include <sstream>
#include <cmath>

namespace de
{
    /**
     * Template class for 2D vectors (points).
     * The members are public for convenient access.
     * The used value type must be serializable.
     *
     * @ingroup types
     */ 
    template <typename Type>
    class Vector2 : public ISerializable
    {
    public:
        typedef Type ValueType;
        
    public:
        Vector2(Type a = 0, Type b = 0) : x(a), y(b) {}
        Vector2(const Type* ab) : x(ab[0]), y(ab[1]) {}

        /// Conversion operator to a float vector.
        operator Vector2<dfloat> () const {
            return Vector2<dfloat>(dfloat(x), dfloat(y));
        }
        /// Conversion operator to a double vector.
        operator Vector2<ddouble> () const {
            return Vector2<ddouble>(ddouble(x), ddouble(y));
        }
        operator String () const {
            return asText();
        }
        Vector2 operator + (const Vector2& other) const {
            return Vector2(x + other.x, y + other.y);
        }
        Vector2 operator - (const Vector2& other) const {
            return Vector2(x - other.x, y - other.y);
        }
        Vector2 operator - () const {
            return Vector2(-x, -y);
        }
        Vector2 operator * (ddouble scalar) const {
            return Vector2(Type(x * scalar), Type(y * scalar));
        }
        Vector2 operator * (const Vector2& other) const {
            return Vector2(x * other.x, y * other.y);
        }
        Vector2& operator += (const Vector2& other) {
            x += other.x;
            y += other.y;
            return *this;
        }
        Vector2& operator -= (const Vector2& other) {
            x -= other.x;
            y -= other.y;
            return *this;
        }
        Vector2& operator *= (ddouble scalar) {
            x = Type(x * scalar);
            y = Type(y * scalar);
            return *this;
        }
        Vector2& operator *= (const Vector2& other) {
            x = Type(x * other.x);
            y = Type(y * other.y);
            return *this;
        }
        bool operator == (const Vector2& other) const {
            return !(*this != other);
        }
        bool operator != (const Vector2& other) const {
            return !de::fequal(x, other.x) || !de::fequal(y, other.y);
        }
        bool operator > (const Vector2& other) const {
            return x > other.x && y > other.y;
        }
        bool operator < (const Vector2& other) const {
            return x < other.x && y < other.y;
        }
        bool operator >= (const Vector2& other) const {
            return x >= other.x && y >= other.y;
        }
        bool operator <= (const Vector2& other) const {
            return x <= other.x && y <= other.y;
        }
        ddouble length() const { 
            return std::sqrt(ddouble(x*x + y*y)); 
        }
        Type distance(const Vector2& other) const {
            return Vector2(other.x - x, other.y - y).length();
        }
        Type dotProduct(const Vector2& other) const {
            return x * other.x + y * other.y;
        }
        String asText() const { 
            std::ostringstream s;
            s << *this;
            return s.str();
        }
        Vector2 min(const Vector2& other) const {
            return Vector2(de::min(x, other.x), de::min(y, other.y));
        }
        Vector2 max(const Vector2& other) const {
            return Vector2(de::max(x, other.x), de::max(y, other.y));
        }
        Type min() const {
            return de::min(x, y);
        }
        Type max() const {
            return de::max(x, y);
        }
        /// @return              @c true, if the two vectors are parallel.
        bool isParallel(const Vector2& other) const {
            // Both must be non-zero vectors.
            if(length() == 0 || other.length() == 0)
                return true;
            // If it's close enough, we'll consider them parallel.
            Type dot = dotProduct(other) / length() / other.length();
            return dot > de::EPSILON || dot < -de::EPSILON;
        }

        // Implements ISerializable.
        void operator >> (Writer& to) const {
            to << x << y;
        }
        void operator << (Reader& from) {
            from >> x >> y;
        }

    public:
        Type x;
        Type y;  
    };
    
    template <typename Type> 
    std::ostream& operator << (std::ostream& os, const Vector2<Type>& vec2)
    {
        os << "(" << vec2.x << ", " << vec2.y << ")";
        return os;
    }
    
    /**
     * Template class for 3D vectors (points).
     * The members are public for convenient access.
     *
     * @ingroup types
     */ 
    template <typename Type>
    class Vector3 : public Vector2<Type>
    {
    public:
        Vector3(Type a = 0, Type b = 0, Type c = 0) : Vector2<Type>(a, b), z(c) {}
        Vector3(const Vector2<Type>& v2) : Vector2<Type>(v2), z(0) {}
        Vector3(const Type* abc) : Vector2<Type>(abc), z(abc[2]) {}
        
        /// Conversion operator to a float vector.
        operator Vector3<dfloat> () const {
            return Vector3<dfloat>(Vector2<Type>::x, Vector2<Type>::y, z);
        }
        /// Conversion operator to a double vector.
        operator Vector3<ddouble> () const {
            return Vector3<ddouble>(Vector2<Type>::x, Vector2<Type>::y, z);
        }
        operator String () const {
            return asText();
        }
        Vector3 operator + (const Vector3& other) const {
            return Vector3(Vector2<Type>::x + other.x, Vector2<Type>::y + other.y, z + other.z);
        }
        Vector3 operator - (const Vector3& other) const {
            return Vector3(Vector2<Type>::x - other.x, Vector2<Type>::y - other.y, z - other.z);
        }
        Vector3 operator - () const {
            return Vector3(-Vector2<Type>::x, -Vector2<Type>::y, -z);
        }
        Vector3 operator * (ddouble scalar) const {
            return Vector3(Type(Vector2<Type>::x * scalar), Type(Vector2<Type>::y * scalar), 
                Type(z * scalar));
        }
        Vector3 operator * (const Vector3& other) const {
            return Vector3(Vector2<Type>::x * other.x, Vector2<Type>::y * other.y, z * other.z);
        }
        Vector3& operator += (const Vector3& other) {
            Vector2<Type>::x += other.x;
            Vector2<Type>::y += other.y;
            z += other.z;
            return *this;
        }
        Vector3& operator -= (const Vector3& other) {
            Vector2<Type>::x -= other.x;
            Vector2<Type>::y -= other.y;
            z -= other.z;
            return *this;
        }
        bool operator == (const Vector3& other) const {
            return !(*this != other);
        }
        bool operator != (const Vector3& other) const {
            return Vector2<Type>::operator != (other) || !de::fequal(z, other.z);
        }
        bool operator > (const Vector3& other) const {
            return Vector2<Type>::operator > (other) && z > other.z;
        }
        bool operator < (const Vector3& other) const {
            return Vector2<Type>::operator < (other) && z < other.z;
        }
        bool operator >= (const Vector3& other) const {
            return Vector2<Type>::operator >= (other) && z >= other.z;
        }
        bool operator <= (const Vector3& other) const {
            return Vector2<Type>::operator <= (other) && z <= other.z;
        }
        ddouble length() const { return std::sqrt(Vector2<Type>::x*Vector2<Type>::x +
            Vector2<Type>::y*Vector2<Type>::y + z*z); }
        String asText() const { 
            std::ostringstream os;
            os << *this;
            return os.str();
        }
        Vector3 min(const Vector3& other) const {
            return Vector3(de::min(Vector2<Type>::x, other.x), de::min(Vector2<Type>::y, other.y), 
                de::min(z, other.z));
        }
        Vector3 max(const Vector3& other) const {
            return Vector3(de::max(Vector2<Type>::x, other.x), de::max(Vector2<Type>::y, other.y), 
                de::max(z, other.z));
        }
        Type min() const {
            return de::min(z, Vector2<Type>::min());
        }
        Type max() const {
            return de::max(z, Vector2<Type>::max());
        }
        // Implements ISerializable.
        void operator >> (Writer& to) const {
            Vector2<Type>::operator >> (to);
            to << z;
        }
        void operator << (Reader& from) {
            Vector2<Type>::operator << (from);
            from >> z;
        }
        
    public:
        Type z;
    };
    
    template <typename Type> 
    std::ostream& operator << (std::ostream& os, const Vector3<Type>& vec3)
    {
        os << "(" << vec3.x << ", " << vec3.y << ", " << vec3.z << ")";
        return os;
    }
    
    /**
     * Template class for 4D vectors.
     * The members are public for convenient access.
     *
     * @ingroup types
     */ 
    template <typename Type>
    class Vector4 : public Vector3<Type>
    {
    public:
        Vector4(Type a = 0, Type b = 0, Type c = 0, Type d = 0) : Vector3<Type>(a, b, c), w(d) {}
        Vector4(const Vector3<Type>& v3) : Vector3<Type>(v3), w(0) {}
        Vector4(const Type* abcd) : Vector3<Type>(abcd), w(abcd[3]) {}

        /// Conversion operator to a float vector.
        operator Vector4<dfloat> () const {
            return Vector4<dfloat>(Vector3<Type>::x, Vector3<Type>::y, Vector3<Type>::z, w);
        }
        /// Conversion operator to a double vector.
        operator Vector4<ddouble> () const {
            return Vector4<ddouble>(Vector3<Type>::x, Vector3<Type>::y, Vector3<Type>::z, w);
        }
        operator String () const {
            return asText();
        }
        Vector4 operator + (const Vector4& other) const {
            return Vector4(Vector3<Type>::x + other.x, Vector3<Type>::y + other.y, 
                Vector3<Type>::z + other.z, w + other.w);
        }
        Vector4 operator - (const Vector4& other) const {
            return Vector4(Vector3<Type>::x - other.x, Vector3<Type>::y - other.y, 
                Vector3<Type>::z - other.z, w - other.w);
        }
        Vector4 operator - () const {
            return Vector4(-Vector3<Type>::x, -Vector3<Type>::y, -Vector3<Type>::z, -w);
        }
        Vector4 operator * (ddouble scalar) const {
            return Vector4(Type(Vector3<Type>::x * scalar), Type(Vector3<Type>::y * scalar),
                Type(Vector3<Type>::z * scalar), Type(w * scalar));
        }
        Vector4 operator * (const Vector4& other) const {
            return Vector4(Vector3<Type>::x * other.x, Vector3<Type>::y * other.y, 
                Vector3<Type>::z * other.z, w * other.w);
        }
        Vector4& operator += (const Vector4& other) {
            Vector3<Type>::x += other.x;
            Vector3<Type>::y += other.y;
            Vector3<Type>::z += other.z;
            w += other.w;
            return *this;
        }
        Vector4& operator -= (const Vector4& other) {
            Vector3<Type>::x -= other.x;
            Vector3<Type>::y -= other.y;
            Vector3<Type>::z -= other.z;
            w -= other.w;
            return *this;
        }
        bool operator == (const Vector4& other) const {
            return !(*this != other);
        }
        bool operator != (const Vector4& other) const {
            return Vector3<Type>::operator != (other) || !de::fequal(w, other.w);
        }
        bool operator > (const Vector4& other) const {
            return Vector3<Type>::operator > (other) && w > other.w;
        }
        bool operator < (const Vector4& other) const {
            return Vector3<Type>::operator < (other) && w < other.w;
        }
        bool operator >= (const Vector4& other) const {
            return Vector3<Type>::operator >= (other) && w >= other.w;
        }
        bool operator <= (const Vector4& other) const {
            return Vector3<Type>::operator <= (other) && w <= other.w;
        }
        String asText() const { 
            std::ostringstream os;
            os << *this;
            return os.str();
        }
        Vector4 min(const Vector4& other) const {
            return Vector4(de::min(Vector3<Type>::x, other.x), de::min(Vector3<Type>::y, other.y),
                de::min(Vector3<Type>::z, other.z), de::min(w, other.w));
        }
        Vector4 max(const Vector4& other) const {
            return Vector4(de::max(Vector3<Type>::x, other.x), de::max(Vector3<Type>::y, other.y),
                de::max(Vector3<Type>::z, other.z), de::max(w, other.w));
        }
        Type min() const {
            return de::min(w, Vector3<Type>::min());
        }
        Type max() const {
            return de::max(w, Vector3<Type>::max());
        }
        // Implements ISerializable.
        void operator >> (Writer& to) const {
            Vector3<Type>::operator >> (to);
            to << w;
        }
        void operator << (Reader& from) {
            Vector3<Type>::operator << (from);
            from >> w;
        }
        
    public:
        Type w;
    };
    
    template <typename Type> 
    std::ostream& operator << (std::ostream& os, const Vector4<Type>& vec4)
    {
        os << "(" << vec4.x << ", " << vec4.y << ", " << vec4.z << ", " << vec4.w << ")";
        return os;
    }
    
    //@{
    /// @ingroup types
    typedef Vector2<dbyte> Vector2b;    ///< 2-component vector of byte values.
    typedef Vector2<dint> Vector2i;     ///< 2-component vector of integer values.
    typedef Vector2<duint> Vector2ui;   ///< 2-component vector of unsigned integer values.
    typedef Vector2<dfloat> Vector2f;   ///< 2-component vector of floating point values.
    typedef Vector2<ddouble> Vector2d;  ///< 2-component vector of high-precision floating point values.
    typedef Vector3<dbyte> Vector3b;    ///< 3-component vector of byte values.
    typedef Vector3<dint> Vector3i;     ///< 3-component vector of integer values.
    typedef Vector3<duint> Vector3ui;   ///< 3-component vector of unsigned integer values.
    typedef Vector3<dfloat> Vector3f;   ///< 3-component vector of floating point values.
    typedef Vector3<ddouble> Vector3d;  ///< 3-component vector of high-precision floating point values.
    typedef Vector4<dbyte> Vector4b;    ///< 4-component vector of byte values.
    typedef Vector4<dint> Vector4i;     ///< 4-component vector of integer values.
    typedef Vector4<duint> Vector4ui;   ///< 4-component vector of unsigned integer values.
    typedef Vector4<dfloat> Vector4f;   ///< 4-component vector of floating point values.
    typedef Vector4<ddouble> Vector4d;  ///< 4-component vector of high-precision floating point values.
    //@}
};

#endif /* LIBDENG2_VECTOR_H */
