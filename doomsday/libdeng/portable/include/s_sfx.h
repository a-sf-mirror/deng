/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * s_sfx.h: Sound Effects
 */

#ifndef __DOOMSDAY_SOUND_SFX_H__
#define __DOOMSDAY_SOUND_SFX_H__

#include "sys_audiod.h"
#include "sys_audiod_sfx.h"
#include "de_play.h"

// Begin and end macros for Critical Operations. They are operations
// that can't be done while a refresh is being made. No refreshing
// will be done between BEGIN_COP and END_COP.
#define BEGIN_COP       Sfx_AllowRefresh(false)
#define END_COP         Sfx_AllowRefresh(true)

// Channel flags.
#define SFXCF_NO_ORIGIN         (0x1) // Sound is coming from a mystical emitter.
#define SFXCF_NO_ATTENUATION    (0x2) // Sound is very, very loud.
#define SFXCF_NO_UPDATE         (0x4) // Channel update is skipped.

typedef struct sfxchannel_s {
    int             flags;
    sfxbuffer_t*    buffer;
    mobj_t*         emitter; // Mobj that is emitting the sound.
    float           pos[3]; // Emit from here (synced with emitter).
    float           volume; // Sound volume: 1.0 is max.
    float           frequency; // Frequency adjustment: 1.0 is normal.
    int             startTime; // When was the channel last started?
} sfxchannel_t;

extern boolean sfxAvail;
extern float sfxReverbStrength;
extern int sfxMaxCacheKB, sfxMaxCacheTics;
extern int sfxBits, sfxRate;
extern int sfx3D, sfx16Bit, sfxSampleRate;

boolean         Sfx_Init(void);
void            Sfx_Shutdown(void);
void            Sfx_Reset(void);
void            Sfx_AllowRefresh(boolean allow);
void            Sfx_MapChange(void);
void            Sfx_StartFrame(void);
void            Sfx_EndFrame(void);
void            Sfx_PurgeCache(void);
void            Sfx_RefreshChannels(void);
int             Sfx_StartSound(sfxsample_t* sample, float volume, float freq,
                               mobj_t* emitter, float* fixedpos, int flags);
int             Sfx_StopSound(int id, mobj_t* emitter);
void            Sfx_StopSoundGroup(int group, mobj_t* emitter);
int             Sfx_CountPlaying(int id);
void            Sfx_UnloadSoundID(int id);

void            Sfx_DebugInfo(void);

#endif
