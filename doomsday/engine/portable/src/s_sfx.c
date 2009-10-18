/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * s_sfx.c: Sound Effects
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"
#include "sys_audio.h"

// MACROS ------------------------------------------------------------------

#define SFX_MAX_CHANNELS        (64)
#define SFX_LOWEST_PRIORITY     (-1000)
#define UPDATE_TIME             (2.0/TICSPERSEC) // Seconds.

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void            Sfx_3DMode(boolean activate);
void            Sfx_SampleFormat(int newBits, int newRate);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// SFX playback functions loaded from a sound driver plugin.
extern audiodriver_t audiodExternal;
extern audiointerface_sfx_t audiodExternalISFX;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean sfxAvail = false;

int sfxMaxChannels = 16;
int sfxDedicated2D = 4;
float sfxReverbStrength = 1;
int sfxBits = 8;
int sfxRate = 11025;

// Console variables:
int sfx3D = false;
int sfx16Bit = false;
int sfxSampleRate = 11025;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// The interfaces.
static audiointerface_sfx_generic_t* iSFX = NULL;

static int numChannels = 0;
static sfxchannel_t* channels;
static mobj_t* listener;
static sector_t* listenerSector = NULL;

static thread_t refreshHandle;
static volatile boolean allowRefresh, refreshing;

static byte refMonitor = 0;

// CODE --------------------------------------------------------------------

/**
 * This is a high-priority thread that periodically checks if the channels
 * need to be updated with more data. The thread terminates when it notices
 * that the channels have been destroyed. The Sfx audioDriver maintains a 250ms
 * buffer for each channel, which means the refresh must be done often
 * enough to keep them filled.
 *
 * \fixme Use a real mutex, will you?
 */
int C_DECL Sfx_ChannelRefreshThread(void* parm)
{
    int                 i;
    sfxchannel_t*       ch;

    // We'll continue looping until the Sfx module is shut down.
    while(sfxAvail && channels)
    {
        // The bit is swapped on each refresh (debug info).
        refMonitor ^= 1;

        if(allowRefresh)
        {
            // Do the refresh.
            refreshing = true;
            for(i = 0, ch = channels; i < numChannels; ++i, ch++)
            {
                if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING))
                    continue;

                iSFX->Refresh(ch->buffer);
            }

            refreshing = false;
            // Let's take a nap.
            Sys_Sleep(200);
        }
        else
        {
            // Refreshing is not allowed, so take a shorter nap while
            // waiting for allowRefresh.
            Sys_Sleep(150);
        }
    }

    // Time to end this thread.
    return 0;
}

/**
 * Enabling refresh is simple: the refresh thread is resumed. When
 * disabling refresh, first make sure a new refresh doesn't begin (using
 * allowRefresh). We still have to see if a refresh is being made and wait
 * for it to stop. Then we can suspend the refresh thread.
 */
void Sfx_AllowRefresh(boolean allow)
{
    if(!sfxAvail)
        return;
    if(allowRefresh == allow)
        return; // No change.

    allowRefresh = allow;

    // If we're denying refresh, let's make sure that if it's currently
    // running, we don't continue until it has stopped.
    if(!allow)
    {
        while(refreshing)
            Sys_Sleep(0);
    }

    // Sys_SuspendThread(refreshHandle, !allow);
}

/**
 * Stop all sounds of the group. If an emitter is specified, only it's
 * sounds are checked.
 */
void Sfx_StopSoundGroup(int group, mobj_t* emitter)
{
    int                 i;
    sfxchannel_t*       ch;

    if(!sfxAvail)
        return;

    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING) ||
           ch->buffer->sample->group != group || (emitter &&
                                                  ch->emitter != emitter))
            continue;

        // This channel must stop.
        iSFX->Stop(ch->buffer);
    }
}

/**
 * Stops all channels that are playing the specified sound.
 *
 * @param id            @c 0 = all sounds are stopped.
 * @param emitter       If not @c NULL, then the channel's emitter mobj
 *                      must match it.
 *
 * @return              The number of samples stopped.
 */
int Sfx_StopSound(int id, mobj_t* emitter)
{
    int                 i, stopCount = 0;
    sfxchannel_t*       ch;

    if(!sfxAvail)
        return false;

    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING) ||
           (id && ch->buffer->sample->id != id) || (emitter &&
                                                    ch->emitter != emitter))
            continue;

        // Can it be stopped?
        if(ch->buffer->flags & SFXBF_DONT_STOP)
        {
            // The emitter might get destroyed...
            ch->emitter = NULL;
            ch->flags |= SFXCF_NO_UPDATE | SFXCF_NO_ORIGIN;
            continue;
        }

        // This channel must be stopped!
        iSFX->Stop(ch->buffer);
        ++stopCount;
    }

    return stopCount;
}

