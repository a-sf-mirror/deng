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

#ifndef LIBDENG2_STRINGTABLE_H
#define LIBDENG2_STRINGTABLE_H

#include "../String"

#include <vector>

namespace de
{
    /**
     * "String interning" container.
     */
    class LIBDENG2_API StringTable
    {
    private:
        /// The string table itself.
        typedef std::vector<String> Strings;
        Strings _strings;

    public:
        /// (Unique) string identifier.
        typedef Strings::size_type StringId;
        enum {
            NONINDEX = 0 // Not a valid StringId.
        };

        StringTable();
        ~StringTable();

        /**
         * Insert a new string into the table.
         *
         * @return          Identifier of the inserted string.
         */
        StringId insert(const String& name);
        StringId insert(const char* name);

        /**
         * Lookup the identifier of a string in the table.
         *
         * @return          Identifier of the located string else @c NONINDEX if not found.
         */
        StringId find(const String& name);
        StringId find(const char* name);

        /**
         * Returns the string table back to its initial (empty) state.
         */
        void clear();

        /**
         * Locate a string in the table by its identifier.
         *
         * @return          String associated with the specified identifier.
         */
        const String& get(StringId Id);
    };
}

#endif /* LIBDENG2_STRINGTABLE_H */
