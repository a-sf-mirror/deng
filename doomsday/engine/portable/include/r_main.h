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
 * r_main.h: Refresh Subsystem
 */

#ifndef __DOOMSDAY_REFRESH_MAIN_H__
#define __DOOMSDAY_REFRESH_MAIN_H__

#include <math.h>

typedef struct viewport_s {
    int             console;
    int             x, y, width, height;
} viewport_t;

typedef struct {
    thinker_t   thinker;
    struct surface_s* suf;
    int         tics;
} matfader_t;

extern float viewX, viewY, viewZ;
extern float viewFrontVec[3], viewUpVec[3], viewSideVec[3];
extern float viewXOffset, viewYOffset, viewZOffset;
extern angle_t viewAngle;
extern float viewPitch;
extern angle_t clipAngle;
extern fixed_t fineTangent[FINEANGLES / 2];
extern float vx, vy, vz, vang, vpitch, fieldOfView, yfov;
extern float viewsidex, viewsidey;

extern float frameTimePos;      // 0...1: fractional part for sharp game tics
extern boolean resyncFrameTimePos;
extern int loadInStartupMode;
extern int alidCount;
extern int viewwidth, viewheight, viewwindowx, viewwindowy;
extern boolean setSizeNeeded;
extern int frameCount;
extern int validCount;
extern int viewAngleOffset;
extern int torchAdditive;
extern float torchColor[];
extern int extraLight;
extern float extraLightDelta;
extern float viewCos, viewSin;
extern int rendInfoTris;

void            R_Register(void);
void            R_Init(void);
void            R_Update(void);
void            R_Shutdown(void);
void            R_BeginWorldFrame(struct map_s* map);
void            R_EndWorldFrame(struct map_s* map);
void            R_RenderPlayerView(int num);
void            R_RenderPlayerViewBorder(void);
void            R_RenderViewPorts(void);
void            R_ResetViewer(void);
void            R_SetViewWindow(int x, int y, int w, int h);
void            R_NewSharpWorld(void);

void            R_SetViewGrid(int numCols, int numRows);
void            R_SetViewWindow(int x, int y, int w, int h);

float           R_FacingViewerDot(struct vertex_s* from, struct vertex_s* to);
void            R_ColorApplyTorchLight(float* color, float distance);

#define R_PointDist2D(c) (fabs((vz-c[VY])*viewsidex - (vx-c[VX])*viewsidey))
float           R_PointDist3D(const float c[3]);

void            R_ApplyLightAdaptation(float* lightvalue);
float           R_LightAdaptationDelta(float lightvalue);
#endif
