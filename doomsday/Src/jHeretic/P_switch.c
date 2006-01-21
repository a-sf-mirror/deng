/*
 * Switches, buttons. Two-state animation. Exits.
 */

// HEADER FILES ------------------------------------------------------------

#include "jHeretic/Doomdef.h"
#include "jHeretic/P_local.h"
#include "jHeretic/Soundst.h"
#include "d_net.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

switchlist_t alphSwitchList[] = {
    {"SW1OFF", "SW1ON", 1}
    ,
    {"SW2OFF", "SW2ON", 1}
    ,
    {"\0", "\0", 0}
};

button_t buttonlist[MAXBUTTONS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int *switchlist;
static int max_numswitches;
static int numswitches;

// CODE --------------------------------------------------------------------

/*
 * Only called at game initialization.
 */
void P_InitSwitchList(void)
{
    int     i;
    int     index;
    int     episode;

    episode = 1;
    if(!shareware)
        episode = 2;

    for(index = 0, i = 0;; i++)
    {
        if(index+1 >= max_numswitches)
        {
            switchlist = realloc(switchlist, sizeof *switchlist *
            (max_numswitches = max_numswitches ? max_numswitches*2 : 8));
        }

        if(!alphSwitchList[i].episode)
        {
            numswitches = index / 2;
            switchlist[index] = -1;
            break;
        }

        if(alphSwitchList[i].episode <= episode)
        {
            switchlist[index++] = R_TextureNumForName(alphSwitchList[i].name1);
            switchlist[index++] = R_TextureNumForName(alphSwitchList[i].name2);
        }
    }
}

/*
 * Start a button counting down till it turns off.
 */
void P_StartButton(line_t *line, bwhere_e w, int texture, int time)
{
    int     i;

    // See if button is already pressed
    for(i = 0; i < MAXBUTTONS; i++)
    {
        if(buttonlist[i].btimer && buttonlist[i].line == line)
        {

            return;
        }
    }

    for(i = 0; i < MAXBUTTONS; i++)
    {
        if(!buttonlist[i].btimer)
        {
            buttonlist[i].line = line;
            buttonlist[i].where = w;
            buttonlist[i].btexture = texture;
            buttonlist[i].btimer = time;
            buttonlist[i].soundorg = P_GetPtrp(DMU_SECTOR,
                P_GetPtrp(line, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
            return;
        }
    }

    Con_Error("P_StartButton: no button slots left!");
}

/*
 * Function that changes wall texture.
 * Tell it if switch is ok to use again (1=yes, it's a button).
 */
void P_ChangeSwitchTexture(line_t *line, int useAgain)
{
    int     texTop;
    int     texMid;
    int     texBot;
    int     i;
    int     sound;
    side_t*     sdef = P_GetPtrp(line, DMU_SIDE0);
    sector_t*   frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);

    if(!useAgain)
        P_XLine(line)->special = 0;

    texTop = P_GetIntp(sdef, DMU_TOP_TEXTURE);
    texMid = P_GetIntp(sdef, DMU_MIDDLE_TEXTURE);
    texBot = P_GetIntp(sdef, DMU_BOTTOM_TEXTURE);

    sound = sfx_switch;

    for(i = 0; i < numswitches * 2; i++)
    {
        if(switchlist[i] == texTop)
        {
            S_StartSound(sound, P_GetPtrp(frontsector,
                                          DMU_SOUND_ORIGIN));

            P_SetIntp(sdef, DMU_TOP_TEXTURE, switchlist[i ^ 1]);

            if(useAgain)
                P_StartButton(line, top, switchlist[i], BUTTONTIME);

            return;
        }
        else
        {
            if(switchlist[i] == texMid)
            {
                S_StartSound(sound, P_GetPtrp(frontsector,
                                              DMU_SOUND_ORIGIN));

                P_SetIntp(sdef, DMU_MIDDLE_TEXTURE, switchlist[i ^ 1]);

                if(useAgain)
                    P_StartButton(line, middle, switchlist[i], BUTTONTIME);

                return;
            }
            else
            {
                if(switchlist[i] == texBot)
                {
                    S_StartSound(sound, P_GetPtrp(frontsector,
                                                  DMU_SOUND_ORIGIN));

                    P_SetIntp(sdef, DMU_BOTTOM_TEXTURE, switchlist[i ^ 1]);

                    if(useAgain)
                        P_StartButton(line, bottom, switchlist[i], BUTTONTIME);

                    return;
                }
            }
        }
    }
}

/*
 * Called when a thing uses a special line.
 * Only the front sides of lines are usable.
 */
boolean P_UseSpecialLine(mobj_t *thing, line_t *line)
{
    xline_t *xline = P_XLine(line);

    // Extended functionality overrides old.
    if(XL_UseLine(line, 0, thing))
        return true;

    // Switches that other things can activate.
    if(!thing->player)
    {
        // never open secret doors
        if(P_GetIntp(line, DMU_FLAGS) & ML_SECRET)
            return false;

        switch (xline->special)
        {
        case 1:             // MANUAL DOOR RAISE
        case 32:                // MANUAL BLUE
        case 33:                // MANUAL RED
        case 34:                // MANUAL YELLOW
            break;
        default:
            return false;
        }
    }

    // do something
    switch (xline->special)
    {
        // MANUALS
    case 1:                 // Vertical Door
    case 26:                    // Blue Door/Locked
    case 27:                    // Yellow Door /Locked
    case 28:                    // Red Door /Locked

    case 31:                    // Manual door open
    case 32:                    // Blue locked door open
    case 33:                    // Red locked door open
    case 34:                    // Yellow locked door open
        EV_VerticalDoor(line, thing);
        break;

        // SWITCHES
    case 7:                 // Switch_Build_Stairs (8 pixel steps)
        if(EV_BuildStairs(line, 8 * FRACUNIT))
        {
            P_ChangeSwitchTexture(line, 0);
        }
        break;
    case 107:                   // Switch_Build_Stairs_16 (16 pixel steps)
        if(EV_BuildStairs(line, 16 * FRACUNIT))
        {
            P_ChangeSwitchTexture(line, 0);
        }
        break;
    case 9:                 // Change Donut
        if(EV_DoDonut(line))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 11:                    // Exit level
        if(cyclingMaps && mapCycleNoExit)
            break;
        G_ExitLevel();
        P_ChangeSwitchTexture(line, 0);
        break;
    case 14:                    // Raise Floor 32 and change texture
        if(EV_DoPlat(line, raiseAndChange, 32))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 15:                    // Raise Floor 24 and change texture
        if(EV_DoPlat(line, raiseAndChange, 24))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 18:                    // Raise Floor to next highest floor
        if(EV_DoFloor(line, raiseFloorToNearest))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 20:                    // Raise Plat next highest floor and change texture
        if(EV_DoPlat(line, raiseToNearestAndChange, 0))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 21:                    // PlatDownWaitUpStay
        if(EV_DoPlat(line, downWaitUpStay, 0))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 23:                    // Lower Floor to Lowest
        if(EV_DoFloor(line, lowerFloorToLowest))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 29:                    // Raise Door
        if(EV_DoDoor(line, normal))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 41:                    // Lower Ceiling to Floor
        if(EV_DoCeiling(line, lowerToFloor))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 71:                    // Turbo Lower Floor
        if(EV_DoFloor(line, turboLower))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 49:                    // Lower Ceiling And Crush
        if(EV_DoCeiling(line, lowerAndCrush))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 50:                    // Close Door
        if(EV_DoDoor(line, close))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 51:                    // Secret EXIT
        if(cyclingMaps && mapCycleNoExit)
            break;
        G_SecretExitLevel();
        P_ChangeSwitchTexture(line, 0);
        break;
    case 55:                    // Raise Floor Crush
        if(EV_DoFloor(line, raiseFloorCrush))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 101:                   // Raise Floor
        if(EV_DoFloor(line, raiseFloor))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 102:                   // Lower Floor to Surrounding floor height
        if(EV_DoFloor(line, lowerFloor))
            P_ChangeSwitchTexture(line, 0);
        break;
    case 103:                   // Open Door
        if(EV_DoDoor(line, open))
            P_ChangeSwitchTexture(line, 0);
        break;

        // BUTTONS
    case 42:                    // Close Door
        if(EV_DoDoor(line, close))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 43:                    // Lower Ceiling to Floor
        if(EV_DoCeiling(line, lowerToFloor))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 45:                    // Lower Floor to Surrounding floor height
        if(EV_DoFloor(line, lowerFloor))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 60:                    // Lower Floor to Lowest
        if(EV_DoFloor(line, lowerFloorToLowest))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 61:                    // Open Door
        if(EV_DoDoor(line, open))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 62:                    // PlatDownWaitUpStay
        if(EV_DoPlat(line, downWaitUpStay, 1))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 63:                    // Raise Door
        if(EV_DoDoor(line, normal))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 64:                    // Raise Floor to ceiling
        if(EV_DoFloor(line, raiseFloor))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 66:                    // Raise Floor 24 and change texture
        if(EV_DoPlat(line, raiseAndChange, 24))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 67:                    // Raise Floor 32 and change texture
        if(EV_DoPlat(line, raiseAndChange, 32))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 65:                    // Raise Floor Crush
        if(EV_DoFloor(line, raiseFloorCrush))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 68:                    // Raise Plat to next highest floor and change texture
        if(EV_DoPlat(line, raiseToNearestAndChange, 0))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 69:                    // Raise Floor to next highest floor
        if(EV_DoFloor(line, raiseFloorToNearest))
            P_ChangeSwitchTexture(line, 1);
        break;
    case 70:                    // Turbo Lower Floor
        if(EV_DoFloor(line, turboLower))
            P_ChangeSwitchTexture(line, 1);
        break;
    }

    return true;
}
