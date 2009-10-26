/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * bsp_map.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_edit.h"
#include "de_refresh.h"

#include <stdlib.h>
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

/**
 * \todo This is unnecessary if we ensure the first and last back ptrs in
 * linedef_t are updated after a half-edge split.
 */
static void hardenLineDefSegList(map_t* map, hedge_t* hEdge, seg_t* seg)
{
    hedge_t* first, *last;
    linedef_t* lineDef;

    if(!seg || !seg->sideDef)
        return;

    lineDef = seg->sideDef->lineDef;

    // Have we already processed this linedef?
    if(lineDef->hEdges[0])
        return;

    // Find the first hedge for this side.
    first = (seg->side? hEdge->twin : hEdge);

    while(((bsp_hedgeinfo_t*)first->data)->lprev)
        first = ((bsp_hedgeinfo_t*) first->data)->lprev;

    // Find the last.
    last = first;
    while(((bsp_hedgeinfo_t*)last->data)->lnext)
        last = ((bsp_hedgeinfo_t*)last->data)->lnext;

    lineDef->hEdges[0] = first;
    lineDef->hEdges[1] = last;
}

static boolean countSegs(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {
        uint* numSegs = (uint*) data;
        face_t* face = (face_t*) BinaryTree_GetData(tree);
        hedge_t* hEdge;

        hEdge = face->hEdge;
        do
        {
            if(!(((bsp_hedgeinfo_t*) hEdge->data)->lineDef &&
                 ((bsp_hedgeinfo_t*) hEdge->data)->lineDef->buildData.windowEffect &&
                 !((bsp_hedgeinfo_t*) hEdge->twin->data)->sector))
                (*numSegs)++;
        } while((hEdge = hEdge->next) != face->hEdge);
    }

    return true; // Continue traversal.
}

static void buildSegsFromHEdges(map_t* map, binarytree_t* rootNode)
{
    uint i;
    halfedgeds_t* halfEdgeDS = Map_HalfEdgeDS(map);

    // Count the number of used hedges (will become segs).
    map->numSegs = 0;
    BinaryTree_InOrder(rootNode, countSegs, &map->numSegs);

    map->segs = Z_Malloc(sizeof(seg_t*) * map->numSegs, PU_STATIC, 0);

    // Generate seg data from (BSP) line segments.
    for(i = 0; i < halfEdgeDS->numHEdges; ++i)
    {
        hedge_t* hEdge = halfEdgeDS->hEdges[i];
        const bsp_hedgeinfo_t* data = (bsp_hedgeinfo_t*) hEdge->data;
        seg_t* seg;

        if(data->lineDef && !data->sector)
        {
            if(hEdge->data)
                Z_Free(hEdge->data);
            hEdge->data = NULL;
            continue;
        }

        seg = Z_Calloc(sizeof(seg_t), PU_STATIC, 0);

        seg->hEdge = hEdge;
        seg->side = data->side;
        seg->sideDef = NULL;
        if(data->lineDef)
        {
            if(data->lineDef->buildData.sideDefs[seg->side])
                seg->sideDef = map->sideDefs[data->lineDef->buildData.sideDefs[data->side]->buildData.index - 1];
        }

        if(seg->sideDef)
        {
            linedef_t* ldef = seg->sideDef->lineDef;
            vertex_t* vtx = ldef->buildData.v[seg->side];

            seg->offset = P_AccurateDistance(hEdge->HE_v1->pos[VX] - vtx->pos[VX],
                                             hEdge->HE_v1->pos[VY] - vtx->pos[VY]);

            hardenLineDefSegList(map, hEdge, seg);
        }

        seg->angle =
            bamsAtan2((int) (hEdge->twin->vertex->pos[VY] - hEdge->vertex->pos[VY]),
                      (int) (hEdge->twin->vertex->pos[VX] - hEdge->vertex->pos[VX])) << FRACBITS;

        // Calculate the length of the segment. We need this for
        // the texture coordinates. -jk
        seg->length = P_AccurateDistance(hEdge->HE_v2->pos[VX] - hEdge->HE_v1->pos[VX],
                                         hEdge->HE_v2->pos[VY] - hEdge->HE_v1->pos[VY]);

        if(seg->length == 0)
            seg->length = 0.01f; // Hmm...

        // Calculate the surface normals
        // Front first
        if(seg->sideDef)
        {
            surface_t* surface = &seg->sideDef->SW_topsurface;

            surface->normal[VY] = (hEdge->HE_v1->pos[VX] - hEdge->HE_v2->pos[VX]) / seg->length;
            surface->normal[VX] = (hEdge->HE_v2->pos[VY] - hEdge->HE_v1->pos[VY]) / seg->length;
            surface->normal[VZ] = 0;

            // All surfaces of a sidedef have the same normal.
            memcpy(seg->sideDef->SW_middlenormal, surface->normal, sizeof(surface->normal));
            memcpy(seg->sideDef->SW_bottomnormal, surface->normal, sizeof(surface->normal));
        }

        map->segs[P_CreateObjectRecord(DMU_SEG, seg) - 1] = seg;
        hEdge->data = seg;
    }
}

