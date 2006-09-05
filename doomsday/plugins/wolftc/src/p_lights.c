/**\file
 * Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 * Copyright © 2006 Daniel Swanson <danij@dengine.net>
 * Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 * Copyright © 1993-1996 by id Software, Inc.
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

/*
 * Handle Sector base lighting effects.
 * Muzzle flash?
 */

// HEADER FILES ------------------------------------------------------------

#include "wolftc.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void T_FireFlicker(fireflicker_t * flick)
{
    int     lightlevel = P_GetIntp(flick->sector, DMU_LIGHT_LEVEL);
    int     amount;

    if(--flick->count)
        return;

    amount = (P_Random() & 3) * 16;

    if(lightlevel - amount < flick->minlight)
        P_SetIntp(flick->sector, DMU_LIGHT_LEVEL, flick->minlight);
    else
        P_SetIntp(flick->sector, DMU_LIGHT_LEVEL, flick->maxlight - amount);

    flick->count = 4;
}

void P_SpawnFireFlicker(sector_t *sector)
{
    int     lightlevel = P_GetIntp(sector, DMU_LIGHT_LEVEL);
    fireflicker_t *flick;

    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    P_XSector(sector)->special = 0;

    flick = Z_Malloc(sizeof(*flick), PU_LEVSPEC, 0);

    P_AddThinker(&flick->thinker);

    flick->thinker.function = T_FireFlicker;
    flick->sector = sector;
    flick->maxlight = lightlevel;
    flick->minlight =
        P_FindMinSurroundingLight(sector, lightlevel) + 16;
    flick->count = 4;
}

/*
 * Broken light flashing.
 */
void T_LightFlash(lightflash_t * flash)
{
    int     lightlevel = P_GetIntp(flash->sector, DMU_LIGHT_LEVEL);

    if(--flash->count)
        return;

    if(lightlevel == flash->maxlight)
    {
        P_SetIntp(flash->sector, DMU_LIGHT_LEVEL, flash->minlight);
        flash->count = (P_Random() & flash->mintime) + 1;
    }
    else
    {
        P_SetIntp(flash->sector, DMU_LIGHT_LEVEL, flash->maxlight);
        flash->count = (P_Random() & flash->maxtime) + 1;
    }

}

/*
 * After the map has been loaded, scan each sector
 * for specials that spawn thinkers
 */
void P_SpawnLightFlash(sector_t *sector)
{
    int     lightlevel = P_GetIntp(sector, DMU_LIGHT_LEVEL);
    lightflash_t *flash;

    // nothing special about it during gameplay
    P_XSector(sector)->special = 0;

    flash = Z_Malloc(sizeof(*flash), PU_LEVSPEC, 0);

    P_AddThinker(&flash->thinker);

    flash->thinker.function = T_LightFlash;
    flash->sector = sector;
    flash->maxlight = P_GetIntp(sector, DMU_LIGHT_LEVEL);

    flash->minlight = P_FindMinSurroundingLight(sector, lightlevel);
    flash->maxtime = 64;
    flash->mintime = 7;
    flash->count = (P_Random() & flash->maxtime) + 1;
}

/*
 * Strobe light flashing.
 */
void T_StrobeFlash(strobe_t * flash)
{
    int     lightlevel = P_GetIntp(flash->sector, DMU_LIGHT_LEVEL);

    if(--flash->count)
        return;

    if(lightlevel == flash->minlight)
    {
        P_SetIntp(flash->sector, DMU_LIGHT_LEVEL, flash->maxlight);
        flash->count = flash->brighttime;
    }
    else
    {
        P_SetIntp(flash->sector, DMU_LIGHT_LEVEL, flash->minlight);
        flash->count = flash->darktime;
    }

}

/*
 * After the map has been loaded, scan each sector
 * for specials that spawn thinkers
 */
void P_SpawnStrobeFlash(sector_t *sector, int fastOrSlow, int inSync)
{
    strobe_t *flash;
    int     lightlevel = P_GetIntp(sector, DMU_LIGHT_LEVEL);

    flash = Z_Malloc(sizeof(*flash), PU_LEVSPEC, 0);

    P_AddThinker(&flash->thinker);

    flash->sector = sector;
    flash->darktime = fastOrSlow;
    flash->brighttime = STROBEBRIGHT;
    flash->thinker.function = T_StrobeFlash;
    flash->maxlight = lightlevel;
    flash->minlight = P_FindMinSurroundingLight(sector, lightlevel);

    if(flash->minlight == flash->maxlight)
        flash->minlight = 0;

    // nothing special about it during gameplay
    P_XSector(sector)->special = 0;

    if(!inSync)
        flash->count = (P_Random() & 7) + 1;
    else
        flash->count = 1;
}

