/**
 * @file linedef.h
 * Map LineDef implementation. @ingroup map
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

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_render.h"

#include "m_bams.h"
#include "m_misc.h"
#include "materialvariant.h"
#include "materials.h"

#include "linedef.h"

static void calcNormal(const LineDef* l, byte side, pvec2f_t normal)
{
    V2f_Set(normal, (l->L_vorigin(side^1)[VY] - l->L_vorigin(side)  [VY]) / l->length,
                    (l->L_vorigin(side)  [VX] - l->L_vorigin(side^1)[VX]) / l->length);
}

static float lightLevelDelta(const pvec2f_t normal)
{
    return (1.0f / 255) * (normal[VX] * 18) * rendLightWallAngle;
}

/**
 * @param lineDef  LineDef instance.
 * @param ignoreOpacity  @c true= do not consider Material opacity.
 * @return  @c true if this LineDef's side is considered "closed" (i.e.,
 *     there is no opening through which the back Sector can be seen).
 *     Tests consider all Planes which interface with this and the "middle"
 *     Material used on the relative front side (if any).
 */
static boolean backClosedForBlendNeighbor(LineDef* lineDef, int side, boolean ignoreOpacity)
{
    Sector* frontSec;
    Sector* backSec;
    assert(lineDef);

    if(!lineDef->L_frontsidedef)   return false;
    if(!lineDef->L_backsidedef) return true;

    frontSec = lineDef->L_sector(side);
    backSec  = lineDef->L_sector(side^1);
    if(frontSec == backSec) return false; // Never.

    if(frontSec && backSec)
    {
        if(backSec->SP_floorvisheight >= backSec->SP_ceilvisheight)   return true;
        if(backSec->SP_ceilvisheight  <= frontSec->SP_floorvisheight) return true;
        if(backSec->SP_floorvisheight >= frontSec->SP_ceilvisheight)  return true;
    }

    return LineDef_MiddleMaterialCoversOpening(lineDef, side, ignoreOpacity);
}

static LineDef* findBlendNeighbor(LineDef* l, byte side, byte right,
    binangle_t* diff)
{
    const lineowner_t* farVertOwner = l->L_vo(right^side);
    if(backClosedForBlendNeighbor(l, side, true/*ignore opacity*/))
    {
        return R_FindSolidLineNeighbor(l->L_sector(side), l, farVertOwner, right, diff);
    }
    return R_FindLineNeighbor(l->L_sector(side), l, farVertOwner, right, diff);
}

coord_t LineDef_PointDistance(LineDef* line, coord_t const point[2], coord_t* offset)
{
    assert(line);
    return V2d_PointLineDistance(point, line->L_v1origin, line->direction, offset);
}

coord_t LineDef_PointXYDistance(LineDef* line, coord_t x, coord_t y, coord_t* offset)
{
    coord_t point[2] = { x, y };
    return LineDef_PointDistance(line, point, offset);
}

coord_t LineDef_PointOnSide(const LineDef* line, coord_t const point[2])
{
    assert(line);
    if(!point)
    {
        DEBUG_Message(("LineDef_PointOnSide: Invalid arguments, returning >0.\n"));
        return 1;
    }
    return V2d_PointOnLineSide(point, line->L_v1origin, line->direction);
}

coord_t LineDef_PointXYOnSide(const LineDef* line, coord_t x, coord_t y)
{
    coord_t point[2] = { x, y };
    return LineDef_PointOnSide(line, point);
}

int LineDef_BoxOnSide(LineDef* line, const AABoxd* box)
{
    assert(line);
    return M_BoxOnLineSide(box, line->L_v1origin, line->direction);
}

void LineDef_SetDivline(const LineDef* line, divline_t* dl)
{
    assert(line);

    if(!dl) return;

    dl->origin[VX] = FLT2FIX(line->L_v1origin[VX]);
    dl->origin[VY] = FLT2FIX(line->L_v1origin[VY]);
    dl->direction[VX] = FLT2FIX(line->direction[VX]);
    dl->direction[VY] = FLT2FIX(line->direction[VY]);
}

