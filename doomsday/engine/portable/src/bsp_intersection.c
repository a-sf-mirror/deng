/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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
 * bsp_intersection.c: Intersections and cutlists (lists of intersections).
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_play.h"
#include "de_misc.h"

#include "bsp_node.h"
#include "bsp_edge.h"
#include "bsp_intersection.h"
#include "bsp_superblock.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct cnode_s {
    void*           data;
    struct cnode_s* next;
    struct cnode_s* prev;
} cnode_t;

/**
 * An "intersection" remembers the half-edge that intercepts the partition.
 */
typedef struct intersection_s {
    hedge_t*        hEdge;

    // How far along the partition line the intercept point is. Zero is at
    // the partition half-edge's start point, positive values move in the
    // same direction as the partition's direction, and negative values move
    // in the opposite direction.
    double          distance;
} intersection_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static cnode_t* allocCNode(void)
{
    return M_Malloc(sizeof(cnode_t));
}

static void freeCNode(cnode_t* node)
{
    M_Free(node);
}

static cnode_t* quickAllocCNode(cutlist_t* list)
{
    cnode_t* node;

    if(list->unused)
    {
        node = list->unused;
        list->unused = list->unused->next;
    }
    else
    {   // Need to allocate another.
        node = allocCNode();
    }

    node->data = NULL;
    node->next = node->prev = NULL;

    return node;
}

static intersection_t* allocIntersection(void)
{
    return M_Calloc(sizeof(intersection_t));
}

static void freeIntersection(intersection_t* cut)
{
    M_Free(cut);
}

/**
 * Destroy the specified intersection.
 *
 * @param cut           Ptr to the intersection to be destroyed.
 */
static void destroyIntersection(cutlist_t* list, intersection_t* cut)
{
    freeIntersection(cut);
}

static intersection_t* quickAllocIntersection(cutlist_t* list)
{
    intersection_t* cut = allocIntersection();

    memset(cut, 0, sizeof(*cut));

    return cut;
}

static void empty(cutlist_t* list, boolean destroyNodes)
{
    cnode_t* node;

    node = list->head;
    while(node)
    {
        cnode_t* p = node->next;

        destroyIntersection(list, node->data);

        // Move the list node to the unused node list?
        if(destroyNodes)
        {
            freeCNode(node);
        }
        else
        {
            node->next = list->unused;
            list->unused = node;
        }

        node = p;
    }

    list->head = NULL;
}

/**
 * Insert the given intersection into the specified cutlist.
 *
 * @return              @c true, if successful.
 */
static boolean insertIntersection(cutlist_t* list, intersection_t* cut)
{
    cnode_t* newNode = quickAllocCNode(list);
    cnode_t* after;

    /**
     * Insert the new intersection into the list.
     */
    after = list->head;
    while(after && after->next)
        after = after->next;

    while(after &&
          cut->distance < ((intersection_t *)after->data)->distance)
        after = after->prev;

    newNode->data = cut;

    // Link it in.
    newNode->next = (after? after->next : list->head);
    newNode->prev = after;

    if(after)
    {
        if(after->next)
            after->next->prev = newNode;

        after->next = newNode;
    }
    else
    {
        if(list->head)
            list->head->prev = newNode;

        list->head = newNode;
    }

    return true;
}

/**
 * Create a new cutlist.
 */
cutlist_t* BSP_CutListCreate(void)
{
    return M_Calloc(sizeof(cutlist_t));
}

/**
 * Destroy a cutlist.
 */
void BSP_CutListDestroy(cutlist_t* list)
{
    assert(list);
    empty(list, true);
    M_Free(list);
}

/**
 * Create a new intersection.
 */
void CutList_Intersect(cutlist_t* list, hedge_t* hEdge, double distance)
{
    assert(list);
    assert(hEdge);
    {
    intersection_t* cut = quickAllocIntersection(list);

    cut->distance = distance;
    cut->hEdge = hEdge;

    insertIntersection(list, cut);
    }
}

/**
 * Empty all intersections from the specified cutlist.
 */
void CutList_Reset(cutlist_t* list)
{
    assert(list);
    empty(list, false);
}

/**
 * Search the given list for an intersection, if found; return it.
 *
 * @param list          The list to be searched.
 * @param hEdge         Ptr to the intercept half-edge to look for.
 *
 * @return              @c true iff an intersection is found ELSE @c false;
 */
boolean CutList_Find(cutlist_t* list, hedge_t* hEdge)
{
    assert(list);
    assert(hEdge);
    {
    cnode_t* node;

    node = list->head;
    while(node)
    {
        intersection_t *cut = node->data;

        if(cut->hEdge == hEdge)
            return true;

        node = node->next;
    }

    return false;
    }
}

