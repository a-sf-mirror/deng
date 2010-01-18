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
 * p_maputil.c: Map Utility Routines
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define ORDER(x,y,a,b)  ( (x)<(y)? ((a)=(x),(b)=(y)) : ((b)=(x),(a)=(y)) )

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float opentop, openbottom, openrange;
float lowfloor;

divline_t traceLOS;
boolean earlyout;
int ptflags;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

float P_AccurateDistanceFixed(fixed_t dx, fixed_t dy)
{
    float               fx = FIX2FLT(dx), fy = FIX2FLT(dy);

    return (float) sqrt(fx * fx + fy * fy);
}

float P_AccurateDistance(float dx, float dy)
{
    return (float) sqrt(dx * dx + dy * dy);
}

/**
 * Gives an estimation of distance (not exact).
 */
float P_ApproxDistance(float dx, float dy)
{
    dx = fabs(dx);
    dy = fabs(dy);

    return dx + dy - ((dx < dy ? dx : dy) / 2);
}

/**
 * Gives an estimation of 3D distance (not exact).
 * The Z axis aspect ratio is corrected.
 */
float P_ApproxDistance3(float dx, float dy, float dz)
{
    return P_ApproxDistance(P_ApproxDistance(dx, dy), dz * 1.2f);
}

/**
 * Either end or fixpoint must be specified. The distance is measured
 * (approximately) in 3D. Start must always be specified.
 */
float P_MobjPointDistancef(mobj_t* start, mobj_t* end, float* fixpoint)
{
    if(!start)
        return 0;

    if(end)
    {
        // Start -> end.
        return M_ApproxDistancef(end->pos[VZ] - start->pos[VZ],
                                 M_ApproxDistancef(end->pos[VX] - start->pos[VX],
                                                   end->pos[VY] - start->pos[VY]));
    }

    if(fixpoint)
    {
        float               sp[3];

        sp[VX] = start->pos[VX];
        sp[VY] = start->pos[VY],
        sp[VZ] = start->pos[VZ];

        return M_ApproxDistancef(fixpoint[VZ] - sp[VZ],
                                 M_ApproxDistancef(fixpoint[VX] - sp[VX],
                                                   fixpoint[VY] - sp[VY]));
    }

    return 0;
}

/**
 * Lines start, end and fdiv must intersect.
 */
#ifdef _MSC_VER
#  pragma optimize("g", off)
#endif
float P_FloatInterceptVertex(fvertex_t* start, fvertex_t* end,
                             fdivline_t* fdiv, fvertex_t* inter)
{
    float               ax = start->pos[VX], ay = start->pos[VY];
    float               bx = end->pos[VX], by = end->pos[VY];
    float               cx = fdiv->pos[VX], cy = fdiv->pos[VY];
    float               dx = cx + fdiv->dX, dy = cy + fdiv->dY;

    /*
           (YA-YC)(XD-XC)-(XA-XC)(YD-YC)
       r = -----------------------------  (eqn 1)
           (XB-XA)(YD-YC)-(YB-YA)(XD-XC)
     */

    float               r =
        ((ay - cy) * (dx - cx) - (ax - cx) * (dy - cy)) /
        ((bx - ax) * (dy - cy) - (by - ay) * (dx - cx));
    /*
       XI = XA+r(XB-XA)
       YI = YA+r(YB-YA)
     */
    inter->pos[VX] = ax + r * (bx - ax);
    inter->pos[VY] = ay + r * (by - ay);
    return r;
}

/**
 * @return              Non-zero if the point is on the right side of the
 *                      specified line.
 */
int P_PointOnLineSide(float x, float y, float lX, float lY, float lDX,
                      float lDY)
{
    /*
       (AY-CY)(BX-AX)-(AX-CX)(BY-AY)
       s = -----------------------------
       L**2

       If s<0      C is left of AB (you can just check the numerator)
       If s>0      C is right of AB
       If s=0      C is on AB
     */
    return ((lY - y) * lDX - (lX - x) * lDY >= 0);
}
#ifdef _MSC_VER
#  pragma optimize("", on)
#endif

/**
 * Determines on which side of dline the point is. Returns true if the
 * point is on the line or on the right side.
 */
