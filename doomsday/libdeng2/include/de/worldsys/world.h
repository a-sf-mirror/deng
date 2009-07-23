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

#ifndef LIBDENG2_WORLD_H
#define LIBDENG2_WORLD_H

#include <de/ISerializable>
#include <de/Record>

#include <string>

/**
 * @defgroup World
 * 
 * The world subsystem takes care of the game world and the players in the world.
 */

namespace de
{
    class Map;
    
    /**
     * Base class for the game world. The game plugin is responsible for creating concrete
     * instances of the World. The game plugin can extend this with whatever information it
     * needs.
     *
     * @ingroup world
     */
    class World : public ISerializable
    {
    public:
        World();
        
        virtual ~World();
        
        /**
         * Loads a map and prepares it for play.
         *
         * @param name  Name of the map.
         */
        virtual void setMap(const std::string& name);
        
        const Record& info() const { return info_; }

        Record& info() { return info_; }

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    private:
        Record info_;
        
        /// The current map.
        Map* map_;
    };
}

#endif /* LIBDENG2_WORLD_H */
