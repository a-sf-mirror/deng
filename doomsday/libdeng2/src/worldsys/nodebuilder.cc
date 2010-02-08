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
#include "de/NodeBuilder"
#include "de/BSPSuperBlock"
#include "de/BSPEdge"
#include "de/BSPIntersection"
#include "de/Map"

using namespace de;

namespace de
{
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
    destroyRootSuperBlockmap();
}

void NodeBuilder::destroyUnusedSuperBlocks()
{
    while(_quickAllocSuperBlockmaps)
    {
        SuperBlockmap* blockmap = _quickAllocSuperBlockmaps;
        _quickAllocSuperBlockmaps = blockmap->subs[0];
        blockmap->subs[0] = NULL;
        delete blockmap;
    }
}

void NodeBuilder::moveSuperBlockToQuickAllocList(SuperBlockmap* blockmap)
{
    dint num;

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        if(blockmap->subs[num])
        {
            moveSuperBlockToQuickAllocList(blockmap->subs[num]);
            blockmap->subs[num] = NULL;
        }
    }

    // Add blockmap to quick-alloc list. Note that subs[0] is used for
    // linking the blocks together.
    blockmap->subs[0] = _quickAllocSuperBlockmaps;
    blockmap->parent = NULL;
    _quickAllocSuperBlockmaps = blockmap;
}

SuperBlockmap* NodeBuilder::createSuperBlockmap()
{
    if(_quickAllocSuperBlockmaps == NULL)
        return new SuperBlockmap();

    SuperBlockmap* blockmap = _quickAllocSuperBlockmaps;
    _quickAllocSuperBlockmaps = blockmap->subs[0];
    // Clear out any old rubbish.
    memset(blockmap, 0, sizeof(*blockmap));
    return blockmap;
}