static sector_t* pickSectorFromHEdges(const hedge_t* firstHEdge, boolean allowSelfRef)
{
    const hedge_t* hEdge;
    sector_t* sector = NULL;

    hEdge = firstHEdge;
    do
    {
        if(!allowSelfRef && hEdge->twin &&
           ((bsp_hedgeinfo_t*) hEdge->data)->sector ==
           ((bsp_hedgeinfo_t*) hEdge->twin->data)->sector)
            continue;
        
        if(((bsp_hedgeinfo_t*) hEdge->data)->lineDef &&
           ((bsp_hedgeinfo_t*) hEdge->data)->sector)
        {
            linedef_t* lineDef = ((bsp_hedgeinfo_t*) hEdge->data)->lineDef;

            if(lineDef->buildData.windowEffect && ((bsp_hedgeinfo_t*) hEdge->data)->side == 1)
                sector = lineDef->buildData.windowEffect;
            else
                sector = lineDef->buildData.sideDefs[
                    ((bsp_hedgeinfo_t*) hEdge->data)->side]->sector;
        }
    } while(!sector && (hEdge = hEdge->next) != firstHEdge);

    return sector;
}

static subsector_t* createSubsector(map_t* map, face_t* face)
{
    subsector_t* subsector = (subsector_t*) Z_Calloc(sizeof(subsector_t), PU_STATIC, 0);
    size_t hEdgeCount;
    hedge_t* hEdge;

    hEdgeCount = 0;
    hEdge = face->hEdge;
    do
    {
        hEdgeCount++;
    } while((hEdge = hEdge->next) != face->hEdge);

    subsector->face = face;
    subsector->hEdgeCount = (uint) hEdgeCount;

    /**
     * Determine which sector this subsector belongs to.
     * On the first pass, we are picky; do not consider half-edges from
     * self-referencing linedefs. If that fails, take whatever we can find.
     */
    subsector->sector = pickSectorFromHEdges(face->hEdge, false);
    if(!subsector->sector)
        subsector->sector = pickSectorFromHEdges(face->hEdge, true);

    if(!subsector->sector)
    {
        Con_Message("hardenLeaf: Warning orphan subsector %p (%i half-edges).\n",
                    subsector, subsector->hEdgeCount);
    }

    map->subsectors[P_CreateObjectRecord(DMU_SUBSECTOR, subsector) - 1] = subsector;

    return subsector;
}

static void createSubsectorForFace(map_t* map, face_t* face)
{
    face->data = createSubsector(map, face);
}

typedef struct {
    uint            faceCurIndex;
    uint            nodeCurIndex;
} hardenbspparams_t;

