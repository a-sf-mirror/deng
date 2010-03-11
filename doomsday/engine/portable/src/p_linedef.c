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
#include "de_misc.h"

#include "m_bams.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void calcNormal(const linedef_t* l, byte side, pvec2_t normal)
{
    V2_Set(normal, (LINE_VERTEX(l, side^1)->pos[VY] - LINE_VERTEX(l, side)->pos[VY])   / l->length,
                   (LINE_VERTEX(l, side)->pos[VX]   - LINE_VERTEX(l, side^1)->pos[VX]) / l->length);
}

static float lightLevelDelta(const pvec2_t normal)
{
    return (1.0f / 255) * (normal[VX] * 18) * rendLightWallAngle;
}

static linedef_t* findBlendNeighbor(const linedef_t* l, byte side, byte right,
    binangle_t* diff)
{
    if(!LINE_BACKSIDE(l) ||
       LINE_BACKSECTOR(l)->SP_ceilvisheight <= LINE_FRONTSECTOR(l)->SP_floorvisheight ||
       LINE_BACKSECTOR(l)->SP_floorvisheight >= LINE_FRONTSECTOR(l)->SP_ceilvisheight)
    {
        return R_FindSolidLineNeighbor(LINE_SECTOR(l, side), l, l->L_vo(right^side), right, diff);
    }

    return R_FindLineNeighbor(LINE_SECTOR(l, side), l, l->L_vo(right^side), right, diff);
}

/**
 * The DOOM lighting model applies a sector light level delta when drawing
 * segs based on their 2D world angle.
 *
 * @param l             Linedef to calculate the delta for.
 * @param side          Side of the linedef we are interested in.
 * @param deltaL        Light delta for the left edge is written back here.
 * @param deltaR        Light delta for the right edge is written back here.
 */
void Linedef_LightLevelDelta(const linedef_t* l, byte side, float* deltaL, float* deltaR)
{
    vec2_t normal;
    float delta;

    // Disabled?
    if(!(rendLightWallAngle > 0))
    {
        *deltaL = *deltaR = 0;
        return;
    }

    calcNormal(l, side, normal);
    delta = lightLevelDelta(normal);

    // If smoothing is disabled use this delta for left and right edges.
    // Must forcibly disable smoothing for polyobj linedefs as they have
    // no owner rings.
    if(!rendLightWallAngleSmooth || (l->inFlags & LF_POLYOBJ))
    {
        *deltaL = *deltaR = delta;
        return;
    }

    // Find the left neighbour linedef for which we will calculate the
    // lightlevel delta and then blend with this to produce the value for
    // the left edge. Blend iff the angle between the two linedefs is less
    // than 45 degrees.
    {
    binangle_t diff = 0;
    linedef_t* other = findBlendNeighbor(l, side, 0, &diff);
    if(other && INRANGE_OF(diff, BANG_180, BANG_45))
    {
        vec2_t otherNormal;

        calcNormal(other, other->L_v2 != LINE_VERTEX(l, side), otherNormal);

        // Average normals.
        V2_Sum(otherNormal, otherNormal, normal);
        otherNormal[VX] /= 2; otherNormal[VY] /= 2;

        *deltaL = lightLevelDelta(otherNormal);
    }
    else
        *deltaL = delta;
    }

    // Do the same for the right edge but with the right neighbour linedef.
    {
    binangle_t diff = 0;
    linedef_t* other = findBlendNeighbor(l, side, 1, &diff);
    if(other && INRANGE_OF(diff, BANG_180, BANG_45))
    {
        vec2_t otherNormal;

        calcNormal(other, other->L_v1 != LINE_VERTEX(l, side^1), otherNormal);

        // Average normals.
        V2_Sum(otherNormal, otherNormal, normal);
        otherNormal[VX] /= 2; otherNormal[VY] /= 2;

        *deltaR = lightLevelDelta(otherNormal);
    }
    else
        *deltaR = delta;
    }
}

/**
 * Returns a two-component float unit vector parallel to the line.
 */
void LineDef_UnitVector(const linedef_t* lineDef, float* unitvec)
{
    assert(lineDef);
    assert(unitvec);
    {
    float len = M_ApproxDistancef(lineDef->dX, lineDef->dY);

    if(len)
    {
        unitvec[VX] = lineDef->dX / len;
        unitvec[VY] = lineDef->dY / len;
    }
    else
    {
        unitvec[VX] = unitvec[VY] = 0;
    }
    }
}

/**
 * @return              Non-zero if the point is on the right side of the
 *                      specified line.
 */
int LineDef_PointOnSide(const linedef_t* line, float x, float y)
{
    return !P_PointOnLineSide(x, y, line->L_v1->pos[VX], line->L_v1->pos[VY],
                              line->dX, line->dY);
}

/**
 * Considers the line to be infinite.
 *
 * @return              @c  0 = completely in front of the line.
 *                      @c  1 = completely behind the line.
 *                      @c -1 = box crosses the line.
 */
