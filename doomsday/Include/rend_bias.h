/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Ker�nen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * rend_bias.h: Light/Shadow Bias
 */

#ifndef __DOOMSDAY_RENDER_SHADOW_BIAS_H__
#define __DOOMSDAY_RENDER_SHADOW_BIAS_H__

#include "p_data.h"

#define MAX_BIAS_LIGHTS   (8 * 32) // Hard limit due to change tracking.
#define MAX_BIAS_AFFECTED 6

typedef struct biastracker_s {
    unsigned int changes[MAX_BIAS_LIGHTS / 32];
} biastracker_t;

struct vertexillum_s;
struct rendpoly_s;

extern int      useBias; // Bias lighting enabled.

void            SB_Register(void);
void            SB_InitForLevel(const char *uniqueId);
void            SB_SegHasMoved(seg_t *seg);
void            SB_PlaneHasMoved(subsector_t *subsector, boolean theCeiling);
void            SB_BeginFrame(void);
void            SB_RendPoly(struct rendpoly_s *poly, boolean isFloor,
                            sector_t *sector,
                            struct vertexillum_s *illumination,
                            biastracker_t *tracker, int mapElementIndex);
void            SB_EndFrame(void);

void            SBE_DrawCursor(void);
void            SBE_DrawHUD(void);

#endif
