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

/**
 * r_seg.c: World segs.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Update the seg, property is selected by DMU_* name.
 */
boolean Seg_SetProperty(seg_t* seg, const setargs_t* args)
{
    switch(args->prop)
    {
    default:
        Con_Error("Seg_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a seg property, selected by DMU_* name.
 */
boolean Seg_GetProperty(const seg_t* seg, setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_VERTEX0:
        {
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_VERTEX, seg->hEdge->HE_v1);
        DMU_GetValue(DMT_SEG_VERTEX1, &r, args, 0);
        break;
        }
    case DMU_VERTEX1:
        {
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_VERTEX, seg->hEdge->HE_v2);
        DMU_GetValue(DMT_SEG_VERTEX2, &r, args, 0);
        break;
        }
    case DMU_LENGTH:
        DMU_GetValue(DMT_SEG_LENGTH, &seg->length, args, 0);
        break;
    case DMU_OFFSET:
        DMU_GetValue(DMT_SEG_OFFSET, &seg->offset, args, 0);
        break;
    case DMU_SIDEDEF:
        {
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_SIDEDEF, seg->sideDef);
        DMU_GetValue(DMT_SEG_SIDEDEF, &r, args, 0);
        break;
        }
    case DMU_LINEDEF:
        {
        void* ptr = NULL;
        if(seg->sideDef)
            ptr = DMU_GetObjRecord(DMU_LINEDEF, seg->sideDef->lineDef);
        DMU_GetValue(DMT_SEG_LINEDEF, &ptr, args, 0);
        break;
        }
    case DMU_FRONT_SECTOR:
        {
        subsector_t* subsector = (subsector_t*) seg->hEdge->face->data;
        dmuobjrecord_t* r = (subsector->sector && seg->sideDef)?
            DMU_GetObjRecord(DMU_SECTOR, subsector->sector) : NULL;
        DMU_GetValue(DMT_SEG_FRONTSECTOR, &r, args, 0);
        break;
        }
    case DMU_BACK_SECTOR:
        {
        void* ptr = NULL;

        if(seg->hEdge->twin)
        {
            subsector_t* subsector = (subsector_t*) seg->hEdge->twin->face->data;

            /**
             * The sector and the sidedef are checked due to the possibility
             * of the "back-sided window effect", which the games are not
             * currently aware of.
             */
            if(subsector->sector && ((seg_t*) seg->hEdge->twin->data)->sideDef)
                ptr = DMU_GetObjRecord(DMU_SECTOR, subsector->sector);
        }

        DMU_GetValue(DMT_SEG_BACKSECTOR, &ptr, args, 0);
        break;
        }
    case DMU_ANGLE:
        DMU_GetValue(DMT_SEG_ANGLE, &seg->angle, args, 0);
        break;
    default:
        Con_Error("Seg_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
