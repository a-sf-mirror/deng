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
//  all external data is defined here
//  most of the data is loaded into different structures at run time
//  some internal structures shared by many modules are here
//
//-----------------------------------------------------------------------------

#ifndef __DOOMDATA__
#define __DOOMDATA__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// The most basic types we use, portability.
#include "doomtype.h"

// Some global defines, that configure the game.
#include "doomdef.h"

//
// Map level types.
// The following data structures define the persistent format
// used in the lumps of the WAD files.
//

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum {
    ML_LABEL,                      // A separator, name, ExMx or MAPxx
    ML_THINGS,                     // Monsters, items..
    ML_LINEDEFS,                   // LineDefs, from editing
    ML_SIDEDEFS,                   // SideDefs, from editing
    ML_VERTEXES,                   // Vertices, edited and BSP splits generated
    ML_SEGS,                       // LineSegs, from LineDefs split by BSP
    ML_SSECTORS,                   // SubSectors, list of LineSegs
    ML_NODES,                      // BSP nodes
    ML_SECTORS,                    // Sectors, from editing
    ML_REJECT,                     // LUT, sector-sector visibility
    ML_BLOCKMAP,                       // LUT, motion clipping, walls/grid element
    ML_BEHAVIOR        // ACS Scripts (not supported currently)
};

//
// LineDef attributes.
//

// Solid, is an obstacle.
#define ML_BLOCKING     1

// Blocks monsters only.
#define ML_BLOCKMONSTERS    2

// Backside will not be present at all
//  if not two sided.
#define ML_TWOSIDED     4

// If a texture is pegged, the texture will have
// the end exposed to air held constant at the
// top or bottom of the texture (stairs or pulled
// down things) and will move with a height change
// of one of the neighbor sectors.
// Unpegged textures allways have the first row of
// the texture at the top pixel of the line for both
// top and bottom textures (use next to windows).

// upper texture unpegged
#define ML_DONTPEGTOP       8

// lower texture unpegged
#define ML_DONTPEGBOTTOM    16

// In AutoMap: don't map as two sided: IT'S A SECRET!
#define ML_SECRET       32

// Sound rendering: don't let sound cross two of these.
#define ML_SOUNDBLOCK       64

// Don't draw on the automap at all.
#define ML_DONTDRAW     128

// Set if already seen, thus drawn in automap.
#define ML_MAPPED       256

// Allows a USE action to pass through a line with a special
#define ML_PASSUSE    512

//If set allows any mobj to trigger the line's special
#define ML_ALLTRIGGER 1024

//
// Thing attributes.
//

// Appears in Easy skill modes
#define MTF_EASY    1

// Appears in Medium skill modes
#define MTF_MEDIUM    2

// Appears in Hard skill modes
#define MTF_HARD    4

// THING is deaf
#define MTF_DEAF    8

// Appears in Multiplayer game modes only
#define MTF_NOTSINGLE    16

// Doesn't appear in Deathmatch
#define MTF_NOTDM    32

// Doesn't appear in Coop
#define MTF_NOTCOOP    64

// THING is invulnerble and inert
#define MTF_DORMANT    512

// BSP node structure.

// Indicate a leaf.
#define NF_SUBSECTOR    0x8000

// This is the common thing_t
typedef struct thing_s {
    short           x;
    short           y;
    short           height;
    short           angle;
    short           type;
    short           options;
} thing_t;

extern thing_t* things;

#endif
