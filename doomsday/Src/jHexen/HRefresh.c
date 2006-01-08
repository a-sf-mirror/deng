
//**************************************************************************
//**
//** HREFRESH.C
//**
//** Hexen-specific refresh stuff.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "h2def.h"
#include "jHexen/p_local.h"
#include "x_config.h"
#include "jHexen/mn_def.h"
#include "../Common/hu_stuff.h"
#include "jHexen/st_stuff.h"
#include "../Common/am_map.h"
#include "f_infine.h"

// MACROS ------------------------------------------------------------------

#define viewheight  Get(DD_VIEWWINDOW_HEIGHT)

// TYPES -------------------------------------------------------------------

// This could hold much more detailed information...
typedef struct {
    char    name[9];            // Name of the texture.
    int     type;               // Which type?
} textype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    X_Drawer(void);
void    R_SetAllDoomsdayFlags(void);
void    H2_AdvanceDemo(void);
void    MN_DrCenterTextA_CS(char *text, int center_x, int y);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean automapactive;
extern boolean amap_fullyopen;
extern float lookOffset;
extern int actual_leveltime;

extern float deffontRGB[];

extern boolean dontrender;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//int demosequence;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/*static int pagetic;
   static char *pagename; */

// CODE --------------------------------------------------------------------

/*
   ==============
   =
   = R_SetViewSize
   =
   = Don't really change anything here, because i might be in the middle of
   = a refresh.  The change will take effect next refresh.
   =
   ==============
 */

boolean setsizeneeded;

void R_SetViewSize(int blocks, int detail)
{
    setsizeneeded = true;
    cfg.setblocks = blocks;
    GL_Update(DDUF_BORDER);
}

void R_HandleSectorSpecials()
{
	int     i, scrollOffset = leveltime >> 1 & 63;
    int     sectorCount = DD_GetInteger(DD_SECTOR_COUNT);

	for(i = 0; i < sectorCount; i++)
    {
        xsector_t* sect = P_XSector(P_ToPtr(DMU_SECTOR, i));
		switch (sect->special)
		{						// Handle scrolling flats
		case 201:
		case 202:
		case 203:				// Scroll_North_xxx
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_Y, 
                       (63 - scrollOffset) << (sect->special - 201));
			break;
		case 204:
		case 205:
		case 206:				// Scroll_East_xxx
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_X, 
                       (63 - scrollOffset) << (sect->special - 204));
			break;
		case 207:
		case 208:
		case 209:				// Scroll_South_xxx
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_Y, 
                       scrollOffset << (sect->special - 207));
			break;
		case 210:
		case 211:
		case 212:				// Scroll_West_xxx
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_X, 
                       scrollOffset << (sect->special - 210));
			break;
		case 213:
		case 214:
		case 215:				// Scroll_NorthWest_xxx
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_X, 
                       scrollOffset << (sect->special - 213));
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_Y, 
                       (63 - scrollOffset) << (sect->special - 213));
			break;
		case 216:
		case 217:
		case 218:				// Scroll_NorthEast_xxx
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_X, 
                       (63 - scrollOffset) << (sect->special - 216));
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_Y, 
                       (63 - scrollOffset) << (sect->special - 216));
			break;
		case 219:
		case 220:
		case 221:				// Scroll_SouthEast_xxx
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_X, 
                       (63 - scrollOffset) << (sect->special - 219));
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_Y, 
                       scrollOffset << (sect->special - 219));
			break;
		case 222:
		case 223:
		case 224:				// Scroll_SouthWest_xxx
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_X, 
                       scrollOffset << (sect->special - 222));
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_Y, 
                       scrollOffset << (sect->special - 222));
			break;
		default:
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_X, 0);
			P_SetFloat(DMU_SECTOR, i, DMU_FLOOR_OFFSET_Y, 0);
			break;
		}
	}
}

