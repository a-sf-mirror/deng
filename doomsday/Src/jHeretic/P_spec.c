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
 *
 *  2006/01/17 DJS - Recreated using jDoom's p_spec.c as a base.
 */

// HEADER FILES ------------------------------------------------------------

#include "jHeretic/Doomdef.h"
#include "jHeretic/h_stat.h"
#include "jHeretic/P_local.h"
#include "jHeretic/G_game.h"
#include "jHeretic/Sounds.h"
#include "jHeretic/h_config.h"

#include "Common/dmu_lib.h"
#include "Common/p_mapsetup.h"

// MACROS ------------------------------------------------------------------

// FIXME: Remove fixed limits

#define MAXLINEANIMS    64 // Animating line specials

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

int   numlinespecials;
line_t *linespeciallist[MAXLINEANIMS];

mobj_t  LavaInflictor; // From the HERETIC source (To be moved!)

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
    if(lump > 0)
    {
        Con_Message("P_InitPicAnims: \"ANIMATED\" lump found. Reading animations...\n");

        animdefs = (animdef_t *)W_CacheLumpNum(lump, PU_STATIC);

        // Read structures until -1 is found
        for(i = 0; animdefs[i].istexture != -1 ; ++i)
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

            if(startFrame != -1 && endFrame != -1)
            {
                // We have a valid animation.
                // Create a new animation group for it.
                groupNum =
                    R_CreateAnimGroup(isTexture ? DD_TEXTURE : DD_FLAT, AGF_SMOOTH);

                // Doomsday's group animation needs to know the texture/flat
                // numbers of ALL frames in the animation group so we'll have
                // to step through the directory adding frames as we go.
                // (DOOM only required the start/end texture/flat numbers and
                // would animate all textures/flats inbetween).

                VERBOSE(Con_Message("P_InitPicAnims: ADD (\"%s\" > \"%s\" %d)\n",
                                    animdefs[i].startname, animdefs[i].endname,
                                    ticsPerFrame));

                // Add all frames from start to end to the group.
                if(endFrame > startFrame)
                {
                    for(j = startFrame; j <= endFrame; j++)
                        R_AddToAnimGroup(groupNum, j, ticsPerFrame, 0);
                }
                else
                {
                    for(j = endFrame; j >= startFrame; j--)
                        R_AddToAnimGroup(groupNum, j, ticsPerFrame, 0);
                }
            }
        }
        Z_Free(animdefs);
        VERBOSE(Con_Message("P_InitPicAnims: Done.\n"));
    }
}

/*
 * Return sector_t * of sector next to current.
 * NULL if not two-sided line
 */
sector_t *getNextSector(line_t *line, sector_t *sec)
{
    if(!(P_GetIntp(line, DMU_FLAGS) & ML_TWOSIDED))
        return NULL;

    if(P_GetPtrp(line, DMU_FRONT_SECTOR) == sec)
        return P_GetPtrp(line, DMU_BACK_SECTOR);

    return P_GetPtrp(line, DMU_FRONT_SECTOR);
}

/*
 * FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
 */
fixed_t P_FindLowestFloorSurrounding(sector_t *sec)
{
    int     i;
    line_t *check;
    sector_t *other;

    fixed_t floor = P_GetFixedp(sec, DMU_FLOOR_HEIGHT);

    for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(other, DMU_FLOOR_HEIGHT) < floor)
            floor = P_GetFixedp(other, DMU_FLOOR_HEIGHT);
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

    for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(other, DMU_FLOOR_HEIGHT) > floor)
            floor = P_GetFixedp(other, DMU_FLOOR_HEIGHT);
    }
    return floor;
}

/*
 * Passed a sector and a floor height, returns the fixed point value
 * of the smallest floor height in a surrounding sector larger than
 * the floor height passed. If no such height exists the floorheight
 * passed is returned.
 *
 * DJS - Rewritten using Lee Killough's algorithm for avoiding the
 *       the fixed array.
 */
