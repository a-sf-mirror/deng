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

#include "de/Record"
#include "de/Variable"
#include "de/Reader"
#include "de/Writer"
#include "de/Value"

using namespace de;

Record::Record()
{}

Record::~Record()
{
    clear();
}

void Record::clear()
{
    if(members_.empty())
    {
        return;
    }
    for(Members::iterator i = members_.begin(); i != members_.end(); ++i)
    {
        delete i->second;
    }
    members_.clear();
}

Variable* Record::add(Variable* variable)
{
    std::auto_ptr<Variable> var(variable);
    if(variable->name().empty())
    {
        throw UnnamedVariableError("Record::add", "All variables in record must have a name");
    }
    members_[variable->name()] = var.release();
    return variable;
}

Variable* Record::remove(Variable& variable)
{
    members_.erase(variable.name());
    return &variable;
}
    
Variable& Record::operator [] (const std::string& name)
{
    return const_cast<Variable&>((*const_cast<const Record*>(this))[name]);
}
    
const Variable& Record::operator [] (const std::string& name) const
{
    Members::const_iterator found = members_.find(name);
    if(found != members_.end())
    {
        return *found->second;
    }
    throw NotFoundError("Record::operator []", "'" + name + "' not found");
}

void Record::operator >> (Writer& to) const
{
    to << duint(members_.size());
    for(Members::const_iterator i = members_.begin(); i != members_.end(); ++i)
    {
        to << *i->second;
    }
}
    
void Record::operator << (Reader& from)
{
    duint count = 0;
    from >> count;
    clear();
    while(count-- > 0)
    {
        std::auto_ptr<Variable> var(new Variable());
        from >> *var.get();
        add(var.release());
    }
}
    
std::ostream& de::operator << (std::ostream& os, const Record& record)
{
    for(Record::Members::const_iterator i = record.members().begin(); 
        i != record.members().end(); ++i)
    {
        os << i->first << ": ";
        os << i->second->value().asText() << "\n";
    }
    return os;
}
