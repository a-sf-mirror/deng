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
 * r_linedef.c: World linedefs.
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

float LineDef_GetLightLevelDelta(const linedef_t* l)
{
    return (1.0f / 255) *
        ((l->L_v2->pos[VY] - l->L_v1->pos[VY]) / l->length * 18);
}

/**
 * Update the linedef, property is selected by DMU_* name.
 */
boolean LineDef_SetProperty(linedef_t* lin, const setargs_t* args)
{
    switch(args->prop)
    {
    /**
     * \todo Re-implement me (conversion from dmuobjrecord_t).
    case DMU_FRONT_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SEC, &LINE_FRONTSECTOR(lin), args, 0);
        break;

    case DMU_BACK_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SEC, &LINE_BACKSECTOR(lin), args, 0);
        break;*/

    /**
     * \todo Re-implement me (need to update all seg->sideDef ptrs).
    case DMU_SIDEDEF0:
        DMU_SetValue(DMT_LINEDEF_SIDE, &LINE_FRONTSIDE(lin), args, 0);
        break;
    case DMU_SIDEDEF1:
        DMU_SetValue(DMT_LINEDEF_SIDE, &LINE_BACKSIDE(lin), args, 0);
        break;*/
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_LINEDEF_VALIDCOUNT, &lin->validCount, args, 0);
        break;
    case DMU_FLAGS:
        {
        sidedef_t*          s;

        DMU_SetValue(DMT_LINEDEF_FLAGS, &lin->flags, args, 0);

        s = LINE_FRONTSIDE(lin);
        Surface_Update(&s->SW_topsurface);
        Surface_Update(&s->SW_bottomsurface);
        Surface_Update(&s->SW_middlesurface);
        if((s = LINE_BACKSIDE(lin)))
        {
            Surface_Update(&s->SW_topsurface);
            Surface_Update(&s->SW_bottomsurface);
            Surface_Update(&s->SW_middlesurface);
        }
        break;
        }
    default:
        Con_Error("LineDef_SetProperty: Property %s is not writable.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}

/**
 * Get the value of a linedef property, selected by DMU_* name.
 */
boolean LineDef_GetProperty(const linedef_t *lin, setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_VERTEX0:
        {
        vertex_t* v = lin->L_v1;
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_VERTEX, v);
        DMU_GetValue(DMT_LINEDEF_VERTEX1, &r, args, 0);
        break;
        }
    case DMU_VERTEX1:
        {
        vertex_t* v = lin->L_v2;
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_VERTEX, v);
        DMU_GetValue(DMT_LINEDEF_VERTEX2, &r, args, 0);
        break;
        }
    case DMU_DX:
        DMU_GetValue(DMT_LINEDEF_DX, &lin->dX, args, 0);
        break;
    case DMU_DY:
        DMU_GetValue(DMT_LINEDEF_DY, &lin->dY, args, 0);
        break;
    case DMU_DXY:
        DMU_GetValue(DMT_LINEDEF_DX, &lin->dX, args, 0);
        DMU_GetValue(DMT_LINEDEF_DY, &lin->dY, args, 1);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DDVT_FLOAT, &lin->length, args, 0);
        break;
    case DMU_ANGLE:
        DMU_GetValue(DDVT_ANGLE, &lin->angle, args, 0);
        break;
    case DMU_SLOPE_TYPE:
        DMU_GetValue(DMT_LINEDEF_SLOPETYPE, &lin->slopeType, args, 0);
        break;
    case DMU_FRONT_SECTOR:
        {
        sector_t* sec = (LINE_FRONTSIDE(lin)? LINE_FRONTSECTOR(lin) : NULL);
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_SECTOR, sec);
        DMU_GetValue(DMT_LINEDEF_SEC, &r, args, 0);
        break;
        }
    case DMU_BACK_SECTOR:
        {
        sector_t* sec = (LINE_BACKSIDE(lin)? LINE_BACKSECTOR(lin) : NULL);
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_SECTOR, sec);
        DMU_GetValue(DMT_LINEDEF_SEC, &r, args, 0);
        break;
        }
    case DMU_FLAGS:
        DMU_GetValue(DMT_LINEDEF_FLAGS, &lin->flags, args, 0);
        break;
    case DMU_SIDEDEF0:
        {
        sidedef_t* side = LINE_FRONTSIDE(lin);
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_SIDEDEF, side);
        DMU_GetValue(DMT_LINEDEF_FRONTSIDEDEF, &r, args, 0);
        break;
        }
    case DMU_SIDEDEF1:
        {
        sidedef_t* side = LINE_BACKSIDE(lin);
        dmuobjrecord_t* r = DMU_GetObjRecord(DMU_SIDEDEF, side);
        DMU_GetValue(DMT_LINEDEF_BACKSIDEDEF, &r, args, 0);
        break;
        }
    case DMU_BOUNDING_BOX:
        if(args->valueType == DDVT_PTR)
        {
            const float* bbox = lin->bBox;
            DMU_GetValue(DDVT_PTR, &bbox, args, 0);
        }
        else
        {
            DMU_GetValue(DMT_LINEDEF_BBOX, &lin->bBox[0], args, 0);
            DMU_GetValue(DMT_LINEDEF_BBOX, &lin->bBox[1], args, 1);
            DMU_GetValue(DMT_LINEDEF_BBOX, &lin->bBox[2], args, 2);
            DMU_GetValue(DMT_LINEDEF_BBOX, &lin->bBox[3], args, 3);
        }
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_LINEDEF_VALIDCOUNT, &lin->validCount, args, 0);
        break;
    default:
        Con_Error("LineDef_GetProperty: No property %s.\n",
                  DMU_Str(args->prop));
    }

    return true; // Continue iteration.
}
