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
 * p_spec.c: Implements map special effects.
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

#include "jdoom.h"

#include "m_argv.h"
#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_player.h"
#include "p_mapspec.h"
#include "p_tick.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_plat.h"
#include "p_switch.h"
#include "d_netsv.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

#pragma pack(1)
typedef struct animdef_s {
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

static void crossSpecialLine(linedef_t* line, int side, mobj_t* thing);
static void shootSpecialLine(mobj_t* thing, linedef_t* line);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// These arrays are treated as a hardcoded replacements for data that can be
// loaded from a lump, so we need to use little-endian byte ordering.
static animdef_t animsShared[] = {
    {0, "BLOOD3",   "BLOOD1",   MACRO_LONG(8)},
    {0, "FWATER4",  "FWATER1",  MACRO_LONG(8)},
    {0, "LAVA4",    "LAVA1",    MACRO_LONG(8)},
    {0, "NUKAGE3",  "NUKAGE1",  MACRO_LONG(8)},
    {1, "BLODRIP4", "BLODRIP1", MACRO_LONG(8)},
    {1, "FIREBLU2", "FIREBLU1", MACRO_LONG(8)},
    {1, "FIRELAVA", "FIRELAV2", MACRO_LONG(8)},
    {1, "FIREMAG3", "FIREMAG1", MACRO_LONG(8)},
    {1, "FIREWALL", "FIREWALA", MACRO_LONG(8)},
    {1, "GSTFONT3", "GSTFONT1", MACRO_LONG(8)},
    {1, "ROCKRED3", "ROCKRED1", MACRO_LONG(8)},
    {1, "SLADRIP3", "SLADRIP1", MACRO_LONG(8)},
    {1, "WFALL4",   "WFALL1",   MACRO_LONG(8)},
    {1, "BLODGR4",  "BLODGR1",  MACRO_LONG(8)},
    {-1, "\0",      "\0"}
};

static animdef_t animsDoom2[] = {
    {0, "RROCK08",  "RROCK05",  MACRO_LONG(8)},
    {0, "SLIME04",  "SLIME01",  MACRO_LONG(8)},
    {0, "SLIME08",  "SLIME05",  MACRO_LONG(8)},
    {0, "SLIME12",  "SLIME09",  MACRO_LONG(8)},
    {1, "BFALL4",   "BFALL1",   MACRO_LONG(8)},
    {1, "DBRAIN4",  "DBRAIN1",  MACRO_LONG(8)},
    {1, "SFALL4",   "SFALL1",   MACRO_LONG(8)},
    {-1, "\0",      "\0"}
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
            lumpnum_t           startFrame, endFrame, n;

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
            int             startFrame, endFrame, n;

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
    Con_Message("P_InitPicAnims: Registering default animations.\n");

    loadAnimatedDefs(animsShared);
    if(gameMode == commercial)
    {
        loadAnimatedDefs(animsDoom2);
    }
}

boolean P_ActivateLine(linedef_t *ld, mobj_t *mo, int side, int actType)
{
    switch(actType)
    {
    case SPAC_CROSS:
        crossSpecialLine(ld, side, mo);
        return true;

    case SPAC_USE:
        return P_UseSpecialLine(mo, ld, side);

    case SPAC_IMPACT:
        shootSpecialLine(mo, ld);
        return true;

    default:
        Con_Error("P_ActivateLine: Unknown Activation Type %i", actType);
        break;
    }

    return false;
}

/**
 * Called every time a thing origin is about to cross a line with a non 0
 * special.
 */
static void crossSpecialLine(linedef_t *line, int side, mobj_t *thing)
{
    int                 ok;
    xline_t            *xline;

    // Extended functionality overrides old.
    if(XL_CrossLine(line, side, thing))
        return;

    xline = P_ToXLine(line);

    // Triggers that other things can activate.
    if(!thing->player)
    {
        // Things that should NOT trigger specials...
        switch(thing->type)
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
        switch(xline->special)
        {
        case 39:                // TELEPORT TRIGGER
        case 97:                // TELEPORT RETRIGGER
        case 125:               // TELEPORT MONSTERONLY TRIGGER
        case 126:               // TELEPORT MONSTERONLY RETRIGGER
        case 4:                 // RAISE DOOR
        case 10:                // PLAT DOWN-WAIT-UP-STAY TRIGGER
        case 88:                // PLAT DOWN-WAIT-UP-STAY RETRIGGER
            ok = 1;
            break;
        }

        // Anything can trigger this line!
        if(xline->flags & ML_ALLTRIGGER)
            ok = 1;

        if(!ok)
            return;
    }

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
        // Light Turn On - max.
        EV_LightTurnOn(line, 1);
        xline->special = 0;
        break;

    case 16:
        // Close Door 30.
        EV_DoDoor(line, DT_CLOSE30THENOPEN);
        xline->special = 0;
        break;

    case 17:
        // Start Light Strobing.
        EV_StartLightStrobing(line);
        xline->special = 0;
        break;

    case 19:
        // Lower Floor.
        EV_DoFloor(line, FT_LOWER);
        xline->special = 0;
        break;

    case 22:
        // Raise floor to nearest height and change texture.
        EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0);
        xline->special = 0;
        break;

    case 25:
        // Ceiling Crush and Raise.
        EV_DoCeiling(line, CT_CRUSHANDRAISE);
        xline->special = 0;
        break;

    case 30:
        // Raise floor to shortest texture height on either side of lines.
        EV_DoFloor(line, FT_RAISETOTEXTURE);
        xline->special = 0;
        break;

    case 35:
        // Lights Very Dark.
        EV_LightTurnOn(line, 35.0f/255);
        xline->special = 0;
        break;

    case 36:
        // Lower Floor (TURBO).
        EV_DoFloor(line, FT_LOWERTURBO);
        xline->special = 0;
        break;

    case 37:
        // LowerAndChange.
        EV_DoFloor(line, FT_LOWERANDCHANGE);
        xline->special = 0;
        break;

    case 38:
        // Lower Floor To Lowest.
        EV_DoFloor(line, FT_LOWERTOLOWEST);
        xline->special = 0;
        break;

    case 39:
        // TELEPORT!
        EV_Teleport(line, side, thing, true);
        xline->special = 0;
        break;

    case 40:
        // RaiseCeilingLowerFloor.
        EV_DoCeiling(line, CT_RAISETOHIGHEST);
        EV_DoFloor(line, FT_LOWERTOLOWEST);
        xline->special = 0;
        break;

    case 44:
        // Ceiling Crush.
        EV_DoCeiling(line, CT_LOWERANDCRUSH);
        xline->special = 0;
        break;

    case 52:
        // EXIT!
        G_LeaveMap(G_GetMapNumber(gameEpisode, gameMap), 0, false);
        break;

    case 53:
        // Perpetual Platform Raise.
        EV_DoPlat(line, PT_PERPETUALRAISE, 0);
        xline->special = 0;
        break;

    case 54:
        // Platform Stop.
        P_PlatDeactivate(xline->tag);
        xline->special = 0;
        break;

    case 56:
        // Raise Floor Crush.
        EV_DoFloor(line, FT_RAISEFLOORCRUSH);
        xline->special = 0;
        break;

    case 57:
        // Ceiling Crush Stop.
        P_CeilingDeactivate(xline->tag);
        xline->special = 0;
        break;

    case 58:
        // Raise Floor 24.
        EV_DoFloor(line, FT_RAISE24);
        xline->special = 0;
        break;

    case 59:
        // Raise Floor 24 And Change.
        EV_DoFloor(line, FT_RAISE24ANDCHANGE);
        xline->special = 0;
        break;

    case 104:
        // Turn lights off in sector(tag).
        EV_TurnTagLightsOff(line);
        xline->special = 0;
        break;

    case 108:
        // Blazing Door Raise (faster than TURBO!).
        EV_DoDoor(line, DT_BLAZERAISE);
        xline->special = 0;
        break;

    case 109:
        // Blazing Door Open (faster than TURBO!).
        EV_DoDoor(line, DT_BLAZEOPEN);
        xline->special = 0;
        break;

    case 100:
        // Build Stairs Turbo 16.
        EV_BuildStairs(line, turbo16);
        xline->special = 0;
        break;

    case 110:
        // Blazing Door Close (faster than TURBO!).
        EV_DoDoor(line, DT_BLAZECLOSE);
        xline->special = 0;
        break;

    case 119:
        // Raise floor to nearest surr. floor.
        EV_DoFloor(line, FT_RAISEFLOORTONEAREST);
        xline->special = 0;
        break;

    case 121:
        // Blazing PlatDownWaitUpStay.
        EV_DoPlat(line, PT_DOWNWAITUPSTAYBLAZE, 0);
        xline->special = 0;
        break;

    case 124:
        // Secret EXIT.
        G_LeaveMap(G_GetMapNumber(gameEpisode, gameMap), 0, true);
        break;

    case 125:
        // TELEPORT MonsterONLY.
        if(!thing->player)
        {
            EV_Teleport(line, side, thing, true);
            xline->special = 0;
        }
        break;

    case 130:
        // Raise Floor Turbo.
        EV_DoFloor(line, FT_RAISEFLOORTURBO);
        xline->special = 0;
        break;

    case 141:
        // Silent Ceiling Crush & Raise.
        EV_DoCeiling(line, CT_SILENTCRUSHANDRAISE);
        xline->special = 0;
        break;

        // RETRIGGERS.  All from here till end.
    case 72:
        // Ceiling Crush.
        EV_DoCeiling(line, CT_LOWERANDCRUSH);
        break;

    case 73:
        // Ceiling Crush and Raise.
        EV_DoCeiling(line, CT_CRUSHANDRAISE);
        break;

    case 74:
        // Ceiling Crush Stop.
        P_CeilingDeactivate(xline->tag);
        break;

    case 75:
        // Close Door.
        EV_DoDoor(line, DT_CLOSE);
        break;

    case 76:
        // Close Door 30.
        EV_DoDoor(line, DT_CLOSE30THENOPEN);
        break;

    case 77:
        // Fast Ceiling Crush & Raise.
        EV_DoCeiling(line, CT_CRUSHANDRAISEFAST);
        break;

    case 79:
        // Lights Very Dark.
        EV_LightTurnOn(line, 35.0f/255.0f);
        break;

    case 80:
        // Light Turn On - brightest near.
        EV_LightTurnOn(line, 0);
        break;

    case 81:
        // Light Turn On 255.
        EV_LightTurnOn(line, 1);
        break;

    case 82:
        // Lower Floor To Lowest.
        EV_DoFloor(line, FT_LOWERTOLOWEST);
        break;

    case 83:
        // Lower Floor.
        EV_DoFloor(line, FT_LOWER);
        break;

    case 84:
        // LowerAndChange.
        EV_DoFloor(line, FT_LOWERANDCHANGE);
        break;

    case 86:
        // Open Door.
        EV_DoDoor(line, DT_OPEN);
        break;

    case 87:
        // Perpetual Platform Raise.
        EV_DoPlat(line, PT_PERPETUALRAISE, 0);
        break;

    case 88:
        // PlatDownWaitUp.
        EV_DoPlat(line, PT_DOWNWAITUPSTAY, 0);
        break;

    case 89:
        // Platform Stop.
        P_PlatDeactivate(xline->tag);
        break;

    case 90:
        // Raise Door.
        EV_DoDoor(line, DT_NORMAL);
        break;

    case 91:
        // Raise Floor.
        EV_DoFloor(line, FT_RAISEFLOOR);
        break;

    case 92:
        // Raise Floor 24.
        EV_DoFloor(line, FT_RAISE24);
        break;

    case 93:
        // Raise Floor 24 And Change.
        EV_DoFloor(line, FT_RAISE24ANDCHANGE);
        break;

    case 94:
        // Raise Floor Crush.
        EV_DoFloor(line, FT_RAISEFLOORCRUSH);
        break;

    case 95:
        // Raise floor to nearest height and change texture.
        EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0);
        break;

    case 96:
        // Raise floor to shortest texture height on either side of lines.
        EV_DoFloor(line, FT_RAISETOTEXTURE);
        break;

    case 97:
        // TELEPORT!
        EV_Teleport(line, side, thing, true);
        break;

    case 98:
        // Lower Floor (TURBO).
        EV_DoFloor(line, FT_LOWERTURBO);
        break;

    case 105:
        // Blazing Door Raise (faster than TURBO!).
        EV_DoDoor(line, DT_BLAZERAISE);
        break;

    case 106:
        // Blazing Door Open (faster than TURBO!).
        EV_DoDoor(line, DT_BLAZEOPEN);
        break;

    case 107:
        // Blazing Door Close (faster than TURBO!).
        EV_DoDoor(line, DT_BLAZECLOSE);
        break;

    case 120:
        // Blazing PlatDownWaitUpStay.
        EV_DoPlat(line, PT_DOWNWAITUPSTAYBLAZE, 0);
        break;

    case 126:
        // TELEPORT MonsterONLY.
        if(!thing->player)
            EV_Teleport(line, side, thing, true);
        break;

    case 128:
        // Raise To Nearest Floor.
        EV_DoFloor(line, FT_RAISEFLOORTONEAREST);
        break;

    case 129:
        // Raise Floor Turbo.
        EV_DoFloor(line, FT_RAISEFLOORTURBO);
        break;
    }
}

