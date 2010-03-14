/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_spec.c: Implements special effects.
 *
 * Texture animation, height or lighting changes according to adjacent
 * sectors, respective utility functions, etc.
 *
 * Line Tag handling. Line and Sector triggers.
 *
 * Events are operations triggered by using, crossing, or shooting special
 * lines, or by timed thinkers.
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "gamemap.h"
#include "m_argv.h"
#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"
#include "p_player.h"
#include "p_tick.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_plat.h"
#include "p_switch.h"
#include "d_netsv.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef enum afxcmd_e {
    afxcmd_play,                // (sound)
    afxcmd_playabsvol,          // (sound, volume)
    afxcmd_playrelvol,          // (sound, volume)
    afxcmd_delay,               // (ticks)
    afxcmd_delayrand,           // (andbits)
    afxcmd_end                  // ()
} afxcmd_t;

#pragma pack(1)
typedef struct animdef_s {
    /* Do NOT change these members in any way */
    signed char istexture;  // if false, it is a flat (instead of bool)
    char        endname[9];
    char        startname[9];
    int         speed;
} animdef_t;
#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_CrossSpecialLine(linedef_t* line, int side, mobj_t* thing);
static void P_ShootSpecialLine(mobj_t* thing, linedef_t* line);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static animdef_t anims[] = {
    {0, "FLTWAWA3", "FLTWAWA1", MACRO_LONG(8)},
    {0, "FLTSLUD3", "FLTSLUD1", MACRO_LONG(8)},
    {0, "FLTTELE4", "FLTTELE1", MACRO_LONG(6)},
    {0, "FLTFLWW3", "FLTFLWW1", MACRO_LONG(9)},
    {0, "FLTLAVA4", "FLTLAVA1", MACRO_LONG(8)},
    {0, "FLATHUH4", "FLATHUH1", MACRO_LONG(8)},
    {1, "LAVAFL3",  "LAVAFL1",  MACRO_LONG(6)},
    {1, "WATRWAL3", "WATRWAL1", MACRO_LONG(4)},
    {-1, "\0",      "\0"}
};

static const int AmbSndSeqInit[] = { // Startup
    afxcmd_end
};

static const int AmbSndSeq1[] = { // Scream
    afxcmd_play, SFX_AMB1,
    afxcmd_end
};

static const int AmbSndSeq2[] = { // Squish
    afxcmd_play, SFX_AMB2,
    afxcmd_end
};

static const int AmbSndSeq3[] = { // Drops
    afxcmd_play, SFX_AMB3,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, SFX_AMB7,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, SFX_AMB3,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, SFX_AMB7,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, SFX_AMB3,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, SFX_AMB7,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_end
};

static const int AmbSndSeq4[] = { // SlowFootSteps
    afxcmd_play, SFX_AMB4,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_end
};

static const int AmbSndSeq5[] = { // Heartbeat
    afxcmd_play, SFX_AMB5,
    afxcmd_delay, 35,
    afxcmd_play, SFX_AMB5,
    afxcmd_delay, 35,
    afxcmd_play, SFX_AMB5,
    afxcmd_delay, 35,
    afxcmd_play, SFX_AMB5,
    afxcmd_end
};

static const int AmbSndSeq6[] = { // Bells
    afxcmd_play, SFX_AMB6,
    afxcmd_delay, 17,
    afxcmd_playrelvol, SFX_AMB6, -8,
    afxcmd_delay, 17,
    afxcmd_playrelvol, SFX_AMB6, -8,
    afxcmd_delay, 17,
    afxcmd_playrelvol, SFX_AMB6, -8,
    afxcmd_end
};

static const int AmbSndSeq7[] = { // Growl
    afxcmd_play, SFX_BSTSIT,
    afxcmd_end
};

static const int AmbSndSeq8[] = { // Magic
    afxcmd_play, SFX_AMB8,
    afxcmd_end
};

static const int AmbSndSeq9[] = { // Laughter
    afxcmd_play, SFX_AMB9,
    afxcmd_delay, 16,
    afxcmd_playrelvol, SFX_AMB9, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, SFX_AMB9, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, SFX_AMB10, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, SFX_AMB10, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, SFX_AMB10, -4,
    afxcmd_end
};

static const int AmbSndSeq10[] = { // FastFootsteps
    afxcmd_play, SFX_AMB4,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_end
};

static const int* AmbientSfx[] = {
    AmbSndSeq1, // Scream
    AmbSndSeq2, // Squish
    AmbSndSeq3, // Drops
    AmbSndSeq4, // SlowFootsteps
    AmbSndSeq5, // Heartbeat
    AmbSndSeq6, // Bells
    AmbSndSeq7, // Growl
    AmbSndSeq8, // Magic
    AmbSndSeq9, // Laughter
    AmbSndSeq10 // FastFootsteps
};

// CODE --------------------------------------------------------------------

