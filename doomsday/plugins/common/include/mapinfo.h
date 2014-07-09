/** @file mapinfo.h  Hexen-format MAPINFO definition parsing.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_MAPINFO_H
#define LIBCOMMON_MAPINFO_H
#ifdef __cplusplus

#include "common.h"

class MapInfo : public de::Record
{
public:
    MapInfo();

    void resetToDefaults();
};

/**
 * Populate the MapInfo database by parsing the MAPINFO lump.
 */
void MapInfoParser(Str const *path);

/**
 * @param mapUri  Identifier of the map to lookup info for. Can be @c 0 in which
 *                case the info for the @em current map will be returned (if set).
 *
 * @return  MAPINFO data for the specified @a mapUri; otherwise @c 0 (not found).
 */
MapInfo *P_MapInfo(de::Uri const *mapUri = 0);

/**
 * Translates a warp map number to unique map identifier, if possible.
 *
 * @note This should only be used where necessary for compatibility reasons as
 * the "warp translation" mechanic is redundant in the context of Doomsday's
 * altogether better handling of map resources and their references. Instead,
 * use the map URI mechanism.
 *
 * @param map  The warp map number to translate.
 *
 * @return The unique map identifier associated with the warp map number given;
 * otherwise an identifier with a empty path.
 */
de::Uri P_TranslateMapIfExists(uint map);

/**
 * Translates a warp map number to unique map identifier. Always returns a valid
 * map identifier.
 *
 * @note This should only be used where necessary for compatibility reasons as
 * the "warp translation" mechanic is redundant in the context of Doomsday's
 * altogether better handling of map resources and their references. Instead,
 * use the map URI mechanism.
 *
 * @param map  The warp map number to translate.
 *
 * @return The unique identifier of the map given a warp map number. If the map
 * is not found a URI to the first available map is returned (i.e., Maps:MAP01)
 */
de::Uri P_TranslateMap(uint map);

#endif // __cplusplus
#endif // LIBCOMMON_MAPINFO_H