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

// SECTORS do store MObjs anyway.
#include "p_mobj.h"

#include "p_xg.h"

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
#if 0
    fixed_t         floorheight;
    fixed_t         ceilingheight;
    short           floorpic;
    short           ceilingpic;
    short           lightlevel;
    byte            rgb[3];
    byte    floorrgb[3];
    byte    ceilingrgb[3];

    // if == validcount, already checked
    int             Validcount;

    // list of mobjs in sector
    mobj_t         *thinglist;

    int             linecount;
    struct line_s **Lines;         // [linecount] size

    float           flooroffx, flooroffy;   // floor texture offset
    float           ceiloffx, ceiloffy; // ceiling texture offset

    int             skyfix;        // Offset to ceiling height
    // rendering w/sky.
    float           reverb[NUM_REVERB_DATA];

    // mapblock bounding box for height changes
    int             blockbox[4];

    plane_t         planes[2];     // PLN_*

    degenmobj_t     soundorg;      // for any sounds played by the sector
#endif
    // --- Don't change anything above ---

    short           special;
    short           tag;

    // 0 = untraversed, 1,2 = sndlines -1
    int             soundtraversed;

    // thing that made a sound (or null)
    mobj_t         *soundtarget;

    // thinker_t for reversable actions
    void           *specialdata;

    // stone, metal, heavy, etc...
    /*seqtype_t*/ byte       seqType;       // NOT USED ATM

    int             origfloor, origceiling, origlight;
    byte            origrgb[3];
    xgsector_t     *xg;

} xsector_t;

//
// The SideDef.
//
#if 0
typedef struct side_s {
    // add this to the calculated texture column
    fixed_t         textureoffset;

    // add this to the calculated texture top
    fixed_t         rowoffset;

    // Texture indices.
    // We do not maintain names here.
    short           toptexture;
    short           bottomtexture;
    short           midtexture;

    byte    toprgb[3], bottomrgb[3], midrgba[4];
    blendmode_t    blendmode; // blending mode
    int          flags;

    // Sector the SideDef is facing.
    sector_t       *sector;

    // --- Don't change anything above ---

} side_t;
#endif

typedef struct xline_s {
#if 0
    // Vertices, from v1 to v2.
    vertex_t       *v1;
    vertex_t       *v2;

    short           flags;

    // Front and back sector.
    // Note: redundant? Can be retrieved from SideDefs.
    sector_t       *frontsector;
    sector_t       *backsector;

    // Precalculated v2 - v1 for side checking.
    fixed_t         dx;
    fixed_t         dy;

    // To aid move clipping.
    slopetype_t     slopetype;

    // if == validcount, already checked
    int             Validcount;

    // Visual appearance: SideDefs.
    //  sidenum[1] will be 0xffff if one sided
    int             sidenum[2];

    fixed_t         bbox[4];

    // --- Don't change anything above ---
#endif
    // Animation related.
    short           special;
    short           tag;
    byte            arg1;
    byte            arg2;
    byte            arg3;
    byte            arg4;
    byte            arg5;
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

int P_HandleMapDataElement(int id, int dtype, int prop, int type, void *data);

#endif