#if 0 // Currently unused.
int Sfx_IsPlaying(int id, mobj_t* emitter)
{
    int                 i;
    sfxchannel_t*       ch;

    if(!sfxAvail)
        return false;

    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING) ||
           ch->emitter != emitter || id && ch->buffer->sample->id != id)
            continue;

        // Once playing, repeating sounds don't stop.
        if(ch->buffer->flags & SFXBF_REPEAT) return true;

        // Check time. The flag is updated after a slight delay
        // (only at refresh).
        if(Sys_GetTime() - ch->starttime <
           ch->buffer->sample->numsamples / (float) ch->buffer->freq *
            TICSPERSEC) return true;
    }

    return false;
}
#endif

/**
 * The specified sample will soon no longer exist. All channel buffers
 * loaded with the sample will be reset.
 */
void Sfx_UnloadSoundID(int id)
{
    int                 i;
    sfxchannel_t*       ch;

    if(!sfxAvail)
        return;

    BEGIN_COP;

    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || !ch->buffer->sample ||
           ch->buffer->sample->id != id)
            continue;

        // Stop and unload.
        iSFX->Reset(ch->buffer);
    }

    END_COP;
}

/**
 * @return              The number of channels the sound is playing on.
 */
int Sfx_CountPlaying(int id)
{
    int                 i = 0, count = 0;
    sfxchannel_t*       ch = channels;

    if(!sfxAvail)
        return 0;

    for(; i < numChannels; i++, ch++)
    {
        if(!ch->buffer || !ch->buffer->sample ||
           ch->buffer->sample->id != id ||
           !(ch->buffer->flags & SFXBF_PLAYING))
            continue;

        count++;
    }

    return count;
}

/**
 * The priority of a sound is affected by distance, volume and age.
 */
float Sfx_Priority(mobj_t* emitter, float* fixPos, float volume,
                   int startTic)
{
    // In five seconds all priority of a sound is gone.
    float               timeoff =
        1000 * (Sys_GetTime() - startTic) / (5.0f * TICSPERSEC);
    float               orig[3];

    if(!listener || (!emitter && !fixPos))
    {   // The sound does not have an origin.
        return 1000 * volume - timeoff;
    }

    // The sound has an origin, base the points on distance.
    if(emitter)
    {
        memcpy(orig, emitter->pos, sizeof(orig));
    }
    else
    {
        // No emitter mobj, use the fixed source position.
        memcpy(orig, fixPos, sizeof(orig));
    }

    return 1000 * volume - P_MobjPointDistancef(listener, 0, orig) / 2 -
        timeoff;
}

/**
 * Calculate priority points for a sound playing on a channel.
 * They are used to determine which sounds can be cancelled by new sounds.
 * Zero is the lowest priority.
 */
float Sfx_ChannelPriority(sfxchannel_t* ch)
{
    if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING))
        return SFX_LOWEST_PRIORITY;

    if(ch->flags & SFXCF_NO_ORIGIN)
        return Sfx_Priority(0, 0, ch->volume, ch->startTime);

    // ch->pos is set to emitter->xyz during updates.
    return Sfx_Priority(0, ch->pos, ch->volume, ch->startTime);
}

/**
 * @return              The actual 3D coordinates of the listener.
 */
void Sfx_GetListenerXYZ(float* pos)
{
    if(!listener)
        return;

    // \fixme Make it exactly eye-level! (viewheight).
    pos[VX] = listener->pos[VX];
    pos[VY] = listener->pos[VY];
    pos[VZ] = listener->pos[VZ] + listener->height - 5;
}

/**
 * Updates the channel buffer's properties based on 2D/3D position
 * calculations. Listener might be NULL. Sounds emitted from the listener
 * object are considered to be inside the listener's head.
 */
