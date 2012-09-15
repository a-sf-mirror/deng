/**
 * @file blockmap.cpp
 * Blockmap. @ingroup map
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

#include <cmath>

#include "de_base.h"
#include "m_vector.h"
#include "gridmap.h"

#include <de/Error>
#include <de/LegacyCore>
#include <de/Log>
#include <de/String>

#include "blockmap.h"

struct BlockmapRingNode
{
    void* object;
    BlockmapRingNode* prev;
    BlockmapRingNode* next;
};

/// @todo optimize: BlockmapRingNodes should be pooled!!
struct BlockmapCellData
{
    BlockmapCellData() : ringNodes(0), objectCount(0) {}

    uint size() const { return objectCount; }

    /**
     * Lookup an object in this cell by memory address.
     */
    BlockmapRingNode* node(void* object)
    {
        if(object)
        {
            for(BlockmapRingNode* node = ringNodes; node; node = node->next)
            {
                if(node->object == object) return node;
            }
        }
        return 0; // Not found.
    }

    BlockmapCellData& unlinkObject(void* object, bool* unlinked = 0)
    {
        if(unlinkObjectFromRing(object))
        {
            // There is now one fewer object in the cell.
            objectCount--;
            if(unlinked) *unlinked = true;
        }
        else
        {
            if(unlinked) *unlinked = false;
        }
        return *this;
    }

    BlockmapCellData& unlinkAllObjects()
    {
        for(BlockmapRingNode* node = ringNodes; node; node = node->next)
        {
            if(node->object)
            {
                // Unlink from the ring (the node will be reused).
                node->object = NULL;
            }
        }
        objectCount = 0;
        return *this;
    }

    BlockmapCellData& linkObject(void* object)
    {
        if(linkObjectToRing(object))
        {
            // There is now one more object in the cell.
            objectCount++;
        }
        return *this;
    }

    BlockmapRingNode* objects() { return ringNodes; }

private:
    bool linkObjectToRing(void* object)
    {
        if(!object) return false;

        BlockmapRingNode* node;

        if(!ringNodes)
        {
            // Create a new root node.
            node = (BlockmapRingNode*)Z_Malloc(sizeof(*node), PU_MAP, 0);
            node->next = NULL;
            node->prev = NULL;
            node->object = object;
            ringNodes = node;
            return true;
        }

        // Is there an available node in the ring we can reuse?
        for(node = ringNodes; node->next && node->object; node = node->next)
        {}

        if(!node->object)
        {
            // This will do nicely.
            node->object = object;
            return true;
        }

        // Add a new node to the ring.
        node->next = (BlockmapRingNode*)Z_Malloc(sizeof(*node), PU_MAP, 0);
        node->next->next = NULL;
        node->next->prev = node;
        node->next->object = object;
        return true;
    }

    /**
     * Unlink the given object from the specified cell ring (if indeed linked).
     *
     * @param object  Object to be unlinked.
     * @return  @c true iff the object was linked to the ring and was unlinked.
     */
    bool unlinkObjectFromRing(void* object)
    {
        BlockmapRingNode* found = node(object);
        if(!found) return false; // Object was not linked.

        // Unlink from the ring (the node will be reused).
        found->object = NULL;
        return true; // Object was unlinked.
    }

private:
    BlockmapRingNode* ringNodes;

    /// Running total of the number of objects linked in this cell.
    uint objectCount;
};

static int BlockmapCellData_IterateObjects(BlockmapCellData* cell,
    int (*callback) (void* object, void* parameters), void* parameters = 0)
{
    DENG2_ASSERT(cell);
    BlockmapRingNode* link = cell->objects();
    while(link)
    {
        BlockmapRingNode* next = link->next;

        if(link->object)
        {
            int result = callback(link->object, parameters);
            if(result) return result; // Stop iteration.
        }

        link = next;
    }
    return false; // Continue iteration.
}

struct de::Blockmap::Instance
{
    /// Minimal and Maximal points in map space coordinates.
    AABoxd bounds;

    /// Cell dimensions in map space coordinates.
    vec2d_t cellSize;

    /// Gridmap which implements the blockmap itself.
    Gridmap gridmap;

