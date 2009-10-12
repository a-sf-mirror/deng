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
static void hardenLineDefSegList(gamemap_t* map, hedge_t* hEdge, seg_t* seg)
{
    const hedge_t* first, *last;
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

    lineDef->hEdges[0] = map->hEdges[((bsp_hedgeinfo_t*) first->data)->index];
    lineDef->hEdges[1] = map->hEdges[((bsp_hedgeinfo_t*) last->data)->index];
}

static int C_DECL hEdgeCompare(const void* p1, const void* p2)
{
    const bsp_hedgeinfo_t* a = (bsp_hedgeinfo_t*) (((const hedge_t**) p1)[0])->data;
    const bsp_hedgeinfo_t* b = (bsp_hedgeinfo_t*) (((const hedge_t**) p2)[0])->data;

    if(a->index == b->index)
        return 0;
    if(a->index < b->index)
        return -1;
    return 1;
}

typedef struct {
    size_t              numHEdges, numSegs;
    hedge_t***          indexPtr;
    boolean             write;
} hedgecollectorparams_t;

static boolean hEdgeCollector(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {
        hedgecollectorparams_t* params = (hedgecollectorparams_t*) data;
        bspleafdata_t* leaf = (bspleafdata_t*) BinaryTree_GetData(tree);
        hedge_node_t* n;

        for(n = leaf->hEdges; n; n = n->next)
        {
            hedge_t* hEdge = n->hEdge;

            if(params->indexPtr)
            {   // Write mode.
                (*params->indexPtr)[params->numHEdges++] = hEdge;
                if(hEdge->twin && ((bsp_hedgeinfo_t*) hEdge->twin->data)->lineDef &&
                   !((bsp_hedgeinfo_t*) hEdge->data)->lineDef->buildData.windowEffect &&
                   !((bsp_hedgeinfo_t*) hEdge->twin->data)->sector)
                    (*params->indexPtr)[params->numHEdges++] = hEdge->twin;
            }
            else
            {   // Count mode.
                if(((bsp_hedgeinfo_t*) hEdge->data)->index == -1)
                    Con_Error("HEdge %p never reached a subsector!", hEdge);

                params->numSegs++;

                params->numHEdges++;
                if(hEdge->twin && ((bsp_hedgeinfo_t*) hEdge->twin->data)->lineDef &&
                   !((bsp_hedgeinfo_t*) hEdge->data)->lineDef->buildData.windowEffect &&
                   !((bsp_hedgeinfo_t*) hEdge->twin->data)->sector)
                    params->numHEdges++;
            }
        }
    }

    return true; // Continue traversal.
}

static sector_t* pickSectorFromHEdges(const hedge_node_t* firstHEdge, boolean allowSelfRef)
{
    const hedge_node_t* node;
    sector_t* sector = NULL;

    for(node = firstHEdge; !sector && node; node = node->next)
    {
        const hedge_t* hEdge = node->hEdge;

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
    }

    return sector;
}

static boolean pickSectorForLeaf(binarytree_t* tree, void* data)
{
    if(BinaryTree_IsLeaf(tree))
    {
        bspleafdata_t* leaf = (bspleafdata_t*) BinaryTree_GetData(tree);

        /**
         * Determine which sector this leaf belongs to.
         * On the first pass, we are picky; do not consider half-edges from
         * self-referencing linedefs. If that fails, take whatever we can find.
         */
        leaf->sector = pickSectorFromHEdges(leaf->hEdges, false);
        if(!leaf->sector)
            leaf->sector = pickSectorFromHEdges(leaf->hEdges, true);
    }

    return true; // Continue traversal.
}

