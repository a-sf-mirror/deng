/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_ceilings.c : Moving ceilings (lowering, crushing, raising).
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "gamemap.h"
#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_mapsetup.h"
#include "p_start.h"
#include "p_tick.h"
#include "p_ceiling.h"

// MACROS ------------------------------------------------------------------

// Sounds played by the ceilings when changing state or moving.
// jHexen uses sound sequences, so it's are defined as 'SFX_NONE'.
#if __JDOOM__
# define SFX_CEILINGMOVE        (SFX_STNMOV)
# define SFX_CEILINGSTOP        (SFX_PSTOP)
#elif __JDOOM64__
# define SFX_CEILINGMOVE        (SFX_STNMOV)
# define SFX_CEILINGSTOP        (SFX_PSTOP)
#elif __JHERETIC__
# define SFX_CEILINGMOVE        (SFX_DORMOV)
# define SFX_CEILINGSTOP        (SFX_NONE)
#elif __JHEXEN__
# define SFX_CEILINGMOVE        (SFX_NONE)
# define SFX_CEILINGSTOP        (SFX_NONE)
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Called when a moving ceiling needs to be removed.
 *
 * @param ceiling       Ptr to the ceiling to be stopped.
 */
static void stopCeiling(ceiling_t* ceiling)
{
    P_ToXSector(ceiling->sector)->specialData = NULL;
#if __JHEXEN__
    Map_SectorTagFinished(Thinker_Map((thinker_t*) ceiling), P_ToXSector(ceiling->sector)->tag);
#endif
    Map_RemoveThinker(Thinker_Map((thinker_t*) ceiling), (thinker_t*) ceiling);
}

void T_MoveCeiling(ceiling_t* ceiling)
{
    GameMap* map = Thinker_Map((thinker_t*) ceiling);
    result_e res;

    switch(ceiling->state)
    {
    case CS_UP: // Going up.
        res =
            T_MovePlane(ceiling->sector, ceiling->speed, ceiling->topHeight,
                        false, 1, 1);

        // Play a "while-moving" sound?
#if !__JHEXEN__
        if(!(map->time & 7))
        {
# if __JHERETIC__
            S_PlaneSound(ceiling->sector, PLN_CEILING, SFX_CEILINGMOVE);
# else
            switch(ceiling->type)
            {
            case CT_SILENTCRUSHANDRAISE:
                break;
            default:
                S_PlaneSound(ceiling->sector, PLN_CEILING, SFX_CEILINGMOVE);
                break;
            }
# endif
        }
#endif

        if(res == pastdest)
        {
#if __JHEXEN__
            SN_StopSequence(P_SectorSoundOrigin(ceiling->sector));
#endif
            switch(ceiling->type)
            {
#if !__JHEXEN__
            case CT_RAISETOHIGHEST:
# if __JDOOM64__
            case CT_CUSTOM: //jd64
# endif
                stopCeiling(ceiling);
                break;
# if !__JHERETIC__
            case CT_SILENTCRUSHANDRAISE:
                S_PlaneSound(ceiling->sector, PLN_CEILING, SFX_CEILINGSTOP);
# endif
            case CT_CRUSHANDRAISEFAST:
#endif
            case CT_CRUSHANDRAISE:
                ceiling->state = CS_DOWN;
#if __JHEXEN__
                ceiling->speed *= 2;
#endif
                break;

            default:
#if __JHEXEN__
                stopCeiling(ceiling);
#endif
                break;
            }

        }
        break;

    case CS_DOWN: // Going down.
        res =
            T_MovePlane(ceiling->sector, ceiling->speed, ceiling->bottomHeight,
                        ceiling->crush, 1, -1);

        // Play a "while-moving" sound?
#if !__JHEXEN__
        if(!(map->time & 7))
        {
# if __JHERETIC__
            S_PlaneSound(ceiling->sector, PLN_CEILING, SFX_CEILINGMOVE);
# else
            switch(ceiling->type)
            {
            case CT_SILENTCRUSHANDRAISE:
                break;
            default:
                S_PlaneSound(ceiling->sector, PLN_CEILING, SFX_CEILINGMOVE);
            }
# endif
        }
#endif

        if(res == pastdest)
        {
#if __JHEXEN__
            SN_StopSequence(P_SectorSoundOrigin(ceiling->sector));
#endif
            switch(ceiling->type)
            {
#if __JDOOM__ || __JDOOM64__
            case CT_SILENTCRUSHANDRAISE:
                S_PlaneSound(ceiling->sector, PLN_CEILING, SFX_CEILINGSTOP);
                ceiling->speed = CEILSPEED;
                ceiling->state = CS_UP;
                break;
#endif

            case CT_CRUSHANDRAISE:
#if __JHEXEN__
            case CT_CRUSHRAISEANDSTAY:
#endif
#if __JHEXEN__
                ceiling->speed = ceiling->speed * .5;
#else
                ceiling->speed = CEILSPEED;
#endif
                ceiling->state = CS_UP;
                break;
#if !__JHEXEN__
            case CT_CRUSHANDRAISEFAST:
                ceiling->state = CS_UP;
                break;

            case CT_LOWERANDCRUSH:
            case CT_LOWERTOFLOOR:
# if __JDOOM64__
            case CT_CUSTOM: //jd64
# endif
                stopCeiling(ceiling);
                break;
#endif

            default:
#if __JHEXEN__
                stopCeiling(ceiling);
#endif
                break;
            }
        }
        else                    // ( res != pastdest )
        {
            if(res == crushed)
            {
                switch(ceiling->type)
                {
#if __JDOOM__ || __JDOOM64__
                case CT_SILENTCRUSHANDRAISE:
#endif
                case CT_CRUSHANDRAISE:
                case CT_LOWERANDCRUSH:
#if __JHEXEN__
                case CT_CRUSHRAISEANDSTAY:
#endif
#if !__JHEXEN__
                    ceiling->speed = CEILSPEED * .125;
#endif
                    break;

                default:
                    break;
                }
            }
        }
        break;
    }
}

