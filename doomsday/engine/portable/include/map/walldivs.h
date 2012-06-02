/**
 * @file walldivs.h
 * Map wall divisions. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_WALLDIVS_H
#define LIBDENG_MAP_WALLDIVS_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct walldivs_s;
struct plane_s;
struct hedge_s;

typedef enum {
    DT_PLANEINTERCEPT = 0,
    DT_ABSOLUTEHEIGHT
} walldivnodetype_t;

typedef struct walldivnode_s {
    struct walldivs_s* divs;
    walldivnodetype_t type;
    union {
        struct {
            struct plane_s* plane;
            struct hedge_s* hedge;
        } intercept;
        coord_t absoluteHeight;
    } data;
} walldivnode_t;

struct walldivs_s* WallDivNode_WallDivs(walldivnode_t* node);
coord_t WallDivNode_Height(walldivnode_t* node);
walldivnode_t* WallDivNode_Next(walldivnode_t* node);
walldivnode_t* WallDivNode_Prev(walldivnode_t* node);

/// Maximum number of walldivnode_ts in a walldivs_t dataset.
#define WALLDIVS_MAX_NODES          64

typedef struct walldivs_s {
    uint num;
    struct walldivnode_s nodes[WALLDIVS_MAX_NODES];
    int lastBuildCount; // Frame number last time this data was updated.
} walldivs_t;

uint WallDivs_Size(const walldivs_t* wallDivs);
walldivnode_t* WallDivs_First(walldivs_t* wallDivs);
walldivnode_t* WallDivs_Last(walldivs_t* wallDivs);
walldivnode_t* WallDivs_AtHeight(walldivs_t* wallDivs, coord_t height);

walldivs_t* WallDivs_AppendPlaneIntercept(walldivs_t* wd, struct plane_s* plane, struct hedge_s* hedge);
walldivs_t* WallDivs_AppendAbsoluteHeight(walldivs_t* wd, coord_t height);

/**
 * Ensure the divisions are sorted (in ascending Z order).
 */
void WallDivs_AssertSorted(walldivs_t* wallDivs);

/**
 * Ensure the divisions do not exceed the specified range.
 */
void WallDivs_AssertInRange(walldivs_t* wallDivs, coord_t low, coord_t hi);

#if _DEBUG
void WallDivs_DebugPrint(walldivs_t* wallDivs);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_WALLDIVS_H
