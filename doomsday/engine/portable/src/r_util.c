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
 * r_util.c: Refresh Utility Routines
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_refresh.h"
#include "de_play.h"

#include "p_dmu.h"

// MACROS ------------------------------------------------------------------

#define SLOPERANGE      2048
#define SLOPEBITS       11
#define DBITS           (FRACBITS-SLOPEBITS)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int tantoangle[SLOPERANGE + 1];  // get from tables.c

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Which side of the partition does the point lie?
 *
 * @param x             X coordinate to test.
 * @param y             Y coordinate to test.
 * @return int          @c 0 = front, else @c 1 = back.
 */
int R_PointOnSide(const float x, const float y, const partition_t *par)
{
    float               dx, dy;
    float               left, right;

    if(!par->dX)
    {
        if(x <= par->x)
            return (par->dY > 0? 1:0);
        else
            return (par->dY < 0? 1:0);
    }
    if(!par->dY)
    {
        if(y <= par->y)
            return (par->dX < 0? 1:0);
        else
            return (par->dX > 0? 1:0);
    }

    dx = (x - par->x);
    dy = (y - par->y);

    // Try to quickly decide by looking at the signs.
    if(par->dX < 0)
    {
        if(par->dY < 0)
        {
            if(dx < 0)
            {
                if(dy >= 0)
                    return 0;
            }
            else if(dy < 0)
                return 1;
        }
        else
        {
            if(dx < 0)
            {
                if(dy < 0)
                    return 1;
            }
            else if(dy >= 0)
                return 0;
        }
    }
    else
    {
        if(par->dY < 0)
        {
            if(dx < 0)
            {
                if(dy < 0)
                    return 0;
            }
            else if(dy >= 0)
                return 1;
        }
        else
        {
            if(dx < 0)
            {
                if(dy >= 0)
                    return 1;
            }
            else if(dy < 0)
                return 0;
        }
    }

    left = par->dY * dx;
    right = dy * par->dX;

    if(right < left)
        return 0; // front side
    else
        return 1; // back side
}

int R_SlopeDiv(unsigned num, unsigned den)
{
    unsigned int        ans;

    if(den < 512)
        return SLOPERANGE;
    ans = (num << 3) / (den >> 8);
    return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

static angle_t pointToAngle(float x, float y)
{
    fixed_t pos[2];

    if(x == 0 && y == 0)
        return 0;

    pos[VX] = FLT2FIX(x);
    pos[VY] = FLT2FIX(y);

    if(pos[VX] >= 0)
    {   // x >=0
        if(pos[VY] >= 0)
        {   // y>= 0
            if(pos[VX] > pos[VY])
                return tantoangle[R_SlopeDiv(pos[VY], pos[VX])]; // octant 0

            return ANG90 - 1 - tantoangle[R_SlopeDiv(pos[VX], pos[VY])]; // octant 1
        }

        // y<0
        pos[VY] = -pos[VY];
        if(pos[VX] > pos[VY])
            return -tantoangle[R_SlopeDiv(pos[VY], pos[VX])]; // octant 8

        return ANG270 + tantoangle[R_SlopeDiv(pos[VX], pos[VY])]; // octant 7
    }

    // x<0
    pos[VX] = -pos[VX];
    if(pos[VY] >= 0)
    {   // y>= 0
        if(pos[VX] > pos[VY])
            return ANG180 - 1 - tantoangle[R_SlopeDiv(pos[VY], pos[VX])]; // octant 3

        return ANG90 + tantoangle[R_SlopeDiv(pos[VX], pos[VY])]; // octant 2
    }

    // y<0
    pos[VY] = -pos[VY];
    if(pos[VX] > pos[VY])
        return ANG180 + tantoangle[R_SlopeDiv(pos[VY], pos[VX])]; // octant 4

    return ANG270 - 1 - tantoangle[R_SlopeDiv(pos[VX], pos[VY])]; // octant 5
}

/**
 * To get a global angle from cartesian coordinates, the coordinates are
 * flipped until they are in the first octant of the coordinate system, then
 * the y (<=x) is scaled and divided by x to get a tangent (slope) value
 * which is looked up in the tantoangle[] table.  The +1 size is to handle
 * the case when x==y without additional checking.
 *
 * @param   x           X coordinate to test.
 * @param   y           Y coordinate to test.
 *
 * @return  angle_t     Angle between the test point and viewX,y.
 */
angle_t R_PointToAngle(float x, float y)
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);

    x -= viewData->current.pos[VX];
    y -= viewData->current.pos[VY];

    return pointToAngle(x, y);
}

angle_t R_PointToAngle2(float x1, float y1, float x2, float y2)
{
    x2 -= x1;
    y2 -= y1;

    return pointToAngle(x2, y2);
}

float R_PointToDist(const float x, const float y)
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
    float dx, dy, temp, dist;
    uint angle;

    dx = fabs(x - viewData->current.pos[VX]);
    dy = fabs(y - viewData->current.pos[VY]);

    if(dy > dx)
    {
        temp = dx;
        dx = dy;
        dy = temp;
    }

    angle =
        (tantoangle[FLT2FIX(dy / dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT;

    dist = dx / FIX2FLT(finesine[angle]); // Use as cosine

    return dist;
}

void* P_PointInSubSector(const float x, const float y)
{
    return P_ObjectRecord(DMU_SUBSECTOR, Map_PointInSubsector(P_CurrentMap(), x, y));
}

void R_ScaleAmbientRGB(float *out, const float *in, float mul)
{
    int                 i;
    float               val;

    if(mul < 0)
        mul = 0;
    else if(mul > 1)
        mul = 1;

    for(i = 0; i < 3; ++i)
    {
        val = in[i] * mul;

        if(out[i] < val)
            out[i] = val;
    }
}

/**
 * Conversion from HSV to RGB.  Everything is [0,1].
 */
void R_HSVToRGB(float* rgb, float h, float s, float v)
{
    int                 i;
    float               f, p, q, t;

    if(!rgb)
        return;

    if(s == 0)
    {
        // achromatic (grey)
        rgb[0] = rgb[1] = rgb[2] = v;
        return;
    }

    if(h >= 1)
        h -= 1;

    h *= 6; // sector 0 to 5
    i = floor(h);
    f = h - i; // factorial part of h
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));

    switch(i)
    {
    case 0:
        rgb[0] = v;
        rgb[1] = t;
        rgb[2] = p;
        break;

    case 1:
        rgb[0] = q;
        rgb[1] = v;
        rgb[2] = p;
        break;

    case 2:
        rgb[0] = p;
        rgb[1] = v;
        rgb[2] = t;
        break;

    case 3:
        rgb[0] = p;
        rgb[1] = q;
        rgb[2] = v;
        break;

    case 4:
        rgb[0] = t;
        rgb[1] = p;
        rgb[2] = v;
        break;

    default:
        rgb[0] = v;
        rgb[1] = p;
        rgb[2] = q;
        break;
    }
}