void NodeBuilder::destroySuperBlockmap(SuperBlockmap* blockmap)
{
    moveSuperBlockToQuickAllocList(blockmap);
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

void NodeBuilder::createRootSuperBlockmap()
{   
    _quickAllocSuperBlockmaps = NULL;

    SuperBlockmap* blockmap = createSuperBlockmap();
    dint mapBounds[4];
    findMapLimits(_map, mapBounds);

    blockmap->bbox[SuperBlockmap::BOXLEFT]   = mapBounds[SuperBlockmap::BOXLEFT]   - (mapBounds[SuperBlockmap::BOXLEFT]   & 0x7);
    blockmap->bbox[SuperBlockmap::BOXBOTTOM] = mapBounds[SuperBlockmap::BOXBOTTOM] - (mapBounds[SuperBlockmap::BOXBOTTOM] & 0x7);

    dint bw = ((mapBounds[SuperBlockmap::BOXRIGHT] - blockmap->bbox[SuperBlockmap::BOXLEFT])   / 128) + 1;
    dint bh = ((mapBounds[SuperBlockmap::BOXTOP]   - blockmap->bbox[SuperBlockmap::BOXBOTTOM]) / 128) + 1;

    blockmap->bbox[SuperBlockmap::BOXRIGHT] = blockmap->bbox[SuperBlockmap::BOXLEFT]   + 128 * de::ceilpow2(bw);
    blockmap->bbox[SuperBlockmap::BOXTOP]   = blockmap->bbox[SuperBlockmap::BOXBOTTOM] + 128 * de::ceilpow2(bh);

    _superBlockmap = blockmap;
}

void NodeBuilder::destroyRootSuperBlockmap()
{
    if(_superBlockmap)
        destroySuperBlockmap(_superBlockmap);
    _superBlockmap = NULL;
    destroyUnusedSuperBlocks();
}

void  NodeBuilder::updateHEdgeInfo(const HalfEdge& hEdge)
{
    HalfEdgeInfo* info = reinterpret_cast<HalfEdgeInfo*>(hEdge.data);

    info->pDelta = hEdge.twin->vertex->pos - hEdge.vertex->pos;

    info->pLength = info->pDelta.length();
    info->pAngle  = slopeToAngle(info->pDelta.x, info->pDelta.y);

    assert(info->pLength > 0);

    info->pPerp =  hEdge.vertex->pos.y * info->pDelta.x - hEdge.vertex->pos.x * info->pDelta.y;
    info->pPara = -hEdge.vertex->pos.x * info->pDelta.x - hEdge.vertex->pos.y * info->pDelta.y;
}

void NodeBuilder::attachHEdgeInfo(HalfEdge& hEdge, LineDef* line,
    LineDef* sourceLine, Sector* sec, bool back)
{
    halfEdgeInfo = reinterpret_cast<HalfEdgeInfo**>(std::realloc(halfEdgeInfo, ++numHalfEdgeInfo * sizeof(HalfEdgeInfo*)));

    HalfEdgeInfo* info = halfEdgeInfo[numHalfEdgeInfo-1] = reinterpret_cast<HalfEdgeInfo*>(std::calloc(1, sizeof(HalfEdgeInfo)));
    info->lineDef = line;
    info->back = back;
    info->sector = sec;
    info->sourceLine = sourceLine;

    hEdge.data = info;
}

HalfEdge& NodeBuilder::createHalfEdge(LineDef* line, LineDef* sourceLine,
    Vertex* start, Sector* sec, bool back)
{
    HalfEdge& hEdge = _map.halfEdgeDS().createHalfEdge(*start);
    attachHEdgeInfo(hEdge, line, sourceLine, sec, back);
    return hEdge;
}

HalfEdge& NodeBuilder::splitHEdge(HalfEdge& oldHEdge, ddouble x, ddouble y)
{
    HalfEdge& newHEdge = HEdge_Split(_map.halfEdgeDS(), oldHEdge);
    newHEdge.vertex->pos.x = x;
    newHEdge.vertex->pos.y = y;

    attachHEdgeInfo(newHEdge, ((HalfEdgeInfo*) oldHEdge.data)->lineDef, ((HalfEdgeInfo*) oldHEdge.data)->sourceLine, ((HalfEdgeInfo*) oldHEdge.data)->sector, ((HalfEdgeInfo*) oldHEdge.data)->back);
    attachHEdgeInfo(*newHEdge.twin, ((HalfEdgeInfo*) oldHEdge.twin->data)->lineDef, ((HalfEdgeInfo*) oldHEdge.twin->data)->sourceLine, ((HalfEdgeInfo*) oldHEdge.twin->data)->sector, ((HalfEdgeInfo*) oldHEdge.twin->data)->back);

    memcpy(newHEdge.data, oldHEdge.data, sizeof(HalfEdgeInfo));
    ((HalfEdgeInfo*) newHEdge.data)->blockmap = NULL;
    memcpy(newHEdge.twin->data, oldHEdge.twin->data, sizeof(HalfEdgeInfo));
    ((HalfEdgeInfo*) newHEdge.twin->data)->blockmap = NULL;

    // Update along-linedef relationships.
    if(((HalfEdgeInfo*) oldHEdge.data)->lineDef)
    {
        LineDef* lineDef = ((HalfEdgeInfo*) oldHEdge.data)->lineDef;
        if(((HalfEdgeInfo*) oldHEdge.data)->back)
        {
            if(lineDef->hEdges[0] == oldHEdge.twin)
                lineDef->hEdges[0] = newHEdge.twin;
        }
        else
        {
            if(lineDef->hEdges[1] == &oldHEdge)
                lineDef->hEdges[1] = &newHEdge;
        }
    }

    updateHEdgeInfo(oldHEdge);
    updateHEdgeInfo(*oldHEdge.twin);
    updateHEdgeInfo(newHEdge);
    updateHEdgeInfo(*newHEdge.twin);

    /*if(!oldHEdge.twin->face && ((HalfEdgeInfo*)oldHEdge.twin->data)->blockmap)
    {
        SuperBlock_IncHEdgeCounts(((HalfEdgeInfo*)oldHEdge.twin->data)->blockmap, ((HalfEdgeInfo*) newHEdge.twin->data)->lineDef != NULL);
        SuperBlock_PushHEdge(((HalfEdgeInfo*)oldHEdge.twin->data)->blockmap, newHEdge.twin);
        ((HalfEdgeInfo*) newHEdge.twin->data)->blockmap = ((HalfEdgeInfo*)oldHEdge.twin->data)->blockmap;
    }*/

    return newHEdge;
}

static __inline dint pointOnHEdgeSide(ddouble x, ddouble y, const HalfEdge* part)
{
    HalfEdgeInfo* data = (HalfEdgeInfo*) part->data;
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
            &n->buildData.sideDefs[LineDef::FRONT]->sector() == &n->buildData.sideDefs[LineDef::BACK]->sector()) /*|| n->buildData.overlap */)
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

        if(lineDef->polyobjOwned)
            continue;

        Vertex* from = lineDef->buildData.v[0];
        Vertex* to = lineDef->buildData.v[1];

        // Detect the 'One-Sided Window' trick.
        bool isOneSidedWindow = (!lineDef->buildData.sideDefs[LineDef::BACK] && lineDef->buildData.windowEffect);

        HalfEdge& front = createHalfEdge(lineDef, lineDef, from, &lineDef->buildData.sideDefs[LineDef::FRONT]->sector(), false);
        HalfEdge& back = createHalfEdge(isOneSidedWindow? ((HalfEdgeInfo*) front.data)->lineDef : lineDef, lineDef, to, isOneSidedWindow? lineDef->buildData.windowEffect : lineDef->buildData.sideDefs[LineDef::BACK]? &lineDef->buildData.sideDefs[LineDef::BACK]->sector() : NULL, true);

        back.twin = &front;
        front.twin = &back;

        updateHEdgeInfo(front);
        updateHEdgeInfo(back);

        lineDef->hEdges[0] = lineDef->hEdges[1] = &front;
        ((linedefowner_t*) lineDef->vo[0])->hEdge = &front;
        ((linedefowner_t*) lineDef->vo[1])->hEdge = &back;
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

