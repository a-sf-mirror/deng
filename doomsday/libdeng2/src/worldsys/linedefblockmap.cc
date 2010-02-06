/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
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


#include <cmath>

#include "de/LineDefBlockmap"

using namespace de;

LineDefBlockmap::LineDefBlockmap(const Rectangle<Vector2f>& aaBB, duint width, duint height)
  : _gridmap(width, height),
    _aaBB(aaBB)
{
    _blockSize = _aaBB.topRight() - _aaBB.bottomLeft();
    _blockSize.x /= width;
    _blockSize.y /= height;
}

LineDefBlockmap::~LineDefBlockmap()
{
    clear();
}

static listnode_t* allocListNode(void)
{
    return Z_Calloc(sizeof(listnode_t), PU_STATIC, 0);
}

static void freeListNode(listnode_t* node)
{
    Z_Free(node);
}

static linklist_t* allocList(void)
{
    return Z_Calloc(sizeof(linklist_t), PU_STATIC, 0);
}

static void freeList(linklist_t* list)
{
    listnode_t* node = list->head;

    while(node)
    {
        listnode_t* next = node->next;
        freeListNode(node);
        node = next;
    }

    Z_Free(list);
}

static bool freeLineDefBlockData(linklist_t* list, void* context)
{
    if(list)
        freeList(list);
    return true; // Continue iteration.
}

void LineDefBlockmap::clear()
{
    _gridmap.iterate(freeLineDefBlockData, NULL);
}

static void listPushFront(linklist_t* list, LineDef* lineDef)
{
    listnode_t* node = allocListNode();

    node->data = lineDef;

    node->next = list->head;
    list->head = node;

    list->size += 1;
}

static bool listRemove(linklist_t* list, LineDef* lineDef)
{
    if(list->head)
    {
        listnode_t** node = &list->head;

        do
        {
            listnode_t** next = &(*node)->next;

            if((*node)->data == lineDef)
            {
                freeListNode(*node);
                *node = *next;
                list->size -= 1;
                return true; // LineDef was unlinked.
            }

            node = next;
        } while(*node);
    }

    return false; // LineDef was not linked.
}

static duint listSize(linklist_t* list)
{
    return list->size;
}

static bool listSearch(linklist_t* list, const LineDef* lineDef)
{
    if(list->head)
    {
        listnode_t** node = &list->head;

        do
        {
            listnode_t** next = &(*node)->next;

            if((*node)->data == lineDef)
                return true;

            node = next;
        } while(*node);
    }

    return false;
}

void LineDefBlockmap::linkLineDefToBlock(LineDef& lineDef, duint x, duint y)
{
    linklist_t* list = _gridmap.block(x, y);
    if(!list)
        list = _gridmap.setBlock(x, y, allocList());
    listPushFront(list, &lineDef);
}

bool LineDefBlockmap::unlinkLineDefFromBlock(LineDef& lineDef, duint x, duint y)
{
    linklist_t* list = _gridmap.block(x, y);

    if(list)
    {
        bool result = listRemove(list, &lineDef);
        if(result && !list->head)
        {
            freeList(list);
            _gridmap.setBlock(x, y, NULL);
        }
        return result;
    }
    return false;
}

bool LineDefBlockmap::isLineDefLinkedToBlock(const LineDef& lineDef, duint x, duint y) const
{
    linklist_t* list = _gridmap.block(x, y);
    if(list)
        return listSearch(list, &lineDef);
    return false;
}

static bool iterateLineDefs(linklist_t* list, void* context)
{
    iteratelinedefs_args_t* args = (iteratelinedefs_args_t*) context;

    if(list)
    {
        listnode_t* node = list->head;

        while(node)
        {
            listnode_t* next = node->next;

            if(node->data)
            {
                LineDef* lineDef = reinterpret_cast<LineDef*>(node->data);

                if(lineDef->validCount != args->localValidCount)
                {
                    LineDef* ptr;

                    lineDef->validCount = args->localValidCount;

                    if(args->retObjRecord)
                        ptr = reinterpret_cast<LineDef*>(P_ObjectRecord(DMU_LINEDEF, lineDef));
                    else
                        ptr = lineDef;

                    if(!args->func(ptr, args->context))
                        return false;
                }
            }

            node = next;
        }
    }

    return true;
}

