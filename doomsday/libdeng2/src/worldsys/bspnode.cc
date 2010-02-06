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

#include "de/Vector"
#include "de/Log"
#include "de/BSPNode"
#include "de/BSPSuperBlock"
#include "de/BSPEdge"
#include "de/BSPIntersection"
#include "de/Map"

using namespace de;

namespace de
{
    // Smallest distance between two points before being considered equal.
    #define DIST_EPSILON        (1.0 / 128.0)

    typedef struct linedefowner_t {
        LineDef* lineDef;
        struct linedefowner_t* next;

        /**
         * HalfEdge on the front side of the LineDef relative to this vertex
         * (i.e., if vertex is linked to the LineDef as vertex1, this half-edge
         * is that on the LineDef's front side, else, that on the back side).
         */
        HalfEdge* hEdge;
    } linedefowner_t;

    /// Used when sorting vertex line owners.
    static Vertex* rootVtx;
}

NodeBuilder::NodeBuilder(Map& map, dint splitFactor)
  : _splitFactor(splitFactor),
    rootNode(0),
    _map(map),
    numHalfEdgeInfo(0),
    halfEdgeInfo(0)
{
    _cutList = BSP_CutListCreate();
    createSuperBlockmap();
}

NodeBuilder::~NodeBuilder()
{
    if(halfEdgeInfo)
    {
        for(dsize i = 0; i < numHalfEdgeInfo; ++i)
            std::free(halfEdgeInfo[i]);
        std::free(halfEdgeInfo);
    }
    destroySuperBlockmap();
    if(_cutList)
        BSP_CutListDestroy(_cutList);
}

void NodeBuilder::destroyUnusedSuperBlocks()
{
    while(_quickAllocSupers)
    {
        superblock_t* block = _quickAllocSupers;
        _quickAllocSupers = block->subs[0];
        block->subs[0] = NULL;
        BSP_DestroySuperBlock(block);
    }
}

void NodeBuilder::moveSuperBlockToQuickAllocList(superblock_t* block)
{
    dint num;

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        if(block->subs[num])
        {
            moveSuperBlockToQuickAllocList(block->subs[num]);
            block->subs[num] = NULL;
        }
    }

    // Add block to quick-alloc list. Note that subs[0] is used for
    // linking the blocks together.
    block->subs[0] = _quickAllocSupers;
    block->parent = NULL;
    _quickAllocSupers = block;
}

superblock_t* NodeBuilder::createSuperBlock()
{
    if(_quickAllocSupers == NULL)
        return BSP_CreateSuperBlock();

    superblock_t* block = _quickAllocSupers;
    _quickAllocSupers = block->subs[0];
    // Clear out any old rubbish.
    memset(block, 0, sizeof(*block));
    return block;
}

void NodeBuilder::destroySuperBlock(superblock_t* block)
{
    moveSuperBlockToQuickAllocList(block);
}

static void findMapLimits(Map& src, dint* bbox)
{
    M_ClearBox(bbox);

    for(duint i = 0; i < src.numLineDefs(); ++i)
    {
        LineDef* lineDef = src.lineDefs[i];
        const Vertex& from = *lineDef->buildData.v[0];
        const Vertex& to   = *lineDef->buildData.v[1];

        Vector2d topLeft = from.pos.min(to.pos);
        M_AddToBox(bbox, Vector2i(dint(floor(topLeft.x)), dint(floor(topLeft.y))));

        Vector2d bottomRight = from.pos.max(to.pos);
        M_AddToBox(bbox, Vector2i(dint(ceil(bottomRight.x)), dint(ceil(bottomRight.y))));
    }
}

void NodeBuilder::createSuperBlockmap()
{   
    _quickAllocSupers = NULL;

    superblock_t* block = createSuperBlock();
    dint mapBounds[4];
    findMapLimits(_map, mapBounds);

    block->bbox[superblock_t::BOXLEFT]   = mapBounds[superblock_t::BOXLEFT]   - (mapBounds[superblock_t::BOXLEFT]   & 0x7);
    block->bbox[superblock_t::BOXBOTTOM] = mapBounds[superblock_t::BOXBOTTOM] - (mapBounds[superblock_t::BOXBOTTOM] & 0x7);

    dint bw = ((mapBounds[superblock_t::BOXRIGHT] - block->bbox[superblock_t::BOXLEFT])   / 128) + 1;
    dint bh = ((mapBounds[superblock_t::BOXTOP]   - block->bbox[superblock_t::BOXBOTTOM]) / 128) + 1;

    block->bbox[superblock_t::BOXRIGHT] = block->bbox[superblock_t::BOXLEFT]   + 128 * de::ceilpow2(bw);
    block->bbox[superblock_t::BOXTOP]   = block->bbox[superblock_t::BOXBOTTOM] + 128 * de::ceilpow2(bh);

    _superBlockmap = block;
}

