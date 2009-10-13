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
 * r_subsector.c: World subsectors.
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
 * Update the subsector, property is selected by DMU_* name.
 */
boolean Subsector_SetProperty(subsector_t* subsector, const setargs_t* args)
{
    Con_Error("Subsector_SetProperty: Property %s is not writable.\n",
              DMU_Str(args->prop));

    return true; // Continue iteration.
}

/**
 * Get the value of a subsector property, selected by DMU_* name.
 */
boolean Subsector_GetProperty(const subsector_t* subsector, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_SECTOR:
        {
        sector_t* sec = subsector->sector;
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_SECTOR, sec);
        DMU_GetValue(DMT_FACE_SECTOR, &r, args, 0);
        break;
        }
    default:
        Con_Error("Subsector_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
