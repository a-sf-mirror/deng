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

struct BlockmapCellData
{
    BlockmapRingNode* ringNodes;

    /// Running total of the number of objects linked in this cell.
    uint objectCount;
};

struct blockmap_s
{
    /// Minimal and Maximal points in map space coordinates.
    AABoxd bounds;

    /// Cell dimensions in map space coordinates.
    vec2d_t cellSize;

    /// Gridmap which implements the blockmap itself.
    Gridmap* gridmap;
};

Blockmap* Blockmap_New(coord_t const min[2], coord_t const max[2], uint cellWidth, uint cellHeight)
{
    Blockmap* bm = (Blockmap*)Z_Calloc(sizeof *bm, PU_MAPSTATIC, 0);
    if(!bm)
    {
        QString msg = QString("Blockmap::New: Failed on allocation of %1 bytes for new Blockmap").arg((unsigned long) sizeof *bm);
        LegacyCore_FatalError(msg.toUtf8().constData());
    }

    V2d_Copy(bm->bounds.min, min);
    V2d_Copy(bm->bounds.max, max);
    bm->cellSize[VX] = cellWidth;
    bm->cellSize[VY] = cellHeight;

    uint width  = uint( ceil((max[0] - min[0]) / coord_t(cellWidth)) );
    uint height = uint( ceil((max[1] - min[1]) / coord_t(cellHeight)) );
    bm->gridmap = Gridmap_New(width, height, sizeof(BlockmapCellData), PU_MAPSTATIC);

    LOG_INFO("Blockmap::New: Width:%u Height:%u") << width << height;
    return bm;
}

BlockmapCoord Blockmap_CellX(Blockmap* bm, coord_t x)
{
    DENG2_ASSERT(bm);
    uint result;
    Blockmap_ClipCellX(bm, &result, x);
    return result;
}

BlockmapCoord Blockmap_CellY(Blockmap* bm, coord_t y)
{
    DENG2_ASSERT(bm);
    uint result;
    Blockmap_ClipCellY(bm, &result, y);
    return result;
}

boolean Blockmap_ClipCellX(Blockmap* bm, BlockmapCoord* outX, coord_t x)
{
    DENG2_ASSERT(bm);
    boolean adjusted = false;
    if(x < bm->bounds.minX)
    {
        x = bm->bounds.minX;
        adjusted = true;
    }
    else if(x >= bm->bounds.maxX)
    {
        x = bm->bounds.maxX - 1;
        adjusted = true;
    }
    if(outX)
    {
        *outX = uint((x - bm->bounds.minX) / bm->cellSize[VX]);
    }
    return adjusted;
}

boolean Blockmap_ClipCellY(Blockmap* bm, BlockmapCoord* outY, coord_t y)
{
    DENG2_ASSERT(bm);
    boolean adjusted = false;
    if(y < bm->bounds.minY)
    {
        y = bm->bounds.minY;
        adjusted = true;
    }
    else if(y >= bm->bounds.maxY)
    {
        y = bm->bounds.maxY - 1;
        adjusted = true;
    }
    if(outY)
    {
        *outY = uint((y - bm->bounds.minY) / bm->cellSize[VY]);
    }
    return adjusted;
}

boolean Blockmap_Cell(Blockmap* bm, BlockmapCell cell, coord_t const pos[2])
{
    DENG2_ASSERT(bm);
    if(cell && pos)
    {
        // Deliberate bitwise OR - we need to clip both X and Y.
        return Blockmap_ClipCellX(bm, &cell[0], pos[VX]) |
               Blockmap_ClipCellY(bm, &cell[1], pos[VY]);
    }
    return false;
}

boolean Blockmap_CellBlock(Blockmap* bm, BlockmapCellBlock* cellBlock, const AABoxd* box)
{
    DENG2_ASSERT(bm);
    if(cellBlock && box)
    {
        // Deliberate bitwise OR - we need to clip both Min and Max.
        return Blockmap_Cell(bm, cellBlock->min, box->min) |
               Blockmap_Cell(bm, cellBlock->max, box->max);
    }
    return false;
}

