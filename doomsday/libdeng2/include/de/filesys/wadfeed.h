/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2009-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright © 2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_WADFEED_H
#define LIBDENG2_WADFEED_H

#include "../Feed"
#include "../String"

namespace de
{
    class WAD;
    
    /**
     * Produces files and folders that represent the contents of a WAD.
     *
     * @ingroup fs
     */
    class WADFeed : public Feed
    {
    public:
        /**
         * Constructs a new WAD feed.
         *
         * @param wadFile  File where the data comes from.
         */
        WADFeed(File& wadFile);

        virtual ~WADFeed();

        void populate(Folder& folder);
        bool prune(File& file) const;

        /**
         * Returns the WAD that the feed accesses.
         */
        WAD& wad();
                    
    private:
        /// File where the WAD is stored.
        File& _file;

        WAD* _wad;
    };
}

#endif /* LIBDENG2_WADFEED_H */