void LineDef_SetTraceOpening(const LineDef* line, TraceOpening* opening)
{
    Sector* front, *back;
    assert(line);

    if(!opening) return;

    if(!line->L_backsidedef)
    {
        opening->range = 0;
        return;
    }

    front = line->L_frontsector;
    back  = line->L_backsector;

    if(front->SP_ceilheight < back->SP_ceilheight)
        opening->top = front->SP_ceilheight;
    else
        opening->top = back->SP_ceilheight;

    if(front->SP_floorheight > back->SP_floorheight)
    {
        opening->bottom   = front->SP_floorheight;
        opening->lowFloor = back->SP_floorheight;
    }
    else
    {
        opening->bottom   = back->SP_floorheight;
        opening->lowFloor = front->SP_floorheight;
    }

    opening->range = opening->top - opening->bottom;
}

void LineDef_UpdateSlope(LineDef* line)
{
    assert(line);
    V2d_Subtract(line->direction, line->L_v2origin, line->L_v1origin);
    line->slopeType = M_SlopeType(line->direction);
}

/**
 * Returns a two-component float unit vector parallel to the line.
 */
void LineDef_UnitVector(LineDef* line, float* unitvec)
{
    coord_t len;
    assert(line);

    len = M_ApproxDistance(line->direction[VX], line->direction[VY]);
    if(len)
    {
        unitvec[VX] = line->direction[VX] / len;
        unitvec[VY] = line->direction[VY] / len;
    }
    else
    {
        unitvec[VX] = unitvec[VY] = 0;
    }
}

void LineDef_UpdateAABox(LineDef* line)
{
    assert(line);

    line->aaBox.minX = MIN_OF(line->L_v2origin[VX], line->L_v1origin[VX]);
    line->aaBox.minY = MIN_OF(line->L_v2origin[VY], line->L_v1origin[VY]);

    line->aaBox.maxX = MAX_OF(line->L_v2origin[VX], line->L_v1origin[VX]);
    line->aaBox.maxY = MAX_OF(line->L_v2origin[VY], line->L_v1origin[VY]);
}

/**
 * @todo Now that we store surface tangent space normals use those rather than angles.
 */
void LineDef_LightLevelDelta(LineDef* l, int side, float* deltaL, float* deltaR)
{
    binangle_t diff;
    LineDef* other;
    vec2f_t normal;
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
    diff = 0;
    other = findBlendNeighbor(l, side, 0, &diff);
    if(other && INRANGE_OF(diff, BANG_180, BANG_45))
    {
        vec2f_t otherNormal;
        calcNormal(other, other->L_v2 != l->L_v(side), otherNormal);

        // Average normals.
        V2f_Sum(otherNormal, otherNormal, normal);
        otherNormal[VX] /= 2; otherNormal[VY] /= 2;

        *deltaL = lightLevelDelta(otherNormal);
    }
    else
    {
        *deltaL = delta;
    }

    // Do the same for the right edge but with the right neighbour linedef.
    diff = 0;
    other = findBlendNeighbor(l, side, 1, &diff);
    if(other && INRANGE_OF(diff, BANG_180, BANG_45))
    {
        vec2f_t otherNormal;
        calcNormal(other, other->L_v1 != l->L_v(side^1), otherNormal);

        // Average normals.
        V2f_Sum(otherNormal, otherNormal, normal);
        otherNormal[VX] /= 2; otherNormal[VY] /= 2;

        *deltaR = lightLevelDelta(otherNormal);
    }
    else
    {
        *deltaR = delta;
    }
}

/**
 * @todo No need to do this each frame. Set a flag in SideDef->flags to
 * denote this. Is sensitive to plane heights, surface properties
 * (e.g. alpha) and surface texture properties.
 */