/**
 * Given a set of world space coordinates of the form minX, minY, maxX, maxY
 * return the blockmap block indices that contain those points, arranged so
 * that bottomLeft is the left lowermost block and topRight is the right
 * uppermost block in the same coordinate space.
 */
Rectangleui LineDefBlockmap::boxToBlocks(const Rectanglef& box) const
{
    Vector2f bottomLeft = _aaBB.bottomLeft().max(box.bottomLeft());
    bottomLeft -= _aaBB.bottomLeft();
    bottomLeft.x /= _blockSize.x;
    bottomLeft.y /= _blockSize.y;

    Vector2f topRight = _aaBB.topRight().min(box.topRight());
    topRight -= _aaBB.bottomLeft();
    topRight.x /= _blockSize.x;
    topRight.y /= _blockSize.y;

    Vector2ui temp  = Vector2ui(0, 0).max(bottomLeft.min(_gridmap.dimensions()));
    Vector2ui temp2 = Vector2ui(0, 0).max(topRight.min(_gridmap.dimensions()));

    return Rectangleui(Vector2ui(temp.x, temp2.y), Vector2ui(temp2.x, temp.y));
}

bool LineDefBlockmap::block(Vector2<duint>& dest, dfloat x, dfloat y) const
{
    if(!(x < _bottomLeft.x || x >= _topRight.x ||
         y < _bottomLeft.y || y >= _topRight.y))
    {
        dest.x = (x - _bottomLeft.x) / _blockSize.x;
        dest.y = (y - _bottomLeft.y) / _blockSize.y;
        return true;
    }
    return false;
}

void LineDefBlockmap::tryLinkLineDefToBlock(duint x, duint y, LineDef* lineDef)
{
    if(isLineDefLinkedToBlock(x, y, lineDef))
        return; // Already linked.

    linkLineDefToBlock(x, y, lineDef);
}

