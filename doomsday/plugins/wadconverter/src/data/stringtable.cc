/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <algorithm>
#include <iterator>
#include <assert.h>

#include "StringTable"

using namespace de;

StringTable::StringTable()
{}

StringTable::~StringTable()
{
    clear();
}

StringTable::StringId StringTable::find(const std::string& name)
{
    Strings::iterator it = std::find(_strings.begin(), _strings.end(), name);
    if(it != _strings.end())
        return (static_cast<StringId> (std::distance(_strings.begin(), it))) + 1; // 1-based index.
    return StringTable::NONINDEX;
}

StringTable::StringId StringTable::find(const char* name)
{
    assert(name && name[0]);
    std::string s = name;
    return find(s);
}

StringTable::StringId StringTable::insert(const std::string& name)
{
    StringId Id;
    if((Id = find(name)) != StringTable::NONINDEX)
        return Id;
    _strings.push_back(name);
    return static_cast<StringId> (_strings.size()); // 1-based index.
}

StringTable::StringId StringTable::insert(const char* name)
{
    assert(name && name[0]);
    std::string s = name;
    return insert(s);
}

const std::string& StringTable::get(StringTable::StringId Id)
{
    assert(Id != StringTable::NONINDEX);
    if(Id - 1 > _strings.size())
        throw std::out_of_range("Id outside valid range");
    return _strings[Id-1];
}

void StringTable::clear()
{
    _strings.clear();
}
