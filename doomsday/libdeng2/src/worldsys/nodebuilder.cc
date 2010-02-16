/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
 * Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 * Copyright © 1998 Raphael Quinet <raphael.quinet@eed.ericsson.se>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/Vector"
#include "de/Log"
#include "de/NodeBuilder"
#include "de/SuperBlock"
#include "de/Node"
#include "de/Map"

using namespace de;

namespace
{
    struct LineDefOwner
    {
        LineDef* lineDef;
        struct LineDefOwner* next;

        /**
         * HalfEdge on the front side of the LineDef relative to this vertex
         * (i.e., if vertex is linked to the LineDef as vertex1, this half-edge
         * is that on the LineDef's front side, else, that on the back side).
         */
        HalfEdge* halfEdge;
    };

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
    createSuperBlock();
}

NodeBuilder::~NodeBuilder()
{
    if(halfEdgeInfo)
    {
        for(dsize i = 0; i < numHalfEdgeInfo; ++i)
            std::free(halfEdgeInfo[i]);
        std::free(halfEdgeInfo);
    }
    destroyRootSuperBlock();
    _intersections.clear();
}

void NodeBuilder::destroyUnusedSuperBlocks()
{
    while(_quickAllocSuperBlocks)
    {
        SuperBlock* superBlock = _quickAllocSuperBlocks;
        _quickAllocSuperBlocks = superBlock->rightChild;
        superBlock->rightChild = NULL;
        delete superBlock;
    }
}

void NodeBuilder::moveSuperBlockToQuickAllocList(SuperBlock* superBlock)
{
    // Recursively handle child blocks.
    if(superBlock->rightChild)
    {
        moveSuperBlockToQuickAllocList(superBlock->rightChild);
        superBlock->rightChild = NULL;
    }
    if(superBlock->leftChild)
    {
        moveSuperBlockToQuickAllocList(superBlock->leftChild);
        superBlock->leftChild = NULL;
    }

    // Add superBlock to quick-alloc list. Note that rightChild is used for
    // linking the blocks together.
    superBlock->rightChild = _quickAllocSuperBlocks;
    superBlock->parent = NULL;
    _quickAllocSuperBlocks = superBlock;
}

SuperBlock* NodeBuilder::createSuperBlock()
{
    if(_quickAllocSuperBlocks == NULL)
        return new SuperBlock();

    SuperBlock* superBlock = _quickAllocSuperBlocks;
    _quickAllocSuperBlocks = superBlock->rightChild;
    // Clear out any old rubbish.
    memset(superBlock, 0, sizeof(*superBlock));
    return superBlock;
}

void NodeBuilder::destroySuperBlock(SuperBlock* superBlock)
{
    moveSuperBlockToQuickAllocList(superBlock);
}

static void findMapLimits(Map& map, dint* bbox)
{
    M_ClearBox(bbox);

    FOR_EACH(i, map.lineDefs(), Map::LineDefs::const_iterator)
    {
        LineDef* lineDef = (*i);
        const Vertex& from = *lineDef->buildData.v[0];
        const Vertex& to   = *lineDef->buildData.v[1];

        Vector2d topLeft = from.pos.min(to.pos);
        M_AddToBox(bbox, Vector2i(dint(floor(topLeft.x)), dint(floor(topLeft.y))));

        Vector2d bottomRight = from.pos.max(to.pos);
        M_AddToBox(bbox, Vector2i(dint(ceil(bottomRight.x)), dint(ceil(bottomRight.y))));
    }
}

void NodeBuilder::createRootSuperBlock()
{   
    _quickAllocSuperBlocks = NULL;

    SuperBlock* superBlock = createSuperBlock();
    dint mapBounds[4];
    findMapLimits(_map, mapBounds);

    superBlock->bbox[SuperBlock::BOXLEFT]   = mapBounds[SuperBlock::BOXLEFT]   - (mapBounds[SuperBlock::BOXLEFT]   & 0x7);
    superBlock->bbox[SuperBlock::BOXBOTTOM] = mapBounds[SuperBlock::BOXBOTTOM] - (mapBounds[SuperBlock::BOXBOTTOM] & 0x7);

    dint bw = ((mapBounds[SuperBlock::BOXRIGHT] - superBlock->bbox[SuperBlock::BOXLEFT])   / 128) + 1;
    dint bh = ((mapBounds[SuperBlock::BOXTOP]   - superBlock->bbox[SuperBlock::BOXBOTTOM]) / 128) + 1;

    superBlock->bbox[SuperBlock::BOXRIGHT] = superBlock->bbox[SuperBlock::BOXLEFT]   + 128 * de::ceilpow2(bw);
    superBlock->bbox[SuperBlock::BOXTOP]   = superBlock->bbox[SuperBlock::BOXBOTTOM] + 128 * de::ceilpow2(bh);

    _rootSuperBlock = superBlock;
}

void NodeBuilder::destroyRootSuperBlock()
{
    if(_rootSuperBlock)
        destroySuperBlock(_rootSuperBlock);
    _rootSuperBlock = NULL;
    destroyUnusedSuperBlocks();
}

