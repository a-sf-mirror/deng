/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

// HEADER FILES ------------------------------------------------------------

// We are referring to sprite numbers.
#include "info.h"
#include "d_items.h"
#include "s_sound.h"
#include "p_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//
// These are used if other definitions are not found.
//
weaponinfo_t weaponinfo[NUMWEAPONS][NUMCLASSES] = {
   {
    { // fist
     GM_ANY,
     {0, 0, 0, 0}, // type: clip | shell | cell | misl
     {0, 0, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_PUNCHUP,
     0,            // raise sound id
     S_PUNCHDOWN,
     S_PUNCH,
     0,            // ready sound
     S_PUNCH1,
     S_NULL
    }
   },
   {
    { // pistol
     GM_ANY,
     {1, 0, 0, 0}, // type: clip | shell | cell | misl
     {1, 0, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_PISTOLUP,
     0,            // raise sound id
     S_PISTOLDOWN,
     S_PISTOL,
     0,            // ready sound
     S_PISTOL1,
     S_PISTOLFLASH
    }
   },
   {
    { // shotgun
     GM_ANY,
     {0, 1, 0, 0}, // type: clip | shell | cell | misl
     {0, 1, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_SGUNUP,
     0,            // raise sound id
     S_SGUNDOWN,
     S_SGUN,
     0,            // ready sound
     S_SGUN1,
     S_SGUNFLASH1
    }
   },
   {
    { // chaingun
     GM_ANY,
     {1, 0, 0, 0}, // type: clip | shell | cell | misl
     {1, 0, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_CHAINUP,
     0,            // raise sound id
     S_CHAINDOWN,
     S_CHAIN,
     0,            // ready sound
     S_CHAIN1,
     S_CHAINFLASH1
    }
   },
   {
    { // missile launcher
     GM_ANY,
     {0, 0, 0, 1}, // type: clip | shell | cell | misl
     {0, 0, 0, 1}, // pershot: clip | shell | cell | misl
     false,        // autofire when raised if fire held
     S_MISSILEUP,
     0,            // raise sound id
     S_MISSILEDOWN,
     S_MISSILE,
     0,            // ready sound
     S_MISSILE1,
     S_MISSILEFLASH1
    }
   },
   {
    { // plasma rifle
     GM_NOTSHAREWARE,
     {0, 0, 1, 0}, // type: clip | shell | cell | misl
     {0, 0, 1, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_PLASMAUP,
     0,            // raise sound id
     S_PLASMADOWN,
     S_PLASMA,
     0,            // ready sound
     S_PLASMA1,
     S_PLASMAFLASH1
    }
   },
   {
    { // bfg 9000
     GM_NOTSHAREWARE,
     {0, 0, 1, 0},  // type: clip | shell | cell | misl
     {0, 0, 40, 0}, // pershot: clip | shell | cell | misl
     false,         // autofire when raised if fire held
     S_BFGUP,
     0,            // raise sound id
     S_BFGDOWN,
     S_BFG,
     0,            // ready sound
     S_BFG1,
     S_BFGFLASH1
    }
   },
   {
    { // chainsaw
     GM_ANY,
     {0, 0, 0, 0}, // type: clip | shell | cell | misl
     {0, 0, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_SAWUP,
     sfx_sawup,    // raise sound id
     S_SAWDOWN,
     S_SAW,
     sfx_sawidl,   // ready sound
     S_SAW1,
     S_NULL
    }
   },
   {
    { // super shotgun
     GM_COMMERCIAL,
     {0, 1, 0, 0}, // type: clip | shell | cell | misl
     {0, 2, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_DSGUNUP,
     0,            // raise sound id
     S_DSGUNDOWN,
     S_DSGUN,
     0,            // ready sound
     S_DSGUN1,
     S_DSGUNFLASH1
    }
   }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Return the default for a value (retrieved from Doomsday)
 */
int GetDefInt(char *def, int *returned_value)
{
    char   *data;
    int     val;

    // Get the value.
    if(!Def_Get(DD_DEF_VALUE, def, &data))
        return 0;               // No such value...
    // Convert to integer.
    val = strtol(data, 0, 0);
    if(returned_value)
        *returned_value = val;
    return val;
}

void GetDefState(char *def, int *val)
{
    char   *data;

    // Get the value.
    if(!Def_Get(DD_DEF_VALUE, def, &data))
        return;
    // Get the state number.
    *val = Def_Get(DD_DEF_STATE, data, 0);
    if(*val < 0)
        *val = 0;
}

/*
 *Initialize weapon info, maxammo and clipammo.
 */
void P_InitWeaponInfo()
{
#define PLMAX "Player|Max ammo|"
#define PLCLP "Player|Clip ammo|"
#define WPINF "Weapon Info|"

    int     i;
    int     pclass = PCLASS_PLAYER;
    ammotype_t k;
    char    buf[80];
    char   *data;
    char   *ammotypes[NUMAMMO] = { "clip", "shell", "cell", "misl"};

    // Max ammo.
    GetDefInt(PLMAX "Clip", &maxammo[am_clip]);
    GetDefInt(PLMAX "Shell", &maxammo[am_shell]);
    GetDefInt(PLMAX "Cell", &maxammo[am_cell]);
    GetDefInt(PLMAX "Misl", &maxammo[am_misl]);

    // Clip ammo.
    GetDefInt(PLCLP "Clip", &clipammo[am_clip]);
    GetDefInt(PLCLP "Shell", &clipammo[am_shell]);
    GetDefInt(PLCLP "Cell", &clipammo[am_cell]);
    GetDefInt(PLCLP "Misl", &clipammo[am_misl]);

    for(i = 0; i < NUMWEAPONS; i++)
    {
        // TODO: Only allows for one type of ammo per weapon.
        sprintf(buf, WPINF "%i|Type", i);
        if(Def_Get(DD_DEF_VALUE, buf, &data))
        {
            // Set the right types of ammo.
            if(!stricmp(data, "noammo"))
            {
                for(k = 0; k < NUMAMMO; ++k)
                {
                    weaponinfo[i][pclass].mode[0].ammotype[k] = false;
                    weaponinfo[i][pclass].mode[0].pershot[k] = 0;
                }
            }
            else
            {
                for(k = 0; k < NUMAMMO; ++k)
                {
                    if(!stricmp(data, ammotypes[k]))
                    {
                        weaponinfo[i][pclass].mode[0].ammotype[k] = true;

                        sprintf(buf, WPINF "%i|Per shot", i);
                        GetDefInt(buf, &weaponinfo[i][pclass].mode[0].pershot[k]);
                        break;
                    }
                }
            }
        }
        // end todo

        sprintf(buf, WPINF "%i|Up", i);
        GetDefState(buf, &weaponinfo[i][pclass].mode[0].upstate);
        sprintf(buf, WPINF "%i|Down", i);
        GetDefState(buf, &weaponinfo[i][pclass].mode[0].downstate);
        sprintf(buf, WPINF "%i|Ready", i);
        GetDefState(buf, &weaponinfo[i][pclass].mode[0].readystate);
        sprintf(buf, WPINF "%i|Atk", i);
        GetDefState(buf, &weaponinfo[i][pclass].mode[0].atkstate);
        sprintf(buf, WPINF "%i|Flash", i);
        GetDefState(buf, &weaponinfo[i][pclass].mode[0].flashstate);
        sprintf(buf, WPINF "%i|Static", i);
        weaponinfo[i][pclass].mode[0].static_switch = GetDefInt(buf, 0);
    }
}

void P_InitPlayerValues(player_t *p)
{
#define PLINA "Player|Init ammo|"
    int     i;
    char    buf[20];

    GetDefInt("Player|Health", &p->health);
    GetDefInt("Player|Weapon", (int *) &p->readyweapon);
    p->pendingweapon = p->readyweapon;
    for(i = 0; i < NUMWEAPONS; i++)
    {
        sprintf(buf, "Weapon Info|%i|Owned", i);
        GetDefInt(buf, (int *) &p->weaponowned[i]);
    }
    GetDefInt(PLINA "Clip", &p->ammo[am_clip]);
    GetDefInt(PLINA "Shell", &p->ammo[am_shell]);
    GetDefInt(PLINA "Cell", &p->ammo[am_cell]);
    GetDefInt(PLINA "Misl", &p->ammo[am_misl]);
}