fixed_t P_FindNextHighestFloor(sector_t *sec, int currentheight)
{
    int     i;
    int     lineCount = P_GetIntp(sec, DMU_LINE_COUNT);
    line_t *check;
    sector_t *other;
    fixed_t height = currentheight;
    fixed_t otherHeight;
    fixed_t anotherHeight;

    for(i = 0; i < lineCount; i++)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = getNextSector(check, sec);

        if(!other)
            continue;

        otherHeight = P_GetFixedp(other, DMU_FLOOR_HEIGHT);

        if(otherHeight > currentheight)
        {
            while(++i < lineCount)
            {
                check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
                other = getNextSector(check, sec);

                if(other)
                {
                    anotherHeight = P_GetFixedp(other, DMU_FLOOR_HEIGHT);

                    if(anotherHeight < otherHeight && anotherHeight > currentheight)
                        otherHeight = anotherHeight;
                }
            }

            return otherHeight;
        }
    }

    return currentheight;
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

    for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(other, DMU_CEILING_HEIGHT) < height)
            height =
                P_GetFixedp(other, DMU_CEILING_HEIGHT);

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

    for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        other = getNextSector(check, sec);

        if(!other)
            continue;

        if(P_GetFixedp(other, DMU_CEILING_HEIGHT) > height)
            height =
                P_GetFixedp(other, DMU_CEILING_HEIGHT);
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

    xline = &xlines[P_ToIndex(line)];


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
    for(i = 0; i < P_GetIntp(sector, DMU_LINE_COUNT); i++)
    {
        line = P_GetPtrp(sector, DMU_LINE_OF_SECTOR | i);
        check = getNextSector(line, sector);

        if(!check)
            continue;

        if(P_GetIntp(check, DMU_LIGHT_LEVEL) < min)
            min = P_GetIntp(check, DMU_LIGHT_LEVEL);
    }
    return min;
}

/*
 * Called every time a thing origin is about to cross a line with
 * a non 0 special.
 */
