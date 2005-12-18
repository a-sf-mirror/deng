/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Ker�nen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "doomstat.h"
#include "s_sound.h"

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

int S_GetMusicNum(int episode, int map)
{
    int     mnum;

    if(gamemode == commercial)
        mnum = mus_runnin + map - 1;
    else
    {
        int     spmus[] = {
            // Song - Who? - Where?
            mus_e3m4,           // American     e4m1
            mus_e3m2,           // Romero       e4m2
            mus_e3m3,           // Shawn        e4m3
            mus_e1m5,           // American     e4m4
            mus_e2m7,           // Tim          e4m5
            mus_e2m4,           // Romero       e4m6
            mus_e2m6,           // J.Anderson   e4m7 CHIRON.WAD
            mus_e2m5,           // Shawn        e4m8
            mus_e1m9            // Tim          e4m9
        };

        if(episode < 4)
            mnum = mus_e1m1 + (episode - 1) * 9 + map - 1;
        else
            mnum = spmus[map - 1];
    }
    return mnum;
}

/*
 * Starts playing the music for this level
 */
void S_LevelMusic(void)
{
    int songid;

    if(gamestate != GS_LEVEL)
        return;

    // Start new music for the level.
    if(Get(DD_MAP_MUSIC) == -1)
    {
        songid = S_GetMusicNum(gameepisode, gamemap);
        S_StartMusicNum(songid, true);
    }
    else
    {
        songid = Get(DD_MAP_MUSIC);
        S_StartMusicNum(songid, true);
    }

    // set the game status cvar for the map music
    gsvMapMusic = songid;
}

/*
 * Doom-like sector sounds: when a new sound starts, stop any old ones
 * from the same origin.
 */
void S_SectorSound(sector_t *sec, int id)
{
#ifdef TODO_MAP_UPDATE
    mobj_t *origin = (mobj_t *) &sec->soundorg;

    S_StopSound(0, origin);
    S_StartSound(id, origin);
#endif
}