static void buildSegsFromHEdges(gamemap_t* map, binarytree_t* rootNode)
{
    uint i;
    hedgecollectorparams_t params;

    BinaryTree_InOrder(rootNode, pickSectorForLeaf, &params);

    // Pass 1: Count the number of used hedges.
    params.numHEdges = params.numSegs = 0;
    params.indexPtr = NULL;
    BinaryTree_InOrder(rootNode, hEdgeCollector, &params);

    if(!(params.numHEdges > 0))
        Con_Error("buildSegsFromHEdges: No halfedges?");

    map->numHEdges = (uint) params.numHEdges;
    map->hEdges = Z_Malloc(sizeof(hedge_t*) * params.numHEdges, PU_STATIC, 0);

    // Pass 2: Collect ptrs the hedges and insert into the index.
    params.numHEdges = 0;
    params.indexPtr = &map->hEdges;
    BinaryTree_InOrder(rootNode, hEdgeCollector, &params);

    // Sort the half-edges into ascending index order.
    qsort(map->hEdges, params.numHEdges, sizeof(hedge_t*), hEdgeCompare);

    // Generate seg data from (BSP) line segments.
    for(i = 0; i < map->numHEdges; ++i)
    {
        hedge_t* hEdge = map->hEdges[i];
        const bsp_hedgeinfo_t* data = (bsp_hedgeinfo_t*) hEdge->data;
        seg_t* seg;

        DMU_AddObjRecord(DMU_HEDGE, hEdge);

        if(data->lineDef && !data->sector)
        {
            if(hEdge->data)
                Z_Free(hEdge->data);
            hEdge->data = NULL;
            continue;
        }

        seg = Z_Calloc(sizeof(seg_t), PU_STATIC, 0);

        seg->side  = data->side;
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

            seg->offset = P_AccurateDistance(hEdge->HE_v1pos[VX] - vtx->V_pos[VX],
                                             hEdge->HE_v1pos[VY] - vtx->V_pos[VY]);

            hardenLineDefSegList(map, hEdge, seg);
        }

        seg->angle =
            bamsAtan2((int) (hEdge->twin->vertex->v.pos[VY] - hEdge->vertex->v.pos[VY]),
                      (int) (hEdge->twin->vertex->v.pos[VX] - hEdge->vertex->v.pos[VX])) << FRACBITS;

        // Calculate the length of the segment. We need this for
        // the texture coordinates. -jk
        seg->length = P_AccurateDistance(hEdge->HE_v2pos[VX] - hEdge->HE_v1pos[VX],
                                         hEdge->HE_v2pos[VY] - hEdge->HE_v1pos[VY]);

        if(seg->length == 0)
            seg->length = 0.01f; // Hmm...

        // Calculate the surface normals
        // Front first
        if(seg->sideDef)
        {
            surface_t*          surface = &seg->sideDef->SW_topsurface;

            surface->normal[VY] = (hEdge->HE_v1pos[VX] - hEdge->HE_v2pos[VX]) / seg->length;
            surface->normal[VX] = (hEdge->HE_v2pos[VY] - hEdge->HE_v1pos[VY]) / seg->length;
            surface->normal[VZ] = 0;

            // All surfaces of a sidedef have the same normal.
            memcpy(seg->sideDef->SW_middlenormal, surface->normal, sizeof(surface->normal));
            memcpy(seg->sideDef->SW_bottomnormal, surface->normal, sizeof(surface->normal));
        }

        hEdge->data = seg;
    }
}

static void hardenLeaf(gamemap_t* map, face_t* dest, const bspleafdata_t* src)
{
    size_t hEdgeCount;
    hedge_t* hEdge;
    hedge_node_t* n;

    dest->hEdge = src->hEdges->hEdge;

    hEdgeCount = 0;
    for(n = src->hEdges; ; n = n->next)
    {
        hedge_t* hEdge = n->hEdge;

        hEdgeCount++;

        if(!n->next)
        {
            hEdge->next = dest->hEdge;
            break;
        }

        hEdge->next = n->next->hEdge;
    }

    hEdge = dest->hEdge;
    do
    {
        hEdge->next->prev = hEdge;
        hEdge->face = dest;
    } while((hEdge = hEdge->next) != dest->hEdge);

    dest->data = Z_Calloc(sizeof(subsector_t), PU_STATIC, 0);

    DMU_AddObjRecord(DMU_FACE, dest);

    {
    subsector_t* subSector = ((subsector_t*) dest->data);

    subSector->hEdgeCount = (uint) hEdgeCount;

    subSector->sector = src->sector;
    if(!subSector->sector)
    {
        Con_Message("hardenLeaf: Warning orphan subsector %p (%i half-edges).\n",
                    subSector, subSector->hEdgeCount);
    }
    }
}