void Sfx_ChannelUpdate(sfxchannel_t* ch)
{
    sfxbuffer_t*        buf = ch->buffer;
    float               normdist, dist, pan, angle, vec[3];

    if(!buf || (ch->flags & SFXCF_NO_UPDATE))
        return;

    // Copy the emitter's position (if any), to the pos coord array.
    if(ch->emitter)
    {
        ch->pos[VX] = ch->emitter->pos[VX];
        ch->pos[VY] = ch->emitter->pos[VY];
        ch->pos[VZ] = ch->emitter->pos[VZ];

        // If this is a mobj, center the Z pos.
        if(P_IsMobjThinker(&ch->emitter->thinker, NULL))
        {
            // Sounds originate from the center.
            ch->pos[VZ] += ch->emitter->height / 2;
        }
    }

    // Frequency is common to both 2D and 3D sounds.
    iSFX->Set(buf, SFXBP_FREQUENCY, ch->frequency);

    if(buf->flags & SFXBF_3D)
    {
        // Volume is affected only by maxvol.
        iSFX->Set(buf, SFXBP_VOLUME, ch->volume * sfxVolume / 255.0f);
        if(ch->emitter && ch->emitter == listener)
        {
            // Emitted by the listener object. Go to relative position mode
            // and set the position to (0,0,0).
            vec[VX] = vec[VY] = vec[VZ] = 0;
            iSFX->Set(buf, SFXBP_RELATIVE_MODE, true);
            iSFX->Setv(buf, SFXBP_POSITION, vec);
        }
        else
        {
            // Use the channel's real position.
            iSFX->Set(buf, SFXBP_RELATIVE_MODE, false);
            iSFX->Setv(buf, SFXBP_POSITION, ch->pos);
        }

        // If the sound is emitted by the listener, speed is zero.
        if(ch->emitter && ch->emitter != listener &&
           P_IsMobjThinker(&ch->emitter->thinker, NULL))
        {
            vec[VX] = ch->emitter->mom[MX] * TICSPERSEC;
            vec[VY] = ch->emitter->mom[MY] * TICSPERSEC;
            vec[VZ] = ch->emitter->mom[MZ] * TICSPERSEC;
            iSFX->Setv(buf, SFXBP_VELOCITY, vec);
        }
        else
        {
            // Not moving.
            vec[VX] = vec[VY] = vec[VZ] = 0;
            iSFX->Setv(buf, SFXBP_VELOCITY, vec);
        }
    }
    else
    {   // This is a 2D buffer.
        if((ch->flags & SFXCF_NO_ORIGIN) ||
           (ch->emitter && ch->emitter == listener))
        {
            dist = 1;
            pan = 0;
        }
        else
        {
            // Calculate roll-off attenuation. [.125/(.125+x), x=0..1]
            dist = P_MobjPointDistancef(listener, 0, ch->pos);
            if(dist < soundMinDist || (ch->flags & SFXCF_NO_ATTENUATION))
            {
                // No distance attenuation.
                dist = 1;
            }
            else if(dist > soundMaxDist)
            {
                // Can't be heard.
                dist = 0;
            }
            else
            {
                normdist = (dist - soundMinDist) /
                    (soundMaxDist - soundMinDist);

                // Apply the linear factor so that at max distance there
                // really is silence.
                dist = .125f / (.125f + normdist) * (1 - normdist);
            }

            // And pan, too. Calculate angle from listener to emitter.
            if(listener)
            {
                angle =
                    (R_PointToAngle2(listener->pos[VX],
                                     listener->pos[VY],
                                     ch->pos[VX],
                                     ch->pos[VY]) -
                     listener->angle) / (float) ANGLE_MAX *360;
                // We want a signed angle.
                if(angle > 180)
                    angle -= 360;
                // Front half.
                if(angle <= 90 && angle >= -90)
                {
                    pan = -angle / 90;
                }
                else
                {
                    // Back half.
                    pan = (angle + (angle > 0 ? -180 : 180)) / 90;
                    // Dampen sounds coming from behind.
                    dist *= (1 + (pan > 0 ? pan : -pan)) / 2;
                }
            }
            else
            {
                // No listener mobj? Can't pan, then.
                pan = 0;
            }
        }

        iSFX->Set(buf, SFXBP_VOLUME,
                    ch->volume * dist * sfxVolume / 255.0f);
        iSFX->Set(buf, SFXBP_PAN, pan);
    }
}

