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

#ifndef LIBDENG2_OPERATOREXPRESSION_H
#define LIBDENG2_OPERATOREXPRESSION_H

#include "../deng.h"
#include "../Operator"
#include "../Expression"

namespace de
{
    class Evaluator;
    class Value;

    /**
     * Evaluates the results of unary and binary operators. 
     * This includes, for example, arithmetic operators, text concatenation,
     * and logical expressions.
     *
     * @ingroup script
     */    
    class OperatorExpression : public Expression
    {
    public:
        /// A unary operation is attempted even though the selected operation cannot
        /// be unary. @ingroup errors
        DEFINE_ERROR(NonUnaryError);
        
        /// A binary operation is attempted even though the selected operation cannot be binary.
        /// @ingroup errors
        DEFINE_ERROR(NonBinaryError);
        
        /// Attempt to assign to a value that cannot be assigned to. @ingroup errors
        DEFINE_ERROR(NotAssignableError);
        
        /// The MEMBER operator receives a non-Record scope on the left side. @ingroup errors
        DEFINE_ERROR(ScopeError);
        
        /// The SLICE operator has invalid arguments. @ingroup errors
        DEFINE_ERROR(SliceError);
        
    public:
        OperatorExpression();
        
        /**
         * Constructor for unary operations (+, -).
         * @param op Operation to perform.
         * @param operand Expression that provides the right-hand operand.
         */
        OperatorExpression(Operator op, Expression* operand);

        /**
         * Constructor for binary operations.
         * @param op Operation to perform.
         * @param leftOperand Expression that provides the left-hand operand.
         * @param rightOperand Expression that provides the right-hand operand.
         */
        OperatorExpression(Operator op, Expression* leftOperand, Expression* rightOperand);

        ~OperatorExpression();

        void push(Evaluator& evaluator, Record* names = 0) const;

        Value* evaluate(Evaluator& evaluator) const;

        /**
         * Verifies that @a value can be used as the l-value of an operator that
         * does assignment.
         *
         * @param value  Value to check.
         */
        static void verifyAssignable(Value* value);

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);         
        
    private:
        /// Performs the slice operation (Python semantics).
        Value* performSlice(Value* leftValue, Value* rightValue) const;

        /// Used to create return values of boolean operations.
        static Value* newBooleanValue(bool isTrue);
                
    private:
        Operator _op;
        Expression* _leftOperand;
        Expression* _rightOperand;
    };
}

#endif /* LIBDENG2_OPERATOREXPRESSION_H */
