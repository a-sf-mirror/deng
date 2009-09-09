/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_lights.c: Handle Sector base lighting effects.
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom64.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void T_FireFlicker(fireflicker_t *flick)
{
    float               lightLevel;
    float               amount;

    if(--flick->count)
        return;

    amount = ((P_Random() & 3) * 16) / 255.0f;

    lightLevel = DMU_GetFloatp(flick->sector, DMU_LIGHT_LEVEL);
    if(lightLevel - amount < flick->minLight)
        DMU_SetFloatp(flick->sector, DMU_LIGHT_LEVEL, flick->minLight);
    else
        DMU_SetFloatp(flick->sector, DMU_LIGHT_LEVEL,
                    flick->maxLight - amount);

    flick->count = 4;
}

void P_SpawnFireFlicker(sector_t *sector)
{
    float               lightLevel = DMU_GetFloatp(sector, DMU_LIGHT_LEVEL);
    float               otherLevel = DDMAXFLOAT;
    fireflicker_t      *flick;

    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    //P_ToXSector(sector)->special = 0; // jd64

    flick = Z_Calloc(sizeof(*flick), PU_MAP, 0);
    flick->thinker.function = T_FireFlicker;
    flick->thinker.header.type = DMU_THINKER_LIGHTFLICK;
    DD_ThinkerAdd(&flick->thinker);

    flick->sector = sector;
    flick->count = 4;
    flick->maxLight = lightLevel;

    P_FindSectorSurroundingLowestLight(sector, &otherLevel);
    if(otherLevel < lightLevel)
        flick->minLight = otherLevel;
    else
        flick->minLight = lightLevel;
    flick->minLight += (16.0f/255.0f);
}

/**
 * Broken light flashing.
 */
void T_LightFlash(lightflash_t *flash)
{
    float               lightLevel;

    if(--flash->count)
        return;

    lightLevel = DMU_GetFloatp(flash->sector, DMU_LIGHT_LEVEL);
    if(lightLevel == flash->maxLight)
    {
        DMU_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->minLight);
        flash->count = (P_Random() & flash->minTime) + 1;
    }
    else
    {
        DMU_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->maxLight);
        flash->count = (P_Random() & flash->maxTime) + 1;
    }
}

/**
 * After the map has been loaded, scan each sector for specials that spawn
 * thinkers.
 */
void P_SpawnLightFlash(sector_t *sector)
{
    float               lightLevel = DMU_GetFloatp(sector, DMU_LIGHT_LEVEL);
    float               otherLevel = DDMAXFLOAT;
    lightflash_t       *flash;

    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    //P_ToXSector(sector)->special = 0; // jd64

    flash = Z_Calloc(sizeof(*flash), PU_MAP, 0);
    flash->thinker.function = T_LightFlash;
    flash->thinker.header.type = DMU_THINKER_LIGHTFLASH;
    DD_ThinkerAdd(&flash->thinker);

    flash->sector = sector;
    flash->maxLight = lightLevel;

    P_FindSectorSurroundingLowestLight(sector, &otherLevel);
    if(otherLevel < lightLevel)
        flash->minLight = otherLevel;
    else
        flash->minLight = lightLevel;
    flash->maxTime = 64;
    flash->minTime = 7;
    flash->count = (P_Random() & flash->maxTime) + 1;
}

/**
 * d64tc
 */
void T_LightBlink(lightblink_t *flash)
{
    float           lightlevel = DMU_GetFloatp(flash->sector, DMU_LIGHT_LEVEL);

    if(--flash->count)
        return;

    if(lightlevel == flash->maxLight)
    {
        DMU_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->minLight);
        flash->count = flash->minTime;
    }
    else
    {
        DMU_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->maxLight);
        flash->count = flash->maxTime;
    }
}

/**
 * d64tc
 */
void P_SpawnLightBlink(sector_t *sector)
{
    lightblink_t       *blink;

    blink = Z_Calloc(sizeof(*blink), PU_MAP, 0);
    blink->thinker.function = T_LightBlink;
    blink->thinker.header.type = DMU_THINKER_LIGHTBLINK;
    DD_ThinkerAdd(&blink->thinker);

    blink->sector = sector;
    blink->maxLight = DMU_GetFloatp(sector, DMU_LIGHT_LEVEL);

    blink->minLight = 0;
    blink->maxTime = blink->minTime = blink->count = 4;
}

/**
 * Strobe light flashing.
 */
void T_StrobeFlash(strobe_t *flash)
{
    float               lightLevel;

    if(--flash->count)
        return;

    lightLevel = DMU_GetFloatp(flash->sector, DMU_LIGHT_LEVEL);
    if(lightLevel == flash->minLight)
    {
        DMU_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->maxLight);
        flash->count = flash->brightTime;
    }
    else
    {
        DMU_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->minLight);
        flash->count = flash->darkTime;
    }
}