int LineDef_BoxOnSide2(const linedef_t* lineDef, float xl, float xh, float yl, float yh)
{
    assert(lineDef);
    {
    int a = 0, b = 0;

    switch(lineDef->slopeType)
    {
    default: // Shut up compiler.
      case ST_HORIZONTAL:
        a = yh > lineDef->L_v1->pos[VY];
        b = yl > lineDef->L_v1->pos[VY];
        if(lineDef->dX < 0)
        {
            a ^= 1;
            b ^= 1;
        }
        break;

      case ST_VERTICAL:
        a = xh < lineDef->L_v1->pos[VX];
        b = xl < lineDef->L_v1->pos[VX];
        if(lineDef->dY < 0)
        {
            a ^= 1;
            b ^= 1;
        }
        break;

      case ST_POSITIVE:
        a = LineDef_PointOnSide(lineDef, xl, yh);
        b = LineDef_PointOnSide(lineDef, xh, yl);
        break;

    case ST_NEGATIVE:
        a = LineDef_PointOnSide(lineDef, xh, yh);
        b = LineDef_PointOnSide(lineDef, xl, yl);
        break;
    }

    if(a == b)
        return a;

    return -1;
    }
}

int LineDef_BoxOnSide(const linedef_t* lineDef, const float* box)
{
    assert(box);
    return LineDef_BoxOnSide2(lineDef, box[BOXLEFT], box[BOXRIGHT],
                              box[BOXBOTTOM], box[BOXTOP]);
}

void LineDef_ConstructDivline(const linedef_t* lineDef, divline_t* divline)
{
    assert(lineDef);
    assert(divline);
    {
    const vertex_t* vtx = lineDef->L_v1;

    divline->pos[VX] = FLT2FIX(vtx->pos[VX]);
    divline->pos[VY] = FLT2FIX(vtx->pos[VY]);
    divline->dX = FLT2FIX(lineDef->dX);
    divline->dY = FLT2FIX(lineDef->dY);
    }
}

/**
 * Update the linedef, property is selected by DMU_* name.
 */
boolean LineDef_SetProperty(linedef_t* lin, const setargs_t* args)
{
    switch(args->prop)
    {
    /**
     * \todo Re-implement me (conversion from objectrecord_t).
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
        objectrecord_t* r = P_ObjectRecord(DMU_VERTEX, v);
        DMU_GetValue(DMT_LINEDEF_VERTEX1, &r, args, 0);
        break;
        }
    case DMU_VERTEX1:
        {
        vertex_t* v = lin->L_v2;
        objectrecord_t* r = P_ObjectRecord(DMU_VERTEX, v);
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
        objectrecord_t* r = P_ObjectRecord(DMU_SECTOR, sec);
        DMU_GetValue(DMT_LINEDEF_SEC, &r, args, 0);
        break;
        }
    case DMU_BACK_SECTOR:
        {
        sector_t* sec = (LINE_BACKSIDE(lin)? LINE_BACKSECTOR(lin) : NULL);
        objectrecord_t* r = P_ObjectRecord(DMU_SECTOR, sec);
        DMU_GetValue(DMT_LINEDEF_SEC, &r, args, 0);
        break;
        }
    case DMU_FLAGS:
        DMU_GetValue(DMT_LINEDEF_FLAGS, &lin->flags, args, 0);
        break;
    case DMU_SIDEDEF0:
        {
        sidedef_t* side = LINE_FRONTSIDE(lin);
        objectrecord_t* r = P_ObjectRecord(DMU_SIDEDEF, side);
        DMU_GetValue(DMT_LINEDEF_FRONTSIDEDEF, &r, args, 0);
        break;
        }
    case DMU_SIDEDEF1:
        {
        sidedef_t* side = LINE_BACKSIDE(lin);
        objectrecord_t* r = P_ObjectRecord(DMU_SIDEDEF, side);
        DMU_GetValue(DMT_LINEDEF_BACKSIDEDEF, &r, args, 0);
        break;
        }
    case DMU_POLYOBJ:
        {
        boolean polyLinked = (lin->inFlags & LF_POLYOBJ) != 0;
        DMU_GetValue(DDVT_BOOL, &polyLinked, args, 0);
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

boolean LineDef_IterateMobjs(linedef_t* lineDef, boolean (*func) (mobj_t*, void*), void* data)
{
    assert(lineDef);
    assert(func);
    {
/**
 * Linkstore is list of pointers gathered when iterating stuff.
 * This is pretty much the only way to avoid *all* potential problems
 * caused by callback routines behaving badly (moving or destroying
 * mobjs). The idea is to get a snapshot of all the objects being
 * iterated before any callbacks are called. The hardcoded limit is
 * a drag, but I'd like to see you iterating 2048 mobjs/lines in one block.
 */
#define MAXLINKED           2048
#define DO_LINKS(it, end)   for(it = linkstore; it < end; it++) \
                                if(!func(*it, data)) return false;

    void* linkstore[MAXLINKED];
    void** end = linkstore, **it;
    nodeindex_t root, nix;
    linknode_t* ln;
    map_t* map = P_CurrentMap(); // @fixme LineDef should tell us which map it belongs to.

    root = map->lineLinks[P_ObjectRecord(DMU_LINEDEF, lineDef)->id - 1];
    ln = map->lineNodes->nodes;

    for(nix = ln[root].next; nix != root; nix = ln[nix].next)
        *end++ = ln[nix].ptr;

    DO_LINKS(it, end);
    return true;

#undef MAXLINKED
#undef DO_LINKS
    }
}