    Instance(coord_t const min[2], coord_t const max[2], BlockmapCoord cellWidth, BlockmapCoord cellHeight)
        : bounds(min, max),
          gridmap(BlockmapCoord( ceil((max[0] - min[0]) / coord_t(cellWidth)) ),
                  BlockmapCoord( ceil((max[1] - min[1]) / coord_t(cellHeight))))
    {
        cellSize[VX] = cellWidth;
        cellSize[VY] = cellHeight;
    }
};

de::Blockmap::Blockmap(coord_t const min[2], coord_t const max[2], BlockmapCoord cellWidth, BlockmapCoord cellHeight)
{
    d = new Instance(min, max, cellWidth, cellHeight);
}

de::Blockmap::~Blockmap()
{
    delete d;
}

const pvec2d_t de::Blockmap::origin() const
{
    return d->bounds.min;
}

const AABoxd& de::Blockmap::bounds() const
{
    return d->bounds;
}

BlockmapCoord de::Blockmap::width() const
{
    return d->gridmap.width();
}

BlockmapCoord de::Blockmap::height() const
{
    return d->gridmap.height();
}

BlockmapCell const& de::Blockmap::widthHeight() const
{
    return d->gridmap.widthHeight();
}

coord_t de::Blockmap::cellWidth() const
{
    return d->cellSize[VX];
}

coord_t de::Blockmap::cellHeight() const
{
    return d->cellSize[VY];
}

const pvec2d_t de::Blockmap::cellSize() const
{
    return d->cellSize;
}

BlockmapCoord de::Blockmap::cellX(coord_t x) const
{
    uint result;
    clipCellX(&result, x);
    return result;
}

BlockmapCoord de::Blockmap::cellY(coord_t y) const
{
    uint result;
    clipCellY(&result, y);
    return result;
}

bool de::Blockmap::clipCellX(BlockmapCoord* outX, coord_t x) const
{
    bool adjusted = false;
    if(x < d->bounds.minX)
    {
        x = d->bounds.minX;
        adjusted = true;
    }
    else if(x >= d->bounds.maxX)
    {
        x = d->bounds.maxX - 1;
        adjusted = true;
    }
    if(outX)
    {
        *outX = uint((x - d->bounds.minX) / d->cellSize[VX]);
    }
    return adjusted;
}

bool de::Blockmap::clipCellY(BlockmapCoord* outY, coord_t y) const
{
    bool adjusted = false;
    if(y < d->bounds.minY)
    {
        y = d->bounds.minY;
        adjusted = true;
    }
    else if(y >= d->bounds.maxY)
    {
        y = d->bounds.maxY - 1;
        adjusted = true;
    }
    if(outY)
    {
        *outY = uint((y - d->bounds.minY) / d->cellSize[VY]);
    }
    return adjusted;
}

bool de::Blockmap::cell(BlockmapCell mcell, coord_t const pos[2]) const
{
    if(mcell && pos)
    {
        // Deliberate bitwise OR - we need to clip both X and Y.
        return clipCellX(&mcell[0], pos[VX]) |
               clipCellY(&mcell[1], pos[VY]);
    }
    return false;
}

bool de::Blockmap::cellBlock(BlockmapCellBlock& cellBlock, AABoxd const& box) const
{
    // Deliberate bitwise OR - we need to clip both Min and Max.
    return cell(cellBlock.min, box.min) |
           cell(cellBlock.max, box.max);
}

bool de::Blockmap::createCellAndLinkObject(const_BlockmapCell mcell_, void* object)
{
    DENG2_ASSERT(object);
    BlockmapCell mcell = { mcell_[0], mcell_[1] };
    d->gridmap.clipCell(mcell);
    BlockmapCellData* cell;
    if(d->gridmap.leafAtCell(mcell))
    {
        cell = reinterpret_cast<BlockmapCellData*>(d->gridmap.cell(mcell));
        DENG2_ASSERT(cell);
    }
    else
    {
        cell = (BlockmapCellData*) Z_Calloc(sizeof(BlockmapCellData), PU_MAPSTATIC, 0);
        d->gridmap.setCell(mcell, cell);
    }
    cell->linkObject(object);
    return true; // Link added.
}

void de::Blockmap::unlinkAllObjectsInCell(const_BlockmapCell mcell)
{
    if(!d->gridmap.leafAtCell(mcell)) return;
    BlockmapCellData* cell = reinterpret_cast<BlockmapCellData*>(d->gridmap.cell(mcell));
    DENG2_ASSERT(cell);
    cell->unlinkAllObjects();
}

