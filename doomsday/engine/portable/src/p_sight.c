/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

/*
 * p_sight.c: Line of Sight Testing
 *
 * This uses specialized forms of the maputils routines for optimized
 * performance.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

//extern intercept_t intercepts[MAXINTERCEPTS], *intercept_p;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

fixed_t sightzstart;            // eye z of looker
fixed_t topslope, bottomslope;  // slopes to top and bottom of target
int     sightcounts[3];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static divline_t strace;

// CODE --------------------------------------------------------------------

boolean PTR_SightTraverse(intercept_t *in)
{
    line_t *li;
    fixed_t slope;

    if(in == NULL) return true; // Something was out of bounds?

    li = in->d.line;

    // Crosses a two sided line.
    P_LineOpening(li);

    if(openbottom >= opentop)   // quick test for totally closed doors
        return false;           // stop

    if(li->L_frontsector->SP_floorheight != li->L_backsector->SP_floorheight)
    {
        slope = FixedDiv(FLT2FIX(openbottom) - sightzstart, in->frac);
        if(slope > bottomslope)
            bottomslope = slope;
    }

    if(li->L_frontsector->SP_ceilheight != li->L_backsector->SP_ceilheight)
    {
        slope = FixedDiv(FLT2FIX(opentop) - sightzstart, in->frac);
        if(slope < topslope)
            topslope = slope;
    }

    if(topslope <= bottomslope)
        return false;           // stop

    return true;                // keep going
}

boolean P_SightBlockLinesIterator(int x, int y)
{
    int         offset;
    long       *list;
    line_t     *ld;
    int         s[2];
    divline_t   dl;
    polyblock_t *polyLink;
    seg_t     **segList;
    uint        i;

    if(x < 0 || y < 0 || x >= bmapwidth || y >= bmapheight)
    {
#if _DEBUG
        Con_Message
            ("P_SightBlockLinesIterator: x=%i, y=%i outside blockmap.\n", x,
             y);
#endif
        return false;
    }

    offset = y * bmapwidth + x;

    polyLink = polyblockmap[offset];
    while(polyLink)
    {
        if(polyLink->polyobj)
        {                       // only check non-empty links
            if(polyLink->polyobj->validcount != validcount)
            {
                segList = polyLink->polyobj->segs;
                for(i = 0; i < polyLink->polyobj->numsegs; ++i, segList++)
                {
                    ld = (*segList)->linedef;
                    if(ld->validcount == validcount)
                        continue;

                    ld->validcount = validcount;

                    s[0] = P_PointOnDivlineSide(FLT2FIX(ld->L_v1pos[VX]),
                                                FLT2FIX(ld->L_v1pos[VY]), &strace);
                    s[1] = P_PointOnDivlineSide(FLT2FIX(ld->L_v2pos[VX]),
                                                FLT2FIX(ld->L_v2pos[VY]), &strace);
                    if(s[0] == s[1])
                        continue;   // line isn't crossed

                    P_MakeDivline(ld, &dl);
                    s[0] = P_PointOnDivlineSide(strace.pos[VX], strace.pos[VY], &dl);
                    s[1] = P_PointOnDivlineSide(strace.pos[VX] + strace.dx,
                                                strace.pos[VY] + strace.dy, &dl);
                    if(s[0] == s[1])
                        continue;   // line isn't crossed

                    // try to early out the check
                    if(!ld->L_backside)
                        return false;   // stop checking

                    // store the line for later intersection testing
                    /*intercept_p->d.line = ld;
                       intercept_p++; */
                    P_AddIntercept(0, true, ld);
                }
                polyLink->polyobj->validcount = validcount;
            }
        }
        polyLink = polyLink->next;
    }

    offset = *(blockmap + offset);

    for(list = blockmaplump + offset; *list != -1; list++)
    {
        ld = LINE_PTR(*list);
        if(ld->flags & LINEF_BENIGN)
            continue;

        if(ld->validcount == validcount)
            continue;           // line has already been checked

        ld->validcount = validcount;

        s[0] = P_PointOnDivlineSide(FLT2FIX(ld->L_v1pos[VX]),
                                    FLT2FIX(ld->L_v1pos[VY]), &strace);
        s[1] = P_PointOnDivlineSide(FLT2FIX(ld->L_v2pos[VX]),
                                    FLT2FIX(ld->L_v2pos[VY]), &strace);
        if(s[0] == s[1])
            continue;           // line isn't crossed

        P_MakeDivline(ld, &dl);
        s[0] = P_PointOnDivlineSide(strace.pos[VX], strace.pos[VY], &dl);
        s[1] = P_PointOnDivlineSide(strace.pos[VX] + strace.dx, strace.pos[VY] + strace.dy,
                                  &dl);
        if(s[0] == s[1])
            continue;           // line isn't crossed

        // try to early out the check
        if(!ld->L_backside)
            return false;       // stop checking

        // store the line for later intersection testing
        /*intercept_p->d.line = ld;
           intercept_p++; */
        P_AddIntercept(0, true, ld);
    }

    return true;                // everything was checked
}

