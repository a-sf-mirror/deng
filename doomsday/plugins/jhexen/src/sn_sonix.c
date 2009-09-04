/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * sn_sonix.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include "jhexen.h"

#include "dmu_lib.h"
#include "g_common.h"

// MACROS ------------------------------------------------------------------

#define SS_MAX_SCRIPTS          64
#define SS_TEMPBUFFER_SIZE      1024
#define SS_SEQUENCE_NAME_LENGTH 32

#define SS_SCRIPT_NAME          "SNDSEQ"
#define SS_STRING_PLAY          "play"
#define SS_STRING_PLAYUNTILDONE "playuntildone"
#define SS_STRING_PLAYTIME      "playtime"
#define SS_STRING_PLAYREPEAT    "playrepeat"
#define SS_STRING_DELAY         "delay"
#define SS_STRING_DELAYRAND     "delayrand"
#define SS_STRING_VOLUME        "volume"
#define SS_STRING_END           "end"
#define SS_STRING_STOPSOUND     "stopsound"

// TYPES -------------------------------------------------------------------

typedef enum sscmds_e {
    SS_CMD_NONE,
    SS_CMD_PLAY,
    SS_CMD_WAITUNTILDONE, // used by PLAYUNTILDONE
    SS_CMD_PLAYTIME,
    SS_CMD_PLAYREPEAT,
    SS_CMD_DELAY,
    SS_CMD_DELAYRAND,
    SS_CMD_VOLUME,
    SS_CMD_STOPSOUND,
    SS_CMD_END
} sscmds_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void VerifySequencePtr(int* base, int* ptr);
static int GetSoundOffset(char* name);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int ActiveSequences;
seqnode_t* SequenceListHead;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static struct sstranslation_s {
    char            name[SS_SEQUENCE_NAME_LENGTH];
    int             scriptNum;
    int             stopSound;
} SequenceTranslate[SEQ_NUMSEQ] = {
    { "Platform", 0, 0 },
    { "Platform", 0, 0 },
    { "PlatformMetal", 0, 0 },
    { "Platform", 0, 0 },
    { "Silence", 0, 0 },
    { "Lava", 0, 0 },
    { "Water", 0, 0 },
    { "Ice", 0, 0 },
    { "Earth", 0, 0 },
    { "PlatformMetal2", 0, 0 },
    { "DoorNormal", 0, 0 },
    { "DoorHeavy", 0, 0 },
    { "DoorMetal", 0, 0 },
    { "DoorCreak", 0, 0 },
    { "Silence", 0, 0 },
    { "Lava", 0, 0 },
    { "Water", 0, 0 },
    { "Ice", 0, 0 },
    { "Earth", 0, 0 },
    { "DoorMetal2", 0, 0 },
    { "Wind", 0, 0 }
};

static int* SequenceData[SS_MAX_SCRIPTS];

// CODE --------------------------------------------------------------------

/**
 * Verifies the integrity of the temporary ptr, and ensures that the ptr
 * isn't exceeding the size of the temporary buffer
 */
static void VerifySequencePtr(int* base, int* ptr)
{
    if(ptr - base > SS_TEMPBUFFER_SIZE)
    {
        Con_Error("VerifySequencePtr:  tempPtr >= %d\n", SS_TEMPBUFFER_SIZE);
    }
}

static int GetSoundOffset(char* name)
{
    int                 i = Def_Get(DD_DEF_SOUND_BY_NAME, name, 0);

    if(!i)
        SC_ScriptError("GetSoundOffset:  Unknown sound name\n");
    return i;
}

