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
// $Log$
// Revision 1.1.2.1  2004/05/16 10:01:36  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.1.4.1  2003/11/19 17:07:12  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.1  2003/02/26 19:21:37  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:45  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//	Put all global tate variables here.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id$";


#ifdef __GNUG__
#pragma implementation "doomstat.h"
#endif
#include "doomstat.h"


// Game Mode - identify IWAD as shareware, retail etc.
GameMode_t		gamemode = indetermined;
GameMission_t	gamemission = doom;

// Language.
Language_t		language = english;

// Set if homebrew PWAD stuff has been added.
boolean			modifiedgame;

boolean			monsterinfight;





