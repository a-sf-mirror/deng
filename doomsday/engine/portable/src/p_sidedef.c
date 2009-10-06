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
 * r_sidedef.c: World sidedefs.
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

void SideDef_ColorTints(sidedef_t* side, segsection_t section,
                        const float** topColor, const float** bottomColor)
{
    // Select the colors for this surface.
    switch(section)
    {
    case SEG_MIDDLE:
        if(side->flags & SDF_BLEND_MIDTOTOP)
        {
            *topColor = side->SW_toprgba;
            *bottomColor = side->SW_middlergba;
        }
        else if(side->flags & SDF_BLEND_MIDTOBOTTOM)
        {
            *topColor = side->SW_middlergba;
            *bottomColor = side->SW_bottomrgba;
        }
        else
        {
            *topColor = side->SW_middlergba;
            *bottomColor = NULL;
        }
        break;

    case SEG_TOP:
        if(side->flags & SDF_BLEND_TOPTOMID)
        {
            *topColor = side->SW_toprgba;
            *bottomColor = side->SW_middlergba;
        }
        else
        {
            *topColor = side->SW_toprgba;
            *bottomColor = NULL;
        }
        break;

    case SEG_BOTTOM:
        // Select the correct colors for this surface.
        if(side->flags & SDF_BLEND_BOTTOMTOMID)
        {
            *topColor = side->SW_middlergba;
            *bottomColor = side->SW_bottomrgba;
        }
        else
        {
            *topColor = side->SW_bottomrgba;
            *bottomColor = NULL;
        }
        break;

    default:
        break;
    }
}

/**
 * Update the sidedef, property is selected by DMU_* name.
 */
boolean SideDef_SetProperty(sidedef_t *sid, const setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_FLAGS:
        DMU_SetValue(DMT_SIDEDEF_FLAGS, &sid->flags, args, 0);
        break;
    default:
        Con_Error("SideDef_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a sidedef property, selected by DMU_* name.
 */
boolean SideDef_GetProperty(const sidedef_t *sid, setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_SECTOR:
        {
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_SECTOR, sid->sector);
        DMU_GetValue(DMT_SIDEDEF_SECTOR, &r, args, 0);
        break;
        }
    case DMU_LINEDEF:
        {
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_LINEDEF, sid->lineDef);
        DMU_GetValue(DMT_SIDEDEF_LINEDEF, &r, args, 0);
        break;
        }
    case DMU_FLAGS:
        DMU_GetValue(DMT_SIDEDEF_FLAGS, &sid->flags, args, 0);
        break;
    default:
        Con_Error("SideDef_GetProperty: Has no property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
