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

#ifndef LIBDENG2_VARIABLE_H
#define LIBDENG2_VARIABLE_H

#include <de/ISerializable>
#include <de/String>
#include <de/Flag>

namespace de
{
    class Value;
    
    /**
     * Variable stores a value and is identified by a name. Variables are stored
     * in Record instances.
     *
     * The variable's behavior is defined by the mode flags.
     *
     * @ingroup data
     */
    class Variable : public ISerializable
    {
    public:
        /// There was an attempt to change the value of a read-only variable. @ingroup errors
        DEFINE_ERROR(ReadOnlyError);
        
        /// An invalid value type was used. The mode flags denied using a value of the
        /// given type with the variable. @ingroup errors
        DEFINE_ERROR(InvalidError);
        
        /** @name Mode Flags */
        //@{
        DEFINE_FLAG(READ_ONLY, 0);
        DEFINE_FLAG(NONE, 1);
        DEFINE_FLAG(NUMBER, 2);
        DEFINE_FLAG(TEXT, 3);
        DEFINE_FLAG(ARRAY, 4);
        DEFINE_FINAL_FLAG(DICTIONARY, 5, Mode);
        //@}
        
        /// The default mode allows reading and writing all types of values, 
        /// including NoneValue.
        static const Flag DEFAULT_MODE = NONE | NUMBER | TEXT | ARRAY | DICTIONARY;
        
    public:
        /**
         * Constructs a new variable.
         *
         * @param name  Name for the variable.
         * @param initial  Initial value. Variable gets ownership. If no value is given here,
         *      a NoneValue will be created for the variable.
         * @param mode  Mode flags.
         */
        Variable(const std::string& name = "", Value* initial = 0, const Mode& mode = DEFAULT_MODE);
            
        virtual ~Variable();
        
        /**
         * Returns the name of the variable.
         */
        const String& name() const { return name_; }
        
        /**
         * Sets the value of the variable.
         *
         * @param v  New value. Variable gets ownership.
         */
        void set(Value* v);
        
        /**
         * Sets the value of the variable.
         *
         * @param v  New value. Variable takes a copy of this.
         */
        void set(const Value& v);
        
        /**
         * Returns the value of the variable.
         */
        const Value& value() const;
        
        /**
         * Checks that a value is valid, checking what is allowed in the mode
         * flags.
         *
         * @param v  Value to check.
         *
         * @return @c true, if the value is valid. @c false otherwise.
         */
        bool isValid(const Value& v) const;
        
        /**
         * Verifies that a value is valid, checking against what is allowed in the 
         * mode flags. If not, an exception is thrown.
         *
         * @param v  Value to test.
         */
        void verifyValid(const Value& v) const;
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
    private:        
        String name_;

        /// Mode flags.        
        Mode mode_;
        
        /// Value of the variable.
        Value* value_;
    };
}

#endif /* LIBDENG2_VARIABLE_H */