void Sfx_ListenerUpdate(void)
{
    int i;
    float vec[4];

    // No volume means no sound.
    if(!sfxAvail || !sfx3D || !sfxVolume)
        return;

    // Update the listener mobj.
    listener = S_GetListenerMobj();

    if(listener)
    {
        subsector_t* subSector = (subsector_t*) ((objectrecord_t*) listener->subsector)->obj;

        // Position. At eye-level.
        Sfx_GetListenerXYZ(vec);
        iSFX->Listenerv(SFXLP_POSITION, vec);

        // Orientation. (0,0) will produce front=(1,0,0) and up=(0,0,1).
        vec[VX] = listener->angle / (float) ANGLE_MAX *360;

        vec[VY] =
            listener->dPlayer ? LOOKDIR2DEG(listener->dPlayer->lookDir) : 0;
        iSFX->Listenerv(SFXLP_ORIENTATION, vec);

        // Velocity. The unit is world distance units per second.
        vec[VX] = listener->mom[MX] * TICSPERSEC;
        vec[VY] = listener->mom[MY] * TICSPERSEC;
        vec[VZ] = listener->mom[MZ] * TICSPERSEC;
        iSFX->Listenerv(SFXLP_VELOCITY, vec);

        // Reverb effects. Has the current sector changed?
        if(listenerSector != subSector->sector)
        {
            listenerSector = subSector->sector;

            for(i = 0; i < NUM_REVERB_DATA; ++i)
            {
                vec[i] = listenerSector->reverb[i];
                if(i == SRD_VOLUME)
                    vec[i] *= sfxReverbStrength;
            }

            iSFX->Listenerv(SFXLP_REVERB, vec);
        }
    }

    // Update all listener properties.
    iSFX->Listener(SFXLP_UPDATE, 0);
}

void Sfx_ListenerNoReverb(void)
{
    float               rev[4] = { 0, 0, 0, 0 };

    if(!sfxAvail)
        return;

    listenerSector = NULL;
    iSFX->Listenerv(SFXLP_REVERB, rev);
    iSFX->Listener(SFXLP_UPDATE, 0);
}

/**
 * Stops the sound playing on the channel.
 * \note Just stopping a buffer doesn't affect refresh.
 */
void Sfx_ChannelStop(sfxchannel_t* ch)
{
    if(!ch->buffer)
        return;

    iSFX->Stop(ch->buffer);
}

void Sfx_GetChannelPriorities(float* prios)
{
    int                 i;

    for(i = 0; i < numChannels; ++i)
        prios[i] = Sfx_ChannelPriority(channels + i);
}

sfxchannel_t* Sfx_ChannelFindVacant(boolean use3D, int bytes, int rate,
                                    int sampleID)
{
    int                 i;
    sfxchannel_t*       ch;

    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || (ch->buffer->flags & SFXBF_PLAYING) ||
           use3D != ((ch->buffer->flags & SFXBF_3D) != 0) ||
           ch->buffer->bytes != bytes || ch->buffer->rate != rate)
            continue;

        // What about the sample?
        if(sampleID > 0)
        {
            if(!ch->buffer->sample || ch->buffer->sample->id != sampleID)
                continue;
        }
        else if(sampleID == 0)
        {
            // We're trying to find a channel with no sample already loaded.
            if(ch->buffer->sample)
                continue;
        }

        // This is perfect, take this!
        return ch;
    }

    return NULL;
}

/**
 * Used by the high-level sound interface to play sounds on this system.
 *
 * @param sample        Ptr must be persistent! No copying is done here.
 * @param volume        Volume at which the sample should be played.
 * @param freq          Relative and modifies the sample's rate.
 * @param emitter       @c NULL = 'fixedPos' is checked for a position.
 *                      If both 'emitter' and 'fixedPos' are @c NULL, then
 *                      the sound is played as centered 2D.
 * @return              @c true, if a sound is started.
 */
