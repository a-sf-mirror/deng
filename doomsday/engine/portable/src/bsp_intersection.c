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
#include "de_bsp.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct cnode_s {
    void*       data;
    struct cnode_s* next;
    struct cnode_s* prev;
} cnode_t;

// The intersection list is kept sorted by along_dist, in ascending order.
typedef struct clist_s {
    cnode_t*    headPtr;
} clist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static clist_t* UnusedIntersectionList;
static cnode_t* unusedCNodes;

static boolean initedOK = false;

// CODE --------------------------------------------------------------------

static cnode_t* allocCNode(void)
{
    return M_Malloc(sizeof(cnode_t));
}

static void freeCNode(cnode_t* node)
{
    M_Free(node);
}

static cnode_t* quickAllocCNode(void)
{
    cnode_t*            node;

    if(initedOK && unusedCNodes)
    {
        node = unusedCNodes;
        unusedCNodes = unusedCNodes->next;
    }
    else
    {   // Need to allocate another.
        node = allocCNode();
    }

    node->data = NULL;
    node->next = node->prev = NULL;

    return node;
}

static clist_t* allocCList(void)
{
    return M_Malloc(sizeof(clist_t));
}

static void freeCList(clist_t* list)
{
    M_Free(list);
}

static intersection_t* allocIntersection(void)
{
    return M_Calloc(sizeof(intersection_t));
}

static void freeIntersection(intersection_t* cut)
{
    M_Free(cut);
}

static intersection_t* quickAllocIntersection(void)
{
    intersection_t*     cut;

    if(initedOK && UnusedIntersectionList->headPtr)
    {
        cnode_t*            node = UnusedIntersectionList->headPtr;

        // Unlink from the unused list.
        UnusedIntersectionList->headPtr = node->next;

        // Grab the intersection.
        cut = node->data;

        // Move the list node to the unused node list.
        node->next = unusedCNodes;
        unusedCNodes = node;
    }
    else
    {
        cut = allocIntersection();
    }

    memset(cut, 0, sizeof(*cut));

    return cut;
}

static void emptyCList(clist_t* list)
{
    cnode_t*            node;

    node = list->headPtr;
    while(node)
    {
        cnode_t*            p = node->next;

        BSP_IntersectionDestroy(node->data);

        // Move the list node to the unused node list.
        node->next = unusedCNodes;
        unusedCNodes = node;

        node = p;
    }

    list->headPtr = NULL;
}

void BSP_InitIntersectionAllocator(void)
{
    if(!initedOK)
    {
        UnusedIntersectionList = M_Calloc(sizeof(clist_t));
        unusedCNodes = NULL;
        initedOK = true;
    }
}

void BSP_ShutdownIntersectionAllocator(void)
{
    if(UnusedIntersectionList)
    {
        cnode_t*            node;

        node = UnusedIntersectionList->headPtr;
        while(node)
        {
            cnode_t*            p = node->next;

            freeIntersection(node->data);
            freeCNode(node);

            node = p;
        }

        M_Free(UnusedIntersectionList);
        UnusedIntersectionList = NULL;
    }

    if(unusedCNodes)
    {
        cnode_t*            node;

        node = unusedCNodes;
        while(node)
        {
            cnode_t*            np = node->next;

            freeCNode(node);
            node = np;
        }

        unusedCNodes = NULL;
    }

    initedOK = false;
}

/**
 * Create a new intersection.
 */
intersection_t* BSP_IntersectionCreate(vertex_t* vert, const struct bspartition_s* part)
{
    intersection_t* cut = quickAllocIntersection();

    cut->vertex = vert;
    cut->alongDist = M_ParallelDist(part->pDX, part->pDY, part->pPara, part->length,
                                    vert->pos[VX], vert->pos[VY]);

    return cut;
}

/**
 * Destroy the specified intersection.
 *
 * @param cut           Ptr to the intersection to be destroyed.
 */
void BSP_IntersectionDestroy(intersection_t* cut)
{
    if(initedOK)
    {   // If the allocator is initialized, move the intersection to the
        // unused list for reuse.
        cnode_t* node = quickAllocCNode();

        node->data = cut;
        node->next = UnusedIntersectionList->headPtr;
        node->prev = NULL;
        UnusedIntersectionList->headPtr = node;
    }
    else
    {   // Just free it.
        freeIntersection(cut);
    }
}

