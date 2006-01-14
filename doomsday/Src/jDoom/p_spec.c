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

/*
 * Implements special effects:
 * Texture animation, height or lighting changes according to adjacent
 * sectors, respective utility functions, etc.
 *
 * Line Tag handling. Line and Sector triggers.
 *
 * Events are operations triggered by using, crossing,
 * or shooting special lines, or by timed thinkers.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_config.h"
#include "m_argv.h"
#include "m_random.h"
#include "r_local.h"
#include "p_local.h"
#include "g_game.h"
#include "s_sound.h"
#include "r_state.h"

// MACROS ------------------------------------------------------------------

// FIXME: Remove fixed limits

#define MAXANIMS        32
#define MAXLINEANIMS    64 // Animating line specials

// Limit number of sectors tested for adjoining height differences
#define MAX_ADJOINING_SECTORS   20

// TYPES -------------------------------------------------------------------

// Animating textures and planes

// In Doomsday these are handled via DED definitions.
// In BOOM they invented the ANIMATED lump for the same purpose.

// This struct is directly read from the lump.
// So its important we keep it aligned.
#pragma pack(1)
typedef struct
{
    /* Do NOT change these members in any way */
    signed char istexture;  //  if false, it is a flat (instead of bool)
    char        endname[9];
    char        startname[9];
    int         speed;
} animdef_t;
#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean levelTimer;
int     levelTimeCount;

int   numlinespecials;
line_t *linespeciallist[MAXLINEANIMS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/* From PrBoom:
 * Load the table of animation definitions, checking for existence of
 * the start and end of each frame. If the start doesn't exist the sequence
 * is skipped, if the last doesn't exist, BOOM exits.
 *
 * Wall/Flat animation sequences, defined by name of first and last frame,
 * The full animation sequence is given using all lumps between the start
 * and end entry, in the order found in the WAD file.
 *
 * This routine modified to read its data from a predefined lump or
 * PWAD lump called ANIMATED rather than a static table in this module to
 * allow wad designers to insert or modify animation sequences.
 *
 * Lump format is an array of byte packed animdef_t structures, terminated
 * by a structure with istexture == -1. The lump can be generated from a
 * text source file using SWANTBLS.EXE, distributed with the BOOM utils.
 * The standard list of switches and animations is contained in the example
 * source text file DEFSWANI.DAT also in the BOOM util distribution.
 */

/*
 * DJS - We'll support this BOOM extension by reading the data and then
 *       registering the new animations into Doomsday using the animation
 *       groups feature.
 *
 *       Support for this extension should be considered depreciated.
 *       All new features should be added, accessed via DED.
 */
void P_InitPicAnims(void)
{
    int i, j;
    int groupNum;
    int isTexture, startFrame, endFrame, ticsPerFrame;
    int numFrames;
    int lump = W_CheckNumForName("ANIMATED");
    animdef_t *animdefs;

    // Has a custom ANIMATED lump been loaded?
    if(lump)
    {
        animdefs = (animdef_t *)W_CacheLumpNum(lump, PU_STATIC);

        // Read structures until -1 is found
        for(i = 0; animdefs[i].istexture != -1 ; i++)
        {
            // Is it a texture?
            if(animdefs[i].istexture)
            {
                // different episode ?
                if(R_CheckTextureNumForName(animdefs[i].startname) == -1)
                    continue;

                endFrame = R_TextureNumForName(animdefs[i].endname);
                startFrame = R_TextureNumForName(animdefs[i].startname);
            }
            else // Its a flat.
            {
                if((W_CheckNumForName(animdefs[i].startname)) == -1)
                    continue;

                endFrame = R_FlatNumForName(animdefs[i].endname);
                startFrame = R_FlatNumForName(animdefs[i].startname);
            }

            isTexture = animdefs[i].istexture;
            numFrames = endFrame - startFrame + 1;

            ticsPerFrame = LONG(animdefs[i].speed);

            if(numFrames < 2)
                Con_Error("P_InitPicAnims: bad cycle from %s to %s",
                         animdefs[i].startname, animdefs[i].endname);

            // We have a valid animation.
            // Create a new animation group for it.
            groupNum =
                R_CreateAnimGroup(isTexture ? DD_TEXTURE : DD_FLAT, AGF_SMOOTH);

            // Doomsday's group animation needs to know the lump IDs of
            // ALL frames in the animation group so we'll have to step
            // through the lump directory adding frames as we go.
            // (DOOM only required the start/end lumps and would animate
            // all textures/flats inbetween).

            // Get the start & end lump numbers.
            startFrame = W_CheckNumForName(animdefs[i].startname);
            endFrame = W_CheckNumForName(animdefs[i].endname);

            // Add all frames from start to end to the group.
            for(j = startFrame; j <= endFrame; j++)
                R_AddToAnimGroup(groupNum, j, ticsPerFrame, 0);
        }

        Z_Free(animdefs);
    }
}

/*
 * Return sector_t * of sector next to current.
 * NULL if not two-sided line
 */
sector_t *getNextSector(line_t *line, sector_t *sec)
{
    if(!(P_GetIntp(DMU_LINE, line, DMU_FLAGS) & ML_TWOSIDED))
        return NULL;

    if(P_GetPtrp(DMU_LINE, line, DMU_FRONT_SECTOR) == sec)
        return P_GetPtrp(DMU_LINE, line, DMU_BACK_SECTOR);

    return P_GetPtrp(DMU_LINE, line, DMU_FRONT_SECTOR);
}

/*
 * FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
 */
fixed_t P_FindLowestFloorSurrounding(sector_t *sec)
{
    int     i;
    line_t *check;
    sector_t *other;

    fixed_t floor = P_GetFixedp(DMU_SECTOR, sec, DMU_FLOOR_HEIGHT);

    for(i = 0; i < P_GetIntp(DMU_SECTOR, sec, DMU_LINE_COUNT); i++)
    {
        check = P_GetPtrp(DMU_LINE_OF_SECTOR, sec, i);
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(DMU_SECTOR, other, DMU_FLOOR_HEIGHT) < floor)
            floor = P_GetFixedp(DMU_SECTOR, other, DMU_FLOOR_HEIGHT);
    }
    return floor;
}

