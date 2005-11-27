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
// Revision 1.7.2.1  2005/11/27 17:42:08  skyjake
// Breaking everything with the new Map Update API (=DMU) (only declared, not
// implemented yet)
//
// - jDoom and jHeretic do not compile
// - jHexen compiles by virtue of #ifdef TODO_MAP_UPDATE, which removes all the
// portions of code that would not compile
// - none of the games work, because DMU has not been implemented or used in any
// of the games
//
// Map data is now hidden from the games. The line_t, seg_t and structs are
// defined only as void*. The functions in the Map Update API (P_Set*, P_Get*,
// P_Callback, P_ToPtr, P_ToIndex) are used for reading and writing the map data
// parameters. There are multiple versions of each function so that using them is
// more convenient in terms of data types.
//
// P_Callback can be used for having the engine call a callback function for each
// of the selected map data objects.
//
// The API is not finalized yet.
//
// The DMU_* constants defined in dd_share.h are used as the 'type' and 'prop'
// parameters.
//
// The games require map data in numerous places of the code. All of these should
// be converted to work with DMU in the most sensible fashion (a direct
// conversion may not always make the most sense). E.g., jHexen has
// some private map data for sound playing, etc. The placement of this data is
// not certain at the moment, but it can remain private to the games if
// necessary.
//
// Games can build their own map changing routines on DMU as they see fit. The
// engine will only provide a generic API, as defined in doomsday.h currently.
//
// Revision 1.7  2005/01/01 22:58:52  skyjake
// Resolved a bunch of compiler warnings
//
// Revision 1.6  2004/05/30 08:42:41  skyjake
// Tweaked indentation style
//
// Revision 1.5  2004/05/29 09:53:29  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.4  2004/05/28 19:52:58  skyjake
// Finished switch from branch-1-7 to trunk, hopefully everything is fine
//
// Revision 1.1.2.1  2004/05/16 10:01:36  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.1.4.1  2003/11/19 17:07:13  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.1  2003/02/26 19:21:54  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:46  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//  Handle Sector base lighting effects.
//  Muzzle flash?
//
//-----------------------------------------------------------------------------

#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"

// State.
#include "r_state.h"

//
// FIRELIGHT FLICKER
//

//
// T_FireFlicker
//
void T_FireFlicker(fireflicker_t * flick)
{
	int     amount;

	if(--flick->count)
		return;

	amount = (P_Random() & 3) * 16;

	if(flick->sector->lightlevel - amount < flick->minlight)
		flick->sector->lightlevel = flick->minlight;
	else
		flick->sector->lightlevel = flick->maxlight - amount;

	flick->count = 4;
}

//
// P_SpawnFireFlicker
//
void P_SpawnFireFlicker(sector_t *sector)
{
	fireflicker_t *flick;

	// Note that we are resetting sector attributes.
	// Nothing special about it during gameplay.
	sector->special = 0;

	flick = Z_Malloc(sizeof(*flick), PU_LEVSPEC, 0);

	P_AddThinker(&flick->thinker);

	flick->thinker.function = T_FireFlicker;
	flick->sector = sector;
	flick->maxlight = sector->lightlevel;
	flick->minlight =
		P_FindMinSurroundingLight(sector, sector->lightlevel) + 16;
	flick->count = 4;
}

//
// BROKEN LIGHT FLASHING
//

//
// T_LightFlash
// Do flashing lights.
//
void T_LightFlash(lightflash_t * flash)
{
	if(--flash->count)
		return;

	if(flash->sector->lightlevel == flash->maxlight)
	{
		flash->sector->lightlevel = flash->minlight;
		flash->count = (P_Random() & flash->mintime) + 1;
	}
	else
	{
		flash->sector->lightlevel = flash->maxlight;
		flash->count = (P_Random() & flash->maxtime) + 1;
	}

}

//
// P_SpawnLightFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void P_SpawnLightFlash(sector_t *sector)
{
	lightflash_t *flash;

	// nothing special about it during gameplay
	sector->special = 0;

	flash = Z_Malloc(sizeof(*flash), PU_LEVSPEC, 0);

	P_AddThinker(&flash->thinker);

	flash->thinker.function = T_LightFlash;
	flash->sector = sector;
	flash->maxlight = sector->lightlevel;

	flash->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);
	flash->maxtime = 64;
	flash->mintime = 7;
	flash->count = (P_Random() & flash->maxtime) + 1;
}

