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

#include "de/FunctionValue"
#include "de/Process"
#include "de/ArrayValue"
#include "de/Writer"
#include "de/Reader"

using namespace de;

FunctionValue::FunctionValue() : _func(new Function())
{
    // We now hold the only reference to the function.
}

FunctionValue::FunctionValue(Function* func) : _func(func->ref<Function>())
{}

FunctionValue::~FunctionValue()
{
    _func->release();
}

Value* FunctionValue::duplicate() const
{
    return new FunctionValue(_func);
}

Value::Text FunctionValue::asText() const
{
    return _func->asText();
}

bool FunctionValue::isTrue() const
{
    return true;
}

bool FunctionValue::isFalse() const
{
    return false;
}

dint FunctionValue::compare(const Value& value) const
{
    const FunctionValue* other = dynamic_cast<const FunctionValue*>(&value);
    if(!other)
    {
        return -1;
    }
    // Address comparison.
    if(_func == other->_func)
    {
        return 0;
    }
    if(_func > other->_func)
    {
        return 1;
    }
    return -1;
}

void FunctionValue::call(Process& process, const Value& arguments) const
{
    const ArrayValue* array = dynamic_cast<const ArrayValue*>(&arguments);
    if(!array)
    {
        /// @throw IllegalError  The call arguments must be an array value.
        throw IllegalError("FunctionValue::call", "Arguments is not an array");
    }
    process.call(*_func, *array);
}

void FunctionValue::operator >> (Writer& to) const
{
    to << SerialId(FUNCTION) << *_func;
}

void FunctionValue::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != FUNCTION)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized value was invalid.
        throw DeserializationError("FunctionValue::operator <<", "Invalid ID");
    }
    from >> *_func;
}
