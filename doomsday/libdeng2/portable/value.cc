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

#include "de/value.h"
#include "de/numbervalue.h"
#include "de/textvalue.h"
#include "de/arrayvalue.h"
#include "de/dictionaryvalue.h"
#include "de/reader.h"

using namespace de;

Value::~Value()
{}

Value::Number Value::asNumber() const
{
	throw ConversionError("Value::asNumber", "Illegal conversion");
}

Value::Number Value::asSafeNumber(const Number& defaultValue) const
{
    try
    {
        return asNumber();
    }
    catch(const Error&)
    {
        return defaultValue;
    }
}

dsize Value::size() const
{
	throw IllegalError("Value::size", "Size is meaningless");
}

const Value* Value::element(const Value& index) const
{
	throw IllegalError("Value::element", "Value cannot be indexed");
}

Value* Value::element(const Value& index)
{
    throw IllegalError("Value::element", "Value cannot be indexed");
}

void Value::setElement(const Value& index, Value* elementValue)
{
    throw IllegalError("Value::setElement", "Value cannot be indexed");
}

bool Value::contains(const Value& value) const
{
	throw IllegalError("Value::contains", "Value is not a container");
}

Value* Value::begin()
{
    throw IllegalError("Value::begin", "Value is not iterable");
}

Value* Value::next()
{
    throw IllegalError("Value::next", "Value is not iterable");
}

bool Value::isFalse() const
{ 
    // Default implementation -- some values may be neither true nor false.
    return !isTrue();
}

dint Value::compare(const Value& value) const
{
    // Default to a generic text-based comparison.
    dint result = asText().compare(value.asText());
    return (result < 0? -1 : result > 0? 1 : 0);
}

void Value::negate()
{
    throw ArithmeticError("Value::negate", "Value cannot be negated");
}
 
void Value::sum(const Value& value)
{
    throw ArithmeticError("Value::sum", "Value cannot be summed");
}

void Value::subtract(const Value& subtrahend)
{
    throw ArithmeticError("Value::subtract", "Value cannot be subtracted from");    
}
 
void Value::divide(const Value& divisor)
{
    throw ArithmeticError("Value::divide", "Value cannot be divided");
}
 
void Value::multiply(const Value& value)
{
    throw ArithmeticError("Value::multiply", "Value cannot be multiplied");
}
 
void Value::modulo(const Value& divisor)
{
    throw ArithmeticError("Value::modulo", "Modulo not defined");
}

Value* Value::constructFrom(Reader& reader)
{
    Value* result = 0;

    SerialId id;
    reader >> id;
    reader.rewind(sizeof(id));
    
    switch(id)
    {
    case NUMBER:
        result = new NumberValue();
        break;
        
    case TEXT:
        result = new TextValue();
        break;
        
    case ARRAY:
        result = new ArrayValue();
        break;
        
    case DICTIONARY:
        result = new DictionaryValue();
        break;
        
    default:
        throw DeserializationError("Value::constructFrom", "Invalid value identifier");
    }

    // Deserialize it.
    reader >> *result;
    return result;
}