/**
 * From PrBoom:
 * Load the table of animation definitions, checking for existence of
 * the start and end of each frame. If the start doesn't exist the sequence
 * is skipped, if the last doesn't exist, BOOM exits.
 *
 * Wall/Flat animation sequences, defined by name of first and last frame,
 * The full animation sequence is given using all lumps between the start
 * and end entry, in the order found in the WAD file.
 */
static void loadAnimatedDefs(animdef_t* animDefs)
{
    int                 i;

    // Read structures until -1 is found
    for(i = 0; animDefs[i].istexture != -1 ; ++i)
    {
        int                 groupNum, ticsPerFrame, numFrames;
        material_namespace_t     mnamespace =
            (animDefs[i].istexture? MN_TEXTURES : MN_FLATS);

        switch(mnamespace)
        {
        case MN_FLATS:
            {
            int                 startFrame, endFrame, n;

            if((startFrame = R_TextureIdForName(MN_FLATS, animDefs[i].startname)) == -1 ||
               (endFrame = R_TextureIdForName(MN_FLATS, animDefs[i].endname)) == -1)
                continue;

            numFrames = endFrame - startFrame + 1;
            ticsPerFrame = LONG(animDefs[i].speed);

            if(numFrames < 2)
                Con_Error("P_InitPicAnims: bad cycle from %s to %s",
                          animDefs[i].startname, animDefs[i].endname);

            if(startFrame && endFrame)
            {   // We have a valid animation.
                // Create a new animation group for it.
                groupNum = P_NewMaterialGroup(AGF_SMOOTH);

                /**
                 * Doomsday's group animation needs to know the texture/flat
                 * numbers of ALL frames in the animation group so we'll
                 * have to step through the directory adding frames as we
                 * go. (DOOM only required the start/end texture/flat
                 * numbers and would animate all textures/flats inbetween).
                 */

                VERBOSE(Con_Message("P_InitPicAnims: ADD (\"%s\" > \"%s\" %d)\n",
                                    animDefs[i].startname, animDefs[i].endname,
                                    ticsPerFrame));

                // Add all frames from start to end to the group.
                if(endFrame > startFrame)
                {
                    for(n = startFrame; n <= endFrame; n++)
                    {
                        material_t*         mat = R_MaterialForTextureId(MN_FLATS, n);

                        if(mat)
                            P_AddMaterialToGroup(groupNum, P_ToIndex(mat), ticsPerFrame, 0);
                    }
                }
                else
                {
                    for(n = endFrame; n >= startFrame; n--)
                    {
                        material_t*         mat = R_MaterialForTextureId(MN_FLATS, n);

                        if(mat)
                            P_AddMaterialToGroup(groupNum, P_ToIndex(mat), ticsPerFrame, 0);
                    }
                }
            }
            break;
            }
        case MN_TEXTURES:
            {   // Same as above but for texture groups.
            int                 startFrame, endFrame, n;

            if((startFrame = R_TextureIdForName(MN_TEXTURES, animDefs[i].startname)) == -1 ||
               (endFrame = R_TextureIdForName(MN_TEXTURES, animDefs[i].endname)) == -1)
                continue;

            numFrames = endFrame - startFrame + 1;
            ticsPerFrame = LONG(animDefs[i].speed);

            if(numFrames < 2)
                Con_Error("P_InitPicAnims: bad cycle from %s to %s",
                          animDefs[i].startname, animDefs[i].endname);

            if(startFrame && endFrame)
            {
                groupNum = P_NewMaterialGroup(AGF_SMOOTH);

                VERBOSE(Con_Message("P_InitPicAnims: ADD (\"%s\" > \"%s\" %d)\n",
                                    animDefs[i].startname, animDefs[i].endname,
                                    ticsPerFrame));

                if(endFrame > startFrame)
                {
                    for(n = startFrame; n <= endFrame; n++)
                        P_AddMaterialToGroup(groupNum, P_ToIndex(R_MaterialForTextureId(MN_TEXTURES, n)), ticsPerFrame, 0);
                }
                else
                {
                    for(n = endFrame; n >= startFrame; n--)
                        P_AddMaterialToGroup(groupNum, P_ToIndex(R_MaterialForTextureId(MN_TEXTURES, n)), ticsPerFrame, 0);
                }
            }
            break;
            }
        default:
            Con_Error("loadAnimDefs: Internal Error, invalid namespace %i.",
                      (int) mnamespace);
        }
    }
}

void P_InitPicAnims(void)
{
    Con_Message("P_InitPicAnims: Registering default animations...\n");
    loadAnimatedDefs(anims);
}

boolean P_ActivateLine(linedef_t *ld, mobj_t *mo, int side, int actType)
{
    switch(actType)
    {
    case SPAC_CROSS:
        P_CrossSpecialLine(ld, side, mo);
        return true;

    case SPAC_USE:
        return P_UseSpecialLine(mo, ld, side);

    case SPAC_IMPACT:
        P_ShootSpecialLine(mo, ld);
        return true;

    default:
        Con_Error("P_ActivateLine: Unknown Activation Type %i", actType);
        break;
    }

    return false;
}