#if __JDOOM64__
static int EV_DoCeiling2(GameMap* map, linedef_t* line, int tag, float basespeed, ceilingtype_e type)
#elif __JHEXEN__
static int EV_DoCeiling2(GameMap* map, byte* arg, int tag, float basespeed, ceilingtype_e type)
#else
static int EV_DoCeiling2(GameMap* map, int tag, float basespeed, ceilingtype_e type)
#endif
{
    int rtn = 0;
    XSector* xsec;
    sector_t* sec = NULL;
    ceiling_t* ceiling;
    IterList* list;

    list = GameMap_SectorIterListForTag(map, tag, false);
    if(!list)
        return rtn;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        xsec = P_ToXSector(sec);

        if(xsec->specialData)
            continue;

        rtn = 1;

        // New door thinker.
        ceiling = Z_Calloc(sizeof(*ceiling), PU_MAP, 0);
        ceiling->thinker.function = T_MoveCeiling;
        Map_ThinkerAdd(map, &ceiling->thinker);

        xsec->specialData = ceiling;

        ceiling->sector = sec;
        ceiling->crush = false;
        ceiling->speed = basespeed;

        switch(type)
        {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        case CT_CRUSHANDRAISEFAST:
            ceiling->crush = true;
            ceiling->topHeight = DMU_GetFloatp(sec, DMU_CEILING_HEIGHT);
            ceiling->bottomHeight =
                DMU_GetFloatp(sec, DMU_FLOOR_HEIGHT) + 8;

            ceiling->state = CS_DOWN;
            ceiling->speed *= 2;
            break;
#endif
#if __JHEXEN__
        case CT_CRUSHRAISEANDSTAY:
            ceiling->crush = (int) arg[2];    // arg[2] = crushing value
            ceiling->topHeight = DMU_GetFloatp(sec, DMU_CEILING_HEIGHT);
            ceiling->bottomHeight = DMU_GetFloatp(sec, DMU_FLOOR_HEIGHT) + 8;
            ceiling->state = CS_DOWN;
            break;
#endif
#if __JDOOM__ || __JDOOM64__
        case CT_SILENTCRUSHANDRAISE:
#endif
        case CT_CRUSHANDRAISE:
#if !__JHEXEN__
            ceiling->crush = true;
#endif
            ceiling->topHeight = DMU_GetFloatp(sec, DMU_CEILING_HEIGHT);

        case CT_LOWERANDCRUSH:
#if __JHEXEN__
            ceiling->crush = (int) arg[2];    // arg[2] = crushing value
#endif
        case CT_LOWERTOFLOOR:
            ceiling->bottomHeight = DMU_GetFloatp(sec, DMU_FLOOR_HEIGHT);

            if(type != CT_LOWERTOFLOOR)
                ceiling->bottomHeight += 8;
            ceiling->state = CS_DOWN;
#if __JDOOM64__
            ceiling->speed *= 8; // jd64
#endif
            break;

        case CT_RAISETOHIGHEST:
            P_FindSectorSurroundingHighestCeiling(sec, 0, &ceiling->topHeight);
#if __JDOOM64__
            ceiling->topHeight -= 8;   // jd64
#endif
            ceiling->state = CS_UP;
            break;
#if __JDOOM64__
        case CT_CUSTOM: // jd64
            {
            //bitmip? wha?
            sidedef_t *front = DMU_GetPtrp(line, DMU_SIDEDEF0);
            sidedef_t *back = DMU_GetPtrp(line, DMU_SIDEDEF1);
            float bitmipL = 0, bitmipR = 0;

            bitmipL = DMU_GetFloatp(front, DMU_MIDDLE_MATERIAL_OFFSET_X);
            if(back)
                bitmipR = DMU_GetFloatp(back, DMU_MIDDLE_MATERIAL_OFFSET_X);

            if(bitmipR > 0)
            {
                P_FindSectorSurroundingHighestCeiling(sec, 0, &ceiling->topHeight);
                ceiling->state = CS_UP;
                ceiling->speed *= bitmipL;
                ceiling->topHeight -= bitmipR;
            }
            else
            {
                ceiling->bottomHeight = DMU_GetFloatp(sec, DMU_FLOOR_HEIGHT);
                ceiling->bottomHeight -= bitmipR;
                ceiling->state = CS_DOWN;
                ceiling->speed *= bitmipL;
            }
            }
#endif
#if __JHEXEN__
        case CT_LOWERBYVALUE:
            ceiling->bottomHeight =
                DMU_GetFloatp(sec, DMU_CEILING_HEIGHT) - (float) arg[2];
            ceiling->state = CS_DOWN;
            break;

        case CT_RAISEBYVALUE:
            ceiling->topHeight =
                DMU_GetFloatp(sec, DMU_CEILING_HEIGHT) + (float) arg[2];
            ceiling->state = CS_UP;
            break;

        case CT_MOVETOVALUEMUL8:
            {
            float   destHeight = (float) arg[2] * 8;

            if(arg[3]) // Going down?
                destHeight = -destHeight;

            if(DMU_GetFloatp(sec, DMU_CEILING_HEIGHT) <= destHeight)
            {
                ceiling->state = CS_UP;
                ceiling->topHeight = destHeight;
                if(DMU_GetFloatp(sec, DMU_CEILING_HEIGHT) == destHeight)
                    rtn = 0;
            }
            else if(DMU_GetFloatp(sec, DMU_CEILING_HEIGHT) > destHeight)
            {
                ceiling->state = CS_DOWN;
                ceiling->bottomHeight = destHeight;
            }
            break;
            }
#endif
        default:
#if __JHEXEN__
            rtn = 0;
#endif
            break;
        }

        ceiling->tag = xsec->tag;
        ceiling->type = type;

#if __JHEXEN__
        if(rtn)
        {
            SN_StartSequence(P_SectorSoundOrigin(ceiling->sector),
                             SEQ_PLATFORM + P_ToXSector(ceiling->sector)->seqType);
        }
#endif
    }
    return rtn;
}

