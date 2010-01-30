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

#ifndef LIBDENG2_REFVALUE_H
#define LIBDENG2_REFVALUE_H

#include "../Value"
#include "../Variable"

namespace de
{
    /**
     * References a Variable. Operations done on a RefValue are actually performed on
     * the variable's value.
     *
     * @ingroup data
     */
    class LIBDENG2_API RefValue : public Value, 
                                  OBSERVES(Variable, Deletion)
    {
    public:
        /// Attempt to dereference a NULL variable. @ingroup errors
        DEFINE_ERROR(NullError);
        
    public:
        /**
         * Constructs a new reference to a variable.
         *
         * @param variable  Variable.
         */
        RefValue(Variable* variable = 0);
        
        virtual ~RefValue();
        
        /**
         * Returns the variable this reference points to.
         */
        Variable* variable() const { return _variable; }

        void verify() const;
        
        Value& dereference();
        
        const Value& dereference() const;
        
        Value* duplicate() const;
        Number asNumber() const;
        Text asText() const;
        dsize size() const;
        const Value& element(const Value& index) const;
        Value& element(const Value& index);
        void setElement(const Value& index, Value* elementValue);
        bool contains(const Value& value) const;
        Value* begin();
        Value* next();
        bool isTrue() const;
        bool isFalse() const;
        dint compare(const Value& value) const;
        void negate();
        void sum(const Value& value);
        void subtract(const Value& subtrahend);
        void divide(const Value& divisor);
        void multiply(const Value& value);
        void modulo(const Value& divisor);
        void assign(Value* value);
        void call(Process& process, const Value& arguments) const;

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);

        // Observes Variable deletion.
        void variableBeingDeleted(Variable& variable);
        
    public:
        Variable* _variable;
    };
}

#endif /* LIBDENG2_REFVALUE_H */
