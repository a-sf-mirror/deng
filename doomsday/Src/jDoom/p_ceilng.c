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
// Revision 1.8.2.1  2005/11/27 17:42:08  skyjake
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
// Revision 1.8  2005/05/29 12:45:09  danij
// Removed fixed limits on number of active plats/ceilings using modified code from PrBoom.
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
// Revision 1.1.4.1  2003/11/19 17:07:12  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.1  2003/02/26 19:21:53  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:46  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:  Ceiling aninmation (lowering, crushing, raising)
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

//
// CEILINGS
//

ceilinglist_t *activeceilings;

//
// T_MoveCeiling
//

void T_MoveCeiling(ceiling_t * ceiling)
{
	result_e res;

	switch (ceiling->direction)
	{
	case 0:
		// IN STASIS
		break;
	case 1:
		// UP
		res =
			T_MovePlane(ceiling->sector, ceiling->speed, ceiling->topheight,
						false, 1, ceiling->direction);

		if(!(leveltime & 7))
		{
			switch (ceiling->type)
			{
			case silentCrushAndRaise:
				break;
			default:
				//S_StartSound((mobj_t*)&ceiling->sector->soundorg, sfx_stnmov);
				S_SectorSound(ceiling->sector, sfx_stnmov);
				break;
			}
		}
		/*if(ceiling->type != silentCrushAndRaise)
		   {
		   gi.Sv_PlaneSound(ceiling->sector, true, sfx_stnmov, 7);
		   } */

		if(res == pastdest)
		{
			switch (ceiling->type)
			{
			case raiseToHighest:
				P_RemoveActiveCeiling(ceiling);
				break;

			case silentCrushAndRaise:
				S_SectorSound(ceiling->sector, sfx_pstop);
			case fastCrushAndRaise:
			case crushAndRaise:
				ceiling->direction = -1;
				break;

			default:
				break;
			}

		}
		break;

	case -1:
		// DOWN
		res =
			T_MovePlane(ceiling->sector, ceiling->speed, ceiling->bottomheight,
						ceiling->crush, 1, ceiling->direction);

		if(!(leveltime & 7))
		{
			switch (ceiling->type)
			{
			case silentCrushAndRaise:
				break;
			default:
				S_SectorSound(ceiling->sector, sfx_stnmov);
			}
		}
		/*if(ceiling->type != silentCrushAndRaise)
		   {
		   gi.Sv_PlaneSound(ceiling->sector, true, sfx_stnmov, 7);
		   } */

		if(res == pastdest)
		{
			switch (ceiling->type)
			{
			case silentCrushAndRaise:
				S_SectorSound(ceiling->sector, sfx_pstop);
			case crushAndRaise:
				ceiling->speed = CEILSPEED;
			case fastCrushAndRaise:
				ceiling->direction = 1;
				break;

			case lowerAndCrush:
			case lowerToFloor:
				P_RemoveActiveCeiling(ceiling);
				break;

			default:
				break;
			}
		}
		else					// ( res != pastdest )
		{
			if(res == crushed)
			{
				switch (ceiling->type)
				{
				case silentCrushAndRaise:
				case crushAndRaise:
				case lowerAndCrush:
					ceiling->speed = CEILSPEED / 8;
					break;

				default:
					break;
				}
			}
		}
		break;
	}
}

