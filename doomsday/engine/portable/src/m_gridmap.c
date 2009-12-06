/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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
 * m_gridmap.c: Generalized blockmap
 */

// HEADER FILES ------------------------------------------------------------

#include "math.h"

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct gmap_s {
    uint            width, height;
    int             zoneTag;
    void**          blockLinks;
} gmap_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static gmap_t* allocGridmap(int tag)
{
    return Z_Calloc(sizeof(gmap_t), tag, 0);
}

static void freeGridmap(gmap_t* gmap)
{
    Z_Free(gmap);
}

static void* blockLinks(gmap_t* gmap, uint x, uint y)
{
    if(x < gmap->width && y < gmap->height)
    {
        return gmap->blockLinks[y * gmap->width + x];
    }

    return NULL;
}

/**
 * Create a new gridmap.
 *
 * @param width         X dimension of the grid.
 * @param height        Y dimension of the grid.
 */
gridmap_t* M_CreateGridmap(uint width, uint height, int zoneTag)
{
    gmap_t* gmap = allocGridmap(zoneTag);

    gmap->width = width;
    gmap->height = height;
    gmap->zoneTag = zoneTag;
    gmap->blockLinks = Z_Calloc(gmap->width * gmap->height * sizeof(void*), gmap->zoneTag, 0);

    return (gridmap_t*) gmap;
}

/**
 * Destroy a gridmap.
 *
 * @param gridmap       The gridmap to be destroyed.
 */
void M_DestroyGridmap(gridmap_t* gridmap)
{
    gmap_t* gmap;

    assert(gridmap);

    gmap = (gmap_t*) gridmap;

    Z_Free(gmap->blockLinks);
    freeGridmap(gmap);
}

/**
 * Empty the gridmap. Only zeros the block links.
 */
void Gridmap_Empty(gridmap_t* gridmap)
{
    assert(gridmap);

    {
    gmap_t* gmap = (gmap_t*) gridmap;
    memset(gmap->blockLinks, 0, gmap->width * gmap->height * sizeof(void*));
    }
}

uint Gridmap_Width(gridmap_t* gridmap)
{
    assert(gridmap);
    return ((gmap_t*) gridmap)->width;
}

uint Gridmap_Height(gridmap_t* gridmap)
{
    assert(gridmap);

    return ((gmap_t*) gridmap)->height;
}

void Gridmap_Dimensions(gridmap_t* gridmap, uint dimensions[2])
{
    gmap_t* gmap;

    assert(gridmap);
    assert(dimensions);

    gmap = (gmap_t*) gridmap;
    dimensions[0] = gmap->width;
    dimensions[1] = gmap->height;
}

void* Gridmap_Block(gridmap_t* gridmap, uint x, uint y)
{
    gmap_t* gmap;

    assert(gridmap);

    gmap = (gmap_t*) gridmap;
    if(x < gmap->width && y < gmap->height)
    {
        return gmap->blockLinks[y * gmap->width + x];
    }

    return NULL;
}

void* Gridmap_SetBlock(gridmap_t* gridmap, uint x, uint y, void* data)
{
    gmap_t* gmap;

    assert(gridmap);

    gmap = (gmap_t*) gridmap;
    if(x < gmap->width && y < gmap->height)
    {
        gmap->blockLinks[y * gmap->width + x] = data;
    }

    return data;
}

/**
 * Iterate all the blocks of the gridmap, calling func for each.
 *
 * @param gridmap       The gridmap being iterated.
 * @param callback      The callback to be made for each block.
 * @param context       Miscellaneous data to be passed in the callback, can be @c NULL.
 *
 * @return              @c true, iff all callbacks return @c true;
 */
boolean Gridmap_Iterate(gridmap_t* gridmap, boolean (*callback) (void* p, void* ctx),
                        void* context)
{
    gmap_t* gmap;
    uint x, y;

    assert(gridmap);
    assert(callback);

    gmap = (gmap_t*) gridmap;
    for(x = 0; x <= gmap->width; ++x)
    {
        for(y = 0; y <= gmap->height; ++y)
        {
            void* data = blockLinks(gmap, x, y);

            if(data)
            {
                if(!callback(data, context))
                    return false;
            }
        }
    }
    return true;
}

/**
 * Iterate a subset of the blocks of the gridmap and calling func for each.
 *
 * @param gridmap       The gridmap being iterated.
 * @param xl            Min X
 * @param xh            Max X
 * @param yl            Min Y
 * @param yh            Max Y
 * @param callback      The callback to be made for each block.
 * @param context       Miscellaneous data to be passed in the callback, can be @c NULL.
 *
 * @return              @c true, iff all callbacks return @c true;
 */
boolean Gridmap_IterateBox(gridmap_t* gridmap, uint xl, uint xh, uint yl, uint yh,
                           boolean (*callback) (void* p, void* ctx),
                           void* context)
{
    gmap_t* gmap;
    uint x, y;

    assert(gridmap);
    assert(callback);

    gmap = (gmap_t*) gridmap;
    if(xh >= gmap->width)
        xh = gmap->width -1;
    if(yh >= gmap->height)
        yh = gmap->height - 1;

    for(x = xl; x <= xh; ++x)
    {
        for(y = yl; y <= yh; ++y)
        {
            void* data = blockLinks(gmap, x, y);

            if(data)
            {
                if(!callback(data, context))
                    return false;
            }
        }
    }
    return true;
}

boolean Gridmap_IterateBoxv(gridmap_t* gridmap, const uint box[4],
                              boolean (*callback) (void* p, void* ctx),
                              void* param)
{
    return Gridmap_IterateBox(gridmap, box[BOXLEFT], box[BOXRIGHT],
                              box[BOXBOTTOM], box[BOXTOP], callback,
                              param);
}