/**
 * Create a new cutlist.
 */
cutlist_t* BSP_CutListCreate(void)
{
    clist_t* list = allocCList();

    list->headPtr = NULL;

    return (cutlist_t*) list;
}

/**
 * Destroy a cutlist.
 */
void BSP_CutListDestroy(cutlist_t* cutList)
{
    if(cutList)
    {
        clist_t* list = (clist_t*) cutList;

        emptyCList(list);
        freeCList(list);
    }
}

/**
 * Empty all intersections from the specified cutlist.
 */
void BSP_CutListEmpty(cutlist_t* cutList)
{
    if(cutList)
    {
        clist_t* list = (clist_t*) cutList;
        emptyCList(list);
    }
}

/**
 * Search the given list for an intersection, if found; return it.
 *
 * @param list          The list to be searched.
 * @param vert          Ptr to the intersection vertex to look for.
 *
 * @return              Ptr to the found intersection, else @c NULL;
 */
intersection_t* BSP_CutListFindIntersection(cutlist_t* cutList, vertex_t* v)
{
    clist_t* list = (clist_t*) cutList;
    cnode_t* node;

    node = list->headPtr;
    while(node)
    {
        intersection_t *cut = node->data;

        if(cut->vertex == v)
            return cut;

        node = node->next;
    }

    return NULL;
}

/**
 * Insert the given intersection into the specified cutlist.
 *
 * @return              @c true, if successful.
 */
boolean BSP_CutListInsertIntersection(cutlist_t* cutList,
                                      intersection_t* cut)
{
    if(cutList && cut)
    {
        clist_t* list = (clist_t*) cutList;
        cnode_t* newNode = quickAllocCNode();
        cnode_t* after;

        /**
         * Insert the new intersection into the list.
         */
        after = list->headPtr;
        while(after && after->next)
            after = after->next;

        while(after &&
              cut->alongDist < ((intersection_t *)after->data)->alongDist)
            after = after->prev;

        newNode->data = cut;

        // Link it in.
        newNode->next = (after? after->next : list->headPtr);
        newNode->prev = after;

        if(after)
        {
            if(after->next)
                after->next->prev = newNode;

            after->next = newNode;
        }
        else
        {
            if(list->headPtr)
                list->headPtr->prev = newNode;

            list->headPtr = newNode;
        }

        return true;
    }

    return false;
}

static boolean isIntersectionOnSelfRefLineDef(const intersection_t* insect)
{
    /*if(insect->after && ((bsp_hedgeinfo_t*) insect->after->data)->lineDef)
    {
        linedef_t* lineDef = ((bsp_hedgeinfo_t*) insect->after->data)->lineDef;

        if(lineDef->buildData.sideDefs[FRONT] &&
           lineDef->buildData.sideDefs[BACK] &&
           lineDef->buildData.sideDefs[FRONT]->sector ==
           lineDef->buildData.sideDefs[BACK]->sector)
            return true;
    }

    if(insect->before && ((bsp_hedgeinfo_t*) insect->before->data)->lineDef)
    {
        linedef_t* lineDef = ((bsp_hedgeinfo_t*) insect->before->data)->lineDef;

        if(lineDef->buildData.sideDefs[FRONT] &&
           lineDef->buildData.sideDefs[BACK] &&
           lineDef->buildData.sideDefs[FRONT]->sector ==
           lineDef->buildData.sideDefs[BACK]->sector)
            return true;
    }*/

    return false;
}