//
// EV_DoCeiling
// Move a ceiling up/down and all around!
//
int EV_DoCeiling(line_t *line, ceiling_e type)
{
	int     secnum;
	int     rtn;
	sector_t *sec;
	ceiling_t *ceiling;

	secnum = -1;
	rtn = 0;

	//  Reactivate in-stasis ceilings...for certain types.
	switch (type)
	{
	case fastCrushAndRaise:
	case silentCrushAndRaise:
	case crushAndRaise:
		P_ActivateInStasisCeiling(line);
	default:
		break;
	}

	while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
	{
#ifdef TODO_MAP_UPDATE
		sec = &sectors[secnum];
#endif
		if(sec->specialdata)
			continue;

		// new door thinker
		rtn = 1;
		ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVSPEC, 0);
		P_AddThinker(&ceiling->thinker);
		sec->specialdata = ceiling;
		ceiling->thinker.function = T_MoveCeiling;
		ceiling->sector = sec;
		ceiling->crush = false;

		switch (type)
		{
		case fastCrushAndRaise:
			ceiling->crush = true;
			ceiling->topheight = sec->ceilingheight;
			ceiling->bottomheight = sec->floorheight + (8 * FRACUNIT);
			ceiling->direction = -1;
			ceiling->speed = CEILSPEED * 2;
			break;

		case silentCrushAndRaise:
		case crushAndRaise:
			ceiling->crush = true;
			ceiling->topheight = sec->ceilingheight;
		case lowerAndCrush:
		case lowerToFloor:
			ceiling->bottomheight = sec->floorheight;
			if(type != lowerToFloor)
				ceiling->bottomheight += 8 * FRACUNIT;
			ceiling->direction = -1;
			ceiling->speed = CEILSPEED;
			break;

		case raiseToHighest:
			ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
			ceiling->direction = 1;
			ceiling->speed = CEILSPEED;
			break;
		}

		ceiling->tag = sec->tag;
		ceiling->type = type;
		P_AddActiveCeiling(ceiling);
	}
	return rtn;
}

//
// P_AddActiveCeiling()
//
// Adds a ceiling to the head of the list of active ceilings
//
// Passed the ceiling motion structure
// Returns nothing
//
void P_AddActiveCeiling(ceiling_t* ceiling)
{
  ceilinglist_t *list = malloc(sizeof *list);
  list->ceiling = ceiling;
  ceiling->list = list;
  if ((list->next = activeceilings))
    list->next->prev = &list->next;
  list->prev = &activeceilings;
  activeceilings = list;
}

//
// P_RemoveActiveCeiling()
//
// Removes a ceiling from the list of active ceilings
//
// Passed the ceiling motion structure
// Returns nothing
//
void P_RemoveActiveCeiling(ceiling_t* ceiling)
{
  ceilinglist_t *list = ceiling->list;
  ceiling->sector->specialdata = NULL;  //jff 2/22/98
  P_RemoveThinker(&ceiling->thinker);
  if ((*list->prev = list->next))
    list->next->prev = list->prev;
  free(list);
}

//
// P_RemoveAllActiveCeilings()
//
// Removes all ceilings from the active ceiling list
//
// Passed nothing, returns nothing
//
void P_RemoveAllActiveCeilings(void)
{
  while (activeceilings)
  {
    ceilinglist_t *next = activeceilings->next;
    free(activeceilings);
    activeceilings = next;
  }
}

//
// P_ActivateInStasisCeiling()
//
// Reactivates all stopped crushers with the right tag
//
// Passed the line reactivating the crusher
// Returns true if a ceiling reactivated
//
//jff 4/5/98 return if activated
int P_ActivateInStasisCeiling(line_t *line)
{
  ceilinglist_t *cl;
  int rtn=0;

  for (cl=activeceilings; cl; cl=cl->next)
  {
    ceiling_t *ceiling = cl->ceiling;
    if (ceiling->tag == line->tag && ceiling->direction == 0)
    {
		ceiling->direction = ceiling->olddirection;
		ceiling->thinker.function	 = (actionf_p1) T_MoveCeiling;
		//jff 4/5/98 return if activated
        rtn=1;
    }
  }
  return rtn;
}

//
// EV_CeilingCrushStop()
//
// Stops all active ceilings with the right tag
//
// Passed the linedef stopping the ceilings
// Returns true if a ceiling put in stasis
//
int EV_CeilingCrushStop(line_t* line)
{
  int rtn=0;

  ceilinglist_t *cl;
  for (cl=activeceilings; cl; cl=cl->next)
  {
    ceiling_t *ceiling = cl->ceiling;
    if (ceiling->direction != 0 && ceiling->tag == line->tag)
    {
      ceiling->olddirection = ceiling->direction;
      ceiling->direction = 0;
      ceiling->thinker.function = NULL;
      rtn=1;
    }
  }
  return rtn;
}
