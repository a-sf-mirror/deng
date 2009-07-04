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

#ifndef LIBDENG2_LIBRARY_H
#define LIBDENG2_LIBRARY_H

#include <de/Error>

#include <string>
#include <map>

namespace de
{
    /**
     * The Library class allows loading shared library files (DLL/so/dylib) and
     * looks up exported symbols in the library.
     */
    class PUBLIC_API Library
    {
    public:
        /// Loading the shared library failed. @ingroup errors
        DEFINE_ERROR(LoadError);
        
        /// A symbol was not found. @ingroup errors
        DEFINE_ERROR(SymbolMissingError);
        
    public:
        /**
         * Constructs a new Library by loading a native shared library.
         * 
         * @param nativePath  Path of the shared library to load.
         */
        Library(const std::string& nativePath);
        
        /**
         * Unloads the shared library.
         */
        virtual ~Library();

        /**
         * Gets the address of an exported symbol.
         *
         * @param name  Name of the exported symbol.
         *
         * @return  A pointer to the symbol. @c NULL, if there is no such symbol.
         */
        void* symbol(const std::string& name);
        
        /**
         * Gets the address of an exported symbol. This will always return a valid
         * pointer to the symbol.
         *
         * @param name  Name of the exported symbol.
         *
         * @return  A pointer to the symbol. If the symbol is not found,  
         *      SymbolMissingError is thrown.
         */
        void* operator [] (const std::string& name);
                
    private:  
        /// Handle to the shared library.
        void* handle_;
        
        typedef std::map<std::string, void*> Symbols;
        Symbols symbols_;
    };
}

#endif /* LIBDENG2_LIBRARY_H */
