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

#include "de/Function"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/Writer"
#include "de/Reader"
#include "de/Log"

#include <sstream>

using namespace de;

Function::Function() : _globals(0)
{}

Function::Function(const Arguments& args, const Defaults& defaults) 
    : _arguments(args), _defaults(defaults), _globals(0)
{}

Function::~Function()
{
    // Delete the default argument values.
    FOR_EACH(i, _defaults, Defaults::iterator)
    {
        delete i->second;
    }
    if(_globals)
    {
        // Stop observing the namespace.
        _globals->audienceForDeletion.remove(this);
    }
}

String Function::asText() const
{
    std::ostringstream os;
    os << "[Function " << this << " (";
    FOR_EACH(i, _arguments, Arguments::const_iterator)
    {
        if(i != _arguments.begin())
        {
            os << ", ";
        }
        os << *i;
        Defaults::const_iterator def = _defaults.find(*i);
        if(def != _defaults.end())
        {
            os << "=" << def->second->asText();
        }
    }
    os << ")]";
    return os.str();
}

void Function::mapArgumentValues(const ArrayValue& args, ArgumentValues& values) const
{
    const DictionaryValue* labeledArgs = dynamic_cast<const DictionaryValue*>(
        args.elements().front());
    assert(labeledArgs != NULL);

    // First use all the unlabeled arguments.
    Arguments::const_iterator k = _arguments.begin();
    for(ArrayValue::Elements::const_iterator i = args.elements().begin() + 1;
        i != args.elements().end(); ++i)
    {
        values.push_back(*i);
        
        if(k != _arguments.end())
        {
            if(labeledArgs->contains(TextValue(*k)))
            {
                /// @throw WrongArgumentsError An argument has been given more than one value.
                throw WrongArgumentsError("Function::mapArgumentValues",
                    "More than one value has been given for '" + 
                    *k + "' in function call");
            }
            ++k;
        }
    }
    
    if(values.size() < _arguments.size())
    {
        // Then apply the labeled arguments, falling back to default values.
        Arguments::const_iterator i = _arguments.begin();
        // Skip past arguments we already have a value for.
        for(duint count = values.size(); count > 0; --count, ++i);
        for(; i != _arguments.end(); ++i)
        {
            try 
            {
                values.push_back(&labeledArgs->element(TextValue(*i)));
            }
            catch(const DictionaryValue::KeyError&)
            {
                // Check the defaults.
                Defaults::const_iterator k = _defaults.find(*i);
                if(k != _defaults.end())
                {
                    values.push_back(k->second);
                }
                else
                {
                    /// @throw WrongArgumentsError Argument is missing a value.
                    throw WrongArgumentsError("Function::mapArgumentValues",
                        "The value of argument '" + *i + "' has not been defined in function call");
                }
            }
        }
    }
    
    // Check that the number of arguments matches what we expect.
    if(values.size() != _arguments.size())
    {
        std::ostringstream os;
        os << "Expected " << _arguments.size() << " arguments, but got " <<
            values.size() << " arguments in function call";
        /// @throw WrongArgumentsError  Wrong number of argument specified.
        throw WrongArgumentsError("Function::mapArgumentValues", os.str());
    }    
}

void Function::setGlobals(Record* globals)
{
    LOG_AS("Function::setGlobals");
    
    if(!_globals)
    {
        _globals = globals;
        _globals->audienceForDeletion.add(this);
    }
    else if(_globals != globals)
    {
        LOG_WARNING("Function was offered a different namespace.");
    }
}

Record* Function::globals() const
{
    return _globals;
}

bool Function::callNative(Context& context, const ArgumentValues& args) const
{
    assert(args.size() == _arguments.size());    
    
    // Do non-native function call.
    return false;
}

void Function::operator >> (Writer& to) const
{
    // Number of arguments.
    to << duint16(_arguments.size());

    // Argument names.
    FOR_EACH(i, _arguments, Arguments::const_iterator)
    {
        to << *i;
    }
    
    // Number of default values.
    to << duint16(_defaults.size());
    
    // Default values.
    FOR_EACH(i, _defaults, Defaults::const_iterator)
    {
        to << i->first << *i->second;        
    }
    
    // The statements of the function.
    to << _compound;        
}

void Function::operator << (Reader& from)
{
    duint16 count = 0;
    
    // Argument names.
    from >> count;
    _arguments.clear();
    while(count--)
    {
        String argName;
        from >> argName;
        _arguments.push_back(argName);
    }
    
    // Default values.
    from >> count;
    _defaults.clear();
    while(count--)
    {
        String name;
        from >> name;
        _defaults[name] = Value::constructFrom(from);
    }
    
    // The statements.
    from >> _compound;
}

void Function::recordBeingDeleted(Record& record)
{
    // The namespace of the record is being deleted.
    assert(_globals == &record);
    _globals = 0;
}
