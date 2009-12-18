/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 *\author Copyright © 1998 Raphael Quinet <raphael.quinet@eed.ericsson.se>
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
 * bsp_node.c: BSP node builder. Recursive node creation and sorting.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"
#include "de_misc.h"

#include <math.h>

#include "bsp_node.h"
#include "bsp_superblock.h"
#include "bsp_edge.h"
#include "bsp_intersection.h"

// MACROS ------------------------------------------------------------------

// Smallest distance between two points before being considered equal.
#define DIST_EPSILON        (1.0 / 128.0)

// TYPES -------------------------------------------------------------------

typedef struct linedefowner_t {
    struct linedef_s* lineDef;
    struct linedefowner_t* next;

    /**
     * Half-edge on the front side of the LineDef relative to this vertex
     * (i.e., if vertex is linked to the LineDef as vertex1, this half-edge
     * is that on the LineDef's front side, else, that on the back side).
     */
    struct hedge_s* hEdge;
} linedefowner_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static vertex_t* rootVtx; // Used when sorting vertex line owners.

// CODE --------------------------------------------------------------------

/**
 * Free all the SuperBlocks on the quick-alloc list.
 */
static void destroyUnusedSuperBlocks(nodebuilder_t* nb)
{
    while(nb->_quickAllocSupers)
    {
        superblock_t* block = nb->_quickAllocSupers;
        nb->_quickAllocSupers = block->subs[0];
        block->subs[0] = NULL;
        BSP_DestroySuperBlock(block);
    }
}

/**
 * Free all memory allocated for the specified SuperBlock.
 */
static void moveSuperBlockToQuickAllocList(nodebuilder_t* nb, superblock_t* block)
{
    int num;

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        if(block->subs[num])
        {
            moveSuperBlockToQuickAllocList(nb, block->subs[num]);
            block->subs[num] = NULL;
        }
    }

    // Add block to quick-alloc list. Note that subs[0] is used for
    // linking the blocks together.
    block->subs[0] = nb->_quickAllocSupers;
    block->parent = NULL;
    nb->_quickAllocSupers = block;
}

/**
 * Acquire memory for a new SuperBlock.
 */
static superblock_t* createSuperBlock(nodebuilder_t* nb)
{
    superblock_t* block;

    if(nb->_quickAllocSupers == NULL)
        return BSP_CreateSuperBlock();

    block = nb->_quickAllocSupers;
    nb->_quickAllocSupers = block->subs[0];

    // Clear out any old rubbish.
    memset(block, 0, sizeof(*block));

    return block;
}

static void destroySuperBlock(nodebuilder_t* nb, superblock_t* block)
{
    moveSuperBlockToQuickAllocList(nb, block);
}

static void findMapLimits(map_t* src, int* bbox)
{
    uint i;

    M_ClearBox(bbox);

    for(i = 0; i < src->numLineDefs; ++i)
    {
        linedef_t* l = src->lineDefs[i];
        double x1 = l->buildData.v[0]->pos[VX];
        double y1 = l->buildData.v[0]->pos[VY];
        double x2 = l->buildData.v[1]->pos[VX];
        double y2 = l->buildData.v[1]->pos[VY];
        int lX = (int) floor(MIN_OF(x1, x2));
        int lY = (int) floor(MIN_OF(y1, y2));
        int hX = (int) ceil(MAX_OF(x1, x2));
        int hY = (int) ceil(MAX_OF(y1, y2));

        M_AddToBox(bbox, lX, lY);
        M_AddToBox(bbox, hX, hY);
    }
}

static void createSuperBlockmap(nodebuilder_t* nb)
{
    int mapBounds[4], bw, bh;
    superblock_t* block;

    nb->_quickAllocSupers = NULL;
    block = createSuperBlock(nb);
    findMapLimits(nb->_map, mapBounds);

    block->bbox[BOXLEFT]   = mapBounds[BOXLEFT]   - (mapBounds[BOXLEFT]   & 0x7);
    block->bbox[BOXBOTTOM] = mapBounds[BOXBOTTOM] - (mapBounds[BOXBOTTOM] & 0x7);
    bw = ((mapBounds[BOXRIGHT] - block->bbox[BOXLEFT])   / 128) + 1;
    bh = ((mapBounds[BOXTOP]   - block->bbox[BOXBOTTOM]) / 128) + 1;

    block->bbox[BOXRIGHT] = block->bbox[BOXLEFT]   + 128 * M_CeilPow2(bw);
    block->bbox[BOXTOP]   = block->bbox[BOXBOTTOM] + 128 * M_CeilPow2(bh);

    nb->_superBlockmap = block;
}

static void destroySuperBlockmap(nodebuilder_t* nb)
{
    if(nb->_superBlockmap)
        destroySuperBlock(nb, nb->_superBlockmap);
    nb->_superBlockmap = NULL;
    destroyUnusedSuperBlocks(nb);
}

/**
 * Update the precomputed members of the hEdge.
 */
void BSP_UpdateHEdgeInfo(const hedge_t* hEdge)
{
    hedge_info_t* info = (hedge_info_t*) hEdge->data;

    info->pDX = hEdge->twin->vertex->pos[VX] - hEdge->vertex->pos[VX];
    info->pDY = hEdge->twin->vertex->pos[VY] - hEdge->vertex->pos[VY];

    info->pLength = M_Length(info->pDX, info->pDY);
    info->pAngle  = M_SlopeToAngle(info->pDX, info->pDY);

    if(info->pLength <= 0)
        Con_Error("HEdge %p has zero p_length.", hEdge);

    info->pPerp =  hEdge->vertex->pos[VY] * info->pDX - hEdge->vertex->pos[VX] * info->pDY;
    info->pPara = -hEdge->vertex->pos[VX] * info->pDX - hEdge->vertex->pos[VY] * info->pDY;
}

