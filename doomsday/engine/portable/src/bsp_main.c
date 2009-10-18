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
 * bsp_main.c: GL-friendly BSP node builder.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_play.h"
#include "de_misc.h"

#include "p_mapdata.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int bspFactor = 7;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Register the ccmds and cvars of the BSP builder. Called during engine
 * startup
 */
void BSP_Register(void)
{
    C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0);
}

static void findMapLimits(gamemap_t* src, int* bbox)
{
    uint i;

    M_ClearBox(bbox);

    for(i = 0; i < src->numLineDefs; ++i)
    {
        linedef_t* l = src->lineDefs[i];
        double x1 = l->buildData.v[0]->pos[VX];
        double y1 = l->buildData.v[0]->pos[VY];
        double x2 = l->buildData.v[1]->pos[VX];
        double y2 = l->buildData.v[1]->pos[VY];
        int lX = (int) floor(MIN_OF(x1, x2));
        int lY = (int) floor(MIN_OF(y1, y2));
        int hX = (int) ceil(MAX_OF(x1, x2));
        int hY = (int) ceil(MAX_OF(y1, y2));

        M_AddToBox(bbox, lX, lY);
        M_AddToBox(bbox, hX, hY);
    }
}

/**
 * Initially create all half-edges, one for each side of a linedef.
 *
 * @return              The list of created half-edges.
 */