/**
 * Move a ceiling up/down.
 */
#if __JHEXEN__
int EV_DoCeiling(linedef_t* line, byte* args, ceilingtype_e type)
#else
int EV_DoCeiling(linedef_t* line, ceilingtype_e type)
#endif
{
    assert(line);
#if __JHEXEN__
    assert(args);
    {
    GameMap* map = P_CurrentMap();
    return EV_DoCeiling2(map, args, (int) args[0], (float) args[1] * (1.0 / 8), type);
    }
#else
    {
    GameMap* map = P_CurrentMap();
    int rtn = 0;

    // Reactivate in-stasis ceilings...for certain types.
    switch(type)
    {
    case CT_CRUSHANDRAISEFAST:
# if __JDOOM__ || __JDOOM64__
    case CT_SILENTCRUSHANDRAISE:
# endif
    case CT_CRUSHANDRAISE:
        rtn = P_CeilingActivate(map, P_ToXLine(line)->tag);
        break;

    default:
        break;
    }
# if __JDOOM64__
    return EV_DoCeiling2(map, line, P_ToXLine(line)->tag, CEILSPEED, type) || rtn;
# else
    return EV_DoCeiling2(map, P_ToXLine(line)->tag, CEILSPEED, type) || rtn;
# endif
    }
#endif
}