/*
 * FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS
 */
fixed_t P_FindHighestFloorSurrounding(sector_t *sec)
{
    int     i;
    line_t *check;
    sector_t *other;
    fixed_t floor = -500 * FRACUNIT;

    for(i = 0; i < P_GetIntp(DMU_SECTOR, sec, DMU_LINE_COUNT); i++)
    {
        check = P_GetPtrp(DMU_LINE_OF_SECTOR, sec, i);
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(DMU_SECTOR, other, DMU_FLOOR_HEIGHT) > floor)
            floor = P_GetFixedp(DMU_SECTOR, other, DMU_FLOOR_HEIGHT);
    }
    return floor;
}

/*
 * FIND NEXT HIGHEST FLOOR IN SURROUNDING SECTORS
 * Note: this should be doable w/o a fixed array.
 */
fixed_t P_FindNextHighestFloor(sector_t *sec, int currentheight)
{
    int     i;
    int     h;
    int     min;
    line_t *check;
    sector_t *other;
    fixed_t height = currentheight;

    fixed_t heightlist[MAX_ADJOINING_SECTORS];

    for(i = 0, h = 0; i < P_GetIntp(DMU_SECTOR, sec, DMU_LINE_COUNT); i++)
    {
        check = P_GetPtrp(DMU_LINE_OF_SECTOR, sec, i);
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(DMU_SECTOR, other, DMU_FLOOR_HEIGHT) > height)
            heightlist[h++] =
                P_GetFixedp(DMU_SECTOR, other, DMU_FLOOR_HEIGHT);

        // Check for overflow. Exit.
        if(h >= MAX_ADJOINING_SECTORS)
        {
            fprintf(stderr, "Sector with more than 20 adjoining sectors\n");
            break;
        }
    }

    // Find lowest height in list
    if(!h)
        return currentheight;

    min = heightlist[0];

    // Range checking?
    for(i = 1; i < h; i++)
        if(heightlist[i] < min)
            min = heightlist[i];

    return min;
}

