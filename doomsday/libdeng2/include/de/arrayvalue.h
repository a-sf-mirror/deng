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

#ifndef LIBDENG2_ARRAYVALUE_H
#define LIBDENG2_ARRAYVALUE_H

#include <de/Value>

#include <vector>

namespace de
{
    /**
     * The ArrayValue class is a subclass of Value that holds a dynamic
     * array of other values. The array is indexed using (integer) numbers.
     */
	class PUBLIC_API ArrayValue : public Value
	{
	public:
		/// Attempt to index the array with indices that are not defined for the array. @ingroup errors
		DEFINE_ERROR(OutOfBoundsError);
		
		/// The index used for accessing the array is of the wrong type. @ingroup errors
        DEFINE_ERROR(IllegalIndexError);

        /// Type for the elements. Public because const access to the elements is public.
		typedef std::vector<Value*> Elements;
		
	public:
		ArrayValue();
        ArrayValue(const ArrayValue& other);
		~ArrayValue();

        /// Const accessor to the array elements.
        const Elements& elements() const { return elements_; }
        
        /**
         * Adds a new Value to the elements of the array. The value is
         * added to the end of the list of elements.
         * @param value Value to add to the array. The array takes 
         *     ownership of the object.
         */
        void add(Value* value);
        
        /**
         * Pops the last element and gives its ownership to the caller.
         *
         * @return Last element of the array. Ownership transferred.
         */
        Value* pop();
        
        /**
         * Inserts a new Value into the elements of the array at an 
         * arbitrary location. The new Value's index will be @c index.
         * @param index  Index for the new Value. 
         * @param value  Value to add. The array takes ownership of the object.
         */ 
        void insert(dint index, Value* value);
        
        /**
         * Replaces an existing Value in the array. The previous value
         * at the index will be destroyed.
         *
         * @param index  Index of the Value.
         * @param value  New value. The array takes ownership of the object.
         */
        void replace(dint index, Value* value);
        
        /**
         * Removes a Value from the array. The Value will be destroyed.
         * @param index  Index of the Value to remove.
         */
        void remove(dint index);
        
        /**
         * Empties the array of all values.
         */
        void clear();
        
        /// Reverse the order of the elements.
        void reverse();
        
        // Implementations of pure virtual methods.
        Value* duplicate() const;
		Text asText() const;
		dsize size() const;		
		const Value* element(const Value& index) const;	
		Value* element(const Value& index);	
        void setElement(const Value& index, Value* value);
        bool contains(const Value& value) const;
        Value* begin();
        Value* next();
        bool isTrue() const;
	    dint compare(const Value& value) const;
        void sum(const Value& value);

        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
	
	private:
        Elements::iterator indexToIterator(dint index);
        Elements::const_iterator indexToIterator(dint index) const;
	
	private:
		Elements elements_;
		
		/// Current position of the iterator.
        dint iteration_;
	};
}

#endif /* LIBDENG2_ARRAYVALUE_H */