int Sfx_StartSound(sfxsample_t* sample, float volume, float freq,
                   mobj_t* emitter, float* fixedPos, int flags)
{
    sfxchannel_t*       ch, *selCh, *prioCh;
    sfxinfo_t*          info;
    int                 i, count, nowTime;
    float               myPrio, lowPrio = 0, channelPrios[SFX_MAX_CHANNELS];
    boolean             haveChannelPrios = false;
    boolean             play3D = sfx3D && (emitter || fixedPos);

    if(!sfxAvail || sample->id < 1 || sample->id >= defs.count.sounds.num ||
       volume <= 0)
        return false;

    // Calculate the new sound's priority.
    nowTime = Sys_GetTime();
    myPrio = Sfx_Priority(emitter, fixedPos, volume, nowTime);

    // Ensure there aren't already too many channels playing this sample.
    info = sounds + sample->id;
    if(info->channels > 0)
    {
        // The decision to stop channels is based on priorities.
        Sfx_GetChannelPriorities(channelPrios);
        haveChannelPrios = true;

        count = Sfx_CountPlaying(sample->id);
        while(count >= info->channels)
        {
            /**
             * Stop the lowest priority sound of the playing instances,
             * again noting sounds that are more important than us.
             */
            for(selCh = NULL, i = 0, ch = channels; i < numChannels;
                ++i, ch++)
            {
                if(ch->buffer && (ch->buffer->flags & SFXBF_PLAYING) &&
                   ch->buffer->sample->id == sample->id &&
                   myPrio >= channelPrios[i] &&
                   (!selCh || channelPrios[i] <= lowPrio))
                {
                    selCh = ch;
                    lowPrio = channelPrios[i];
                }
            }

            if(!selCh)
            {
                /**
                 * The new sound can't be played because we were unable to
                 * stop enough channels to accommodate the limitation.
                 */
                return false;
            }

            // Stop this one.
            count--;
            Sfx_ChannelStop(selCh);
        }
    }

    // Hit count tells how many times the cached sound has been used.
    Sfx_CacheHit(sample->id);

    /**
     * Pick a channel for the sound. We will do our best to play the sound,
     * cancelling existing ones if need be. The best choice would be a
     * free channel already loaded with the sample, in the correct format
     * and mode.
     */

    BEGIN_COP;

    // First look through the stopped channels. At this stage we're very
    // picky: only the perfect choice will be good enough.
    selCh = Sfx_ChannelFindVacant(play3D, sample->bytesPer, sample->rate,
                                  sample->id);

    // The second step is to look for a vacant channel with any sample,
    // but preferably one with no sample already loaded.
    if(!selCh)
    {
        selCh = Sfx_ChannelFindVacant(play3D, sample->bytesPer,
                                      sample->rate, 0);
    }

    // Then try any non-playing channel in the correct format.
    if(!selCh)
    {
        selCh = Sfx_ChannelFindVacant(play3D, sample->bytesPer,
                                      sample->rate, -1);
    }

    if(!selCh)
    {   // A perfect channel could not be found.

        /**
         * We must use a channel with the wrong format or decide which one
         * of the playing ones gets stopped.
         */

        if(!haveChannelPrios)
            Sfx_GetChannelPriorities(channelPrios);

        // All channels with a priority less than or equal to ours can be
        // stopped.
        for(prioCh = NULL, i = 0, ch = channels; i < numChannels; ++i, ch++)
        {
            if(!ch->buffer || play3D != ((ch->buffer->flags & SFXBF_3D) != 0))
                continue; // No buffer or in the wrong mode.

            if(!(ch->buffer->flags & SFXBF_PLAYING))
            {   // This channel is not playing, just take it!
                selCh = ch;
                break;
            }

            // Are we more important than this sound?
            // We want to choose the lowest priority sound.
            if(myPrio >= channelPrios[i] &&
               (!prioCh || channelPrios[i] <= lowPrio))
            {
                prioCh = ch;
                lowPrio = channelPrios[i];
            }
        }

        // If a good low-priority channel was found, use it.
        if(prioCh)
        {
            selCh = prioCh;
            Sfx_ChannelStop(selCh);
        }
    }

    if(!selCh)
    {   // A suitable channel was not found.
        END_COP;
        return false;
    }

    // Does our channel need to be reformatted?
    if(selCh->buffer->rate != sample->rate ||
       selCh->buffer->bytes != sample->bytesPer)
    {
        iSFX->Destroy(selCh->buffer);
        // Create a new buffer with the correct format.
        selCh->buffer =
            iSFX->Create(play3D ? SFXBF_3D : 0, sample->bytesPer * 8,
                         sample->rate);
    }

    // Clear flags.
    selCh->buffer->flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);

    // Set buffer flags.
    if(flags & SF_REPEAT)
        selCh->buffer->flags |= SFXBF_REPEAT;
    if(flags & SF_DONT_STOP)
        selCh->buffer->flags |= SFXBF_DONT_STOP;

    // Init the channel information.
    selCh->flags &=
        ~(SFXCF_NO_ORIGIN | SFXCF_NO_ATTENUATION | SFXCF_NO_UPDATE);
    selCh->volume = volume;
    selCh->frequency = freq;
    if(!emitter && !fixedPos)
    {
        selCh->flags |= SFXCF_NO_ORIGIN;
        selCh->emitter = NULL;
    }
    else
    {
        selCh->emitter = emitter;
        if(fixedPos)
            memcpy(selCh->pos, fixedPos, sizeof(selCh->pos));
    }

    if(flags & SF_NO_ATTENUATION)
    {   // The sound can be heard from any distance.
        selCh->flags |= SFXCF_NO_ATTENUATION;
    }

    /**
     * Load in the sample. Must load prior to setting properties, because
     * the audioDriver might actually create the real buffer only upon loading.
     */
    iSFX->Load(selCh->buffer, sample);

    // Update channel properties.
    Sfx_ChannelUpdate(selCh);

    // 3D sounds need a few extra properties set up.
    if(play3D)
    {
        // Init the buffer's min/max distances.
        // This is only done once, when the sound is started (i.e. here).
        iSFX->Set(selCh->buffer, SFXBP_MIN_DISTANCE,
                  (selCh->flags & SFXCF_NO_ATTENUATION)? 10000 :
                  soundMinDist);

        iSFX->Set(selCh->buffer, SFXBP_MAX_DISTANCE,
                  (selCh->flags & SFXCF_NO_ATTENUATION)? 20000 :
                  soundMaxDist);
    }

    // This'll commit all the deferred properties.
    iSFX->Listener(SFXLP_UPDATE, 0);

    // Start playing.
    iSFX->Play(selCh->buffer);

    END_COP;

    // Take note of the start time.
    selCh->startTime = nowTime;

    // Sound successfully started.
    return true;
}

