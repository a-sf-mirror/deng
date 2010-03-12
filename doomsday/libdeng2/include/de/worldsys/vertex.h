/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG2_VERTEX_H__
#define LIBDENG2_VERTEX_H__

#include "deng.h"

#include "../Error"
#include "../HalfEdgeDS"

namespace de
{
    class LineDef;

    struct shadowvert_t {
        Vector2d inner;
        Vector2d extended;
    };

    struct lineowner_t {
        LineDef* lineDef;
        lineowner_t* link[2]; // {prev, next} (i.e. {anticlk, clk}).
        dbinangle angle; // between this and next clockwise.
        shadowvert_t shadowOffsets;

        lineowner_t& prev() const { return *link[0]; }
        lineowner_t& next() const { return *link[1]; }
    };

    class MVertex
    {
    public:
        /// Attempt made to modify MVertex. @ingroup errors
        DEFINE_ERROR(WriteError);
        /// Unknown property name was given. @ingroup errors
        DEFINE_ERROR(UnknownPropertyError);

    public:
        duint numLineOwners; // Number of line owners.
        lineowner_t* lineOwners; // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.

        // Vertex index. Always valid after loading and pruning of unused
        // vertices has occurred.
        duint index;

        // Reference count. When building normal node info, unused vertices
        // will be pruned.
        duint refCount;

        // Usually NULL, unless this vertex occupies the same location as a
        // previous vertex. Only used during the pruning phase.
        Vertex* equiv;

        /**
         * Updates all the shadow offsets.
         *
         * @pre Lineowner rings MUST be set up.
         */
        void updateShadowOffsets();

        /**
         * Get the value of a vertex property, selected by DMU_* name.
         */
        //bool getProperty(setargs_t* args) const;

        /**
         * Update the vertex, property is selected by DMU_* name.
         */
        //bool setProperty(const setargs_t* args);
    };
}

#endif /* LIBDENG2_VERTEX_H__ */
