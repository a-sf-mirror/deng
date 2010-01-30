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

#include "de/BuiltInExpression"
#include "de/Evaluator"
#include "de/NumberValue"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/RefValue"
#include "de/RecordValue"
#include "de/BlockValue"
#include "de/Writer"
#include "de/Reader"

using namespace de;

BuiltInExpression::BuiltInExpression() : _type(NONE), _arg(0)
{}

BuiltInExpression::BuiltInExpression(Type type, Expression* argument)
    : _type(type), _arg(argument)
{}

BuiltInExpression::~BuiltInExpression()
{
    delete _arg;
}

void BuiltInExpression::push(Evaluator& evaluator, Record*) const
{
    Expression::push(evaluator);    
    _arg->push(evaluator);
}

Value* BuiltInExpression::evaluate(Evaluator& evaluator) const
{
    std::auto_ptr<Value> value(evaluator.popResult());
    ArrayValue* args = dynamic_cast<ArrayValue*>(value.get());
    assert(args != NULL); // must be an array
    
    switch(_type)
    {
    case LENGTH:
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for LENGTH");
        }
        return new NumberValue(args->at(1).size());
        
    case DICTIONARY_KEYS:
    case DICTIONARY_VALUES:
    {
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for " +
                std::string(_type == DICTIONARY_KEYS? 
                    "DICTIONARY_KEYS" : "DICTIONARY_VALUES"));
        }
        
        const DictionaryValue* dict = dynamic_cast<const DictionaryValue*>(&args->at(1));
        if(!dict)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Argument must be a dictionary");
        }
        ArrayValue* array = new ArrayValue();
        for(DictionaryValue::Elements::const_iterator i = dict->elements().begin();
            i != dict->elements().end(); ++i)
        {
            if(_type == DICTIONARY_KEYS)
            {
                array->add(i->first.value->duplicate());
            }
            else
            {
                array->add(i->second->duplicate());
            }
        }
        return array;
    }
    
    case RECORD_MEMBERS:    
    case RECORD_SUBRECORDS:
    {
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for " +
                std::string(_type == RECORD_MEMBERS? 
                    "RECORD_MEMBERS" : "RECORD_SUBRECORDS"));
        }
        
        const RecordValue* rec = dynamic_cast<const RecordValue*>(&args->at(1));
        if(!rec)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Argument must be a record");
        }
        DictionaryValue* dict = new DictionaryValue();
        if(_type == RECORD_MEMBERS)
        {
            for(Record::Members::const_iterator i = rec->dereference().members().begin();
                i != rec->dereference().members().end(); ++i)
            {
                dict->add(new TextValue(i->first), new RefValue(i->second));
            }
        }
        else
        {
            for(Record::Subrecords::const_iterator i = rec->dereference().subrecords().begin();
                i != rec->dereference().subrecords().end(); ++i)
            {
                dict->add(new TextValue(i->first), new RecordValue(i->second));
            }
        }
        return dict;
    }
    
    case AS_NUMBER:
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for AS_NUMBER");
        }
        return new NumberValue(args->at(1).asNumber());

    case AS_TEXT:
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for AS_TEXT");
        }
        return new TextValue(args->at(1).asText());
        
    case LOCAL_NAMESPACE:
    {
        // Collect the namespaces to search.
        Evaluator::Namespaces spaces;
        evaluator.namespaces(spaces);
        if(args->size() != 1)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "No arguments expected for LOCAL_NAMESPACE");
        }
        return new RecordValue(spaces.front());
    }
    
    case SERIALIZE:
    {
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for SERIALIZE");
        }
        std::auto_ptr<BlockValue> data(new BlockValue);
        Writer(*data.get()) << args->at(1);
        return data.release();
    }
    
    case DESERIALIZE:
    {
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for DESERIALIZE");
        }
        const BlockValue* block = dynamic_cast<const BlockValue*>(&args->at(1));
        if(block)
        {
            Reader reader(*block);
            return Value::constructFrom(reader);
        }
        // Alternatively allow deserializing from a text value.
        const TextValue* text = dynamic_cast<const TextValue*>(&args->at(1));
        if(text)
        {
            Reader reader(*text);
            return Value::constructFrom(reader);
        }
        throw WrongArgumentsError("BuiltInExpression::evaluate",
            "deserialize() can operate only on block and text values");
    }
        
    default:
        assert(false);
    }
    return NULL;
}

void BuiltInExpression::operator >> (Writer& to) const
{
    to << SerialId(BUILT_IN) << duint8(_type) << *_arg;
}

void BuiltInExpression::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != BUILT_IN)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized expression was invalid.
        throw DeserializationError("BuiltInExpression::operator <<", "Invalid ID");
    }
    duint8 t;
    from >> t;
    _type = Type(t);
    delete _arg;
    _arg = 0;
    _arg = Expression::constructFrom(from);
}

BuiltInExpression::Type BuiltInExpression::findType(const String& identifier)
{
    struct {
        const char* str;
        Type type;
    } types[] = {
        { "len",            LENGTH },
        { "dictkeys",       DICTIONARY_KEYS },
        { "dictvalues",     DICTIONARY_VALUES },
        { "Text",           AS_TEXT },
        { "Number",         AS_NUMBER },
        { "locals",         LOCAL_NAMESPACE },
        { "members",        RECORD_MEMBERS },
        { "subrecords",     RECORD_SUBRECORDS },
        { "serialize",      SERIALIZE },
        { "deserialize",    DESERIALIZE },
        { NULL,             NONE }
    };
    
    for(duint i = 0; types[i].str; ++i)
    {
        if(identifier == types[i].str)
        {
            return types[i].type;
        }
    }
    return NONE;
}
