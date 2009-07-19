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

#ifndef LIBDENG2_RECORD_H
#define LIBDENG2_RECORD_H

#include <de/ISerializable>
#include <de/Variable>

#include <iostream>
#include <map>

namespace de
{
    /**
     * A Record is a set of variables.
     *
     * @see http://en.wikipedia.org/wiki/Record_(computer_science)
     *
     * @ingroup data
     */
    class Record : public ISerializable
    {
    public:
        /// Unknown variable name was given. @ingroup errors
        DEFINE_ERROR(NotFoundError);

        /// All variables in the record must have a name. @ingroup errors
        DEFINE_ERROR(UnnamedVariableError);
        
        typedef std::map<std::string, Variable*> Members;
        
    public:
        Record();
        virtual ~Record();

        /**
         * Deletes all the variables in the record.
         */
        void clear();
        
        /**
         * Adds a new variable to the record. 
         *
         * @param variable  Variable to add. Record gets ownership. 
         *      The variable must have a name.
         *
         * @return @c variable, for convenience.
         */
        Variable* add(Variable* variable);

        /**
         * Removes a variable from the record.
         *
         * @param variable  Variable owned by the record.
         *
         * @return  Caller gets ownership of the removed variable.
         */
        Variable* remove(Variable& variable);
        
        /**
         * Looks up a variable in the record.
         *
         * @param name  Variable name.
         *
         * @return  Variable.
         */
        Variable& operator [] (const std::string& name);
        
        /**
         * Looks up a variable in the record.
         *
         * @param name  Variable name.
         *
         * @return  Variable (non-modifiable).
         */
        const Variable& operator [] (const std::string& name) const;

        /**
         * Returns a non-modifiable map of the members.
         */
        const Members& members() const { return members_; }
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    private:  
        Members members_;
    };
    
    /// Converts the record into a human-readable text representation.
    std::ostream& operator << (std::ostream& os, const Record& record);
}

#endif /* LIBDENG2_RECORD_H */
