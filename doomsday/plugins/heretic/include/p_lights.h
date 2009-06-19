/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_lights.h: Per-sector lighting effects - jHeretic specific.
 */

#ifndef __P_LIGHTS_H__
#define __P_LIGHTS_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#define GLOWSPEED               (8)
#define STROBEBRIGHT            (5)
#define FASTDARK                (15)
#define SLOWDARK                (35)

typedef struct {
    thinker_t       thinker;
    sector_t*       sector;
    int             count;
    float           maxLight;
    float           minLight;
    int             maxTime;
    int             minTime;
} lightflash_t;

typedef struct {
    thinker_t       thinker;
    sector_t*       sector;
    int             count;
    float           minLight;
    float           maxLight;
    int             darkTime;
    int             brightTime;
} strobe_t;

typedef struct {
    thinker_t       thinker;
    sector_t*       sector;
    float           minLight;
    float           maxLight;
    int             direction;
} glow_t;

void            T_LightFlash(lightflash_t* flash);
void            P_SpawnLightFlash(sector_t* sector);

void            T_StrobeFlash(strobe_t* flash);
void            P_SpawnStrobeFlash(sector_t* sector, int fastOrSlow,
                                   int inSync);
void            T_Glow(glow_t* g);
void            P_SpawnGlowingLight(sector_t* sector);

void            EV_StartLightStrobing(linedef_t* line);
void            EV_TurnTagLightsOff(linedef_t* line);
void            EV_LightTurnOn(linedef_t* line, float bright);

#endif
