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
//	Sky rendering.
//
//-----------------------------------------------------------------------------


#ifndef __R_SKY__
#define __R_SKY__


#ifdef __GNUG__
#pragma interface
#endif

// SKY, store the number for name.
#define			SKYFLATNAME  "F_SKY1"

// The sky map is 256*128*4 maps.
#define			ANGLETOSKYSHIFT		22

extern  int		skytexture;
extern int		skytexturemid;

// Called whenever the view size changes.
void R_InitSkyMap (void);

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.2.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.2  2003/02/27 23:14:31  skyjake
// Obsolete jDoom files removed
//
// Revision 1.1  2003/02/26 19:18:43  skyjake
// Initial checkin
//
//
//-----------------------------------------------------------------------------

