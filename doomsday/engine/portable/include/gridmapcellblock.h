/**
 * @file gridmapcellblock.h
 * GridmapCellBlock. @ingroup map
 *
 * @authors Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_DATA_GRIDMAP_CELLBLOCK_H
#define LIBDENG_DATA_GRIDMAP_CELLBLOCK_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint GridmapCoord;
typedef GridmapCoord GridmapCell[2];
typedef const GridmapCoord const_GridmapCell[2];

/**
 * GridmapCellBlock. Handy POD structure for representing a rectangular range
 * of cells (a "cell block").
 */
typedef struct gridmapcellblock_s {
    union {
        struct {
            GridmapCoord minX;
            GridmapCoord minY;
            GridmapCoord maxX;
            GridmapCoord maxY;
        };
        struct {
            GridmapCell min;
            GridmapCell max;
        };
        struct {
            GridmapCell box[2];
        };
    };
#ifdef __cplusplus
    gridmapcellblock_s(GridmapCoord _minX = 0, GridmapCoord _minY = 0, GridmapCoord _maxX = 0, GridmapCoord _maxY = 0)
        : minX(_minX), minY(_minY), maxX(_maxX), maxY(_maxY) {}

    gridmapcellblock_s& operator = (const gridmapcellblock_s& other)
    {
        minX = other.minX;
        minY = other.minY;
        maxX = other.maxX;
        maxY = other.maxY;
        return *this;
    }

    gridmapcellblock_s& setCoords(const_GridmapCell newMin, const_GridmapCell newMax)
    {
        if(newMin)
        {
            minX = newMin[0];
            minY = newMin[1];
        }
        if(newMax)
        {
            maxX = newMax[0];
            maxY = newMax[1];
        }
        return *this;
    }

    gridmapcellblock_s& setCoords(GridmapCoord newMinX, GridmapCoord newMinY, GridmapCoord newMaxX, GridmapCoord newMaxY)
    {
        minX = newMinX;
        minY = newMinY;
        maxX = newMaxX;
        maxY = newMaxY;
        return *this;
    }
#endif
} GridmapCellBlock;

/**
 * Initialize @a block using the specified coordinates.
 */
GridmapCellBlock* GridmapBlock_SetCoords(GridmapCellBlock* block, const_GridmapCell min, const_GridmapCell max);
GridmapCellBlock* GridmapBlock_SetCoordsXY(GridmapCellBlock* block, GridmapCoord minX, GridmapCoord minY, GridmapCoord maxX, GridmapCoord maxY);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_DATA_GRIDMAP_CELLBLOCK_H