/**
 * Called every time a thing origin is about to cross a line with
 * a non 0 special.
 */
static void P_CrossSpecialLine(linedef_t *line, int side, mobj_t *thing)
{
    int                 ok;
    XLineDef*            xline;

    // Extended functionality overrides old.
    if(XL_CrossLine(line, side, thing))
        return;

    xline = P_ToXLine(line);

    // Triggers that other things can activate.
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
        switch(xline->special)
        {
        case 39:                // TELEPORT TRIGGER
        case 97:                // TELEPORT RETRIGGER
      //case 125:               // TELEPORT MONSTERONLY TRIGGER
      //case 126:               // TELEPORT MONSTERONLY RETRIGGER
        case 4:                 // RAISE DOOR
      //case 10:                // PLAT DOWN-WAIT-UP-STAY TRIGGER
      //case 88:                // PLAT DOWN-WAIT-UP-STAY RETRIGGER
            ok = 1;
            break;
        }
/*      // DJS - Not implemented in jHeretic
        // Anything can trigger this line!
        if(DMU_GetInt(DMU_LINEDEF, linenum, DMU_FLAGS) & ML_ALLTRIGGER)
            ok = 1;
*/
        if(!ok)
            return;
    }

    // Note: could use some const's here.
    switch(xline->special)
    {
        // TRIGGERS.
        // All from here to RETRIGGERS.
    case 2:
        // Open Door
        EV_DoDoor(line, DT_OPEN);
        xline->special = 0;
        break;

    case 3:
        // Close Door
        EV_DoDoor(line, DT_CLOSE);
        xline->special = 0;
        break;

    case 4:
        // Raise Door
        EV_DoDoor(line, DT_NORMAL);
        xline->special = 0;
        break;

    case 5:
        // Raise Floor
        EV_DoFloor(line, FT_RAISEFLOOR);
        xline->special = 0;
        break;

    case 6:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, CT_CRUSHANDRAISEFAST);
        xline->special = 0;
        break;

    case 8:
        // Build Stairs
        EV_BuildStairs(line, build8);
        xline->special = 0;
        break;

    case 10:
        // PlatDownWaitUp
        EV_DoPlat(line, PT_DOWNWAITUPSTAY, 0);
        xline->special = 0;
        break;

    case 12:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        xline->special = 0;
        break;

    case 13:
        // Light Turn On 255
        EV_LightTurnOn(line, 1);
        xline->special = 0;
        break;

    case 16:
        // Close Door 30
        EV_DoDoor(line, DT_CLOSE30THENOPEN);
        xline->special = 0;
        break;

    case 17:
        // Start Light Strobing
        EV_StartLightStrobing(line);
        xline->special = 0;
        break;

    case 19:
        // Lower Floor
        EV_DoFloor(line, FT_LOWER);
        xline->special = 0;
        break;

    case 22:
        // Raise floor to nearest height and change texture
        EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0);
        xline->special = 0;
        break;

    case 25:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, CT_CRUSHANDRAISE);
        xline->special = 0;
        break;

    case 30:
        // Raise floor to shortest texture height
        // on either side of lines.
        EV_DoFloor(line, FT_RAISETOTEXTURE);
        xline->special = 0;
        break;

    case 35:
        // Lights Very Dark
        EV_LightTurnOn(line, 35.0f/255.0f);
        xline->special = 0;
        break;

    case 36:
        // Lower Floor (TURBO)
        EV_DoFloor(line, FT_LOWERTURBO);
        xline->special = 0;
        break;

    case 37:
        // LowerAndChange
        EV_DoFloor(line, FT_LOWERANDCHANGE);
        xline->special = 0;
        break;

    case 38:
        // Lower Floor To Lowest
        EV_DoFloor(line, FT_LOWERTOLOWEST);
        xline->special = 0;
        break;

    case 39:
        // TELEPORT!
        EV_Teleport(line, side, thing, true);
        xline->special = 0;
        break;

    case 40:
        // RaiseCeilingLowerFloor
        EV_DoCeiling(line, CT_RAISETOHIGHEST);
        EV_DoFloor(line, FT_LOWERTOLOWEST);
        xline->special = 0;
        break;

    case 44:
        // Ceiling Crush
        EV_DoCeiling(line, CT_LOWERANDCRUSH);
        xline->special = 0;
        break;

    case 52:
        // EXIT!
        G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, false), 0, false);
        break;

    case 53:
        // Perpetual Platform Raise
        EV_DoPlat(line, PT_PERPETUALRAISE, 0);
        xline->special = 0;
        break;

    case 54:
        // Platform Stop
        P_PlatDeactivate(P_CurrentMap(), xline->tag);
        xline->special = 0;
        break;

    case 56:
        // Raise Floor Crush
        EV_DoFloor(line, FT_RAISEFLOORCRUSH);
        xline->special = 0;
        break;

    case 57:
        // Ceiling Crush Stop
        P_CeilingDeactivate(P_CurrentMap(), xline->tag);
        xline->special = 0;
        break;

    case 58:
        // Raise Floor 24
        EV_DoFloor(line, FT_RAISE24);
        xline->special = 0;
        break;

    case 59:
        // Raise Floor 24 And Change
        EV_DoFloor(line, FT_RAISE24ANDCHANGE);
        xline->special = 0;
        break;

    case 104:
        // Turn lights off in sector(tag)
        EV_TurnTagLightsOff(line);
        xline->special = 0;
        break;