static void attachHEdgeInfo(nodebuilder_t* nb, hedge_t* hEdge,
                            linedef_t* line, linedef_t* sourceLine,
                            sector_t* sec, boolean back)
{
    assert(hEdge);
    {
    hedge_info_t* info;

    nb->halfEdgeInfo = Z_Realloc(nb->halfEdgeInfo, ++nb->numHalfEdgeInfo * sizeof(hedge_info_t*), PU_STATIC);
    info = nb->halfEdgeInfo[nb->numHalfEdgeInfo-1] =
        Z_Calloc(sizeof(hedge_info_t), PU_STATIC, 0);

    info->lineDef = line;
    info->side = (back? 1 : 0);
    info->sector = sec;
    info->sourceLine = sourceLine;
    hEdge->data = info;
    }
}

hedge_t* NodeBuilder_CreateHEdge(nodebuilder_t* nb, linedef_t* line,
                                 linedef_t* sourceLine, vertex_t* start,
                                 sector_t* sec, boolean back)
{
    assert(nb);
    {
    hedge_t* hEdge = HalfEdgeDS_CreateHEdge(Map_HalfEdgeDS(nb->_map), start);

    attachHEdgeInfo(nb, hEdge, line, sourceLine, sec, back);
    return hEdge;
    }
}

/**
 * @note
 * If the half-edge has a twin, it is also split and is inserted into the
 * same list as the original (and after it), thus all half-edges (except the
 * one we are currently splitting) must exist on a singly-linked list
 * somewhere.
 *
 * @note
 * We must update the count values of any SuperBlock that contains the
 * half-edge (and/or backseg), so that future processing is not messed up by
 * incorrect counts.
 */
hedge_t* NodeBuilder_SplitHEdge(nodebuilder_t* nb, hedge_t* oldHEdge,
                                double x, double y)
{
    assert(nb);
    assert(oldHEdge);
    {
    hedge_t* newHEdge = HEdge_Split(Map_HalfEdgeDS(nb->_map), oldHEdge);

    newHEdge->vertex->pos[VX] = x;
    newHEdge->vertex->pos[VY] = y;

    attachHEdgeInfo(nb, newHEdge, ((hedge_info_t*) oldHEdge->data)->lineDef, ((hedge_info_t*) oldHEdge->data)->sourceLine, ((hedge_info_t*) oldHEdge->data)->sector, ((hedge_info_t*) oldHEdge->data)->side);
    attachHEdgeInfo(nb, newHEdge->twin, ((hedge_info_t*) oldHEdge->twin->data)->lineDef, ((hedge_info_t*) oldHEdge->twin->data)->sourceLine, ((hedge_info_t*) oldHEdge->twin->data)->sector, ((hedge_info_t*) oldHEdge->twin->data)->side);

    memcpy(newHEdge->data, oldHEdge->data, sizeof(hedge_info_t));
    ((hedge_info_t*) newHEdge->data)->block = NULL;
    memcpy(newHEdge->twin->data, oldHEdge->twin->data, sizeof(hedge_info_t));
    ((hedge_info_t*) newHEdge->twin->data)->block = NULL;

    // Update along-linedef relationships.
    if(((hedge_info_t*) oldHEdge->data)->lineDef)
    {
        linedef_t* lineDef = ((hedge_info_t*) oldHEdge->data)->lineDef;
        if(((hedge_info_t*) oldHEdge->data)->side == FRONT)
        {
            if(lineDef->hEdges[1] == oldHEdge)
                lineDef->hEdges[1] = newHEdge;
        }
        else
        {
            if(lineDef->hEdges[0] == oldHEdge->twin)
                lineDef->hEdges[0] = newHEdge->twin;
        }
    }

    BSP_UpdateHEdgeInfo(oldHEdge);
    BSP_UpdateHEdgeInfo(oldHEdge->twin);
    BSP_UpdateHEdgeInfo(newHEdge);
    BSP_UpdateHEdgeInfo(newHEdge->twin);

    /*if(!oldHEdge->twin->face && ((hedge_info_t*)oldHEdge->twin->data)->block)
    {
        SuperBlock_IncHEdgeCounts(((hedge_info_t*)oldHEdge->twin->data)->block, ((hedge_info_t*) newHEdge->twin->data)->lineDef != NULL);
        SuperBlock_PushHEdge(((hedge_info_t*)oldHEdge->twin->data)->block, newHEdge->twin);
        ((hedge_info_t*) newHEdge->twin->data)->block = ((hedge_info_t*)oldHEdge->twin->data)->block;
    }*/

    return newHEdge;
    }
}

static __inline int pointOnHEdgeSide(double x, double y,
                                     const hedge_t* part)
{
    hedge_info_t* data = (hedge_info_t*) part->data;

    return P_PointOnLineDefSide2(x, y, data->pDX, data->pDY, data->pPerp,
                                 data->pLength, DIST_EPSILON);
}

/**
 * @algorithm Cast a line horizontally or vertically and see what we hit.
 * @todo The blockmap has already been constructed by this point; so use it!
 */
