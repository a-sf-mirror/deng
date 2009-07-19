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

#include "de/Variable"
#include "de/Value"
#include "de/NoneValue"
#include "de/NumberValue"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/Reader"
#include "de/Writer"

using namespace de;

Variable::Variable(const std::string& name, Value* initial, const Mode& m)
    : name_(name), mode_(m), value_(0)
{
    if(!initial)
    {
        initial = new NoneValue();
    }
    std::auto_ptr<Value> v(initial);
    verifyValid(*initial);
    value_ = v.release();
}

Variable::~Variable()
{
    delete value_;
}

void Variable::set(Value* v)
{
    std::auto_ptr<Value> val(v);
    verifyValid(*v);
    delete value_;
    value_ = val.release();
}

void Variable::set(const Value& v)
{
    verifyValid(v);
    delete value_;
    value_ = v.duplicate();
}

const Value& Variable::value() const
{
    assert(value_ != NULL);
    return *value_;
}

bool Variable::isValid(const Value& v) const
{
    if((dynamic_cast<const NoneValue*>(&v) && !mode_[NONE_BIT]) ||
        (dynamic_cast<const NumberValue*>(&v) && !mode_[NUMBER_BIT]) ||
        (dynamic_cast<const TextValue*>(&v) && !mode_[TEXT_BIT]) ||
        (dynamic_cast<const ArrayValue*>(&v) && !mode_[ARRAY_BIT]) ||
        (dynamic_cast<const DictionaryValue*>(&v) && !mode_[DICTIONARY_BIT]))
    {
        return false;
    }
    // It's ok.
    return true;
}

void Variable::verifyValid(const Value& v) const
{
    if(!isValid(v))
    {
        throw InvalidError("Variable::verifyValid", 
            "Value type is not allowed by the variable '" + name_ + "'");
    }
}

void Variable::operator >> (Writer& to) const
{
    to << name_ << duint32(mode_.to_ulong()) << *value_;
}

void Variable::operator << (Reader& from)
{
    duint32 modeFlags = 0;
    from >> name_ >> modeFlags;
    mode_ = modeFlags;
    delete value_;
    try
    {
        value_ = Value::constructFrom(from);
    }
    catch(const Error& err)
    {
        // Always need to have a value.
        value_ = new NoneValue();
        err.raise();
    }    
}