//==========================================================================
// R_DrawMapTitle
//==========================================================================
void R_DrawMapTitle(void)
{
    float   alpha = 1;
    int     y = 12;
    char   *lname, *lauthor;

    if(!cfg.levelTitle || actual_leveltime > 6 * 35)
        return;

    // Make the text a bit smaller.
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
    gl.Translatef(160, y, 0);
    gl.Scalef(.75f, .75f, 1);   // Scale to 3/4
    gl.Translatef(-160, -y, 0);

    if(actual_leveltime < 35)
        alpha = actual_leveltime / 35.0f;
    if(actual_leveltime > 5 * 35)
        alpha = 1 - (actual_leveltime - 5 * 35) / 35.0f;

    lname = (char *) DD_GetVariable(DD_MAP_NAME);
    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);

    // Use stardard map name if DED didn't define it.
    if(!lname)
        lname = P_GetMapName(gamemap);

    Draw_BeginZoom((1 + cfg.hudScale)/2, 160, y);

    if(lname)
    {
        M_WriteText3(160 - M_StringWidth(lname, hu_font_b) / 2, y, lname,
                    hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], alpha, false, 0);
        y += 20;
    }

    if(lauthor)
    {
        M_WriteText3(160 - M_StringWidth(lauthor, hu_font_a) / 2, y, lauthor,
                    hu_font_a, .5f, .5f, .5f, alpha, false, 0);
    }

    Draw_EndZoom();

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();
}