/*
 * Traces a line from x1,y1 to x2,y2, calling the traverser function for
 * each. Returns true if the traverser function returns true for all lines.
 */
boolean P_SightPathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    fixed_t xt1, yt1, xt2, yt2;
    fixed_t xstep, ystep;
    fixed_t partial;
    fixed_t xintercept, yintercept;
    int     mapx, mapy, mapxstep, mapystep;
    int     count;

    validcount++;
    //intercept_p = intercepts;
    P_ClearIntercepts();

    if(((x1 - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0)
        x1 += FRACUNIT;         // don't side exactly on a line
    if(((y1 - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0)
        y1 += FRACUNIT;         // don't side exactly on a line

    strace.pos[VX] = x1;
    strace.pos[VY] = y1;
    strace.dx = x2 - x1;
    strace.dy = y2 - y1;

    x1 -= bmaporgx;
    y1 -= bmaporgy;
    xt1 = x1 >> MAPBLOCKSHIFT;
    yt1 = y1 >> MAPBLOCKSHIFT;

    x2 -= bmaporgx;
    y2 -= bmaporgy;
    xt2 = x2 >> MAPBLOCKSHIFT;
    yt2 = y2 >> MAPBLOCKSHIFT;

    // points should never be out of bounds, but check once instead of
    // each block
    if(xt1 < 0 || yt1 < 0 || xt1 >= bmapwidth || yt1 >= bmapheight || xt2 < 0
       || yt2 < 0 || xt2 >= bmapwidth || yt2 >= bmapheight)
        return false;

    if(xt2 > xt1)
    {
        mapxstep = 1;
        partial = FRACUNIT - ((x1 >> MAPBTOFRAC) & (FRACUNIT - 1));
        ystep = FixedDiv(y2 - y1, abs(x2 - x1));
    }
    else if(xt2 < xt1)
    {
        mapxstep = -1;
        partial = (x1 >> MAPBTOFRAC) & (FRACUNIT - 1);
        ystep = FixedDiv(y2 - y1, abs(x2 - x1));
    }
    else
    {
        mapxstep = 0;
        partial = FRACUNIT;
        ystep = 256 * FRACUNIT;
    }
    yintercept = (y1 >> MAPBTOFRAC) + FixedMul(partial, ystep);

    if(yt2 > yt1)
    {
        mapystep = 1;
        partial = FRACUNIT - ((y1 >> MAPBTOFRAC) & (FRACUNIT - 1));
        xstep = FixedDiv(x2 - x1, abs(y2 - y1));
    }
    else if(yt2 < yt1)
    {
        mapystep = -1;
        partial = (y1 >> MAPBTOFRAC) & (FRACUNIT - 1);
        xstep = FixedDiv(x2 - x1, abs(y2 - y1));
    }
    else
    {
        mapystep = 0;
        partial = FRACUNIT;
        xstep = 256 * FRACUNIT;
    }
    xintercept = (x1 >> MAPBTOFRAC) + FixedMul(partial, xstep);

    // Step through map blocks.
    // Count is present to prevent a round off error from skipping the break.
    mapx = xt1;
    mapy = yt1;

    for(count = 0; count < 64; count++)
    {
        if(!P_SightBlockLinesIterator(mapx, mapy))
        {
            sightcounts[1]++;
            return false;       // early out
        }

        // At or past the target?
        if((mapx == xt2 && mapy == yt2) ||
           (((x2 >= x1 && mapx >= xt2) || (x2 < x1 && mapx <= xt2)) &&
            ((y2 >= y1 && mapy >= yt2) || (y2 < y1 && mapy <= yt2))))
            break;

        if((yintercept >> FRACBITS) == mapy)
        {
            yintercept += ystep;
            mapx += mapxstep;
        }
        else if((xintercept >> FRACBITS) == mapx)
        {
            xintercept += xstep;
            mapy += mapystep;
        }
    }

    // Couldn't early out, so go through the sorted list
    sightcounts[2]++;

    return P_SightTraverseIntercepts(&strace, PTR_SightTraverse);
}

#if 0
boolean P_SightPathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    fixed_t xt1, yt1, xt2, yt2;
    fixed_t xstep, ystep;
    fixed_t partial;
    fixed_t xintercept, yintercept;
    int     mapx, mapy, mapxstep, mapystep;
    int     count;

    validcount++;
    intercept_p = intercepts;

    if(((x1 - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0)
        x1 += FRACUNIT;         // don't side exactly on a line
    if(((y1 - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0)
        y1 += FRACUNIT;         // don't side exactly on a line

    strace.x = x1;
    strace.y = y1;
    strace.dx = x2 - x1;
    strace.dy = y2 - y1;

    x1 -= bmaporgx;
    y1 -= bmaporgy;
    xt1 = x1 >> MAPBLOCKSHIFT;
    yt1 = y1 >> MAPBLOCKSHIFT;

    x2 -= bmaporgx;
    y2 -= bmaporgy;
    xt2 = x2 >> MAPBLOCKSHIFT;
    yt2 = y2 >> MAPBLOCKSHIFT;

    // points should never be out of bounds, but check once instead of
    // each block
    if(xt1 < 0 || yt1 < 0 || xt1 >= bmapwidth || yt1 >= bmapheight || xt2 < 0
       || yt2 < 0 || xt2 >= bmapwidth || yt2 >= bmapheight)
        return false;

    if(xt2 > xt1)
    {
        mapxstep = 1;
        partial = FRACUNIT - ((x1 >> MAPBTOFRAC) & (FRACUNIT - 1));
        ystep = FixedDiv(y2 - y1, abs(x2 - x1));
    }
    else if(xt2 < xt1)
    {
        mapxstep = -1;
        partial = (x1 >> MAPBTOFRAC) & (FRACUNIT - 1);
        ystep = FixedDiv(y2 - y1, abs(x2 - x1));
    }
    else
    {
        mapxstep = 0;
        partial = FRACUNIT;
        ystep = 256 * FRACUNIT;
    }
    yintercept = (y1 >> MAPBTOFRAC) + FixedMul(partial, ystep);

    if(yt2 > yt1)
    {
        mapystep = 1;
        partial = FRACUNIT - ((y1 >> MAPBTOFRAC) & (FRACUNIT - 1));
        xstep = FixedDiv(x2 - x1, abs(y2 - y1));
    }
    else if(yt2 < yt1)
    {
        mapystep = -1;
        partial = (y1 >> MAPBTOFRAC) & (FRACUNIT - 1);
        xstep = FixedDiv(x2 - x1, abs(y2 - y1));
    }
    else
    {
        mapystep = 0;
        partial = FRACUNIT;
        xstep = 256 * FRACUNIT;
    }
    xintercept = (x1 >> MAPBTOFRAC) + FixedMul(partial, xstep);

    // Step through map blocks.
    // Count is present to prevent a round off error from skipping the break.
    mapx = xt1;
    mapy = yt1;

    for(count = 0; count < 64; count++)
    {
        /*if(mapx < 0 || mapy < 0 ||
           mapx >= bmapwidth || mapy >= bmapheight) */
        {
            ST_Message("Smx=%i my=%i\n", mapx, mapy);
            /*ST_Message("P_SPthTrv: mapx=%i mapy=%i dest=(%i,%i)\n"
               "  xint=%f yint=%f mxst=%i myst=%i\n"
               "  x1=%i partial=%f xstep=%f\n",
               mapx, mapy, xt2, yt2, FIX2FLT(xintercept),
               FIX2FLT(yintercept), mapxstep, mapystep,
               x1>>FRACBITS, FIX2FLT(partial), FIX2FLT(xstep)); */
        }
        if(!P_SightBlockLinesIterator(mapx, mapy))
        {
            sightcounts[1]++;
            return false;       // early out
        }

        if(mapx == xt2 && mapy == yt2)
            break;

        if((yintercept >> FRACBITS) == mapy)
        {
            yintercept += ystep;
            mapx += mapxstep;
        }
        else if((xintercept >> FRACBITS) == mapx)
        {
            xintercept += xstep;
            mapy += mapystep;
        }
    }

    // Couldn't early out, so go through the sorted list
    sightcounts[2]++;

    return P_SightTraverseIntercepts();
}
#endif

/*
 * Checks the reject matrix to find out if the two sectors are visible
 * from each other.
 */
boolean P_CheckReject(sector_t *sec1, sector_t *sec2)
{
    int     s1;
    int     s2;
    int     pnum;
    int     bytenum;
    int     bitnum;

    if(rejectmatrix != NULL)
    {
        // Determine subsector entries in REJECT table.
        s1 = GET_SECTOR_IDX(sec1);
        s2 = GET_SECTOR_IDX(sec2);
        pnum = s1 * numsectors + s2;
        bytenum = pnum >> 3;
        bitnum = 1 << (pnum & 7);

        // Check in REJECT table.
        if(rejectmatrix[bytenum] & bitnum)
        {
            sightcounts[0]++;
            // Can't possibly be connected.
            return false;
        }
    }
    return true;
}

/*
 * Returns true if a straight line between t1 and t2 is unobstructed.
 * Look from eyes of t1 to any part of t2 (start from middle of t1).
 * Uses specialized forms of the maputils routines for optimized
 * performance.
 */
boolean P_CheckSight(mobj_t *t1, mobj_t *t2)
{
    // If either is unlinked, they can't see each other.
    if(!t1->subsector || !t2->subsector)
        return false;

    // Check for trivial rejection.
    if(!P_CheckReject(t1->subsector->sector, t2->subsector->sector))
        return false;

    if(t2->dplayer && t2->dplayer->flags & DDPF_CAMERA)
        return false;           // Cameramen don't exist!

    // Check precisely.
    sightzstart = t1->pos[VZ];
    if(!(t1->dplayer && t1->dplayer->flags & DDPF_CAMERA))
        sightzstart += FLT2FIX(t1->height) + -(FLT2FIX(t1->height) >> 2);
    topslope = (t2->pos[VZ] + FLT2FIX(t2->height)) - sightzstart;
    bottomslope = (t2->pos[VZ]) - sightzstart;

    return P_SightPathTraverse(t1->pos[VX], t1->pos[VY], t2->pos[VX], t2->pos[VY]);
}

boolean P_CheckLineSight(float from[3], float to[3])
{
    sightzstart = from[VZ]*FRACUNIT;
    topslope = to[VZ]*FRACUNIT + FRACUNIT - sightzstart;
    bottomslope = to[VZ]*FRACUNIT - FRACUNIT - sightzstart;

    return P_SightPathTraverse(from[VX] * FRACUNIT, from[VY] * FRACUNIT,
                               to[VX] * FRACUNIT, to[VY] * FRACUNIT);
}
