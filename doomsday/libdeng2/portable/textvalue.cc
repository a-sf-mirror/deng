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

#include "de/textvalue.h"
#include "de/numbervalue.h"
#include "de/arrayvalue.h"
#include "de/textvalue.h"
#include "de/string.h"
#include "de/writer.h"
#include "de/reader.h"

#include <list>
#include <string>
#include <sstream>
#include <cctype>

using namespace de;
using std::list;
using std::string;
using std::ostringstream;

TextValue::TextValue(const Text& initialValue)
    : value_(initialValue)
{    
}

Value* TextValue::duplicate() const
{
    return new TextValue(value_);
}

Value::Number TextValue::asNumber() const
{
	std::istringstream str(value_);
	Number number = 0;
	str >> number;
	return number;
}

Value::Text TextValue::asText() const
{
	return value_;
}

duint TextValue::size() const
{
	return value_.size();
}

bool TextValue::isTrue() const
{
    // If there is at least one nonwhite character, this is considered a truth.
    for(Text::const_iterator i = value_.begin(); i != value_.end(); ++i)
    {
        if(!std::isspace(*i))
            return true;
    }
    return false;
}

dint TextValue::compare(const Value& value) const
{
    const TextValue* other = dynamic_cast<const TextValue*>(&value);
    if(other)
    {
        return value_.compare(other->value_);
    }
    return Value::compare(value);
}

void TextValue::sum(const Value& value)
{
    const TextValue* other = dynamic_cast<const TextValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("TextValue::sum", "Value cannot be summed");
    }
    
    value_ += other->value_;
}

void TextValue::multiply(const Value& value)
{
    const NumberValue* other = dynamic_cast<const NumberValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("Value::multiply", "Value cannot be multiplied");
    }
    
    int factor = other->asNumber();
    
    if(factor < 1)
    {
        value_.clear();
    }
    else
    {
        string mul = value_;
        while(--factor > 0)
        {
            value_ += mul;
        }
    }
}

void TextValue::divide(const Value& value)
{
    const TextValue* other = dynamic_cast<const TextValue*>(&value);
    if(!other)
    {
        throw ArithmeticError("TextValue::divide", "Text cannot be divided");
    }
    value_ = value_.concatenatePath(other->value_);
}

void TextValue::modulo(const Value& value)
{
    list<const Value*> args;
    
    const ArrayValue* array = dynamic_cast<const ArrayValue*>(&value);
    if(array)
    {
        for(ArrayValue::Elements::const_iterator i = array->elements().begin();
            i != array->elements().end(); ++i)
        {
            args.push_back(*i);
        }
    }
    else
    {
        // Just one.
        args.push_back(&value);
    }
    
    value_ = substitutePlaceholders(value_, args);
}

string TextValue::substitutePlaceholders(const std::string& pattern, const std::list<const Value*>& args)
{
    ostringstream result;
    list<const Value*>::const_iterator arg = args.begin();
    
    for(string::const_iterator i = pattern.begin(); i != pattern.end(); ++i)
    {
        char ch = *i;
        
        if(ch == '%')
        {
            if(arg == args.end())
            {
                throw IllegalPatternError("TextValue::replacePlaceholders",
                    "Too few substitution values");
            }
            
            result << String::patternFormat(i, pattern.end(), **arg);
            ++arg;
        }
        else
        {
            result << ch;
        }
    }
    
    return result.str();
}

void TextValue::operator >> (Writer& to) const
{
    to << SerialId(TEXT) << value_;
}

void TextValue::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != TEXT)
    {
        throw DeserializationError("TextValue::operator <<", "Invalid ID");
    }
    from >> value_;
}