const pvec2d_t Blockmap_Origin(Blockmap* bm)
{
    DENG2_ASSERT(bm);
    return bm->bounds.min;
}

const AABoxd* Blockmap_Bounds(Blockmap* bm)
{
    DENG2_ASSERT(bm);
    return &bm->bounds;
}

BlockmapCoord Blockmap_Width(Blockmap* bm)
{
    DENG2_ASSERT(bm);
    return Gridmap_Width(bm->gridmap);
}

BlockmapCoord Blockmap_Height(Blockmap* bm)
{
    DENG2_ASSERT(bm);
    return Gridmap_Height(bm->gridmap);
}

void Blockmap_Size(Blockmap* bm, BlockmapCoord v[])
{
    DENG2_ASSERT(bm);
    Gridmap_Size(bm->gridmap, v);
}

coord_t Blockmap_CellWidth(Blockmap* bm)
{
    DENG2_ASSERT(bm);
    return bm->cellSize[VX];
}

coord_t Blockmap_CellHeight(Blockmap* bm)
{
    DENG2_ASSERT(bm);
    return bm->cellSize[VY];
}

const pvec2d_t Blockmap_CellSize(Blockmap* bm)
{
    DENG2_ASSERT(bm);
    return bm->cellSize;
}

static void linkObjectToRing(void* object, BlockmapCellData* data)
{
    DENG2_ASSERT(object && data);
    BlockmapRingNode* node;

    if(!data->ringNodes)
    {
        // Create a new root node.
        node = (BlockmapRingNode*)Z_Malloc(sizeof(*node), PU_MAP, 0);
        node->next = NULL;
        node->prev = NULL;
        node->object = object;
        data->ringNodes = node;
        return;
    }

    // Is there an available node in the ring we can reuse?
    for(node = data->ringNodes; node->next && node->object; node = node->next)
    {}

    if(!node->object)
    {
        // This will do nicely.
        node->object = object;
        return;
    }

    // Add a new node to the ring.
    node->next = (BlockmapRingNode*)Z_Malloc(sizeof(*node), PU_MAP, 0);
    node->next->next = NULL;
    node->next->prev = node;
    node->next->object = object;
}

/**
 * Lookup an object in this cell by memory address.
 */
static BlockmapRingNode* BlockmapCellData_Node(BlockmapCellData* data, void* object)
{
    DENG2_ASSERT(data);
    if(!object) return NULL;
    for(BlockmapRingNode* node = data->ringNodes; node; node = node->next)
    {
        if(node->object == object) return node;
    }
    return NULL;
}

/**
 * Unlink the given object from the specified cell ring (if indeed linked).
 *
 * @param object  Object to be unlinked.
 * @return  @c true iff the object was linked to the ring and was unlinked.
 */
static boolean unlinkObjectFromRing(void* object, BlockmapCellData* data)
{
    BlockmapRingNode* node = BlockmapCellData_Node(data, object);
    if(!node) return false; // Object was not linked.

    // Unlink from the ring (the node will be reused).
    node->object = NULL;
    return true; // Object was unlinked.
}

static int unlinkObjectInCell(void* ptr, void* parameters)
{
    BlockmapCellData* data = reinterpret_cast<BlockmapCellData*>(ptr);
    if(unlinkObjectFromRing(parameters/*object ptr*/, data))
    {
        // There is now one fewer object in the cell.
        data->objectCount--;
    }
    return false; // Continue iteration.
}

static int linkObjectInCell(void* ptr, void* parameters)
{
    BlockmapCellData* data = reinterpret_cast<BlockmapCellData*>(ptr);
    linkObjectToRing(parameters/*object ptr*/, data);

    // There is now one more object in the cell.
    data->objectCount++;
    return false; // Continue iteration.
}