void SN_InitSequenceScript(void)
{
    int                 i, j, inSequence;
    int*                tempDataStart = 0;
    int*                tempDataPtr = 0;

    inSequence = -1;
    ActiveSequences = 0;
    for(i = 0; i < SS_MAX_SCRIPTS; ++i)
    {
        SequenceData[i] = NULL;
    }

    SC_Open(SS_SCRIPT_NAME);
    while(SC_GetString())
    {
        if(*sc_String == ':')
        {
            if(inSequence != -1)
            {
                SC_ScriptError("SN_InitSequenceScript:  Nested Script Error");
            }

            tempDataStart = Z_Malloc(SS_TEMPBUFFER_SIZE, PU_STATIC, NULL);
            memset(tempDataStart, 0, SS_TEMPBUFFER_SIZE);
            tempDataPtr = tempDataStart;
            for(i = 0; i < SS_MAX_SCRIPTS; ++i)
            {
                if(SequenceData[i] == NULL)
                {
                    break;
                }
            }

            if(i == SS_MAX_SCRIPTS)
            {
                Con_Error("Number of SS Scripts >= SS_MAX_SCRIPTS");
            }

            for(j = 0; j < SEQ_NUMSEQ; ++j)
            {
                if(!strcasecmp(SequenceTranslate[j].name, sc_String + 1))
                {
                    SequenceTranslate[j].scriptNum = i;
                    inSequence = j;
                    break;
                }
            }
            continue; // parse the next command
        }

        if(inSequence == -1)
        {
            continue;
        }

        if(SC_Compare(SS_STRING_PLAYUNTILDONE))
        {
            VerifySequencePtr(tempDataStart, tempDataPtr);
            SC_MustGetString();
            *tempDataPtr++ = SS_CMD_PLAY;
            *tempDataPtr++ = GetSoundOffset(sc_String);
            *tempDataPtr++ = SS_CMD_WAITUNTILDONE;
        }
        else if(SC_Compare(SS_STRING_PLAY))
        {
            VerifySequencePtr(tempDataStart, tempDataPtr);
            SC_MustGetString();
            *tempDataPtr++ = SS_CMD_PLAY;
            *tempDataPtr++ = GetSoundOffset(sc_String);
        }
        else if(SC_Compare(SS_STRING_PLAYTIME))
        {
            VerifySequencePtr(tempDataStart, tempDataPtr);
            SC_MustGetString();
            *tempDataPtr++ = SS_CMD_PLAY;
            *tempDataPtr++ = GetSoundOffset(sc_String);
            SC_MustGetNumber();
            *tempDataPtr++ = SS_CMD_DELAY;
            *tempDataPtr++ = sc_Number;
        }
        else if(SC_Compare(SS_STRING_PLAYREPEAT))
        {
            VerifySequencePtr(tempDataStart, tempDataPtr);
            SC_MustGetString();
            *tempDataPtr++ = SS_CMD_PLAYREPEAT;
            *tempDataPtr++ = GetSoundOffset(sc_String);
        }
        else if(SC_Compare(SS_STRING_DELAY))
        {
            VerifySequencePtr(tempDataStart, tempDataPtr);
            *tempDataPtr++ = SS_CMD_DELAY;
            SC_MustGetNumber();
            *tempDataPtr++ = sc_Number;
        }
        else if(SC_Compare(SS_STRING_DELAYRAND))
        {
            VerifySequencePtr(tempDataStart, tempDataPtr);
            *tempDataPtr++ = SS_CMD_DELAYRAND;
            SC_MustGetNumber();
            *tempDataPtr++ = sc_Number;
            SC_MustGetNumber();
            *tempDataPtr++ = sc_Number;
        }
        else if(SC_Compare(SS_STRING_VOLUME))
        {
            VerifySequencePtr(tempDataStart, tempDataPtr);
            *tempDataPtr++ = SS_CMD_VOLUME;
            SC_MustGetNumber();
            *tempDataPtr++ = sc_Number;
        }
        else if(SC_Compare(SS_STRING_END))
        {
            int                 dataSize;

            *tempDataPtr++ = SS_CMD_END;
            dataSize = (tempDataPtr - tempDataStart) * sizeof(int);
            SequenceData[i] = Z_Malloc(dataSize, PU_STATIC, NULL);
            memcpy(SequenceData[i], tempDataStart, dataSize);
            Z_Free(tempDataStart);
            inSequence = -1;
        }
        else if(SC_Compare(SS_STRING_STOPSOUND))
        {
            SC_MustGetString();
            SequenceTranslate[inSequence].stopSound =
                GetSoundOffset(sc_String);
            *tempDataPtr++ = SS_CMD_STOPSOUND;
        }
        else
        {
            SC_ScriptError("SN_InitSequenceScript:  Unknown commmand.\n");
        }
    }
}

void SN_StartSequence(mobj_t* mobj, int sequence)
{
    seqnode_t*          node;

    SN_StopSequence(mobj); // Stop any previous sequence
    node = Z_Malloc(sizeof(seqnode_t), PU_STATIC, NULL);
    node->sequencePtr = SequenceData[SequenceTranslate[sequence].scriptNum];
    node->sequence = sequence;
    node->mobj = mobj;
    node->delayTics = 0;
    node->stopSound = SequenceTranslate[sequence].stopSound;
    node->volume = 127; // Start at max volume

    if(!SequenceListHead)
    {
        SequenceListHead = node;
        node->next = node->prev = NULL;
    }
    else
    {
        SequenceListHead->prev = node;
        node->next = SequenceListHead;
        node->prev = NULL;
        SequenceListHead = node;
    }
    ActiveSequences++;
}

void SN_StartSequenceInSec(sector_t* sector, int seqBase)
{
    SN_StartSequence(DMU_GetPtrp(sector, DMU_SOUND_ORIGIN),
                     seqBase + P_ToXSector(sector)->seqType);
}

void SN_StopSequenceInSec(sector_t* sector)
{
    SN_StopSequence(DMU_GetPtrp(sector, DMU_SOUND_ORIGIN));
}