/*
    // DJS - This stuff isn't in Heretic
    case 108:
        // Blazing Door Raise (faster than TURBO!)
        EV_DoDoor(line, DT_BLAZERAISE);
        xline->special = 0;
        break;

    case 109:
        // Blazing Door Open (faster than TURBO!)
        EV_DoDoor(line, DT_BLAZEOPEN);
        xline->special = 0;
        break;

    case 100:
        // Build Stairs Turbo 16
        EV_BuildStairs(line, turbo16);
        xline->special = 0;
        break;

    case 110:
        // Blazing Door Close (faster than TURBO!)
        EV_DoDoor(line, DT_BLAZECLOSE);
        xline->special = 0;
        break;

    case 119:
        // Raise floor to nearest surr. floor
        EV_DoFloor(line, FT_RAISEFLOORTONEAREST);
        xline->special = 0;
        break;

    case 121:
        // Blazing PlatDownWaitUpStay
        EV_DoPlat(line, PT_DOWNWAITUPSTAYBLAZE, 0);
        xline->special = 0;
        break;
*/
  //case 124: // DJS - In Heretic, the secret exit is 105
    case 105:
        // Secret EXIT
        G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, true), 0, true);
        break;

    // DJS - Heretic has an additional stair build special
    //       that moves in steps of 16.
    case 106:
        // Build Stairs
        EV_BuildStairs(line, build16);
        xline->special = 0;
        break;

/*  // DJS - more specials that arn't in Heretic
    case 125:
        // TELEPORT MonsterONLY
        if(!thing->player)
        {
            EV_Teleport(line, side, thing);
            xline->special = 0;
        }
        break;

    case 130:
        // Raise Floor Turbo
        EV_DoFloor(line, FT_RAISEFLOORTURBO);
        xline->special = 0;
        break;

    case 141:
        // Silent Ceiling Crush & Raise
        EV_DoCeiling(line, CT_SILENTCRUSHANDRAISE);
        xline->special = 0;
        break;
*/

        // RETRIGGERS.  All from here till end.
    case 72:
        // Ceiling Crush
        EV_DoCeiling(line, CT_LOWERANDCRUSH);
        break;

    case 73:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, CT_CRUSHANDRAISE);
        break;

    case 74:
        // Ceiling Crush Stop
        P_CeilingDeactivate(P_CurrentMap(), xline->tag);
        break;

    case 75:
        // Close Door
        EV_DoDoor(line, DT_CLOSE);
        break;

    case 76:
        // Close Door 30
        EV_DoDoor(line, DT_CLOSE30THENOPEN);
        break;

    case 77:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, CT_CRUSHANDRAISEFAST);
        break;

    case 79:
        // Lights Very Dark
        EV_LightTurnOn(line, 35.0f/255.0f);
        break;

    case 80:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        break;

    case 81:
        // Light Turn On 255
        EV_LightTurnOn(line, 1);
        break;

    case 82:
        // Lower Floor To Lowest
        EV_DoFloor(line, FT_LOWERTOLOWEST);
        break;

    case 83:
        // Lower Floor
        EV_DoFloor(line, FT_LOWER);
        break;

    case 84:
        // LowerAndChange
        EV_DoFloor(line, FT_LOWERANDCHANGE);
        break;

    case 86:
        // Open Door
        EV_DoDoor(line, DT_OPEN);
        break;

    case 87:
        // Perpetual Platform Raise
        EV_DoPlat(line, PT_PERPETUALRAISE, 0);
        break;

    case 88:
        // PlatDownWaitUp
        EV_DoPlat(line, PT_DOWNWAITUPSTAY, 0);
        break;

    case 89:
        // Platform Stop
        P_PlatDeactivate(P_CurrentMap(), xline->tag);
        break;

    case 90:
        // Raise Door
        EV_DoDoor(line, DT_NORMAL);
        break;

    case 91:
        // Raise Floor
        EV_DoFloor(line, FT_RAISEFLOOR);
        break;

    case 92:
        // Raise Floor 24
        EV_DoFloor(line, FT_RAISE24);
        break;

    case 93:
        // Raise Floor 24 And Change
        EV_DoFloor(line, FT_RAISE24ANDCHANGE);
        break;

    case 94:
        // Raise Floor Crush
        EV_DoFloor(line, FT_RAISEFLOORCRUSH);
        break;

    case 95:
        // Raise floor to nearest height
        // and change texture.
        EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0);
        break;

    case 96:
        // Raise floor to shortest texture height
        // on either side of lines.
        EV_DoFloor(line, FT_RAISETOTEXTURE);
        break;

    case 97:
        // TELEPORT!
        EV_Teleport(line, side, thing, true);
        break;

    case 100:
        // DJS - Heretic has one turbo door raise
        EV_DoDoor(line, DT_BLAZEOPEN);
        break;