boolean Blockmap_CreateCellAndLinkObjectXY(Blockmap* bm, BlockmapCoord x, BlockmapCoord y, void* object)
{
    DENG2_ASSERT(bm && object);
    BlockmapCellData* data = reinterpret_cast<BlockmapCellData*>(Gridmap_CellXY(bm->gridmap, x, y, true));
    if(!data) return false; // Outside the blockmap?
    linkObjectInCell((void*)data, object);
    return true; // Link added.
}

boolean Blockmap_CreateCellAndLinkObject(Blockmap* bm, const_BlockmapCell cell, void* object)
{
    DENG2_ASSERT(cell);
    return Blockmap_CreateCellAndLinkObjectXY(bm, cell[VX], cell[VY], object);
}

boolean Blockmap_UnlinkObjectInCell(Blockmap* bm, const_BlockmapCell cell, void* object)
{
    DENG2_ASSERT(bm);

    boolean unlinked = false;
    BlockmapCellData* data = reinterpret_cast<BlockmapCellData*>(Gridmap_Cell(bm->gridmap, cell, false));
    if(data)
    {
        unlinked = unlinkObjectInCell((void*)data, (void*)object);
    }
    return unlinked;
}

boolean Blockmap_UnlinkObjectInCellXY(Blockmap* bm, BlockmapCoord x, BlockmapCoord y, void* object)
{
    BlockmapCell cell = { x, y };
    return Blockmap_UnlinkObjectInCell(bm, cell, object);
}

void Blockmap_UnlinkObjectInCellBlock(Blockmap* bm, const BlockmapCellBlock* cellBlock, void* object)
{
    DENG2_ASSERT(bm);
    if(!cellBlock) return;

    Gridmap_BlockIterate2(bm->gridmap, cellBlock, unlinkObjectInCell, object);
}

uint Blockmap_CellObjectCount(Blockmap* bm, const_BlockmapCell cell)
{
    DENG2_ASSERT(bm);

    BlockmapCellData* data = reinterpret_cast<BlockmapCellData*>(Gridmap_Cell(bm->gridmap, cell, false));
    if(!data) return 0;
    return data->objectCount;
}

uint Blockmap_CellXYObjectCount(Blockmap* bm, BlockmapCoord x, BlockmapCoord y)
{
    BlockmapCell cell = { x, y };
    return Blockmap_CellObjectCount(bm, cell);
}

int BlockmapCellData_IterateObjects(BlockmapCellData* data,
    int (*callback) (void* object, void* parameters), void* parameters)
{
    DENG2_ASSERT(data);
    BlockmapRingNode* link = data->ringNodes;
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

int Blockmap_IterateCellObjects(Blockmap* bm, const_BlockmapCell cell,
    int (*callback) (void* object, void* parameters), void* parameters)
{
    DENG2_ASSERT(bm);
    BlockmapCellData* data = reinterpret_cast<BlockmapCellData*>(Gridmap_Cell(bm->gridmap, cell, false));
    if(data)
    {
        return BlockmapCellData_IterateObjects(data, callback, parameters);
    }
    return false; // Continue iteration.
}

typedef struct {
    int (*callback)(void* userData, void* parameters);
    void* parameters;
} cellobjectiterator_params_t;

static int cellObjectIterator(void* userData, void* parameters)
{
    BlockmapCellData* data = reinterpret_cast<BlockmapCellData*>(userData);
    cellobjectiterator_params_t* args = static_cast<cellobjectiterator_params_t*>(parameters);
    DENG2_ASSERT(args);

    return BlockmapCellData_IterateObjects(data, args->callback, args->parameters);
}

int Blockmap_IterateCellBlockObjects(Blockmap* bm, const BlockmapCellBlock* cellBlock,
    int (*callback) (void* object, void* parameters), void* parameters)
{
    DENG2_ASSERT(bm);

    cellobjectiterator_params_t args;
    args.callback   = callback;
    args.parameters = parameters;
    return Gridmap_BlockIterate2(bm->gridmap, cellBlock, cellObjectIterator, (void*)&args);
}

const Gridmap* Blockmap_Gridmap(Blockmap* bm)
{
    DENG2_ASSERT(bm);
    return bm->gridmap;
}