/**
 * Update channel and listener properties.
 */
void Sfx_Update(void)
{
    int                 i;
    sfxchannel_t*       ch;

    // If the display player doesn't have a mobj, no positioning is done.
    listener = S_GetListenerMobj();

    // Update channels.
    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(!ch->buffer || !(ch->buffer->flags & SFXBF_PLAYING))
            continue; // Not playing sounds on this...

        Sfx_ChannelUpdate(ch);
    }

    // Update listener.
    Sfx_ListenerUpdate();
}

/**
 * Periodical routines: channel updates, cache purge, cvar checks.
 */
void Sfx_StartFrame(void)
{
    static int          old3DMode = false;
    static int          old16Bit = false;
    static int          oldRate = 11025;
    static double       lastUpdate = 0;

    double              nowTime = Sys_GetSeconds();

    if(!sfxAvail)
        return;

    // Tell the audioDriver that the sound frame begins.
    audioDriver->Event(SFXEV_BEGIN);

    // Have there been changes to the cvar settings?
    if(old3DMode != sfx3D)
    {
        Sfx_3DMode(sfx3D != 0);
        old3DMode = sfx3D;
    }

    // Check that the rate is valid.
    if(sfxSampleRate != 11025 && sfxSampleRate != 22050 && sfxSampleRate != 44100)
    {
        Con_Message("sound-rate corrected to 11025.\n");
        sfxSampleRate = 11025;
    }

    // Do we need to change the sample format?
    if(old16Bit != sfx16Bit || oldRate != sfxSampleRate)
    {
        Sfx_SampleFormat(sfx16Bit ? 16 : 8, sfxSampleRate);
        old16Bit = sfx16Bit;
        oldRate = sfxSampleRate;
    }

    // Should we purge the cache (to conserve memory)?
    Sfx_PurgeCache();

    // Is it time to do a channel update?
    if(nowTime - lastUpdate >= UPDATE_TIME)
    {
        lastUpdate = nowTime;
        Sfx_Update();
    }
}

void Sfx_EndFrame(void)
{
    if(!sfxAvail)
        return;

    // The sound frame ends.
    audioDriver->Event(SFXEV_END);
}

/**
 * Creates the buffers for the channels.
 *
 * @param num2D         Number of 2D the rest will be 3D.
 */
void Sfx_CreateChannels(int num2D, int bits, int rate)
{
    int                 i;
    sfxchannel_t*       ch;
    float               parm[2];

    // Change the primary buffer's format to match the channel format.
    parm[0] = bits;
    parm[1] = rate;
    iSFX->Listenerv(SFXLP_PRIMARY_FORMAT, parm);

    // Try to create a buffer for each channel.
    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        ch->buffer = iSFX->Create(num2D-- > 0 ? 0 : SFXBF_3D, bits, rate);
        if(!ch->buffer)
        {
            Con_Message("Sfx_CreateChannels: Failed to create "
                        "buffer for #%i.\n", i);
            continue;
        }
    }
}

