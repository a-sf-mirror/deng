#ifndef __COMMON_PLAYSTART_H__
#define __COMMON_PLAYSTART_H__

#define MAXSTARTS	100

extern mapthing_t playerstarts[MAXSTARTS], *playerstart_p;

void P_RegisterPlayerStart(mapthing_t *mthing);
void P_DealPlayerStarts(void);
boolean P_CheckSpot(int playernum, mapthing_t* mthing);
boolean P_FuzzySpawn(mapthing_t *spot, int playernum);
void P_SpawnPlayers(void);

#if __JHEXEN__
mapthing_t *P_GetPlayerStart(int group, int pnum);
#endif

#endif