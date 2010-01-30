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

#include "de/File"
#include "de/App"
#include "de/FS"
#include "de/Folder"
#include "de/Date"
#include "de/NumberValue"

#include <sstream>

using namespace de;

File::File(const String& fileName)
    : _parent(0), _originFeed(0), _name(fileName)
{
    _source = this;
    
    // Create the default set of info variables common to all files.
    _info.add(new Variable("name", new Accessor(*this, Accessor::NAME), Accessor::VARIABLE_MODE));
    _info.add(new Variable("path", new Accessor(*this, Accessor::PATH), Accessor::VARIABLE_MODE));
    _info.add(new Variable("type", new Accessor(*this, Accessor::TYPE), Accessor::VARIABLE_MODE));
    _info.add(new Variable("size", new Accessor(*this, Accessor::SIZE), Accessor::VARIABLE_MODE));
    _info.add(new Variable("modifiedAt", new Accessor(*this, Accessor::MODIFIED_AT),
        Accessor::VARIABLE_MODE));
}

File::~File()
{
    FOR_AUDIENCE(Deletion, i) i->fileBeingDeleted(*this);

    flush();
    if(_source != this) 
    {
        // If we own a source, get rid of it.
        delete _source;
        _source = 0;
    }
    if(_parent)
    {
        // Remove from parent folder.
        _parent->remove(this);
    }
    deindex();
}

void File::deindex()
{
    fileSystem().deindex(*this);
}

void File::flush()
{}

void File::clear()
{
    verifyWriteAccess();
}

FS& File::fileSystem()
{
    return App::fileSystem();
}

void File::setOriginFeed(Feed* feed)
{
    _originFeed = feed;
}

const String File::path() const
{
    String thePath = name();
    for(Folder* i = _parent; i; i = i->_parent)
    {
        thePath = i->name() / thePath;
    }
    return "/" + thePath;
}
        
void File::setSource(File* source)
{
    if(_source != this)
    {
        // Delete the old source.
        delete _source;
    }
    _source = source;
}        
        
const File* File::source() const
{
    if(_source != this)
    {
        return _source->source();
    }
    return _source;
}

File* File::source()
{
    if(_source != this)
    {
        return _source->source();
    }
    return _source;
}

void File::setStatus(const Status& status)
{
    // The source file status is the official one.
    if(this != _source)
    {
        _source->setStatus(status);
    }
    else
    {
        _status = status;
    }
}

const File::Status& File::status() const 
{
    if(this != _source)
    {
        return _source->status();
    }
    return _status;
}

void File::setMode(const Mode& newMode)
{
    if(this != _source)
    {
        _source->setMode(newMode);
    }
    else
    {
        _mode = newMode;
    }
}

const File::Mode& File::mode() const 
{
    if(this != _source)
    {
        return _source->mode();
    }
    return _mode;
}

void File::verifyWriteAccess()
{
    if(!mode()[WRITE_BIT])
    {
        /// @throw ReadOnlyError  File is in read-only mode.
        throw ReadOnlyError("File::verifyWriteAccess", path() + " is in read-only mode");
    }
}

File::Size File::size() const
{
    return status().size;
}

void File::get(Offset at, Byte* values, Size count) const
{
    if(at >= size() || at + count > size())
    {
        throw OffsetError("File::get", "Out of range");
    }
}

void File::set(Offset at, const Byte* values, Size count)
{
    verifyWriteAccess();
}

File::Accessor::Accessor(File& owner, Property prop) : _owner(owner), _prop(prop)
{
    update();
}

void File::Accessor::update() const
{
    // We need to alter the value content.
    Accessor* nonConst = const_cast<Accessor*>(this);
    
    std::ostringstream os;    
    switch(_prop)
    {
    case NAME:
        nonConst->setValue(_owner.name());
        break;
        
    case PATH:
        nonConst->setValue(_owner.path());
        break;

    case TYPE:
        nonConst->setValue(_owner.status().type() == File::Status::FILE? "file" : "folder");
        break;

    case SIZE:
        os << _owner.status().size;
        nonConst->setValue(os.str());
        break;

    case MODIFIED_AT:
        os << _owner.status().modifiedAt.asDate();
        nonConst->setValue(os.str());
        break;
    }
}

Value* File::Accessor::duplicateContent() const
{
    if(_prop == SIZE)
    {
        return new NumberValue(asNumber());
    }
    return new TextValue(*this);
}