void  NodeBuilder::updateHalfEdgeInfo(const HalfEdge& halfEdge)
{
    HalfEdgeInfo* info = reinterpret_cast<HalfEdgeInfo*>(halfEdge.data);

    info->direction = halfEdge.twin->vertex->pos - halfEdge.vertex->pos;

    info->length = info->direction.length();
    info->angle  = slopeToAngle(info->direction.x, info->direction.y);

    assert(info->length > 0);

    info->perpendicularDistance =  halfEdge.vertex->pos.y * info->direction.x - halfEdge.vertex->pos.x * info->direction.y;
    info->parallelDistance = -halfEdge.vertex->pos.x * info->direction.x - halfEdge.vertex->pos.y * info->direction.y;
}

void NodeBuilder::attachHalfEdgeInfo(HalfEdge& halfEdge, LineDef* line,
    LineDef* sourceLineDef, Sector* sec, bool back)
{
    halfEdgeInfo = reinterpret_cast<HalfEdgeInfo**>(std::realloc(halfEdgeInfo, ++numHalfEdgeInfo * sizeof(HalfEdgeInfo*)));

    HalfEdgeInfo* info = halfEdgeInfo[numHalfEdgeInfo-1] = reinterpret_cast<HalfEdgeInfo*>(std::calloc(1, sizeof(HalfEdgeInfo)));
    info->lineDef = line;
    info->back = back;
    info->sector = sec;
    info->sourceLineDef = sourceLineDef;

    halfEdge.data = info;
}

HalfEdge& NodeBuilder::createHalfEdge(LineDef* line, LineDef* sourceLine,
    Vertex* start, Sector* sec, bool back)
{
    HalfEdge& halfEdge = _map.halfEdgeDS().createHalfEdge(*start);
    attachHalfEdgeInfo(halfEdge, line, sourceLine, sec, back);
    return halfEdge;
}

HalfEdge& NodeBuilder::splitHalfEdge(HalfEdge& oldHEdge, ddouble x, ddouble y)
{
    HalfEdge& newHEdge = HEdge_Split(_map.halfEdgeDS(), oldHEdge);
    newHEdge.vertex->pos.x = x;
    newHEdge.vertex->pos.y = y;

    attachHalfEdgeInfo(newHEdge, ((HalfEdgeInfo*) oldHEdge.data)->lineDef, ((HalfEdgeInfo*) oldHEdge.data)->sourceLineDef, ((HalfEdgeInfo*) oldHEdge.data)->sector, ((HalfEdgeInfo*) oldHEdge.data)->back);
    attachHalfEdgeInfo(*newHEdge.twin, ((HalfEdgeInfo*) oldHEdge.twin->data)->lineDef, ((HalfEdgeInfo*) oldHEdge.twin->data)->sourceLineDef, ((HalfEdgeInfo*) oldHEdge.twin->data)->sector, ((HalfEdgeInfo*) oldHEdge.twin->data)->back);

    memcpy(newHEdge.data, oldHEdge.data, sizeof(HalfEdgeInfo));
    ((HalfEdgeInfo*) newHEdge.data)->superBlock = NULL;
    memcpy(newHEdge.twin->data, oldHEdge.twin->data, sizeof(HalfEdgeInfo));
    ((HalfEdgeInfo*) newHEdge.twin->data)->superBlock = NULL;

    // Update along-linedef relationships.
    if(((HalfEdgeInfo*) oldHEdge.data)->lineDef)
    {
        LineDef* lineDef = ((HalfEdgeInfo*) oldHEdge.data)->lineDef;
        if(((HalfEdgeInfo*) oldHEdge.data)->back)
        {
            if(lineDef->halfEdges[0] == oldHEdge.twin)
                lineDef->halfEdges[0] = newHEdge.twin;
        }
        else
        {
            if(lineDef->halfEdges[1] == &oldHEdge)
                lineDef->halfEdges[1] = &newHEdge;
        }
    }

    updateHalfEdgeInfo(oldHEdge);
    updateHalfEdgeInfo(*oldHEdge.twin);
    updateHalfEdgeInfo(newHEdge);
    updateHalfEdgeInfo(*newHEdge.twin);

    /*if(!oldHEdge.twin->face && ((HalfEdgeInfo*)oldHEdge.twin->data)->superBlock)
    {
        SuperBlock_IncHEdgeCounts(((HalfEdgeInfo*)oldHEdge.twin->data)->superBlock, ((HalfEdgeInfo*) newHEdge.twin->data)->lineDef != NULL);
        SuperBlock_PushHEdge(((HalfEdgeInfo*)oldHEdge.twin->data)->superBlock, newHEdge.twin);
        ((HalfEdgeInfo*) newHEdge.twin->data)->superBlock = ((HalfEdgeInfo*)oldHEdge.twin->data)->superBlock;
    }*/

    return newHEdge;
}

