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

#ifndef LIBDENG2_NUMBERVALUE_H
#define LIBDENG2_NUMBERVALUE_H

#include <de/Value>

namespace de
{
    /**
     * The NumberValue class is a subclass of Value that holds a single
     * double precision floating point number.
     *
     * @ingroup data
     */
	class PUBLIC_API NumberValue : public Value
	{
	public:
	    /// Truth values for logical operations. They are treated as
	    /// number values.
	    enum {
	        FALSE = 0,
	        TRUE = 1
        };
	    
	public:
		NumberValue(Number initialValue = 0);

        /**
         * Conversion template that forces a cast to another type.
         */
        template <typename Type>
        Type as() const { return Type(value_); }

        Value* duplicate() const;
		Number asNumber() const;
		Text asText() const;
        bool isTrue() const;
        dint compare(const Value& value) const;
        void negate();        
        void sum(const Value& value);        
        void subtract(const Value& value);
        void divide(const Value& divisor);
        void multiply(const Value& value);
        void modulo(const Value& divisor);

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
	private:
		Number value_;
	};
}

#endif /* LIBDENG2_NUMBERVALUE_H */