/**
 * After the map has been loaded, scan each sector for specials that spawn
 * thinkers.
 */
void P_SpawnStrobeFlash(sector_t *sector, int fastOrSlow, int inSync)
{
    strobe_t           *flash;
    float               lightLevel = DMU_GetFloatp(sector, DMU_LIGHT_LEVEL);
    float               otherLevel = DDMAXFLOAT;

    flash = Z_Calloc(sizeof(*flash), PU_MAP, 0);
    flash->thinker.function = T_StrobeFlash;
    flash->thinker.header.type = DMU_THINKER_LIGHTSTROBE;
    DD_ThinkerAdd(&flash->thinker);

    flash->sector = sector;
    flash->darkTime = fastOrSlow;
    flash->brightTime = STROBEBRIGHT;
    flash->maxLight = lightLevel;
    P_FindSectorSurroundingLowestLight(sector, &otherLevel);
    if(otherLevel < lightLevel)
        flash->minLight = otherLevel;
    else
        flash->minLight = lightLevel;

    if(flash->minLight == flash->maxLight)
        flash->minLight = 0;

    // nothing special about it during gameplay
    P_ToXSector(sector)->special = 0;

    if(!inSync)
        flash->count = (P_Random() & 7) + 1;
    else
        flash->count = 1;
}

/*
 * Start strobing lights (usually from a trigger)
 */
void EV_StartLightStrobing(linedef_t *line)
{
    sector_t           *sec = NULL;
    iterlist_t         *list;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_ToXSector(sec)->specialData)
            continue;

        P_SpawnStrobeFlash(sec, SLOWDARK, 0);
    }
}

void EV_TurnTagLightsOff(linedef_t *line)
{
    sector_t           *sec = NULL;
    iterlist_t         *list;
    float               lightLevel;
    float               otherLevel;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        lightLevel = DMU_GetFloatp(sec, DMU_LIGHT_LEVEL);
        otherLevel = DDMAXFLOAT;
        P_FindSectorSurroundingLowestLight(sec, &otherLevel);
        if(otherLevel < lightLevel)
            lightLevel = otherLevel;

        DMU_SetFloatp(sec, DMU_LIGHT_LEVEL, lightLevel);
    }
}

void EV_LightTurnOn(linedef_t *line, float max)
{
    sector_t           *sec = NULL;
    iterlist_t         *list;
    float               lightLevel, otherLevel;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return;

    if(max != 0)
        lightLevel = max;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        // If Max = 0 means to search for the highest light level in the
        // surrounding sector.
        if(max == 0)
        {
            lightLevel = DMU_GetFloatp(sec, DMU_LIGHT_LEVEL);
            otherLevel = DDMINFLOAT;
            P_FindSectorSurroundingHighestLight(sec, &otherLevel);
            if(otherLevel > lightLevel)
                lightLevel = otherLevel;
        }

        DMU_SetFloatp(sec, DMU_LIGHT_LEVEL, lightLevel);
    }
}

void T_Glow(glow_t *g)
{
    float               lightLevel = DMU_GetFloatp(g->sector, DMU_LIGHT_LEVEL);
    float               glowDelta = (1.0f / 255.0f) * (float) GLOWSPEED;

    switch(g->direction)
    {
    case -1: // Down.
        lightLevel -= glowDelta;
        if(lightLevel <= g->minLight)
        {
            lightLevel += glowDelta;
            g->direction = 1;
        }
        break;

    case 1: // Up.
        lightLevel += glowDelta;
        if(lightLevel >= g->maxLight)
        {
            lightLevel -= glowDelta;
            g->direction = -1;
        }
        break;
    }

    DMU_SetFloatp(g->sector, DMU_LIGHT_LEVEL, lightLevel);
}

void P_SpawnGlowingLight(sector_t* sector)
{
    float               lightLevel = DMU_GetFloatp(sector, DMU_LIGHT_LEVEL);
    float               otherLevel = DDMAXFLOAT;
    glow_t*             g;

    g = Z_Calloc(sizeof(*g), PU_MAP, 0);
    g->thinker.function = T_Glow;
    g->thinker.header.type = DMU_THINKER_LIGHTGLOW;
    DD_ThinkerAdd(&g->thinker);

    g->sector = sector;
    P_FindSectorSurroundingLowestLight(sector, &otherLevel);
    if(otherLevel < lightLevel)
        g->minLight = otherLevel;
    else
        g->minLight = lightLevel;
    g->maxLight = lightLevel;
    g->direction = -1;
}