static void testForWindowEffect(map_t* map, linedef_t* l)
{
    uint i;
    double mX, mY, dX, dY;
    boolean castHoriz;
    double backDist = DDMAXFLOAT;
    sector_t* backOpen = NULL;
    double frontDist = DDMAXFLOAT;
    sector_t* frontOpen = NULL;
    linedef_t* frontLine = NULL, *backLine = NULL;

    mX = (l->buildData.v[0]->pos[VX] + l->buildData.v[1]->pos[VX]) / 2.0;
    mY = (l->buildData.v[0]->pos[VY] + l->buildData.v[1]->pos[VY]) / 2.0;

    dX = l->buildData.v[1]->pos[VX] - l->buildData.v[0]->pos[VX];
    dY = l->buildData.v[1]->pos[VY] - l->buildData.v[0]->pos[VY];

    castHoriz = (fabs(dX) < fabs(dY)? true : false);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* n = map->lineDefs[i];
        double dist;
        boolean isFront;
        sidedef_t* hitSide;
        double dX2, dY2;

        if(n == l || (n->buildData.sideDefs[FRONT] && n->buildData.sideDefs[BACK] &&
            n->buildData.sideDefs[FRONT]->sector == n->buildData.sideDefs[BACK]->sector) /*|| n->buildData.overlap */)
            continue;

        dX2 = n->buildData.v[1]->pos[VX] - n->buildData.v[0]->pos[VX];
        dY2 = n->buildData.v[1]->pos[VY] - n->buildData.v[0]->pos[VY];

        if(castHoriz)
        {   // Horizontal.
            if(fabs(dY2) < DIST_EPSILON)
                continue;

            if((MAX_OF(n->buildData.v[0]->pos[VY], n->buildData.v[1]->pos[VY]) < mY - DIST_EPSILON) ||
               (MIN_OF(n->buildData.v[0]->pos[VY], n->buildData.v[1]->pos[VY]) > mY + DIST_EPSILON))
                continue;

            dist = (n->buildData.v[0]->pos[VX] + (mY - n->buildData.v[0]->pos[VY]) * dX2 / dY2) - mX;

            isFront = (((dY > 0) != (dist > 0)) ? true : false);

            dist = fabs(dist);
            if(dist < DIST_EPSILON)
                continue; // Too close (overlapping lines ?)

            hitSide = n->buildData.sideDefs[(dY > 0) ^ (dY2 > 0) ^ !isFront];
        }
        else
        {   // Vertical.
            if(fabs(dX2) < DIST_EPSILON)
                continue;

            if((MAX_OF(n->buildData.v[0]->pos[VX], n->buildData.v[1]->pos[VX]) < mX - DIST_EPSILON) ||
               (MIN_OF(n->buildData.v[0]->pos[VX], n->buildData.v[1]->pos[VX]) > mX + DIST_EPSILON))
                continue;

            dist = (n->buildData.v[0]->pos[VY] + (mX - n->buildData.v[0]->pos[VX]) * dY2 / dX2) - mY;

            isFront = (((dX > 0) == (dist > 0)) ? true : false);

            dist = fabs(dist);

            hitSide = n->buildData.sideDefs[(dX > 0) ^ (dX2 > 0) ^ !isFront];
        }

        if(dist < DIST_EPSILON) // Too close (overlapping lines ?)
            continue;

        if(isFront)
        {
            if(dist < frontDist)
            {
                frontDist = dist;
                if(hitSide)
                    frontOpen = hitSide->sector;
                else
                    frontOpen = NULL;

                frontLine = n;
            }
        }
        else
        {
            if(dist < backDist)
            {
                backDist = dist;
                if(hitSide)
                    backOpen = hitSide->sector;
                else
                    backOpen = NULL;

                backLine = n;
            }
        }
    }

/*#if _DEBUG
Con_Message("back line: %d  back dist: %1.1f  back_open: %s\n",
            (backLine? backLine->buildData.index : -1), backDist,
            (backOpen? "OPEN" : "CLOSED"));
Con_Message("front line: %d  front dist: %1.1f  front_open: %s\n",
            (frontLine? frontLine->buildData.index : -1), frontDist,
            (frontOpen? "OPEN" : "CLOSED"));
#endif*/

    if(backOpen && frontOpen && l->buildData.sideDefs[FRONT]->sector == backOpen)
    {
        Con_Message("LineDef #%d seems to be a One-Sided Window "
                    "(back faces sector #%d).\n", l->buildData.index - 1,
                    backOpen->buildData.index - 1);

        l->buildData.windowEffect = frontOpen;
    }
}

static void countVertexLineOwners(linedefowner_t* lineOwners, uint* oneSided, uint* twoSided)
{
    if(lineOwners)
    {
        linedefowner_t* owner;

        owner = lineOwners;
        do
        {
            if(!owner->lineDef->buildData.sideDefs[FRONT] ||
               !owner->lineDef->buildData.sideDefs[BACK])
                (*oneSided)++;
            else
                (*twoSided)++;
        } while((owner = owner->next) != NULL);
    }
}

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (linedefowner_t*) ptrs.
 */
