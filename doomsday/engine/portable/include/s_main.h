/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * s_main.h: Sound Subsystem
 */

#ifndef __DOOMSDAY_SOUND_MAIN_H__
#define __DOOMSDAY_SOUND_MAIN_H__

#include "con_decl.h"
#include "p_object.h"
#include "def_main.h"
#include "sys_audiod.h"

#define SF_RANDOM_SHIFT     0x1    // Random frequency shift.
#define SF_RANDOM_SHIFT2    0x2    // 2x bigger random frequency shift.
#define SF_GLOBAL_EXCLUDE   0x4    // Exclude all emitters.
#define SF_NO_ATTENUATION   0x8    // Very, very loud...
#define SF_REPEAT           0x10   // Repeats until stopped.
#define SF_DONT_STOP        0x20   // Sound can't be stopped while playing.

extern int      showSoundInfo;
extern int      soundMinDist, soundMaxDist;
extern int      sfxVolume, musVolume;

extern audiodriver_t* audioDriver;

void            S_Register(void);
boolean         S_Init(void);
void            S_Shutdown(void);
void            S_MapChange(void);
void            S_Reset(void);
void            S_StartFrame(void);
void            S_EndFrame(void);
sfxinfo_t*      S_GetSoundInfo(int sound_id, float* freq, float* volume);
mobj_t*         S_GetListenerMobj(void);
int             S_LocalSoundAtVolumeFrom(int sound_id, mobj_t* origin,
                                         float* fixedpos, float volume);
int             S_LocalSoundAtVolume(int sound_id, mobj_t* origin,
                                     float volume);
int             S_LocalSound(int sound_id, mobj_t* origin);
int             S_LocalSoundFrom(int sound_id, float* fixedpos);
int             S_StartSound(int soundId, mobj_t* origin);
int             S_StartSoundEx(int soundId, mobj_t* origin);
int             S_StartSoundAtVolume(int sound_id, mobj_t* origin,
                                     float volume);
int             S_ConsoleSound(int sound_id, mobj_t* origin,
                               int target_console);
void            S_StopSound(int sound_id, mobj_t* origin);
int             S_IsPlaying(int sound_id, mobj_t* emitter);
int             S_StartMusic(const char* musicid, boolean looped);
void            S_StopMusic(void);
void            S_PauseMusic(boolean paused);
void            S_Drawer(void);

#endif