boolean LineDef_MiddleMaterialCoversOpening(LineDef *line, int side,
    boolean ignoreOpacity)
{
    assert(line);
    if(line->L_backsidedef)
    {
        SideDef* sideDef = line->L_sidedef(side);
        Sector* frontSec = line->L_sector(side);
        Sector*  backSec = line->L_sector(side^1);

        if(sideDef->SW_middlematerial)
        {
            // Ensure we have up to date info.
            const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
                MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
            material_t* mat = sideDef->SW_middlematerial;
            const materialsnapshot_t* ms = Materials_Prepare(mat, spec, true);

            if(ignoreOpacity || (ms->isOpaque && !sideDef->SW_middleblendmode && sideDef->SW_middlergba[3] >= 1))
            {
                coord_t openTop[2], matTop[2];
                coord_t openBottom[2], matBottom[2];

                if(sideDef->flags & SDF_MIDDLE_STRETCH)
                    return true;

                openTop[0] = openTop[1] =
                    matTop[0] = matTop[1] = LineDef_CeilingMin(line)->visHeight;
                openBottom[0] = openBottom[1] =
                    matBottom[0] = matBottom[1] = LineDef_FloorMax(line)->visHeight;

                // Might the middle material cover the whole gap?
                if(ms->size.height >= (openTop[0] - openBottom[0]) &&
                   ms->size.height >= (openTop[1] - openBottom[1]))
                {
                    // Possibly; check the placement.
                    if(R_MiddleSectionCoords(line, side, frontSec, backSec,
                                             &matBottom[0], &matBottom[1], &matTop[0], &matTop[1],
                                             NULL))
                    {
                        if(matTop[0] >= openTop[0] &&
                           matTop[1] >= openTop[1] &&
                           matBottom[0] <= openBottom[0] &&
                           matBottom[1] <= openBottom[1])
                            return true;
                    }
                }
            }
        }
    }

    return false;
}

Plane* LineDef_FloorMin(const LineDef* line)
{
    assert(line);
    if(!line->L_frontsector) return NULL; // No interfaces.
    if(!line->L_backsidedef || line->L_backsector == line->L_frontsector)
        return line->L_frontsector->SP_floor;
    return line->L_backsector->SP_floorvisheight < line->L_frontsector->SP_floorvisheight?
               line->L_backsector->SP_floor : line->L_frontsector->SP_floor;
}

Plane* LineDef_FloorMax(const LineDef* line)
{
    assert(line);
    if(!line->L_frontsector) return NULL; // No interfaces.
    if(!line->L_backsidedef || line->L_backsector == line->L_frontsector)
        return line->L_frontsector->SP_floor;
    return line->L_backsector->SP_floorvisheight > line->L_frontsector->SP_floorvisheight?
               line->L_backsector->SP_floor : line->L_frontsector->SP_floor;
}

Plane* LineDef_CeilingMin(const LineDef* line)
{
    assert(line);
    if(!line->L_frontsector) return NULL; // No interfaces.
    if(!line->L_backsidedef || line->L_backsector == line->L_frontsector)
        return line->L_frontsector->SP_ceil;
    return line->L_backsector->SP_ceilvisheight < line->L_frontsector->SP_ceilvisheight?
               line->L_backsector->SP_ceil : line->L_frontsector->SP_ceil;
}

Plane* LineDef_CeilingMax(const LineDef* line)
{
    assert(line);
    if(!line->L_frontsector) return NULL; // No interfaces.
    if(!line->L_backsidedef || line->L_backsector == line->L_frontsector)
        return line->L_frontsector->SP_ceil;
    return line->L_backsector->SP_ceilvisheight > line->L_frontsector->SP_ceilvisheight?
               line->L_backsector->SP_ceil : line->L_frontsector->SP_ceil;
}

void LineDef_ReportDrawn(LineDef* line, int pnum)
{
    assert(line);

    if(pnum < 0 || pnum >= DDMAXPLAYERS) return;

    // Already reported?
    if(line->mapped[pnum]) return;

    // Mark as drawn for this player.
    line->mapped[pnum] = true;

    // Send a status report.
    if(gx.HandleMapObjectStatusReport)
    {
        int tmp = pnum;
        gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED, GET_LINE_IDX(line), DMU_LINEDEF, &tmp);
    }
}

