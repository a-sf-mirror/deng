/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DOOMSDAY_MAP_H
#define DOOMSDAY_MAP_H

#if defined(__JDOOM__) || defined(__JHERETIC__) || defined(__JHEXEN__)
#  error "Attempted to include internal Doomsday map.h from a game"
#endif

#include "dd_share.h"
#include "m_vector.h"
#include "halfedgeds.h"
#include "mobjblockmap.h"
#include "linedefblockmap.h"
#include "subsectorblockmap.h"
#include "particleblockmap.h"
#include "lumobjblockmap.h"
#include "m_binarytree.h"
#include "gameobjrecords.h"
#include "p_maptypes.h"
#include "m_nodepile.h"
#include "p_polyob.h"
#include "rend_bias.h"
#include "p_think.h"
#include "p_particle.h"
#include "r_lgrid.h"

// Return the index of plane within a sector's planes array.
#define GET_PLANE_IDX(pln)  ((pln) - (pln)->sector->planes[0])

typedef struct fvertex_s {
    float           pos[2];
} fvertex_t;

/**
 * Stores the data pertaining to vertex lighting for a worldmap, surface.
 */
typedef struct biassurface_s {
    uint            updated;
    uint            size;
    vertexillum_t*  illum; // [size]
    biastracker_t   tracker;
    biasaffection_t affected[MAX_BIAS_AFFECTED];

    struct biassurface_s* next;
} biassurface_t;

typedef struct dynlist_s {
    struct dynlistnode_s* head, *tail;
} dynlist_t;

#define MAP_SIZE            gx.mapSize

extern int bspFactor;

void            P_MapRegister(void);

map_t*          P_CreateMap(const char* mapID);
void            P_DestroyMap(map_t* map);

boolean         Map_Load(map_t* map);
void            Map_Precache(map_t* map);

lightgrid_t*    Map_LightGrid(map_t* map);

#endif /* DOOMSDAY_MAP_H */
