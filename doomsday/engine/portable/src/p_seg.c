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
boolean Seg_SetProperty(hedge_t* hEdge, const setargs_t* args)
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
boolean Seg_GetProperty(const hedge_t* hEdge, setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_VERTEX0:
        {
        seg_t* seg = (seg_t*) hEdge->data;
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_VERTEX, hEdge->HE_v1);
        DMU_GetValue(DMT_HEDGE_V, &r, args, 0);
        break;
        }
    case DMU_VERTEX1:
        {
        seg_t* seg = (seg_t*) hEdge->data;
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_VERTEX, hEdge->HE_v2);
        DMU_GetValue(DMT_HEDGE_V, &r, args, 0);
        break;
        }
    case DMU_LENGTH:
        DMU_GetValue(DMT_HEDGE_LENGTH, &((seg_t*) hEdge->data)->length, args, 0);
        break;
    case DMU_OFFSET:
        DMU_GetValue(DMT_HEDGE_OFFSET, &((seg_t*) hEdge->data)->offset, args, 0);
        break;
    case DMU_SIDEDEF:
        {
        seg_t* seg = (seg_t*) hEdge->data;
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_SIDEDEF, seg->sideDef);
        DMU_GetValue(DMT_HEDGE_SIDEDEF, &r, args, 0);
        break;
        }
    case DMU_LINEDEF:
        {
        void*               ptr = NULL;

        if(hEdge->data)
        {
        seg_t*              seg = (seg_t*) hEdge->data;

        if(seg->sideDef)
            ptr = DMU_GetObjRecord(DMU_LINEDEF, seg->sideDef->lineDef);
        }
        DMU_GetValue(DMT_HEDGE_LINEDEF, &ptr, args, 0);
        break;
        }
    case DMU_FRONT_SECTOR:
        {
        subsector_t*        subSector = (subsector_t*) hEdge->face->data;
        dmuobjrecord_t* r = (subSector->sector && ((seg_t*) hEdge->data)->sideDef)?
            DMU_GetObjRecord(DMU_SECTOR, subSector->sector) : NULL;
        DMU_GetValue(DMT_HEDGE_SEC, &r, args, 0);
        break;
        }
    case DMU_BACK_SECTOR:
        {
        void*               ptr = NULL;

        if(hEdge->twin)
        {
            subsector_t*        subSector = (subsector_t*) hEdge->twin->face->data;

            /**
             * The sector and the sidedef are checked due to the possibility
             * of the "back-sided window effect", which the games are not
             * currently aware of.
             */
            if(subSector->sector && ((seg_t*) hEdge->twin->data)->sideDef)
                ptr = DMU_GetObjRecord(DMU_SECTOR, subSector->sector);
        }

        DMU_GetValue(DMT_HEDGE_SEC, &ptr, args, 0);
        break;
        }
    case DMU_ANGLE:
        DMU_GetValue(DMT_HEDGE_ANGLE, &((seg_t*) hEdge->data)->angle, args, 0);
        break;
    default:
        Con_Error("Seg_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
