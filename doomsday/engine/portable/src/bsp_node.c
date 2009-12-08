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
#include "de_bsp.h"
#include "de_play.h"
#include "de_misc.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static __inline int pointOnHEdgeSide(double x, double y,
                                     const hedge_t* part)
{
    bsp_hedgeinfo_t* data = (bsp_hedgeinfo_t*) part->data;

    return P_PointOnLineDefSide2(x, y, data->pDX, data->pDY, data->pPerp,
                                 data->pLength, DIST_EPSILON);
}

/**
 * Add the given half-edge to the specified list.
 */
void BSP_AddHEdgeToSuperBlock(superblock_t* block, hedge_t* hEdge)
{
#define SUPER_IS_LEAF(s)  \
    ((s)->bbox[BOXRIGHT] - (s)->bbox[BOXLEFT] <= 256 && \
     (s)->bbox[BOXTOP] - (s)->bbox[BOXBOTTOM] <= 256)

    for(;;)
    {
        int p1, p2, child, midPoint[2];
        superblock_t* sub;
        bsp_hedgeinfo_t* info = (bsp_hedgeinfo_t*) hEdge->data;

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
            ((bsp_hedgeinfo_t*) hEdge->data)->block = block;
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
            ((bsp_hedgeinfo_t*) hEdge->data)->block = block;
            return;
        }

        // The seg lies in one half of this block. Create the block if it
        // doesn't already exist, and loop back to add the seg.
        if(!block->subs[child])
        {
            block->subs[child] = sub = BSP_SuperBlockCreate();
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

static boolean getAveragedCoords(const hedge_t* firstHEdge, double* x, double* y)
{
    size_t total = 0;
    double avg[2];
    const hedge_t* hEdge;

    if(!x || !y)
        return false;

    avg[VX] = avg[VY] = 0;

    hEdge = firstHEdge;
    do
    {
        avg[VX] += hEdge->vertex->pos[VX];
        avg[VY] += hEdge->vertex->pos[VY];

        avg[VX] += hEdge->twin->vertex->pos[VX];
        avg[VY] += hEdge->twin->vertex->pos[VY];

        total += 2;
    } while((hEdge = hEdge->next) != firstHEdge);

    if(total > 0)
    {
        *x = avg[VX] / total;
        *y = avg[VY] / total;
        return true;
    }

    return false;
}

/**
 * Sort half-edges by angle (from the middle point to the start vertex).
 */
static void sortHEdgesByAngleAroundMidPoint(face_t* face)
{
    hedge_t* hEdge;
    double midPoint[2];

    if(!face->hEdge || face->hEdge->next == face->hEdge)
        return;

    getAveragedCoords(face->hEdge, &midPoint[0], &midPoint[1]);

    hEdge = face->hEdge;
    for(;;)
    {
        const hedge_t* hEdgeA = hEdge;
        const hedge_t* hEdgeB = hEdge->next;
        angle_g angle1, angle2;

        if(hEdge->next == face->hEdge)
            break; // Sorted.

        angle1 = M_SlopeToAngle(hEdgeA->vertex->pos[0] - midPoint[0],
                                hEdgeA->vertex->pos[1] - midPoint[1]);
        angle2 = M_SlopeToAngle(hEdgeB->vertex->pos[0] - midPoint[0],
                                hEdgeB->vertex->pos[1] - midPoint[1]);

        if(angle1 + ANG_EPSILON < angle2)
        {   // Swap them.
            hedge_t* other = hEdge->next;

            if(hEdge == face->hEdge)
                face->hEdge = hEdge->next;

            hEdge->prev->next = hEdge->next;
            hEdge->next->prev = hEdge->prev;

            hEdge->next->next->prev = hEdge;
            hEdge->next = hEdge->next->next;

            hEdge->prev = other;
            other->next = hEdge;

            hEdge = face->hEdge;
        }
        else
        {
            hEdge = hEdge->next;
        }
    }
}

/**
 * Sort the list of half-edges in the leaf into clockwise order, based on their
 * position/orientation around the mid point in the leaf.
 */
static void clockwiseOrder(face_t* face)
{
    {
    hedge_t* firstHEdge;
    hedge_node_t* node, *next;

    // Copy order from face.
    node = (hedge_node_t*) face->hEdge;
    do
    {
        hedge_t* hEdge = node->hEdge;

        hEdge->next = node->next->hEdge;
        hEdge->next->prev = hEdge;
    } while((node = node->next) != (hedge_node_t*) face->hEdge);

    // Delete the nodes and switch to the hedge links.
    firstHEdge = ((hedge_node_t*) face->hEdge)->hEdge;
    node = (hedge_node_t*) face->hEdge;
    do
    {
        next = node->next;
        M_Free(node);
    } while((node = next) != (hedge_node_t*) face->hEdge);

    face->hEdge = firstHEdge;
    }

    sortHEdgesByAngleAroundMidPoint(face);

/*#if _DEBUG
{
const hedge_t* hEdge;

Con_Message("Sorted half-edges around (%1.1f,%1.1f)\n", x, y);

hEdge = face->hEdge;
do
{
    angle_g angle = M_SlopeToAngle(hEdge->vertex->pos[0] - midPoint[0],
                                   hEdge->vertex->pos[1] - midPoint[1]);

    Con_Message("  half-edge %p: Angle %1.6f  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
                hEdge, angle, (float) hEdge->vertex->pos[0], (float) hEdge->vertex->pos[1],
                (float) hEdge->twin->vertex->pos[0], (float) hEdge->twin->vertex->pos[1]);
} while((hEdge = hEdge->next) != face->hEdge);
}
#endif*/
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
    const bsp_hedgeinfo_t* data;

    {
    const hedge_t* n;

    // Find a suitable half-edge for comparison.
    n = face->hEdge;
    do
    {
        if(!((bsp_hedgeinfo_t*) n->data)->sector)
            continue;

        cur = n;
        break;
    } while((n = n->next) != face->hEdge);

    if(!cur)
        return;

    data = (bsp_hedgeinfo_t*) cur->data;
    cur = cur->next;
    }

    do
    {
        const bsp_hedgeinfo_t* curData = (bsp_hedgeinfo_t*) cur->data;

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
        if(((const bsp_hedgeinfo_t*) n->data)->lineDef)
            return true;
    } while((n = n->next) != face->hEdge);

    return false;
}

static boolean C_DECL clockwiseLeaf(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {
        face_t* face = (face_t*) BinaryTree_GetData(tree);

        clockwiseOrder(face);

        sanityCheckClosed(face);
        sanityCheckSameSector(face);
        if(!sanityCheckHasRealHEdge(face))
        {
            Con_Error("BSP leaf #%p has no linedef-linked half-edge.", face);
        }
    }

    return true; // Continue traversal.
}

/**
 * Traverse the BSP tree and put all the half-edges in each subsector into
 * clockwise order.
 *
 * @note This cannot be done during BuildNodes() since splitting a half-edge
 * with a twin may insert another half-edge into that twin's list, usually
 * in the wrong place order-wise.
 *
 * danij 25/10/2009: Actually, this CAN (and should) be done during BuildNodes().
 */
void ClockwiseBspTree(binarytree_t* rootNode)
{
    uint curIndex = 0;
    BinaryTree_PostOrder(rootNode, clockwiseLeaf, &curIndex);
}

static void takeHEdgesFromSuperblock(face_t* face, superblock_t* block)
{
    uint num;
    hedge_t* hEdge;

    while((hEdge = SuperBlock_PopHEdge(block)))
    {
        ((bsp_hedgeinfo_t*) hEdge->data)->block = NULL;
        // Link it into head of the leaf's list.
        Face_LinkHEdge(face, hEdge);
        ((bsp_hedgeinfo_t*) hEdge->data)->face = face;
    }

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        superblock_t* a = block->subs[num];

        if(a)
        {
            takeHEdgesFromSuperblock(face, a);

            if(a->realNum + a->miniNum > 0)
                Con_Error("createSubSectorWorker: child %d not empty!", num);

            BSP_SuperBlockDestroy(a);
            block->subs[num] = NULL;
        }
    }

    block->realNum = block->miniNum = 0;
}

/**
 * Create a new leaf from a list of half-edges.
 */
static face_t* createBSPLeaf(superblock_t* hEdgeList)
{
    face_t* face = HalfEdgeDS_CreateFace(Map_HalfEdgeDS(editMap));

    takeHEdgesFromSuperblock(face, hEdgeList);

    return face;
}

/**
 * Takes the half-edge list and determines if it is convex, possibly
 * converting it into a subsector. Otherwise, the list is divided into two
 * halves and recursion will continue on the new sub list.
 *
 * @param hEdgeList     Ptr to the list of half edges at the current node.
 * @param parent        Ptr to write back the address of any newly created
 *                      subtree.
 * @param depth         Current tree depth.
 * @param cutList       Ptr to the cutlist to use for storing new segment
 *                      intersections (cuts).
 * @return              @c true, if successfull.
 */
boolean BuildNodes(superblock_t* hEdgeList, binarytree_t** parent,
                   size_t depth, cutlist_t* cutList)
{
    binarytree_t* subTree;
    superblock_t* hEdgeSet[2];
    boolean builtOK = false;
    bspartition_t partition;
    node_t* node;

    *parent = NULL;

/*#if _DEBUG
Con_Message("Build: Begun @ %lu\n", (unsigned long) depth);
BSP_PrintSuperblockHEdges(hEdgeList);
#endif*/

    // Pick the next partition to use.
    if(!SuperBlock_PickPartition(hEdgeList, depth, &partition))
    {   // No partition required, already convex.
/*#if _DEBUG
Con_Message("BuildNodes: Convex.\n");
#endif*/
        *parent = BinaryTree_Create(createBSPLeaf(hEdgeList));
        return true;
    }

/*#if _DEBUG
Con_Message("BuildNodes: Partition %p (%1.0f,%1.0f) -> (%1.0f,%1.0f).\n",
            best, best->v[0]->V_pos[VX], best->v[0]->V_pos[VY],
            best->v[1]->V_pos[VX], best->v[1]->V_pos[VY]);
#endif*/

    // Create left and right super blocks.
    hEdgeSet[RIGHT] = (superblock_t *) BSP_SuperBlockCreate();
    hEdgeSet[LEFT]  = (superblock_t *) BSP_SuperBlockCreate();

    // Copy the bounding box of the edge list to the superblocks.
    M_CopyBox(hEdgeSet[LEFT]->bbox, hEdgeList->bbox);
    M_CopyBox(hEdgeSet[RIGHT]->bbox, hEdgeList->bbox);

    // Divide the half-edges into two lists: left & right.
    BSP_PartitionHEdges(hEdgeList, &partition, hEdgeSet[RIGHT],
                        hEdgeSet[LEFT], cutList);
    BSP_CutListEmpty(cutList);

    node = Z_Calloc(sizeof(*node), PU_STATIC, 0);
    *parent = BinaryTree_Create(node);

    BSP_FindNodeBounds(node, hEdgeSet[RIGHT], hEdgeSet[LEFT]);

    node->partition.x = partition.x;
    node->partition.y = partition.y;
    node->partition.dX = partition.dX;
    node->partition.dY = partition.dY;

    builtOK = BuildNodes(hEdgeSet[RIGHT], &subTree, depth + 1, cutList);
    BinaryTree_SetChild(*parent, RIGHT, subTree);
    BSP_SuperBlockDestroy(hEdgeSet[RIGHT]);

    if(builtOK)
    {
        builtOK = BuildNodes(hEdgeSet[LEFT], &subTree, depth + 1, cutList);
        BinaryTree_SetChild(*parent, LEFT, subTree);
    }

    BSP_SuperBlockDestroy(hEdgeSet[LEFT]);

    return builtOK;
}
