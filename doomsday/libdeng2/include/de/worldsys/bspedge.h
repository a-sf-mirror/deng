/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
 * Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#ifndef LIBDENG_HALFEDGEINFO_H
#define LIBDENG_HALFEDGEINFO_H

#include "deng.h"

#include "../Vector"
#include "../LineDef"
#include "../BSPSuperBlock"

namespace de
{
    struct HalfEdgeInfo
    {
        static const dbyte FRONT = 0;
        static const dbyte BACK = 1;

        /// The SuperBlock that contains this half-edge, or NULL if the half-edge
        /// is no longer in any SuperBlock (e.g., now in a leaf).
        SuperBlockmap* blockmap;

        // Precomputed data for faster calculations.
        Vector2d pDelta;
        ddouble pLength;
        ddouble pAngle;
        ddouble pPara;
        ddouble pPerp;

        // LineDef that this half-edge goes along, or NULL if miniseg.
        LineDef* lineDef;

        // LineDef that this half-edge initially comes from.
        // For "real" half-edges, this is just the same as the 'linedef' field
        // above. For "miniedges", this is the linedef of the partition line.
        LineDef* sourceLine;

        Sector* sector; // Adjacent sector or, NULL if minihedge / twin on single sided linedef.

        /// @c true = this is on the backside of the edge.
        bool back;
    };
}

#endif /* LIBDENG_HALFEDGEINFO_H */