static int C_DECL lineAngleSorter2(const void* a, const void* b)
{
    uint i;
    fixed_t dx, dy;
    binangle_t angles[2];
    linedefowner_t* own[2];
    linedef_t* line;

    own[0] = (linedefowner_t*) a;
    own[1] = (linedefowner_t*) b;
    for(i = 0; i < 2; ++i)
    {
        vertex_t* otherVtx;

        line = own[i]->lineDef;
        otherVtx = line->buildData.v[line->buildData.v[0] == rootVtx? 1:0];

        dx = otherVtx->pos[VX] - rootVtx->pos[VX];
        dy = otherVtx->pos[VY] - rootVtx->pos[VY];

        angles[i] = bamsAtan2(-100 *dx, 100 * dy);
    }

    return (angles[0] - angles[1]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return              Ptr to the newly merged list.
 */
static linedefowner_t* mergeLineOwners2(linedefowner_t* left, linedefowner_t* right,
                                        int (C_DECL *compare) (const void* a, const void* b))
{
    linedefowner_t tmp, *np;

    np = &tmp;
    tmp.next = np;
    while(left != NULL && right != NULL)
    {
        if(compare(left, right) <= 0)
        {
            np->next = left;
            np = left;

            left = left->next;
        }
        else
        {
            np->next = right;
            np = right;

            right = right->next;
        }
    }

    // At least one of these lists is now empty.
    if(left)
        np->next = left;
    if(right)
        np->next = right;

    // Is the list empty?
    if(tmp.next == &tmp)
        return NULL;

    return tmp.next;
}

static linedefowner_t* splitLineOwners2(linedefowner_t* list)
{
    linedefowner_t* lista, *listb, *listc;

    if(!list)
        return NULL;

    lista = listb = listc = list;
    do
    {
        listc = listb;
        listb = listb->next;
        lista = lista->next;
        if(lista != NULL)
            lista = lista->next;
    } while(lista);

    listc->next = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static linedefowner_t* sortLineOwners2(linedefowner_t* list,
                                     int (C_DECL *compare) (const void* a, const void* b))
{
    linedefowner_t* p;

    if(list && list->next)
    {
        p = splitLineOwners2(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners2(sortLineOwners2(list, compare),
                                sortLineOwners2(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner2(linedefowner_t** lineOwners, uint* numLineOwners,
                                linedef_t* lineDef, byte vertex,
                                linedefowner_t** storage)
{
    linedefowner_t* newOwner;

    // Has this line already been registered with this vertex?
    if((*numLineOwners) != 0)
    {
        linedefowner_t* owner = (*lineOwners);

        do
        {
            if(owner->lineDef == lineDef)
                return; // Yes, we can exit.

            owner = owner->next;
        } while(owner);
    }

    // Add a new owner.
    (*numLineOwners)++;

    newOwner = (*storage)++;
    newOwner->lineDef = lineDef;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->next = (*lineOwners);
    (*lineOwners) = newOwner;

    // Link the line to its respective owner node.
    lineDef->L_vo(vertex) = (lineowner_t*) newOwner;
}

/**
 * Initially create all half-edges, one for each side of a linedef.
 */
static void createInitialHEdges(nodebuilder_t* nb)
{
typedef struct {
    uint            numLineOwners;
    linedefowner_t* lineOwners; // Head of the lineowner list.
} ownerinfo_t;

    uint startTime = Sys_GetRealTime();

    linedefowner_t* lineOwners, *storage;
    ownerinfo_t* vertexInfo;
    uint i, numVertices;

    numVertices = HalfEdgeDS_NumVertices(Map_HalfEdgeDS(nb->_map));
    vertexInfo = M_Calloc(sizeof(*vertexInfo) * numVertices);

    // We know how many lineowners are needed: number of lineDefs * 2.
    lineOwners = storage = M_Calloc(sizeof(*lineOwners) * nb->_map->numLineDefs * 2);

    for(i = 0; i < nb->_map->numLineDefs; ++i)
    {
        linedef_t* lineDef = nb->_map->lineDefs[i];
        vertex_t* from = lineDef->buildData.v[0];
        vertex_t* to = lineDef->buildData.v[1];

        setVertexLineOwner2(&vertexInfo[((mvertex_t*) from->data)->index - 1].lineOwners,
                            &vertexInfo[((mvertex_t*) from->data)->index - 1].numLineOwners,
                            lineDef, 0, &storage);
        setVertexLineOwner2(&vertexInfo[((mvertex_t*) to->data)->index - 1].lineOwners,
                            &vertexInfo[((mvertex_t*) to->data)->index - 1].numLineOwners,
                            lineDef, 1, &storage);
    }

    /**
     * Detect "one-sided window" mapping tricks.
     *
     * @todo Does not belong in the engine; move this logic to dpWADMapConverter.
     *
     * @note Algorithm:
     * Scan the linedef list looking for possible candidates, checking for an
     * odd number of one-sided linedefs connected to a single vertex.
     * This idea courtesy of Graham Jackson.
     */
    {
    uint i, oneSiders, twoSiders;

    for(i = 0; i < nb->_map->numLineDefs; ++i)
    {
        linedef_t* lineDef = nb->_map->lineDefs[i];
        vertex_t* from = lineDef->buildData.v[0];
        vertex_t* to = lineDef->buildData.v[1];

        if((lineDef->buildData.sideDefs[FRONT] && lineDef->buildData.sideDefs[BACK]) ||
           !lineDef->buildData.sideDefs[FRONT] /* ||
           lineDef->buildData.overlap*/)
            continue;

        oneSiders = twoSiders = 0;
        countVertexLineOwners(vertexInfo[((mvertex_t*) from->data)->index - 1].lineOwners,
                              &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*#if _DEBUG
Con_Message("FUNNY LINE %d : start vertex %d has odd number of one-siders\n",
        i, lineDef->buildData.v[0]->index);
#endif*/
            testForWindowEffect(nb->_map, lineDef);
            continue;
        }

        oneSiders = twoSiders = 0;
        countVertexLineOwners(vertexInfo[((mvertex_t*) to->data)->index - 1].lineOwners,
                              &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*#if _DEBUG
Con_Message("FUNNY LINE %d : end vertex %d has odd number of one-siders\n",
        i, lineDef->buildData.v[1]->index));
#endif*/
            testForWindowEffect(nb->_map, lineDef);
        }
    }
    }

    for(i = 0; i < nb->_map->numLineDefs; ++i)
    {
        linedef_t* lineDef = nb->_map->lineDefs[i];
        vertex_t* from = lineDef->buildData.v[0];
        vertex_t* to = lineDef->buildData.v[1];
        hedge_t* back, *front;

        front = NodeBuilder_CreateHEdge(nb, lineDef, lineDef, from,
                                        lineDef->buildData.sideDefs[FRONT]->sector, false);

        // Handle the 'One-Sided Window' trick.
        if(!lineDef->buildData.sideDefs[BACK] && lineDef->buildData.windowEffect)
        {
            back = NodeBuilder_CreateHEdge(nb, ((hedge_info_t*) front->data)->lineDef,
                                           lineDef, to,
                                           lineDef->buildData.windowEffect, true);
        }
        else
        {
            back = NodeBuilder_CreateHEdge(nb, lineDef, lineDef, to,
                                           lineDef->buildData.sideDefs[BACK]? lineDef->buildData.sideDefs[BACK]->sector : NULL, true);
        }

        back->twin = front;
        front->twin = back;

        BSP_UpdateHEdgeInfo(front);
        BSP_UpdateHEdgeInfo(back);

        lineDef->hEdges[0] = lineDef->hEdges[1] = front;
        ((linedefowner_t*) lineDef->vo[0])->hEdge = front;
        ((linedefowner_t*) lineDef->vo[1])->hEdge = back;
    }

    // Sort vertex owners, clockwise by angle (i.e., ascending).
    for(i = 0; i < numVertices; ++i)
    {
        ownerinfo_t* info = &vertexInfo[i];

        if(info->numLineOwners == 0)
            continue;

        rootVtx = nb->_map->_halfEdgeDS->vertices[i];
        info->lineOwners = sortLineOwners2(info->lineOwners, lineAngleSorter2);
    }

    // Link half-edges around vertex.
    for(i = 0; i < numVertices; ++i)
    {
        ownerinfo_t* info = &vertexInfo[i];
        linedefowner_t* owner, *prev;

        if(info->numLineOwners == 0)
            continue;

        prev = info->lineOwners;
        while(prev->next)
            prev = prev->next;

        owner = info->lineOwners;
        do
        {
            hedge_t* hEdge = owner->hEdge;

            hEdge->prev = owner->next ? owner->next->hEdge->twin : info->lineOwners->hEdge->twin;
            hEdge->prev->next = hEdge;

            hEdge->twin->next = prev->hEdge;
            hEdge->twin->next->prev = hEdge->twin;

            prev = owner;
        } while((owner = owner->next) != NULL);
    }

    // Release temporary storage.
    M_Free(lineOwners);
    M_Free(vertexInfo);

    // How much time did we spend?
    VERBOSE(Con_Message("createInitialHEdges: Done in %.2f seconds.\n",
                        (Sys_GetRealTime() - startTime) / 1000.0f));
}

static int addHEdgeToSuperBlock(hedge_t* hEdge, void* context)
{
    superblock_t* block = (superblock_t*) context;

    if(!(((hedge_info_t*) hEdge->data)->lineDef))
        return true; // Continue iteration.

    if(((hedge_info_t*) hEdge->data)->side == BACK &&
       !((hedge_info_t*) hEdge->data)->lineDef->buildData.sideDefs[BACK])
        return true; // Continue iteration.

    if(((hedge_info_t*) hEdge->data)->side == BACK &&
       ((hedge_info_t*) hEdge->data)->lineDef->buildData.windowEffect)
        return true; // Continue iteration.

    BSP_AddHEdgeToSuperBlock(block, hEdge);
    return true; // Continue iteration.
}

static void createInitialHEdgesAndAddtoSuperBlockmap(nodebuilder_t* nb)
{
    createInitialHEdges(nb);
    HalfEdgeDS_IterateHEdges(Map_HalfEdgeDS(nb->_map), addHEdgeToSuperBlock, nb->_superBlockmap);
}

/**
 * Add the given half-edge to the specified list.
 */
void BSP_AddHEdgeToSuperBlock(superblock_t* block, hedge_t* hEdge)
{
    assert(block);
    assert(hEdge);
    {
#define SUPER_IS_LEAF(s)  \
    ((s)->bbox[BOXRIGHT] - (s)->bbox[BOXLEFT] <= 256 && \
     (s)->bbox[BOXTOP] - (s)->bbox[BOXBOTTOM] <= 256)

    for(;;)
    {
        int p1, p2, child, midPoint[2];
        superblock_t* sub;
        hedge_info_t* info = (hedge_info_t*) hEdge->data;

        midPoint[VX] = (block->bbox[BOXLEFT]   + block->bbox[BOXRIGHT]) / 2;
        midPoint[VY] = (block->bbox[BOXBOTTOM] + block->bbox[BOXTOP])   / 2;

        // Update half-edge counts.
        if(info->lineDef)
            block->realNum++;
        else
            block->miniNum++;

        if(SUPER_IS_LEAF(block))
        {   // Block is a leaf -- no subdivision possible.
            SuperBlock_PushHEdge(block, hEdge);
            ((hedge_info_t*) hEdge->data)->block = block;
            return;
        }

        if(block->bbox[BOXRIGHT] - block->bbox[BOXLEFT] >=
           block->bbox[BOXTOP]   - block->bbox[BOXBOTTOM])
        {   // Block is wider than it is high, or square.
            p1 = hEdge->vertex->pos[VX] >= midPoint[VX];
            p2 = hEdge->twin->vertex->pos[VX] >= midPoint[VX];
        }
        else
        {   // Block is higher than it is wide.
            p1 = hEdge->vertex->pos[VY] >= midPoint[VY];
            p2 = hEdge->twin->vertex->pos[VY] >= midPoint[VY];
        }

        if(p1 && p2)
            child = 1;
        else if(!p1 && !p2)
            child = 0;
        else
        {   // Line crosses midpoint -- link it in and return.
            SuperBlock_PushHEdge(block, hEdge);
            ((hedge_info_t*) hEdge->data)->block = block;
            return;
        }

        // The seg lies in one half of this block. Create the block if it
        // doesn't already exist, and loop back to add the seg.
        if(!block->subs[child])
        {
            block->subs[child] = sub = BSP_CreateSuperBlock();
            sub->parent = block;

            if(block->bbox[BOXRIGHT] - block->bbox[BOXLEFT] >=
               block->bbox[BOXTOP]   - block->bbox[BOXBOTTOM])
            {
                sub->bbox[BOXLEFT] =
                    (child? midPoint[VX] : block->bbox[BOXLEFT]);
                sub->bbox[BOXBOTTOM] = block->bbox[BOXBOTTOM];

                sub->bbox[BOXRIGHT] =
                    (child? block->bbox[BOXRIGHT] : midPoint[VX]);
                sub->bbox[BOXTOP] = block->bbox[BOXTOP];
            }
            else
            {
                sub->bbox[BOXLEFT] = block->bbox[BOXLEFT];
                sub->bbox[BOXBOTTOM] =
                    (child? midPoint[VY] : block->bbox[BOXBOTTOM]);

                sub->bbox[BOXRIGHT] = block->bbox[BOXRIGHT];
                sub->bbox[BOXTOP] =
                    (child? block->bbox[BOXTOP] : midPoint[VY]);
            }
        }

        block = block->subs[child];
    }

#undef SUPER_IS_LEAF
    }
}

static void sanityCheckClosed(const face_t* face)
{
    int total = 0, gaps = 0;
    const hedge_t* hEdge;

    hEdge = face->hEdge;
    do
    {
        const hedge_t* a = hEdge;
        const hedge_t* b = hEdge->next;

        if(a->twin->vertex->pos[VX] != b->vertex->pos[VX] ||
           a->twin->vertex->pos[VY] != b->vertex->pos[VY])
            gaps++;

        total++;
    } while((hEdge = hEdge->next) != face->hEdge);

    if(gaps > 0)
    {
        VERBOSE(
        Con_Message("HEdge list for face #%p is not closed "
                    "(%d gaps, %d half-edges)\n", face, gaps, total))

/*#if _DEBUG
hEdge = face->hEdge;
do
{
    Con_Message("  half-edge %p  (%1.1f,%1.1f) --> (%1.1f,%1.1f)\n", hEdge,
                (float) hEdge->vertex->pos[VX], (float) hEdge->vertex->pos[VY],
                (float) hEdge->twin->vertex->pos[VX], (float) hEdge->twin->vertex->pos[VY]);
} while((hEdge = hEdge->next) != face->hEdge);
#endif*/
    }
}

static void sanityCheckSameSector(const face_t* face)
{
    const hedge_t* cur = NULL;
    const hedge_info_t* data;

    {
    const hedge_t* n;

    // Find a suitable half-edge for comparison.
    n = face->hEdge;
    do
    {
        if(!((hedge_info_t*) n->data)->sector)
            continue;

        cur = n;
        break;
    } while((n = n->next) != face->hEdge);

    if(!cur)
        return;

    data = (hedge_info_t*) cur->data;
    cur = cur->next;
    }

    do
    {
        const hedge_info_t* curData = (hedge_info_t*) cur->data;

        if(!curData->sector)
            continue;

        if(curData->sector == data->sector)
            continue;

        // Prevent excessive number of warnings.
        if(data->sector->buildData.warnedFacing ==
           curData->sector->buildData.index)
            continue;

        data->sector->buildData.warnedFacing =
            curData->sector->buildData.index;

        if(verbose >= 1)
        {
            if(curData->lineDef)
                Con_Message("Sector #%d has sidedef facing #%d (line #%d).\n",
                            data->sector->buildData.index,
                            curData->sector->buildData.index,
                            curData->lineDef->buildData.index);
            else
                Con_Message("Sector #%d has sidedef facing #%d.\n",
                            data->sector->buildData.index,
                            curData->sector->buildData.index);
        }
    } while((cur = cur->next) != face->hEdge);
}

static boolean sanityCheckHasRealHEdge(const face_t* face)
{
    const hedge_t* n;

    n = face->hEdge;
    do
    {
        if(((const hedge_info_t*) n->data)->lineDef)
            return true;
    } while((n = n->next) != face->hEdge);

    return false;
}

static boolean C_DECL finishLeaf(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {
        face_t* face = (face_t*) BinaryTree_GetData(tree);

        Face_SwitchToHEdgeLinks(face);

        sanityCheckClosed(face);
        sanityCheckSameSector(face);
        if(!sanityCheckHasRealHEdge(face))
        {
            Con_Error("BSP leaf #%p has no linedef-linked half-edge.", face);
        }
    }

    return true; // Continue traversal.
}

static void takeHEdgesFromSuperBlock(nodebuilder_t* nb, face_t* face,
                                     superblock_t* block)
{
    uint num;
    hedge_t* hEdge;

    while((hEdge = SuperBlock_PopHEdge(block)))
    {
        ((hedge_info_t*) hEdge->data)->block = NULL;
        // Link it into head of the leaf's list.
        Face_LinkHEdge(face, hEdge);
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        superblock_t* a = block->subs[num];

        if(a)
        {
            takeHEdgesFromSuperBlock(nb, face, a);

            if(a->realNum + a->miniNum > 0)
                Con_Error("createSubSectorWorker: child %d not empty!", num);

            destroySuperBlock(nb, a);
            block->subs[num] = NULL;
        }
    }

    block->realNum = block->miniNum = 0;
}

/**
 * Create a new leaf from a list of half-edges.
 */
static face_t* createBSPLeaf(nodebuilder_t* nb, face_t* face,
                             superblock_t* hEdgeList)
{
    takeHEdgesFromSuperBlock(nb, face, hEdgeList);
    return face;
}

static void makeIntersection(cutlist_t* cutList, vertex_t* vert,
                             double x, double y, double dX, double dY,
                             const hedge_t* partHEdge)
{
    if(!CutList_Find(cutList, vert))
        CutList_Intersect(cutList, vert,
            M_ParallelDist(dX, dY, ((hedge_info_t*) partHEdge->data)->pPara, ((hedge_info_t*) partHEdge->data)->pLength,
                           vert->pos[VX], vert->pos[VY]));
}

/**
 * Calculate the intersection location between the current half-edge and the
 * partition. Takes advantage of some common situations like horizontal and
 * vertical lines to choose a 'nicer' intersection point.
 */
static __inline void calcIntersection(const hedge_t* cur,
                                      double x, double y, double dX, double dY,
                                      double perpC, double perpD,
                                      double* iX, double* iY)
{
    hedge_info_t* data = (hedge_info_t*) cur->data;
    double ds;

    // Horizontal partition against vertical half-edge.
    if(dY == 0 && data->pDX == 0)
    {
        *iX = cur->vertex->pos[VX];
        *iY = y;
        return;
    }

    // Vertical partition against horizontal half-edge.
    if(dX == 0 && data->pDY == 0)
    {
        *iX = x;
        *iY = cur->vertex->pos[VY];
        return;
    }

    // 0 = start, 1 = end.
    ds = perpC / (perpC - perpD);

    if(data->pDX == 0)
        *iX = cur->vertex->pos[VX];
    else
        *iX = cur->vertex->pos[VX] + (data->pDX * ds);

    if(data->pDY == 0)
        *iY = cur->vertex->pos[VY];
    else
        *iY = cur->vertex->pos[VY] + (data->pDY * ds);
}

/**
 * Partition the given edge and perform any further necessary action (moving
 * it into either the left list, right list, or splitting it).
 *
 * Take the given half-edge 'cur', compare it with the partition line, and
 * determine it's fate: moving it into either the left or right lists
 * (perhaps both, when splitting it in two). Handles the twin as well.
 * Updates the intersection list if the half-edge lies on or crosses the
 * partition line.
 *
 * \note
 * I have rewritten this routine based on evalPartition() (which I've also
 * reworked, heavily). I think it is important that both these routines
 * follow the exact same logic when determining which half-edges should go
 * left, right or be split. - AJA
 */
static void divideOneHEdge(nodebuilder_t* nb, hedge_t* curHEdge, double x,
                           double y, double dX, double dY,
                           const hedge_t* partHEdge, superblock_t* bRight,
                           superblock_t* bLeft)
{
    hedge_info_t* data = ((hedge_info_t*) curHEdge->data);
    hedge_t* newHEdge;
    double a, b;

    // Get state of lines' relation to each other.
    if(partHEdge && data->sourceLine ==
       ((hedge_info_t*) partHEdge->data)->sourceLine)
    {
        a = b = 0;
    }
    else
    {
        hedge_info_t* part = (hedge_info_t*) partHEdge->data;

        a = M_PerpDist(dX, dY, part->pPerp, part->pLength,
                       curHEdge->vertex->pos[VX], curHEdge->vertex->pos[VY]);
        b = M_PerpDist(dX, dY, part->pPerp, part->pLength,
                       curHEdge->twin->vertex->pos[VX], curHEdge->twin->vertex->pos[VY]);
    }

    // Check for being on the same line.
    if(fabs(a) <= DIST_EPSILON && fabs(b) <= DIST_EPSILON)
    {
        makeIntersection(nb->_cutList, curHEdge->vertex, x, y, dX, dY, partHEdge);
        makeIntersection(nb->_cutList, curHEdge->twin->vertex, x, y, dX, dY, partHEdge);

        // This seg runs along the same line as the partition. Check
        // whether it goes in the same direction or the opposite.
        if(data->pDX * dX + data->pDY * dY < 0)
        {
            BSP_AddHEdgeToSuperBlock(bLeft, curHEdge);
        }
        else
        {
            BSP_AddHEdgeToSuperBlock(bRight, curHEdge);
        }

        return;
    }

    // Check for right side.
    if(a > -DIST_EPSILON && b > -DIST_EPSILON)
    {
        if(a < DIST_EPSILON)
            makeIntersection(nb->_cutList, curHEdge->vertex, x, y, dX, dY, partHEdge);
        else if(b < DIST_EPSILON)
            makeIntersection(nb->_cutList, curHEdge->twin->vertex, x, y, dX, dY, partHEdge);

        BSP_AddHEdgeToSuperBlock(bRight, curHEdge);
        return;
    }

    // Check for left side.
    if(a < DIST_EPSILON && b < DIST_EPSILON)
    {
        if(a > -DIST_EPSILON)
            makeIntersection(nb->_cutList, curHEdge->vertex, x, y, dX, dY, partHEdge);
        else if(b > -DIST_EPSILON)
            makeIntersection(nb->_cutList, curHEdge->twin->vertex, x, y, dX, dY, partHEdge);

        BSP_AddHEdgeToSuperBlock(bLeft, curHEdge);
        return;
    }

    // When we reach here, we have a and b non-zero and opposite sign,
    // hence this edge will be split by the partition line.
    {
    double iX, iY;
    calcIntersection(curHEdge, x, y, dX, dY, a, b, &iX, &iY);
    newHEdge = NodeBuilder_SplitHEdge(nb, curHEdge, iX, iY);
    makeIntersection(nb->_cutList, newHEdge->vertex, x, y, dX, dY, partHEdge);
    }

    if(a < 0)
    {
        BSP_AddHEdgeToSuperBlock(bLeft,  curHEdge);
        BSP_AddHEdgeToSuperBlock(bRight, newHEdge);
    }
    else
    {
        BSP_AddHEdgeToSuperBlock(bRight, curHEdge);
        BSP_AddHEdgeToSuperBlock(bLeft,  newHEdge);
    }

    if(!curHEdge->twin->face && ((hedge_info_t*)curHEdge->twin->data)->block)
        BSP_AddHEdgeToSuperBlock(((hedge_info_t*)curHEdge->twin->data)->block, newHEdge->twin);
}

static void divideHEdges(nodebuilder_t* nb, superblock_t* hEdgeList,
                         double x, double y, double dX, double dY, const hedge_t* partHEdge,
                         superblock_t* rights, superblock_t* lefts)
{
    hedge_t* hEdge;
    uint num;

    while((hEdge = SuperBlock_PopHEdge(hEdgeList)))
    {
        ((hedge_info_t*) hEdge->data)->block = NULL;
        divideOneHEdge(nb, hEdge, x, y, dX, dY, partHEdge, rights, lefts);
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        superblock_t* a = hEdgeList->subs[num];

        if(a)
        {
            divideHEdges(nb, a, x, y, dX, dY, partHEdge, rights, lefts);

            if(a->realNum + a->miniNum > 0)
                Con_Error("BSP_SeparateHEdges: child %d not empty!", num);

            destroySuperBlock(nb, a);
            hEdgeList->subs[num] = NULL;
        }
    }

    hEdgeList->realNum = hEdgeList->miniNum = 0;
}

/**
 * Analyze the intersection list, and add any needed minihedges to the given
 * half-edge lists (one minihedge on each side).
 */
static void addMiniHEdges(nodebuilder_t* nb, double x, double y, double dX,
                          double dY, const hedge_t* partHEdge,
                          superblock_t* bRight, superblock_t* bLeft)
{
    BSP_MergeOverlappingIntersections(nb->_cutList);
    NodeBuilder_ConnectGaps(nb, x, y, dX, dY, partHEdge, bRight, bLeft);
}

/**
 * Remove all the half-edges from the list, partitioning them into the left
 * or right lists based on the given partition line. Adds any intersections
 * onto the intersection list as it goes.
 */
static void partitionHEdges(nodebuilder_t* nb, superblock_t* hEdgeList,
                            double x, double y, double dX, double dY,
                            const hedge_t* partHEdge,
                            superblock_t** right, superblock_t** left)
{
    superblock_t* bRight = createSuperBlock(nb);
    superblock_t* bLeft = createSuperBlock(nb);

    M_CopyBox(bRight->bbox, hEdgeList->bbox);
    M_CopyBox(bLeft->bbox, hEdgeList->bbox);

    divideHEdges(nb, hEdgeList, x, y, dX, dY, partHEdge, bRight, bLeft);

    // Sanity checks...
    if(bRight->realNum + bRight->miniNum == 0)
        Con_Error("BuildNodes: Separated halfedge-list has no right side.");

    if(bLeft->realNum + bLeft->miniNum == 0)
        Con_Error("BuildNodes: Separated halfedge-list has no left side.");

    addMiniHEdges(nb, x, y, dX, dY, partHEdge, bRight, bLeft);
    CutList_Reset(nb->_cutList);

    *right = bRight;
    *left = bLeft;
}

/**
 * Takes the half-edge list and determines if it is convex, possibly
 * converting it into a subsector. Otherwise, the list is divided into two
 * halves and recursion will continue on the new sub list.
 *
 * @param hEdgeList     Ptr to the list of half edges at the current node.
 * @param cutList       Ptr to the cutlist to use for storing new segment
 *                      intersections (cuts).
 * @return              Ptr to the newly created subtree ELSE @c NULL.
 */
static binarytree_t* buildNodes(nodebuilder_t* nb, superblock_t* hEdgeList)
{
    superblock_t* rightHEdges, *leftHEdges;
    float rightAABB[4], leftAABB[4];
    binarytree_t* tree;
    hedge_t* partHEdge;
    double x, y, dX, dY;

    // Pick half-edge with which to derive the next partition.
    if(!(partHEdge = SuperBlock_PickPartition(hEdgeList, nb->bspFactor)))
    {   // No partition required, already convex.
        return BinaryTree_Create(createBSPLeaf(nb, HalfEdgeDS_CreateFace(Map_HalfEdgeDS(nb->_map)), hEdgeList));
    }

    x  = partHEdge->vertex->pos[VX];
    y  = partHEdge->vertex->pos[VY];
    dX = partHEdge->twin->vertex->pos[VX] - partHEdge->vertex->pos[VX];
    dY = partHEdge->twin->vertex->pos[VY] - partHEdge->vertex->pos[VY];

/*#if _DEBUG
Con_Message("BuildNodes: Partition xy{%1.0f, %1.0f} delta{%1.0f, %1.0f}.\n",
            x, y, dX, dY);
#endif*/

    partitionHEdges(nb, hEdgeList, x, y, dX, dY, partHEdge, &rightHEdges, &leftHEdges);
    BSP_FindAABBForHEdges(rightHEdges, rightAABB);
    BSP_FindAABBForHEdges(leftHEdges, leftAABB);

    tree = BinaryTree_Create(Map_CreateNode(nb->_map, x, y, dX, dY, rightAABB, leftAABB));

    // Recurse on right half-edge list.
    BinaryTree_SetChild(tree, RIGHT, buildNodes(nb, rightHEdges));
    destroySuperBlock(nb, rightHEdges);

    // Recurse on left half-edge list.
    BinaryTree_SetChild(tree, LEFT, buildNodes(nb, leftHEdges));
    destroySuperBlock(nb, leftHEdges);

    return tree;
}

nodebuilder_t* P_CreateNodeBuilder(map_t* map, int bspFactor)
{
    nodebuilder_t* nb;
    assert(map);
    nb = Z_Malloc(sizeof(*nb), PU_STATIC, 0);
    nb->bspFactor = bspFactor;
    nb->rootNode = NULL;
    nb->_map = map;
    nb->_cutList = BSP_CutListCreate();
    createSuperBlockmap(nb);
    nb->numHalfEdgeInfo = 0;
    nb->halfEdgeInfo = NULL;
    return nb;
}

void P_DestroyNodeBuilder(nodebuilder_t* nb)
{
    assert(nb);
    if(nb->halfEdgeInfo)
    {
        size_t i;
        for(i = 0; i < nb->numHalfEdgeInfo; ++i)
            Z_Free(nb->halfEdgeInfo[i]);
        Z_Free(nb->halfEdgeInfo);
    }
    destroySuperBlockmap(nb);
    if(nb->_cutList)
        BSP_CutListDestroy(nb->_cutList);
    Z_Free(nb);
}

static boolean C_DECL clockwiseLeaf(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
        Face_ClockwiseOrder((face_t*) BinaryTree_GetData(tree));
    return true; // Continue iteration.
}

void NodeBuilder_Build(nodebuilder_t* nb)
{
    assert(nb);
    {
    createInitialHEdgesAndAddtoSuperBlockmap(nb);

    nb->rootNode = buildNodes(nb, nb->_superBlockmap);
    if(nb->rootNode)
        BinaryTree_PostOrder(nb->rootNode, finishLeaf, NULL);

    /**
     * Traverse the BSP tree and put all the half-edges in each
     * subsector into clockwise order.
     *
     * @note This cannot be done during BuildNodes() since splitting a
     * half-edge with a twin may insert another half-edge into that
     * twin's list, usually in the wrong place order-wise.
     *
     * danij 25/10/2009: Actually, this CAN (and should) be done during
     * buildNodes, it just requires updating any present faces.
     */
    if(nb->rootNode)
    {
        BinaryTree_PostOrder(nb->rootNode, clockwiseLeaf, NULL);
    }
    }
}
