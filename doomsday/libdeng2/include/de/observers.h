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

#ifndef LIBDENG2_OBSERVERS_H
#define LIBDENG2_OBSERVERS_H

#include <de/deng.h>

#include <set>

namespace de
{
    template <typename Type>
    class Observers
    {
    public:
        typedef std::set<Type*> Members;
        typedef typename Members::iterator iterator;
        typedef typename Members::const_iterator const_iterator;
        typedef typename Members::size_type size_type;
        
    public:
        Observers() : members_(0) {}

        virtual ~Observers() {
            delete members_;
        }
        
        /// Add an observer into the set. The set does not receive
        /// ownership of the observer instance.
        void add(Type* observer) {
            checkExists();
            members_->insert(observer);
        }            
        
        void remove(Type* observer) {
            if(members_) {
                members_->erase(observer);
            }
            if(!members_->size()) {
                delete members_;
                members_ = 0;
            }
        }
        
        size_type size() const {
            if(members_) {
                return members_->size();
            }
            return 0;
        }
        
        iterator begin() {
            checkExists();
            return members_->begin();
        }

        iterator end() {
            checkExists();
            return members_->end();
        }
        
        const_iterator begin() const {
            if(!members_) {
                return 0;
            }
            return members_->begin();
        }

        const_iterator end() const {
            if(!members_) {
                return 0;
            }
            return members_->end();
        }

    protected:
        void checkExists() {
            if(!members_) {
                members_ = new Members;
            }            
        }
        
    private:
        Members* members_;
    };
};

#endif /* LIBDENG2_OBSERVERS_H */