static void mergeOverlappingIntersections(cnode_t* firstNode)
{
    cnode_t* node, *np;

    node = firstNode;
    np = node->next;
    while(node && np)
    {
        intersection_t* cur = node->data;
        intersection_t* next = np->data;
        double len = next->alongDist - cur->alongDist;

        if(len < -0.1)
        {
            Con_Error("mergeOverlappingIntersections: Bad order in intersect list "
                      "%1.3f > %1.3f\n", cur->alongDist, next->alongDist);
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

        BSP_IntersectionDestroy(next);

        np = node->next;
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
        if(((bsp_hedgeinfo_t*) hEdge->data)->pAngle <=
           ((bsp_hedgeinfo_t*) first->data)->pAngle)
            first = hEdge;
    } while((hEdge = hEdge->twin->next) != vertex->hEdge);

    if(antiClockwise)
    {
        first = first->twin->next;
        hEdge = first;
        while(((bsp_hedgeinfo_t*) hEdge->data)->pAngle > angle + ANG_EPSILON &&
              (hEdge = hEdge->twin->next) != first);

        return hEdge->twin;
    }
    else
    {
        hEdge = first;
        while(((bsp_hedgeinfo_t*) hEdge->data)->pAngle < angle + ANG_EPSILON &&
              (hEdge = hEdge->prev->twin) != first);

        return hEdge;
    }
}

static void connectGaps(const bspartition_t* part, superblock_t* rightList,
                        superblock_t* leftList, cnode_t* firstNode)
{
    cnode_t* node;

    node = firstNode;
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

        angle = M_SlopeToAngle(-part->pDX, -part->pDY);
        hEdge = next->vertex->hEdge;
        do
        {
            angle_g diff = fabs(((bsp_hedgeinfo_t*) hEdge->data)->pAngle - angle);
            if(diff < ANG_EPSILON || diff > (360.0 - ANG_EPSILON))
            {
                alongPartition = true;
                break;
            }
        } while((hEdge = hEdge->prev->twin) != next->vertex->hEdge);
        }

        if(!alongPartition)
        {
            farHEdge = vertexCheckOpen(next->vertex, M_SlopeToAngle(-part->pDX, -part->pDY), false);
            nearHEdge = vertexCheckOpen(cur->vertex, M_SlopeToAngle(part->pDX, part->pDY), true);

            nearSector = nearHEdge ? ((bsp_hedgeinfo_t*) nearHEdge->data)->sector : NULL;
            farSector = farHEdge? ((bsp_hedgeinfo_t*) farHEdge->data)->sector : NULL;
        }

        if(!(!nearSector && !farSector))
        {
            // Check for some nasty open/closed or close/open cases.
            if(nearSector && !farSector)
            {
                if(!isIntersectionOnSelfRefLineDef(cur))
                {
                    double pos[2];

                    pos[0] = cur->vertex->pos[0] + next->vertex->pos[0];
                    pos[1] = cur->vertex->pos[1] + next->vertex->pos[1];
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

                    pos[0] = cur->vertex->pos[0] + next->vertex->pos[0];
                    pos[1] = cur->vertex->pos[1] + next->vertex->pos[1];
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
            (float) cur->vertex->pos[0], (float) cur->vertex->pos[1],
            farSector->buildData.index - 1,
            (float) next->vertex->pos[0], (float) next->vertex->pos[1]));
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

                right = BSP_CreateHEdge(NULL, part->lineDef, cur->vertex, ((bsp_hedgeinfo_t*) nearHEdge->data)->sector, ((bsp_hedgeinfo_t*) nearHEdge->data)->side);
                right->face = nearHEdge->face;

                left = BSP_CreateHEdge(NULL, part->lineDef, next->vertex, ((bsp_hedgeinfo_t*) farHEdge->prev->data)->sector, ((bsp_hedgeinfo_t*) farHEdge->prev->data)->side);
                left->face = farHEdge->prev->face;

                // Twin the half-edges together.
                right->twin = left;
                left->twin = right;

#if 0
                /**
                 * @todo Use method for linking the new half-edges along the
                 * partition into their respective vertex rings.
                 * Not possible until createInitialHEdges is updated.
                 */
                left->prev = farHEdge->prev;
                right->prev = nearHEdge;

                right->next = farHEdge;
                left->next = nearHEdge->next;

                left->prev->next = left;
                right->prev->next = right;

                right->next->prev = right;
                left->next->prev = left;

#if _DEBUG
testVertexHEdgeRings(cur->vertex);
testVertexHEdgeRings(next->vertex);
#endif
#else
                right->next = right->prev = right;
                left->next = left->prev = left;
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

/**
 * Analyze the intersection list, and add any needed minihedges to the given
 * half-edge lists (one minihedge on each side).
 *
 * @note All the intersections in the cutList will be freed back into the
 * quick-alloc list for re-use!
 *
 * @todo Does not belong in here.
 */
void BSP_AddMiniHEdges(const bspartition_t* part, superblock_t* rightList,
                       superblock_t* leftList, cutlist_t* cutList)
{
    clist_t* list;

    if(!cutList)
        return;

    list = (clist_t*) cutList;

    mergeOverlappingIntersections(list->headPtr);
    connectGaps(part, rightList, leftList, list->headPtr);
}
