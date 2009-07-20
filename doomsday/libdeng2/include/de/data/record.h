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
#include <de/Value>

#include <iostream>
#include <map>

namespace de
{
    /**
     * A Record is a set of variables. A record may have any number of subrecords.
     * Note that the members of a record do not have an order.
     *
     * @see http://en.wikipedia.org/wiki/Record_(computer_science)
     *
     * @ingroup data
     */
    class PUBLIC_API Record : public ISerializable
    {
    public:
        /// Unknown variable name was given. @ingroup errors
        DEFINE_ERROR(NotFoundError);

        /// All variables and subrecords in the record must have a name. @ingroup errors
        DEFINE_ERROR(UnnamedError);
        
        typedef std::map<std::string, Variable*> Members;
        typedef std::map<std::string, Record*> Subrecords;
        
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
        Variable& add(Variable* variable);

        /**
         * Removes a variable from the record.
         *
         * @param variable  Variable owned by the record.
         *
         * @return  Caller gets ownership of the removed variable.
         */
        Variable* remove(Variable& variable);
        
        /**
         * Adds a number variable to the record. The variable is set up to only accept
         * number values.
         *
         * @param variableName  Name of the variable.
         * @param number  Value of the variable.
         *
         * @return  The number variable.
         */
        Variable& addNumber(const std::string& variableName, const Value::Number& number);
        
        /**
         * Adds a text variable to the record. The variable is set up to only accept
         * text values.
         *
         * @param variableName  Name of the variable.
         * @param text  Value of the variable.
         *
         * @return  The text variable.
         */
        Variable& addText(const std::string& variableName, const Value::Text& text);
        
        /**
         * Adds an array variable to the record. The variable is set up to only accept
         * array values.
         *
         * @param variableName  Name of the variable.
         *
         * @return  The array variable.
         */
        Variable& addArray(const std::string& variableName);

        /**
         * Adds a dictionary variable to the record. The variable is set up to only accept
         * dictionary values.
         *
         * @param variableName  Name of the variable.
         *
         * @return  The dictionary variable.
         */
        Variable& addDictionary(const std::string& variableName);
        
        /**
         * Adds a new subrecord to the record. 
         *
         * @param name  Name to use for the subrecord. This must be a valid variable name.
         * @param subrecord  Record to add. This record gets ownership
         *      of the subrecord. The variable must have a name.
         *
         * @return @c variable, for convenience.
         */
        Record& add(const std::string& name, Record* subrecord);

        /**
         * Adds a new empty subrecord to the record.
         *
         * @param name  Name to use for the subrecord. This must be a valid variable name.
         *
         * @return  The new subrecord.
         */
        Record& addRecord(const std::string& name);

        /**
         * Removes a subrecord from the record.
         * 
         * @param subrecord  Subrecord owned by this record.
         *
         * @return  Caller gets ownership of the removed record.
         */
        Record* remove(const std::string& name);
        
        /**
         * Looks up a variable in the record. Variables in subrecords can be accessed
         * using the path notation: subrecord-name/variable-name.
         *
         * @param name  Variable name.
         *
         * @return  Variable.
         */
        Variable& operator [] (const std::string& name);
        
        /**
         * Looks up a variable in the record. Variables in subrecords can be accessed
         * using the path notation: subrecord-name/variable-name.
         *
         * @param name  Variable name.
         *
         * @return  Variable (non-modifiable).
         */
        const Variable& operator [] (const std::string& name) const;

        /**
         * Looks up a subrecord in the record.
         *
         * @param name  Name of the subrecord.
         *
         * @return  Subrecord.
         */
        Record& subrecord(const std::string& name);

        /**
         * Looks up a subrecord in the record.
         *
         * @param name  Name of the subrecord.
         *
         * @return  Subrecord (non-modifiable).
         */
        const Record& subrecord(const std::string& name) const;

        /**
         * Returns a non-modifiable map of the members.
         */
        const Members& members() const { return members_; }
        
        /**
         * Returns a non-modifiable map of the subrecords.
         */
        const Subrecords& subrecords() const { return subrecords_; }
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    private:  
        Members members_;
        Subrecords subrecords_;
    };
    
    /// Converts the record into a human-readable text representation.
    std::ostream& operator << (std::ostream& os, const Record& record);
}

#endif /* LIBDENG2_RECORD_H */
