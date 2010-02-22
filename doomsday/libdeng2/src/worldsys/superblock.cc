/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
 * Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include "de/Log"
#include "de/SuperBlock"
#include "de/NodeBuilder"
#include "de/Sector"
#include "de/LineDef"
#include "de/HalfEdgeInfo"

using namespace de;

SuperBlock::SuperBlock()
{}

SuperBlock::~SuperBlock()
{
    halfEdges.clear();
    // Recursively handle sub-blocks.
    if(rightChild)
        delete rightChild;
    if(leftChild)
        delete leftChild;
}

void SuperBlock::push(HalfEdge* halfEdge)
{
    assert(halfEdge);
#if _DEBUG
// Ensure halfEdge is not already in this SuperBlock. *Should* be impossible.
FOR_EACH(i, halfEdges, HalfEdges::iterator)
    assert(*i != halfEdge);
#endif
    halfEdges.push_back(halfEdge);
}

HalfEdge* SuperBlock::pop()
{
    if(!halfEdges.empty())
    {
        HalfEdge* halfEdge = halfEdges.back();
        halfEdges.pop_back();
        return halfEdge;
    }
    return NULL;
}

void SuperBlock::incHalfEdgeCounts(bool lineLinked)
{
    if(lineLinked)
        realNum++;
    else
        miniNum++;
    if(parent)
        parent->incHalfEdgeCounts(lineLinked);
}

static void findBoundsWorker(const SuperBlock* block, dfloat* bbox)
{
    FOR_EACH(i, block->halfEdges, SuperBlock::HalfEdges::const_iterator)
    {
        const HalfEdge* cur = (*i);
        const Vector2f l = cur->vertex->pos.min(cur->twin->vertex->pos);
        const Vector2f h = cur->vertex->pos.max(cur->twin->vertex->pos);

        if(l.x < bbox[SuperBlock::BOXLEFT])
            bbox[SuperBlock::BOXLEFT] = l.x;
        else if(l.x > bbox[SuperBlock::BOXRIGHT])
            bbox[SuperBlock::BOXRIGHT] = l.x;
        if(l.y < bbox[SuperBlock::BOXBOTTOM])
            bbox[SuperBlock::BOXBOTTOM] = l.y;
        else if(l.y > bbox[SuperBlock::BOXTOP])
            bbox[SuperBlock::BOXTOP] = l.y;

        if(h.x < bbox[SuperBlock::BOXLEFT])
            bbox[SuperBlock::BOXLEFT] = h.x;
        else if(h.x > bbox[SuperBlock::BOXRIGHT])
            bbox[SuperBlock::BOXRIGHT] = h.x;
        if(h.y < bbox[SuperBlock::BOXBOTTOM])
            bbox[SuperBlock::BOXBOTTOM] = h.y;
        else if(h.y > bbox[SuperBlock::BOXTOP])
            bbox[SuperBlock::BOXTOP] = h.y;
    }

    // Recursively handle sub-blocks.
    if(block->rightChild)
        findBoundsWorker(block->rightChild, bbox);
    if(block->leftChild)
        findBoundsWorker(block->leftChild, bbox);
}

MapRectanglef SuperBlock::aaBounds() const
{
    dfloat bbox[4];
    bbox[BOXTOP] = bbox[BOXRIGHT] = MINFLOAT;
    bbox[BOXBOTTOM] = bbox[BOXLEFT] = MAXFLOAT;
    findBoundsWorker(this, bbox);
    return MapRectangle(bbox);
}

#if _DEBUG
void SuperBlock::print() const
{
    FOR_EACH(i, halfEdges, HalfEdges::const_iterator)
    {
        const HalfEdge* halfEdge = (*i);
        const HalfEdgeInfo* data = reinterpret_cast<HalfEdgeInfo*>(halfEdge->data);

        LOG_MESSAGE("Build: %s %p sector=%d %s -> %s")
            << (data->lineDef? "NORM" : "MINI") << halfEdge << data->sector->buildData.index
            << halfEdge->vertex->pos << halfEdge->twin->vertex->pos;
    }

    if(rightChild)
        rightChild->print();
    if(leftChild)
        leftChild->print();
}
#endif