void P_CrossSpecialLine(int linenum, int side, mobj_t *thing)
{
    line_t *line = P_ToPtr(DMU_LINE, linenum);
    int ok;

    // Extended functionality overrides old.
    if(XL_CrossLine(line, side, thing))
        return;

    //  Triggers that other things can activate
    if(!thing->player)
    {
/*      // DJS - All things can trigger specials in Heretic
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
*/
        ok = 0;
        switch (P_XLine(line)->special)
        {
        case 39:                // TELEPORT TRIGGER
        case 97:                // TELEPORT RETRIGGER
      //case 125:               // TELEPORT MONSTERONLY TRIGGER
      //case 126:               // TELEPORT MONSTERONLY RETRIGGER
        case 4:             // RAISE DOOR
      //case 10:                // PLAT DOWN-WAIT-UP-STAY TRIGGER
      //case 88:                // PLAT DOWN-WAIT-UP-STAY RETRIGGER
            ok = 1;
            break;
        }
/*      // DJS - Not implemented in jHeretic
        // Anything can trigger this line!
        if(P_GetInt(DMU_LINE, linenum, DMU_FLAGS) & ML_ALLTRIGGER)
            ok = 1;
*/
        if(!ok)
            return;
    }

    // Note: could use some const's here.
    switch (P_XLine(line)->special)
    {
        // TRIGGERS.
        // All from here to RETRIGGERS.
    case 2:
        // Open Door
        EV_DoDoor(line, open);
        P_XLine(line)->special = 0;
        break;

    case 3:
        // Close Door
        EV_DoDoor(line, close);
        P_XLine(line)->special = 0;
        break;

    case 4:
        // Raise Door
        EV_DoDoor(line, normal);
        P_XLine(line)->special = 0;
        break;

    case 5:
        // Raise Floor
        EV_DoFloor(line, raiseFloor);
        P_XLine(line)->special = 0;
        break;

    case 6:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, fastCrushAndRaise);
        P_XLine(line)->special = 0;
        break;

    case 8:
        // Build Stairs
        EV_BuildStairs(line, build8);
        P_XLine(line)->special = 0;
        break;

    case 10:
        // PlatDownWaitUp
        EV_DoPlat(line, downWaitUpStay, 0);
        P_XLine(line)->special = 0;
        break;

    case 12:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        P_XLine(line)->special = 0;
        break;

    case 13:
        // Light Turn On 255
        EV_LightTurnOn(line, 255);
        P_XLine(line)->special = 0;
        break;

    case 16:
        // Close Door 30
        EV_DoDoor(line, close30ThenOpen);
        P_XLine(line)->special = 0;
        break;

    case 17:
        // Start Light Strobing
        EV_StartLightStrobing(line);
        P_XLine(line)->special = 0;
        break;

    case 19:
        // Lower Floor
        EV_DoFloor(line, lowerFloor);
        P_XLine(line)->special = 0;
        break;

    case 22:
        // Raise floor to nearest height and change texture
        EV_DoPlat(line, raiseToNearestAndChange, 0);
        P_XLine(line)->special = 0;
        break;

    case 25:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, crushAndRaise);
        P_XLine(line)->special = 0;
        break;

    case 30:
        // Raise floor to shortest texture height
        // on either side of lines.
        EV_DoFloor(line, raiseToTexture);
        P_XLine(line)->special = 0;
        break;

    case 35:
        // Lights Very Dark
        EV_LightTurnOn(line, 35);
        P_XLine(line)->special = 0;
        break;

    case 36:
        // Lower Floor (TURBO)
        EV_DoFloor(line, turboLower);
        P_XLine(line)->special = 0;
        break;

    case 37:
        // LowerAndChange
        EV_DoFloor(line, lowerAndChange);
        P_XLine(line)->special = 0;
        break;

    case 38:
        // Lower Floor To Lowest
        EV_DoFloor(line, lowerFloorToLowest);
        P_XLine(line)->special = 0;
        break;

    case 39:
        // TELEPORT!
        EV_Teleport(line, side, thing);
        P_XLine(line)->special = 0;
        break;

    case 40:
        // RaiseCeilingLowerFloor
        EV_DoCeiling(line, raiseToHighest);
        EV_DoFloor(line, lowerFloorToLowest);
        P_XLine(line)->special = 0;
        break;

    case 44:
        // Ceiling Crush
        EV_DoCeiling(line, lowerAndCrush);
        P_XLine(line)->special = 0;
        break;

    case 52:
        // EXIT!
        G_ExitLevel();
        break;

    case 53:
        // Perpetual Platform Raise
        EV_DoPlat(line, perpetualRaise, 0);
        P_XLine(line)->special = 0;
        break;

    case 54:
        // Platform Stop
        EV_StopPlat(line);
        P_XLine(line)->special = 0;
        break;

    case 56:
        // Raise Floor Crush
        EV_DoFloor(line, raiseFloorCrush);
        P_XLine(line)->special = 0;
        break;

    case 57:
        // Ceiling Crush Stop
        EV_CeilingCrushStop(line);
        P_XLine(line)->special = 0;
        break;

    case 58:
        // Raise Floor 24
        EV_DoFloor(line, raiseFloor24);
        P_XLine(line)->special = 0;
        break;

    case 59:
        // Raise Floor 24 And Change
        EV_DoFloor(line, raiseFloor24AndChange);
        P_XLine(line)->special = 0;
        break;

    case 104:
        // Turn lights off in sector(tag)
        EV_TurnTagLightsOff(line);
        P_XLine(line)->special = 0;
        break;

/*
    // DJS - This stuff isn't in Heretic
    case 108:
        // Blazing Door Raise (faster than TURBO!)
        EV_DoDoor(line, blazeRaise);
        P_XLine(line)->special = 0;
        break;

    case 109:
        // Blazing Door Open (faster than TURBO!)
        EV_DoDoor(line, blazeOpen);
        P_XLine(line)->special = 0;
        break;

    case 100:
        // Build Stairs Turbo 16
        EV_BuildStairs(line, turbo16);
        P_XLine(line)->special = 0;
        break;

    case 110:
        // Blazing Door Close (faster than TURBO!)
        EV_DoDoor(line, blazeClose);
        P_XLine(line)->special = 0;
        break;

    case 119:
        // Raise floor to nearest surr. floor
        EV_DoFloor(line, raiseFloorToNearest);
        P_XLine(line)->special = 0;
        break;

    case 121:
        // Blazing PlatDownWaitUpStay
        EV_DoPlat(line, blazeDWUS, 0);
        P_XLine(line)->special = 0;
        break;
*/
  //case 124: // DJS - In Heretic, the secret exit is 105
    case 105:
        // Secret EXIT
        G_SecretExitLevel();
        break;

    // DJS - Heretic has an additional stair build special
    //       that moves in steps of 16.
    case 106:
        // Build Stairs
        EV_BuildStairs(line, build16);
        P_XLine(line)->special = 0;
        break;