void LineDefBlockmap::linkLineDef(LineDef* lineDef, const Vector2<dint>& vtx1, const Vector2<dint>& vtx2)
{
#define BLKSHIFT                7 // places to shift rel position for cell num
#define BLKMASK                 ((1<<BLKSHIFT)-1) // mask for rel position within cell

    duint i, blockBox[2][2], dimensions[2];
    dint vert, horiz;
    dint origin[2], aabb[2][2], delta[2];
    bool slopePos, slopeNeg;

    assert(lineDef);

    origin[0] = (dint) blockmap->_bottomLeft.x;
    origin[1] = (dint) blockmap->_bottomLeft.y;
    Gridmap_Dimensions(blockmap->gridmap, dimensions);

    // Determine all blocks it touches and add the lineDef number to those blocks.
    _bottomLeft.x = MIN_OF(vtx1[0], vtx2[0]);
    _bottomLeft.y = MIN_OF(vtx1[1], vtx2[1]);

    _topRight.x = MAX_OF(vtx1[0], vtx2[0]);
    _topRight.y = MAX_OF(vtx1[1], vtx2[1]);

    LineDefBlockmap_Block2f(blockmap, blockBox[0], _bottomLeft.x, _bottomLeft.y);
    LineDefBlockmap_Block2f(blockmap, blockBox[1], _topRight.x, _topRight.y);

    delta[0] = vtx2[0] - vtx1[0];
    delta[1] = vtx2[1] - vtx1[1];

    vert = !delta[0];
    horiz = !delta[1];

    slopePos = (delta[0] ^ delta[1]) > 0;
    slopeNeg = (delta[0] ^ delta[1]) < 0;

    // The lineDef always belongs to the blocks containing its endpoints
    linkLineDefToBlock(blockmap, blockBox[0][0], blockBox[0][1], lineDef);
    if(blockBox[0][0] != blockBox[1][0] || blockBox[0][1] != blockBox[1][1])
        linkLineDefToBlock(blockmap, blockBox[1][0], blockBox[1][1], lineDef);

    // For each column, see where the lineDef along its left edge, which
    // it contains, intersects the LineDef. We don't want to interesect
    // vertical lines with columns.
    if(!vert)
    {
        for(i = blockBox[0][0]; i <= blockBox[1][0]; ++i)
        {
            // intersection of LineDef with x=origin[0]+(i<<BLKSHIFT)
            // (y-vtx1[1])*delta[0] = delta[1]*(x-vtx1[0])
            // y = delta[1]*(x-vtx1[0])+vtx1[1]*delta[0];
            dint x = origin[0] + (i << BLKSHIFT);
            dint y, yb, yp;

            // Does the lineDef touch this column at all?
            if(x < _bottomLeft.x || x > _topRight.x)
                continue;

            y = (delta[1] * (x - vtx1[0])) / delta[0] + vtx1[1];
            yb = (y - origin[1]) >> BLKSHIFT; // block row number

            // Outside the blockmap?
            if(yb < 0 || yb > (signed) (dimensions[1]) + 1)
                continue;

            // The cell that contains the intersection point is always added.
            tryLinkLineDefToBlock(blockmap, i, yb, lineDef);

            // If the intersection is at a corner it depends on the slope
            // (and whether the lineDef extends past the intersection) which
            // blocks are hit.

            // Where does the intersection occur?
            yp = (y - origin[1]) & BLKMASK; // y position within block
            if(yp == 0)
            {
                // Intersection occured at a corner
                if(slopeNeg) //   \ - blocks x,y-, x-,y
                {
                    if(yb > 0 && _bottomLeft.y < y)
                        tryLinkLineDefToBlock(blockmap, i, yb - 1, lineDef);

                    if(i > 0 && _bottomLeft.x < x)
                        tryLinkLineDefToBlock(blockmap, i - 1, yb, lineDef);
                }
                else if(slopePos) //   / - block x-,y-
                {
                    if(yb > 0 && i > 0 && _bottomLeft.x < x)
                        tryLinkLineDefToBlock(blockmap, i - 1, yb - 1, lineDef);
                }
                else if(horiz) //   - - block x-,y
                {
                    if(i > 0 && _bottomLeft.x < x)
                        tryLinkLineDefToBlock(blockmap, i - 1, yb, lineDef);
                }
            }
            else if(i > 0 && _bottomLeft.x < x)
            {
                // Else not at corner: x-,y
                tryLinkLineDefToBlock(blockmap, i - 1, yb, lineDef);
            }
        }
    }

    // For each row, see where the lineDef along its bottom edge, which
    // it contains, intersects the LineDef.
    if(!horiz)
    {
        for(i = blockBox[0][1]; i <= blockBox[1][1]; ++i)
        {
            // intersection of LineDef with y=origin[1]+(i<<BLKSHIFT)
            // (x,y) on LineDef satisfies: (y-vtx1[1])*delta[0] = delta[1]*(x-vtx1[0])
            // x = delta[0]*(y-vtx1[1])/delta[1]+vtx1[0];
            dint y = origin[1] + (i << BLKSHIFT); // (x,y) is intersection
            dint x, xb, xp;

            // Touches this row?
            if(y < _bottomLeft.y || y > _topRight.y)
                continue;

            x = (delta[0] * (y - vtx1[1])) / delta[1] + vtx1[0];
            xb = (x - origin[0]) >> BLKSHIFT; // block column number

            // Outside the blockmap?
            if(xb < 0 || xb > (signed) (dimensions[0]) + 1)
                continue;

            tryLinkLineDefToBlock(blockmap, xb, i, lineDef);

            // If the intersection is at a corner it depends on the slope
            // (and whether the lineDef extends past the intersection) which
            // blocks are hit

            // Where does the intersection occur?
            xp = (x - origin[0]) & BLKMASK; // x position within block
            if(xp == 0)
            {
                // Intersection occured at a corner
                if(slopeNeg) //   \ - blocks x,y-, x-,y
                {
                    if(i > 0 && _bottomLeft.y < y)
                        tryLinkLineDefToBlock(blockmap, xb, i - 1, lineDef);

                    if(xb > 0 && _bottomLeft.x < x)
                        tryLinkLineDefToBlock(blockmap, xb - 1, i, lineDef);
                }
                else if(vert) //   | - block x,y-
                {
                    if(i > 0 && _bottomLeft.y < y)
                        tryLinkLineDefToBlock(blockmap, xb, i - 1, lineDef);
                }
                else if(slopePos) //   / - block x-,y-
                {
                    if(xb > 0 && i > 0 && _bottomLeft.y < y)
                        tryLinkLineDefToBlock(blockmap, xb - 1, i - 1, lineDef);
                }
            }
            else if(i > 0 && _bottomLeft.y < y)
            {
                // Else not on a corner: x, y-
                tryLinkLineDefToBlock(blockmap, xb, i - 1, lineDef);
            }
        }
    }

#undef BLKSHIFT
#undef BLKMASK
}