int P_PointOnDivLineSidef(fvertex_t* pnt, fdivline_t* dline)
{
    return !P_PointOnLineSide(pnt->pos[VX], pnt->pos[VY], dline->pos[VX],
                              dline->pos[VY], dline->dX, dline->dY);
}

int DMU_PointOnLineDefSide(float x, float y, void* p)
{
    return LineDef_PointOnSide((linedef_t*) ((objectrecord_t*) p)->obj, x, y);
}

/**
 * Where is the given point in relation to the line.
 *
 * @param pointX        X coordinate of the point.
 * @param pointY        Y coordinate of the point.
 * @param lineDX        X delta of the line.
 * @param lineDY        Y delta of the line.
 * @param linePerp      Perpendicular d of the line.
 * @param lineLength    Length of the line.
 *
 * @return              @c <0= on left side.
 *                      @c  0= intersects.
 *                      @c >0= on right side.
 */
int P_PointOnLineDefSide2(double pointX, double pointY, double lineDX,
                       double lineDY, double linePerp, double lineLength,
                       double epsilon)
{
    double              perp =
        M_PerpDist(lineDX, lineDY, linePerp, lineLength, pointX, pointY);

    if(fabs(perp) <= epsilon)
        return 0;

    return (perp < 0? -1 : +1);
}

/**
 * Check the spatial relationship between the given box and a partitioning
 * line.
 *
 * @param bbox          Ptr to the box being tested.
 * @param lineSX        X coordinate of the start of the line.
 * @param lineSY        Y coordinate of the end of the line.
 * @param lineDX        X delta of the line (slope).
 * @param lineDY        Y delta of the line (slope).
 * @param linePerp      Perpendicular d of the line.
 * @param lineLength    Length of the line.
 * @param epsilon       Points within this distance will be considered equal.
 *
 * @return              @c <0= bbox is wholly on the left side.
 *                      @c  0= line intersects bbox.
 *                      @c >0= bbox wholly on the right side.
 */
int P_BoxOnLineSide3(const int bbox[4], double lineSX, double lineSY,
                     double lineDX, double lineDY, double linePerp,
                     double lineLength, double epsilon)
{
#define IFFY_LEN        4.0

    int                 p1, p2;
    double              x1 = (double)bbox[BOXLEFT]   - IFFY_LEN * 1.5;
    double              y1 = (double)bbox[BOXBOTTOM] - IFFY_LEN * 1.5;
    double              x2 = (double)bbox[BOXRIGHT]  + IFFY_LEN * 1.5;
    double              y2 = (double)bbox[BOXTOP]    + IFFY_LEN * 1.5;

    if(lineDX == 0)
    {   // Horizontal.
        p1 = (x1 > lineSX? +1 : -1);
        p2 = (x2 > lineSX? +1 : -1);

        if(lineDY < 0)
        {
            p1 = -p1;
            p2 = -p2;
        }
    }
    else if(lineDY == 0)
    {   // Vertical.
        p1 = (y1 < lineSY? +1 : -1);
        p2 = (y2 < lineSY? +1 : -1);

        if(lineDX < 0)
        {
            p1 = -p1;
            p2 = -p2;
        }
    }
    else if(lineDX * lineDY > 0)
    {   // Positive slope.
        p1 = P_PointOnLineDefSide2(x1, y2, lineDX, lineDY, linePerp, lineLength, epsilon);
        p2 = P_PointOnLineDefSide2(x2, y1, lineDX, lineDY, linePerp, lineLength, epsilon);
    }
    else
    {   // Negative slope.
        p1 = P_PointOnLineDefSide2(x1, y1, lineDX, lineDY, linePerp, lineLength, epsilon);
        p2 = P_PointOnLineDefSide2(x2, y2, lineDX, lineDY, linePerp, lineLength, epsilon);
    }

    if(p1 == p2)
        return p1;

    return 0;

#undef IFFY_LEN
}

int DMU_BoxOnLineSide(const float* box, void* p)
{
    return LineDef_BoxOnSide((linedef_t*) ((objectrecord_t*) p)->obj, box);
}

/**
 * @return              @c 0 if point is in front of the line, else @c 1.
 */