void SN_StartSequenceName(mobj_t* mobj, const char* name)
{
    int                 i;

    for(i = 0; i < SEQ_NUMSEQ; ++i)
    {
        if(!strcmp(name, SequenceTranslate[i].name))
        {
            SN_StartSequence(mobj, i);
            return;
        }
    }
}

void SN_StopSequence(mobj_t* mobj)
{
    seqnode_t*          node;

    for(node = SequenceListHead; node; node = node->next)
    {
        if(node->mobj == mobj)
        {
            S_StopSound(0, mobj);
            if(node->stopSound)
            {
                S_StartSoundAtVolume(node->stopSound, mobj,
                                     node->volume / 127.0f);
            }

            if(SequenceListHead == node)
            {
                SequenceListHead = node->next;
            }

            if(node->prev)
            {
                node->prev->next = node->next;
            }

            if(node->next)
            {
                node->next->prev = node->prev;
            }

            Z_Free(node);
            ActiveSequences--;
        }
    }
}

void SN_UpdateActiveSequences(void)
{
    seqnode_t*          node;
    boolean             sndPlaying;

    if(!ActiveSequences || paused)
    {   // No sequences currently playing/game is paused
        return;
    }

    for(node = SequenceListHead; node; node = node->next)
    {
        if(node->delayTics)
        {
            node->delayTics--;
            continue;
        }

        // If ID is zero, S_IsPlaying returns true if any sound is playing.
        sndPlaying =
            node->currentSoundID ? S_IsPlaying(node->currentSoundID,
                                               node->mobj) : false;

        switch(*node->sequencePtr)
        {
        case SS_CMD_PLAY:
#if 0
Con_Message("play: %s: %p\n", SequenceTranslate[node->sequence].name,
            node->mobj);
#endif
            if(!sndPlaying)
            {
                node->currentSoundID = *(node->sequencePtr + 1);
#if 0
Con_Message("PLAY: %s: %p\n", SequenceTranslate[node->sequence].name,
            node->mobj);
#endif
                S_StartSoundAtVolume(node->currentSoundID, node->mobj,
                                     node->volume / 127.0f);
            }
            node->sequencePtr += 2;
            break;

        case SS_CMD_WAITUNTILDONE:
            if(!sndPlaying)
            {
                node->sequencePtr++;
                node->currentSoundID = 0;
            }
            break;

        case SS_CMD_PLAYREPEAT:
#if 0
Con_Message("rept: %s: %p\n", SequenceTranslate[node->sequence].name,
            node->mobj);
#endif

            if(!sndPlaying)
            {
#if 0
Con_Printf("REPT: id=%i, %s: %p\n", node->currentSoundID,
           SequenceTranslate[node->sequence].name, node->mobj);
#endif
                node->currentSoundID = *(node->sequencePtr + 1);

                S_StartSoundAtVolume(node->currentSoundID | DDSF_REPEAT,
                                     node->mobj, node->volume / 127.0f);
            }
            break;

        case SS_CMD_DELAY:
            node->delayTics = *(node->sequencePtr + 1);
            node->sequencePtr += 2;
            node->currentSoundID = 0;
            break;

        case SS_CMD_DELAYRAND:
            node->delayTics =
                *(node->sequencePtr + 1) +
                M_Random() % (*(node->sequencePtr + 2) -
                              *(node->sequencePtr + 1));
            node->sequencePtr += 2;
            node->currentSoundID = 0;
            break;

        case SS_CMD_VOLUME:
            node->volume = (127 * (*(node->sequencePtr + 1))) / 100;
            node->sequencePtr += 2;
            break;

        case SS_CMD_STOPSOUND:
            // Wait until something else stops the sequence
            break;

        case SS_CMD_END:
            SN_StopSequence(node->mobj);
            break;

        default:
            break;
        }
    }
}

void SN_StopAllSequences(void)
{
    seqnode_t*          node;

    for(node = SequenceListHead; node; node = node->next)
    {
        node->stopSound = 0;    // don't play any stop sounds
        SN_StopSequence(node->mobj);
    }
}

int SN_GetSequenceOffset(int sequence, int *sequencePtr)
{
    return (sequencePtr - SequenceData[SequenceTranslate[sequence].scriptNum]);
}

/**
 * NOTE: nodeNum zero is the first node
 */
void SN_ChangeNodeData(int nodeNum, int seqOffset, int delayTics,
                       int volume, int currentSoundID)
{
    int                 i;
    seqnode_t*          node;

    i = 0;
    node = SequenceListHead;
    while(node && i < nodeNum)
    {
        node = node->next;
        i++;
    }
    if(!node)
    {   // reach the end of the list before finding the nodeNum-th node
        return;
    }

    node->delayTics = delayTics;
    node->volume = volume;
    node->sequencePtr += seqOffset;
    node->currentSoundID = currentSoundID;
}