bool de::Blockmap::unlinkObjectInCell(const_BlockmapCell mcell, void* object)
{
    bool unlinked = false;
    if(d->gridmap.leafAtCell(mcell))
    {
        BlockmapCellData* cell = reinterpret_cast<BlockmapCellData*>(d->gridmap.cell(mcell));
        DENG2_ASSERT(cell);
        cell->unlinkObject(object, &unlinked);
    }
    return unlinked;
}

static int unlinkObjectInCellWorker(void* ptr, void* parameters)
{
    BlockmapCellData* cell = reinterpret_cast<BlockmapCellData*>(ptr);
    cell->unlinkObject(parameters/*object ptr*/);
    return false; // Continue iteration.
}

void de::Blockmap::unlinkObjectInCellBlock(BlockmapCellBlock const& cellBlock, void* object)
{
    d->gridmap.blockIterate(cellBlock, unlinkObjectInCellWorker, object);
}

static int unlinkAllObjectsInCellWorker(void* ptr, void* /*parameters*/)
{
    BlockmapCellData* cell = reinterpret_cast<BlockmapCellData*>(ptr);
    cell->unlinkAllObjects();
    return false; // Continue iteration.
}

void de::Blockmap::unlinkAllObjectsInCellBlock(BlockmapCellBlock const& cellBlock)
{
    d->gridmap.blockIterate(cellBlock, unlinkAllObjectsInCellWorker);
}

uint de::Blockmap::cellObjectCount(const_BlockmapCell mcell) const
{
    if(!d->gridmap.leafAtCell(mcell)) return 0;
    BlockmapCellData* cell = reinterpret_cast<BlockmapCellData*>(d->gridmap.cell(mcell));
    DENG2_ASSERT(cell);
    return cell->size();
}

Gridmap const& de::Blockmap::gridmap()
{
    return d->gridmap;
}

int de::Blockmap::iterateCellObjects(const_BlockmapCell mcell,
    int (*callback) (void* object, void* parameters), void* parameters)
{
    if(!d->gridmap.leafAtCell(mcell)) return false; // Continue iteration.
    BlockmapCellData* cell = reinterpret_cast<BlockmapCellData*>(d->gridmap.cell(mcell));
    DENG2_ASSERT(cell);
    return BlockmapCellData_IterateObjects(cell, callback, parameters);
}

typedef struct {
    int (*callback)(void* userData, void* parameters);
    void* parameters;
} cellobjectiterator_params_t;

static int cellObjectIterator(void* userData, void* parameters)
{
    BlockmapCellData* cell = reinterpret_cast<BlockmapCellData*>(userData);
    cellobjectiterator_params_t* args = static_cast<cellobjectiterator_params_t*>(parameters);
    DENG2_ASSERT(args);

    return BlockmapCellData_IterateObjects(cell, args->callback, args->parameters);
}

int de::Blockmap::iterateCellBlockObjects(BlockmapCellBlock const& cellBlock,
    int (*callback) (void* object, void* parameters), void* parameters)
{
    cellobjectiterator_params_t args;
    args.callback   = callback;
    args.parameters = parameters;
    return d->gridmap.blockIterate(cellBlock, cellObjectIterator, (void*)&args);
}

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::Blockmap*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<const de::Blockmap*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::Blockmap* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    const de::Blockmap* self = TOINTERNAL_CONST(inst)

Blockmap* Blockmap_New(coord_t const min[2], coord_t const max[2], BlockmapCoord cellWidth, BlockmapCoord cellHeight)
{
    return reinterpret_cast<Blockmap*>(new de::Blockmap(min, max,cellWidth, cellHeight));
}

void Blockmap_Delete(Blockmap* bm)
{
    if(bm)
    {
        SELF(bm);
        delete self;
    }
}

BlockmapCoord Blockmap_CellX(Blockmap const* bm, coord_t x)
{
    SELF_CONST(bm);
    return self->cellX(x);
}

BlockmapCoord Blockmap_CellY(Blockmap const* bm, coord_t y)
{
    SELF_CONST(bm);
    return self->cellY(y);
}

