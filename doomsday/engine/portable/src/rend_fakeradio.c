/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * rend_fakeradio.c: Faked Radiosity Lighting
 *
 * Perhaps the most distinctive characteristic of radiosity lighting
 * is that the corners of a room are slightly dimmer than the rest of
 * the surfaces.  (It's not the only characteristic, however.)  We
 * will fake these shadowed areas by generating shadow polygons for
 * wall segments and determining, which subsector vertices will be
 * shadowed.
 *
 * In other words, walls use shadow polygons (over entire segs), while
 * planes use vertex lighting.  Since planes are usually tesselated
 * into a great deal of subsectors (and triangles), they are better
 * suited for vertex lighting.  In some cases we will be forced to
 * split a subsector into smaller pieces than strictly necessary in
 * order to achieve better accuracy in the shadow effect.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"

#include "m_vector.h"

// MACROS ------------------------------------------------------------------

#define MIN_OPEN                (.1f)
#define EDGE_OPEN_THRESHOLD     (8) // world units (Z axis)

#define MINDIFF                 (8) // min plane height difference (world units)
#define INDIFF                  (8) // max plane height for indifference offset

#define BOTTOM                  (0)
#define TOP                     (1)

// TYPES -------------------------------------------------------------------

typedef struct edge_s {
    boolean         done;
    linedef_t*      line;
    sector_t*       sector;
    float           length;
    binangle_t      diff;
} edge_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void scanEdges(sideradioconfig_t* radioConfig,
                      const linedef_t* line, boolean backSide);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int rendFakeRadio = true; // cvar
float rendFakeRadioDarkness = 1.2f; // cvar

float rendRadioLongWallMin = 400;
float rendRadioLongWallMax = 1500;
float rendRadioLongWallDiv = 30;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Rend_RadioRegister(void)
{
    C_VAR_INT("rend-fakeradio", &rendFakeRadio, 0, 0, 2);
    C_VAR_FLOAT("rend-fakeradio-darkness", &rendFakeRadioDarkness, 0, 0, 2);
}

static __inline float calcShadowDarkness(float lightLevel)
{
    return (0.6f - lightLevel * 0.4f) * rendFakeRadioDarkness;
}

/**
 * Called to update the shadow properties used when doing FakeRadio for the
 * given linedef.
 */
void Rend_RadioUpdateLineDef(linedef_t* line, boolean backSide)
{
    sideradioconfig_t* radioConfig;

    if(!rendFakeRadio || mapFullBright || // Disabled?
       !line)
        return;

    // Have we yet determined the shadow properties to be used with segs
    // on this sidedef?
    radioConfig = &LINE_SIDE(line, backSide)->radioConfig;
    if(radioConfig->fakeRadioUpdateCount != frameCount)
    {   // Not yet. Calculate now.
        uint                i;

        for(i = 0; i < 2; ++i)
        {
            radioConfig->spans[i].length = line->length;
            radioConfig->spans[i].shift = 0;
        }

        scanEdges(radioConfig, line, backSide);

        radioConfig->fakeRadioUpdateCount = frameCount; // Mark as done.
    }
}

/**
 * Set the vertex colors in the rendpoly.
 */
static void setRendpolyColor(rcolor_t* rcolors, uint num, float darkness)
{
    uint                i;

    // Clamp it.
    if(darkness < 0)
        darkness = 0;
    if(darkness > 1)
        darkness = 1;

    // Shadows are black.
    for(i = 0; i < num; ++i)
    {
        rcolors[i].rgba[CR] =
            rcolors[i].rgba[CG] =
                rcolors[i].rgba[CB] = 0;
        rcolors[i].rgba[CA] = darkness;
    }
}

/**
 * @return              @c true, if there is open space in the sector.
 */
static __inline boolean isSectorOpen(sector_t* sector)
{
    return (sector && sector->SP_ceilheight > sector->SP_floorheight);
}

/**
 * Set the rendpoly's X offset and texture size.
 *
 * @param length        If negative; implies that the texture is flipped
 *                      horizontally.
 */
static __inline
float calcTexCoordX(float lineLength, float segOffset)
{
    if(lineLength > 0)
        return segOffset;

    return lineLength + segOffset;
}

/**
 * Set the rendpoly's Y offset and texture size.
 *
 * @param size          If negative; implies that the texture is flipped
 *                      vertically.
 */
static __inline
float calcTexCoordY(float z, float bottom, float top, float texHeight)
{
    if(texHeight > 0)
        return top - z;

    return bottom - z;
}

static void scanNeighbor(boolean scanTop, const linedef_t* line, uint side,
                         edge_t* edge, boolean toLeft)
{
#define SEP             (10)

    linedef_t*          iter;
    lineowner_t*        own;
    binangle_t          diff = 0;
    float               lengthDelta = 0, gap = 0;
    float               iFFloor, iFCeil;
    float               iBFloor, iBCeil;
    int                 scanSecSide = side;
    sector_t*           startSector = LINE_SECTOR(line, side);
    sector_t*           scanSector;
    boolean             clockwise = toLeft;
    boolean             stopScan = false;
    boolean             closed;
    float               fCeil, fFloor;

    fFloor = LINE_SECTOR(line, side)->SP_floorvisheight;
    fCeil  = LINE_SECTOR(line, side)->SP_ceilvisheight;

    // Retrieve the start owner node.
    own = R_GetVtxLineOwner(LINE_VERTEX(line, side^!toLeft), line);

    do
    {
        // Select the next line.
        diff = (clockwise? own->angle : own->LO_prev->angle);
        iter = own->link[clockwise]->lineDef;

        scanSecSide = (LINE_FRONTSECTOR(iter) == startSector);

        // Step over selfreferencing lines?
        while(LINE_SELFREF(iter))
        {
            own = own->link[clockwise];
            diff += (clockwise? own->angle : own->LO_prev->angle);
            iter = own->link[clockwise]->lineDef;

            scanSecSide = (LINE_FRONTSECTOR(iter) == startSector);
        }

        // Determine the relative backsector.
        if(LINE_SIDE(iter, scanSecSide))
            scanSector = LINE_SECTOR(iter, scanSecSide);
        else
            scanSector = NULL;

        // Pick plane heights for relative offset comparison.
        if(!stopScan)
        {
            iFFloor = LINE_FRONTSECTOR(iter)->SP_floorvisheight;
            iFCeil  = LINE_FRONTSECTOR(iter)->SP_ceilvisheight;

            if(LINE_BACKSIDE(iter))
            {
                iBFloor = LINE_BACKSECTOR(iter)->SP_floorvisheight;
                iBCeil  = LINE_BACKSECTOR(iter)->SP_ceilvisheight;
            }
            else
                iBFloor = iBCeil = 0;
        }

        lengthDelta = 0;
        if(!stopScan)
        {
            // This line will attribute to this seg's shadow edge.
            // Store identity for later use.
            edge->diff = diff;
            edge->line = iter;
            edge->sector = scanSector;

            closed = false;
            if(side == 0 && LINE_BACKSIDE(iter))
            {
                if(scanTop)
                {
                    if(iBFloor >= fCeil)
                        closed = true; // Compared to "this" sector anyway
                }
                else
                {
                    if(iBCeil <= fFloor)
                        closed = true; // Compared to "this" sector anyway
                }
            }

            // Does this line's length contribute to the alignment of the
            // texture on the seg shadow edge being rendered?
            if(scanTop)
            {
                if(LINE_BACKSIDE(iter) &&
                   ((side == 0 && LINE_BACKSECTOR(iter) == LINE_FRONTSECTOR(line) &&
                    iFCeil >= fCeil) ||
                   (side == 1 && LINE_BACKSECTOR(iter) == LINE_BACKSECTOR(line) &&
                    iFCeil >= fCeil) ||
                    (side == 0 && closed == false && LINE_BACKSECTOR(iter) != LINE_FRONTSECTOR(line) &&
                    iBCeil >= fCeil &&
                    isSectorOpen(LINE_BACKSECTOR(iter)))))
                {
                    gap += iter->length; // Should we just mark it done instead?
                }
                else
                {
                    edge->length += iter->length + gap;
                    gap = 0;
                }
            }
            else
            {
                if(LINE_BACKSIDE(iter) &&
                   ((side == 0 && LINE_BACKSECTOR(iter) == LINE_FRONTSECTOR(line) &&
                    iFFloor <= fFloor) ||
                   (side == 1 && LINE_BACKSECTOR(iter) == LINE_BACKSECTOR(line) &&
                    iFFloor <= fFloor) ||
                   (side == 0 && closed == false && LINE_BACKSECTOR(iter) != LINE_FRONTSECTOR(line) &&
                    iBFloor <= fFloor &&
                    isSectorOpen(LINE_BACKSECTOR(iter)))))
                {
                    gap += iter->length; // Should we just mark it done instead?
                }
                else
                {
                    lengthDelta = iter->length + gap;
                    gap = 0;
                }
            }
        }

        // Time to stop?
        if(iter == line)
        {
            stopScan = true;
        }
        else
        {
            // Is this line coalignable?
            if(!(diff >= BANG_180 - SEP && diff <= BANG_180 + SEP))
                stopScan = true; // no.
            else if(scanSector)
            {
                // Perhaps its a closed edge?
                if(!isSectorOpen(scanSector))
                {
                    stopScan = true;
                }
                else
                {
                    // A height difference from the start sector?
                    if(scanTop)
                    {
                        if(scanSector->SP_ceilvisheight != fCeil &&
                           scanSector->SP_floorvisheight <
                                startSector->SP_ceilvisheight)
                            stopScan = true;
                    }
                    else
                    {
                        if(scanSector->SP_floorvisheight != fFloor &&
                           scanSector->SP_ceilvisheight >
                                startSector->SP_floorvisheight)
                            stopScan = true;
                    }
                }
            }
        }

        // Swap to the iter line's owner node (i.e: around the corner)?
        if(!stopScan)
        {
            // Around the corner.
            if(own->link[clockwise] == iter->L_vo2)
                own = iter->L_vo1;
            else if(own->link[clockwise] == iter->L_vo1)
                own = iter->L_vo2;

            // Skip into the back neighbor sector of the iter line if
            // heights are within accepted range.
            if(scanSector && LINE_SIDE(line, side^1) &&
               scanSector != LINE_SECTOR(line, side^1) &&
                ((scanTop && scanSector->SP_ceilvisheight ==
                                startSector->SP_ceilvisheight) ||
                 (!scanTop && scanSector->SP_floorvisheight ==
                                startSector->SP_floorvisheight)))
            {
                // If the map is formed correctly, we should find a back
                // neighbor attached to this line. However, if this is not
                // the case and a line which SHOULD be two sided isn't, we
                // need to check whether there is a valid neighbor.
                linedef_t *backNeighbor =
                    R_FindLineNeighbor(startSector, iter, own, !toLeft, NULL);

                if(backNeighbor && backNeighbor != iter)
                {
                    // Into the back neighbor sector.
                    own = own->link[clockwise];
                    startSector = scanSector;
                }
            }

            // The last line was co-alignable so apply any length delta.
            edge->length += lengthDelta;
        }
    } while(!stopScan);

    // Now we've found the furthest coalignable neighbor, select the back
    // neighbor if present for "edge open-ness" comparison.
    if(edge->sector) // the back sector of the coalignable neighbor.
    {
        // Since we have the details of the backsector already, simply
        // get the next neighbor (it IS the backneighbor).
        edge->line =
            R_FindLineNeighbor(edge->sector, edge->line,
                               edge->line->vo[(LINE_BACKSIDE(edge->line) && LINE_BACKSECTOR(edge->line) == edge->sector)^!toLeft],
                               !toLeft, &edge->diff);
    }

#undef SEP
}

static void scanNeighbors(sideradioconfig_t* radioConfig,
                          const linedef_t* line, uint side, boolean toLeft)
{
    uint                i;
    edge_t              edges[2], *edge; // {bottom, top}
    edgespan_t*         span;
    shadowcorner_t*     corner;
    float               fCeil, fFloor;

    if(LINE_SELFREF(line))
        return;

    fFloor = LINE_SECTOR(line, side)->SP_floorvisheight;
    fCeil  = LINE_SECTOR(line, side)->SP_ceilvisheight;

    memset(edges, 0, sizeof(edges));

    scanNeighbor(false, line, side, &edges[0], toLeft);
    scanNeighbor(true, line, side, &edges[1], toLeft);

    for(i = 0; i < 2; ++i) // 0=bottom, 1=top
    {
        corner = (i == 0 ? &radioConfig->bottomCorners[!toLeft] : &radioConfig->topCorners[!toLeft]);
        edge = &edges[i];
        span = &radioConfig->spans[i];

        // Increment the apparent line length/offset.
        span->length += edge->length;
        if(toLeft)
            span->shift += edge->length;

        // Compare the relative angle difference of this edge to determine
        // an "open-ness" factor.
        if(edge->line && edge->line != line)
        {
            if(edge->diff > BANG_180)
            {   // The corner between the walls faces outwards.
                corner->corner = -1;
            }
            else if(edge->diff == BANG_180)
            {   // Perfectly coaligned? Great.
                corner->corner = 0;
            }
            else if(edge->diff < BANG_45 / 5)
            {   // The difference is too small, there won't be a shadow.
                corner->corner = 0;
            }
            // 90 degrees is the largest effective difference.
            else if(edge->diff > BANG_90)
            {
                corner->corner = (float) BANG_90 / edge->diff;
            }
            else
            {
                corner->corner = (float) edge->diff / BANG_90;
            }
        }
        else
        {   // Consider it coaligned.
            corner->corner = 0;
        }

        // Determine relative height offsets (affects shadow map selection).
        if(edge->sector)
        {
            corner->proximity = edge->sector;
            if(i == 0) // Floor.
            {
                corner->pOffset = corner->proximity->SP_floorvisheight - fFloor;
                corner->pHeight = corner->proximity->SP_floorvisheight;
            }
            else // Ceiling.
            {
                corner->pOffset = corner->proximity->SP_ceilvisheight - fCeil;
                corner->pHeight = corner->proximity->SP_ceilvisheight;
            }
        }
        else
        {
            corner->proximity = NULL;
            corner->pOffset = 0;
            corner->pHeight = 0;
        }
    }
}

/**
 * To determine the dimensions of a shadow, we'll need to scan edges. Edges
 * are composed of aligned lines. It's important to note that the scanning
 * is done separately for the top/bottom edges (both in the left and right
 * direction) and the left/right edges.
 *
 * The length of the top/bottom edges are returned in the array 'spans'.
 *
 * This may look like a complicated operation (performed for all wall polys)
 * but in most cases this won't take long. Aligned neighbours are relatively
 * rare.
 */
static void scanEdges(sideradioconfig_t* radioConfig,
                      const linedef_t* line, boolean backSide)
{
    uint                i, sid = (backSide? BACK : FRONT);
    sidedef_t*          side;
    linedef_t*          other;

    side = LINE_SIDE(line, sid);

    memset(radioConfig->sideCorners, 0, sizeof(radioConfig->sideCorners));

    // Find the sidecorners first: left and right neighbour.
    for(i = 0; i < 2; ++i)
    {
        binangle_t          diff = 0;
        lineowner_t*        vo;

        vo = line->L_vo(i^sid);

        other = R_FindSolidLineNeighbor(LINE_SECTOR(line, sid), line,
                                        vo, i, &diff);
        if(other && other != line)
        {
            if(diff > BANG_180)
            {   // The corner between the walls faces outwards.
                radioConfig->sideCorners[i].corner = -1;
            }
            else if(diff == BANG_180)
            {
                radioConfig->sideCorners[i].corner = 0;
            }
            else if(diff < BANG_45 / 5)
            {   // The difference is too small, there won't be a shadow.
                radioConfig->sideCorners[i].corner = 0;
            }
            else if(diff > BANG_90)
            {   // 90 degrees is the largest effective difference.
                radioConfig->sideCorners[i].corner = (float) BANG_90 / diff;
            }
            else
            {
                radioConfig->sideCorners[i].corner = (float) diff / BANG_90;
            }
        }
        else
            radioConfig->sideCorners[i].corner = 0;

        scanNeighbors(radioConfig, line, sid, !i);
    }
}

/**
 * Long walls get slightly larger shadows. The bonus will simply be added
 * to the shadow size for the wall in question.
 */
static __inline float calcLongWallBonus(float span)
{
    float               limit;

    if(rendRadioLongWallDiv > 0 && span > rendRadioLongWallMin)
    {
        limit = span - rendRadioLongWallMin;
        if(limit > rendRadioLongWallMax)
            limit = rendRadioLongWallMax;
        return limit / rendRadioLongWallDiv;
    }
    return 0;
}

typedef struct {
    lightingtexid_t     texture;
    boolean             horizontal;
    float               shadowMul;
    float               texWidth;
    float               texHeight;
    float               texOffset[2];
    float               wallLength;
} rendershadowseg_params_t;

static void setTopShadowParams(rendershadowseg_params_t* p, float size,
                               float top,
                               float xOffset, float segLength,
                               const float* fFloor, const float* fCeil,
                               const sideradioconfig_t* radioConfig)
{
    p->shadowMul = 1;
    p->horizontal = false;
    p->texHeight = size;
    p->texOffset[VY] = calcTexCoordY(top, *fFloor, *fCeil, p->texHeight);
    p->wallLength = segLength;

    p->texture = LST_RADIO_OO;
    // Corners without a neighbour backsector
    if(radioConfig->sideCorners[0].corner == -1 || radioConfig->sideCorners[1].corner == -1)
    {   // At least one corner faces outwards
        p->texture = LST_RADIO_OO;
        p->texWidth = radioConfig->spans[TOP].length;
        p->texOffset[VX] =
            calcTexCoordX(radioConfig->spans[TOP].length, radioConfig->spans[TOP].shift + xOffset);

        if((radioConfig->sideCorners[0].corner == -1 && radioConfig->sideCorners[1].corner == -1) ||
           (radioConfig->topCorners[0].corner == -1 && radioConfig->topCorners[1].corner == -1))
        {   // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(radioConfig->sideCorners[1].corner == -1)
        {   // right corner faces outwards
            if(-radioConfig->topCorners[0].pOffset < 0 && radioConfig->bottomCorners[0].pHeight < *fCeil)
            {// Must flip horizontally!
                p->texWidth = -radioConfig->spans[TOP].length;
                p->texOffset[VX] =
                    calcTexCoordX(-radioConfig->spans[TOP].length, radioConfig->spans[TOP].shift + xOffset);
                p->texture = LST_RADIO_OE;
            }
        }
        else  // left corner faces outwards
        {
            if(-radioConfig->topCorners[1].pOffset < 0 && radioConfig->bottomCorners[1].pHeight < *fCeil)
            {
                p->texture = LST_RADIO_OE;
            }
        }
    }
    else
    {   // Corners WITH a neighbour backsector
        p->texWidth = radioConfig->spans[TOP].length;
        p->texOffset[VX] =
            calcTexCoordX(radioConfig->spans[TOP].length, radioConfig->spans[TOP].shift + xOffset);
        if(radioConfig->topCorners[0].corner == -1 && radioConfig->topCorners[1].corner == -1)
        {   // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(radioConfig->topCorners[1].corner == -1 && radioConfig->topCorners[0].corner > MIN_OPEN)
        {   // Right corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        else if(radioConfig->topCorners[0].corner == -1 && radioConfig->topCorners[1].corner > MIN_OPEN)
        {   // Left corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        // Open edges
        else if(radioConfig->topCorners[0].corner <= MIN_OPEN && radioConfig->topCorners[1].corner <= MIN_OPEN)
        {   // Both edges are open
            p->texture = LST_RADIO_OO;
            if(radioConfig->topCorners[0].proximity && radioConfig->topCorners[1].proximity)
            {
                if(-radioConfig->topCorners[0].pOffset >= 0 && -radioConfig->topCorners[1].pOffset < 0)
                {
                    p->texture = LST_RADIO_CO;
                    // The shadow can't go over the higher edge.
                    if(size > -radioConfig->topCorners[0].pOffset)
                    {
                        if(-radioConfig->topCorners[0].pOffset < INDIFF)
                            p->texture = LST_RADIO_OE;
                        else
                        {
                            p->texHeight = -radioConfig->topCorners[0].pOffset;
                            p->texOffset[VY] =
                                calcTexCoordY(top, *fFloor, *fCeil,
                                              p->texHeight);
                        }
                    }
                }
                else if(-radioConfig->topCorners[0].pOffset < 0 && -radioConfig->topCorners[1].pOffset >= 0)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_CO;
                    p->texWidth = -radioConfig->spans[TOP].length;
                    p->texOffset[VX] =
                        calcTexCoordX(-radioConfig->spans[TOP].length, radioConfig->spans[TOP].shift + xOffset);

                    // The shadow can't go over the higher edge.
                    if(size > -radioConfig->topCorners[1].pOffset)
                    {
                        if(-radioConfig->topCorners[1].pOffset < INDIFF)
                            p->texture = LST_RADIO_OE;
                        else
                        {
                            p->texHeight = -radioConfig->topCorners[1].pOffset;
                            p->texOffset[VY] =
                                calcTexCoordY(top, *fFloor, *fCeil,
                                              p->texHeight);
                        }
                    }
                }
            }
            else
            {
                if(-radioConfig->topCorners[0].pOffset < -MINDIFF)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_OE;
                    p->texWidth = -radioConfig->spans[BOTTOM].length;
                    p->texOffset[VX] =
                        calcTexCoordX(-radioConfig->spans[BOTTOM].length,
                                      radioConfig->spans[BOTTOM].shift + xOffset);
                }
                else if(-radioConfig->topCorners[1].pOffset < -MINDIFF)
                    p->texture = LST_RADIO_OE;
            }
        }
        else if(radioConfig->topCorners[0].corner <= MIN_OPEN)
        {
            if(-radioConfig->topCorners[0].pOffset < 0)
                p->texture = LST_RADIO_CO;
            else
                p->texture = LST_RADIO_OO;

            // Must flip horizontally!
            p->texWidth = -radioConfig->spans[TOP].length;
            p->texOffset[VX] =
                calcTexCoordX(-radioConfig->spans[TOP].length, radioConfig->spans[TOP].shift + xOffset);
        }
        else if(radioConfig->topCorners[1].corner <= MIN_OPEN)
        {
            if(-radioConfig->topCorners[1].pOffset < 0)
                p->texture = LST_RADIO_CO;
            else
                p->texture = LST_RADIO_OO;
        }
        else  // C/C ???
        {
            p->texture = LST_RADIO_OO;
        }
    }
}

static void setBottomShadowParams(rendershadowseg_params_t *p, float size,
                                  float top,
                                  float xOffset, float segLength,
                                  const float* fFloor, const float* fCeil,
                                  const sideradioconfig_t* radioConfig)
{
    p->shadowMul = 1;
    p->horizontal = false;
    p->texHeight = -size;
    p->texOffset[VY] = calcTexCoordY(top, *fFloor, *fCeil, p->texHeight);
    p->wallLength = segLength;

    p->texture = LST_RADIO_OO;
    // Corners without a neighbour backsector
    if(radioConfig->sideCorners[0].corner == -1 || radioConfig->sideCorners[1].corner == -1)
    {   // At least one corner faces outwards
        p->texture = LST_RADIO_OO;
        p->texWidth = radioConfig->spans[BOTTOM].length;
        p->texOffset[VX] =
            calcTexCoordX(radioConfig->spans[BOTTOM].length, radioConfig->spans[BOTTOM].shift + xOffset);

        if((radioConfig->sideCorners[0].corner == -1 && radioConfig->sideCorners[1].corner == -1) ||
           (radioConfig->bottomCorners[0].corner == -1 && radioConfig->bottomCorners[1].corner == -1) )
        {   // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(radioConfig->sideCorners[1].corner == -1) // right corner faces outwards
        {
            if(radioConfig->bottomCorners[0].pOffset < 0 && radioConfig->topCorners[0].pHeight > *fFloor)
            {   // Must flip horizontally!
                p->texWidth = -radioConfig->spans[BOTTOM].length;
                p->texOffset[VX] =
                    calcTexCoordX(-radioConfig->spans[BOTTOM].length, radioConfig->spans[BOTTOM].shift + xOffset);
                p->texture = LST_RADIO_OE;
            }
        }
        else
        {   // left corner faces outwards
            if(radioConfig->bottomCorners[1].pOffset < 0 && radioConfig->topCorners[1].pHeight > *fFloor)
            {
                p->texture = LST_RADIO_OE;
            }
        }
    }
    else
    {   // Corners WITH a neighbour backsector
        p->texWidth = radioConfig->spans[BOTTOM].length;
        p->texOffset[VX] =
            calcTexCoordX(radioConfig->spans[BOTTOM].length, radioConfig->spans[BOTTOM].shift + xOffset);
        if(radioConfig->bottomCorners[0].corner == -1 && radioConfig->bottomCorners[1].corner == -1)
        {   // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(radioConfig->bottomCorners[1].corner == -1 && radioConfig->bottomCorners[0].corner > MIN_OPEN)
        {   // Right corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        else if(radioConfig->bottomCorners[0].corner == -1 && radioConfig->bottomCorners[1].corner > MIN_OPEN)
        {   // Left corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        // Open edges
        else if(radioConfig->bottomCorners[0].corner <= MIN_OPEN && radioConfig->bottomCorners[1].corner <= MIN_OPEN)
        {   // Both edges are open
            p->texture = LST_RADIO_OO;

            if(radioConfig->bottomCorners[0].proximity && radioConfig->bottomCorners[1].proximity)
            {
                if(radioConfig->bottomCorners[0].pOffset >= 0 && radioConfig->bottomCorners[1].pOffset < 0)
                {
                    p->texture = LST_RADIO_CO;
                    // The shadow can't go over the higher edge.
                    if(size > radioConfig->bottomCorners[0].pOffset)
                    {
                        if(radioConfig->bottomCorners[0].pOffset < INDIFF)
                            p->texture = LST_RADIO_OE;
                        else
                        {
                            p->texHeight = -radioConfig->bottomCorners[0].pOffset;
                            p->texOffset[VY] =
                                calcTexCoordY(top, *fFloor, *fCeil,
                                              p->texHeight);
                        }
                    }
                }
                else if(radioConfig->bottomCorners[0].pOffset < 0 && radioConfig->bottomCorners[1].pOffset >= 0)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_CO;
                    p->texWidth = -radioConfig->spans[BOTTOM].length;
                    p->texOffset[VX] =
                        calcTexCoordX(-radioConfig->spans[BOTTOM].length,
                                      radioConfig->spans[BOTTOM].shift + xOffset);

                    if(size > radioConfig->bottomCorners[1].pOffset)
                    {
                        if(radioConfig->bottomCorners[1].pOffset < INDIFF)
                            p->texture = LST_RADIO_OE;
                        else
                        {
                            p->texHeight = -radioConfig->bottomCorners[1].pOffset;
                            p->texOffset[VY] =
                                calcTexCoordY(top, *fFloor, *fCeil,
                                              p->texHeight);
                        }
                    }
                }
            }
            else
            {
                if(radioConfig->bottomCorners[0].pOffset < -MINDIFF)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_OE;
                    p->texWidth = -radioConfig->spans[BOTTOM].length;
                    p->texOffset[VX] =
                        calcTexCoordX(-radioConfig->spans[BOTTOM].length,
                                      radioConfig->spans[BOTTOM].shift + xOffset);
                }
                else if(radioConfig->bottomCorners[1].pOffset < -MINDIFF)
                    p->texture = LST_RADIO_OE;
            }
        }
        else if(radioConfig->bottomCorners[0].corner <= MIN_OPEN) // Right Corner is Closed
        {
            if(radioConfig->bottomCorners[0].pOffset < 0)
                p->texture = LST_RADIO_CO;
            else
                p->texture = LST_RADIO_OO;

            // Must flip horizontally!
            p->texWidth = -radioConfig->spans[BOTTOM].length;
            p->texOffset[VX] =
                calcTexCoordX(-radioConfig->spans[BOTTOM].length, radioConfig->spans[BOTTOM].shift + xOffset);
        }
        else if(radioConfig->bottomCorners[1].corner <= MIN_OPEN)  // Left Corner is closed
        {
            if(radioConfig->bottomCorners[1].pOffset < 0)
                p->texture = LST_RADIO_CO;
            else
                p->texture = LST_RADIO_OO;
        }
        else  // C/C ???
        {
            p->texture = LST_RADIO_OO;
        }
    }
}

static void setSideShadowParams(rendershadowseg_params_t* p,
                                float size, float bottom, float top,
                                boolean rightSide, boolean bottomGlow,
                                boolean topGlow,
                                float xOffset, float segLength,
                                const float* fFloor, const float* fCeil,
                                const float* bFloor, const float* bCeil,
                                float lineLength,
                                const shadowcorner_t* sideCn)
{
    p->shadowMul = sideCn[rightSide? 1 : 0].corner;
    p->shadowMul *= p->shadowMul * p->shadowMul;
    p->horizontal = true;
    p->texOffset[VY] = bottom - *fFloor;
    p->texHeight = *fCeil - *fFloor;
    p->wallLength = segLength;

    if(rightSide)
    {   // Right shadow.
        p->texOffset[VX] = -(lineLength) + xOffset;
        // Make sure the shadow isn't too big
        if(size > lineLength)
        {
            if(sideCn[0].corner <= MIN_OPEN)
                p->texWidth = -lineLength;
            else
                p->texWidth = -(lineLength / 2);
        }
        else
            p->texWidth = -size;
    }
    else
    {   // Left shadow.
        p->texOffset[VX] = xOffset;
        // Make sure the shadow isn't too big
        if(size > lineLength)
        {
            if(sideCn[1].corner <= MIN_OPEN)
                p->texWidth = lineLength;
            else
                p->texWidth = lineLength / 2;
        }
        else
            p->texWidth = size;
    }

    if(bFloor)
    {   // There is a backside.
        if(*bFloor > *fFloor && *bCeil < *fCeil)
        {
            if(!bottomGlow && !topGlow)
            {
                p->texture = LST_RADIO_CC;
            }
            else if(bottomGlow)
            {
                p->texOffset[VY] = bottom - *fCeil;
                p->texHeight = -(*fCeil - *fFloor);
                p->texture = LST_RADIO_CO;
            }
            else
                p->texture = LST_RADIO_CO;
        }
        else if(*bFloor > *fFloor)
        {
            if(!bottomGlow && !topGlow)
            {
                p->texture = LST_RADIO_CC;
            }
            else if(bottomGlow)
            {
                p->texOffset[VY] = bottom - *fCeil;
                p->texHeight = -(*fCeil - *fFloor);
                p->texture = LST_RADIO_CO;
            }
            else
                p->texture = LST_RADIO_CO;
        }
        else if(*bCeil < *fCeil)
        {
            if(!bottomGlow && !topGlow)
            {
                p->texture = LST_RADIO_CC;
            }
            else if(bottomGlow)
            {
                p->texOffset[VY] = bottom - *fCeil;
                p->texHeight = -(*fCeil - *fFloor);
                p->texture = LST_RADIO_CO;
            }
            else
                p->texture = LST_RADIO_CO;
        }
    }
    else
    {
        if(bottomGlow)
        {
            p->texHeight = -(*fCeil - *fFloor);
            p->texOffset[VY] =
                calcTexCoordY(top, *fFloor, *fCeil, p->texHeight);

            p->texture = LST_RADIO_CO;
        }
        else if(topGlow)
            p->texture = LST_RADIO_CO;
        else
            p->texture = LST_RADIO_CC;
    }
}

static void quadTexCoords(rtexcoord_t* tc, const rvertex_t* rverts,
                          float wallLength, float texWidth, float texHeight,
                          const float texOrigin[2][3], const float texOffset[2],
                          boolean horizontal)
{
    if(horizontal)
    {   // Special horizontal coordinates for wall shadows.
        tc[0].st[0] = tc[2].st[0] =
            rverts[0].pos[VX] - texOrigin[0][VX] + texOffset[VY] / texHeight;
        tc[0].st[1] = tc[1].st[1] =
            rverts[0].pos[VY] - texOrigin[0][VY] + texOffset[VX] / texWidth;
        tc[1].st[0] = tc[0].st[0] + (rverts[1].pos[VZ] - texOrigin[0][VZ]) / texHeight;
        tc[3].st[0] = tc[0].st[0] + (rverts[3].pos[VZ] - texOrigin[0][VZ]) / texHeight;
        tc[3].st[1] = tc[0].st[1] + wallLength / texWidth;
        tc[2].st[1] = tc[0].st[1] + wallLength / texWidth;
        return;
    }

    tc[0].st[0] = tc[1].st[0] = rverts[0].pos[VX] - texOrigin[0][VX] +
        texOffset[VX] / texWidth;
    tc[3].st[1] = tc[1].st[1] = rverts[0].pos[VY] - texOrigin[0][VY] +
        texOffset[VY] / texHeight;
    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength / texWidth;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]) / texHeight;
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]) / texHeight;
}

static void renderShadowSeg(const rvertex_t* origVertices,
                            const walldiv_t* wdivs,
                            const rendershadowseg_params_t* p,
                            float shadowDark)
{
    float               texOrigin[2][3];
    rcolor_t*           rcolors;
    rtexcoord_t*        rtexcoords;
    rtexmapunit_t       rTU[NUM_TEXMAP_UNITS];
    uint                realNumVertices;

    if(wdivs && (wdivs[0].num || wdivs[1].num))
    {
        realNumVertices = 3 + wdivs[0].num + 3 + wdivs[1].num;
    }
    else
    {
        realNumVertices = 4;
    }

    memset(rTU, 0, sizeof(rTU));
    rTU[TU_PRIMARY].tex = GL_PrepareLSTexture(p->texture);
    rTU[TU_PRIMARY].magMode = GL_LINEAR;
    rTU[TU_PRIMARY].blend = 1;

    // Top left.
    texOrigin[0][VX] = origVertices[1].pos[VX];
    texOrigin[0][VY] = origVertices[1].pos[VY];
    texOrigin[0][VZ] = origVertices[1].pos[VZ];

    // Bottom right.
    texOrigin[1][VX] = origVertices[2].pos[VX];
    texOrigin[1][VY] = origVertices[2].pos[VY];
    texOrigin[1][VZ] = origVertices[2].pos[VZ];

    // Allocate enough for the divisions too.
    rtexcoords = R_AllocRendTexCoords(realNumVertices);
    rcolors = R_AllocRendColors(realNumVertices);

    quadTexCoords(rtexcoords, origVertices, p->wallLength, p->texWidth,
                  p->texHeight, texOrigin, p->texOffset,
                  p->horizontal);

    setRendpolyColor(rcolors, 4, p->shadowMul * shadowDark);

    if(rendFakeRadio != 2)
    {
        // Write multiple polys depending on rend params.
        if(wdivs && (wdivs[0].num || wdivs[1].num))
        {
            float               bL, tL, bR, tR;
            rvertex_t*          rvertices;
            rtexcoord_t         origTexCoords[4];
            rcolor_t            origColors[4];

            /**
             * Need to swap indices around into fans set the position
             * of the division vertices, interpolate texcoords and
             * color.
             */

            rvertices = R_AllocRendVertices(realNumVertices);

            memcpy(origTexCoords, rtexcoords, sizeof(rtexcoord_t) * 4);
            memcpy(origColors, rcolors, sizeof(rcolor_t) * 4);

            bL = origVertices[0].pos[VZ];
            tL = origVertices[1].pos[VZ];
            bR = origVertices[2].pos[VZ];
            tR = origVertices[3].pos[VZ];

            R_DivVerts(rvertices, origVertices, wdivs);
            R_DivTexCoords(rtexcoords, origTexCoords, wdivs, bL, tL, bR, tR);
            R_DivVertColors(rcolors, origColors, wdivs, bL, tL, bR, tR);

            RL_AddPoly(PT_FAN, RPT_SHADOW, 3 + wdivs[1].num, rvertices + 3 + wdivs[0].num,
                       NULL, rtexcoords + 3 + wdivs[0].num, NULL,
                       rcolors + 3 + wdivs[0].num,
                       0, 0, NULL, rTU);
            RL_AddPoly(PT_FAN, RPT_SHADOW, 3 + wdivs[0].num, rvertices, NULL, rtexcoords, NULL,
                       rcolors, 0, 0, NULL, rTU);

            R_FreeRendVertices(rvertices);
        }
        else
        {
            RL_AddPoly(PT_TRIANGLE_STRIP, RPT_SHADOW, 4, origVertices,
                       NULL, rtexcoords, NULL,
                       rcolors, 0, 0, NULL, rTU);
        }
    }

    R_FreeRendTexCoords(rtexcoords);
    R_FreeRendColors(rcolors);
}

/**
 * Create the appropriate FakeRadio shadow polygons for the wall segment.
 */
static void rendRadioSegSection(const rvertex_t* rvertices,
                                const walldiv_t* wdivs,
                                float shadowSize, float shadowDark,
                                const sideradioconfig_t* radioConfig,
                                const rendsegradio_params_t* p)
{
    const float*        fFloor, *fCeil, *bFloor, *bCeil;
    float               size;
    boolean             bottomGlow, topGlow;

    bottomGlow = R_IsGlowingPlane(p->frontSec->SP_plane(PLN_FLOOR));
    topGlow = R_IsGlowingPlane(p->frontSec->SP_plane(PLN_CEILING));

    fFloor = &p->frontSec->SP_floorvisheight;
    fCeil = &p->frontSec->SP_ceilvisheight;
    if(p->backSec)
    {
        bFloor = &p->backSec->SP_floorvisheight;
        bCeil  = &p->backSec->SP_ceilvisheight;
    }
    else
        bFloor = bCeil = NULL;

    /*
     * Top Shadow.
     */
    if(!topGlow)
    {
        // The top shadow will reach this far down.
        size = shadowSize + calcLongWallBonus(radioConfig->spans[TOP].length);
        if(rvertices[3].pos[VZ] > *fCeil - size &&
           rvertices[0].pos[VZ] < *fCeil)
        {
            rendershadowseg_params_t params;

            setTopShadowParams(&params, size, rvertices[1].pos[VZ],
                               p->segOffset, p->segLength, fFloor, fCeil,
                               radioConfig);
            renderShadowSeg(rvertices, wdivs, &params, shadowDark);
        }
    }

    /*
     * Bottom Shadow.
     */
    if(!bottomGlow)
    {
        size = shadowSize + calcLongWallBonus(radioConfig->spans[BOTTOM].length);
        if(rvertices[0].pos[VZ] < *fFloor + size &&
           rvertices[3].pos[VZ] > *fFloor)
        {
            rendershadowseg_params_t params;

            setBottomShadowParams(&params, size, rvertices[1].pos[VZ],
                                  p->segOffset, p->segLength, fFloor, fCeil,
                                  radioConfig);
            renderShadowSeg(rvertices, wdivs, &params, shadowDark);
        }
    }

    // Walls with glowing floor & ceiling get no side shadows.
    // Is there anything better we can do?
    if(bottomGlow && topGlow)
        return;

    size = shadowSize + calcLongWallBonus(p->linedefLength);

    /*
     * Left Shadow.
     */
    if(radioConfig->sideCorners[0].corner > 0 && p->segOffset < size)
    {
        rendershadowseg_params_t params;

        setSideShadowParams(&params, size, rvertices[0].pos[VZ],
                            rvertices[1].pos[VZ], false,
                            bottomGlow, topGlow, p->segOffset, p->segLength,
                            fFloor, fCeil, bFloor, bCeil, p->linedefLength,
                            radioConfig->sideCorners);
        renderShadowSeg(rvertices, wdivs, &params, shadowDark);
    }

    /*
     * Right Shadow.
     */
    if(radioConfig->sideCorners[1].corner > 0 &&
       p->segOffset + p->segLength > p->linedefLength - size)
    {
        rendershadowseg_params_t params;

        setSideShadowParams(&params, size, rvertices[0].pos[VZ],
                            rvertices[1].pos[VZ], true,
                            bottomGlow, topGlow, p->segOffset, p->segLength,
                            fFloor, fCeil, bFloor, bCeil, p->linedefLength,
                            radioConfig->sideCorners);
        renderShadowSeg(rvertices, wdivs, &params, shadowDark);
    }
}

/**
 * Render FakeRadio for the given seg section.
 */
void Rend_RadioSegSection(const rvertex_t* rvertices, const walldiv_t* wdivs,
                          const sideradioconfig_t* radioConfig,
                          const rendsegradio_params_t* params)
{
    float               lightLevel, shadowSize, shadowDark;

    if(!rendFakeRadio || !renderTextures || mapFullBright) // Disabled?
        return;

    if(!rvertices || !params)
        return; // Wha?

    lightLevel = params->sectorLightLevel;
    R_ApplyLightAdaptation(&lightLevel);
    if(!(lightLevel > 0))
        return; // No point drawing shadows in a PITCH black sector.

    // Determine the shadow properties.
    // \fixme Make cvars out of constants.
    shadowSize = 2 * (8 + 16 - lightLevel * 16);

    if(!(shadowSize > 0))
        return;

    shadowDark = calcShadowDarkness(lightLevel) *.8f;

    rendRadioSegSection(rvertices, wdivs, shadowSize, shadowDark, radioConfig, params);
}

/**
 * Returns a value in the range of 0...2, which depicts how open the
 * specified edge is. Zero means that the edge is completely closed: it is
 * facing a wall or is relatively distant from the edge on the other side.
 * Values between zero and one describe how near the other edge is. An
 * openness value of one means that the other edge is at the same height as
 * this one. 2 means that the other edge is past our height ("clearly open").
 */
static float radioEdgeOpenness(float fz, float bz, float bhz)
{
    if(fz <= bz - EDGE_OPEN_THRESHOLD || fz >= bhz)
        return 0; // Fully closed.

    if(fz >= bhz - EDGE_OPEN_THRESHOLD)
        return (bhz - fz) / EDGE_OPEN_THRESHOLD;

    if(fz <= bz)
        return 1 - (bz - fz) / EDGE_OPEN_THRESHOLD;

    if(fz <= bz + EDGE_OPEN_THRESHOLD)
        return 1 + (fz - bz) / EDGE_OPEN_THRESHOLD;

    // Fully open!
    return 2;
}

static void setRelativeHeights(sector_t *front, sector_t *back,
                               boolean isCeiling,
                               float *fz, float *bz, float *bhz)
{
    if(fz)
    {
        *fz  = front->planes[isCeiling? PLN_CEILING:PLN_FLOOR]->visHeight;
        if(isCeiling)
            *fz = -(*fz);
    }
    if(bz)
    {
        *bz  =  back->planes[isCeiling? PLN_CEILING:PLN_FLOOR]->visHeight;
        if(isCeiling)
            *bz = -(*bz);
    }
    if(bhz)
    {
        *bhz =  back->planes[isCeiling? PLN_FLOOR:PLN_CEILING]->visHeight;
        if(isCeiling)
            *bhz = -(*bhz);
    }
}

static uint radioEdgeHackType(linedef_t *line, sector_t *front,
                              sector_t *back, int backside,
                              boolean isCeiling, float fz, float bz)
{
    surface_t*          surface = &LINE_SIDE(line, backside)->
                             sections[isCeiling? SEG_TOP:SEG_BOTTOM];

    if(fz < bz && !surface->material &&
       !(surface->inFlags & SUIF_MATERIAL_FIX))
        return 3; // Consider it fully open.

    // Is the back sector closed?
    if(front->SP_floorvisheight >= back->SP_ceilvisheight)
    {
        if(IS_SKYSURFACE(&front->planes[isCeiling? PLN_FLOOR:PLN_CEILING]->surface))
        {
            if(IS_SKYSURFACE(&back->planes[isCeiling? PLN_FLOOR:PLN_CEILING]->surface))
                return 3; // Consider it fully open.
        }
        else
            return 1; // Consider it fully closed.
    }

    // Check for unmasked midtextures on twosided lines that completely
    // fill the gap between floor and ceiling (we don't want to give away
    // the location of any secret areas (false walls)).
    if(R_DoesMiddleMaterialFillGap(line, backside))
        return 1; // Consider it fully closed.

    return 0;
}

/**
 * Calculate the corner coordinates and add a new shadow polygon to the
 * rendering lists.
 */
static void radioAddShadowEdge(const linedef_t* line, byte side,
                               vec2_t inner[2], vec2_t outer[2], float z,
                               float darkness, float sideOpen[2],
                               float normal[3])
{
    static const uint floorIndices[][4] = {{0, 1, 2, 3}, {1, 2, 3, 0}};
    static const uint ceilIndices[][4]  = {{0, 3, 2, 1}, {1, 0, 3, 2}};

    uint wind; // Winding: 0 = left, 1 = right
    const uint* idx;
    rvertex_t rvertices[4];
    rcolor_t rcolors[4];
    rtexmapunit_t rTU[NUM_TEXMAP_UNITS];
    float shadowAlpha;
    vertex_t* vtx1, *vtx2;
    vec2_t v1pos, v2pos;

    if(darkness < 0)
        return;

    shadowAlpha = MIN_OF(darkness, 1.0f);

    vtx1 = LINE_VERTEX(line, side^0);
    V2_Set(v1pos, vtx1->pos[VX], vtx1->pos[VY]);

    vtx2 = LINE_VERTEX(line, side^1);
    V2_Set(v2pos, vtx2->pos[VX], vtx2->pos[VY]);

    // What vertex winding order?
    // (for best results, the cross edge should always be the shortest).
    wind = (V2_Distance(inner[1], v2pos) >
            V2_Distance(inner[0], v1pos)? 1 : 0);

    memset(rTU, 0, sizeof(rTU));
    rTU[TU_PRIMARY].blend = 1;

    idx = (normal[VZ] > 0 ? floorIndices[wind] : ceilIndices[wind]);

    // Left outer corner.
    rvertices[idx[0]].pos[VX] = v1pos[VX];
    rvertices[idx[0]].pos[VY] = v1pos[VY];
    rvertices[idx[0]].pos[VZ] = z;

    rcolors[idx[0]].rgba[CR] = rcolors[idx[0]].rgba[CG] =
        rcolors[idx[0]].rgba[CB] = (renderWireframe? 1 : 0);
    rcolors[idx[0]].rgba[CA] = shadowAlpha;
    if(sideOpen[0] < 1)
        rcolors[idx[0]].rgba[CA] *= 1 - sideOpen[0];

    // Right outer corner.
    rvertices[idx[1]].pos[VX] = v2pos[VX];
    rvertices[idx[1]].pos[VY] = v2pos[VY];
    rvertices[idx[1]].pos[VZ] = z;

    rcolors[idx[1]].rgba[CR] = rcolors[idx[1]].rgba[CG] =
        rcolors[idx[1]].rgba[CB] = (renderWireframe? 1 : 0);
    rcolors[idx[1]].rgba[CA] = shadowAlpha;
    if(sideOpen[1] < 1)
        rcolors[idx[1]].rgba[CA] *= 1 - sideOpen[1];

    // Right inner corner.
    rvertices[idx[2]].pos[VX] = inner[1][VX];
    rvertices[idx[2]].pos[VY] = inner[1][VY];
    rvertices[idx[2]].pos[VZ] = z;

    rcolors[idx[2]].rgba[CR] = rcolors[idx[2]].rgba[CG] =
        rcolors[idx[2]].rgba[CB] = (renderWireframe? 1 : 0);
    rcolors[idx[2]].rgba[CA] = 0;

    // Left inner corner.
    rvertices[idx[3]].pos[VX] = inner[0][VX];
    rvertices[idx[3]].pos[VY] = inner[0][VY];
    rvertices[idx[3]].pos[VZ] = z;

    rcolors[idx[3]].rgba[CR] = rcolors[idx[3]].rgba[CG] =
        rcolors[idx[3]].rgba[CB] = (renderWireframe? 1 : 0);
    rcolors[idx[3]].rgba[CA] = 0;

    if(rendFakeRadio != 2)
        RL_AddPoly(PT_FAN, (renderWireframe? RPT_NORMAL : RPT_SHADOW),
                   4, rvertices, NULL, NULL, NULL,
                   rcolors, 0, 0, NULL, rTU);
}

/**
 * Render the shadowpolygons linked to the subsector, if they haven't
 * already been rendered.
 *
 * Don't use the global radio state in here, the subsector can be part of
 * any sector, not the one chosen for wall rendering.
 */
static void radioSubsectorEdges(const subsector_t* subsector)
{
    static size_t doPlaneSize = 0;
    static byte* doPlane = NULL;

    uint i, pln, hack, side;
    float open, sideOpen[2], vec[3];
    float fz, bz, bhz, plnHeight;
    sector_t* front, *back;
    linedef_t* line;
    surface_t* suf;
    shadowlink_t* link;
    vec2_t inner[2], outer[2];
    boolean workToDo = false;
    float shadowSize, shadowDark;
    float sectorlight = subsector->sector->lightLevel;

    R_ApplyLightAdaptation(&sectorlight);

    if(sectorlight == 0)
        return; // No point drawing shadows in a PITCH black sector.

    // Determine the shadow properties.
    // \fixme Make cvars out of constants.
    shadowSize = 2 * (8 + 16 - sectorlight * 16);
    shadowDark = calcShadowDarkness(sectorlight) *.8f;

    vec[VX] = vx - subsector->midPoint[VX];
    vec[VY] = vz - subsector->midPoint[VY];

    // Do we need to enlarge the size of the doPlane array?
    if(subsector->sector->planeCount > doPlaneSize)
    {
        if(!doPlaneSize)
            doPlaneSize = 2;
        else
            doPlaneSize *= 2;

        doPlane = Z_Realloc(doPlane, doPlaneSize, PU_STATIC);
    }

    memset(doPlane, 0, doPlaneSize);

    // See if any of this face's planes will get shadows.
    for(pln = 0; pln < subsector->sector->planeCount; ++pln)
    {
        plane_t* plane = subsector->sector->planes[pln];

        if(R_IsGlowingPlane(plane))
            continue;

        vec[VZ] = vy - plane->visHeight;

        // Don't bother with planes facing away from the camera.
        if(M_DotProduct(vec, plane->PS_normal) < 0)
            continue;

        doPlane[pln] = true;
        workToDo = true;
    }

    if(!workToDo)
        return;

    // We need to check all the shadow lines linked to this face for
    // the purpose of fakeradio shadowing.
    for(link = subsector->shadows; link != NULL; link = link->next)
    {
        // Already rendered during the current frame? We only want to
        // render each shadow once per frame.
        if(link->lineDef->shadowVisFrame[link->side] == (ushort) frameCount)
            continue;

        // Now it will be rendered.
        link->lineDef->shadowVisFrame[link->side] = (ushort) frameCount;

        line = link->lineDef;
        side = link->side;

        for(pln = 0; pln < subsector->sector->planeCount; ++pln)
        {
            plane_t* plane;

            if(!doPlane[pln])
                continue;

            plane = LINE_SECTOR(line, side)->SP_plane(pln);

            // Determine the openness of the line. If this edge is open,
            // there won't be a shadow at all. Open neighbours cause some
            // changes in the polygon corner vertices (placement, colour).

            suf = &subsector->sector->planes[pln]->surface;
            plnHeight = plane->visHeight;
            vec[VZ] = vy - plnHeight;

            // Glowing surfaces or missing textures shouldn't have shadows.
            if((suf->inFlags & SUIF_NO_RADIO) || !suf->material)
                continue;

            if(LINE_BACKSIDE(line))
            {
                front = LINE_SECTOR(line, side);
                back  = LINE_SECTOR(line, side ^ 1);
                setRelativeHeights(front, back, pln, &fz, &bz, &bhz);

                hack =
                    radioEdgeHackType(line, front, back, side, pln, fz, bz);
                if(hack)
                    open = hack - 1;
                else
                    open = radioEdgeOpenness(fz, bz, bhz);
            }
            else
                open = 0;

            if(open >= 1)
                continue;

            // Find the neighbors of this edge and determine their 'openness'.
            sideOpen[0] = sideOpen[1] = 0;
            for(i = 0; i < 2; ++i)
            {
                lineowner_t* vo;
                linedef_t* neighbor;

                vo = line->L_vo(side^i)->link[i^1];
                neighbor = vo->lineDef;

                if(neighbor != line && !LINE_BACKSIDE(neighbor) &&
                   neighbor->buildData.windowEffect &&
                   LINE_FRONTSECTOR(neighbor) != subsector->sector)
                {   // A one-way window, open side.
                    sideOpen[i] = 1;
                }
                else if(!(neighbor == line || !LINE_BACKSIDE(neighbor)))
                {
                    sector_t* othersec;
                    byte otherSide;

                    otherSide = (LINE_VERTEX(line, i^side) == neighbor->L_v1? i : i^1);
                    othersec = LINE_SECTOR(neighbor, otherSide);

                    // Exclude 'special' neighbors which we pretend to be solid.
                    if(LINE_SELFREF(neighbor) ||
                       ((IS_SKYSURFACE(&othersec->SP_planesurface(pln)) ||
                         IS_SKYSURFACE(&othersec->SP_planesurface(PLN_CEILING))) &&
                        othersec->SP_floorvisheight >= othersec->SP_ceilvisheight))
                    {
                        sideOpen[i] = 1;
                    }
                    else if(R_DoesMiddleMaterialFillGap(neighbor, otherSide^1))
                    {
                        sideOpen[i] = 0;
                    }
                    else
                    {   // Its a normal neighbor.
                        if(LINE_SECTOR(neighbor, otherSide) !=
                           LINE_SECTOR(line, side) &&
                           !((pln == PLN_FLOOR   && othersec->SP_ceilvisheight <= plane->visHeight) ||
                             (pln == PLN_CEILING && othersec->SP_floorheight >= plane->visHeight)))
                        {
                            front = LINE_SECTOR(line, side);
                            back  = LINE_SECTOR(neighbor, otherSide);

                            setRelativeHeights(front, back, pln, &fz, &bz, &bhz);
                            sideOpen[i] = radioEdgeOpenness(fz, bz, bhz);
                        }
                    }
                }

                if(sideOpen[i] < 1)
                {
                    vertex_t* vtx = LINE_VERTEX(line, i^side);
                    vec2_t vpos;

                    vo = line->L_vo(i^side);
                    if(i)
                        vo = vo->LO_prev;

                    V2_Set(vpos, vtx->pos[VX], vtx->pos[VY]);
                    V2_Sum(inner[i], vpos, vo->shadowOffsets.inner);
                }
                else
                {
                    vertex_t* vtx = LINE_VERTEX(line, i^side);
                    vec2_t vpos;

                    V2_Set(vpos, vtx->pos[VX], vtx->pos[VY]);
                    V2_Sum(inner[i], vpos, vo->shadowOffsets.extended);
                }
            }

            radioAddShadowEdge(line, side, inner, outer, plnHeight,
                               shadowDark * (1 - open), sideOpen, suf->normal);
        }
    }
}

void Rend_RadioSubsectorEdges(subsector_t* subsector)
{
    if(!rendFakeRadio || mapFullBright)
        return;

    radioSubsectorEdges(subsector);
}

#if _DEBUG
static void drawPoint(float pos[3], float radius, const float color[4])
{
    const viewdata_t* viewData = R_ViewData(viewPlayer - ddPlayers);
    float viewPos[3], viewToCenter[3], finalPos[3], scale, leftOff[3],
        rightOff[3], radX, radY;
    int i;

    viewPos[VX] = vx;
    viewPos[VY] = vy;
    viewPos[VZ] = vz;

    // viewSideVec is to the left.
    for(i = 0; i < 3; ++i)
    {
        leftOff[i] = viewData->upVec[i] + viewData->sideVec[i];
        rightOff[i] = viewData->upVec[i] - viewData->sideVec[i];

        viewToCenter[i] = pos[i] - viewPos[i];
    }

    scale = M_DotProduct(viewToCenter, viewData->frontVec) /
                M_DotProduct(viewData->frontVec, viewData->frontVec);

    finalPos[VX] = pos[VX];
    finalPos[VY] = pos[VZ];
    finalPos[VZ] = pos[VY];

    // The final radius.
    radX = radius * 1;
    radY = radX / 1.2f;

    glColor4fv(color);

    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex3f(finalPos[VX] + radX * leftOff[VX],
                   finalPos[VY] + radY * leftOff[VY],
                   finalPos[VZ] + radX * leftOff[VZ]);
        glTexCoord2f(1, 0);
        glVertex3f(finalPos[VX] + radX * rightOff[VX],
                   finalPos[VY] + radY * rightOff[VY],
                   finalPos[VZ] + radX * rightOff[VZ]);
        glTexCoord2f(1, 1);
        glVertex3f(finalPos[VX] - radX * leftOff[VX],
                   finalPos[VY] - radY * leftOff[VY],
                   finalPos[VZ] - radX * leftOff[VZ]);
        glTexCoord2f(0, 1);
        glVertex3f(finalPos[VX] - radX * rightOff[VX],
                   finalPos[VY] - radY * rightOff[VY],
                   finalPos[VZ] - radX * rightOff[VZ]);
    glEnd();
}

/**
 * Render the shadow poly vertices, for debug.
 */
void Rend_DrawShadowOffsetVerts(map_t* map)
{
    static const float red[4] = { 1.f, .2f, .2f, 1.f};
    static const float yellow[4] = {.7f, .7f, .2f, 1.f};

    uint i;

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    GL_BindTexture(GL_PrepareLSTexture(LST_DYNAMIC), GL_LINEAR);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* line = map->lineDefs[i];
        uint k;

        for(k = 0; k < 2; ++k)
        {
            vertex_t* vtx = LINE_VERTEX(line, k);
            lineowner_t* vo = ((mvertex_t*) vtx->data)->lineOwners;
            uint j;

            for(j = 0; j < ((mvertex_t*) vtx->data)->numLineOwners; ++j)
            {
                vec3_t pos;

                pos[VX] = vtx->pos[VX] + vo->shadowOffsets.extended[VX];
                pos[VY] = vtx->pos[VY] + vo->shadowOffsets.extended[VY];
                pos[VZ] = LINE_FRONTSECTOR(vo->lineDef)->SP_floorvisheight;

                drawPoint(pos, 1.f, yellow);

                pos[VX] = vtx->pos[VX] + vo->shadowOffsets.inner[VX];
                pos[VY] = vtx->pos[VY] + vo->shadowOffsets.inner[VY];
                pos[VZ] = LINE_FRONTSECTOR(vo->lineDef)->SP_floorvisheight;

                drawPoint(pos, 1.f, red);

                vo = vo->LO_next;
            }
        }
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}
#endif