void NodeBuilder::destroySuperBlockmap()
{
    if(_superBlockmap)
        destroySuperBlock(_superBlockmap);
    _superBlockmap = NULL;
    destroyUnusedSuperBlocks();
}

void  NodeBuilder::updateHEdgeInfo(const HalfEdge& hEdge)
{
    hedge_info_t* info = reinterpret_cast<hedge_info_t*>(hEdge.data);

    info->pDelta = hEdge.twin->vertex->pos - hEdge.vertex->pos;

    info->pLength = info->pDelta.length();
    info->pAngle  = slopeToAngle(info->pDelta.x, info->pDelta.y);

    assert(info->pLength > 0);

    info->pPerp =  hEdge.vertex->pos.y * info->pDelta.x - hEdge.vertex->pos.x * info->pDelta.y;
    info->pPara = -hEdge.vertex->pos.x * info->pDelta.x - hEdge.vertex->pos.y * info->pDelta.y;
}

void NodeBuilder::attachHEdgeInfo(HalfEdge* hEdge, LineDef* line,
    LineDef* sourceLine, Sector* sec, bool back)
{
    assert(hEdge);

    halfEdgeInfo = reinterpret_cast<hedge_info_t**>(std::realloc(halfEdgeInfo, ++numHalfEdgeInfo * sizeof(hedge_info_t*)));

    hedge_info_t* info = halfEdgeInfo[numHalfEdgeInfo-1] = reinterpret_cast<hedge_info_t*>(std::calloc(1, sizeof(hedge_info_t)));
    info->lineDef = line;
    info->back = back;
    info->sector = sec;
    info->sourceLine = sourceLine;
    hEdge->data = info;
}