/**
 * Called when a thing shoots a special line.
 */
static void shootSpecialLine(mobj_t *thing, linedef_t *line)
{
    // Impacts that other things can activate.
    if(!thing->player)
    {
        switch(P_ToXLine(line)->special)
        {
        case 46: // OPEN DOOR IMPACT
            break;

        default:
            return; // Cannot be shot at.
        }
    }

    switch(P_ToXLine(line)->special)
    {
    case 24:
        // RAISE FLOOR
        EV_DoFloor(line, FT_RAISEFLOOR);
        P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
        P_ToXLine(line)->special = 0;
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
        P_ToXLine(line)->special = 0;
        break;

    default:
        break;
    }
}

/**
 * Called every tic frame that the player origin is in a special sector
 */
void P_PlayerInSpecialSector(player_t* player)
{
    sector_t* sector = DMU_GetPtrp(player->plr->mo->subsector, DMU_SECTOR);

    // Falling, not all the way down yet?
    if(player->plr->mo->pos[VZ] != DMU_GetFloatp(sector, DMU_FLOOR_HEIGHT))
        return;

    // Has hitten ground.
    switch(P_ToXSector(sector)->special)
    {
    case 5:
        // HELLSLIME DAMAGE.
        if(!player->powers[PT_IRONFEET])
        {
            if(!(mapTime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 10, false);
        }
        break;

    case 7:
        // NUKAGE DAMAGE.
        if(!player->powers[PT_IRONFEET])
        {
            if(!(mapTime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 5, false);
        }
        break;

    case 16:
        // SUPER HELLSLIME DAMAGE
    case 4:
        // STROBE HURT
        if(!player->powers[PT_IRONFEET] || (P_Random() < 5))
        {
            if(!(mapTime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 20, false);
        }
        break;

    case 9:
        // SECRET SECTOR
        player->secretCount++;
        P_ToXSector(sector)->special = 0;
        if(cfg.secretMsg)
        {
            P_SetMessage(player, "You've found a secret area!", false);
            S_ConsoleSound(SFX_GETPOW, 0, player - players);
        }
        break;

    case 11:
        // EXIT SUPER DAMAGE! (for E1M8 finale)
        player->cheats &= ~CF_GODMODE;

        if(!(mapTime & 0x1f))
            P_DamageMobj(player->plr->mo, NULL, NULL, 20, false);

        if(player->health <= 10)
            G_LeaveMap(G_GetMapNumber(gameEpisode, gameMap), 0, false);
        break;

    default:
        break;
    }
}

/**
 * Animate planes, scroll walls, etc.
 */
void P_UpdateSpecials(void)
{
    linedef_t*          line;
    sidedef_t*          side;

    // Extended lines and sectors.
    XG_Ticker();

    // Animate line specials.
    if(P_IterListSize(linespecials))
    {
        float               x, offset;

        P_IterListResetIterator(linespecials, false);
        while((line = P_IterListIterator(linespecials)) != NULL)
        {
            xline_t            *xline = P_ToXLine(line);

            switch(xline->special)
            {
            case 48:
            case 85:
                side = DMU_GetPtrp(line, DMU_SIDEDEF0);
                if(xline->special == 85)
                    offset = -1;
                else
                    offset = 1;

                x = DMU_GetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X);
                DMU_SetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X, x += offset);
                x = DMU_GetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X);
                DMU_SetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X, x += offset);
                x = DMU_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X);
                DMU_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X, x += offset);
                break;

            default:
                break;
            }
        }
    }
}

