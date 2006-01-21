#ifndef __COMMON_PLAYSTART_H__
#define __COMMON_PLAYSTART_H__

#define MAXSTARTS   100

extern thing_t playerstarts[MAXSTARTS], *playerstart_p;

void            P_Init(void);
void            P_RegisterPlayerStart(thing_t * mthing);
boolean         P_CheckSpot(int playernum, thing_t * mthing,
                            boolean doTeleSpark);
boolean         P_FuzzySpawn(thing_t * spot, int playernum,
                             boolean doTeleSpark);
thing_t        *P_GetPlayerStart(int group, int pnum);

void            P_DealPlayerStarts(void);

void            P_SpawnPlayers(void);

void            P_GetMapLumpName(int episode, int map, char *lumpName);
void            P_LocateMapLumps(int episode, int map, int *lumpIndices);

#endif
