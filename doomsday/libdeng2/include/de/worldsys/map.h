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

#ifndef LIBDENG2_MAP_H
#define LIBDENG2_MAP_H

#include <de/ISerializable>
#include <de/Enumerator>
#include <de/Record>
#include <de/Object>
#include <de/String>

#include <map>

namespace de
{
    /**
     * Contains everything that makes a map work: sectors, lines, scripts, 
     * objects, etc.
     *
     * @ingroup world
     */
    class PUBLIC_API Map : public ISerializable
    {
    public:
        /**
         * Constructs a map.
         *
         * @param name  Identifier of the map. The resources of the map are
         *      located based on this identifier. If not specified, the instance
         *      becomes a blank map that is expected to be deserialized from
         *      somewhere.
         */
        Map(const std::string& name = "");
        
        virtual ~Map();

        const Record& info() const { return info_; }

        Record& info() { return info_; }

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    private:
        /// Name of the map.
        String name_;
        
        /// Map-specific information. Lost when the map changes.
        Record info_;
        
        /// Generates ids for objects.
        Enumerator objectEnum_;
        
        typedef std::map<Object::Id, Object*> Objects;
        Objects objects_;
    };
}

#endif /* LIBDENG2_MAP_H */