static __inline dint pointOnHEdgeSide(ddouble x, ddouble y, const HalfEdge* part)
{
    HalfEdgeInfo* data = (HalfEdgeInfo*) part->data;
    return P_PointOnLineSide2(x, y, data->direction.x, data->direction.y, data->perpendicularDistance,
                              data->length, DIST_EPSILON);
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

    FOR_EACH(i, map.lineDefs(), Map::LineDefs::const_iterator)
    {
        LineDef* n = (*i);

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

static void countVertexLineOwners(LineDefOwner* lineOwners, duint* oneSided, duint* twoSided)
{
    if(lineOwners)
    {
        LineDefOwner* owner;

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
 *      which are (LineDefOwner*) ptrs.
 */
static dint C_DECL lineAngleSorter2(const void* a, const void* b)
{
    dbinangle angles[2];
    LineDefOwner* own[2];
    own[0] = (LineDefOwner*) a;
    own[1] = (LineDefOwner*) b;
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
static LineDefOwner* mergeLineOwners2(LineDefOwner* left, LineDefOwner* right,
                                        dint (C_DECL *compare) (const void* a, const void* b))
{
    LineDefOwner tmp, *np;

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

static LineDefOwner* splitLineOwners2(LineDefOwner* list)
{
    LineDefOwner* lista, *listb, *listc;

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
static LineDefOwner* sortLineOwners2(LineDefOwner* list,
                                     dint (C_DECL *compare) (const void* a, const void* b))
{
    LineDefOwner* p;

    if(list && list->next)
    {
        p = splitLineOwners2(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners2(sortLineOwners2(list, compare),
                                sortLineOwners2(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner2(LineDefOwner** lineOwners, duint* numLineOwners,
                                LineDef* lineDef, dbyte vertex,
                                LineDefOwner** storage)
{
    LineDefOwner* newOwner;

    // Has this line already been registered with this vertex?
    if((*numLineOwners) != 0)
    {
        LineDefOwner* owner = (*lineOwners);

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

void NodeBuilder::createInitialHalfEdges()
{
typedef struct {
    duint numLineOwners;
    LineDefOwner* lineOwners; // Head of the lineowner list.
} ownerinfo_t;

    LineDefOwner* lineOwners, *storage;
    ownerinfo_t* vertexInfo;
    duint i, numVertices;

    numVertices = _map.halfEdgeDS().numVertices();
    vertexInfo = reinterpret_cast<ownerinfo_t*>(std::calloc(1, sizeof(*vertexInfo) * numVertices));

    // We know how many lineowners are needed: number of lineDefs * 2.
    lineOwners = storage = reinterpret_cast<LineDefOwner*>(std::calloc(1, sizeof(*lineOwners) * _map.numLineDefs() * 2));

    FOR_EACH(i, _map.lineDefs(), Map::LineDefs::const_iterator)
    {
        LineDef* lineDef = (*i);
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

    FOR_EACH(i, _map.lineDefs(), Map::LineDefs::const_iterator)
    {
        LineDef* lineDef = (*i);
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

    FOR_EACH(i, _map.lineDefs(), Map::LineDefs::const_iterator)
    {
        LineDef* lineDef = (*i);

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

        updateHalfEdgeInfo(front);
        updateHalfEdgeInfo(back);

        lineDef->halfEdges[0] = lineDef->halfEdges[1] = &front;
        ((LineDefOwner*) lineDef->vo[0])->halfEdge = &front;
        ((LineDefOwner*) lineDef->vo[1])->halfEdge = &back;
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
        LineDefOwner* owner, *prev;

        if(info->numLineOwners == 0)
            continue;

        prev = info->lineOwners;
        while(prev->next)
            prev = prev->next;

        owner = info->lineOwners;
        do
        {
            HalfEdge* halfEdge = owner->halfEdge;

            halfEdge->prev = owner->next ? owner->next->halfEdge->twin : info->lineOwners->halfEdge->twin;
            halfEdge->prev->next = halfEdge;

            halfEdge->twin->next = prev->halfEdge;
            halfEdge->twin->next->prev = halfEdge->twin;

            prev = owner;
        } while((owner = owner->next) != NULL);
    }

    // Release temporary storage.
    std::free(lineOwners);
    std::free(vertexInfo);
}

void NodeBuilder::createInitialHalfEdgesAndAddtoRootSuperBlock()
{
    createInitialHalfEdges();
    FOR_EACH(i, _map.halfEdgeDS().halfEdges(), HalfEdgeDS::HalfEdges::const_iterator)
    {
        HalfEdge* halfEdge = (*i);

        if(!(((HalfEdgeInfo*) halfEdge->data)->lineDef))
            continue;

        if(((HalfEdgeInfo*) halfEdge->data)->back &&
           !((HalfEdgeInfo*) halfEdge->data)->lineDef->buildData.sideDefs[LineDef::BACK])
            continue;

        if(((HalfEdgeInfo*) halfEdge->data)->back &&
           ((HalfEdgeInfo*) halfEdge->data)->lineDef->buildData.windowEffect)
            continue;

        addHalfEdgeToSuperBlock(_rootSuperBlock, halfEdge);
    }
}

void NodeBuilder::addHalfEdgeToSuperBlock(SuperBlock* superBlock, HalfEdge* halfEdge)
{
    assert(superBlock);
    assert(halfEdge);

#define SUPER_IS_LEAF(s)  \
    ((s)->bbox[SuperBlock::BOXRIGHT] - (s)->bbox[SuperBlock::BOXLEFT] <= 256 && \
     (s)->bbox[SuperBlock::BOXTOP] - (s)->bbox[SuperBlock::BOXBOTTOM] <= 256)

    for(;;)
    {
        dint p1, p2;
        HalfEdgeInfo* info = reinterpret_cast<HalfEdgeInfo*>(halfEdge->data);

        Vector2i midPoint = Vector2i(superBlock->bbox[SuperBlock::BOXLEFT] + superBlock->bbox[SuperBlock::BOXRIGHT],
                                     superBlock->bbox[SuperBlock::BOXBOTTOM] + superBlock->bbox[SuperBlock::BOXTOP]);
        midPoint.x /= 2;
        midPoint.y /= 2;

        // Update half-edge counts.
        if(info->lineDef)
            superBlock->realNum++;
        else
            superBlock->miniNum++;

        if(SUPER_IS_LEAF(superBlock))
        {   // Block is a leaf -- no subdivision possible.
            superBlock->push(halfEdge);
            reinterpret_cast<HalfEdgeInfo*>(halfEdge->data)->superBlock = superBlock;
            return;
        }

        if(superBlock->bbox[SuperBlock::BOXRIGHT] - superBlock->bbox[SuperBlock::BOXLEFT] >=
           superBlock->bbox[SuperBlock::BOXTOP]   - superBlock->bbox[SuperBlock::BOXBOTTOM])
        {   // Block is wider than it is high, or square.
            p1 = halfEdge->vertex->pos.x >= midPoint.x;
            p2 = halfEdge->twin->vertex->pos.x >= midPoint.x;
        }
        else
        {   // Block is higher than it is wide.
            p1 = halfEdge->vertex->pos.y >= midPoint.y;
            p2 = halfEdge->twin->vertex->pos.y >= midPoint.y;
        }

        bool isLeft;
        if(p1 && p2)
            isLeft = true;
        else if(!p1 && !p2)
            isLeft = false;
        else
        {   // Line crosses midpoint -- link it in and return.
            superBlock->push(halfEdge);
            reinterpret_cast<HalfEdgeInfo*>(halfEdge->data)->superBlock = superBlock;
            return;
        }

        // The seg lies in one half of this superBlock. Create the SuperBlock
        // if it doesn't already exist, and loop back to add the seg.
        if((!isLeft && !superBlock->rightChild)||
            (isLeft && !superBlock->leftChild))
        {
            SuperBlock* sub = new SuperBlock();

            sub->parent = superBlock;
            if(isLeft)
                superBlock->leftChild = sub;
            else
                superBlock->rightChild = sub;

            if(superBlock->bbox[SuperBlock::BOXRIGHT] - superBlock->bbox[SuperBlock::BOXLEFT] >=
               superBlock->bbox[SuperBlock::BOXTOP]   - superBlock->bbox[SuperBlock::BOXBOTTOM])
            {
                sub->bbox[SuperBlock::BOXLEFT] =
                    (isLeft? midPoint.x : superBlock->bbox[SuperBlock::BOXLEFT]);
                sub->bbox[SuperBlock::BOXBOTTOM] = superBlock->bbox[SuperBlock::BOXBOTTOM];

                sub->bbox[SuperBlock::BOXRIGHT] =
                    (isLeft? superBlock->bbox[SuperBlock::BOXRIGHT] : midPoint.x);
                sub->bbox[SuperBlock::BOXTOP] = superBlock->bbox[SuperBlock::BOXTOP];
            }
            else
            {
                sub->bbox[SuperBlock::BOXLEFT] = superBlock->bbox[SuperBlock::BOXLEFT];
                sub->bbox[SuperBlock::BOXBOTTOM] =
                    (isLeft? midPoint.y : superBlock->bbox[SuperBlock::BOXBOTTOM]);

                sub->bbox[SuperBlock::BOXRIGHT] = superBlock->bbox[SuperBlock::BOXRIGHT];
                sub->bbox[SuperBlock::BOXTOP] =
                    (isLeft? superBlock->bbox[SuperBlock::BOXTOP] : midPoint.y);
            }
        }

        superBlock = isLeft? superBlock->leftChild : superBlock->rightChild;
    }

#undef SUPER_IS_LEAF
}

static void sanityCheckClosed(const Face* face)
{
    dint total = 0, gaps = 0;

    const HalfEdge* halfEdge = face->halfEdge;
    do
    {
        const HalfEdge* a = halfEdge;
        const HalfEdge* b = halfEdge->next;

        if(a->twin->vertex->pos != b->vertex->pos)
            gaps++;

        total++;
    } while((halfEdge = halfEdge->next) != face->halfEdge);

    if(gaps > 0)
    {
        LOG_VERBOSE("HEdge list for face #%p is not closed (%d gaps, %d half-edges).")
            << face << gaps << total;

/*halfEdge = face->halfEdge;
do
{
    LOG_DEBUG("  half-edge %p (%s) --> (%s)") << halfEdge << halfEdge->vertex->pos << halfEdge->twin->vertex->pos;
} while((halfEdge = halfEdge->next) != face->halfEdge);*/
    }
}

static void sanityCheckSameSector(const Face* face)
{
    const HalfEdge* cur = NULL;
    const HalfEdgeInfo* data;

    {
    const HalfEdge* n;

    // Find a suitable half-edge for comparison.
    n = face->halfEdge;
    do
    {
        if(!((HalfEdgeInfo*) n->data)->sector)
            continue;

        cur = n;
        break;
    } while((n = n->next) != face->halfEdge);

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
    } while((cur = cur->next) != face->halfEdge);
}

static bool sanityCheckHasRealHEdge(const Face* face)
{
    const HalfEdge* n;

    n = face->halfEdge;
    do
    {
        if(((const HalfEdgeInfo*) n->data)->lineDef)
            return true;
    } while((n = n->next) != face->halfEdge);

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

void NodeBuilder::copyHalfEdgeListFromSuperBlock(Face& face, SuperBlock* superBlock)
{
    HalfEdge* halfEdge;
    while((halfEdge = superBlock->pop()))
    {
        ((HalfEdgeInfo*) halfEdge->data)->superBlock = NULL;
        // Link it into head of the leaf's list.
        face.linkHEdge(halfEdge);
    }

    // Recursively handle sub-blocks.
    if(superBlock->rightChild)
    {
        copyHalfEdgeListFromSuperBlock(face, superBlock->rightChild);

        if(superBlock->rightChild->realNum + superBlock->rightChild->miniNum > 0)
            LOG_ERROR("copyHalfEdgeListFromSuperBlock: Right child not empty!");

        destroySuperBlock(superBlock->rightChild);
        superBlock->rightChild = NULL;
    }
    if(superBlock->leftChild)
    {
        copyHalfEdgeListFromSuperBlock(face, superBlock->leftChild);

        if(superBlock->leftChild->realNum + superBlock->leftChild->miniNum > 0)
            LOG_ERROR("copyHalfEdgeListFromSuperBlock: Left child not empty!");

        destroySuperBlock(superBlock->leftChild);
        superBlock->leftChild = NULL;
    }

    superBlock->realNum = superBlock->miniNum = 0;
}

Face& NodeBuilder::createBSPLeaf(Face& face, SuperBlock* hEdgeList)
{
    copyHalfEdgeListFromSuperBlock(face, hEdgeList);
    return face;
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
    if(dY == 0 && data->direction.x == 0)
    {
        *iX = cur->vertex->pos.x;
        *iY = y;
        return;
    }

    // Vertical partition against horizontal half-edge.
    if(dX == 0 && data->direction.y == 0)
    {
        *iX = x;
        *iY = cur->vertex->pos.y;
        return;
    }

    // 0 = start, 1 = end.
    ds = perpC / (perpC - perpD);

    if(data->direction.x == 0)
        *iX = cur->vertex->pos.x;
    else
        *iX = cur->vertex->pos.x + (data->direction.x * ds);

    if(data->direction.y == 0)
        *iY = cur->vertex->pos.y;
    else
        *iY = cur->vertex->pos.y + (data->direction.y * ds);
}

void NodeBuilder::divideHalfEdge(const BSPartition& bsp, HalfEdge& curHEdge,
   SuperBlock* bRight, SuperBlock* bLeft)
{
#define INTERSECT(e, p) if(!findIntersection(e)) \
    insertIntersection(e, M_ParallelDist((p).direction.x, (p).direction.y, (p).para, (p).length, (e)->vertex->pos.x, (e)->vertex->pos.y))

    HalfEdgeInfo* data = ((HalfEdgeInfo*) curHEdge.data);
    ddouble perpC, perpD;

    // Get state of lines' relation to each other.
    if(data->sourceLineDef == bsp.sourceLineDef)
    {
        perpC = perpD = 0;
    }
    else
    {
        perpC = M_PerpDist(bsp.direction.x, bsp.direction.y, bsp.perp, bsp.length,
                       curHEdge.vertex->pos.x, curHEdge.vertex->pos.y);
        perpD = M_PerpDist(bsp.direction.x, bsp.direction.y, bsp.perp, bsp.length,
                       curHEdge.twin->vertex->pos.x, curHEdge.twin->vertex->pos.y);
    }

    // Check for being on the same line.
    if(fabs(perpC) <= DIST_EPSILON && fabs(perpD) <= DIST_EPSILON)
    {
        INTERSECT(&curHEdge, bsp);
        INTERSECT(curHEdge.twin, bsp);

        // This seg runs along the same line as the  Check
        // whether it goes in the same direction or the opposite.
        if(data->direction.x * bsp.direction.x + data->direction.y * bsp.direction.y < 0)
        {
            addHalfEdgeToSuperBlock(bLeft, &curHEdge);
        }
        else
        {
            addHalfEdgeToSuperBlock(bRight, &curHEdge);
        }

        return;
    }

    // Check for right side.
    if(perpC > -DIST_EPSILON && perpD > -DIST_EPSILON)
    {
        if(perpC < DIST_EPSILON)
            INTERSECT(&curHEdge, bsp);
        else if(perpD < DIST_EPSILON)
            INTERSECT(curHEdge.twin, bsp);

        addHalfEdgeToSuperBlock(bRight, &curHEdge);
        return;
    }

    // Check for left side.
    if(perpC < DIST_EPSILON && perpD < DIST_EPSILON)
    {
        if(perpC > -DIST_EPSILON)
            INTERSECT(&curHEdge, bsp);
        else if(perpD > -DIST_EPSILON)
            INTERSECT(curHEdge.twin, bsp);

        addHalfEdgeToSuperBlock(bLeft, &curHEdge);
        return;
    }

    // When we reach here, we have perpC and perpD non-zero and opposite sign,
    // hence this edge will be split by the partition line.

    ddouble iX, iY;
    calcIntersection(&curHEdge, bsp.point.x, bsp.point.y, bsp.direction.x, bsp.direction.y, perpC, perpD, &iX, &iY);
    HalfEdge& newHEdge = splitHalfEdge(curHEdge, iX, iY);
    INTERSECT(&newHEdge, bsp);

    if(perpC < 0)
    {
        addHalfEdgeToSuperBlock(bLeft,  &curHEdge);
        addHalfEdgeToSuperBlock(bRight, &newHEdge);
    }
    else
    {
        addHalfEdgeToSuperBlock(bRight, &curHEdge);
        addHalfEdgeToSuperBlock(bLeft,  &newHEdge);
    }

    if(!curHEdge.twin->face && ((HalfEdgeInfo*)curHEdge.twin->data)->superBlock)
        addHalfEdgeToSuperBlock(((HalfEdgeInfo*)curHEdge.twin->data)->superBlock, newHEdge.twin);

#undef INTERSECT
}

void NodeBuilder::divideHalfEdges(const BSPartition& partition, SuperBlock* hEdgeList,
    SuperBlock* rights, SuperBlock* lefts)
{
    HalfEdge* halfEdge;
    while((halfEdge = hEdgeList->pop()))
    {
        ((HalfEdgeInfo*) halfEdge->data)->superBlock = NULL;
        divideHalfEdge(partition, *halfEdge, rights, lefts);
    }

    // Recursively handle sub-blocks.
    if(hEdgeList->rightChild)
    {
        divideHalfEdges(partition, hEdgeList->rightChild, rights, lefts);

        if(hEdgeList->rightChild->realNum + hEdgeList->rightChild->miniNum > 0)
            LOG_ERROR("NodeBuilder::divideHalfEdges: Right child not empty!");

        destroySuperBlock(hEdgeList->rightChild);
        hEdgeList->rightChild = NULL;
    }
    if(hEdgeList->leftChild)
    {
        divideHalfEdges(partition, hEdgeList->leftChild, rights, lefts);

        if(hEdgeList->leftChild->realNum + hEdgeList->leftChild->miniNum > 0)
            LOG_ERROR("NodeBuilder::divideHalfEdges: Left child not empty!");

        destroySuperBlock(hEdgeList->leftChild);
        hEdgeList->leftChild = NULL;
    }

    hEdgeList->realNum = hEdgeList->miniNum = 0;
}

void NodeBuilder::createHalfEdgesAlongPartition(const BSPartition& partition,
    SuperBlock* bRight, SuperBlock* bLeft)
{
    mergeIntersectionOverlaps();
    connectIntersectionGaps(partition, bRight, bLeft);
}

void NodeBuilder::partitionHalfEdges(const BSPartition& partition,
    SuperBlock* hEdgeList, SuperBlock** right, SuperBlock** left)
{
    SuperBlock* bRight = createSuperBlock();
    SuperBlock* bLeft = createSuperBlock();

    M_CopyBox(bRight->bbox, hEdgeList->bbox);
    M_CopyBox(bLeft->bbox, hEdgeList->bbox);

    divideHalfEdges(partition, hEdgeList, bRight, bLeft);

    // Sanity checks...
    if(bRight->realNum + bRight->miniNum == 0)
        LOG_ERROR("NodeBuilder::partitionHalfEdges: Separated halfedge-list has no right side.");

    if(bLeft->realNum + bLeft->miniNum == 0)
        LOG_ERROR("NodeBuilder::partitionHalfEdges: Separated halfedge-list has no left side.");

    createHalfEdgesAlongPartition(partition, bRight, bLeft);

    *right = bRight;
    *left = bLeft;
}

BinaryTree<void*>* NodeBuilder::buildNodes(SuperBlock* hEdgeList)
{
    // Pick half-edge with which to derive the next partition.
    BSPartition partition;
    if(!pickPartition(hEdgeList, partition))
    {   // No partition required, already convex.
        return new BinaryTree<void*>(reinterpret_cast<void*>(&createBSPLeaf(_map.halfEdgeDS().createFace(), hEdgeList)));
    }

/*#if _DEBUG
LOG_MESSAGE("NodeBuilder::buildNodes: Partition P %s D %s.")
    << partition.point << partition.direction;
#endif*/

    SuperBlock* rightHEdges, *leftHEdges;
    partitionHalfEdges(partition, hEdgeList, &rightHEdges, &leftHEdges);

    BinaryTree<void*>* tree = new BinaryTree<void*>(reinterpret_cast<void*>(
        _map.createNode(partition, rightHEdges->aaBounds(), leftHEdges->aaBounds())));

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
    createInitialHalfEdgesAndAddtoRootSuperBlock();

    rootNode = buildNodes(_rootSuperBlock);
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

void NodeBuilder::insertIntersection(HalfEdge* halfEdge, ddouble distance)
{
    /// Find the insertion point in the list: sorted by distance near to far.
    Intersections::iterator i = _intersections.end();
    while(i != _intersections.begin() && !(distance < (*i).distance)) --i;
    _intersections.insert(i, Intersection(halfEdge, distance));
}

bool NodeBuilder::findIntersection(const HalfEdge* halfEdge) const
{
    if(!halfEdge)
        return false;
    for(Intersections::const_iterator i = _intersections.begin();
        i != _intersections.end(); ++i)
        if((*i).halfEdge == halfEdge)
            return true;
    return false;
}

void NodeBuilder::mergeIntersectionOverlaps()
{
    if(!(_intersections.size() > 1))
        return;

    Intersections::iterator i = _intersections.begin();
    for(;;)
    {
        Intersections::iterator prev = i;
        Intersections::iterator cur = ++i;

        if(cur == _intersections.end())
            return;

        ddouble len = cur->distance - prev->distance;
        if(len < -0.1)
            LOG_ERROR("CutList::mergeOverlap: Bad order %1.3f > %1.3f")
                << dfloat(prev->distance) << dfloat(cur->distance);
        if(len > 0.2)
            continue;
/*      if(len > DIST_EPSILON)
        {
            LOG_MESSAGE("Skipping very short half-edge (len=%1.3f) near %s") << len << prev->vertex.pos;
            continue;
        }*/

        // Merge by simply removing previous from the list.
        _intersections.erase(prev);
    }
}

/**
 * Look for the first HalfEdge whose angle is past that required.
 */
static HalfEdge* vertexCheckOpen(Vertex* vertex, angle_g angle, dbyte antiClockwise)
{
    HalfEdge* halfEdge, *first;

    first = vertex->halfEdge;
    halfEdge = first->twin->next;
    do
    {
        if(((HalfEdgeInfo*) halfEdge->data)->angle <=
           ((HalfEdgeInfo*) first->data)->angle)
            first = halfEdge;
    } while((halfEdge = halfEdge->twin->next) != vertex->halfEdge);

    if(antiClockwise)
    {
        first = first->twin->next;
        halfEdge = first;
        while(((HalfEdgeInfo*) halfEdge->data)->angle > angle + ANG_EPSILON &&
              (halfEdge = halfEdge->twin->next) != first);

        return halfEdge->twin;
    }
    else
    {
        halfEdge = first;
        while(((HalfEdgeInfo*) halfEdge->data)->angle < angle + ANG_EPSILON &&
              (halfEdge = halfEdge->prev->twin) != first);

        return halfEdge;
    }
}

static bool isIntersectionOnSelfRefLineDef(const HalfEdge* halfEdge)
{
    /*if(halfEdge && ((HalfEdgeInfo*) halfEdge->data)->lineDef)
    {
        LineDef* lineDef = ((HalfEdgeInfo*) halfEdge->data)->lineDef;

        if(lineDef->buildData.sideDefs[FRONT] &&
           lineDef->buildData.sideDefs[BACK] &&
           lineDef->buildData.sideDefs[FRONT]->sector ==
           lineDef->buildData.sideDefs[BACK]->sector)
            return true;
    }*/
    return false;
}

void NodeBuilder::connectIntersectionGaps(const BSPartition& partition,
    SuperBlock* rightList, SuperBlock* leftList)
{
    Intersections::iterator i = _intersections.begin();
    for(;;)
    {
        Intersections::iterator prev = i;
        Intersections::iterator cur = ++i;

        if(prev == _intersections.end())
            return;

        // Is this half-edge exactly aligned to the partition?
        bool alongPartition = false;
        {
        angle_g angle = slopeToAngle(-partition.direction.x, -partition.direction.y);
        HalfEdge* halfEdge = prev->halfEdge;
        do
        {
            angle_g diff = fabs(((HalfEdgeInfo*) halfEdge->data)->angle - angle);
            if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
            {
                alongPartition = true;
                break;
            }
        } while((halfEdge = halfEdge->prev->twin) != cur->halfEdge);
        }

        HalfEdge* farHalfEdge, *nearHalfEdge;
        Sector* nearSector = NULL, *farSector = NULL;
        if(!alongPartition)
        {
            farHalfEdge = vertexCheckOpen(cur->halfEdge->vertex, slopeToAngle(-partition.direction.x, -partition.direction.y), false);
            nearHalfEdge = vertexCheckOpen(prev->halfEdge->vertex, slopeToAngle(partition.direction.x, partition.direction.y), true);

            nearSector = nearHalfEdge ? ((HalfEdgeInfo*) nearHalfEdge->data)->sector : NULL;
            farSector = farHalfEdge? ((HalfEdgeInfo*) farHalfEdge->data)->sector : NULL;
        }

        if(!(!nearSector && !farSector))
        {
            // Check for some nasty open/closed or close/open cases.
            if(nearSector && !farSector)
            {
                if(!isIntersectionOnSelfRefLineDef(prev->halfEdge))
                {
                    nearSector->flags |= Sector::UNCLOSED;

                    Vector2d pos = prev->halfEdge->vertex->pos + cur->halfEdge->vertex->pos;
                    pos.x /= 2;
                    pos.y /= 2;

                    LOG_VERBOSE("Warning: Unclosed sector #%d near %s")
                        << nearSector->buildData.index - 1 << pos;
                }
            }
            else if(!nearSector && farSector)
            {
                if(!isIntersectionOnSelfRefLineDef(cur->halfEdge))
                {
                    farSector->flags |= Sector::UNCLOSED;

                    Vector2d pos = prev->halfEdge->vertex->pos + cur->halfEdge->vertex->pos;
                    pos.x /= 2;
                    pos.y /= 2;

                    LOG_VERBOSE("Warning: Unclosed s    ector #%d near %s")
                        << (farSector->buildData.index - 1) << pos;
                }
            }
            else
            {
                /**
                 * This is definitetly open space. Build half-edges on each
                 * side of the gap.
                 */

                if(nearSector != farSector)
                {
                    if(!isIntersectionOnSelfRefLineDef(prev->halfEdge) &&
                       !isIntersectionOnSelfRefLineDef(cur->halfEdge))
                    {
                        LOG_DEBUG("Sector mismatch: #%d %s != #%d %s")
                            << (nearSector->buildData.index - 1) << prev->halfEdge->vertex->pos
                            << (farSector->buildData.index - 1) << cur->halfEdge->vertex->pos;
                    }

                    // Choose the non-self-referencing sector when we can.
                    if(isIntersectionOnSelfRefLineDef(prev->halfEdge) &&
                       !isIntersectionOnSelfRefLineDef(cur->halfEdge))
                    {
                        nearSector = farSector;
                    }
                }

                {
                HalfEdge& right = createHalfEdge(NULL, partition.lineDef, prev->halfEdge->vertex, ((HalfEdgeInfo*) nearHalfEdge->data)->sector, ((HalfEdgeInfo*) nearHalfEdge->data)->back);
                HalfEdge& left = createHalfEdge(NULL, partition.lineDef, cur->halfEdge->vertex, ((HalfEdgeInfo*) farHalfEdge->prev->data)->sector, ((HalfEdgeInfo*) farHalfEdge->prev->data)->back);

                // Twin the half-edges together.
                right.twin = &left;
                left.twin = &right;

                left.prev = farHalfEdge->prev;
                right.prev = nearHalfEdge;

                right.next = farHalfEdge;
                left.next = nearHalfEdge->next;

                left.prev->next = &left;
                right.prev->next = &right;

                right.next->prev = &right;
                left.next->prev = &left;

#if _DEBUG
testVertexHEdgeRings(prev->halfEdge->vertex);
testVertexHEdgeRings(cur->halfEdge->vertex);
#endif

                updateHalfEdgeInfo(right);
                updateHalfEdgeInfo(left);

                // Add the new half-edges to the appropriate lists.

                /*if(nearHalfEdge->face)
                    Face_LinkHEdge(nearHalfEdge->face, &right);
                else*/
                    addHalfEdgeToSuperBlock(rightList, &right);
                /*if(farHalfEdge->prev->face)
                    Face_LinkHEdge(farHalfEdge->prev->face, &left);
                else*/
                    addHalfEdgeToSuperBlock(leftList, &left);

                LOG_DEBUG(" Capped intersection:");
                LOG_DEBUG("  %p RIGHT  sector %d %s -> %s")
                    << &right << (reinterpret_cast<HalfEdgeInfo*>(nearHalfEdge->data)->sector? reinterpret_cast<HalfEdgeInfo*>(nearHalfEdge->data)->sector->buildData.index : -1)
                    << right.vertex->pos << right.twin->vertex->pos;
                LOG_DEBUG("  %p LEFT sector %d %s -> %s")
                    << &left << (reinterpret_cast<HalfEdgeInfo*>(farHalfEdge->prev->data)->sector? reinterpret_cast<HalfEdgeInfo*>(farHalfEdge->prev->data)->sector->buildData.index : -1)
                    << left.vertex->pos << left.twin->vertex->pos;
                }
            }
        }
    }
}
