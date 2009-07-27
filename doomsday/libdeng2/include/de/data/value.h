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

#ifndef LIBDENG2_VALUE_H
#define LIBDENG2_VALUE_H

#include "../deng.h"
#include "../ISerializable"
#include "../String"

/**
 * @defgroup data Data
 *
 * Classes related to accessing, storing, and processing of data.
 */

namespace de
{
    /**
     * The base class for all runtime values.  This is an abstract class.
     *
     * @ingroup data
     */
    class PUBLIC_API Value : public String::IPatternArg, public ISerializable
    {
    public:
        /// An illegal operation (i.e., one that is not defined by the Value) was attempted. 
        /// @ingroup errors
        DEFINE_ERROR(IllegalError);
        
        /// An illegal conversion was attempted. @ingroup errors
        DEFINE_SUB_ERROR(IllegalError, ConversionError);
        
        /// An illegal arithmetic operation is attempted (e.g., division by text). @ingroup errors
        DEFINE_SUB_ERROR(IllegalError, ArithmeticError);

        // Types used by all values:
        typedef ddouble Number;     /**< Numbers are in double-precision. */
        typedef String Text;        /**< Text strings. */

    public:
        virtual ~Value();
        
        /**
         * Creates a duplicate copy of the value.
         *
         * @return New Value object. Caller gets ownership of the object.
         */
        virtual Value* duplicate() const = 0;

        /**
         * Convert the value to a number.  Implementing this is
         * optional.  The default implementation will raise an
         * exception.
         */
        virtual Number asNumber() const;
        
        /**
         * Convert the value to a number. This will never raise an
         * exception. If the conversion is not possible, it will 
         * return @a defaultValue.
         *
         * @return Value as number.
         */
        virtual Number asSafeNumber(const Number& defaultValue = 0.0) const;
        
        /**
         * Convert the value to into a text string.  All values have
         * to implement this.
         */
        virtual Text asText() const = 0;

        /**
         * Determine the size of the value.  The meaning of this
         * depends on the type of the value.
         */
        virtual dsize size() const;

        /**
         * Get a specific element of the value.  This is meaningful with 
         * arrays and dictionaries.
         */
        virtual const Value* element(const Value& index) const;

        /**
         * Get a specific element of the value.  This is meaningful with 
         * arrays and dictionaries. This method is non-const, meaning that
         * the returned value can be modified.
         */
        virtual Value* element(const Value& index);
        
        /**
         * Set a specific element of the value. This is meaningful only with
         * arrays and dictionaries, which are composed out of modifiable 
         * elements.
         *
         * @param index  Index of the element.
         * @param elementValue  New value for the element. This value will take
         *               ownership of @a elementValue.
         */
        virtual void setElement(const Value& index, Value* elementValue);

        /**
         * Determines whether the value contains the equivalent of another
         * value. Defined for arrays.
         *
         * @param value Value to look for.
         *
         * @return @c true, if the value is there, otherwise @c false.
         */
        virtual bool contains(const Value& value) const;

        /**
         * Begin iteration of contained values. This is only meaningful with
         * iterable values such as arrays.
         *
         * @return  The first value. Caller gets ownership. @c NULL, if there 
         *      are no items.
         */
        virtual Value* begin();

        /**
         * Iterate the next value. This is only meaningful with iterable 
         * values such as arrays.
         *
         * @return @c NULL, if the iteration is over. Otherwise a new Value
         * containing the next iterated value. Caller gets ownership.
         */
        virtual Value* next();

        /**
         * Determine if the value can be thought of as a logical truth.
         */
        virtual bool isTrue() const = 0;

        /**
         * Determine if the value can be thought of as a logical falsehood.
         */
        virtual bool isFalse() const;

        /**
         * Compares this value to another.
         * 
         * @param value Value to compare with.
         * 
         * @return 0, if the values are equal. 1, if @a value is greater than
         *      this value. -1, if @a value is less than this value.
         */
        virtual dint compare(const Value& value) const;
        
        /**
         * Negate the value of this value.
         */
        virtual void negate();
        
        /**
         * Calculate the sum of this value and an another value, storing the
         * result in this value.
         */
        virtual void sum(const Value& value);

        /**
         * Calculate the subtraction of this value and an another value, 
         * storing the result in this value.
         */
        virtual void subtract(const Value& subtrahend);
        
        /**
         * Calculate the division of this value divided by @a divisor, storing
         * the result in this value.
         */
        virtual void divide(const Value& divisor);
        
        /**
         * Calculate the multiplication of this value by @a value, storing
         * the result in this value.
         */
        virtual void multiply(const Value& value);
        
        /**
         * Calculate the modulo of this value divided by @a divisor, storing
         * the result in this value.
         */
        virtual void modulo(const Value& divisor);
        
    public:
        /**
         * Construct a value by reading data from the Reader.
         *
         * @param reader  Data for the value.
         *
         * @return  Value. Caller gets owernship.
         */
        static Value* constructFrom(Reader& reader);
        
    protected:
        typedef dbyte SerialId;
        
        enum SerialIds {
            NONE,
            NUMBER,
            TEXT,
            ARRAY,
            DICTIONARY,
            BLOCK
        };
    };
}

#endif /* LIBDENG2_VALUE_H */
