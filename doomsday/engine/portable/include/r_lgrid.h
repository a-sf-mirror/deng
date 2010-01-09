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

/**
 * r_lgrid.h: Ambient world lighting (smoothed sector lighting).
 */

#ifndef DOOMSDAY_REFRESH_LIGHT_GRID_H
#define DOOMSDAY_REFRESH_LIGHT_GRID_H

/**
 * @defGroup gridBlockFlags Grid Block Flags
 */
/*{*/
#define GBF_CHANGED     0x1     // Grid block sector light has changed.
#define GBF_CONTRIBUTOR 0x2     // Contributes light to a changed block.
/*}*/

typedef struct lgridblock_s {
    sector_t*       sector; // @see gridBlockFlags
    byte            flags;
    char            bias; // Positive bias means that the light is shining in the floor of the sector.
    float           rgb[3]; // Color of the light in this block.
    // Used instead of rgb if the lighting in this block has changed and we
    // have not yet done a full grid update.
    float           oldRGB[3];
} lgridblock_t;

typedef struct lightgrid_s {
    boolean         inited, needsUpdate;
    float           origin[3];
    int             blockWidth, blockHeight;
    lgridblock_t*   grid;
} lightgrid_t;

void            R_RegisterLightGrid(void);

lightgrid_t*    P_CreateLightGrid(void);
void            P_DestroyLightGrid(lightgrid_t* lg);

void            LightGrid_Init(struct map_s* map);
void            LightGrid_Update(lightgrid_t* lg);
void            LightGrid_MarkForUpdate(lightgrid_t* lg, const unsigned short* blocks,
                                        unsigned int changedBlockCount, unsigned int blockCount);
void            LightGrid_Evaluate(lightgrid_t* lg, const float* point, float* destColor);

void            Rend_LightGridVisual(lightgrid_t* lg);

#endif /* DOOMSDAY_REFRESH_LIGHT_GRID_H */