void BSP_MergeOverlappingIntersections(cutlist_t* list)
{
    assert(list);
    {
    cnode_t* firstNode, *node, *np;

    node = firstNode = list->head;
    np = node->next;
    while(node && np)
    {
        intersection_t* cur = node->data;
        intersection_t* next = np->data;
        double len = next->distance - cur->distance;

        if(len < -0.1)
        {
            Con_Error("mergeOverlappingIntersections: Bad order in intersect list "
                      "%1.3f > %1.3f\n", cur->distance, next->distance);
        }
        else if(len > 0.2)
        {
            node = np;
            np = node->next;
            continue;
        }
/*        else if(len > DIST_EPSILON)
        {
#if _DEBUG
Con_Message(" Skipping very short half-edge (len=%1.3f) near "
            "(%1.1f,%1.1f)\n", len, cur->vertex->V_pos[VX],
            cur->vertex->V_pos[VY]);
#endif
        }*/

        // Free the unused cut.
        node->next = np->next;

        destroyIntersection(list, next);

        np = node->next;
    }
    }
}

/**
 * Look for the first half-edge whose angle is past that required.
 */
static hedge_t* vertexCheckOpen(vertex_t* vertex, angle_g angle, byte antiClockwise)
{
    hedge_t* hEdge, *first;

    first = vertex->hEdge;
    hEdge = first->twin->next;
    do
    {
        if(((hedge_info_t*) hEdge->data)->pAngle <=
           ((hedge_info_t*) first->data)->pAngle)
            first = hEdge;
    } while((hEdge = hEdge->twin->next) != vertex->hEdge);

    if(antiClockwise)
    {
        first = first->twin->next;
        hEdge = first;
        while(((hedge_info_t*) hEdge->data)->pAngle > angle + ANG_EPSILON &&
              (hEdge = hEdge->twin->next) != first);

        return hEdge->twin;
    }
    else
    {
        hEdge = first;
        while(((hedge_info_t*) hEdge->data)->pAngle < angle + ANG_EPSILON &&
              (hEdge = hEdge->prev->twin) != first);

        return hEdge;
    }
}

static boolean isIntersectionOnSelfRefLineDef(const intersection_t* insect)
{
    /*if(insect->after && ((hedge_info_t*) insect->after->data)->lineDef)
    {
        linedef_t* lineDef = ((hedge_info_t*) insect->after->data)->lineDef;

        if(lineDef->buildData.sideDefs[FRONT] &&
           lineDef->buildData.sideDefs[BACK] &&
           lineDef->buildData.sideDefs[FRONT]->sector ==
           lineDef->buildData.sideDefs[BACK]->sector)
            return true;
    }

    if(insect->before && ((hedge_info_t*) insect->before->data)->lineDef)
    {
        linedef_t* lineDef = ((hedge_info_t*) insect->before->data)->lineDef;

        if(lineDef->buildData.sideDefs[FRONT] &&
           lineDef->buildData.sideDefs[BACK] &&
           lineDef->buildData.sideDefs[FRONT]->sector ==
           lineDef->buildData.sideDefs[BACK]->sector)
            return true;
    }*/

    return false;
}