int P_PointOnDivlineSide(float fx, float fy, const divline_t* line)
{
    fixed_t x = FLT2FIX(fx);
    fixed_t y = FLT2FIX(fy);

    if(!line->dX)
    {
        return (x <= line->pos[VX])? line->dY > 0 : line->dY < 0;
    }
    else if(!line->dY)
    {
        return (y <= line->pos[VY])? line->dX < 0 : line->dX > 0;
    }
    else
    {
        fixed_t dX = x - line->pos[VX];
        fixed_t dY = y - line->pos[VY];

        // Try to quickly decide by comparing signs.
        if((line->dY ^ line->dX ^ dX ^ dY) & 0x80000000)
        {   // Left is negative.
            return ((line->dY ^ dX) & 0x80000000)? 1 : 0;
        }
        else
        {   // if left >= right return 1 else 0.
            return FixedMul(dY >> 8, line->dX >> 8) >=
                FixedMul(line->dY >> 8, dX >> 8);
        }
    }
}

void DMU_MakeDivline(void* p, divline_t* dl)
{
    LineDef_ConstructDivline(((objectrecord_t*) p)->obj, dl);
}

/**
 * @return              Fractional intercept point along the first divline.
 */
float P_InterceptVector(const divline_t* v2, const divline_t* v1)
{
    float frac = 0;
    fixed_t den = FixedMul(v1->dY >> 8, v2->dX) - FixedMul(v1->dX >> 8, v2->dY);

    if(den)
    {
        fixed_t             f;

        f = FixedMul((v1->pos[VX] - v2->pos[VX]) >> 8, v1->dY) +
            FixedMul((v2->pos[VY] - v1->pos[VY]) >> 8, v1->dX);

        f = FixedDiv(f, den);

        frac = FIX2FLT(f);
    }

    return frac;
}

/**
 * Sets opentop and openbottom to the window through a two sided line.
 * \fixme $nplanes.
 */
void P_LineOpening(linedef_t* linedef)
{
    sector_t*           front, *back;

    if(!LINE_BACKSIDE(linedef))
    {   // Single sided line.
        openrange = 0;
        return;
    }

    front = LINE_FRONTSECTOR(linedef);
    back = LINE_BACKSECTOR(linedef);

    if(front->SP_ceilheight < back->SP_ceilheight)
        opentop = front->SP_ceilheight;
    else
        opentop = back->SP_ceilheight;

    if(front->SP_floorheight > back->SP_floorheight)
    {
        openbottom = front->SP_floorheight;
        lowfloor = back->SP_floorheight;
    }
    else
    {
        openbottom = back->SP_floorheight;
        lowfloor = front->SP_floorheight;
    }

    openrange = opentop - openbottom;
}

void DMU_LineOpening(void* p)
{
    P_LineOpening(((objectrecord_t*) p)->obj);
}

/**
 * Unlinks a mobj from everything it has been linked to.
 *
 * @param mo            Ptr to the mobj to be unlinked.
 * @return              DDLINK_* flags denoting what the mobj was unlinked
 *                      from (in case we need to re-link).
 */
int P_MobjUnlink(mobj_t* mo)
{
    return Map_UnlinkMobj(Thinker_Map((thinker_t*) mo), mo);
}

/**
 * Links a mobj into both a block and a subsector based on it's (x,y).
 * Sets mobj->subsector properly. Calling with flags==0 only updates
 * the subsector pointer. Can be called without unlinking first.
 */
void P_MobjLink(mobj_t* mo, byte flags)
{
    Map_LinkMobj(Thinker_Map((thinker_t*) mo), mo, flags);
}

/**
 * The callback function will be called once for each line that crosses
 * trough the object. This means all the lines will be two-sided.
 */
boolean P_MobjLinesIterator(const mobj_t* mo, boolean (*func) (linedef_t*, void*),
                            void* data)
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
    nodeindex_t nix;
    linknode_t* tn;
    map_t* map;

    if(!mo)
        return true;
    if(!func)
        return true;
    if(!mo->lineRoot)
        return true; // No lines to process.

    map = Thinker_Map((thinker_t*) mo);

    tn = map->mobjNodes->nodes;
    for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
        nix = tn[nix].next)
        *end++ = tn[nix].ptr;

    DO_LINKS(it, end);
    return true;