//==========================================================================
//
// G_Drawer
//
//==========================================================================

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t wipegamestate = GS_DEMOSCREEN;
extern boolean inhelpscreens;
#define SIZEFACT 4
#define SIZEFACT2 16
void G_Drawer(void)
{
	static boolean viewactivestate = false;
	static boolean menuactivestate = false;
	static boolean inhelpscreensstate = false;
	static int targx =0, targy = 0, targw =0, targh = 0;
	static int /*x = 0, y = 0, */ w = 320, h = 200, offy = 0;
	static int fullscreenmode = 0;
	static gamestate_t oldgamestate = -1;
	int     py;
	player_t *vplayer = &players[displayplayer];
	boolean iscam = (vplayer->plr->flags & DDPF_CAMERA) != 0;	// $democam

	// $democam: can be set on every frame
	if(cfg.setblocks > 10 || iscam)
	{
		// Full screen.
		targx = 0;
		targy = 0;
		targw = 320;
		targh = 200;
	}
	else
	{
		targw = cfg.setblocks * 32;
		targh = cfg.setblocks * (200 - SBARHEIGHT * cfg.sbarscale / 20) / 10;
		targx = 160 - (targw >> 1);
		targy = (200 - SBARHEIGHT * cfg.sbarscale / 20 - targh) >> 1;
	}

	if(targw > w)
		w+= (((targw-w)>> 1)+SIZEFACT2)>>SIZEFACT;
	if(targw < w)
		w-= (((w-targw)>> 1)+SIZEFACT2)>>SIZEFACT;
	if(targh > h)
		h+= (((targh-h)>> 1)+SIZEFACT2)>>SIZEFACT;
	if(targh < h)
		h-= (((h-targh)>> 1)+SIZEFACT2)>>SIZEFACT;

	if(cfg.setblocks < 10)
	{
		offy = (SBARHEIGHT * cfg.sbarscale/20);
		R_ViewWindow(160-(w>>1), 100-((h+offy) >>1), w, h);
	} else
		R_ViewWindow(targx, targy, targw, targh);

	// Do buffered drawing
	switch (gamestate)
	{
	case GS_LEVEL:
		// Clients should be a little careful about the first frames.
		if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
			break;

		// Good luck trying to render the view without a viewpoint...
		if(!vplayer->plr->mo)
			break;

		if(leveltime < 2)
		{
			// Don't render too early; the first couple of frames 
			// might be a bit unstable -- this should be considered
			// a bug, but since there's an easy fix...
			break;
		}

		if(!automapactive || !amap_fullyopen || cfg.automapBack[3] < 1 || cfg.automapWidth < 1 || cfg.automapHeight < 1)
		{
			boolean special200 = false;

			R_HandleSectorSpecials();
			// Set flags for the renderer.
			if(IS_CLIENT)
			{
				// Server updates mobj flags in NetSv_Ticker.
				R_SetAllDoomsdayFlags();
			}
			GL_SetFilter(vplayer->plr->filter);	// $democam
			// Check for the sector special 200: use sky2.
			// I wonder where this is used?
			if(P_XSectorOfSubsector(vplayer->plr->mo->subsector)->special == 200)
			{
				special200 = true;
				Rend_SkyParams(0, DD_DISABLE, 0);
				Rend_SkyParams(1, DD_ENABLE, 0);
			}
			// How about a bit of quake?
			if(localQuakeHappening[displayplayer] && !paused)
			{
				int     intensity = localQuakeHappening[displayplayer];

				Set(DD_VIEWX_OFFSET,
					((M_Random() % (intensity << 2)) -
					 (intensity << 1)) << FRACBITS);
				Set(DD_VIEWY_OFFSET,
					((M_Random() % (intensity << 2)) -
					 (intensity << 1)) << FRACBITS);
			}
			else
			{
				Set(DD_VIEWX_OFFSET, 0);
				Set(DD_VIEWY_OFFSET, 0);
			}
			// The view angle offset.
			Set(DD_VIEWANGLE_OFFSET, ANGLE_MAX * -lookOffset);
			// Render the view.
			if(!dontrender)
			{
				R_RenderPlayerView(vplayer->plr);
			}
			if(special200)
			{
				Rend_SkyParams(0, DD_ENABLE, 0);
				Rend_SkyParams(1, DD_DISABLE, 0);
			}
			if(!iscam)
				X_Drawer();		// Draw the crosshair.

        }

        // Draw the automap?
        if(automapactive)
            AM_Drawer();

        // These various HUD's will be drawn unless Doomsday advises not to
        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        {

            // Level information is shown for a few seconds in the
            // beginning of a level.
            R_DrawMapTitle();

            GL_Update(DDUF_FULLSCREEN);

            // DJS - Do we need to render a full status bar at this point?
            if (!(automapactive && cfg.automapHudDisplay == 0 )){

                if(!iscam)
                    if(true == (viewheight == 200) )
                    {
                        // Fullscreen. Which mode?
                        ST_Drawer(cfg.setblocks - 10, true);    // $democam
                    } else {
                        ST_Drawer(0 , true);    // $democam
                    }

                fullscreenmode = viewheight == 200;

            }

            HU_Drawer();
        }

        // Need to update the borders?
        if(oldgamestate != GS_LEVEL ||
            (Get(DD_VIEWWINDOW_WIDTH) != 320 || menuactive ||
                cfg.sbarscale < 20 || (automapactive && cfg.automapHudDisplay == 0 )))
        {
            // Update the borders.
            GL_Update(DDUF_BORDER);
        }
        break;

    case GS_INTERMISSION:
        IN_Drawer();
        break;

    case GS_INFINE:
        GL_Update(DDUF_FULLSCREEN);
        break;

    case GS_WAITING:
        GL_DrawRawScreen(W_GetNumForName("TITLE"), 0, 0);
        gl.Color3f(1, 1, 1);
        MN_DrCenterTextA_CS("WAITING... PRESS ESC FOR MENU", 160, 188);
        GL_Update(DDUF_FULLSCREEN);
        break;

    default:
        break;
    }

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;

    if(paused && !fi_active)
    {
        if(automapactive)
            py = 4;
        else
            py = 4; // in jDOOM this is viewwindowy + 4

        GL_DrawPatch(160, py, W_GetNumForName("PAUSED"));
    }

    // InFine is drawn whenever active.
    FI_Drawer();
}

//==========================================================================
//
// PageDrawer
//
//==========================================================================