boolean Blockmap_ClipCellX(Blockmap const* bm, BlockmapCoord* outX, coord_t x)
{
    SELF_CONST(bm);
    return CPP_BOOL(self->clipCellX(outX, x));
}

boolean Blockmap_ClipCellY(Blockmap const* bm, BlockmapCoord* outY, coord_t y)
{
    SELF_CONST(bm);
    return CPP_BOOL(self->clipCellY(outY, y));
}

boolean Blockmap_Cell(Blockmap const* bm, BlockmapCell mcell, coord_t const pos[2])
{
    SELF_CONST(bm);
    return CPP_BOOL(self->cell(mcell, pos));
}

boolean Blockmap_CellBlock(Blockmap const* bm, BlockmapCellBlock* cellBlock, const AABoxd* box)
{
    SELF_CONST(bm);
    DENG2_ASSERT(cellBlock && box);
    return CPP_BOOL(self->cellBlock(*cellBlock, *box));
}

const pvec2d_t Blockmap_Origin(Blockmap const* bm)
{
    SELF_CONST(bm);
    return self->origin();
}

const AABoxd* Blockmap_Bounds(Blockmap const* bm)
{
    SELF_CONST(bm);
    return &self->bounds();
}

BlockmapCoord Blockmap_Width(Blockmap const* bm)
{
    SELF_CONST(bm);
    return self->width();
}

BlockmapCoord Blockmap_Height(Blockmap const* bm)
{
    SELF_CONST(bm);
    return self->height();
}

void Blockmap_WidthHeight(Blockmap const* bm, BlockmapCell v)
{
    SELF_CONST(bm);
    DENG2_ASSERT(v);
    v[0] = self->width();
    v[1] = self->height();
}

coord_t Blockmap_CellWidth(Blockmap const* bm)
{
    SELF_CONST(bm);
    return self->cellWidth();
}

coord_t Blockmap_CellHeight(Blockmap const* bm)
{
    SELF_CONST(bm);
    return self->cellHeight();
}

const pvec2d_t Blockmap_CellSize(Blockmap const* bm)
{
    SELF_CONST(bm);
    return self->cellSize();
}

boolean Blockmap_CreateCellAndLinkObjectXY(Blockmap* bm, BlockmapCoord x, BlockmapCoord y, void* object)
{
    SELF(bm);
    return CPP_BOOL(self->createCellAndLinkObject(x, y, object));
}

boolean Blockmap_CreateCellAndLinkObject(Blockmap* bm, const_BlockmapCell mcell, void* object)
{
    SELF(bm);
    return CPP_BOOL(self->createCellAndLinkObject(mcell, object));
}

boolean Blockmap_UnlinkObjectInCell(Blockmap* bm, const_BlockmapCell mcell, void* object)
{
    SELF(bm);
    return CPP_BOOL(self->unlinkObjectInCell(mcell, object));
}

boolean Blockmap_UnlinkObjectInCellXY(Blockmap* bm, BlockmapCoord x, BlockmapCoord y, void* object)
{
    SELF(bm);
    return CPP_BOOL(self->unlinkObjectInCell(x, y, object));
}

void Blockmap_UnlinkObjectInCellBlock(Blockmap* bm, const BlockmapCellBlock* cellBlock, void* object)
{
    SELF(bm);
    if(!cellBlock) return;
    self->unlinkObjectInCellBlock(*cellBlock, object);
}

uint Blockmap_CellObjectCount(Blockmap const* bm, const_BlockmapCell mcell)
{
    SELF_CONST(bm);
    return self->cellObjectCount(mcell);
}

uint Blockmap_CellXYObjectCount(Blockmap const* bm, BlockmapCoord x, BlockmapCoord y)
{
    SELF_CONST(bm);
    return self->cellObjectCount(x, y);
}

int Blockmap_IterateCellObjects(Blockmap* bm, const_BlockmapCell mcell,
    int (*callback) (void* object, void* parameters), void* parameters)
{
    SELF(bm);
    return self->iterateCellObjects(mcell, callback, parameters);
}

int Blockmap_IterateCellBlockObjects(Blockmap* bm, const BlockmapCellBlock* cellBlock,
    int (*callback) (void* object, void* parameters), void* parameters)
{
    SELF(bm);
    if(!cellBlock) return false;
    return self->iterateCellBlockObjects(*cellBlock, callback, parameters);
}