#undef MAXLINKED
#undef DO_LINKS
}

/**
 * Increment validCount before calling this routine. The callback function
 * will be called once for each sector the mobj is touching (totally or
 * partly inside). This is not a 3D check; the mobj may actually reside
 * above or under the sector.
 */
boolean P_MobjSectorsIterator(const mobj_t* mo,
                              boolean (*func) (sector_t*, void*),
                              void* data)
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
    nodeindex_t nix;
    linknode_t* tn;
    linedef_t* ld;
    sector_t* sec;
    map_t* map;

    if(!mo)
        return true;
    if(!func)
        return true;

    map = Thinker_Map((thinker_t*) mo);
    tn = map->mobjNodes->nodes;

    // Always process the mobj's own sector first.
    *end++ = sec = ((subsector_t*) ((objectrecord_t*) mo->subsector)->obj)->sector;
    sec->validCount = validCount;

    // Any good lines around here?
    if(mo->lineRoot)
    {
        for(nix = tn[mo->lineRoot].next; nix != mo->lineRoot;
            nix = tn[nix].next)
        {
            ld = (linedef_t *) tn[nix].ptr;

            // All these lines are two-sided. Try front side.
            sec = LINE_FRONTSECTOR(ld);
            if(sec->validCount != validCount)
            {
                *end++ = sec;
                sec->validCount = validCount;
            }

            // And then the back side.
            if(LINE_BACKSIDE(ld))
            {
                sec = LINE_BACKSECTOR(ld);
                if(sec->validCount != validCount)
                {
                    *end++ = sec;
                    sec->validCount = validCount;
                }
            }
        }
    }

    DO_LINKS(it, end);
    return true;

#undef MAXLINKED
#undef DO_LINKS
}

boolean P_LineMobjsIterator(linedef_t* lineDef, boolean (*func) (mobj_t*, void*), void* data)
{
    if(!lineDef)
        Con_Error("P_LineMobjsIterator: Invalid LineDef reference.");
    if(!func)
        Con_Error("P_LineMobjsIterator: A callback function must be specified.");
    return LineDef_IterateMobjs(lineDef, func, data);
}

/**
 * Increment validCount before using this. 'func' is called for each mobj
 * that is (even partly) inside the sector. This is not a 3D test, the
 * mobjs may actually be above or under the sector.
 *
 * (Lovely name; actually this is a combination of SectorMobjs and
 * a bunch of LineMobjs iterations.)
 */
boolean P_SectorTouchingMobjsIterator(sector_t* sector, int (*func) (void*, void*), void* data)
{
    if(!sector)
        Con_Error("Sector_IterateMobjsTouching: Invalid Sector reference.");
    if(!func)
        Con_Error("Sector_IterateMobjsTouching: A callback function must be specified.");
    return Sector_IterateMobjsTouching(sector, func, data);
}