/*
 * FIND LOWEST CEILING IN THE SURROUNDING SECTORS
 */
fixed_t P_FindLowestCeilingSurrounding(sector_t *sec)
{
    int     i;
    line_t *check;
    sector_t *other;
    fixed_t height = MAXINT;

    for(i = 0; i < P_GetIntp(DMU_SECTOR, sec, DMU_LINE_COUNT); i++)
    {
        check = P_GetPtrp(DMU_LINE_OF_SECTOR, sec, i);
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(DMU_SECTOR, other, DMU_CEILING_HEIGHT) < height)
            height =
                P_GetFixedp(DMU_SECTOR, other, DMU_CEILING_HEIGHT);

    }
    return height;
}

/*
 * FIND HIGHEST CEILING IN THE SURROUNDING SECTORS
 */
fixed_t P_FindHighestCeilingSurrounding(sector_t *sec)
{
    int     i;
    line_t *check;
    sector_t *other;
    fixed_t height = 0;

    for(i = 0; i < P_GetIntp(DMU_SECTOR, sec, DMU_LINE_COUNT); i++)
    {
        check = P_GetPtrp(DMU_LINE_OF_SECTOR, sec, i);
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(DMU_SECTOR, other, DMU_CEILING_HEIGHT) > height)
            height =
                P_GetFixedp(DMU_SECTOR, other, DMU_CEILING_HEIGHT);
    }
    return height;
}

/*
 * RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
 */
int P_FindSectorFromLineTag(line_t *line, int start)
{
    int     i;
    xline_t *xline;

    xline = &xlines[P_ToIndex(DMU_LINE, line)];


    for(i = start + 1; i < numsectors; i++)
        if(xsectors[i].tag == xline->tag)
            return i;

    return -1;
}

/*
 * Find minimum light from an adjacent sector
 */
int P_FindMinSurroundingLight(sector_t *sector, int max)
{
    int     i;
    int     min;

    line_t *line;
    sector_t *check;

    min = max;
    for(i = 0; i < P_GetIntp(DMU_SECTOR, sector, DMU_LINE_COUNT); i++)
    {
        line = P_GetPtrp(DMU_LINE_OF_SECTOR, sector, i);
        check = getNextSector(line, sector);

        if(!check)
            continue;

        if(P_GetFixedp(DMU_SECTOR, check, DMU_LIGHT_LEVEL) < min)
            min = P_GetIntp(DMU_SECTOR, check, DMU_LIGHT_LEVEL);
    }
    return min;
}

/*
 * Called every time a thing origin is about to cross a line with
 * a non 0 special.
 */