static boolean C_DECL hardenNode(binarytree_t* tree, void* data)
{
    binarytree_t* right, *left;
    bspnodedata_t* nodeData;
    hardenbspparams_t* params;
    node_t* node;

    if(BinaryTree_IsLeaf(tree))
        return true; // Continue iteration.

    nodeData = BinaryTree_GetData(tree);
    params = (hardenbspparams_t*) data;

    node = editMap->nodes[nodeData->index = params->nodeCurIndex++];

    node->partition.x = nodeData->partition.x;
    node->partition.y = nodeData->partition.y;
    node->partition.dX = nodeData->partition.dX;
    node->partition.dY = nodeData->partition.dY;

    node->bBox[RIGHT][BOXTOP]    = nodeData->bBox[RIGHT][BOXTOP];
    node->bBox[RIGHT][BOXBOTTOM] = nodeData->bBox[RIGHT][BOXBOTTOM];
    node->bBox[RIGHT][BOXLEFT]   = nodeData->bBox[RIGHT][BOXLEFT];
    node->bBox[RIGHT][BOXRIGHT]  = nodeData->bBox[RIGHT][BOXRIGHT];

    node->bBox[LEFT][BOXTOP]     = nodeData->bBox[LEFT][BOXTOP];
    node->bBox[LEFT][BOXBOTTOM]  = nodeData->bBox[LEFT][BOXBOTTOM];
    node->bBox[LEFT][BOXLEFT]    = nodeData->bBox[LEFT][BOXLEFT];
    node->bBox[LEFT][BOXRIGHT]   = nodeData->bBox[LEFT][BOXRIGHT];

    right = BinaryTree_GetChild(tree, RIGHT);
    if(right)
    {
        if(BinaryTree_IsLeaf(right))
        {
            face_t* face = (face_t*) BinaryTree_GetData(right);
            uint idx;
           
            idx = params->faceCurIndex++;
            node->children[RIGHT] = idx | NF_SUBSECTOR;
            createSubsectorForFace(editMap, face);
        }
        else
        {
            bspnodedata_t* data = (bspnodedata_t*) BinaryTree_GetData(right);

            node->children[RIGHT] = data->index;
        }
    }

    left = BinaryTree_GetChild(tree, LEFT);
    if(left)
    {
        if(BinaryTree_IsLeaf(left))
        {
            face_t* face = (face_t*) BinaryTree_GetData(left);
            uint idx;
            
            idx = params->faceCurIndex++;
            node->children[LEFT] = idx | NF_SUBSECTOR;
            createSubsectorForFace(editMap, face);
        }
        else
        {
            bspnodedata_t* data = (bspnodedata_t*) BinaryTree_GetData(left);

            node->children[LEFT] = data->index;
        }
    }

    return true; // Continue iteration.
}

static boolean C_DECL countNode(binarytree_t* tree, void* data)
{
    if(!BinaryTree_IsLeaf(tree))
    {
        (*((uint*) data))++;
    }

    return true; // Continue iteration.
}

static boolean C_DECL countFace(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {
        (*((uint*) data))++;
    }

    return true; // Continue iteration.
}

static void hardenBSP(map_t* map, binarytree_t* rootNode)
{
    halfedgeds_t* halfEdgeDS = Map_HalfEdgeDS(map);

    {
    uint i;
    map->numNodes = 0;
    BinaryTree_PostOrder(rootNode, countNode, &map->numNodes);
    map->nodes = Z_Malloc(map->numNodes * sizeof(node_t*), PU_STATIC, 0);
    for(i = 0; i < map->numNodes; ++i)
        map->nodes[i] = Z_Calloc(sizeof(node_t), PU_STATIC, 0);
    }

    map->numSubsectors = halfEdgeDS->numFaces;
    map->subsectors = Z_Malloc(map->numSubsectors * sizeof(subsector_t*), PU_STATIC, 0);

    {
    hardenbspparams_t params;

    params.faceCurIndex = 0;
    params.nodeCurIndex = 0;

    BinaryTree_PostOrder(rootNode, hardenNode, &params);
    }
}

void SaveMap(map_t* map, void* rootNode)
{
    uint startTime = Sys_GetRealTime();
    binarytree_t* rn = (binarytree_t*) rootNode;

    hardenBSP(map, rn);
    buildSegsFromHEdges(map, rn);

    // How much time did we spend?
    VERBOSE(
    Con_Message("SaveMap: Done in %.2f seconds.\n",
                (Sys_GetRealTime() - startTime) / 1000.0f));
}