void NodeBuilder::createInitialHEdgesAndAddtoSuperBlockmap()
{
    createInitialHEdges();
    FOR_EACH(i, _map.halfEdgeDS().halfEdges(), HalfEdgeDS::HalfEdges::const_iterator)
    {
        HalfEdge* hEdge = (*i);

        if(!(((HalfEdgeInfo*) hEdge->data)->lineDef))
            continue;

        if(((HalfEdgeInfo*) hEdge->data)->back &&
           !((HalfEdgeInfo*) hEdge->data)->lineDef->buildData.sideDefs[LineDef::BACK])
            continue;

        if(((HalfEdgeInfo*) hEdge->data)->back &&
           ((HalfEdgeInfo*) hEdge->data)->lineDef->buildData.windowEffect)
            continue;

        addHalfEdgeToSuperBlockmap(_superBlockmap, hEdge);
    }
}

void NodeBuilder::addHalfEdgeToSuperBlockmap(SuperBlockmap* blockmap, HalfEdge* hEdge)
{
    assert(blockmap);
    assert(hEdge);

#define SUPER_IS_LEAF(s)  \
    ((s)->bbox[SuperBlockmap::BOXRIGHT] - (s)->bbox[SuperBlockmap::BOXLEFT] <= 256 && \
     (s)->bbox[SuperBlockmap::BOXTOP] - (s)->bbox[SuperBlockmap::BOXBOTTOM] <= 256)

    for(;;)
    {
        dint p1, p2, child;
        SuperBlockmap* sub;
        HalfEdgeInfo* info = (HalfEdgeInfo*) hEdge->data;

        Vector2i midPoint = Vector2i(blockmap->bbox[SuperBlockmap::BOXLEFT] + blockmap->bbox[SuperBlockmap::BOXRIGHT],
                                     blockmap->bbox[SuperBlockmap::BOXBOTTOM] + blockmap->bbox[SuperBlockmap::BOXTOP]);
        midPoint.x /= 2;
        midPoint.y /= 2;

        // Update half-edge counts.
        if(info->lineDef)
            blockmap->realNum++;
        else
            blockmap->miniNum++;

        if(SUPER_IS_LEAF(blockmap))
        {   // Block is a leaf -- no subdivision possible.
            blockmap->push(hEdge);
            ((HalfEdgeInfo*) hEdge->data)->blockmap = blockmap;
            return;
        }

        if(blockmap->bbox[SuperBlockmap::BOXRIGHT] - blockmap->bbox[SuperBlockmap::BOXLEFT] >=
           blockmap->bbox[SuperBlockmap::BOXTOP]   - blockmap->bbox[SuperBlockmap::BOXBOTTOM])
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
            blockmap->push(hEdge);
            ((HalfEdgeInfo*) hEdge->data)->blockmap = blockmap;
            return;
        }

        // The seg lies in one half of this blockmap. Create the blockmap if it
        // doesn't already exist, and loop back to add the seg.
        if(!blockmap->subs[child])
        {
            blockmap->subs[child] = sub = new SuperBlockmap();
            sub->parent = blockmap;

            if(blockmap->bbox[SuperBlockmap::BOXRIGHT] - blockmap->bbox[SuperBlockmap::BOXLEFT] >=
               blockmap->bbox[SuperBlockmap::BOXTOP]   - blockmap->bbox[SuperBlockmap::BOXBOTTOM])
            {
                sub->bbox[SuperBlockmap::BOXLEFT] =
                    (child? midPoint.x : blockmap->bbox[SuperBlockmap::BOXLEFT]);
                sub->bbox[SuperBlockmap::BOXBOTTOM] = blockmap->bbox[SuperBlockmap::BOXBOTTOM];

                sub->bbox[SuperBlockmap::BOXRIGHT] =
                    (child? blockmap->bbox[SuperBlockmap::BOXRIGHT] : midPoint.x);
                sub->bbox[SuperBlockmap::BOXTOP] = blockmap->bbox[SuperBlockmap::BOXTOP];
            }
            else
            {
                sub->bbox[SuperBlockmap::BOXLEFT] = blockmap->bbox[SuperBlockmap::BOXLEFT];
                sub->bbox[SuperBlockmap::BOXBOTTOM] =
                    (child? midPoint.y : blockmap->bbox[SuperBlockmap::BOXBOTTOM]);

                sub->bbox[SuperBlockmap::BOXRIGHT] = blockmap->bbox[SuperBlockmap::BOXRIGHT];
                sub->bbox[SuperBlockmap::BOXTOP] =
                    (child? blockmap->bbox[SuperBlockmap::BOXTOP] : midPoint.y);
            }
        }

        blockmap = blockmap->subs[child];
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
    const HalfEdgeInfo* data;

    {
    const HalfEdge* n;

    // Find a suitable half-edge for comparison.
    n = face->hEdge;
    do
    {
        if(!((HalfEdgeInfo*) n->data)->sector)
            continue;

        cur = n;
        break;
    } while((n = n->next) != face->hEdge);

    if(!cur)
        return;

    data = (HalfEdgeInfo*) cur->data;
    cur = cur->next;
    }

    do
    {
        const HalfEdgeInfo* curData = (HalfEdgeInfo*) cur->data;

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
        if(((const HalfEdgeInfo*) n->data)->lineDef)
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

void NodeBuilder::takeHEdgesFromSuperBlock(Face& face, SuperBlockmap* blockmap)
{
    HalfEdge* hEdge;
    while((hEdge = blockmap->pop()))
    {
        ((HalfEdgeInfo*) hEdge->data)->blockmap = NULL;
        // Link it into head of the leaf's list.
        face.linkHEdge(hEdge);
    }

    // Recursively handle sub-blocks.
    for(duint num = 0; num < 2; ++num)
    {
        SuperBlockmap* a = blockmap->subs[num];

        if(a)
        {
            takeHEdgesFromSuperBlock(face, a);

            if(a->realNum + a->miniNum > 0)
                LOG_ERROR("takeHEdgesFromSuperBlock: child %d not empty!") << num;

            destroySuperBlockmap(a);
            blockmap->subs[num] = NULL;
        }
    }

    blockmap->realNum = blockmap->miniNum = 0;
}

Face& NodeBuilder::createBSPLeaf(Face& face, SuperBlockmap* hEdgeList)
{
    takeHEdgesFromSuperBlock(face, hEdgeList);
    return face;
}

static void makeIntersection(CutList& cutList, HalfEdge& hEdge,
                             ddouble x, ddouble y, ddouble dX, ddouble dY,
                             const HalfEdgeInfo* partInfo)
{
    if(!cutList.find(hEdge))
        cutList.intersect(hEdge,
            M_ParallelDist(dX, dY, partInfo->pPara, partInfo->pLength,
                           hEdge.vertex->pos.x, hEdge.vertex->pos.y));
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
    HalfEdgeInfo* data = (HalfEdgeInfo*) cur->data;
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

void NodeBuilder::divideOneHEdge(HalfEdge& curHEdge, ddouble x,
   ddouble y, ddouble dX, ddouble dY, const HalfEdgeInfo* partInfo,
   SuperBlockmap* bRight, SuperBlockmap* bLeft)
{
    HalfEdgeInfo* data = ((HalfEdgeInfo*) curHEdge.data);
    ddouble perpC, perpD;

    // Get state of lines' relation to each other.
    if(data->sourceLine == partInfo->sourceLine)
    {
        perpC = perpD = 0;
    }
    else
    {
        perpC = M_PerpDist(dX, dY, partInfo->pPerp, partInfo->pLength,
                       curHEdge.vertex->pos.x, curHEdge.vertex->pos.y);
        perpD = M_PerpDist(dX, dY, partInfo->pPerp, partInfo->pLength,
                       curHEdge.twin->vertex->pos.x, curHEdge.twin->vertex->pos.y);
    }

    // Check for being on the same line.
    if(fabs(perpC) <= DIST_EPSILON && fabs(perpD) <= DIST_EPSILON)
    {
        makeIntersection(_cutList, curHEdge, x, y, dX, dY, partInfo);
        makeIntersection(_cutList, *curHEdge.twin, x, y, dX, dY, partInfo);

        // This seg runs along the same line as the partition. Check
        // whether it goes in the same direction or the opposite.
        if(data->pDelta.x * dX + data->pDelta.y * dY < 0)
        {
            addHalfEdgeToSuperBlockmap(bLeft, &curHEdge);
        }
        else
        {
            addHalfEdgeToSuperBlockmap(bRight, &curHEdge);
        }

        return;
    }

    // Check for right side.
    if(perpC > -DIST_EPSILON && perpD > -DIST_EPSILON)
    {
        if(perpC < DIST_EPSILON)
            makeIntersection(_cutList, curHEdge, x, y, dX, dY, partInfo);
        else if(perpD < DIST_EPSILON)
            makeIntersection(_cutList, *curHEdge.twin, x, y, dX, dY, partInfo);

        addHalfEdgeToSuperBlockmap(bRight, &curHEdge);
        return;
    }

    // Check for left side.
    if(perpC < DIST_EPSILON && perpD < DIST_EPSILON)
    {
        if(perpC > -DIST_EPSILON)
            makeIntersection(_cutList, curHEdge, x, y, dX, dY, partInfo);
        else if(perpD > -DIST_EPSILON)
            makeIntersection(_cutList, *curHEdge.twin, x, y, dX, dY, partInfo);

        addHalfEdgeToSuperBlockmap(bLeft, &curHEdge);
        return;
    }

    // When we reach here, we have perpC and perpD non-zero and opposite sign,
    // hence this edge will be split by the partition line.

    ddouble iX, iY;
    calcIntersection(&curHEdge, x, y, dX, dY, perpC, perpD, &iX, &iY);
    HalfEdge& newHEdge = splitHEdge(curHEdge, iX, iY);
    makeIntersection(_cutList, newHEdge, x, y, dX, dY, partInfo);

    if(perpC < 0)
    {
        addHalfEdgeToSuperBlockmap(bLeft,  &curHEdge);
        addHalfEdgeToSuperBlockmap(bRight, &newHEdge);
    }
    else
    {
        addHalfEdgeToSuperBlockmap(bRight, &curHEdge);
        addHalfEdgeToSuperBlockmap(bLeft,  &newHEdge);
    }

    if(!curHEdge.twin->face && ((HalfEdgeInfo*)curHEdge.twin->data)->blockmap)
        addHalfEdgeToSuperBlockmap(((HalfEdgeInfo*)curHEdge.twin->data)->blockmap, newHEdge.twin);
}

void NodeBuilder::divideHEdges(SuperBlockmap* hEdgeList, ddouble x, ddouble y,
    ddouble dX, ddouble dY, const HalfEdgeInfo* partInfo, SuperBlockmap* rights,
    SuperBlockmap* lefts)
{
    HalfEdge* hEdge;
    duint num;

    while((hEdge = hEdgeList->pop()))
    {
        ((HalfEdgeInfo*) hEdge->data)->blockmap = NULL;
        divideOneHEdge(*hEdge, x, y, dX, dY, partInfo, rights, lefts);
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        SuperBlockmap* a = hEdgeList->subs[num];

        if(a)
        {
            divideHEdges(a, x, y, dX, dY, partInfo, rights, lefts);

            if(a->realNum + a->miniNum > 0)
                LOG_ERROR("NodeBuilder::divideHEdges: child %d not empty!") << num;

            destroySuperBlockmap(a);
            hEdgeList->subs[num] = NULL;
        }
    }

    hEdgeList->realNum = hEdgeList->miniNum = 0;
}

void NodeBuilder::addMiniHEdges(ddouble x, ddouble y, ddouble dX, ddouble dY,
    const HalfEdgeInfo* partInfo, SuperBlockmap* bRight, SuperBlockmap* bLeft)
{
    BSP_MergeOverlappingIntersections(_cutList);
    connectGaps(x, y, dX, dY, partInfo, bRight, bLeft);
}

void NodeBuilder::partitionHEdges(SuperBlockmap* hEdgeList, ddouble x,
    ddouble y, ddouble dX, ddouble dY, const HalfEdgeInfo* partInfo,
    SuperBlockmap** right, SuperBlockmap** left)
{
    SuperBlockmap* bRight = createSuperBlockmap();
    SuperBlockmap* bLeft = createSuperBlockmap();

    M_CopyBox(bRight->bbox, hEdgeList->bbox);
    M_CopyBox(bLeft->bbox, hEdgeList->bbox);

    divideHEdges(hEdgeList, x, y, dX, dY, partInfo, bRight, bLeft);

    // Sanity checks...
    if(bRight->realNum + bRight->miniNum == 0)
        LOG_ERROR("NodeBuilder::partitionHEdges: Separated halfedge-list has no right side.");

    if(bLeft->realNum + bLeft->miniNum == 0)
        LOG_ERROR("NodeBuilder::partitionHEdges: Separated halfedge-list has no left side.");

    addMiniHEdges(x, y, dX, dY, partInfo, bRight, bLeft);
    _cutList.clear();

    *right = bRight;
    *left = bLeft;
}

BinaryTree<void*>* NodeBuilder::buildNodes(SuperBlockmap* hEdgeList)
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

    SuperBlockmap* rightHEdges, *leftHEdges;
    dfloat rightBBox[4], leftBBox[4];

    partitionHEdges(hEdgeList, pos.x, pos.y, delta.x, delta.y, reinterpret_cast<HalfEdgeInfo*>(partHEdge->data), &rightHEdges, &leftHEdges);
    
    rightHEdges->aaBounds(rightBBox);
    leftHEdges->aaBounds(leftBBox);

    BinaryTree<void*>* tree = new BinaryTree<void*>(reinterpret_cast<void*>(_map.createNode(dfloat(pos.x), dfloat(pos.y), dfloat(delta.x), dfloat(delta.y), rightBBox, leftBBox)));

    // Recurse on right half-edge list.
    tree->setRight(buildNodes(rightHEdges));
    destroySuperBlockmap(rightHEdges);

    // Recurse on left half-edge list.
    tree->setLeft(buildNodes(leftHEdges));
    destroySuperBlockmap(leftHEdges);

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