//
// STROBE LIGHT FLASHING
//

//
// T_StrobeFlash
//
void T_StrobeFlash(strobe_t * flash)
{
	if(--flash->count)
		return;

	if(flash->sector->lightlevel == flash->minlight)
	{
		flash->sector->lightlevel = flash->maxlight;
		flash->count = flash->brighttime;
	}
	else
	{
		flash->sector->lightlevel = flash->minlight;
		flash->count = flash->darktime;
	}

}

//
// P_SpawnStrobeFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void P_SpawnStrobeFlash(sector_t *sector, int fastOrSlow, int inSync)
{
	strobe_t *flash;

	flash = Z_Malloc(sizeof(*flash), PU_LEVSPEC, 0);

	P_AddThinker(&flash->thinker);

	flash->sector = sector;
	flash->darktime = fastOrSlow;
	flash->brighttime = STROBEBRIGHT;
	flash->thinker.function = T_StrobeFlash;
	flash->maxlight = sector->lightlevel;
	flash->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);

	if(flash->minlight == flash->maxlight)
		flash->minlight = 0;

	// nothing special about it during gameplay
	sector->special = 0;

	if(!inSync)
		flash->count = (P_Random() & 7) + 1;
	else
		flash->count = 1;
}

//
// Start strobing lights (usually from a trigger)
//
void EV_StartLightStrobing(line_t *line)
{
	int     secnum;
	sector_t *sec;

	secnum = -1;
	while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
	{
#ifdef TODO_MAP_UPDATE
		sec = &sectors[secnum];
#endif
		if(sec->specialdata)
			continue;

		P_SpawnStrobeFlash(sec, SLOWDARK, 0);
	}
}

//
// TURN LINE'S TAG LIGHTS OFF
//
void EV_TurnTagLightsOff(line_t *line)
{
	int     i;
	int     j;
	int     min;
	sector_t *sector;
	sector_t *tsec;
	line_t *templine;

#ifdef TODO_MAP_UPDATE
	sector = sectors;

	for(j = 0; j < numsectors; j++, sector++)
	{
		if(sector->tag == line->tag)
		{
			min = sector->lightlevel;
			for(i = 0; i < sector->linecount; i++)
			{
				templine = sector->Lines[i];
				tsec = getNextSector(templine, sector);
				if(!tsec)
					continue;
				if(tsec->lightlevel < min)
					min = tsec->lightlevel;
			}
			sector->lightlevel = min;
			//gi.Sv_LightReport(sector);
		}
	}
#endif
}

//
// TURN LINE'S TAG LIGHTS ON
//
void EV_LightTurnOn(line_t *line, int bright)
{
	int     i;
	int     j;
	sector_t *sector;
	sector_t *temp;
	line_t *templine;

#ifdef TODO_MAP_UPDATE
	sector = sectors;

	for(i = 0; i < numsectors; i++, sector++)
	{
		if(sector->tag == line->tag)
		{
			// bright = 0 means to search
			// for highest light level
			// surrounding sector
			if(!bright)
			{
				for(j = 0; j < sector->linecount; j++)
				{
					templine = sector->Lines[j];
					temp = getNextSector(templine, sector);

					if(!temp)
						continue;

					if(temp->lightlevel > bright)
						bright = temp->lightlevel;
				}
			}
			sector->lightlevel = bright;
			//gi.Sv_LightReport(sector);
		}
	}
#endif
}

//
// Spawn glowing light
//

void T_Glow(glow_t * g)
{
	switch (g->direction)
	{
	case -1:
		// DOWN
		g->sector->lightlevel -= GLOWSPEED;
		if(g->sector->lightlevel <= g->minlight)
		{
			g->sector->lightlevel += GLOWSPEED;
			g->direction = 1;
		}
		break;

	case 1:
		// UP
		g->sector->lightlevel += GLOWSPEED;
		if(g->sector->lightlevel >= g->maxlight)
		{
			g->sector->lightlevel -= GLOWSPEED;
			g->direction = -1;
		}
		break;
	}
}

void P_SpawnGlowingLight(sector_t *sector)
{
	glow_t *g;

	g = Z_Malloc(sizeof(*g), PU_LEVSPEC, 0);

	P_AddThinker(&g->thinker);

	g->sector = sector;
	g->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);
	g->maxlight = sector->lightlevel;
	g->thinker.function = T_Glow;
	g->direction = -1;

	sector->special = 0;
}
