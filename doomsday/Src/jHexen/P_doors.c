
//**************************************************************************
//**
//** p_doors.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

#include "h2def.h"
#include "jHexen/p_local.h"
#include "jHexen/soundst.h"

//==================================================================
//==================================================================
//
//                                                      VERTICAL DOORS
//
//==================================================================
//==================================================================

//==================================================================
//
//      T_VerticalDoor
//
//==================================================================
void T_VerticalDoor(vldoor_t * door)
{
#ifdef TODO_MAP_UPDATE
	result_e res;

	switch (door->direction)
	{
	case 0:					// WAITING
		if(!--door->topcountdown)
			switch (door->type)
			{
			case DREV_NORMAL:
				door->direction = -1;	// time to go back down
				SN_StartSequence((mobj_t *) &door->sector->soundorg,
								 SEQ_DOOR_STONE + door->sector->seqType);
				break;
			case DREV_CLOSE30THENOPEN:
				door->direction = 1;
				break;
			default:
				break;
			}
		break;
	case 2:					// INITIAL WAIT
		if(!--door->topcountdown)
		{
			switch (door->type)
			{
			case DREV_RAISEIN5MINS:
				door->direction = 1;
				door->type = DREV_NORMAL;
				break;
			default:
				break;
			}
		}
		break;
	case -1:					// DOWN
		res =
			T_MovePlane(door->sector, door->speed, door->sector->floorheight,
						false, 1, door->direction);
		if(res == RES_PASTDEST)
		{
			SN_StopSequence((mobj_t *) &door->sector->soundorg);
			switch (door->type)
			{
			case DREV_NORMAL:
			case DREV_CLOSE:
				door->sector->specialdata = NULL;
				P_TagFinished(door->sector->tag);
				P_RemoveThinker(&door->thinker);	// unlink and free
				break;
			case DREV_CLOSE30THENOPEN:
				door->direction = 0;
				door->topcountdown = 35 * 30;
				break;
			default:
				break;
			}
		}
		else if(res == RES_CRUSHED)
		{
			switch (door->type)
			{
			case DREV_CLOSE:	// DON'T GO BACK UP!
				break;
			default:
				door->direction = 1;
				break;
			}
		}
		break;
	case 1:					// UP
		res =
			T_MovePlane(door->sector, door->speed, door->topheight, false, 1,
						door->direction);
		if(res == RES_PASTDEST)
		{
			SN_StopSequence((mobj_t *) &door->sector->soundorg);
			switch (door->type)
			{
			case DREV_NORMAL:
				door->direction = 0;	// wait at top
				door->topcountdown = door->topwait;
				break;
			case DREV_CLOSE30THENOPEN:
			case DREV_OPEN:
				door->sector->specialdata = NULL;
				P_TagFinished(door->sector->tag);
				P_RemoveThinker(&door->thinker);	// unlink and free
				break;
			default:
				break;
			}
		}
		break;
	}
#endif
}

//----------------------------------------------------------------------------
//
// EV_DoDoor
//
// Move a door up/down
//
//----------------------------------------------------------------------------

int EV_DoDoor(line_t *line, byte *args, vldoor_e type)
{
	int     secnum;
	int     retcode;
	sector_t *sec;
	vldoor_t *door;
	fixed_t speed;

	speed = args[1] * FRACUNIT / 8;
	secnum = -1;
	retcode = 0;
	while((secnum = P_FindSectorFromTag(args[0], secnum)) >= 0)
	{
#ifdef TODO_MAP_UPDATe
		sec = &sectors[secnum];
		if(sec->specialdata)
		{
			continue;
		}
#endif
		// Add new door thinker
		retcode = 1;
		door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
		P_AddThinker(&door->thinker);
#ifdef TODO_MAP_UPDATE
		sec->specialdata = door;
#endif
		door->thinker.function = T_VerticalDoor;
		door->sector = sec;
		switch (type)
		{
		case DREV_CLOSE:
			door->topheight = P_FindLowestCeilingSurrounding(sec);
			door->topheight -= 4 * FRACUNIT;
			door->direction = -1;
			break;
		case DREV_CLOSE30THENOPEN:
#ifdef TODO_MAP_UPDATE
			door->topheight = sec->ceilingheight;
#endif
			door->direction = -1;
			break;
		case DREV_NORMAL:
		case DREV_OPEN:
			door->direction = 1;
			door->topheight = P_FindLowestCeilingSurrounding(sec);
			door->topheight -= 4 * FRACUNIT;
			break;
		default:
			break;
		}
		door->type = type;
		door->speed = speed;
		door->topwait = args[2];	// line->arg3
#ifdef TODO_MAP_UPDATE
		SN_StartSequence((mobj_t *) &door->sector->soundorg,
						 SEQ_DOOR_STONE + door->sector->seqType);
#endif
	}
	return (retcode);
}

