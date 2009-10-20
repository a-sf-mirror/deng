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
 * p_maputil.h: Map Utility Routines
 */

#ifndef __DOOMSDAY_MAP_UTILITIES_H__
#define __DOOMSDAY_MAP_UTILITIES_H__

#include "m_vector.h"
#include "p_object.h"

#define MAXINTERCEPTS   128

extern float  opentop, openbottom, openrange, lowfloor;
extern divline_t traceLOS;

float           P_AccurateDistanceFixed(fixed_t dx, fixed_t dy);
float           P_AccurateDistance(float dx, float dy);
float           P_ApproxDistance(float dx, float dy);
float           P_ApproxDistance3(float dx, float dy, float dz);
void            P_LineUnitVector(linedef_t* line, float* unitvec);
float           P_MobjPointDistancef(mobj_t* start, mobj_t* end,
                                     float* fixpoint);
int             P_PointOnLineDefSide(float x, float y, const linedef_t* line);
int             P_PointOnLineDefSide2(double pointX, double pointY,
                                   double lineDX, double lineDY,
                                   double linePerp, double lineLength,
                                   double epsilon);
int             P_BoxOnLineSide(const float* tmbox, const linedef_t* ld);
int             P_BoxOnLineSide2(float xl, float xh, float yl, float yh,
                                 const linedef_t* ld);
int             P_BoxOnLineSide3(const int bbox[4], double lineSX,
                                 double lineSY, double lineDX, double lineDY,
                                 double linePerp, double lineLength,
                                 double epsilon);
void            P_MakeDivline(const linedef_t* li, divline_t* dl);
int             P_PointOnDivlineSide(float x, float y, const divline_t* line);
float           P_InterceptVector(const divline_t* v2, const divline_t* v1);
int             P_PointOnDivLineSidef(fvertex_t* pnt, fdivline_t* dline);
float           P_FloatInterceptVertex(fvertex_t* start, fvertex_t* end,
                                       fdivline_t* fdiv, fvertex_t* inter);
void            P_LineOpening(linedef_t* linedef);
void            P_MobjLink(mobj_t* mo, byte flags);
int             P_MobjUnlink(mobj_t* mo);

boolean         PIT_AddLineIntercepts(linedef_t* ld, void* data);
boolean         PIT_AddMobjIntercepts(mobj_t* mo, void* data);

boolean         P_MobjLinesIterator(mobj_t* mo,
                                    boolean (*func) (linedef_t*, void*),
                                    void* data);
boolean         P_MobjSectorsIterator(mobj_t* mo,
                                      boolean (*func) (sector_t*, void*),
                                      void* data);
boolean         P_LineMobjsIterator(linedef_t* line,
                                    boolean (*func) (mobj_t*, void*),
                                    void* data);
boolean         P_SectorTouchingMobjsIterator(sector_t* sector,
                                              int (*func) (void*, void*),
                                              void*data);
#endif