/*
    // DJS - Yet more specials not in Heretic
    case 98:
        // Lower Floor (TURBO)
        EV_DoFloor(line, FT_LOWERTURBO);
        break;

    case 105:
        // Blazing Door Raise (faster than TURBO!)
        EV_DoDoor(line, DT_BLAZERAISE);
        break;

    case 106:
        // Blazing Door Open (faster than TURBO!)
        EV_DoDoor(line, DT_BLAZEOPEN);
        break;

    case 107:
        // Blazing Door Close (faster than TURBO!)
        EV_DoDoor(line, DT_BLAZECLOSE);
        break;

    case 120:
        // Blazing PlatDownWaitUpStay.
        EV_DoPlat(line, PT_DOWNWAITUPSTAYBLAZE, 0);
        break;

    case 126:
        // TELEPORT MonsterONLY.
        if(!thing->player)
            EV_Teleport(line, side, thing);
        break;

    case 128:
        // Raise To Nearest Floor
        EV_DoFloor(line, FT_RAISEFLOORTONEAREST);
        break;

    case 129:
        // Raise Floor Turbo
        EV_DoFloor(line, FT_RAISEFLOORTURBO);
        break;
*/
    }
}

/**
 * Called when a thing shoots a special line.
 */
static void P_ShootSpecialLine(mobj_t* thing, linedef_t* line)
{
    XLineDef*            xline = P_ToXLine(line);

    // Impacts that other things can activate.
    if(!thing->player)
    {
        switch(xline->special)
        {
        case 46:
            // OPEN DOOR IMPACT
            break;

        default:
            return;
        }
    }

    switch(xline->special)
    {
    case 24:
        // RAISE FLOOR
        EV_DoFloor(line, FT_RAISEFLOOR);
        P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
        xline->special = 0;
        break;

    case 46:
        // OPEN DOOR
        EV_DoDoor(line, DT_OPEN);
        P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 47:
        // RAISE FLOOR NEAR AND CHANGE
        EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0);
        P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
        xline->special = 0;
        break;

    default:
        break;
    }
}

/**
 * Called every tic frame that the player origin is in a special sector.
 */
