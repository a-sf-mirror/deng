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

#include "de/CatchStatement"
#include "de/NameExpression"
#include "de/Writer"
#include "de/Reader"
#include "de/Context"
#include "de/TextValue"
#include "de/RefValue"

using namespace de;

CatchStatement::CatchStatement(ArrayExpression* args) : _args(args)
{
    if(!_args)
    {
        _args = new ArrayExpression;
    }
}

CatchStatement::~CatchStatement()
{
    delete _args;
}

void CatchStatement::execute(Context& context) const
{
    context.proceed();
}

bool CatchStatement::isFinal() const
{
    return flags[FINAL_BIT];
}

bool CatchStatement::matches(const Error& err) const
{
    if(!_args->size())
    {
        // Not specified, so catches all.
        return true;
    }
    
    const NameExpression* name = dynamic_cast<const NameExpression*>(&_args->at(0));
    assert(name != NULL);
    
    return (name->identifier() == err.name() ||
        String(err.name()).endsWith("_" + name->identifier()));
}

void CatchStatement::executeCatch(Context& context, const Error& err) const
{
    if(_args->size() > 1)
    {
        // Place the error message into the specified variable.
        RefValue& ref = context.evaluator().evaluateTo<RefValue>(&_args->at(1));
        ref.assign(new TextValue(err.asText()));
    }
    
    // Begin the catch compound.
    context.start(_compound.firstStatement(), next());
}

void CatchStatement::operator >> (Writer& to) const
{
    to << SerialId(CATCH) << duint8(flags.to_ulong()) << *_args << _compound;
}

void CatchStatement::operator << (Reader& from)
{
    SerialId id;
    from >> id;
    if(id != CATCH)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized statement was invalid.
        throw DeserializationError("CatchStatement::operator <<", "Invalid ID");
    }
    duint8 f;
    from >> f;
    flags = f;
    from >> *_args >> _compound;
}