/*
 * Start strobing lights (usually from a trigger)
 */
void EV_StartLightStrobing(line_t *line)
{
    int     secnum;
    sector_t *sec;

    secnum = -1;
    while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = P_ToPtr(DMU_SECTOR, secnum);

        if(xsectors[secnum].specialdata)
            continue;

        P_SpawnStrobeFlash(sec, SLOWDARK, 0);
    }
}

void EV_TurnTagLightsOff(line_t *line)
{
    int     i;
    int     j;
    int     min;
    int     linetag;
    int     lightlevel;
    sector_t *tsec;
    line_t *other;

    linetag = P_XLine(line)->tag;

    for(j = 0; j < numsectors; j++)
    {
        if(xsectors[j].tag == linetag)
        {
            min = P_GetInt(DMU_SECTOR, j, DMU_LIGHT_LEVEL);
            for(i = 0; i < P_GetInt(DMU_SECTOR, j, DMU_LINE_COUNT); i++)
            {
                other = P_GetPtr(DMU_SECTOR, j, DMU_LINE_OF_SECTOR | i);
                tsec = getNextSector(other, P_ToPtr(DMU_SECTOR, j));

                if(!tsec)
                    continue;

                lightlevel = P_GetIntp(tsec, DMU_LIGHT_LEVEL);

                if(lightlevel < min)
                    min = lightlevel;
            }

            P_SetInt(DMU_SECTOR, j, DMU_LIGHT_LEVEL, min);
        }
    }
}

void EV_LightTurnOn(line_t *line, int bright)
{
    int     i;
    int     j;
    int     linetag;
    int     lightlevel;
    sector_t *temp;
    line_t *templine;

    linetag = P_XLine(line)->tag;

    for(i = 0; i < numsectors; i++)
    {
        if(xsectors[i].tag == linetag)
        {
            // bright = 0 means to search
            // for highest light level
            // surrounding sector
            if(!bright)
            {
                for(j = 0; j < P_GetInt(DMU_SECTOR, i, DMU_LINE_COUNT); j++)
                {
                    templine = P_GetPtr(DMU_SECTOR, i, DMU_LINE_OF_SECTOR | j);
                    temp = getNextSector(templine, P_ToPtr(DMU_SECTOR, i));

                    if(!temp)
                        continue;

                    lightlevel = P_GetIntp(temp, DMU_LIGHT_LEVEL);

                    if(lightlevel > bright)
                        bright = lightlevel;
                }
            }
            P_SetInt(DMU_SECTOR, i, DMU_LIGHT_LEVEL, bright);
        }
    }
}

void T_Glow(glow_t * g)
{
    int lightlevel = P_GetIntp(g->sector, DMU_LIGHT_LEVEL);

    switch (g->direction)
    {
    case -1:
        // DOWN
        lightlevel -= GLOWSPEED;
        if(lightlevel <= g->minlight)
        {
            lightlevel += GLOWSPEED;
            g->direction = 1;
        }
        break;

    case 1:
        // UP
        lightlevel += GLOWSPEED;
        if(lightlevel >= g->maxlight)
        {
            lightlevel -= GLOWSPEED;
            g->direction = -1;
        }
        break;
    }

    P_SetIntp(g->sector, DMU_LIGHT_LEVEL, lightlevel);
}

void P_SpawnGlowingLight(sector_t *sector)
{
    int lightlevel = P_GetIntp(sector, DMU_LIGHT_LEVEL);
    glow_t *g;

    g = Z_Malloc(sizeof(*g), PU_LEVSPEC, 0);

    P_AddThinker(&g->thinker);

    g->sector = sector;
    g->minlight = P_FindMinSurroundingLight(sector, lightlevel);
    g->maxlight = lightlevel;
    g->thinker.function = T_Glow;
    g->direction = -1;

    P_XSector(sector)->special = 0;
}
