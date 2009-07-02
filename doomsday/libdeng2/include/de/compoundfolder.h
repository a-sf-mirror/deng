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

#ifndef LIBDENG2_COMPOUNDFOLDER_H
#define LIBDENG2_COMPOUNDFOLDER_H

#include <de/Folder>

namespace de
{
    /**
     * A special type of folder that merges a set of other folders of any 
     * kind into one. 
     *
     * Files with duplicate names are treated according to the precedence order
     * of the compound members, so that the last folder with the file name in 
     * question is the one that is visible.
     */
    class PUBLIC_API CompoundFolder : public Folder
    {
    public:
        CompoundFolder(const std::string& name = "");
        
        virtual ~CompoundFolder();
        
        void populate();
        
    private:        
    };
}

#endif /* LIBDENG2_COMPOUNDFOLDER_H */