static superblock_t* createInitialHEdges(gamemap_t* map)
{
    uint startTime = Sys_GetRealTime();

    uint i;
    int bw, bh;
    hedge_t* back, *front;
    superblock_t* block;
    int mapBounds[4];

    // Find maximal vertexes.
    findMapLimits(map, mapBounds);

    VERBOSE(Con_Message("Map goes from (%d,%d) to (%d,%d)\n",
                        mapBounds[BOXLEFT], mapBounds[BOXBOTTOM],
                        mapBounds[BOXRIGHT], mapBounds[BOXTOP]));

    block = BSP_SuperBlockCreate();

    block->bbox[BOXLEFT]   = mapBounds[BOXLEFT]   - (mapBounds[BOXLEFT]   & 0x7);
    block->bbox[BOXBOTTOM] = mapBounds[BOXBOTTOM] - (mapBounds[BOXBOTTOM] & 0x7);
    bw = ((mapBounds[BOXRIGHT] - block->bbox[BOXLEFT])   / 128) + 1;
    bh = ((mapBounds[BOXTOP]   - block->bbox[BOXBOTTOM]) / 128) + 1;

    block->bbox[BOXRIGHT] = block->bbox[BOXLEFT]   + 128 * M_CeilPow2(bw);
    block->bbox[BOXTOP]   = block->bbox[BOXBOTTOM] + 128 * M_CeilPow2(bh);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        linedef_t* line = map->lineDefs[i];

        front = back = NULL;

        // Ignore polyobj lines.
        if(/*line->buildData.overlap || */
           (line->inFlags & LF_POLYOBJ))
            continue;

        // Check for Humungously long lines.
        if(ABS(line->buildData.v[0]->pos[VX] - line->buildData.v[1]->pos[VX]) >= 10000 ||
           ABS(line->buildData.v[0]->pos[VY] - line->buildData.v[1]->pos[VY]) >= 10000)
        {
            if(3000 >=
               M_Length(line->buildData.v[0]->pos[VX] - line->buildData.v[1]->pos[VX],
                        line->buildData.v[0]->pos[VY] - line->buildData.v[1]->pos[VY]))
            {
                Con_Message("LineDef #%d is VERY long, it may cause problems\n",
                            line->buildData.index);
            }
        }

        front = HEdge_Create(line, line, line->buildData.v[0],
                             line->buildData.sideDefs[FRONT]->sector, false);

        // Handle the 'One-Sided Window' trick.
        if(!line->buildData.sideDefs[BACK] && line->buildData.windowEffect)
        {
            back = HEdge_Create(((bsp_hedgeinfo_t*) front->data)->lineDef,
                                 line, line->buildData.v[1],
                                 line->buildData.windowEffect, true);
        }
        else
        {
            back = HEdge_Create(line, line, line->buildData.v[1],
                                line->buildData.sideDefs[BACK]? line->buildData.sideDefs[BACK]->sector : NULL, true);
        }

        // Half-edges always maintain a one-to-one relationship
        // with their twins, so if one gets split, the other
        // must be split also.
        back->twin = front;
        front->twin = back;

        BSP_UpdateHEdgeInfo(front);
        BSP_UpdateHEdgeInfo(back);

        BSP_AddHEdgeToSuperBlock(block, front);
        if((line->buildData.sideDefs[BACK] && line->buildData.sideDefs[BACK]->sector) ||
           (!line->buildData.sideDefs[BACK] && line->buildData.windowEffect))
            BSP_AddHEdgeToSuperBlock(block, back);

        // \todo edge tips should be created when half-edges are created.
        {
        double x1 = line->buildData.v[0]->pos[VX];
        double y1 = line->buildData.v[0]->pos[VY];
        double x2 = line->buildData.v[1]->pos[VX];
        double y2 = line->buildData.v[1]->pos[VY];

        BSP_CreateVertexEdgeTip(line->buildData.v[0], x2 - x1, y2 - y1, back, front);
        BSP_CreateVertexEdgeTip(line->buildData.v[1], x1 - x2, y1 - y2, front, back);
        }
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("createInitialHEdges: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

    return block;
}

static boolean C_DECL freeBSPData(binarytree_t *tree, void *data)
{
    void* bspData = BinaryTree_GetData(tree);

    if(bspData)
    {
        if(BinaryTree_IsLeaf(tree))
            BSPLeaf_Destroy(bspData);
        else
            M_Free(bspData);
    }

    BinaryTree_SetData(tree, NULL);

    return true; // Continue iteration.
}

/**
 * Build the BSP for the given map.
 *
 * @param map           The map to build the BSP for.
 * @return              @c true, if completed successfully.
 */
boolean Map_BuildBSP(gamemap_t* map)
{
    boolean builtOK;
    uint startTime;
    superblock_t* hEdgeList;
    binarytree_t* rootNode;

    if(verbose >= 1)
    {
        Con_Message("Map_BuildBSP: Processing map using tunable "
                    "factor of %d...\n", bspFactor);
    }

    // It begins...
    startTime = Sys_GetRealTime();

    BSP_InitSuperBlockAllocator();
    BSP_InitIntersectionAllocator();

    // Create initial half-edges.
    hEdgeList = createInitialHEdges(map);

    // Build the BSP.
    {
    uint buildStartTime = Sys_GetRealTime();
    cutlist_t* cutList;

    cutList = BSP_CutListCreate();

    // Recursively create nodes.
    rootNode = NULL;
    builtOK = BuildNodes(hEdgeList, &rootNode, 0, cutList);

    // The cutlist data is no longer needed.
    BSP_CutListDestroy(cutList);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("BuildNodes: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - buildStartTime) / 1000.0f));
    }

    BSP_SuperBlockDestroy(hEdgeList);

    if(builtOK)
    {   // Success!
        // Wind the BSP tree and save to the map.
        ClockwiseBspTree(rootNode);
        SaveMap(map, rootNode);

        Con_Message("Map_BuildBSP: Built %d Nodes, %d Faces, %d HEdges, %d Vertexes\n",
                    map->numNodes, map->_halfEdgeDS.numFaces, map->_halfEdgeDS.numHEdges,
                    map->_halfEdgeDS.numVertices);

        if(rootNode && !BinaryTree_IsLeaf(rootNode))
        {
            long rHeight, lHeight;

            rHeight = (long)
                BinaryTree_GetHeight(BinaryTree_GetChild(rootNode, RIGHT));
            lHeight = (long)
                BinaryTree_GetHeight(BinaryTree_GetChild(rootNode, LEFT));

            Con_Message("  Balance %+ld (l%ld - r%ld).\n", lHeight - rHeight,
                        lHeight, rHeight);
        }
    }

    // We are finished with the BSP build data.
    if(rootNode)
    {
        BinaryTree_PostOrder(rootNode, freeBSPData, NULL);
        BinaryTree_Destroy(rootNode);
    }
    rootNode = NULL;

    // Free temporary storage.
    BSP_ShutdownIntersectionAllocator();
    BSP_ShutdownSuperBlockAllocator();

    // How much time did we spend?
    VERBOSE(Con_Message("  Done in %.2f seconds.\n",
                        (Sys_GetRealTime() - startTime) / 1000.0f));

    return builtOK;
}