typedef struct {
    gamemap_t*      dest;
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

    node = params->dest->nodes[nodeData->index = params->nodeCurIndex++];

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
            bspleafdata_t*  leaf = (bspleafdata_t*) BinaryTree_GetData(right);
            uint            idx;
           
            idx = params->faceCurIndex++;
            node->children[RIGHT] = idx | NF_SUBSECTOR;
            hardenLeaf(params->dest, params->dest->faces[idx], leaf);
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
            bspleafdata_t*  leaf = (bspleafdata_t*) BinaryTree_GetData(left);
            uint            idx;
            
            idx = params->faceCurIndex++;
            node->children[LEFT] = idx | NF_SUBSECTOR;
            hardenLeaf(params->dest, params->dest->faces[idx], leaf);
        }
        else
        {
            bspnodedata_t*  data = (bspnodedata_t*) BinaryTree_GetData(left);

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

static void hardenBSP(gamemap_t* dest, binarytree_t* rootNode)
{
    {
    uint i;
    dest->numNodes = 0;
    BinaryTree_PostOrder(rootNode, countNode, &dest->numNodes);
    dest->nodes = Z_Malloc(dest->numNodes * sizeof(node_t*), PU_STATIC, 0);
    for(i = 0; i < dest->numNodes; ++i)
        dest->nodes[i] = Z_Calloc(sizeof(node_t), PU_STATIC, 0);
    }

    {
    uint i;
    dest->numFaces = 0;
    BinaryTree_PostOrder(rootNode, countFace, &dest->numFaces);
    dest->faces = Z_Malloc(dest->numFaces * sizeof(face_t*), PU_STATIC, 0);
    for(i = 0; i < dest->numFaces; ++i)
        dest->faces[i] = Z_Calloc(sizeof(face_t), PU_STATIC, 0);
    }

    if(rootNode)
    {
        hardenbspparams_t params;

        params.dest = dest;
        params.faceCurIndex = 0;
        params.nodeCurIndex = 0;

        BinaryTree_PostOrder(rootNode, hardenNode, &params);
    }
}

static void finishVertexes(gamemap_t* dest, vertex_t*** vertexes,
                           uint* numVertexes)
{
    uint i;

    dest->numVertexes = *numVertexes;
    dest->vertexes = *vertexes;

    for(i = 0; i < dest->numVertexes; ++i)
    {
        vertex_t* vtx = dest->vertexes[i];

        {
        edgetip_t* tip, *n;
        tip = vtx->buildData.tipSet;
        while(tip)
        {
            n = tip->ET_next;
            BSP_DestroyVertexEdgeTip(tip);
            tip = n;
        }
        }

        //// \fixme Add some rounding.
        vtx->V_pos[VX] = (float) vtx->buildData.pos[VX];
        vtx->V_pos[VY] = (float) vtx->buildData.pos[VY];

        DMU_AddObjRecord(DMU_VERTEX, vtx);
    }
}

void SaveMap(gamemap_t* dest, void* rootNode, vertex_t*** vertexes,
             uint* numVertexes)
{
    uint startTime = Sys_GetRealTime();
    binarytree_t* rn = (binarytree_t*) rootNode;

    finishVertexes(dest, vertexes, numVertexes);
    buildSegsFromHEdges(dest, rn);
    hardenBSP(dest, rn);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("SaveMap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}