void P_PlayerInSpecialSector(player_t* player)
{
    GameMap* map = Thinker_Map((thinker_t*) player->plr->mo);
    sector_t* sector = DMU_GetPtrp(player->plr->mo->subsector, DMU_SECTOR);

    // Falling, not all the way down yet?
    if(player->plr->mo->pos[VZ] != DMU_GetFloatp(sector, DMU_FLOOR_HEIGHT))
        return;

    // Has hitten ground.
    switch(P_ToXSector(sector)->special)
    {
    case 5:
        // LAVA DAMAGE WEAK
        if(!(map->time & 15))
        {
            P_DamageMobj(player->plr->mo, &map->lavaInflictor, NULL, 5, false);
            P_HitFloor(player->plr->mo);
        }
        break;

    case 7:
        // SLUDGE DAMAGE
        if(!(map->time & 31))
            P_DamageMobj(player->plr->mo, NULL, NULL, 4, false);
        break;

    case 16:
        // LAVA DAMAGE HEAVY
        if(!(map->time & 15))
        {
            P_DamageMobj(player->plr->mo, &map->lavaInflictor, NULL, 8, false);
            P_HitFloor(player->plr->mo);
        }
        break;

    case 4:
        // LAVA DAMAGE WEAK PLUS SCROLL EAST
        P_Thrust(player, 0, FIX2FLT(2048 * 28));
        if(!(map->time & 15))
        {
            P_DamageMobj(player->plr->mo, &map->lavaInflictor, NULL, 5, false);
            P_HitFloor(player->plr->mo);
        }
        break;

    case 9:
        // SECRET SECTOR
        player->secretCount++;
        P_ToXSector(sector)->special = 0;
        if(cfg.secretMsg)
        {
            P_SetMessage(player, "You've found a secret area!", false);
            S_ConsoleSound(SFX_WPNUP, 0, player - players);
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

/**
 * Animate planes, scroll walls, etc.
 */
void GameMap_UpdateSpecials(GameMap* map)
{
    assert(map);
    {
#define PLANE_MATERIAL_SCROLLUNIT (8.f/35*2)

    linedef_t* line;
    sidedef_t* side;
    uint i;
    float x;

    // Extended lines and sectors.
    XG_Ticker();

    // Update scrolling plane materials.
    for(i = 0; i < Map_NumSectors(map); ++i)
    {
        XSector*          sect = P_ToXSector(P_ToPtr(DMU_SECTOR, i));
        float               texOff[2];

        switch(sect->special)
        {
        case 25: // Scroll north.
        case 26:
        case 27:
        case 28:
        case 29:
            texOff[VY] = P_GetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_Y);
            texOff[VY] -= PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 25);
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_Y, texOff[VY]);
            break;

        case 20: // Scroll east.
        case 21:
        case 22:
        case 23:
        case 24:
            texOff[VX] = P_GetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X);
            texOff[VX] -= PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 20);
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X, texOff[VX]);
            break;

        case 4: // Scroll east (lava damage).
            texOff[VX] = P_GetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X);
            texOff[VX] -= PLANE_MATERIAL_SCROLLUNIT * 8 * (1 + sect->special - 4);
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X, texOff[VX]);
            break;

        case 30: // Scroll south.
        case 31:
        case 32:
        case 33:
        case 34:
            texOff[VY] = P_GetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_Y);
            texOff[VY] += PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 30);
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_Y, texOff[VY]);
            break;

        case 35: // Scroll west.
        case 36:
        case 37:
        case 38:
        case 39:
            texOff[VX] = P_GetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X);
            texOff[VX] += PLANE_MATERIAL_SCROLLUNIT * (1 + sect->special - 35);
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X, texOff[VX]);
            break;

        default:
            // DJS - Is this really necessary every tic?
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_X, 0);
            P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_MATERIAL_OFFSET_Y, 0);
            break;
        }
    }

    // ANIMATE LINE SPECIALS
    if(P_IterListSize(map->_linespecials))
    {
        P_IterListResetIterator(map->_linespecials, false);
        while((line = P_IterListIterator(map->_linespecials)) != NULL)
        {
            switch(P_ToXLine(line)->special)
            {
            case 48:
                side = DMU_GetPtrp(line, DMU_SIDEDEF0);

                // EFFECT FIRSTCOL SCROLL +
                x = DMU_GetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X);
                DMU_SetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X, x + 1);
                x = DMU_GetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X);
                DMU_SetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X, x + 1);
                x = DMU_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X);
                DMU_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X, x + 1);
                break;

            // DJS - Heretic also has a backwards wall scroller.
            case 99:
                side = DMU_GetPtrp(line, DMU_SIDEDEF0);

                // EFFECT FIRSTCOL SCROLL +
                x = DMU_GetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X);
                DMU_SetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X, x - 1);
                x = DMU_GetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X);
                DMU_SetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X, x - 1);
                x = DMU_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X);
                DMU_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X, x - 1);
                break;

            default:
                break;
            }
        }
    }

#undef PLANE_MATERIAL_SCROLLUNIT
    }
}

/**
 * After the map has been loaded, scan for specials that spawn thinkers.
 */
void GameMap_SpawnSpecials(GameMap* map)
{
    assert(map);
    {
    uint i;
    linedef_t* line;
    XLineDef* xline;
    IterList* list;
    sector_t* sec;
    XSector* xsec;

    // Init special SECTORs.
    GameMap_DestroySectorTagLists(map);
    for(i = 0; i < Map_NumSectors(map); ++i)
    {
        sec = DMU_ToPtr(DMU_SECTOR, i);
        xsec = P_ToXSector(sec);

        if(xsec->tag)
        {
            list = GameMap_SectorIterListForTag(map, xsec->tag, true);
            P_AddObjectToIterList(list, sec);
        }

        if(!xsec->special)
            continue;

        if(IS_CLIENT)
        {
            switch(xsec->special)
            {
            case 9:
                // SECRET SECTOR
                map->totalSecret++;
                break;

            default:
                break;
            }

            continue;
        }

        switch(xsec->special)
        {
        case 1:
            // FLICKERING LIGHTS
            P_SpawnLightFlash(sec);
            break;

        case 2:
            // STROBE FAST
            P_SpawnStrobeFlash(sec, FASTDARK, 0);
            break;

        case 3:
            // STROBE SLOW
            P_SpawnStrobeFlash(sec, SLOWDARK, 0);
            break;

        case 4:
            // STROBE FAST/DEATH SLIME
            P_SpawnStrobeFlash(sec, FASTDARK, 0);
            xsec->special = 4;
            break;

        case 8:
            // GLOWING LIGHT
            P_SpawnGlowingLight(sec);
            break;

        case 9:
            // SECRET SECTOR
            map->totalSecret++;
            break;

        case 10:
            // DOOR CLOSE IN 30 SECONDS
            P_SpawnDoorCloseIn30(sec);
            break;

        case 12:
            // SYNC STROBE SLOW
            P_SpawnStrobeFlash(sec, SLOWDARK, 1);
            break;

        case 13:
            // SYNC STROBE FAST
            P_SpawnStrobeFlash(sec, FASTDARK, 1);
            break;

        case 14:
            // DOOR RAISE IN 5 MINUTES
            P_SpawnDoorRaiseIn5Mins(sec);
            break;

/*
        // DJS - Heretic doesn't use these.
        case 17:
            P_SpawnFireFlicker(sec);
            break;
*/
        default:
            break;
        }
    }

    // Init animating line specials.
    P_EmptyIterList(map->_linespecials);
    GameMap_DestroyLineTagLists(map);
    for(i = 0; i < Map_NumLineDefs(map); ++i)
    {
        line = DMU_ToPtr(DMU_LINEDEF, i);
        xline = P_ToXLine(line);

        switch(xline->special)
        {
        case 48:
            // EFFECT FIRSTCOL SCROLL+
        case 99:
            // EFFECT FIRSTCOL SCROLL-
            // DJS - Heretic also has a backwards wall scroller.
            P_AddObjectToIterList(map->_linespecials, line);
            break;

        default:
            break;
        }

        if(xline->tag)
        {
           list = GameMap_IterListForTag(map, xline->tag, true);
           P_AddObjectToIterList(list, line);
        }
    }

    // Init extended generalized lines and sectors.
    XG_Init(map);
    }
}