/*  // DJS - more specials that arn't in Heretic
    case 125:
        // TELEPORT MonsterONLY
        if(!thing->player)
        {
            EV_Teleport(line, side, thing);
            P_XLine(line)->special = 0;
        }
        break;

    case 130:
        // Raise Floor Turbo
        EV_DoFloor(line, raiseFloorTurbo);
        P_XLine(line)->special = 0;
        break;

    case 141:
        // Silent Ceiling Crush & Raise
        EV_DoCeiling(line, silentCrushAndRaise);
        P_XLine(line)->special = 0;
        break;
*/

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

    case 100:
        // DJS - Heretic has one turbo door raise
        EV_DoDoor(line, blazeOpen);
        break;

/*
    // DJS - Yet more specials not in Heretic
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
*/
    }
}

/*
 * Called when a thing shoots a special line.
 */
void P_ShootSpecialLine(mobj_t *thing, line_t *line)
{
    int     ok;

    //  Impacts that other things can activate.
    if(!thing->player)
    {
        ok = 0;
        switch (P_XLine(line)->special)
        {
        case 46:
            // OPEN DOOR IMPACT
            ok = 1;
            break;
        }
        if(!ok)
            return;
    }

    switch(P_XLine(line)->special)
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
    sector_t *sector =
        P_GetPtrp(player->plr->mo->subsector, DMU_SECTOR);

    // Falling, not all the way down yet?
    if(player->plr->mo->pos[VZ] != P_GetFixedp(sector, DMU_FLOOR_HEIGHT))
        return;

    // Has hitten ground.
    switch(P_XSector(sector)->special)
    {

    case 5:
        // LAVA DAMAGE WEAK
        if(!(leveltime & 15))
        {
            P_DamageMobj(player->plr->mo, &LavaInflictor, NULL, 5);
            P_HitFloor(player->plr->mo);
        }
        break;

    case 7:
        // SLUDGE DAMAGE
        if(!(leveltime & 31))
            P_DamageMobj(player->plr->mo, NULL, NULL, 4);
        break;

    case 16:
        // LAVA DAMAGE HEAVY
        if(!(leveltime & 15))
        {
            P_DamageMobj(player->plr->mo, &LavaInflictor, NULL, 8);
            P_HitFloor(player->plr->mo);
        }
        break;

    case 4:
        // LAVA DAMAGE WEAK PLUS SCROLL EAST
        P_Thrust(player, 0, 2048 * 28);
        if(!(leveltime & 15))
        {
            P_DamageMobj(player->plr->mo, &LavaInflictor, NULL, 5);
            P_HitFloor(player->plr->mo);
        }
        break;

    case 9:
        // SECRET SECTOR
        player->secretcount++;
        P_XSector(sector)->special = 0;
        if(cfg.secretMsg)
        {
            P_SetMessage(player, "You've found a secret area!");
            S_ConsoleSound(sfx_wpnup, 0, player - players);
        }
        break;

    case 11:
        // EXIT SUPER DAMAGE! (for E1M8 finale)
/*      // DJS - Not used in Heretic
        player->cheats &= ~CF_GODMODE;

        if(!(leveltime & 0x1f))
            P_DamageMobj(player->plr->mo, NULL, NULL, 20);

        if(player->health <= 10)
            G_ExitLevel();
*/
        break;

    // DJS - These specials are handled elsewhere in jHeretic.
    case 15: // LOW FRICTION

    case 40: // WIND SPECIALS
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 51:
        break;

    default:
        P_PlayerInWindSector(player);
        break;
    }
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

    //  ANIMATE LINE SPECIALS
    for(i = 0; i < numlinespecials; i++)
    {
        line = linespeciallist[i];

        switch(P_XLine(line)->special)
        {
        case 48:
            side = P_GetPtrp(line, DMU_SIDE0);

            // EFFECT FIRSTCOL SCROLL +
            x = P_GetFixedp(side, DMU_TEXTURE_OFFSET_X);
            P_SetFixedp(side, DMU_TEXTURE_OFFSET_X, x += FRACUNIT);
            break;

        // DJS - Heretic also has a backwards wall scroller.
        case 99:
            side = P_GetPtrp(line, DMU_SIDE0);

            // EFFECT FIRSTCOL SCROLL +
            x = P_GetFixedp(side, DMU_TEXTURE_OFFSET_X);
            P_SetFixedp(side, DMU_TEXTURE_OFFSET_X, x -= FRACUNIT);
            break;

        default:
            break;
        }
    }

    //  DO BUTTONS
    // FIXME! remove fixed limt.
    for(i = 0; i < MAXBUTTONS; i++)
    {
        if(buttonlist[i].btimer)
        {
            buttonlist[i].btimer--;
            if(!buttonlist[i].btimer)
            {
                side_t* sdef = P_GetPtrp(buttonlist[i].line,
                                         DMU_SIDE0);
                sector_t* frontsector = P_GetPtrp(buttonlist[i].line,
                                                  DMU_FRONT_SECTOR);

                switch (buttonlist[i].where)
                {
                case top:
                    P_SetIntp(sdef, DMU_TOP_TEXTURE,
                              buttonlist[i].btexture);
                    break;

                case middle:
                    P_SetIntp(sdef, DMU_MIDDLE_TEXTURE,
                              buttonlist[i].btexture);
                    break;

                case bottom:
                    P_SetIntp(sdef, DMU_BOTTOM_TEXTURE,
                              buttonlist[i].btexture);
                    break;

                default:
                    Con_Error("P_UpdateSpecials: Unknown sidedef section \"%d\".",
                              buttonlist[i].where);
                }

                S_StartSound(sfx_switch /* sfx_swtchn */,
                             P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));

                memset(&buttonlist[i], 0, sizeof(button_t));
            }
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
        if(P_XSector(s1)->specialdata)
            continue;

        rtn = 1;

        s2 = getNextSector(P_GetPtrp(s1, DMU_LINE_OF_SECTOR | 0), s1);
        for(i = 0; i < P_GetIntp(s2, DMU_LINE_COUNT); i++)
        {
            check = P_GetPtrp(s2, DMU_LINE_OF_SECTOR | i);

            s3 = P_GetPtrp(check, DMU_BACK_SECTOR);

            if((!(P_GetIntp(check, DMU_FLAGS) & ML_TWOSIDED)) ||
               s3 == s1)
                continue;

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
            floor->texture = P_GetIntp(s3, DMU_FLOOR_TEXTURE);
            floor->newspecial = 0;
            floor->floordestheight = P_GetFixedp(s3, DMU_FLOOR_HEIGHT);

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
            floor->floordestheight = P_GetFixedp(s3, DMU_FLOOR_HEIGHT);
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

    //  Init special SECTORs.
    for(i = 0; i < numsectors; i++)
    {
        sector = P_ToPtr(DMU_SECTOR, i);

        if(!P_XSector(sector)->special)
            continue;

        if(IS_CLIENT)
        {
            switch (P_XSector(sector)->special)
            {
            case 9:
                // SECRET SECTOR
                totalsecret++;
                break;
            }
            continue;
        }

        switch (P_XSector(sector)->special)
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
            P_XSector(sector)->special = 4;
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

/*
        // DJS - Heretic doesn't use these.
        case 17:
            P_SpawnFireFlicker(sector);
            break;
*/
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
        case 99:
            // EFFECT FIRSTCOL SCROLL-
            // DJS - Heretic also has a backwards wall scroller.
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

/*
 * The code bellow this point has been taken from the Heretic source.
 * As such the HERETIC / HEXEN SOURCE CODE LICENSE applies.
 *
 * TODO: Move this stuff out of this file so that the above GPL code
 *       which is based on linuxdoom-1.10 can live in peace.
 */

// FROM HERETIC ------------------------------------------------------------

// MACROS ------------------------------------------------------------------

#define MAX_AMBIENT_SFX 8       // Per level

// TYPES -------------------------------------------------------------------

typedef enum {
    afxcmd_play,                // (sound)
    afxcmd_playabsvol,          // (sound, volume)
    afxcmd_playrelvol,          // (sound, volume)
    afxcmd_delay,               // (ticks)
    afxcmd_delayrand,           // (andbits)
    afxcmd_end                  // ()
} afxcmd_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int    *LevelAmbientSfx[MAX_AMBIENT_SFX];
int    *AmbSfxPtr;
int     AmbSfxCount;
int     AmbSfxTics;
int     AmbSfxVolume;

int     AmbSndSeqInit[] = {     // Startup
    afxcmd_end
};
int     AmbSndSeq1[] = {        // Scream
    afxcmd_play, sfx_amb1,
    afxcmd_end
};
int     AmbSndSeq2[] = {        // Squish
    afxcmd_play, sfx_amb2,
    afxcmd_end
};
int     AmbSndSeq3[] = {        // Drops
    afxcmd_play, sfx_amb3,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, sfx_amb7,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, sfx_amb3,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, sfx_amb7,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, sfx_amb3,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, sfx_amb7,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_end
};
int     AmbSndSeq4[] = {        // SlowFootSteps
    afxcmd_play, sfx_amb4,
    afxcmd_delay, 15,
    afxcmd_playrelvol, sfx_amb11, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, sfx_amb4, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, sfx_amb11, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, sfx_amb4, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, sfx_amb11, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, sfx_amb4, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, sfx_amb11, -3,
    afxcmd_end
};
int     AmbSndSeq5[] = {        // Heartbeat
    afxcmd_play, sfx_amb5,
    afxcmd_delay, 35,
    afxcmd_play, sfx_amb5,
    afxcmd_delay, 35,
    afxcmd_play, sfx_amb5,
    afxcmd_delay, 35,
    afxcmd_play, sfx_amb5,
    afxcmd_end
};
int     AmbSndSeq6[] = {        // Bells
    afxcmd_play, sfx_amb6,
    afxcmd_delay, 17,
    afxcmd_playrelvol, sfx_amb6, -8,
    afxcmd_delay, 17,
    afxcmd_playrelvol, sfx_amb6, -8,
    afxcmd_delay, 17,
    afxcmd_playrelvol, sfx_amb6, -8,
    afxcmd_end
};
int     AmbSndSeq7[] = {        // Growl
    afxcmd_play, sfx_bstsit,
    afxcmd_end
};
int     AmbSndSeq8[] = {        // Magic
    afxcmd_play, sfx_amb8,
    afxcmd_end
};
int     AmbSndSeq9[] = {        // Laughter
    afxcmd_play, sfx_amb9,
    afxcmd_delay, 16,
    afxcmd_playrelvol, sfx_amb9, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, sfx_amb9, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, sfx_amb10, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, sfx_amb10, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, sfx_amb10, -4,
    afxcmd_end
};
int     AmbSndSeq10[] = {       // FastFootsteps
    afxcmd_play, sfx_amb4,
    afxcmd_delay, 8,
    afxcmd_playrelvol, sfx_amb11, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, sfx_amb4, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, sfx_amb11, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, sfx_amb4, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, sfx_amb11, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, sfx_amb4, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, sfx_amb11, -3,
    afxcmd_end
};

int    *AmbientSfx[] = {
    AmbSndSeq1,                 // Scream
    AmbSndSeq2,                 // Squish
    AmbSndSeq3,                 // Drops
    AmbSndSeq4,                 // SlowFootsteps
    AmbSndSeq5,                 // Heartbeat
    AmbSndSeq6,                 // Bells
    AmbSndSeq7,                 // Growl
    AmbSndSeq8,                 // Magic
    AmbSndSeq9,                 // Laughter
    AmbSndSeq10                 // FastFootsteps
};

int    *TerrainTypes;
struct {
    char   *name;
    int     type;
} TerrainTypeDefs[] =
{
    {
    "FLTWAWA1", FLOOR_WATER},
    {
    "FLTFLWW1", FLOOR_WATER},
    {
    "FLTLAVA1", FLOOR_LAVA},
    {
    "FLATHUH1", FLOOR_LAVA},
    {
    "FLTSLUD1", FLOOR_SLUDGE},
    {
    "END", -1}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void P_InitLava(void)
{
    memset(&LavaInflictor, 0, sizeof(mobj_t));
    LavaInflictor.type = MT_PHOENIXFX2;
    LavaInflictor.flags2 = MF2_FIREDAMAGE | MF2_NODMGTHRUST;
}

void P_InitTerrainTypes(void)
{
    int     i;
    int     lump;
    int     size;

    size = Get(DD_NUMLUMPS) * sizeof(int);
    TerrainTypes = Z_Malloc(size, PU_STATIC, 0);
    memset(TerrainTypes, 0, size);
    for(i = 0; TerrainTypeDefs[i].type != -1; i++)
    {
        lump = W_CheckNumForName(TerrainTypeDefs[i].name);
        if(lump != -1)
        {
            TerrainTypes[lump] = TerrainTypeDefs[i].type;
        }
    }
}

/*
 *  Handles sector specials 25 - 39.
 */
void P_PlayerInWindSector(player_t *player)
{
    sector_t *sector =
        P_GetPtrp(player->plr->mo->subsector, DMU_SECTOR);

    static int pushTab[5] = {
        2048 * 5,
        2048 * 10,
        2048 * 25,
        2048 * 30,
        2048 * 35
    };

    switch (P_XSector(sector)->special)
    {
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
        // Scroll_North
        P_Thrust(player, ANG90, pushTab[P_XSector(sector)->special - 25]);
        break;

    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
        // Scroll_East
        P_Thrust(player, 0, pushTab[P_XSector(sector)->special - 20]);
        break;

    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
        // Scroll_South
        P_Thrust(player, ANG270, pushTab[P_XSector(sector)->special - 30]);
        break;

    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
        // Scroll_West
        P_Thrust(player, ANG180, pushTab[P_XSector(sector)->special - 35]);
        break;
    }

    // The other wind types (40..51).
    P_WindThrust(player->plr->mo);
}

void P_InitAmbientSound(void)
{
    AmbSfxCount = 0;
    AmbSfxVolume = 0;
    AmbSfxTics = 10 * TICSPERSEC;
    AmbSfxPtr = AmbSndSeqInit;
}

/*
 * Called by (P_mobj):P_SpawnMapThing during (P_setup):P_SetupLevel.
 */
void P_AddAmbientSfx(int sequence)
{
    if(AmbSfxCount == MAX_AMBIENT_SFX)
    {
        Con_Error("Too many ambient sound sequences");
    }
    LevelAmbientSfx[AmbSfxCount++] = AmbientSfx[sequence];
}

/*
 * Called every tic by (P_tick):P_Ticker.
 */
void P_AmbientSound(void)
{
    afxcmd_t cmd;
    int     sound;
    boolean done;

    // No ambient sound sequences on current level
    if(!AmbSfxCount)
        return;

    if(--AmbSfxTics)
        return;

    done = false;
    do
    {
        cmd = *AmbSfxPtr++;
        switch (cmd)
        {
        case afxcmd_play:
            AmbSfxVolume = P_Random() >> 2;
            S_StartSoundAtVolume(*AmbSfxPtr++, NULL, AmbSfxVolume / 127.0f);
            break;

        case afxcmd_playabsvol:
            sound = *AmbSfxPtr++;
            AmbSfxVolume = *AmbSfxPtr++;
            S_StartSoundAtVolume(sound, NULL, AmbSfxVolume / 127.0f);
            break;

        case afxcmd_playrelvol:
            sound = *AmbSfxPtr++;
            AmbSfxVolume += *AmbSfxPtr++;

            if(AmbSfxVolume < 0)
                AmbSfxVolume = 0;
            else if(AmbSfxVolume > 127)
                AmbSfxVolume = 127;

            S_StartSoundAtVolume(sound, NULL, AmbSfxVolume / 127.0f);
            break;

        case afxcmd_delay:
            AmbSfxTics = *AmbSfxPtr++;
            done = true;
            break;

        case afxcmd_delayrand:
            AmbSfxTics = P_Random() & (*AmbSfxPtr++);
            done = true;
            break;

        case afxcmd_end:
            AmbSfxTics = 6 * TICSPERSEC + P_Random();
            AmbSfxPtr = LevelAmbientSfx[P_Random() % AmbSfxCount];
            done = true;
            break;

        default:
            Con_Error("P_AmbientSound: Unknown afxcmd %d", cmd);
            break;
        }
    } while(done == false);
}