int LineDef_SetProperty(LineDef* line, const setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_FRONT_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SECTOR, &line->L_frontsector, args, 0);
        break;
    case DMU_BACK_SECTOR:
        DMU_SetValue(DMT_LINEDEF_SECTOR, &line->L_backsector, args, 0);
        break;
    case DMU_SIDEDEF0:
        DMU_SetValue(DMT_LINEDEF_SIDEDEF, &line->L_frontsidedef, args, 0);
        break;
    case DMU_SIDEDEF1:
        DMU_SetValue(DMT_LINEDEF_SIDEDEF, &line->L_backsidedef, args, 0);
        break;
    case DMU_VALID_COUNT:
        DMU_SetValue(DMT_LINEDEF_VALIDCOUNT, &line->validCount, args, 0);
        break;
    case DMU_FLAGS: {
        SideDef* s;

        DMU_SetValue(DMT_LINEDEF_FLAGS, &line->flags, args, 0);

        s = line->L_frontsidedef;
        Surface_Update(&s->SW_topsurface);
        Surface_Update(&s->SW_bottomsurface);
        Surface_Update(&s->SW_middlesurface);
        if(line->L_backsidedef)
        {
            s = line->L_backsidedef;
            Surface_Update(&s->SW_topsurface);
            Surface_Update(&s->SW_bottomsurface);
            Surface_Update(&s->SW_middlesurface);
        }
        break; }

    default:
        Con_Error("LineDef_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}

int LineDef_GetProperty(const LineDef* line, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_LINEDEF_V, &line->L_v1, args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_LINEDEF_V, &line->L_v2, args, 0);
        break;
    case DMU_DX:
        DMU_GetValue(DMT_LINEDEF_DX, &line->direction[VX], args, 0);
        break;
    case DMU_DY:
        DMU_GetValue(DMT_LINEDEF_DY, &line->direction[VY], args, 0);
        break;
    case DMU_DXY:
        DMU_GetValue(DMT_LINEDEF_DX, &line->direction[VX], args, 0);
        DMU_GetValue(DMT_LINEDEF_DY, &line->direction[VY], args, 1);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DDVT_FLOAT, &line->length, args, 0);
        break;
    case DMU_ANGLE: {
        angle_t lineAngle = BANG_TO_ANGLE(line->angle);
        DMU_GetValue(DDVT_ANGLE, &lineAngle, args, 0);
        break; }
    case DMU_SLOPETYPE:
        DMU_GetValue(DMT_LINEDEF_SLOPETYPE, &line->slopeType, args, 0);
        break;
    case DMU_FRONT_SECTOR: {
        Sector* sec = (line->L_frontsidedef? line->L_frontsector : NULL);
        DMU_GetValue(DMT_LINEDEF_SECTOR, &sec, args, 0);
        break; }
    case DMU_BACK_SECTOR: {
        Sector* sec = (line->L_backsidedef? line->L_backsector : NULL);
        DMU_GetValue(DMT_LINEDEF_SECTOR, &sec, args, 0);
        break; }
    case DMU_FLAGS:
        DMU_GetValue(DMT_LINEDEF_FLAGS, &line->flags, args, 0);
        break;
    case DMU_SIDEDEF0:
        DMU_GetValue(DDVT_PTR, &line->L_frontsidedef, args, 0);
        break;
    case DMU_SIDEDEF1:
        DMU_GetValue(DDVT_PTR, &line->L_backsidedef, args, 0);
        break;
    case DMU_BOUNDING_BOX:
        if(args->valueType == DDVT_PTR)
        {
            const AABoxd* aaBox = &line->aaBox;
            DMU_GetValue(DDVT_PTR, &aaBox, args, 0);
        }
        else
        {
            DMU_GetValue(DMT_LINEDEF_AABOX, &line->aaBox.minX, args, 0);
            DMU_GetValue(DMT_LINEDEF_AABOX, &line->aaBox.maxX, args, 1);
            DMU_GetValue(DMT_LINEDEF_AABOX, &line->aaBox.minY, args, 2);
            DMU_GetValue(DMT_LINEDEF_AABOX, &line->aaBox.maxY, args, 3);
        }
        break;
    case DMU_VALID_COUNT:
        DMU_GetValue(DMT_LINEDEF_VALIDCOUNT, &line->validCount, args, 0);
        break;
    default:
        Con_Error("LineDef_GetProperty: No property %s.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}