/*static void PageDrawer(void)
   {
   if(!pagename) return;
   GL_DrawRawScreen(W_GetNumForName(pagename), 0, 0);
   if(demosequence == 1)
   {
   GL_DrawPatch(4, 160, W_GetNumForName("ADVISOR"));
   }
   GL_Update(DDUF_FULLSCREEN);
   } */

#define FMAKERGBA(r,g,b,a) ( (byte)(0xff*r) + ((byte)(0xff*g)<<8) + ((byte)(0xff*b)<<16) + ((byte)(0xff*a)<<24) )

int H2_GetFilterColor(int filter)
{
    //int rgba = 0;

    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
        // Red?
        return FMAKERGBA(1, 0, 0, filter / 8.0);    // Full red with filter 8.
    else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
        // Light Yellow?
        return FMAKERGBA(1, 1, .5, (filter - STARTBONUSPALS + 1) / 16.0);
    else if(filter >= STARTPOISONPALS &&
            filter < STARTPOISONPALS + NUMPOISONPALS)
        // Green?
        return FMAKERGBA(0, 1, 0, (filter - STARTPOISONPALS + 1) / 16.0);
    else if(filter >= STARTSCOURGEPAL)
        // Orange?
        return FMAKERGBA(1, .5, 0, (STARTSCOURGEPAL + 3 - filter) / 6.0);
    else if(filter >= STARTHOLYPAL)
        // White?
        return FMAKERGBA(1, 1, 1, (STARTHOLYPAL + 3 - filter) / 6.0);
    else if(filter == STARTICEPAL)
        // Light blue?
        return FMAKERGBA(.5f, .5f, 1, .4f);
    else if(filter)
        Con_Error("H2_GetFilterColor: Strange filter number: %d.\n", filter);
    return 0;
}

void H2_SetFilter(int filter)
{
    GL_SetFilter(H2_GetFilterColor(filter));
}

void H2_EndFrame(void)
{
    SN_UpdateActiveSequences();
    /*  S_UpdateSounds(players[displayplayer].plr->mo); */
}

void H2_ConsoleBg(int *width, int *height)
{
    extern int consoleFlat;
    extern float consoleZoom;

    GL_SetFlat(consoleFlat + W_CheckNumForName("F_START") + 1);
    *width = 64 * consoleZoom;
    *height = 64 * consoleZoom;
}

/*
   //==========================================================================
   //
   // H2_PageTicker
   //
   //==========================================================================

   void H2_PageTicker(void)
   {
   if(--pagetic < 0)
   {
   H2_AdvanceDemo();
   }
   }

   //==========================================================================
   //
   // H2_DoAdvanceDemo
   //
   //==========================================================================

   void H2_DoAdvanceDemo(void)
   {
   players[consoleplayer].playerstate = PST_LIVE; // don't reborn
   advancedemo = false;
   usergame = false; // can't save/end game here
   paused = false;
   gameaction = ga_nothing;
   demosequence = (demosequence+1)%7;
   switch(demosequence)
   {
   case 0:
   pagetic = 280;
   gamestate = GS_DEMOSCREEN;
   pagename = "TITLE";
   S_StartMusic("hexen", true);
   break;
   case 1:
   pagetic = 210;
   gamestate = GS_DEMOSCREEN;
   pagename = "TITLE";
   break;
   case 2:
   GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
   G_DeferedPlayDemo("demo1");
   break;
   case 3:
   pagetic = 200;
   gamestate = GS_DEMOSCREEN;
   pagename = "CREDIT";
   break;
   case 4:
   GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
   G_DeferedPlayDemo("demo2");
   break;
   case 5:
   pagetic = 200;
   gamestate = GS_DEMOSCREEN;
   pagename = "CREDIT";
   break;
   case 6:
   GL_Update(DDUF_BORDER | DDUF_FULLSCREEN);
   G_DeferedPlayDemo("demo3");
   break;
   }
   }
 */

//==========================================================================
//
//
//
//==========================================================================