void NodeBuilder_ConnectGaps(nodebuilder_t* nb, double x, double y,
                             double dX, double dY, const hedge_t* partHEdge,
                             superblock_t* rightList, superblock_t* leftList)
{
    cnode_t* node, *firstNode;

    node = firstNode = nb->_cutList->head;
    while(node && node->next)
    {
        const intersection_t* cur = node->data;
        const intersection_t* next = node->next->data;
        hedge_t* farHEdge, *nearHEdge;
        sector_t* nearSector = NULL, *farSector = NULL;
        boolean alongPartition = false;

        // Is this half-edge exactly aligned to the partition?
        {
        hedge_t* hEdge;
        angle_g angle;

        angle = M_SlopeToAngle(-dX, -dY);
        hEdge = next->hEdge;
        do
        {
            angle_g diff = fabs(((hedge_info_t*) hEdge->data)->pAngle - angle);
            if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
            {
                alongPartition = true;
                break;
            }
        } while((hEdge = hEdge->prev->twin) != next->hEdge);
        }

        if(!alongPartition)
        {
            farHEdge = vertexCheckOpen(next->hEdge->vertex, M_SlopeToAngle(-dX, -dY), false);
            nearHEdge = vertexCheckOpen(cur->hEdge->vertex, M_SlopeToAngle(dX, dY), true);

            nearSector = nearHEdge ? ((hedge_info_t*) nearHEdge->data)->sector : NULL;
            farSector = farHEdge? ((hedge_info_t*) farHEdge->data)->sector : NULL;
        }

        if(!(!nearSector && !farSector))
        {
            // Check for some nasty open/closed or close/open cases.
            if(nearSector && !farSector)
            {
                if(!isIntersectionOnSelfRefLineDef(cur))
                {
                    double pos[2];

                    pos[0] = cur->hEdge->vertex->pos[0] + next->hEdge->vertex->pos[0];
                    pos[1] = cur->hEdge->vertex->pos[1] + next->hEdge->vertex->pos[1];
                    pos[0] /= 2;
                    pos[1] /= 2;

                    nearSector->flags |= SECF_UNCLOSED;

                    VERBOSE(
                    Con_Message("Warning: Unclosed sector #%d near [%1.1f, %1.1f]\n",
                                nearSector->buildData.index - 1, pos[0], pos[1]))
                }
            }
            else if(!nearSector && farSector)
            {
                if(!isIntersectionOnSelfRefLineDef(next))
                {
                    double pos[2];

                    pos[0] = cur->hEdge->vertex->pos[0] + next->hEdge->vertex->pos[0];
                    pos[1] = cur->hEdge->vertex->pos[1] + next->hEdge->vertex->pos[1];
                    pos[0] /= 2;
                    pos[1] /= 2;

                    farSector->flags |= SECF_UNCLOSED;

                    VERBOSE(
                    Con_Message("Warning: Unclosed sector #%d near [%1.1f, %1.1f]\n",
                                farSector->buildData.index - 1, pos[0], pos[1]))
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
                    if(!isIntersectionOnSelfRefLineDef(cur) &&
                       !isIntersectionOnSelfRefLineDef(next))
                    {
#if _DEBUG
VERBOSE(
Con_Message("Sector mismatch: #%d (%1.1f,%1.1f) != #%d (%1.1f,%1.1f)\n",
            nearSector->buildData.index - 1,
            (float) cur->hEdge->vertex->pos[0], (float) cur->hEdge->vertex->pos[1],
            farSector->buildData.index - 1,
            (float) next->hEdge->vertex->pos[0], (float) next->hEdge->vertex->pos[1]));
#endif
                    }

                    // Choose the non-self-referencing sector when we can.
                    if(isIntersectionOnSelfRefLineDef(cur) &&
                       !isIntersectionOnSelfRefLineDef(next))
                    {
                        nearSector = farSector;
                    }
                }

                {
                hedge_t* right, *left;

                right = NodeBuilder_CreateHEdge(nb, NULL, ((hedge_info_t*) partHEdge->data)->lineDef, cur->hEdge->vertex, ((hedge_info_t*) nearHEdge->data)->sector, ((hedge_info_t*) nearHEdge->data)->side);
                right->face = nearHEdge->face;

                left = NodeBuilder_CreateHEdge(nb, NULL, ((hedge_info_t*) partHEdge->data)->lineDef, next->hEdge->vertex, ((hedge_info_t*) farHEdge->prev->data)->sector, ((hedge_info_t*) farHEdge->prev->data)->side);
                left->face = farHEdge->prev->face;

                // Twin the half-edges together.
                right->twin = left;
                left->twin = right;

                left->prev = farHEdge->prev;
                right->prev = nearHEdge;

                right->next = farHEdge;
                left->next = nearHEdge->next;

                left->prev->next = left;
                right->prev->next = right;

                right->next->prev = right;
                left->next->prev = left;

#if _DEBUG
testVertexHEdgeRings(cur->hEdge->vertex);
testVertexHEdgeRings(next->hEdge->vertex);
#endif

                BSP_UpdateHEdgeInfo(right);
                BSP_UpdateHEdgeInfo(left);

                // Add the new half-edges to the appropriate lists.
                BSP_AddHEdgeToSuperBlock(rightList, right);
                BSP_AddHEdgeToSuperBlock(leftList, left);

/*#if _DEBUG
Con_Message("buildEdgeBetweenIntersections: Capped intersection:\n");
Con_Message("  %p RIGHT  sector %d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
            right, (right->sector? right->sector->index : -1),
            right->vertex->V_pos[VX], right->vertex->V_pos[VY],
            right->twin->vertex->V_pos[VX], right->twin->vertex->V_pos[VY]);
Con_Message("  %p LEFT sector %d (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
            left, (left->sector? left->sector->index : -1),
            left->vertex->V_pos[VX], left->vertex->V_pos[VY],
            left->twin->vertex->V_pos[VX], left->twin->vertex->V_pos[VY]);
#endif*/
                }
            }
        }

        node = node->next;
    }
}
