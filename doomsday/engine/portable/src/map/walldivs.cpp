/**
 * @file walldivs.cpp
 * Map wall divisions. @ingroup map
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

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include <WallDivs>
#include <de/Log>

walldivs_t* WallDivNode_WallDivs(walldivnode_t* node)
{
    Q_ASSERT(node);
    return node->divs;
}

coord_t WallDivNode_Height(walldivnode_t* node)
{
    Q_ASSERT(node);
    switch(node->type)
    {
    case DT_PLANEINTERCEPT:     return node->data.intercept.plane->visHeight;
    case DT_ABSOLUTEHEIGHT:     return node->data.absoluteHeight;
    default:
        Con_Error("WallDivNode::Height: Internal error, invalid node type %i.", (int)node->type);
        exit(0); // Unreachable.
    }
}

walldivnode_t* WallDivNode_Next(walldivnode_t* node)
{
    Q_ASSERT(node);
    uint idx = node - node->divs->nodes;
    if(idx+1 >= node->divs->num) return 0;
    return &node->divs->nodes[idx+1];
}

walldivnode_t* WallDivNode_Prev(walldivnode_t* node)
{
    Q_ASSERT(node);
    uint idx = node - node->divs->nodes;
    if(idx == 0) return 0;
    return &node->divs->nodes[idx-1];
}

uint WallDivs_Size(const walldivs_t* wd)
{
    Q_ASSERT(wd);
    return wd->num;
}

walldivnode_t* WallDivs_First(walldivs_t* wd)
{
    Q_ASSERT(wd);
    return &wd->nodes[0];
}

walldivnode_t* WallDivs_Last(walldivs_t* wd)
{
    Q_ASSERT(wd);
    return &wd->nodes[wd->num-1];
}

walldivnode_t* WallDivs_AtHeight(walldivs_t* wd, coord_t height)
{
    Q_ASSERT(wd);
    for(uint i = 0; i < wd->num; ++i)
    {
        walldivnode_t* node = &wd->nodes[i];
        if(WallDivNode_Height(node) == height)
            return node;
    }
    return NULL;
}

walldivs_t* WallDivs_AppendPlaneIntercept(walldivs_t* wd, Plane* plane, HEdge* hedge)
{
    Q_ASSERT(wd);
    struct walldivnode_s* node = &wd->nodes[wd->num++];
    node->divs = wd;
    node->type = DT_PLANEINTERCEPT;
    node->data.intercept.plane = plane;
    node->data.intercept.hedge = hedge;
    return wd;
}

walldivs_t* WallDivs_AppendAbsoluteHeight(walldivs_t* wd, coord_t height)
{
    Q_ASSERT(wd);
    struct walldivnode_s* node = &wd->nodes[wd->num++];
    node->divs = wd;
    node->type = DT_ABSOLUTEHEIGHT;
    node->data.absoluteHeight = height;
    return wd;
}

void WallDivs_AssertSorted(walldivs_t* wd)
{
#if _DEBUG
    walldivnode_t* node = WallDivs_First(wd);
    coord_t highest = WallDivNode_Height(node);
    for(uint i = 0; i < wd->num; ++i, node = WallDivNode_Next(node))
    {
        coord_t height = WallDivNode_Height(node);
        Q_ASSERT(height >= highest);
        highest = height;
    }
#else
    DENG_UNUSED(wd);
#endif
}

void WallDivs_AssertInRange(walldivs_t* wd, coord_t low, coord_t hi)
{
#if _DEBUG
    Q_ASSERT(wd);
    walldivnode_t* node = WallDivs_First(wd);
    for(uint i = 0; i < wd->num; ++i, node = WallDivNode_Next(node))
    {
        coord_t height = WallDivNode_Height(node);
        Q_ASSERT(height >= low && height <= hi);
    }
#else
    DENG_UNUSED(wd);
    DENG_UNUSED(low);
    DENG_UNUSED(hi);
#endif
}

#if _DEBUG
void WallDivs_DebugPrint(walldivs_t* wd)
{
    Q_ASSERT(wd);
    LOG_DEBUG("WallDivs [%p]:") << wd;
    for(uint i = 0; i < wd->num; ++i)
    {
        walldivnode_t* node = &wd->nodes[i];
        LOG_DEBUG("  %i: %f") << i << WallDivNode_Height(node);
    }
}
#endif