void P_CrossSpecialLine(int linenum, int side, mobj_t *thing)
{
    line_t *line;
    int     ok;

    line = P_ToPtr(DMU_LINE, linenum);

    // Extended functionality overrides old.
    if(XL_CrossLine(line, side, thing))
        return;

    //  Triggers that other things can activate
    if(!thing->player)
    {
        // Things that should NOT trigger specials...
        switch (thing->type)
        {
        case MT_ROCKET:
        case MT_PLASMA:
        case MT_BFG:
        case MT_TROOPSHOT:
        case MT_HEADSHOT:
        case MT_BRUISERSHOT:
            return;
            break;

        default:
            break;
        }

        ok = 0;
        switch (xlines[linenum].special)
        {
        case 39:                // TELEPORT TRIGGER
        case 97:                // TELEPORT RETRIGGER
        case 125:               // TELEPORT MONSTERONLY TRIGGER
        case 126:               // TELEPORT MONSTERONLY RETRIGGER
        case 4:             // RAISE DOOR
        case 10:                // PLAT DOWN-WAIT-UP-STAY TRIGGER
        case 88:                // PLAT DOWN-WAIT-UP-STAY RETRIGGER
            ok = 1;
            break;
        }

        // Anything can trigger this line!
            if(P_GetInt(DMU_LINE, linenum, DMU_FLAGS) & ML_ALLTRIGGER)
                ok = 1;

        if(!ok)
            return;
    }

    // Note: could use some const's here.
    switch (xlines[linenum].special)
    {
        // TRIGGERS.
        // All from here to RETRIGGERS.
    case 2:
        // Open Door
        EV_DoDoor(line, open);
        xlines[linenum].special = 0;
        break;

    case 3:
        // Close Door
        EV_DoDoor(line, close);
        xlines[linenum].special = 0;
        break;

    case 4:
        // Raise Door
        EV_DoDoor(line, normal);
        xlines[linenum].special = 0;
        break;

    case 5:
        // Raise Floor
        EV_DoFloor(line, raiseFloor);
        xlines[linenum].special = 0;
        break;

    case 6:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, fastCrushAndRaise);
        xlines[linenum].special = 0;
        break;

    case 8:
        // Build Stairs
        EV_BuildStairs(line, build8);
        xlines[linenum].special = 0;
        break;

    case 10:
        // PlatDownWaitUp
        EV_DoPlat(line, downWaitUpStay, 0);
        xlines[linenum].special = 0;
        break;

    case 12:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        xlines[linenum].special = 0;
        break;

    case 13:
        // Light Turn On 255
        EV_LightTurnOn(line, 255);
        xlines[linenum].special = 0;
        break;

    case 16:
        // Close Door 30
        EV_DoDoor(line, close30ThenOpen);
        xlines[linenum].special = 0;
        break;

    case 17:
        // Start Light Strobing
        EV_StartLightStrobing(line);
        xlines[linenum].special = 0;
        break;

    case 19:
        // Lower Floor
        EV_DoFloor(line, lowerFloor);
        xlines[linenum].special = 0;
        break;

    case 22:
        // Raise floor to nearest height and change texture
        EV_DoPlat(line, raiseToNearestAndChange, 0);
        xlines[linenum].special = 0;
        break;

    case 25:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, crushAndRaise);
        xlines[linenum].special = 0;
        break;

    case 30:
        // Raise floor to shortest texture height
        //  on either side of lines.
        EV_DoFloor(line, raiseToTexture);
        xlines[linenum].special = 0;
        break;

    case 35:
        // Lights Very Dark
        EV_LightTurnOn(line, 35);
        xlines[linenum].special = 0;
        break;

    case 36:
        // Lower Floor (TURBO)
        EV_DoFloor(line, turboLower);
        xlines[linenum].special = 0;
        break;

    case 37:
        // LowerAndChange
        EV_DoFloor(line, lowerAndChange);
        xlines[linenum].special = 0;
        break;

    case 38:
        // Lower Floor To Lowest
        EV_DoFloor(line, lowerFloorToLowest);
        xlines[linenum].special = 0;
        break;

    case 39:
        // TELEPORT!
        EV_Teleport(line, side, thing);
        xlines[linenum].special = 0;
        break;

    case 40:
        // RaiseCeilingLowerFloor
        EV_DoCeiling(line, raiseToHighest);
        EV_DoFloor(line, lowerFloorToLowest);
        xlines[linenum].special = 0;
        break;

    case 44:
        // Ceiling Crush
        EV_DoCeiling(line, lowerAndCrush);
        xlines[linenum].special = 0;
        break;

    case 52:
        // EXIT!
        G_ExitLevel();
        break;

    case 53:
        // Perpetual Platform Raise
        EV_DoPlat(line, perpetualRaise, 0);
        xlines[linenum].special = 0;
        break;

    case 54:
        // Platform Stop
        EV_StopPlat(line);
        xlines[linenum].special = 0;
        break;

    case 56:
        // Raise Floor Crush
        EV_DoFloor(line, raiseFloorCrush);
        xlines[linenum].special = 0;
        break;

    case 57:
        // Ceiling Crush Stop
        EV_CeilingCrushStop(line);
        xlines[linenum].special = 0;
        break;

    case 58:
        // Raise Floor 24
        EV_DoFloor(line, raiseFloor24);
        xlines[linenum].special = 0;
        break;

    case 59:
        // Raise Floor 24 And Change
        EV_DoFloor(line, raiseFloor24AndChange);
        xlines[linenum].special = 0;
        break;

    case 104:
        // Turn lights off in sector(tag)
        EV_TurnTagLightsOff(line);
        xlines[linenum].special = 0;
        break;

    case 108:
        // Blazing Door Raise (faster than TURBO!)
        EV_DoDoor(line, blazeRaise);
        xlines[linenum].special = 0;
        break;

    case 109:
        // Blazing Door Open (faster than TURBO!)
        EV_DoDoor(line, blazeOpen);
        xlines[linenum].special = 0;
        break;

    case 100:
        // Build Stairs Turbo 16
        EV_BuildStairs(line, turbo16);
        xlines[linenum].special = 0;
        break;

    case 110:
        // Blazing Door Close (faster than TURBO!)
        EV_DoDoor(line, blazeClose);
        xlines[linenum].special = 0;
        break;

    case 119:
        // Raise floor to nearest surr. floor
        EV_DoFloor(line, raiseFloorToNearest);
        xlines[linenum].special = 0;
        break;

    case 121:
        // Blazing PlatDownWaitUpStay
        EV_DoPlat(line, blazeDWUS, 0);
        xlines[linenum].special = 0;
        break;

    case 124:
        // Secret EXIT
        G_SecretExitLevel();
        break;

    case 125:
        // TELEPORT MonsterONLY
        if(!thing->player)
        {
            EV_Teleport(line, side, thing);
            xlines[linenum].special = 0;
        }
        break;

    case 130:
        // Raise Floor Turbo
        EV_DoFloor(line, raiseFloorTurbo);
        xlines[linenum].special = 0;
        break;

    case 141:
        // Silent Ceiling Crush & Raise
        EV_DoCeiling(line, silentCrushAndRaise);
        xlines[linenum].special = 0;
        break;

        // RETRIGGERS.  All from here till end.
    case 72:
        // Ceiling Crush
        EV_DoCeiling(line, lowerAndCrush);
        break;

    case 73:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, crushAndRaise);
        break;

    case 74:
        // Ceiling Crush Stop
        EV_CeilingCrushStop(line);
        break;

    case 75:
        // Close Door
        EV_DoDoor(line, close);
        break;

    case 76:
        // Close Door 30
        EV_DoDoor(line, close30ThenOpen);
        break;

    case 77:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, fastCrushAndRaise);
        break;

    case 79:
        // Lights Very Dark
        EV_LightTurnOn(line, 35);
        break;

    case 80:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        break;

    case 81:
        // Light Turn On 255
        EV_LightTurnOn(line, 255);
        break;

    case 82:
        // Lower Floor To Lowest
        EV_DoFloor(line, lowerFloorToLowest);
        break;

    case 83:
        // Lower Floor
        EV_DoFloor(line, lowerFloor);
        break;

    case 84:
        // LowerAndChange
        EV_DoFloor(line, lowerAndChange);
        break;

    case 86:
        // Open Door
        EV_DoDoor(line, open);
        break;

    case 87:
        // Perpetual Platform Raise
        EV_DoPlat(line, perpetualRaise, 0);
        break;

    case 88:
        // PlatDownWaitUp
        EV_DoPlat(line, downWaitUpStay, 0);
        break;

    case 89:
        // Platform Stop
        EV_StopPlat(line);
        break;

    case 90:
        // Raise Door
        EV_DoDoor(line, normal);
        break;

    case 91:
        // Raise Floor
        EV_DoFloor(line, raiseFloor);
        break;

    case 92:
        // Raise Floor 24
        EV_DoFloor(line, raiseFloor24);
        break;

    case 93:
        // Raise Floor 24 And Change
        EV_DoFloor(line, raiseFloor24AndChange);
        break;

    case 94:
        // Raise Floor Crush
        EV_DoFloor(line, raiseFloorCrush);
        break;

    case 95:
        // Raise floor to nearest height
        // and change texture.
        EV_DoPlat(line, raiseToNearestAndChange, 0);
        break;

    case 96:
        // Raise floor to shortest texture height
        // on either side of lines.
        EV_DoFloor(line, raiseToTexture);
        break;

    case 97:
        // TELEPORT!
        EV_Teleport(line, side, thing);
        break;

    case 98:
        // Lower Floor (TURBO)
        EV_DoFloor(line, turboLower);
        break;

    case 105:
        // Blazing Door Raise (faster than TURBO!)
        EV_DoDoor(line, blazeRaise);
        break;

    case 106:
        // Blazing Door Open (faster than TURBO!)
        EV_DoDoor(line, blazeOpen);
        break;

    case 107:
        // Blazing Door Close (faster than TURBO!)
        EV_DoDoor(line, blazeClose);
        break;

    case 120:
        // Blazing PlatDownWaitUpStay.
        EV_DoPlat(line, blazeDWUS, 0);
        break;

    case 126:
        // TELEPORT MonsterONLY.
        if(!thing->player)
            EV_Teleport(line, side, thing);
        break;

    case 128:
        // Raise To Nearest Floor
        EV_DoFloor(line, raiseFloorToNearest);
        break;

    case 129:
        // Raise Floor Turbo
        EV_DoFloor(line, raiseFloorTurbo);
        break;
    }
}