void LineDefBlockmap_Link(LineDefBlockmap* blockmap, LineDef* lineDef)
{
    assert(blockmap);
    assert(lineDef);
    {
    dint vtx1[2], vtx2[2];

    vtx1[0] = (dint) lineDef->buildData.v[0]->pos[0];
    vtx1[1] = (dint) lineDef->buildData.v[0]->pos[1];

    vtx2[0] = (dint) lineDef->buildData.v[1]->pos[0];
    vtx2[1] = (dint) lineDef->buildData.v[1]->pos[1];

    linkLineDef(blockmap, lineDef, vtx1, vtx2);
    }
}

void LineDefBlockmap_Link2(LineDefBlockmap* blockmap, LineDef* lineDef)
{
    assert(blockmap);
    assert(lineDef);
    {
    dint vtx1[2], vtx2[2];

    vtx1[0] = (dint) lineDef->L_v1->pos[0];
    vtx1[1] = (dint) lineDef->L_v1->pos[1];

    vtx2[0] = (dint) lineDef->L_v2->pos[0];
    vtx2[1] = (dint) lineDef->L_v2->pos[1];

    linkLineDef(blockmap, lineDef, vtx1, vtx2);
    }
}

bool LineDefBlockmap_Unlink(LineDefBlockmap* blockmap, LineDef* lineDef)
{
    duint x, y, dimensions[2];

    assert(blockmap);
    assert(lineDef);

    Gridmap_Dimensions(blockmap->gridmap, dimensions);

    // @optimize Keep a record of linedef to block links.
    for(y = 0; y < dimensions[1]; ++y)
        for(x = 0; x < dimensions[0]; ++x)
            unlinkLineDefFromBlock(blockmap, x, y, lineDef);

    return true;
}

void LineDefBlockmap_Bounds(LineDefBlockmap* blockmap, pvec2_t min, pvec2_t max)
{
    assert(blockmap);

    if(min)
        V2_Copy(min, blockmap->aabb[0]);
    if(max)
        V2_Copy(max, blockmap->aabb[1]);
}

void LineDefBlockmap_BlockSize(LineDefBlockmap* blockmap, pvec2_t blockSize)
{
    assert(blockmap);
    assert(blockSize);

    V2_Copy(blockSize, blockmap->blockSize);
}

void LineDefBlockmap_Dimensions(LineDefBlockmap* blockmap, duint v[2])
{
    assert(blockmap);

    Gridmap_Dimensions(blockmap->gridmap, v);
}

duint LineDefBlockmap_NumInBlock(LineDefBlockmap* blockmap, duint x, duint y)
{
    linklist_t* list;

    assert(blockmap);

    list = (linklist_t*) Gridmap_Block(blockmap->gridmap, x, y);
    if(list)
        return listSize(list);

    return 0;
}

bool LineDefBlockmap_Iterate(LineDefBlockmap* blockmap, const duint block[2],
                                bool (*func) (LineDef*, void*),
                                void* context, bool retObjRecord)
{
    iteratelinedefs_args_t args;

    assert(blockmap);
    assert(block);
    assert(func);

    args.func = func;
    args.context = context;
    args.localValidCount = validCount;
    args.retObjRecord = retObjRecord;

    return iterateLineDefs(Gridmap_Block(blockmap->gridmap, block[0], block[1]),
                           (void*) &args);
}

bool LineDefBlockmap_BoxIterate(LineDefBlockmap* blockmap, const duint blockBox[4],
                                   bool (*func) (LineDef*, void*),
                                   void* context, bool retObjRecord)
{
    iteratelinedefs_args_t args;

    assert(blockmap);
    assert(blockBox);
    assert(func);

    args.func = func;
    args.context = context;
    args.localValidCount = validCount;
    args.retObjRecord = retObjRecord;

    return Gridmap_IterateBoxv(blockmap->gridmap, blockBox, iterateLineDefs, (void*) &args);
}

/**
 * Looks for lines in the given block that intercept the given trace to add
 * to the intercepts list
 * A line is crossed if its endpoints are on opposite sides of the trace.
 *
 * @return              @c true if earlyout and a solid line hit.
 */
