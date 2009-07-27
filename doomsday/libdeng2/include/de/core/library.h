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

#include "../deng.h"
#include "../String"

#include <map>

/**
 * Convenience macro for accessing symbols that have a type defined in de::Library
 * with the type name matching the symbol name.
 */
#define SYMBOL(Name) symbol<de::Library::Name>(#Name)

namespace de
{
    class Audio;
    class Map;
    class User;
    class Video;
    class World;
    
    /**
     * The Library class allows loading shared library files (DLL/so/dylib) and
     * looks up exported symbols in the library.
     *
     * Library type identifiers;
     * - <code>library/generic</code>: A shared library with no special function.
     * - <code>deng-plugin/generic</code>: Generic libdeng2 plugin. Loaded always.
     * - <code>deng-plugin/game</code>: The game plugin. Only one of these can be loaded.
     * - <code>deng-plugin/video</code>: Video subsystem. Optional. One of these can be loaded.
     * - <code>deng-plugin/audio</code>: Audio subsystem. Optional. One of these can be loaded.
     *
     * @ingroup core
     */
    class PUBLIC_API Library
    {
    public:
        /// Loading the shared library failed. @ingroup errors
        DEFINE_ERROR(LoadError);
        
        /// A symbol was not found. @ingroup errors
        DEFINE_ERROR(SymbolMissingError);
        
        /// Default type identifier.
        static const char* DEFAULT_TYPE;
        
        // Common function profiles.
        /**
         * Queries the plugin for a type identifier string. If this function is not
         * defined, the identifier defaults to DEFAULT_TYPE.
         *
         * @return  Type identifier string.
         */
        typedef const char* (*deng_LibraryType)(void);
        
        /**
         * Performs any one-time initialization necessary for the usage of the plugin.
         * If this symbol is exported from a shared library, it gets called automatically
         * when the library is loaded. 
         */
        typedef void (*deng_InitializePlugin)(void);
        
        /**
         * Frees resources reserved by the plugin. If this symbol is exported from a 
         * shared library, it gets called automatically when the library is unloaded.
         */
        typedef void (*deng_ShutdownPlugin)(void);
        
        /**
         * Constructs a new instance of a video subsystem.
         *
         * @return  Video subsystem. 
         */
        typedef Video* (*deng_NewVideo)(void);

        /**
         * Constructs a new instance of an audio subsystem.
         *
         * @return  Audio subsystem. 
         */
        typedef Audio* (*deng_NewAudio)(void);
        
        /**
         * Constructs a new game world.
         */
        typedef World* (*deng_NewWorld)(void);
        
        /**
         * Constructs a new game map.
         *
         * @param name  Name of the map.
         */
        typedef Map* (*deng_NewMap)(const char* name);
        
        /**
         * Constructs a new user.
         */
        typedef User* (*deng_NewUser)(void);
        
        typedef dint (*deng_GetInteger)(dint id);
        typedef const char* (*deng_GetString)(dint id);
        typedef void* (*deng_GetAddress)(dint id);
        typedef void (*deng_Ticker)(ddouble tickLength);
        
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
         * Returns the type identifier of the library. This affects how libdeng2
         * will treat the library.
         */
        const std::string& type() const { return type_; }

        /**
         * Gets the address of an exported symbol. This will always return a valid
         * pointer to the symbol.
         *
         * @param name  Name of the exported symbol.
         *
         * @return  A pointer to the symbol.
         */
        void* address(const std::string& name);
        
        template <typename Type>
        Type symbol(const std::string& name) {
            return reinterpret_cast<Type>(address(name));
        }
                
    private:  
        /// Handle to the shared library.
        void* handle_;
        
        typedef std::map<std::string, void*> Symbols;
        Symbols symbols_;
        
        /// Type identifier for the library (e.g., "deng-plugin/generic").
        /// Queried by calling deng_Identifier(), if one is exported in the library.
        String type_;
    };
}

#endif /* LIBDENG2_LIBRARY_H */