#if !__JHEXEN__
typedef struct {
    short               tag;
    int                 count;
} activateceilingparams_t;

static int activateCeiling(void* p, void* context)
{
    ceiling_t*          ceiling = (ceiling_t*) p;
    activateceilingparams_t* params = (activateceilingparams_t*) context;

    if(ceiling->tag == (int) params->tag && ceiling->thinker.inStasis)
    {
        ceiling->state = ceiling->oldState;
        Thinker_SetStasis(&ceiling->thinker, false);
        params->count++;
    }

    return true; // Continue iteration.
}

/**
 * Reactivates all stopped crushers with the right tag.
 *
 * @param tag           Tag of ceilings to activate.
 *
 * @return              @c true, if a ceiling is activated.
 */
int P_CeilingActivate(GameMap* map, short tag)
{
    assert(map);
    {
    activateceilingparams_t params;

    params.tag = tag;
    params.count = 0;
    Map_IterateThinkers(map, T_MoveCeiling, activateCeiling, &params);

    return params.count;
    }
}
#endif

typedef struct {
    short               tag;
    int                 count;
} deactivateceilingparams_t;

static int deactivateCeiling(void* p, void* context)
{
    ceiling_t* ceiling = (ceiling_t*) p;
    deactivateceilingparams_t* params =
        (deactivateceilingparams_t*) context;

#if __JHEXEN__
    if(ceiling->tag == (int) params->tag)
    {   // Destroy it.
        SN_StopSequence(P_SectorSoundOrigin(ceiling->sector));
        stopCeiling(ceiling);
        params->count++;
        return false; // Stop iteration.
    }
#else
    if(!ceiling->thinker.inStasis && ceiling->tag == (int) params->tag)
    {   // Put it into stasis.
        ceiling->oldState = ceiling->state;
        Thinker_SetStasis(&ceiling->thinker, true);
        params->count++;
    }
#endif
    return true; // Continue iteration.
}

/**
 * Stops all active ceilings with the right tag.
 *
 * @param tag           Tag of ceilings to stop.
 *
 * @return              @c true, if a ceiling put in stasis.
 */
int P_CeilingDeactivate(GameMap* map, short tag)
{
    assert(map);
    {
    deactivateceilingparams_t params;

    params.tag = tag;
    params.count = 0;
    Map_IterateThinkers(map, T_MoveCeiling, deactivateCeiling, &params);

    return params.count;
    }
}
