/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * po_man.h: Polyobjects.
 */

#ifndef __PO_MAN_H__
#define __PO_MAN_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

typedef enum {
    PODOOR_NONE,
    PODOOR_SLIDE,
    PODOOR_SWING,
} podoortype_t;

typedef struct {
    thinker_t       thinker;
    int             polyobj;
    int             intSpeed;
    unsigned int    dist;
    int             fangle;
    float           speed[2]; // for sliding walls
} polyevent_t;

typedef struct {
    thinker_t       thinker;
    int             polyobj;
    int             intSpeed;
    int             dist;
    int             totalDist;
    int             direction;
    float           speed[2];
    int             tics;
    int             waitTics;
    podoortype_t    type;
    boolean         close;
} polydoor_t;

enum {
    PO_ANCHOR_DOOMEDNUM = 3000,
    PO_SPAWN_DOOMEDNUM,
    PO_SPAWNCRUSH_DOOMEDNUM
};

boolean     PO_Busy(int polyobj);

boolean     PO_FindAndCreatePolyobj(int tag, boolean crush, float startX,
                                    float startY);
void        PO_ThrustMobj(struct mobj_s* mo, void* lineDefPtr, void* pop);

void        T_PolyDoor(polydoor_t* pd);
void        T_RotatePoly(polyevent_t* pe);
boolean     EV_RotatePoly(linedef_t* line, byte* args, int direction,
                          boolean overRide);
void        T_MovePoly(polyevent_t* pe);
boolean     EV_MovePoly(linedef_t* line, byte* args, boolean timesEight,
                        boolean overRide);
boolean     EV_OpenPolyDoor(linedef_t* line, byte* args, podoortype_t type);

#endif
