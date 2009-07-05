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

#ifdef UNIX
#   include <dlfcn.h>
#endif

#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

#include "de/library.h"

using namespace de;

Library::Library(const std::string& nativePath)
    : handle_(0)
{
    std::cout << "Loading library: " << nativePath << "\n";
#ifdef UNIX
    if((handle_ = dlopen(nativePath.c_str(), RTLD_LAZY)) == NULL)
    {
        // Opening of the dynamic library failed.
        throw LoadError("Library::Library", dlerror());
    }
#endif 

#ifdef WIN32
    if(!(handle_ = reinterpret_cast<void*>(LoadLibrary(nativePath.c_str()))))
    {
        throw LoadError("Library::Library", "LoadLibrary failed: '" + nativePath + "'");
    }
#endif
    std::cout << "...loaded, address is " << handle_ << "\n";
}

Library::~Library()
{
    if(handle_)
    {
        std::cout << "Unloading library with address " << handle_ << "\n";
        
#ifdef UNIX
        // Close the library.
        dlclose(handle_);
#endif

#ifdef WIN32
        FreeLibrary(reinterpret_cast<HMODULE>(handle_));
#endif
    }
}

void* Library::symbol(const std::string& name)
{
    if(!handle_)
    {
        return NULL;
    }
    
    // Already looked up?
    Symbols::iterator found = symbols_.find(name);
    if(found != symbols_.end())
    {
        return found->second;
    }
    
    void* address = 0;

#ifdef UNIX
    address = dlsym(handle_, name.c_str());
#endif

#ifdef WIN32
    address = reinterpret_cast<void*>(
        GetProcAddress(reinterpret_cast<HMODULE>(handle_), name.c_str()));
#endif 

    if(address)
    {
        symbols_[name] = address;
    }
    return address;
}

void* Library::operator [] (const std::string& name)
{
    void* address = symbol(name);
    if(!address)
    {
        throw SymbolMissingError("Library::operator[]", "Symbol '" + name + "' was not found");
    }
    return address;
}