void GameMap_InitLava(GameMap* map)
{
    assert(map);

    memset(&map->lavaInflictor, 0, sizeof(map->lavaInflictor));
    map->lavaInflictor.type = MT_PHOENIXFX2;
    map->lavaInflictor.flags2 = MF2_FIREDAMAGE | MF2_NODMGTHRUST;
}

/**
 * Handles sector specials 25 - 39.
 */
void P_PlayerInWindSector(player_t* player)
{
    sector_t* sector = DMU_GetPtrp(player->plr->mo->subsector, DMU_SECTOR);

    static int pushTab[5] = {
        2048 * 5,
        2048 * 10,
        2048 * 25,
        2048 * 30,
        2048 * 35
    };

    switch(P_ToXSector(sector)->special)
    {
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
        // Scroll_North
        P_Thrust(player, ANG90, FIX2FLT(pushTab[P_ToXSector(sector)->special - 25]));
        break;

    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
        // Scroll_East
        P_Thrust(player, 0, FIX2FLT(pushTab[P_ToXSector(sector)->special - 20]));
        break;

    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
        // Scroll_South
        P_Thrust(player, ANG270, FIX2FLT(pushTab[P_ToXSector(sector)->special - 30]));
        break;

    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
        // Scroll_West
        P_Thrust(player, ANG180, FIX2FLT(pushTab[P_ToXSector(sector)->special - 35]));
        break;
    }

    // The other wind types (40..51).
    P_WindThrust(player->plr->mo);
}

void GameMap_InitAmbientSfx(GameMap* map)
{
    assert(map);

    map->ambientSfxCount = 0;
    map->ambSfx.volume = 0;
    map->ambSfx.tics = 10 * TICSPERSEC;
    map->ambSfx.ptr = AmbSndSeqInit;
}

void GameMap_AddAmbientSfx(GameMap* map, int sequence)
{
    assert(map);

    if(map->ambientSfxCount == MAX_AMBIENT_SFX)
    {
        Con_Error("Too many ambient sound sequences");
    }
    map->ambientSfx[map->ambientSfxCount++] = AmbientSfx[sequence];
}

void GameMap_PlayAmbientSfx(GameMap* map)
{
    assert(map);
    {
    afxcmd_t cmd;
    int sound;
    boolean done;

    if(!map->ambientSfxCount)
        return; // No ambient sound sequences on current map.

    if(--map->ambSfx.tics)
        return;

    done = false;
    do
    {
        cmd = *map->ambSfx.ptr++;
        switch(cmd)
        {
        case afxcmd_play:
            map->ambSfx.volume = P_Random() >> 2;
            S_StartSoundAtVolume(*map->ambSfx.ptr++, NULL, map->ambSfx.volume / 127.0f);
            break;

        case afxcmd_playabsvol:
            sound = *map->ambSfx.ptr++;
            map->ambSfx.volume = *map->ambSfx.ptr++;
            S_StartSoundAtVolume(sound, NULL, map->ambSfx.volume / 127.0f);
            break;

        case afxcmd_playrelvol:
            sound = *map->ambSfx.ptr++;
            map->ambSfx.volume += *map->ambSfx.ptr++;

            if(map->ambSfx.volume < 0)
                map->ambSfx.volume = 0;
            else if(map->ambSfx.volume > 127)
                map->ambSfx.volume = 127;

            S_StartSoundAtVolume(sound, NULL, map->ambSfx.volume / 127.0f);
            break;

        case afxcmd_delay:
            map->ambSfx.tics = *map->ambSfx.ptr++;
            done = true;
            break;

        case afxcmd_delayrand:
            map->ambSfx.tics = P_Random() & (*map->ambSfx.ptr++);
            done = true;
            break;

        case afxcmd_end:
            map->ambSfx.tics = 6 * TICSPERSEC + P_Random();
            map->ambSfx.ptr = map->ambientSfx[P_Random() % map->ambientSfxCount];
            done = true;
            break;

        default:
            Con_Error("GameMap_PlayAmbientSfx: Unknown afxcmd %d", cmd);
            break;
        }
    } while(done == false);
    }
}