static bool addLineIntercepts(LineDef* ld, void* data)
{
    dint s[2];
    float frac;
    divline_t dl;

    // Avoid precision problems with two routines.
    if(traceLOS.dX > FRACUNIT * 16 || traceLOS.dY > FRACUNIT * 16 ||
       traceLOS.dX < -FRACUNIT * 16 || traceLOS.dY < -FRACUNIT * 16)
    {
        s[0] = P_PointOnDivlineSide(ld->L_v1->pos[VX],
                                    ld->L_v1->pos[VY], &traceLOS);
        s[1] = P_PointOnDivlineSide(ld->L_v2->pos[VX],
                                    ld->L_v2->pos[VY], &traceLOS);
    }
    else
    {
        s[0] = LineDef_PointOnSide(ld, FIX2FLT(traceLOS.pos[VX]),
                                       FIX2FLT(traceLOS.pos[VY]));
        s[1] = LineDef_PointOnSide(ld, FIX2FLT(traceLOS.pos[VX] + traceLOS.dX),
                                       FIX2FLT(traceLOS.pos[VY] + traceLOS.dY));
    }

    if(s[0] == s[1])
        return true; // Line isn't crossed.

    // Hit the line.
    LineDef_ConstructDivline(ld, &dl);
    frac = P_InterceptVector(&traceLOS, &dl);
    if(frac < 0)
        return true; // Behind source.

    // Try to early out the check.
    if(earlyout && frac < 1 && !LINE_BACKSIDE(ld))
        return false; // Stop iteration.

    P_AddIntercept(frac, true, ld);

    return true; // Continue iteration.
}

bool LineDefBlockmap_PathTraverse(LineDefBlockmap* blockmap, const duint originBlock[2],
                                     const duint destBlock[2], const float origin[2],
                                     const float dest[2],
                                     bool (*func) (intercept_t*))
{
    duint count, block[2];
    float delta[2], partial;
    dfixed intercept[2], step[2];
    dint stepDir[2];

    if(destBlock[0] > originBlock[0])
    {
        stepDir[0] = 1;
        partial = FIX2FLT(FRACUNIT - ((FLT2FIX(origin[0]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
        delta[1] = (dest[1] - origin[1]) / fabs(dest[0] - origin[0]);
    }
    else if(destBlock[0] < originBlock[0])
    {
        stepDir[0] = -1;
        partial = FIX2FLT((FLT2FIX(origin[0]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[1] = (dest[1] - origin[1]) / fabs(dest[0] - origin[0]);
    }
    else
    {
        stepDir[0] = 0;
        partial = 1;
        delta[1] = 256;
    }
    intercept[1] = (FLT2FIX(origin[1]) >> MAPBTOFRAC) +
        FLT2FIX(partial * delta[1]);

    if(destBlock[1] > originBlock[1])
    {
        stepDir[1] = 1;
        partial = FIX2FLT(FRACUNIT - ((FLT2FIX(origin[1]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
        delta[0] = (dest[0] - origin[0]) / fabs(dest[1] - origin[1]);
    }
    else if(destBlock[1] < originBlock[1])
    {
        stepDir[1] = -1;
        partial = FIX2FLT((FLT2FIX(origin[1]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[0] = (dest[0] - origin[0]) / fabs(dest[1] - origin[1]);
    }
    else
    {
        stepDir[1] = 0;
        partial = 1;
        delta[0] = 256;
    }
    intercept[0] = (FLT2FIX(origin[0]) >> MAPBTOFRAC) +
        FLT2FIX(partial * delta[0]);

    /**
     * Step through map blocks.
     *
     * Count is present because DOOM fails to check the case where the ray
     * passes diagonally through a vertex into another block.
     *
     * @todo Fix the problem. We only need to retain the original behaviour
     * with the mobj blockmap in DOOM, Heretic and Hexen.
     */
    block[0] = originBlock[0];
    block[1] = originBlock[1];
    step[0] = FLT2FIX(delta[0]);
    step[1] = FLT2FIX(delta[1]);
    for(count = 0; count < 64; ++count)
    {
        if(!LineDefBlockmap_Iterate(blockmap, block, addLineIntercepts, 0, false))
            return false; // Early out

        if(block[0] == destBlock[0] && block[1] == destBlock[1])
            break;

        if((unsigned) (intercept[1] >> FRACBITS) == block[1])
        {
            intercept[1] += step[1];
            block[0] += stepDir[0];
        }
        else if((unsigned) (intercept[0] >> FRACBITS) == block[0])
        {
            intercept[0] += step[0];
            block[1] += stepDir[1];
        }
    }

    return true;
}