/*
 * Called when a thing shoots a special line.
 */
void P_ShootSpecialLine(mobj_t *thing, line_t *line)
{
    int     ok;
    int     lineid = P_ToIndex(DMU_LINE, line);

    //  Impacts that other things can activate.
    if(!thing->player)
    {
        ok = 0;
        switch (xlines[lineid].special)
        {
        case 46:
            // OPEN DOOR IMPACT
            ok = 1;
            break;
        }
        if(!ok)
            return;
    }

    switch(xlines[lineid].special)
    {
    case 24:
        // RAISE FLOOR
        EV_DoFloor(line, raiseFloor);
        P_ChangeSwitchTexture(line, 0);
        break;

    case 46:
        // OPEN DOOR
        EV_DoDoor(line, open);
        P_ChangeSwitchTexture(line, 1);
        break;

    case 47:
        // RAISE FLOOR NEAR AND CHANGE
        EV_DoPlat(line, raiseToNearestAndChange, 0);
        P_ChangeSwitchTexture(line, 0);
        break;
    }
}

/*
 * Called every tic frame that the player origin is in a special sector
 */
void P_PlayerInSpecialSector(player_t *player)
{
    int sec;
    sector_t *sector;

    sector = P_GetPtrp(DMU_SUBSECTOR, player->plr->mo->subsector, DMU_SECTOR);
    sec = P_ToIndex(DMU_SECTOR, sector);

    // Falling, not all the way down yet?
    if(player->plr->mo->z != P_GetInt(DMU_SECTOR, sec, DMU_FLOOR_HEIGHT))
        return;

    // Has hitten ground.
    switch(xsectors[sec].special)
    {
    case 5:
        // HELLSLIME DAMAGE
        if(!player->powers[pw_ironfeet])
            if(!(leveltime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 10);
        break;

    case 7:
        // NUKAGE DAMAGE
        if(!player->powers[pw_ironfeet])
            if(!(leveltime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 5);
        break;

    case 16:
        // SUPER HELLSLIME DAMAGE
    case 4:
        // STROBE HURT
        if(!player->powers[pw_ironfeet] || (P_Random() < 5))
        {
            if(!(leveltime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 20);
        }
        break;

    case 9:
        // SECRET SECTOR
        player->secretcount++;
        xsectors[sec].special = 0;
        if(cfg.secretMsg)
        {
            P_SetMessage(player, "You've found a secret area!");
            S_ConsoleSound(sfx_getpow, 0, player - players);
            //S_NetStartPlrSound(player, sfx_itmbk);
            //gamemode==commercial? sfx_radio : sfx_tink);
        }
        break;

    case 11:
        // EXIT SUPER DAMAGE! (for E1M8 finale)
        player->cheats &= ~CF_GODMODE;

        if(!(leveltime & 0x1f))
            P_DamageMobj(player->plr->mo, NULL, NULL, 20);

        if(player->health <= 10)
            G_ExitLevel();
        break;

    default:
        /*Con_Error ("P_PlayerInSpecialSector: "
           "unknown special %i",
           sector->special); */
        break;
    };
}

/*
 * Animate planes, scroll walls, etc.
 */
void P_UpdateSpecials(void)
{
    int     i;
    int     x;
    line_t *line;
    side_t *side;

    // Extended lines and sectors.
    XG_Ticker();

    //  LEVEL TIMER
    if(levelTimer == true)
    {
        levelTimeCount--;
        if(!levelTimeCount)
            G_ExitLevel();
    }

#if 0
    //  ANIMATE FLATS AND TEXTURES GLOBALLY
    for(anim = anims; anim < lastanim; anim++)
    {
        for(i = anim->basepic; i < anim->basepic + anim->numpics; i++)
        {
            pic =
                anim->basepic +
                ((leveltime / anim->speed + i) % anim->numpics);
            if(anim->istexture)
                R_SetTextureTranslation(i, pic);
            else
                R_SetFlatTranslation(i, pic);
        }
    }
#endif

    //  ANIMATE LINE SPECIALS
    for(i = 0; i < numlinespecials; i++)
    {
        line = linespeciallist[i];

        switch(P_XLine(line)->special)
        {
        case 48:
            side = P_GetPtrp(DMU_LINE, line, DMU_SIDE0);

            // EFFECT FIRSTCOL SCROLL +
            x = P_GetFixedp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_X);
            P_SetFixedp(DMU_SIDE, side, DMU_TEXTURE_OFFSET_X, x += FRACUNIT);
            break;

        default:
            break;
        }
    }

    //  DO BUTTONS
    // FIXME! remove fixed limt.
    for(i = 0; i < MAXBUTTONS; i++)
        if(buttonlist[i].btimer)
        {
            buttonlist[i].btimer--;
            if(!buttonlist[i].btimer)
            {
                side_t* sdef = P_GetPtrp(DMU_LINE, buttonlist[i].line,
                                         DMU_SIDE0);
                sector_t* frontsector = P_GetPtrp(DMU_LINE, buttonlist[i].line,
                                                  DMU_FRONT_SECTOR);

                switch (buttonlist[i].where)
                {
                case top:
                    P_SetIntp(DMU_SIDE, sdef, DMU_TOP_TEXTURE,
                              buttonlist[i].btexture);
                    break;

                case middle:
                    P_SetIntp(DMU_SIDE, sdef, DMU_MIDDLE_TEXTURE,
                              buttonlist[i].btexture);
                    break;

                case bottom:
                    P_SetIntp(DMU_SIDE, sdef, DMU_BOTTOM_TEXTURE,
                              buttonlist[i].btexture);
                    break;

                default:
                    Con_Error("P_UpdateSpecials: Unknown sidedef section \"%d\".",
                              buttonlist[i].where);
                }

                S_StartSound(sfx_swtchn,
                             P_GetPtrp(DMU_SECTOR, frontsector, DMU_SOUND_ORIGIN));

                memset(&buttonlist[i], 0, sizeof(button_t));
            }
        }

}

int EV_DoDonut(line_t *line)
{
    sector_t *s1;
    sector_t *s2;
    sector_t *s3;
    int     secnum;
    int     rtn;
    int     i;
    line_t *check;
    floormove_t *floor;

    secnum = -1;
    rtn = 0;
    while((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        s1 = P_ToPtr(DMU_SECTOR, secnum);

        // ALREADY MOVING?  IF SO, KEEP GOING...
        if(xsectors[secnum].specialdata)
            continue;

        rtn = 1;

        s2 = getNextSector(P_GetPtrp(DMU_LINE_OF_SECTOR, s1, 0), s1);
        for(i = 0; i < P_GetIntp(DMU_SECTOR, s2, DMU_LINE_COUNT); i++)
        {
            check = P_GetPtrp(DMU_LINE_OF_SECTOR, s2, i);

            if((!P_GetIntp(DMU_LINE, check, DMU_FLAGS) & ML_TWOSIDED) ||
               (P_GetPtrp(DMU_LINE, check, DMU_BACK_SECTOR) == s1))
                continue;

            s3 = P_GetPtrp(DMU_LINE, check, DMU_BACK_SECTOR);

            //  Spawn rising slime
            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
            P_AddThinker(&floor->thinker);

            P_XSector(s2)->specialdata = floor;

            floor->thinker.function = T_MoveFloor;
            floor->type = donutRaise;
            floor->crush = false;
            floor->direction = 1;
            floor->sector = s2;
            floor->speed = FLOORSPEED / 2;
            floor->texture = P_GetIntp(DMU_SECTOR, s3, DMU_FLOOR_TEXTURE);
            floor->newspecial = 0;
            floor->floordestheight = P_GetIntp(DMU_SECTOR, s3, DMU_FLOOR_HEIGHT);

            //  Spawn lowering donut-hole
            floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
            P_AddThinker(&floor->thinker);

            P_XSector(s1)->specialdata = floor;

            floor->thinker.function = T_MoveFloor;
            floor->type = lowerFloor;
            floor->crush = false;
            floor->direction = -1;
            floor->sector = s1;
            floor->speed = FLOORSPEED / 2;
            floor->floordestheight = P_GetIntp(DMU_SECTOR, s3, DMU_FLOOR_HEIGHT);
            break;
        }
    }
    return rtn;
}

/*
 * After the map has been loaded, scan for specials that spawn thinkers
 * Parses command line parameters (FIXME: use global state variables).
 */
void P_SpawnSpecials(void)
{
    sector_t *sector;
    int     i;

    // See if -TIMER needs to be used.
    levelTimer = false;

    i = ArgCheck("-avg");
    if(i && deathmatch)
    {
        levelTimer = true;
        levelTimeCount = 20 * 60 * 35;
    }

    i = ArgCheck("-timer");
    if(i && deathmatch)
    {
        int     time;

        time = atoi(Argv(i + 1)) * 60 * 35;
        levelTimer = true;
        levelTimeCount = time;
    }

    //  Init special SECTORs.
    for(i = 0; i < numsectors; i++)
    {
        sector = P_ToPtr(DMU_SECTOR, i);

        if(!xsectors[i].special)
            continue;

        if(IS_CLIENT)
        {
            switch (xsectors[i].special)
            {
            case 9:
                // SECRET SECTOR
                totalsecret++;
                break;
            }
            continue;
        }

        switch (xsectors[i].special)
        {
        case 1:
            // FLICKERING LIGHTS
            P_SpawnLightFlash(sector);
            break;

        case 2:
            // STROBE FAST
            P_SpawnStrobeFlash(sector, FASTDARK, 0);
            break;

        case 3:
            // STROBE SLOW
            P_SpawnStrobeFlash(sector, SLOWDARK, 0);
            break;

        case 4:
            // STROBE FAST/DEATH SLIME
            P_SpawnStrobeFlash(sector, FASTDARK, 0);
            xsectors[i].special = 4;
            break;

        case 8:
            // GLOWING LIGHT
            P_SpawnGlowingLight(sector);
            break;

        case 9:
            // SECRET SECTOR
            totalsecret++;
            break;

        case 10:
            // DOOR CLOSE IN 30 SECONDS
            P_SpawnDoorCloseIn30(sector);
            break;

        case 12:
            // SYNC STROBE SLOW
            P_SpawnStrobeFlash(sector, SLOWDARK, 1);
            break;

        case 13:
            // SYNC STROBE FAST
            P_SpawnStrobeFlash(sector, FASTDARK, 1);
            break;

        case 14:
            // DOOR RAISE IN 5 MINUTES
            P_SpawnDoorRaiseIn5Mins(sector, i);
            break;

        case 17:
            P_SpawnFireFlicker(sector);
            break;
        }
    }

    //  Init line EFFECTs
    numlinespecials = 0;
    for(i = 0; i < numlines; i++)
    {
        switch (xlines[i].special)
        {
        case 48:
            // EFFECT FIRSTCOL SCROLL+
            linespeciallist[numlinespecials] = P_ToPtr(DMU_LINE, i);
            numlinespecials++;
            break;
        }
    }

    P_RemoveAllActiveCeilings();  // jff 2/22/98 use killough's scheme

    P_RemoveAllActivePlats();     // killough

    // FIXME: Remove fixed limit
    for(i = 0; i < MAXBUTTONS; i++)
        memset(&buttonlist[i], 0, sizeof(button_t));

    // Init extended generalized lines and sectors.
    XG_Init();
}