boolean P_UseSpecialLine2(mobj_t* mo, linedef_t* line, int side)
{
    XLineDef* xline = P_ToXLine(line);

    // Switches that other things can activate.
    if(!mo->player)
    {
        // never DT_OPEN secret doors
        if(xline->flags & ML_SECRET)
            return false;
    }

    if(!mo->player)
    {
        switch(xline->special)
        {
        case 1: // MANUAL DOOR RAISE
        case 32: // MANUAL BLUE
        case 33: // MANUAL RED
        case 34: // MANUAL YELLOW
            break;

        default:
            return false;
        }
    }

    // Do something.
    switch(xline->special)
    {
    // MANUALS
    case 1: // Vertical Door
    case 26: // Blue Door/Locked
    case 27: // Yellow Door /Locked
    case 28: // Red Door /Locked

    case 31: // Manual door DT_OPEN
    case 32: // Blue locked door DT_OPEN
    case 33: // Red locked door DT_OPEN
    case 34: // Yellow locked door DT_OPEN
        EV_VerticalDoor(line, mo);
        break;

    // SWITCHES
    case 7: // Switch_Build_Stairs (8 pixel steps)
        if(EV_BuildStairs(line, build8))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 107: // Switch_Build_Stairs_16 (16 pixel steps)
        if(EV_BuildStairs(line, build16))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 9: // Change Donut.
        if(EV_DoDonut(line))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 11: // Exit level.
        if(cyclingMaps && mapCycleNoExit)
            break;

        G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, false), 0, false);
        P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
        xline->special = 0;
        break;

    case 14: // Raise Floor 32 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 32))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 15: // Raise Floor 24 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 24))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 18: // Raise Floor to next highest floor.
        if(EV_DoFloor(line, FT_RAISEFLOORTONEAREST))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 20: // Raise Plat next highest floor and change texture.
        if(EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 21: // PlatDownWaitUpStay.
        if(EV_DoPlat(line, PT_DOWNWAITUPSTAY, 0))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 23: // Lower Floor to Lowest.
        if(EV_DoFloor(line, FT_LOWERTOLOWEST))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 29: // Raise Door.
        if(EV_DoDoor(line, DT_NORMAL))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 41: // Lower Ceiling to Floor.
        if(EV_DoCeiling(line, CT_LOWERTOFLOOR))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 71: // Turbo Lower Floor.
        if(EV_DoFloor(line, FT_LOWERTURBO))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 49: // Lower Ceiling And Crush.
        if(EV_DoCeiling(line, CT_LOWERANDCRUSH))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 50: // Close Door.
        if(EV_DoDoor(line, DT_CLOSE))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 51: // Secret EXIT.
        if(cyclingMaps && mapCycleNoExit)
            break;

        G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, true), 0, true);
        P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
        xline->special = 0;
        break;

    case 55: // Raise Floor Crush.
        if(EV_DoFloor(line, FT_RAISEFLOORCRUSH))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 101: // Raise Floor.
        if(EV_DoFloor(line, FT_RAISEFLOOR))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 102: // Lower Floor to Surrounding floor height.
        if(EV_DoFloor(line, FT_LOWER))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 103: // Open Door.
        if(EV_DoDoor(line, DT_OPEN))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    // BUTTONS
    case 42: // Close Door.
        if(EV_DoDoor(line, DT_CLOSE))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 43: // Lower Ceiling to Floor.
        if(EV_DoCeiling(line, CT_LOWERTOFLOOR))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 45: // Lower Floor to Surrounding floor height.
        if(EV_DoFloor(line, FT_LOWER))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 60: // Lower Floor to Lowest.
        if(EV_DoFloor(line, FT_LOWERTOLOWEST))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 61: // Open Door.
        if(EV_DoDoor(line, DT_OPEN))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 62: // PlatDownWaitUpStay.
        if(EV_DoPlat(line, PT_DOWNWAITUPSTAY, 1))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 63: // Raise Door.
        if(EV_DoDoor(line, DT_NORMAL))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 64: // Raise Floor to ceiling.
        if(EV_DoFloor(line, FT_RAISEFLOOR))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 66: // Raise Floor 24 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 24))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 67: // Raise Floor 32 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 32))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 65: // Raise Floor Crush.
        if(EV_DoFloor(line, FT_RAISEFLOORCRUSH))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 68: // Raise Plat to next highest floor and change texture.
        if(EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 69: // Raise Floor to next highest floor.
        if(EV_DoFloor(line, FT_RAISEFLOORTONEAREST))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 70: // Turbo Lower Floor.
        if(EV_DoFloor(line, FT_LOWERTURBO))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    default:
        break;
    }

    return true;
}