/**
 * Stop all channels and destroy their buffers.
 */
void Sfx_DestroyChannels(void)
{
    int                 i;

    BEGIN_COP;
    for(i = 0; i < numChannels; ++i)
    {
        Sfx_ChannelStop(channels + i);

        if(channels[i].buffer)
            iSFX->Destroy(channels[i].buffer);
        channels[i].buffer = NULL;
    }
    END_COP;
}

void Sfx_InitChannels(void)
{
    numChannels = sfxMaxChannels;

    // The -sfxchan option can be used to change the number of channels.
    if(ArgCheckWith("-sfxchan", 1))
    {
        numChannels = strtol(ArgNext(), 0, 0);
        if(numChannels < 1)
            numChannels = 1;
        if(numChannels > SFX_MAX_CHANNELS)
            numChannels = SFX_MAX_CHANNELS;

        Con_Message("Sfx_InitChannels: %i channels.\n", numChannels);
    }

    // Allocate and init the channels.
    channels = Z_Calloc(sizeof(*channels) * numChannels, PU_STATIC, 0);

    // Create channels according to the current mode.
    Sfx_CreateChannels(sfx3D ? sfxDedicated2D : numChannels, sfxBits,
                       sfxRate);
}

/**
 * Frees all memory allocated for the channels.
 */
void Sfx_ShutdownChannels(void)
{
    Sfx_DestroyChannels();

    if(channels)
        Z_Free(channels);
    channels = NULL;
    numChannels = 0;
}

/**
 * Start the channel refresh thread. It will stop on its own when it
 * notices that the rest of the sound system is going down.
 */
void Sfx_StartRefresh(void)
{
    refreshing = false;
    allowRefresh = true;
    refreshHandle = Sys_StartThread(Sfx_ChannelRefreshThread, NULL);
    if(!refreshHandle)
        Con_Error("Sfx_StartRefresh: Failed to start refresh.\n");
}

/**
 * Initialize the Sfx module. This includes setting up the available Sfx
 * drivers and the channels, and initializing the sound cache. Returns
 * true if the module is operational after the init.
 */
boolean Sfx_Init(void)
{
    if(sfxAvail)
        return true; // Already initialized.

    // Check if sound has been disabled with a command line option.
    if(ArgExists("-nosfx"))
    {
        Con_Message("Sfx_Init: Disabled.\n");
        return true;
    }

    Con_Message("Sfx_Init: Initializing...\n");

    // Use the external SFX playback facilities, if available.
    if(audioDriver == &audiod_dummy)
    {
        iSFX = (audiointerface_sfx_generic_t*) &audiod_dummy_sfx;
    }
    else if(audioDriver == &audiod_sdlmixer)
    {
        iSFX = (audiointerface_sfx_generic_t*) &audiod_sdlmixer_sfx;
    }
    else
    {
        iSFX = (audiodExternalISFX.gen.Init ?
            (audiointerface_sfx_generic_t*) &audiodExternalISFX : 0);
    }

    if(!iSFX)
    {   // No interface for SFX playback.
        return false;
    }

    // This is based on the scientific calculations that if the DOOM marine
    // is 56 units tall, 60 is about two meters.
    //// \fixme Derive from the viewheight.
    iSFX->Listener(SFXLP_UNITS_PER_METER, 30);
    iSFX->Listener(SFXLP_DOPPLER, 1);

    // The audioDriver is working, let's create the channels.
    Sfx_InitChannels();

    // Init the sample cache.
    Sfx_InitCache();

    // The Sfx module is now available.
    sfxAvail = true;

    Sfx_ListenerNoReverb();

    // Finally, start the refresh thread.
    Sfx_StartRefresh();
    return true;
}

/**
 * Shut down the whole Sfx module: drivers, channel buffers and the cache.
 */
void Sfx_Shutdown(void)
{
    if(!sfxAvail)
        return; // Not initialized.

    // These will stop further refreshing.
    sfxAvail = false;
    allowRefresh = false;

    // Destroy the sample cache.
    Sfx_ShutdownCache();

    // Destroy channels.
    Sfx_ShutdownChannels();
}

/**
 * Stop all channels, clear the cache.
 */
void Sfx_Reset(void)
{
    int                 i;

    if(!sfxAvail)
        return;

    listenerSector = NULL;

    // Stop all channels.
    for(i = 0; i < numChannels; ++i)
        Sfx_ChannelStop(&channels[i]);

    // Free all samples.
    Sfx_ShutdownCache();
}

