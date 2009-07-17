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

#include "de/library.h"

#ifdef UNIX
#   include <dlfcn.h>
#endif

#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

using namespace de;

const char* Library::DEFAULT_TYPE = "library/generic";

Library::Library(const std::string& nativePath)
    : handle_(0), type_(DEFAULT_TYPE)
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

    try 
    {
        // Automatically call the initialization function, if one exists.
        SYMBOL(deng_InitializePlugin)();
    }
    catch(const SymbolMissingError&)
    {}
    
    try
    {
        // Query the type identifier.
        type_ = SYMBOL(deng_LibraryType)();
    }
    catch(const SymbolMissingError&)
    {}
}

Library::~Library()
{
    if(handle_)
    {
        try 
        {
            // Automatically call the shutdown function, if one exists.
            SYMBOL(deng_ShutdownPlugin)();
        }
        catch(const SymbolMissingError&)
        {}

#ifdef UNIX
        // Close the library.
        dlclose(handle_);
#endif

#ifdef WIN32
        FreeLibrary(reinterpret_cast<HMODULE>(handle_));
#endif
    }
}

void* Library::address(const std::string& name)
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
    
    void* ptr = 0;

#ifdef UNIX
    ptr = dlsym(handle_, name.c_str());
#endif

#ifdef WIN32
    ptr = reinterpret_cast<void*>(
        GetProcAddress(reinterpret_cast<HMODULE>(handle_), name.c_str()));
#endif 

    if(!ptr)
    {
        throw SymbolMissingError("Library::symbol", "Symbol '" + name + "' was not found");
    }

    symbols_[name] = ptr;
    return ptr;
}
