// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//      Refresh/rendering module, shared data struct definitions.
//
//-----------------------------------------------------------------------------

#ifndef __R_DEFS__
#define __R_DEFS__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// Screenwidth.
#include "doomdef.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
#include "d_think.h"

#include "p_xg.h"

// SECTORS do store MObjs anyway.
#include "p_mobj.h"

#ifdef __GNUG__
#pragma interface
#endif

// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
#define SIL_NONE        0
#define SIL_BOTTOM      1
#define SIL_TOP         2
#define SIL_BOTH        3

//
// INTERNAL MAP TYPES
//  used by play and refresh
//

// Forward of LineDefs, for Sectors.
struct line_s;
//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
typedef struct xsector_s {
    short           special;
    short           tag;

    // 0 = untraversed, 1,2 = sndlines -1
    int             soundtraversed;

    // thing that made a sound (or null)
    struct mobj_s  *soundtarget;

    // thinker_t for reversable actions
    void           *specialdata;

    // stone, metal, heavy, etc...
    /*seqtype_t*/ byte       seqType;       // NOT USED ATM

    int             origfloor, origceiling, origlight;
    byte            origrgb[3];
    xgsector_t     *xg;

} xsector_t;

typedef struct xline_s {
    // Animation related.
    short           special;
    short           tag;
    // thinker_t for reversable actions
    void           *specialdata;

    // Extended generalized lines.
    xgline_t       *xg;
} xline_t;

extern xsector_t *xsectors;
extern xline_t *xlines;

void P_SetupForThings(int num);
void P_SetupForLines(int num);
void P_SetupForSides(int num);
void P_SetupForSectors(int num);

int P_HandleMapDataProperty(int id, int dtype, int prop, int type, void *data);

#endif