/**
 * Destroys all channels and creates them again.
 */
void Sfx_RecreateChannels(void)
{
    Sfx_DestroyChannels();
    Sfx_CreateChannels(sfx3D ? sfxDedicated2D : numChannels, sfxBits,
                       sfxRate);
}

/**
 * Swaps between 2D and 3D sound modes. Called automatically by
 * Sfx_StartFrame when cvar changes.
 */
void Sfx_3DMode(boolean activate)
{
    if(sfx3D == activate)
        return; // No change; do nothing.

    sfx3D = activate;
    // To make the change effective, re-create all channels.
    Sfx_RecreateChannels();
    // If going to 2D, make sure the reverb is off.
    Sfx_ListenerNoReverb();
}

/**
 * Reconfigures the sample bits and rate. Called automatically by
 * Sfx_StartFrame when changes occur.
 */
void Sfx_SampleFormat(int newBits, int newRate)
{
    if(sfxBits == newBits && sfxRate == newRate)
        return; // No change; do nothing.

    // Set the new buffer format.
    sfxBits = newBits;
    sfxRate = newRate;
    Sfx_RecreateChannels();

    // The cache just became useless, clear it.
    Sfx_ShutdownCache();
}

/**
 * Must be done before the map is changed (from P_SetupMap, via
 * S_MapChange).
 */
void Sfx_MapChange(void)
{
    int                 i;
    sfxchannel_t*       ch;

    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(ch->emitter)
        {
            // Mobjs are about to be destroyed.
            ch->emitter = NULL;

            // Stop all channels with an origin.
            Sfx_ChannelStop(ch);
        }
    }

    // Sectors, too, for that matter.
    listenerSector = NULL;
}

void Sfx_DebugInfo(void)
{
    int                 i, lh = FR_TextHeight("W") - 3;
    sfxchannel_t*       ch;
    char                buf[200];
    uint                cachesize, ccnt;

    glColor3f(1, 1, 0);
    if(!sfxAvail)
    {
        FR_ShadowTextOut("Sfx disabled", 0, 0);
        return;
    }

    if(refMonitor)
        FR_ShadowTextOut("!", 0, 0);

    // Sample cache information.
    Sfx_GetCacheInfo(&cachesize, &ccnt);
    sprintf(buf, "Cached:%i (%i)", cachesize, ccnt);
    glColor3f(1, 1, 1);
    FR_ShadowTextOut(buf, 10, 0);

    // Print a line of info about each channel.
    for(i = 0, ch = channels; i < numChannels; ++i, ch++)
    {
        if(ch->buffer && (ch->buffer->flags & SFXBF_PLAYING))
            glColor3f(1, 1, 1);
        else
            glColor3f(1, 1, 0);

        sprintf(buf, "%02i: %c%c%c v=%3.1f f=%3.3f st=%i et=%u", i,
                !(ch->flags & SFXCF_NO_ORIGIN) ? 'O' : '.',
                !(ch->flags & SFXCF_NO_ATTENUATION) ? 'A' : '.',
                ch->emitter ? 'E' : '.', ch->volume, ch->frequency,
                ch->startTime, ch->buffer ? ch->buffer->endTime : 0);
        FR_ShadowTextOut(buf, 5, lh * (1 + i * 2));

        if(!ch->buffer)
            continue;
        sprintf(buf,
                "    %c%c%c%c id=%03i/%-8s ln=%05i b=%i rt=%2i bs=%05i "
                "(C%05i/W%05i)", (ch->buffer->flags & SFXBF_3D) ? '3' : '.',
                (ch->buffer->flags & SFXBF_PLAYING) ? 'P' : '.',
                (ch->buffer->flags & SFXBF_REPEAT) ? 'R' : '.',
                (ch->buffer->flags & SFXBF_RELOAD) ? 'L' : '.',
                ch->buffer->sample ? ch->buffer->sample->id : 0,
                ch->buffer->sample ? defs.sounds[ch->buffer->sample->id].
                id : "", ch->buffer->sample ? ch->buffer->sample->size : 0,
                ch->buffer->bytes, ch->buffer->rate / 1000, ch->buffer->length,
                ch->buffer->cursor, ch->buffer->written);
        FR_ShadowTextOut(buf, 5, lh * (2 + i * 2));
    }
}