//==================================================================
//
//      EV_VerticalDoor : open a door manually, no tag value
//
//==================================================================
boolean EV_VerticalDoor(line_t *line, mobj_t *thing)
{
	int     secnum;
	sector_t *sec;
	vldoor_t *door;
	int     side;

	side = 0;					// only front sides can be used

	// if the sector has an active thinker, use it
#ifdef TODO_MAP_UPDATE
	sec = sides[line->sidenum[side ^ 1]].sector;
	secnum = sec - sectors;
	if(sec->specialdata)
	{
		return false;
		/*
		   door = sec->specialdata;
		   switch(line->special)
		   {    // only for raise doors
		   case 12:
		   if(door->direction == -1)
		   {
		   door->direction = 1; // go back up
		   }
		   else
		   {
		   if(!thing->player)
		   { // Monsters don't close doors
		   return;
		   }
		   door->direction = -1; // start going down immediately
		   }
		   return;
		   }
		 */
	}
#endif
	//
	// new door thinker
	//
	door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
	P_AddThinker(&door->thinker);
#ifdef TODO_MAP_UPDATE
	sec->specialdata = door;
#endif
	door->thinker.function = T_VerticalDoor;
	door->sector = sec;
	door->direction = 1;
#ifdef TODO_MAP_UPDATE
	switch (line->special)
	{
	case 11:
		door->type = DREV_OPEN;
		line->special = 0;
		break;
	case 12:
	case 13:
		door->type = DREV_NORMAL;
		break;
	default:
		door->type = DREV_NORMAL;
		break;
	}
	door->speed = line->arg2 * (FRACUNIT / 8);
	door->topwait = line->arg3;
#endif

	//
	// find the top and bottom of the movement range
	//
	door->topheight = P_FindLowestCeilingSurrounding(sec);
	door->topheight -= 4 * FRACUNIT;
#ifdef TODO_MAP_UPDATE
	SN_StartSequence((mobj_t *) &door->sector->soundorg,
					 SEQ_DOOR_STONE + door->sector->seqType);
#endif
	return true;
}

//==================================================================
//
//      Spawn a door that closes after 30 seconds
//
//==================================================================

/*
   void P_SpawnDoorCloseIn30(sector_t *sec)
   {
   vldoor_t *door;

   door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
   P_AddThinker(&door->thinker);
   sec->specialdata = door;
   sec->special = 0;
   door->thinker.function = T_VerticalDoor;
   door->sector = sec;
   door->direction = 0;
   door->type = DREV_NORMAL;
   door->speed = VDOORSPEED;
   door->topcountdown = 30*35;
   }
 */

//==================================================================
//
//      Spawn a door that opens after 5 minutes
//
//==================================================================

/*
   void P_SpawnDoorRaiseIn5Mins(sector_t *sec, int secnum)
   {
   vldoor_t *door;

   door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
   P_AddThinker(&door->thinker);
   sec->specialdata = door;
   sec->special = 0;
   door->thinker.function = T_VerticalDoor;
   door->sector = sec;
   door->direction = 2;
   door->type = DREV_RAISEIN5MINS;
   door->speed = VDOORSPEED;
   door->topheight = P_FindLowestCeilingSurrounding(sec);
   door->topheight -= 4*FRACUNIT;
   door->topwait = VDOORWAIT;
   door->topcountdown = 5*60*35;
   }
 */
