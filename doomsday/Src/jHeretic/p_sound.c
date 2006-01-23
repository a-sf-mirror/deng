
//**************************************************************************
//**
//** P_SOUND.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "jHeretic/Doomdef.h"
#include "jHeretic/h_stat.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gsvMapMusic;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Start the song for the current map.
 */
void S_LevelMusic(void)
{
    ddmapinfo_t info;
    char    id[10];
    int    songid = 0;

    if(gamestate != GS_LEVEL)
        return;

    sprintf(id, "E%iM%i", gameepisode, gamemap);
    if(Def_Get(DD_DEF_MAP_INFO, id, &info) && info.music >= 0)
    {
        songid = info.music;
        S_StartMusicNum(songid, true);
    }
    else
    {
        songid = (gameepisode - 1) * 9 + gamemap - 1;
        S_StartMusicNum(songid, true);
    }

    // set the map music game status cvar
    gsvMapMusic = songid;
}

/*
 * Doom-like sector sounds: when a new sound starts, stop any old ones
 * from the same origin.
 */
void S_SectorSound(sector_t *sec, int id)
{
    mobj_t *origin = (mobj_t *) P_GetPtrp(sec, DMU_SOUND_ORIGIN);

    S_StopSound(0, origin);
    S_StartSound(id, origin);
}
