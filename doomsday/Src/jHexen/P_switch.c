
//**************************************************************************
//**
//** p_switch.c : Heretic 2 : Raven Software, Corp.
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
//
//      CHANGE THE TEXTURE OF A WALL SWITCH TO ITS OPPOSITE
//
//==================================================================
switchlist_t switchInfo[] = {
	{"SW_1_UP", "SW_1_DN", SFX_SWITCH1}
	,
	{"SW_2_UP", "SW_2_DN", SFX_SWITCH1}
	,
	{"VALVE1", "VALVE2", SFX_VALVE_TURN}
	,
	{"SW51_OFF", "SW51_ON", SFX_SWITCH2}
	,
	{"SW52_OFF", "SW52_ON", SFX_SWITCH2}
	,
	{"SW53_UP", "SW53_DN", SFX_ROPE_PULL}
	,
	{"PUZZLE5", "PUZZLE9", SFX_SWITCH1}
	,
	{"PUZZLE6", "PUZZLE10", SFX_SWITCH1}
	,
	{"PUZZLE7", "PUZZLE11", SFX_SWITCH1}
	,
	{"PUZZLE8", "PUZZLE12", SFX_SWITCH1}
	,
	{"\0", "\0", 0}
};

int     switchlist[MAXSWITCHES * 2];
int     numswitches;
button_t buttonlist[MAXBUTTONS];

/*
   ===============
   =
   = P_InitSwitchList
   =
   = Only called at game initialization
   =
   ===============
 */

void P_InitSwitchList(void)
{
	int     i;
	int     index;

	for(index = 0, i = 0; i < MAXSWITCHES; i++)
	{
		if(!switchInfo[i].soundID)
		{
			numswitches = index / 2;
			switchlist[index] = -1;
			break;
		}
		switchlist[index++] = R_CheckTextureNumForName(switchInfo[i].name1);
		switchlist[index++] = R_CheckTextureNumForName(switchInfo[i].name2);
	}
}

//==================================================================
//
//      Start a button counting down till it turns off.
//
//==================================================================
void P_StartButton(line_t *line, bwhere_e w, int texture, int time)
{
	int     i;

	for(i = 0; i < MAXBUTTONS; i++)
	{
		if(!buttonlist[i].btimer)
		{
			buttonlist[i].line = line;
			buttonlist[i].where = w;
			buttonlist[i].btexture = texture;
			buttonlist[i].btimer = time;
			buttonlist[i].soundorg = P_GetPtrp(
                                               P_GetPtrp(line, DMU_FRONT_SECTOR), 
                                               DMU_SOUND_ORIGIN);
			return;
		}
	}
	Con_Error("P_StartButton: no button slots left!");
}

//==================================================================
//
//      Function that changes wall texture.
//      Tell it if switch is ok to use again (1=yes, it's a button).
//
//==================================================================
void P_ChangeSwitchTexture(line_t *line, int useAgain)
{
	int         texTop;
	int         texMid;
	int         texBot;
	int         i;
    side_t*     sdef = P_GetPtrp(line, DMU_SIDE0);
    sector_t*   frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);

	texTop = P_GetIntp(sdef, DMU_TOP_TEXTURE);
	texMid = P_GetIntp(sdef, DMU_MIDDLE_TEXTURE);
	texBot = P_GetIntp(sdef, DMU_BOTTOM_TEXTURE);

	for(i = 0; i < numswitches * 2; i++)
	{
		if(switchlist[i] == texTop)
		{
			S_StartSound(switchInfo[i / 2].soundID,
						 P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));
			P_SetIntp(sdef, DMU_TOP_TEXTURE, switchlist[i ^ 1]);
			if(useAgain)
			{
				P_StartButton(line, SWTCH_TOP, switchlist[i], BUTTONTIME);
			}
			return;
		}
		else if(switchlist[i] == texMid)
		{
			S_StartSound(switchInfo[i / 2].soundID,
						 P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));
			P_SetIntp(sdef, DMU_MIDDLE_TEXTURE, switchlist[i ^ 1]);
			if(useAgain)
			{
				P_StartButton(line, SWTCH_MIDDLE, switchlist[i], BUTTONTIME);
			}
			return;
		}
		else if(switchlist[i] == texBot)
		{
			S_StartSound(switchInfo[i / 2].soundID,
						 P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));
			P_SetIntp(sdef, DMU_BOTTOM_TEXTURE, switchlist[i ^ 1]);
			if(useAgain)
			{
				P_StartButton(line, SWTCH_BOTTOM, switchlist[i], BUTTONTIME);
			}
			return;
		}
	}
}