/**
 * After the map has been loaded, scan for specials that spawn thinkers.
 */
void P_SpawnSpecials(void)
{
    uint                i;
    linedef_t*          line;
    xline_t*            xline;
    iterlist_t*         list;
    sector_t*           sec;
    xsector_t*          xsec;

    // Init special sectors.
    P_DestroySectorTagLists();

    for(i = 0; i < numsectors; ++i)
    {
        sec = DMU_ToPtr(DMU_SECTOR, i);
        xsec = P_ToXSector(sec);

        if(xsec->tag)
        {
           list = P_GetSectorIterListForTag(xsec->tag, true);
           P_AddObjectToIterList(list, sec);
        }

        if(!xsec->special)
            continue;

        if(IS_CLIENT)
        {
            switch(xsec->special)
            {
            case 9: // A secret sector.
                totalSecret++;
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
            totalSecret++;
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

        case 17:
            P_SpawnFireFlicker(sec);
            break;

        default:
            break;
        }
    }

    // Init animating line specials.
    P_EmptyIterList(linespecials);
    P_DestroyLineTagLists();
    for(i = 0; i < numlines; ++i)
    {
        line = DMU_ToPtr(DMU_LINEDEF, i);
        xline = P_ToXLine(line);

        switch(xline->special)
        {
        case 48: // EFFECT FIRSTCOL SCROLL+
            P_AddObjectToIterList(linespecials, line);
            break;

        default:
            break;
        }

        if(xline->tag)
        {
           list = P_GetLineIterListForTag(xline->tag, true);
           P_AddObjectToIterList(list, line);
        }
    }

    // Init extended generalized lines and sectors.
    XG_Init();
}

boolean P_UseSpecialLine2(mobj_t* mo, linedef_t* line, int side)
{
    xline_t*            xline = P_ToXLine(line);

    // Use the back sides of VERY SPECIAL lines...
    if(side)
    {
        switch(xline->special)
        {
        case 124:
            // Sliding door open&close
            // UNUSED?
            break;

        default:
            return false;
            break;
        }
    }

    // Switches that other things can activate.
    if(!mo->player)
    {
        // Never open secret doors.
        if(xline->flags & ML_SECRET)
            return false;

        switch(xline->special)
        {
        case 1:                 // MANUAL DOOR RAISE
        case 32:                // MANUAL BLUE
        case 33:                // MANUAL RED
        case 34:                // MANUAL YELLOW
            break;

        default:
            return false;
            break;
        }
    }

    // Do something.
    switch(xline->special)
    {
        // MANUALS
    case 1:                     // Vertical Door
    case 26:                    // Blue Door/Locked
    case 27:                    // Yellow Door /Locked
    case 28:                    // Red Door /Locked

    case 31:                    // Manual door open
    case 32:                    // Blue locked door open
    case 33:                    // Red locked door open
    case 34:                    // Yellow locked door open

    case 117:                   // Blazing door raise
    case 118:                   // Blazing door open
        EV_VerticalDoor(line, mo);
        break;

        //UNUSED - Door Slide Open&Close
        // case 124:
        // EV_SlidingDoor (line, mo);
        // break;

        // SWITCHES
    case 7:
        // Build Stairs
        if(EV_BuildStairs(line, build8))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 9:
        // Change Donut
        if(EV_DoDonut(line))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 11:
        // Exit map
        if(cyclingMaps && mapCycleNoExit)
            break;

        // Prevent 'zombie players' from exiting maps.
        if(mo->player && mo->player->health <= 0 && !cfg.zombiesCanExit)
        {
            S_StartSound(SFX_NOWAY, mo);
            return false;
        }

        P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_SWTCHX, false, 0);
        xline->special = 0;
        G_LeaveMap(G_GetMapNumber(gameEpisode, gameMap), 0, false);
        break;

    case 14:
        // Raise Floor 32 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 32))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 15:
        // Raise Floor 24 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 24))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 18:
        // Raise Floor to next highest floor.
        if(EV_DoFloor(line, FT_RAISEFLOORTONEAREST))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 20:
        // Raise Plat next highest floor and change texture.
        if(EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 21:
        // PlatDownWaitUpStay.
        if(EV_DoPlat(line, PT_DOWNWAITUPSTAY, 0))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 23:
        // Lower Floor to Lowest.
        if(EV_DoFloor(line, FT_LOWERTOLOWEST))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 29:
        // Raise Door.
        if(EV_DoDoor(line, DT_NORMAL))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 41:
        // Lower Ceiling to Floor.
        if(EV_DoCeiling(line, CT_LOWERTOFLOOR))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 71:
        // Turbo Lower Floor.
        if(EV_DoFloor(line, FT_LOWERTURBO))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 49:
        // Ceiling Crush And Raise.
        if(EV_DoCeiling(line, CT_CRUSHANDRAISE))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 50:
        // Close Door.
        if(EV_DoDoor(line, DT_CLOSE))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 51:
        // Secret EXIT.
        if(cyclingMaps && mapCycleNoExit)
            break;

        // Prevent 'zombie players' from exiting maps.
        if(mo->player && mo->player->health <= 0 && !cfg.zombiesCanExit)
        {
            S_StartSound(SFX_NOWAY, mo);
            return false;
        }

        P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
        xline->special = 0;
        G_LeaveMap(G_GetMapNumber(gameEpisode, gameMap), 0, true);
        break;

    case 55:
        // Raise Floor Crush.
        if(EV_DoFloor(line, FT_RAISEFLOORCRUSH))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 101:
        // Raise Floor.
        if(EV_DoFloor(line, FT_RAISEFLOOR))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 102:
        // Lower Floor to Surrounding floor height.
        if(EV_DoFloor(line, FT_LOWER))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 103:
        // Open Door.
        if(EV_DoDoor(line, DT_OPEN))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 111:
        // Blazing Door Raise (faster than TURBO!).
        if(EV_DoDoor(line, DT_BLAZERAISE))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 112:
        // Blazing Door Open (faster than TURBO!).
        if(EV_DoDoor(line, DT_BLAZEOPEN))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 113:
        // Blazing Door Close (faster than TURBO!).
        if(EV_DoDoor(line, DT_BLAZECLOSE))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 122:
        // Blazing PlatDownWaitUpStay.
        if(EV_DoPlat(line, PT_DOWNWAITUPSTAYBLAZE, 0))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 127:
        // Build Stairs Turbo 16.
        if(EV_BuildStairs(line, turbo16))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 131:
        // Raise Floor Turbo.
        if(EV_DoFloor(line, FT_RAISEFLOORTURBO))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 133:
        // BlzOpenDoor BLUE.
    case 135:
        // BlzOpenDoor RED.
    case 137:
        // BlzOpenDoor YELLOW.
        if(EV_DoLockedDoor(line, DT_BLAZEOPEN, mo))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 140:
        // Raise Floor 512.
        if(EV_DoFloor(line, FT_RAISE512))
        {
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

        // BUTTONS
    case 42:
        // Close Door.
        if(EV_DoDoor(line, DT_CLOSE))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 43:
        // Lower Ceiling to Floor.
        if(EV_DoCeiling(line, CT_LOWERTOFLOOR))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 45:
        // Lower Floor to Surrounding floor height.
        if(EV_DoFloor(line, FT_LOWER))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 60:
        // Lower Floor to Lowest.
        if(EV_DoFloor(line, FT_LOWERTOLOWEST))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 61:
        // Open Door.
        if(EV_DoDoor(line, DT_OPEN))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 62:
        // PlatDownWaitUpStay.
        if(EV_DoPlat(line, PT_DOWNWAITUPSTAY, 1))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 63:
        // Raise Door.
        if(EV_DoDoor(line, DT_NORMAL))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 64:
        // Raise Floor to ceiling.
        if(EV_DoFloor(line, FT_RAISEFLOOR))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 66:
        // Raise Floor 24 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 24))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 67:
        // Raise Floor 32 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 32))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 65:
        // Raise Floor Crush.
        if(EV_DoFloor(line, FT_RAISEFLOORCRUSH))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 68:
        // Raise Plat to next highest floor and change texture.
        if(EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 69:
        // Raise Floor to next highest floor.
        if(EV_DoFloor(line, FT_RAISEFLOORTONEAREST))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 70:
        // Turbo Lower Floor.
        if(EV_DoFloor(line, FT_LOWERTURBO))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 114:
        // Blazing Door Raise (faster than TURBO!).
        if(EV_DoDoor(line, DT_BLAZERAISE))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 115:
        // Blazing Door Open (faster than TURBO!).
        if(EV_DoDoor(line, DT_BLAZEOPEN))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 116:
        // Blazing Door Close (faster than TURBO!).
        if(EV_DoDoor(line, DT_BLAZECLOSE))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 123:
        // Blazing PlatDownWaitUpStay.
        if(EV_DoPlat(line, PT_DOWNWAITUPSTAYBLAZE, 0))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 132:
        // Raise Floor Turbo.
        if(EV_DoFloor(line, FT_RAISEFLOORTURBO))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 99:
        // BlzOpenDoor BLUE.
    case 134:
        // BlzOpenDoor RED.
    case 136:
        // BlzOpenDoor YELLOW.
        if(EV_DoLockedDoor(line, DT_BLAZEOPEN, mo))
            P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 138:
        // Light Turn On.
        EV_LightTurnOn(line, 1);
        P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 139:
        // Light Turn Off.
        EV_LightTurnOn(line, 35.0f/255.0f);
        P_ToggleSwitch(DMU_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    default:
        break;
    }

    return true;
}