static boolean pathTraverseMobjs(mobjblockmap_t* blockmap, float x1, float y1, float x2,
                                 float y2, int flags, boolean (*trav) (intercept_t*))
{
    float origin[2], dest[2];
    uint originBlock[2], destBlock[2];
    fixed_t blockSize[2];
    vec2_t min, max;

    V2_Set(origin, x1, y1);
    V2_Set(dest, x2, y2);

    MobjBlockmap_Bounds(blockmap, min, max);
    {
    vec2_t size;
    MobjBlockmap_BlockSize(blockmap, size);
    blockSize[0] = FLT2FIX(size[0]);
    blockSize[1] = FLT2FIX(size[1]);
    }

    if(!(origin[VX] >= min[VX] && origin[VX] <= max[VX] &&
         origin[VY] >= min[VY] && origin[VY] <= max[VY]))
    {   // Origin is outside the blockmap (really? very unusual...)
        return false;
    }

    // Check the easy case of a path that lies completely outside the bmap.
    if((origin[VX] < min[VX] && dest[VX] < min[VX]) ||
       (origin[VX] > max[VX] && dest[VX] > max[VX]) ||
       (origin[VY] < min[VY] && dest[VY] < min[VY]) ||
       (origin[VY] > max[VY] && dest[VY] > max[VY]))
    {   // Nothing intercepts outside the blockmap!
        return false;
    }

    if((FLT2FIX(origin[VX] - min[VX]) & (blockSize[0] - 1)) == 0)
        origin[VX] += 1; // Don't side exactly on a line.
    if((FLT2FIX(origin[VY] - min[VY]) & (blockSize[1] - 1)) == 0)
        origin[VY] += 1; // Don't side exactly on a line.

    traceLOS.pos[VX] = FLT2FIX(origin[VX]);
    traceLOS.pos[VY] = FLT2FIX(origin[VY]);
    traceLOS.dX = FLT2FIX(dest[VX] - origin[VX]);
    traceLOS.dY = FLT2FIX(dest[VY] - origin[VY]);

    /**
     * It is possible that one or both points are outside the blockmap.
     * Clip path so that dest is within the AABB of the blockmap (note we
     * would have already abandoned if origin lay outside. Also, to avoid
     * potential rounding errors which might occur when determining the
     * blocks later, we will shrink the bbox slightly first.
     */

    if(!(dest[VX] >= min[VX] && dest[VX] <= max[VX] &&
         dest[VY] >= min[VY] && dest[VY] <= max[VY]))
    {   // Dest is outside the blockmap.
        float ab;
        vec2_t bbox[4], point;

        V2_Set(bbox[0], min[VX] + 1, min[VY] + 1);
        V2_Set(bbox[1], min[VX] + 1, max[VY] - 1);
        V2_Set(bbox[2], max[VX] - 1, max[VY] - 1);
        V2_Set(bbox[3], max[VX] - 1, min[VY] + 1);

        ab = V2_Intercept(origin, dest, bbox[0], bbox[1], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[1], bbox[2], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[2], bbox[3], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[3], bbox[0], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);
    }

    if(!(MobjBlockmap_Block2fv(blockmap, originBlock, origin) &&
         MobjBlockmap_Block2fv(blockmap, destBlock, dest)))
    {   // Shouldn't reach here due to the clipping above.
        return false;
    }

    earlyout = flags & PT_EARLYOUT;

    V2_Subtract(origin, origin, min);
    V2_Subtract(dest, dest, min);

    if(!MobjBlockmap_PathTraverse(blockmap, originBlock, destBlock, origin, dest, trav))
        return false; // Early out.

    return true;
}