HalfEdge* NodeBuilder::createHEdge(LineDef* line, LineDef* sourceLine,
    Vertex* start, Sector* sec, bool back)
{
    HalfEdge* hEdge = _map.halfEdgeDS().createHEdge(start);
    attachHEdgeInfo(hEdge, line, sourceLine, sec, back);
    return hEdge;
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
HalfEdge* NodeBuilder::splitHEdge(HalfEdge* oldHEdge, ddouble x, ddouble y)
{
    assert(oldHEdge);

    HalfEdge* newHEdge = HEdge_Split(_map.halfEdgeDS(), oldHEdge);
    newHEdge->vertex->pos.x = x;
    newHEdge->vertex->pos.y = y;

    attachHEdgeInfo(newHEdge, ((hedge_info_t*) oldHEdge->data)->lineDef, ((hedge_info_t*) oldHEdge->data)->sourceLine, ((hedge_info_t*) oldHEdge->data)->sector, ((hedge_info_t*) oldHEdge->data)->back);
    attachHEdgeInfo(newHEdge->twin, ((hedge_info_t*) oldHEdge->twin->data)->lineDef, ((hedge_info_t*) oldHEdge->twin->data)->sourceLine, ((hedge_info_t*) oldHEdge->twin->data)->sector, ((hedge_info_t*) oldHEdge->twin->data)->back);

    memcpy(newHEdge->data, oldHEdge->data, sizeof(hedge_info_t));
    ((hedge_info_t*) newHEdge->data)->block = NULL;
    memcpy(newHEdge->twin->data, oldHEdge->twin->data, sizeof(hedge_info_t));
    ((hedge_info_t*) newHEdge->twin->data)->block = NULL;

    // Update along-linedef relationships.
    if(((hedge_info_t*) oldHEdge->data)->lineDef)
    {
        LineDef* lineDef = ((hedge_info_t*) oldHEdge->data)->lineDef;
        if(((hedge_info_t*) oldHEdge->data)->back)
        {
            if(lineDef->hEdges[0] == oldHEdge->twin)
                lineDef->hEdges[0] = newHEdge->twin;
        }
        else
        {
            if(lineDef->hEdges[1] == oldHEdge)
                lineDef->hEdges[1] = newHEdge;
        }
    }

    updateHEdgeInfo(*oldHEdge);
    updateHEdgeInfo(*oldHEdge->twin);
    updateHEdgeInfo(*newHEdge);
    updateHEdgeInfo(*newHEdge->twin);

    /*if(!oldHEdge->twin->face && ((hedge_info_t*)oldHEdge->twin->data)->block)
    {
        SuperBlock_IncHEdgeCounts(((hedge_info_t*)oldHEdge->twin->data)->block, ((hedge_info_t*) newHEdge->twin->data)->lineDef != NULL);
        SuperBlock_PushHEdge(((hedge_info_t*)oldHEdge->twin->data)->block, newHEdge->twin);
        ((hedge_info_t*) newHEdge->twin->data)->block = ((hedge_info_t*)oldHEdge->twin->data)->block;
    }*/

    return newHEdge;
}

static __inline dint pointOnHEdgeSide(ddouble x, ddouble y, const HalfEdge* part)
{
    hedge_info_t* data = (hedge_info_t*) part->data;
    return P_PointOnLineDefSide2(x, y, data->pDelta.x, data->pDelta.y, data->pPerp,
                                 data->pLength, DIST_EPSILON);
}

/**
 * @algorithm Cast a line horizontally or vertically and see what we hit.
 * @todo The blockmap has already been constructed by this point; so use it!
 */
static void testForWindowEffect(Map& map, LineDef* l)
{
    ddouble backDist = MAXFLOAT;
    Sector* backOpen = NULL;
    ddouble frontDist = MAXFLOAT;
    Sector* frontOpen = NULL;
    LineDef* frontLine = NULL, *backLine = NULL;

    Vector2d center = l->buildData.v[0]->pos + l->buildData.v[1]->pos;
    center.x /= 2;
    center.y /= 2;

    Vector2d lineDelta = l->buildData.v[1]->pos - l->buildData.v[0]->pos;

    bool castHoriz = (fabs(lineDelta.x) < fabs(lineDelta.y)? true : false);

    for(duint i = 0; i < map.numLineDefs(); ++i)
    {
        LineDef* n = map.lineDefs[i];

        if(n == l || (n->buildData.sideDefs[LineDef::FRONT] && n->buildData.sideDefs[LineDef::BACK] &&
            n->buildData.sideDefs[LineDef::FRONT]->sector() == n->buildData.sideDefs[LineDef::BACK]->sector()) /*|| n->buildData.overlap */)
            continue;

        Vector2d delta = n->buildData.v[1]->pos - n->buildData.v[0]->pos;
        SideDef* hitSide;
        bool isFront;
        ddouble dist;

        if(castHoriz)
        {   // Horizontal.
            if(fabs(delta.y) < DIST_EPSILON)
                continue;

            if((max(n->buildData.v[0]->pos.y, n->buildData.v[1]->pos.y) < center.y - DIST_EPSILON) ||
               (min(n->buildData.v[0]->pos.y, n->buildData.v[1]->pos.y) > center.y + DIST_EPSILON))
                continue;

            dist = (n->buildData.v[0]->pos.x + (center.y - n->buildData.v[0]->pos.y) * delta.x / delta.y) - center.x;

            isFront = (((lineDelta.y > 0) != (dist > 0)) ? true : false);

            dist = fabs(dist);
            if(dist < DIST_EPSILON)
                continue; // Too close (overlapping lines ?)

            hitSide = n->buildData.sideDefs[(lineDelta.y > 0) ^ (delta.y > 0) ^ !isFront];
        }
        else
        {   // Vertical.
            if(fabs(delta.x) < DIST_EPSILON)
                continue;

            if((max(n->buildData.v[0]->pos.x, n->buildData.v[1]->pos.x) < center.x - DIST_EPSILON) ||
               (min(n->buildData.v[0]->pos.x, n->buildData.v[1]->pos.x) > center.x + DIST_EPSILON))
                continue;

            dist = (n->buildData.v[0]->pos.y + (center.x - n->buildData.v[0]->pos.x) * delta.y / delta.x) - center.y;

            isFront = (((lineDelta.x > 0) == (dist > 0)) ? true : false);

            dist = fabs(dist);

            hitSide = n->buildData.sideDefs[(lineDelta.x > 0) ^ (delta.x > 0) ^ !isFront];
        }

        if(dist < DIST_EPSILON) // Too close (overlapping lines ?)
            continue;

        if(isFront)
        {
            if(dist < frontDist)
            {
                frontDist = dist;
                if(hitSide)
                    frontOpen = &hitSide->sector();
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
                    backOpen = &hitSide->sector();
                else
                    backOpen = NULL;

                backLine = n;
            }
        }
    }

/*
LOG_DEBUG("back line: %d  back dist: %1.1f  back_open: %s")
    << (backLine? backLine->buildData.index : -1) << backDist << (backOpen? "OPEN" : "CLOSED");
LOG_DEBUG("front line: %d  front dist: %1.1f  front_open: %s")
    << (frontLine? frontLine->buildData.index : -1) << frontDist << (frontOpen? "OPEN" : "CLOSED");
*/

    if(backOpen && frontOpen && backOpen == &l->buildData.sideDefs[LineDef::FRONT]->sector())
    {
        LOG_MESSAGE("LineDef #%d seems to be a One-Sided Window (back faces sector #%d).")
            << (l->buildData.index - 1) << (backOpen->buildData.index - 1);

        l->buildData.windowEffect = frontOpen;
    }
}

static void countVertexLineOwners(linedefowner_t* lineOwners, duint* oneSided, duint* twoSided)
{
    if(lineOwners)
    {
        linedefowner_t* owner;

        owner = lineOwners;
        do
        {
            if(!owner->lineDef->buildData.sideDefs[LineDef::FRONT] ||
               !owner->lineDef->buildData.sideDefs[LineDef::BACK])
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
static dint C_DECL lineAngleSorter2(const void* a, const void* b)
{
    dbinangle angles[2];
    linedefowner_t* own[2];
    own[0] = (linedefowner_t*) a;
    own[1] = (linedefowner_t*) b;
    for(duint i = 0; i < 2; ++i)
    {
        LineDef* line = own[i]->lineDef;
        Vertex* otherVtx = line->buildData.v[line->buildData.v[0] == rootVtx? 1:0];

        Vector2d delta = otherVtx->pos - rootVtx->pos;
        angles[i] = bamsAtan2(-100 * dint(delta.x), 100 * dint(delta.y));
    }

    return (angles[0] - angles[1]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return              Ptr to the newly merged list.
 */
static linedefowner_t* mergeLineOwners2(linedefowner_t* left, linedefowner_t* right,
                                        dint (C_DECL *compare) (const void* a, const void* b))
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
                                     dint (C_DECL *compare) (const void* a, const void* b))
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

static void setVertexLineOwner2(linedefowner_t** lineOwners, duint* numLineOwners,
                                LineDef* lineDef, dbyte vertex,
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
    lineDef->vo[vertex] = (lineowner_t*) newOwner;
}

void NodeBuilder::createInitialHEdges()
{
typedef struct {
    duint numLineOwners;
    linedefowner_t* lineOwners; // Head of the lineowner list.
} ownerinfo_t;

    duint startTime = Sys_GetRealTime();

    linedefowner_t* lineOwners, *storage;
    ownerinfo_t* vertexInfo;
    duint i, numVertices;

    numVertices = _map.halfEdgeDS().numVertices();
    vertexInfo = reinterpret_cast<ownerinfo_t*>(std::calloc(1, sizeof(*vertexInfo) * numVertices));

    // We know how many lineowners are needed: number of lineDefs * 2.
    lineOwners = storage = reinterpret_cast<linedefowner_t*>(std::calloc(1, sizeof(*lineOwners) * _map.numLineDefs() * 2));

    for(i = 0; i < _map.numLineDefs(); ++i)
    {
        LineDef* lineDef = _map.lineDefs[i];
        Vertex* from, *to;

        if(lineDef->polyobjOwned)
            continue;

        from = lineDef->buildData.v[0];
        to = lineDef->buildData.v[1];

        setVertexLineOwner2(&vertexInfo[((MVertex*) from->data)->index - 1].lineOwners,
                            &vertexInfo[((MVertex*) from->data)->index - 1].numLineOwners,
                            lineDef, 0, &storage);
        setVertexLineOwner2(&vertexInfo[((MVertex*) to->data)->index - 1].lineOwners,
                            &vertexInfo[((MVertex*) to->data)->index - 1].numLineOwners,
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
    duint i, oneSiders, twoSiders;

    for(i = 0; i < _map.numLineDefs(); ++i)
    {
        LineDef* lineDef = _map.lineDefs[i];
        Vertex* from, *to;

        if(lineDef->polyobjOwned)
            continue;

        if((lineDef->buildData.sideDefs[LineDef::FRONT] && lineDef->buildData.sideDefs[LineDef::BACK]) ||
           !lineDef->buildData.sideDefs[LineDef::FRONT] /* ||
           lineDef->buildData.overlap*/)
            continue;

        from = lineDef->buildData.v[0];
        to = lineDef->buildData.v[1];

        oneSiders = twoSiders = 0;
        countVertexLineOwners(vertexInfo[((MVertex*) from->data)->index - 1].lineOwners,
                              &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*#if _DEBUG
Con_Message("FUNNY LINE %d : start vertex %d has odd number of one-siders\n",
        i, lineDef->buildData.v[0]->index);
#endif*/
            testForWindowEffect(_map, lineDef);
            continue;
        }

        oneSiders = twoSiders = 0;
        countVertexLineOwners(vertexInfo[((MVertex*) to->data)->index - 1].lineOwners,
                              &oneSiders, &twoSiders);

        if((oneSiders % 2) == 1 && (oneSiders + twoSiders) > 1)
        {
/*#if _DEBUG
Con_Message("FUNNY LINE %d : end vertex %d has odd number of one-siders\n",
        i, lineDef->buildData.v[1]->index));
#endif*/
            testForWindowEffect(_map, lineDef);
        }
    }
    }

    for(i = 0; i < _map.numLineDefs(); ++i)
    {
        LineDef* lineDef = _map.lineDefs[i];
        Vertex* from, *to;
        HalfEdge* back, *front;

        if(lineDef->polyobjOwned)
            continue;

        from = lineDef->buildData.v[0];
        to = lineDef->buildData.v[1];

        front = createHEdge(lineDef, lineDef, from, &lineDef->buildData.sideDefs[LineDef::FRONT]->sector(), false);

        // Handle the 'One-Sided Window' trick.
        if(!lineDef->buildData.sideDefs[LineDef::BACK] && lineDef->buildData.windowEffect)
        {
            back = createHEdge(((hedge_info_t*) front->data)->lineDef, lineDef, to, lineDef->buildData.windowEffect, true);
        }
        else
        {
            back = createHEdge(lineDef, lineDef, to, lineDef->buildData.sideDefs[LineDef::BACK]? &lineDef->buildData.sideDefs[LineDef::BACK]->sector() : NULL, true);
        }

        back->twin = front;
        front->twin = back;

        updateHEdgeInfo(*front);
        updateHEdgeInfo(*back);

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

        rootVtx = _map.halfEdgeDS().vertices[i];
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
            HalfEdge* hEdge = owner->hEdge;

            hEdge->prev = owner->next ? owner->next->hEdge->twin : info->lineOwners->hEdge->twin;
            hEdge->prev->next = hEdge;

            hEdge->twin->next = prev->hEdge;
            hEdge->twin->next->prev = hEdge->twin;

            prev = owner;
        } while((owner = owner->next) != NULL);
    }

    // Release temporary storage.
    std::free(lineOwners);
    std::free(vertexInfo);

    // How much time did we spend?
    LOG_VERBOSE("Nodebuilder::createInitialHEdges: Done in %.2f seconds.")
        << ((Sys_GetRealTime() - startTime) / 1000.0f);
}

static dint addHEdgeToSuperBlock(HalfEdge* hEdge, void* context)
{
    superblock_t* block = (superblock_t*) context;

    if(!(((hedge_info_t*) hEdge->data)->lineDef))
        return true; // Continue iteration.

    if(((hedge_info_t*) hEdge->data)->back &&
       !((hedge_info_t*) hEdge->data)->lineDef->buildData.sideDefs[LineDef::BACK])
        return true; // Continue iteration.

    if(((hedge_info_t*) hEdge->data)->back &&
       ((hedge_info_t*) hEdge->data)->lineDef->buildData.windowEffect)
        return true; // Continue iteration.

    BSP_AddHEdgeToSuperBlock(block, hEdge);
    return true; // Continue iteration.
}

void NodeBuilder::createInitialHEdgesAndAddtoSuperBlockmap()
{
    createInitialHEdges();
    _map.halfEdgeDS().iterateHEdges(addHEdgeToSuperBlock, _superBlockmap);
}

/**
 * Add the given half-edge to the specified list.
 */
void BSP_AddHEdgeToSuperBlock(superblock_t* block, HalfEdge* hEdge)
{
    assert(block);
    assert(hEdge);

#define SUPER_IS_LEAF(s)  \
    ((s)->bbox[superblock_t::BOXRIGHT] - (s)->bbox[superblock_t::BOXLEFT] <= 256 && \
     (s)->bbox[superblock_t::BOXTOP] - (s)->bbox[superblock_t::BOXBOTTOM] <= 256)

    for(;;)
    {
        dint p1, p2, child;
        superblock_t* sub;
        hedge_info_t* info = (hedge_info_t*) hEdge->data;

        Vector2i midPoint = Vector2i(block->bbox[superblock_t::BOXLEFT] + block->bbox[superblock_t::BOXRIGHT],
                                     block->bbox[superblock_t::BOXBOTTOM] + block->bbox[superblock_t::BOXTOP]);
        midPoint.x /= 2;
        midPoint.y /= 2;

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

        if(block->bbox[superblock_t::BOXRIGHT] - block->bbox[superblock_t::BOXLEFT] >=
           block->bbox[superblock_t::BOXTOP]   - block->bbox[superblock_t::BOXBOTTOM])
        {   // Block is wider than it is high, or square.
            p1 = hEdge->vertex->pos.x >= midPoint.x;
            p2 = hEdge->twin->vertex->pos.x >= midPoint.x;
        }
        else
        {   // Block is higher than it is wide.
            p1 = hEdge->vertex->pos.y >= midPoint.y;
            p2 = hEdge->twin->vertex->pos.y >= midPoint.y;
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

            if(block->bbox[superblock_t::BOXRIGHT] - block->bbox[superblock_t::BOXLEFT] >=
               block->bbox[superblock_t::BOXTOP]   - block->bbox[superblock_t::BOXBOTTOM])
            {
                sub->bbox[superblock_t::BOXLEFT] =
                    (child? midPoint.x : block->bbox[superblock_t::BOXLEFT]);
                sub->bbox[superblock_t::BOXBOTTOM] = block->bbox[superblock_t::BOXBOTTOM];

                sub->bbox[superblock_t::BOXRIGHT] =
                    (child? block->bbox[superblock_t::BOXRIGHT] : midPoint.x);
                sub->bbox[superblock_t::BOXTOP] = block->bbox[superblock_t::BOXTOP];
            }
            else
            {
                sub->bbox[superblock_t::BOXLEFT] = block->bbox[superblock_t::BOXLEFT];
                sub->bbox[superblock_t::BOXBOTTOM] =
                    (child? midPoint.y : block->bbox[superblock_t::BOXBOTTOM]);

                sub->bbox[superblock_t::BOXRIGHT] = block->bbox[superblock_t::BOXRIGHT];
                sub->bbox[superblock_t::BOXTOP] =
                    (child? block->bbox[superblock_t::BOXTOP] : midPoint.y);
            }
        }

        block = block->subs[child];
    }

#undef SUPER_IS_LEAF
}

static void sanityCheckClosed(const Face* face)
{
    dint total = 0, gaps = 0;

    const HalfEdge* hEdge = face->hEdge;
    do
    {
        const HalfEdge* a = hEdge;
        const HalfEdge* b = hEdge->next;

        if(a->twin->vertex->pos != b->vertex->pos)
            gaps++;

        total++;
    } while((hEdge = hEdge->next) != face->hEdge);

    if(gaps > 0)
    {
        LOG_VERBOSE("HEdge list for face #%p is not closed (%d gaps, %d half-edges).")
            << face << gaps << total;

/*hEdge = face->hEdge;
do
{
    LOG_DEBUG("  half-edge %p (%s) --> (%s)") << hEdge << hEdge->vertex->pos << hEdge->twin->vertex->pos;
} while((hEdge = hEdge->next) != face->hEdge);*/
    }
}

static void sanityCheckSameSector(const Face* face)
{
    const HalfEdge* cur = NULL;
    const hedge_info_t* data;

    {
    const HalfEdge* n;

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

        if(curData->lineDef)
            LOG_VERBOSE("Sector #%d has sidedef facing #%d (line #%d).")
                << data->sector->buildData.index
                << curData->sector->buildData.index
                << curData->lineDef->buildData.index;
        else
            LOG_VERBOSE("Sector #%d has sidedef facing #%d.")
                << data->sector->buildData.index
                << curData->sector->buildData.index;
    } while((cur = cur->next) != face->hEdge);
}

static bool sanityCheckHasRealHEdge(const Face* face)
{
    const HalfEdge* n;

    n = face->hEdge;
    do
    {
        if(((const hedge_info_t*) n->data)->lineDef)
            return true;
    } while((n = n->next) != face->hEdge);

    return false;
}

static bool C_DECL finishLeaf(BinaryTree<void*>* tree, void* data)
{
    if(tree->isLeaf())
    {
        Face* face = reinterpret_cast<Face*>(tree->data());

        face->switchToHEdgeLinks();

        sanityCheckClosed(face);
        sanityCheckSameSector(face);
        if(!sanityCheckHasRealHEdge(face))
        {
            LOG_ERROR("BSP leaf #%p has no linedef-linked half-edge.") << face;
        }
    }

    return true; // Continue traversal.
}

void NodeBuilder::takeHEdgesFromSuperBlock(Face& face, superblock_t* block)
{
    HalfEdge* hEdge;
    while((hEdge = SuperBlock_PopHEdge(block)))
    {
        ((hedge_info_t*) hEdge->data)->block = NULL;
        // Link it into head of the leaf's list.
        face.linkHEdge(hEdge);
    }

    // Recursively handle sub-blocks.
    for(duint num = 0; num < 2; ++num)
    {
        superblock_t* a = block->subs[num];

        if(a)
        {
            takeHEdgesFromSuperBlock(face, a);

            if(a->realNum + a->miniNum > 0)
                LOG_ERROR("takeHEdgesFromSuperBlock: child %d not empty!") << num;

            destroySuperBlock(a);
            block->subs[num] = NULL;
        }
    }

    block->realNum = block->miniNum = 0;
}

Face& NodeBuilder::createBSPLeaf(Face& face, superblock_t* hEdgeList)
{
    takeHEdgesFromSuperBlock(face, hEdgeList);
    return face;
}

static void makeIntersection(cutlist_t* cutList, HalfEdge* hEdge,
                             ddouble x, ddouble y, ddouble dX, ddouble dY,
                             const HalfEdge* partHEdge)
{
    if(!CutList_Find(cutList, hEdge))
        CutList_Intersect(cutList, hEdge,
            M_ParallelDist(dX, dY, ((hedge_info_t*) partHEdge->data)->pPara, ((hedge_info_t*) partHEdge->data)->pLength,
                           hEdge->vertex->pos.x, hEdge->vertex->pos.y));
}

/**
 * Calculate the intersection location between the current half-edge and the
 * partition. Takes advantage of some common situations like horizontal and
 * vertical lines to choose a 'nicer' intersection point.
 */
static __inline void calcIntersection(const HalfEdge* cur, ddouble x,
    ddouble y, ddouble dX, ddouble dY, ddouble perpC, ddouble perpD,
    ddouble* iX, ddouble* iY)
{
    hedge_info_t* data = (hedge_info_t*) cur->data;
    ddouble ds;

    // Horizontal partition against vertical half-edge.
    if(dY == 0 && data->pDelta.x == 0)
    {
        *iX = cur->vertex->pos.x;
        *iY = y;
        return;
    }

    // Vertical partition against horizontal half-edge.
    if(dX == 0 && data->pDelta.y == 0)
    {
        *iX = x;
        *iY = cur->vertex->pos.y;
        return;
    }

    // 0 = start, 1 = end.
    ds = perpC / (perpC - perpD);

    if(data->pDelta.x == 0)
        *iX = cur->vertex->pos.x;
    else
        *iX = cur->vertex->pos.x + (data->pDelta.x * ds);

    if(data->pDelta.y == 0)
        *iY = cur->vertex->pos.y;
    else
        *iY = cur->vertex->pos.y + (data->pDelta.y * ds);
}

void NodeBuilder::divideOneHEdge(HalfEdge* curHEdge, ddouble x,
   ddouble y, ddouble dX, ddouble dY, const HalfEdge* partHEdge,
   superblock_t* bRight, superblock_t* bLeft)
{
    hedge_info_t* data = ((hedge_info_t*) curHEdge->data);
    HalfEdge* newHEdge;
    ddouble perpC, perpD;

    // Get state of lines' relation to each other.
    if(partHEdge && data->sourceLine ==
       ((hedge_info_t*) partHEdge->data)->sourceLine)
    {
        perpC = perpD = 0;
    }
    else
    {
        hedge_info_t* part = (hedge_info_t*) partHEdge->data;

        perpC = M_PerpDist(dX, dY, part->pPerp, part->pLength,
                       curHEdge->vertex->pos.x, curHEdge->vertex->pos.y);
        perpD = M_PerpDist(dX, dY, part->pPerp, part->pLength,
                       curHEdge->twin->vertex->pos.x, curHEdge->twin->vertex->pos.y);
    }

    // Check for being on the same line.
    if(fabs(perpC) <= DIST_EPSILON && fabs(perpD) <= DIST_EPSILON)
    {
        makeIntersection(_cutList, curHEdge, x, y, dX, dY, partHEdge);
        makeIntersection(_cutList, curHEdge->twin, x, y, dX, dY, partHEdge);

        // This seg runs along the same line as the partition. Check
        // whether it goes in the same direction or the opposite.
        if(data->pDelta.x * dX + data->pDelta.y * dY < 0)
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
    if(perpC > -DIST_EPSILON && perpD > -DIST_EPSILON)
    {
        if(perpC < DIST_EPSILON)
            makeIntersection(_cutList, curHEdge, x, y, dX, dY, partHEdge);
        else if(perpD < DIST_EPSILON)
            makeIntersection(_cutList, curHEdge->twin, x, y, dX, dY, partHEdge);

        BSP_AddHEdgeToSuperBlock(bRight, curHEdge);
        return;
    }

    // Check for left side.
    if(perpC < DIST_EPSILON && perpD < DIST_EPSILON)
    {
        if(perpC > -DIST_EPSILON)
            makeIntersection(_cutList, curHEdge, x, y, dX, dY, partHEdge);
        else if(perpD > -DIST_EPSILON)
            makeIntersection(_cutList, curHEdge->twin, x, y, dX, dY, partHEdge);

        BSP_AddHEdgeToSuperBlock(bLeft, curHEdge);
        return;
    }

    // When we reach here, we have perpC and perpD non-zero and opposite sign,
    // hence this edge will be split by the partition line.
    {
    ddouble iX, iY;
    calcIntersection(curHEdge, x, y, dX, dY, perpC, perpD, &iX, &iY);
    newHEdge = splitHEdge(curHEdge, iX, iY);
    makeIntersection(_cutList, newHEdge, x, y, dX, dY, partHEdge);
    }

    if(perpC < 0)
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

void NodeBuilder::divideHEdges(superblock_t* hEdgeList, ddouble x, ddouble y,
    ddouble dX, ddouble dY, const HalfEdge* partHEdge, superblock_t* rights,
    superblock_t* lefts)
{
    HalfEdge* hEdge;
    duint num;

    while((hEdge = SuperBlock_PopHEdge(hEdgeList)))
    {
        ((hedge_info_t*) hEdge->data)->block = NULL;
        divideOneHEdge(hEdge, x, y, dX, dY, partHEdge, rights, lefts);
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        superblock_t* a = hEdgeList->subs[num];

        if(a)
        {
            divideHEdges(a, x, y, dX, dY, partHEdge, rights, lefts);

            if(a->realNum + a->miniNum > 0)
                LOG_ERROR("NodeBuilder::divideHEdges: child %d not empty!") << num;

            destroySuperBlock(a);
            hEdgeList->subs[num] = NULL;
        }
    }

    hEdgeList->realNum = hEdgeList->miniNum = 0;
}

void NodeBuilder::addMiniHEdges(ddouble x, ddouble y, ddouble dX, ddouble dY,
    const HalfEdge* partHEdge, superblock_t* bRight, superblock_t* bLeft)
{
    BSP_MergeOverlappingIntersections(_cutList);
    connectGaps(x, y, dX, dY, partHEdge, bRight, bLeft);
}

void NodeBuilder::partitionHEdges(superblock_t* hEdgeList, ddouble x,
    ddouble y, ddouble dX, ddouble dY, const HalfEdge* partHEdge,
    superblock_t** right, superblock_t** left)
{
    superblock_t* bRight = createSuperBlock();
    superblock_t* bLeft = createSuperBlock();

    M_CopyBox(bRight->bbox, hEdgeList->bbox);
    M_CopyBox(bLeft->bbox, hEdgeList->bbox);

    divideHEdges(hEdgeList, x, y, dX, dY, partHEdge, bRight, bLeft);

    // Sanity checks...
    if(bRight->realNum + bRight->miniNum == 0)
        LOG_ERROR("NodeBuilder::partitionHEdges: Separated halfedge-list has no right side.");

    if(bLeft->realNum + bLeft->miniNum == 0)
        LOG_ERROR("NodeBuilder::partitionHEdges: Separated halfedge-list has no left side.");

    addMiniHEdges(x, y, dX, dY, partHEdge, bRight, bLeft);
    CutList_Reset(_cutList);

    *right = bRight;
    *left = bLeft;
}

BinaryTree<void*>* NodeBuilder::buildNodes(superblock_t* hEdgeList)
{
    // Pick half-edge with which to derive the next partition.
    HalfEdge* partHEdge;
    if(!(partHEdge = SuperBlock_PickPartition(hEdgeList, _splitFactor)))
    {   // No partition required, already convex.
        return new BinaryTree<void*>(reinterpret_cast<void*>(&createBSPLeaf(_map.halfEdgeDS().createFace(), hEdgeList)));
    }

    const Vector2d& pos = partHEdge->vertex->pos;
    const Vector2d delta = partHEdge->twin->vertex->pos - partHEdge->vertex->pos;

/*#if _DEBUG
LOG_DEBUG("NodeBuilder::buildNodes: Partition xy{%1.0f, %1.0f} delta{%1.0f, %1.0f}.")
    << dfloat(pos.x) << dfloat(pos.y) << dfloat(delta.x) << dfloat(delta.y);
#endif*/

    superblock_t* rightHEdges, *leftHEdges;
    dfloat rightBBox[4], leftBBox[4];

    partitionHEdges(hEdgeList, pos.x, pos.y, delta.x, delta.y, partHEdge, &rightHEdges, &leftHEdges);
    BSP_FindBBoxForHEdges(rightHEdges, rightBBox);
    BSP_FindBBoxForHEdges(leftHEdges, leftBBox);

    BinaryTree<void*>* tree = new BinaryTree<void*>(reinterpret_cast<void*>(_map.createNode(dfloat(pos.x), dfloat(pos.y), dfloat(delta.x), dfloat(delta.y), rightBBox, leftBBox)));

    // Recurse on right half-edge list.
    tree->setRight(buildNodes(rightHEdges));
    destroySuperBlock(rightHEdges);

    // Recurse on left half-edge list.
    tree->setLeft(buildNodes(leftHEdges));
    destroySuperBlock(leftHEdges);

    return tree;
}

void NodeBuilder::build()
{
    createInitialHEdgesAndAddtoSuperBlockmap();

    rootNode = buildNodes(_superBlockmap);
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
    if(rootNode)
        rootNode->postOrder(finishLeaf, NULL);
}