static boolean pathTraverseLineDefs(linedefblockmap_t* blockmap, float x1, float y1,
                                    float x2, float y2, int flags, boolean (*trav) (intercept_t*))
{
    float origin[2], dest[2];
    uint originBlock[2], destBlock[2];
    fixed_t blockSize[2];
    vec2_t min, max;

    V2_Set(origin, x1, y1);
    V2_Set(dest, x2, y2);

    LineDefBlockmap_Bounds(blockmap, min, max);
    {
    vec2_t size;
    LineDefBlockmap_BlockSize(blockmap, size);
    blockSize[0] = FLT2FIX(size[0]);
    blockSize[1] = FLT2FIX(size[1]);
    }

    if(!(origin[VX] >= min[VX] && origin[VX] <= max[VX] &&
         origin[VY] >= min[VY] && origin[VY] <= max[VY]))
    {   // Origin is outside the blockmap (really? very unusual...)
        return false;
    }

    // Check the easy case of a path that lies completely outside the bmap.
    if((origin[VX] < min[VX] && dest[VX] < min[VX]) ||
       (origin[VX] > max[VX] && dest[VX] > max[VX]) ||
       (origin[VY] < min[VY] && dest[VY] < min[VY]) ||
       (origin[VY] > max[VY] && dest[VY] > max[VY]))
    {   // Nothing intercepts outside the blockmap!
        return false;
    }

    if((FLT2FIX(origin[VX] - min[VX]) & (blockSize[0] - 1)) == 0)
        origin[VX] += 1; // Don't side exactly on a line.
    if((FLT2FIX(origin[VY] - min[VY]) & (blockSize[1] - 1)) == 0)
        origin[VY] += 1; // Don't side exactly on a line.

    traceLOS.pos[VX] = FLT2FIX(origin[VX]);
    traceLOS.pos[VY] = FLT2FIX(origin[VY]);
    traceLOS.dX = FLT2FIX(dest[VX] - origin[VX]);
    traceLOS.dY = FLT2FIX(dest[VY] - origin[VY]);

    /**
     * It is possible that one or both points are outside the blockmap.
     * Clip path so that dest is within the AABB of the blockmap (note we
     * would have already abandoned if origin lay outside. Also, to avoid
     * potential rounding errors which might occur when determining the
     * blocks later, we will shrink the bbox slightly first.
     */

    if(!(dest[VX] >= min[VX] && dest[VX] <= max[VX] &&
         dest[VY] >= min[VY] && dest[VY] <= max[VY]))
    {   // Dest is outside the blockmap.
        float ab;
        vec2_t bbox[4], point;

        V2_Set(bbox[0], min[VX] + 1, min[VY] + 1);
        V2_Set(bbox[1], min[VX] + 1, max[VY] - 1);
        V2_Set(bbox[2], max[VX] - 1, max[VY] - 1);
        V2_Set(bbox[3], max[VX] - 1, min[VY] + 1);

        ab = V2_Intercept(origin, dest, bbox[0], bbox[1], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[1], bbox[2], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[2], bbox[3], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);

        ab = V2_Intercept(origin, dest, bbox[3], bbox[0], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(dest, point);
    }

    if(!(LineDefBlockmap_Block2fv(blockmap, originBlock, origin) &&
         LineDefBlockmap_Block2fv(blockmap, destBlock, dest)))
    {   // Shouldn't reach here due to the clipping above.
        return false;
    }

    earlyout = flags & PT_EARLYOUT;

    V2_Subtract(origin, origin, min);
    V2_Subtract(dest, dest, min);

    if(!LineDefBlockmap_PathTraverse(blockmap, originBlock, destBlock, origin, dest, trav))
        return false; // Early out.

    return true;
}

/**
 * Traces a line from x1,y1 to x2,y2, calling the traverser function for each
 * Returns true if the traverser function returns true for all lines
 */
boolean Map_PathTraverse(map_t* map, float x1, float y1, float x2, float y2,
                         int flags, boolean (*trav) (intercept_t*))
{
    if(!map)
        return true;

    P_ClearIntercepts();

    validCount++;

    if(flags & PT_ADDMOBJS)
    {
        if(!pathTraverseMobjs(map->_mobjBlockmap, x1, y1, x2, y2, flags, trav))
            return false; // Early out.
    }

    if(flags & PT_ADDLINEDEFS)
    {
        if(!pathTraverseLineDefs(map->_lineDefBlockmap, x1, y1, x2, y2, flags, trav))
            return false; // Early out.
    }

    // Go through the sorted list.
    return P_TraverseIntercepts(trav, 1.0f);
}

/**
 * @note Part of the Doomsday public API.
 */
boolean P_PathTraverse(float x1, float y1, float x2, float y2, int flags,
                       boolean (*trav) (intercept_t*))
{
    return Map_PathTraverse(P_CurrentMap(), x1, y1, x2, y2, flags, trav);
}

boolean P_MobjsBoxIterator(const float box[4], boolean (*func) (mobj_t*, void*), void* data)
{
    return Map_MobjsBoxIterator(P_CurrentMap(), box, func, data);
}

/**
 * @note Part of the Doomsday public API.
 */
boolean P_LineDefsBoxIterator(const float box[4], boolean (*func) (linedef_t*, void*),
                           void* data)
{
    vec2_t bounds[2];

    bounds[0][VX] = box[BOXLEFT];
    bounds[0][VY] = box[BOXBOTTOM];
    bounds[1][VX] = box[BOXRIGHT];
    bounds[1][VY] = box[BOXTOP];

    return Map_LineDefsBoxIteratorv(P_CurrentMap(), bounds, func, data, true);
}

/**
 * @note Part of the Doomsday public API.
 */
boolean P_SubsectorsBoxIterator(const float box[4], void* p,
                                boolean (*func) (subsector_t*, void*),
                                void* parm)
{
    return Map_SubsectorsBoxIterator(P_CurrentMap(), box,
        p? ((objectrecord_t*) p)->obj : NULL, func, parm, true);
}
