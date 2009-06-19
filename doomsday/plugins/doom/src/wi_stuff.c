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
 * wi_stuff.c: Intermission/stat screens - jDoom specific.
 *
 * Different between registered DOOM (1994) and
 * Ultimate DOOM - Final edition (retail, 1995?).
 * This is supposedly ignored for commercial release (aka DOOM II),
 * which had 34 maps in one episode. So there.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <ctype.h>

#include "jdoom.h"

#include "hu_stuff.h"
#include "d_net.h"
#include "p_start.h"

// MACROS ------------------------------------------------------------------

#define NUM_TEAMS               (4) // Color = team.

#define NUMEPISODES             (4)
#define NUMMAPS                 (9)

// GLOBAL LOCATIONS
#define WI_TITLEY               (2)
#define WI_SPACINGY             (33)

// SINGPLE-PLAYER STUFF
#define SP_STATSX               (50)
#define SP_STATSY               (50)
#define SP_TIMEX                (16)
#define SP_TIMEY                (SCREENHEIGHT-32)

// NET GAME STUFF
#define NG_STATSY               (50)
#define NG_STATSX               (32 + star.width/2 + 32*!doFrags)
#define NG_SPACINGX             (64)

// DEATHMATCH STUFF
#define DM_MATRIXX              (42)
#define DM_MATRIXY              (68)
#define DM_SPACINGX             (40)
#define DM_TOTALSX              (269)
#define DM_KILLERSX             (10)
#define DM_KILLERSY             (100)
#define DM_VICTIMSX             (5)
#define DM_VICTIMSY             (50)

// States for single-player
#define SP_KILLS                (0)
#define SP_ITEMS                (2)
#define SP_SECRET               (4)
#define SP_FRAGS                (6)
#define SP_TIME                 (8)
#define SP_PAR                  (ST_TIME)
#define SP_PAUSE                (1)

#define SHOWNEXTLOCDELAY        (4) // In seconds.

// TYPES -------------------------------------------------------------------

typedef enum animenum_e {
    ANIM_ALWAYS,
    ANIM_RANDOM,
    ANIM_MAP
} animenum_t;

typedef struct point_s {
    int             x, y;
} point_t;

typedef struct wianim_s {
    animenum_t      type;

    // Period in tics between animations.
    int             period;

    // Number of animation frames.
    int             numAnimFrames;

    // Location of animation.
    point_t         loc;

    int             data1, data2;

    // Actual graphics for frames of animations.
    dpatch_t        p[3];

// Following must be initialized to zero before use!

    // Next value of bcnt (used in conjunction with period).
    int             nextTic;

    // Last drawn animation frame.
    int             lastDrawn;

    // Next frame number to animate.
    int             ctr;

    // Used by RANDOM and LEVEL when animating.
    int             state;
} wianim_t;

typedef struct teaminfo_s {
    int             members; // 0 if team not present.
    int             frags[NUM_TEAMS];
    int             totalFrags; // Kills minus suicides.
    int             items;
    int             kills;
    int             secret;
} teaminfo_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static teaminfo_t teamInfo[NUM_TEAMS];

static point_t mapPoints[NUMEPISODES][NUMMAPS] = {
    { // Episode 0
     {185, 164},
     {148, 143},
     {69, 122},
     {209, 102},
     {116, 89},
     {166, 55},
     {71, 56},
     {135, 29},
     {71, 24}
    },
    { // Episode 1
     {254, 25},
     {97, 50},
     {188, 64},
     {128, 78},
     {214, 92},
     {133, 130},
     {208, 136},
     {148, 140},
     {235, 158}
    },
    { // Episode 2
     {156, 168},
     {48, 154},
     {174, 95},
     {265, 75},
     {130, 48},
     {279, 23},
     {198, 48},
     {140, 25},
     {281, 136}
    }
};

// Animation locations.
static wianim_t epsd0animinfo[] = {
    {ANIM_ALWAYS, TICRATE / 3, 3, {224, 104}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {184, 160}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {112, 136}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {72, 112}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {88, 96}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {64, 48}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {192, 40}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {136, 16}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {80, 16}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {64, 24}}
};

static wianim_t epsd1animinfo[] = {
    {ANIM_MAP, TICRATE / 3, 1, {128, 136}, 1},
    {ANIM_MAP, TICRATE / 3, 1, {128, 136}, 2},
    {ANIM_MAP, TICRATE / 3, 1, {128, 136}, 3},
    {ANIM_MAP, TICRATE / 3, 1, {128, 136}, 4},
    {ANIM_MAP, TICRATE / 3, 1, {128, 136}, 5},
    {ANIM_MAP, TICRATE / 3, 1, {128, 136}, 6},
    {ANIM_MAP, TICRATE / 3, 1, {128, 136}, 7},
    {ANIM_MAP, TICRATE / 3, 3, {192, 144}, 8},
    {ANIM_MAP, TICRATE / 3, 1, {128, 136}, 8}
};

static wianim_t epsd2animinfo[] = {
    {ANIM_ALWAYS, TICRATE / 3, 3, {104, 168}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {40, 136}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {160, 96}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {104, 80}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {120, 32}},
    {ANIM_ALWAYS, TICRATE / 4, 3, {40, 0}}
};

static int NUMANIMS[NUMEPISODES] = {
    sizeof(epsd0animinfo) / sizeof(wianim_t),
    sizeof(epsd1animinfo) / sizeof(wianim_t),
    sizeof(epsd2animinfo) / sizeof(wianim_t)
};

static wianim_t *anims[NUMEPISODES] = {
    epsd0animinfo,
    epsd1animinfo,
    epsd2animinfo
};

// Signals to refresh everything for one frame.
static int firstRefresh;

// Used to accelerate or skip a stage.
static int accelerateStage;

static boolean snlPointerOn = false;

static int spState, dmState, ngState;

// specifies current state
static interludestate_t state;

static int dmFrags[NUM_TEAMS][NUM_TEAMS];
static int dmTotals[NUM_TEAMS];

static int doFrags;

// wbs->pnum
static int me;
static int myTeam;

// Used for general timing.
static int cnt;

// Used for timing of background animation.
static int bcnt;

static int cntKills[NUM_TEAMS];
static int cntItems[NUM_TEAMS];
static int cntSecret[NUM_TEAMS];
static int cntFrags[NUM_TEAMS];
static int cntTime;
static int cntPar;
static int cntPause;

// Contains information passed into intermission.
static wbstartstruct_t *wbs;

static wbplayerstruct_t *plrs;  // wbs->plyr[]

static dpatch_t bg; // Background (map of maps).
static dpatch_t yah[2]; // You Are Here.
static dpatch_t splat; // Splat.
static dpatch_t percent; // % graphic.
static dpatch_t colon; // : graphic.
static dpatch_t num[10]; // 0-9 numbers.
static dpatch_t wiminus; // Minus sign.
static dpatch_t finished; // "Finished!"
static dpatch_t entering; // "Entering"
static dpatch_t sp_secret; // "secret"
static dpatch_t kills; // "Kills"
static dpatch_t secret; // "Scrt"
static dpatch_t items; // "Items"
static dpatch_t frags; // "Frags"
static dpatch_t time; // "time"
static dpatch_t par; // "par"
static dpatch_t sucks; // "sucks!"
static dpatch_t killers; // "killers"
static dpatch_t victims; // "victims"
static dpatch_t total; // "Total"
static dpatch_t star; // Player live icon.
static dpatch_t bstar; // Player dead icon.
static dpatch_t p[MAXPLAYERS]; // "red P[1..MAXPLAYERS]"
static dpatch_t bp[MAXPLAYERS]; // "gray P[1..MAXPLAYERS]"

// CODE --------------------------------------------------------------------

void WI_slamBackground(void)
{
    GL_DrawPatch(0, 0, bg.lump);
}

/**
 * The ticker is used to detect keys because of timing issues in netgames.
 */
boolean WI_Responder(event_t *ev)
{
    return false;
}

/**
 * Draws "<MapName> Finished!"
 */
void WI_drawLF(void)
{
    int                 y = WI_TITLEY;
    int                 mapNum;
    char               *mapName;

    if(gameMode == commercial)
        mapNum = wbs->last;
    else
        mapNum = ((gameEpisode -1) * 9) + wbs->last;

    mapName = (char *) DD_GetVariable(DD_MAP_NAME);
    // Skip the E#M# or Map #.
    if(mapName)
    {
        char               *ptr = strchr(mapName, ':');

        if(ptr)
        {
            mapName = ptr + 1;
            while(*mapName && isspace(*mapName))
                mapName++;
        }
    }

    // Draw <MapName>
    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1,
                 &mapNamePatches[mapNum], mapName, false, ALIGN_CENTER);

    // Draw "Finished!"
    y += (5 * mapNamePatches[mapNum].height) / 4;

    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1,
                 &finished, NULL, false, ALIGN_CENTER);
}

/**
 * Draws "Entering <MapName>"
 */
void WI_drawEL(void)
{
    int                 y = WI_TITLEY;
    int                 mapNum;
    char               *mapName = NULL;
    ddmapinfo_t         minfo;
    char                lumpName[10];

    mapNum = G_GetMapNumber(gameEpisode, wbs->next);

    // See if there is a map name.
    P_GetMapLumpName(gameEpisode, wbs->next+1, lumpName);
    if(Def_Get(DD_DEF_MAP_INFO, lumpName, &minfo) && minfo.name)
        mapName = minfo.name;

    // Skip the E#M# or Map #.
    if(mapName)
    {
        char               *ptr = strchr(mapName, ':');

        if(ptr)
        {
            mapName = ptr + 1;
            while(*mapName && isspace(*mapName))
                mapName++;
        }
    }

    // Draw "Entering"
    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1, &entering,
                 NULL, false, ALIGN_CENTER);

    // Draw map.
    y += (5 * mapNamePatches[wbs->next].height) / 4;

    WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, 1,
                 &mapNamePatches[((gameEpisode -1) * 9) + wbs->next],
                 mapName, false, ALIGN_CENTER);
}

void WI_DrawOnMapNode(int n, dpatch_t * c)
{
    int                 i;
    int                 left, top, right, bottom;
    boolean             fits = false;

    i = 0;
    do
    {
        left = mapPoints[wbs->epsd][n].x - c[i].leftOffset;
        top = mapPoints[wbs->epsd][n].y - c[i].topOffset;
        right = left + c[i].width;
        bottom = top + c[i].height;
        if(left >= 0 && right < SCREENWIDTH && top >= 0 &&
           bottom < SCREENHEIGHT)
            fits = true;
        else
            i++;
    } while(!fits && i != 2);

    if(fits && i < 2)
    {
        WI_DrawPatch(mapPoints[wbs->epsd][n].x, mapPoints[wbs->epsd][n].y,
                     1, 1, 1, 1,
                     &c[i], NULL, false, ALIGN_LEFT);
    }
    else
    {
        Con_Message("Could not place patch on map %d", n + 1);
    }
}

void WI_initAnimatedBack(void)
{
    int                 i;
    wianim_t           *a;

    if(gameMode == commercial)
        return;
    if(wbs->epsd > 2)
        return;

    for(i = 0; i < NUMANIMS[wbs->epsd]; ++i)
    {
        a = &anims[wbs->epsd][i];

        a->ctr = -1;

        // Specify the next time to draw it.
        if(a->type == ANIM_ALWAYS)
            a->nextTic = bcnt + 1 + (M_Random() % a->period);
        else if(a->type == ANIM_RANDOM)
            a->nextTic = bcnt + 1 + a->data2 + (M_Random() % a->data1);
        else if(a->type == ANIM_MAP)
            a->nextTic = bcnt + 1;
    }
}

void WI_updateAnimatedBack(void)
{
    int                 i;
    wianim_t           *a;

    if(gameMode == commercial)
        return;
    if(wbs->epsd > 2)
        return;

    for(i = 0; i < NUMANIMS[wbs->epsd]; ++i)
    {
        a = &anims[wbs->epsd][i];

        if(bcnt == a->nextTic)
        {
            switch (a->type)
            {
            case ANIM_ALWAYS:
                if(++a->ctr >= a->numAnimFrames)
                    a->ctr = 0;
                a->nextTic = bcnt + a->period;
                break;

            case ANIM_RANDOM:
                a->ctr++;
                if(a->ctr == a->numAnimFrames)
                {
                    a->ctr = -1;
                    a->nextTic = bcnt + a->data2 + (M_Random() % a->data1);
                }
                else
                    a->nextTic = bcnt + a->period;
                break;

            case ANIM_MAP:
                // Gawd-awful hack for map anims.
                if(!(state == ILS_SHOW_STATS && i == 7) && wbs->next == a->data1)
                {
                    a->ctr++;
                    if(a->ctr == a->numAnimFrames)
                        a->ctr--;
                    a->nextTic = bcnt + a->period;
                }
                break;
            }
        }
    }
}

void WI_drawAnimatedBack(void)
{
    int                 i;
    wianim_t           *a;

    if(gameMode == commercial)
        return;
    if(wbs->epsd > 2)
        return;

    for(i = 0; i < NUMANIMS[wbs->epsd]; ++i)
    {
        a = &anims[wbs->epsd][i];
        if(a->ctr >= 0)
            WI_DrawPatch(a->loc.x, a->loc.y, 1, 1, 1, 1, &a->p[a->ctr],
                         NULL, false, ALIGN_LEFT);
    }
}

/**
 * Draw a number.
 *
 * @param digits        If > 0, then use that many digits minimum,
 *                      otherwise only use as many as necessary.
 * @return              New x position.
 */
int WI_drawNum(int x, int y, int n, int digits)
{
    int                 fontwidth = num[0].width;
    int                 neg, temp;

    if(digits < 0)
    {
        if(!n)
        {
            // Make variable-length zeros 1 digit long.
            digits = 1;
        }
        else
        {
            // Figure out # of digits in #.
            digits = 0;
            temp = n;
            while(temp)
            {
                temp /= 10;
                digits++;
            }
        }
    }
    neg = n < 0;
    if(neg)
        n = -n;

    // If non-number, do not draw it.
    if(n == 1994)
        return 0;

    // Draw the new number.
    while(digits--)
    {
        x -= fontwidth;
        WI_DrawPatch(x, y, 1, 1, 1, 1, &num[n % 10], NULL, false, ALIGN_LEFT);
        n /= 10;
    }

    // Draw a minus sign if necessary.
    if(neg)
        WI_DrawPatch(x -= 8, y, 1, 1, 1, 1, &wiminus, NULL, false, ALIGN_LEFT);

    return x;
}

void WI_drawPercent(int x, int y, int p)
{
    if(p < 0)
        return;

    WI_DrawPatch(x, y, 1, 1, 1, 1, &percent, NULL, false, ALIGN_LEFT);
    WI_drawNum(x, y, p, -1);
}

/**
 * Display map completion time and par, or "sucks" message if overflow.
 */
void WI_drawTime(int x, int y, int t)
{
    int                 div, n;

    if(t < 0)
        return;

    if(t <= 61 * 59)
    {
        div = 1;
        do
        {
            n = (t / div) % 60;
            x = WI_drawNum(x, y, n, 2) - colon.width;
            div *= 60;

            if(div == 60 || t / div)
                WI_DrawPatch(x, y, 1, 1, 1, 1, &colon, NULL, false,
                             ALIGN_LEFT);

        } while(t / div);
    }
    else
    {
        // "sucks"
        WI_DrawPatch(x - sucks.width, y, 1, 1, 1, 1, &sucks,
                     NULL, false, ALIGN_LEFT);
    }
}

void WI_End(void)
{
    NetSv_Intermission(IMF_END, 0, 0);
}

void WI_initNoState(void)
{
    state = ILS_NONE;
    accelerateStage = 0;
    cnt = 10;

    NetSv_Intermission(IMF_STATE, state, 0);
}

void WI_updateNoState(void)
{
    WI_updateAnimatedBack();

    if(!--cnt)
    {
        if(IS_CLIENT)
            return;
        WI_End();
        G_WorldDone();
    }
}

void WI_initShowNextLoc(void)
{
    state = ILS_SHOW_NEXTMAP;
    accelerateStage = 0;
    cnt = SHOWNEXTLOCDELAY * TICRATE;

    WI_initAnimatedBack();

    NetSv_Intermission(IMF_STATE, state, 0);
}

void WI_updateShowNextLoc(void)
{
    WI_updateAnimatedBack();

    if(!--cnt || accelerateStage)
        WI_initNoState();
    else
        snlPointerOn = (cnt & 31) < 20;
}

void WI_drawShowNextLoc(void)
{
    int                 i;
    int                 last;

    WI_slamBackground();

    // Draw animated background.
    WI_drawAnimatedBack();

    if(gameMode != commercial)
    {
        if(wbs->epsd > 2)
        {
            WI_drawEL();
            return;
        }

        last = (wbs->last == 8) ? wbs->next - 1 : wbs->last;

        // Draw a splat on taken cities.
        for(i = 0; i <= last; ++i)
            WI_DrawOnMapNode(i, &splat);

        // Splat the secret map?
        if(wbs->didSecret)
            WI_DrawOnMapNode(8, &splat);

        // Draw flashing ptr.
        if(snlPointerOn)
            WI_DrawOnMapNode(wbs->next, yah);
    }

    // Draws which map you are entering..
    if((gameMode != commercial) || wbs->next != 30)
        WI_drawEL();
}

void WI_drawNoState(void)
{
    snlPointerOn = true;
    WI_drawShowNextLoc();
}

int WI_fragSum(int teamnum)
{
    return teamInfo[teamnum].totalFrags;
}

void WI_initDeathmatchStats(void)
{
    int                 i;

    state = ILS_SHOW_STATS;
    accelerateStage = 0;
    dmState = 1;

    cntPause = TICRATE;

    // Clear the on-screen counters.
    memset(dmTotals, 0, sizeof(dmTotals));
    for(i = 0; i < NUM_TEAMS; ++i)
        memset(dmFrags[i], 0, sizeof(dmFrags[i]));

    WI_initAnimatedBack();
}

void WI_updateDeathmatchStats(void)
{
    int                 i, j;
    boolean             stillTicking;

    WI_updateAnimatedBack();

    if(accelerateStage && dmState != 4)
    {
        accelerateStage = 0;
        for(i = 0; i < NUM_TEAMS; ++i)
        {
            for(j = 0; j < NUM_TEAMS; ++j)
            {
                dmFrags[i][j] = teamInfo[i].frags[j];
            }

            dmTotals[i] = WI_fragSum(i);
        }

        S_LocalSound(SFX_BAREXP, 0);
        dmState = 4;
    }

    if(dmState == 2)
    {
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        stillTicking = false;
        for(i = 0; i < NUM_TEAMS; ++i)
        {
            for(j = 0; j < NUM_TEAMS; ++j)
            {
                if(dmFrags[i][j] != teamInfo[i].frags[j])
                {
                    if(teamInfo[i].frags[j] < 0)
                        dmFrags[i][j]--;
                    else
                        dmFrags[i][j]++;

                    if(dmFrags[i][j] > 99)
                        dmFrags[i][j] = 99;

                    if(dmFrags[i][j] < -99)
                        dmFrags[i][j] = -99;

                    stillTicking = true;
                }
            }

            dmTotals[i] = WI_fragSum(i);

            if(dmTotals[i] > 99)
                dmTotals[i] = 99;

            if(dmTotals[i] < -99)
                dmTotals[i] = -99;
        }

        if(!stillTicking)
        {
            S_LocalSound(SFX_BAREXP, 0);
            dmState++;
        }
    }
    else if(dmState == 4)
    {
        if(accelerateStage)
        {
            S_LocalSound(SFX_SLOP, 0);
            if(gameMode == commercial)
                WI_initNoState();
            else
                WI_initShowNextLoc();
        }
    }
    else if(dmState & 1)
    {
        if(!--cntPause)
        {
            dmState++;
            cntPause = TICRATE;
        }
    }
}

void WI_drawDeathmatchStats(void)
{
    int                 i, j;
    int                 x, y;
    int                 w;
    int                 lh; // Line height.

    lh = WI_SPACINGY;

    WI_slamBackground();

    // Draw animated background.
    WI_drawAnimatedBack();
    WI_drawLF();

    // Draw stat titles (top line).
    WI_DrawPatch(DM_TOTALSX - total.width / 2,
                 DM_MATRIXY - WI_SPACINGY + 10, 1, 1, 1, 1, &total, NULL,
                 false, ALIGN_LEFT);

    WI_DrawPatch(DM_KILLERSX, DM_KILLERSY, 1, 1, 1, 1, &killers, NULL,
                 false, ALIGN_LEFT);
    WI_DrawPatch(DM_VICTIMSX, DM_VICTIMSY, 1, 1, 1, 1, &victims, NULL,
                 false, ALIGN_LEFT);

    x = DM_MATRIXX + DM_SPACINGX;
    y = DM_MATRIXY;

    for(i = 0; i < NUM_TEAMS; ++i)
    {
        if(teamInfo[i].members)
        {
            WI_DrawPatch(x - p[i].width / 2, DM_MATRIXY - WI_SPACINGY,
                         1, 1, 1, 1, &p[i], NULL, false, ALIGN_LEFT);

            WI_DrawPatch(DM_MATRIXX - p[i].width / 2, y, 1, 1, 1, 1,
                         &p[i], NULL, false, ALIGN_LEFT);

            if(i == myTeam)
            {
                WI_DrawPatch(x - p[i].width / 2,
                             DM_MATRIXY - WI_SPACINGY, 1, 1, 1, 1, &bstar,
                             NULL, false, ALIGN_LEFT);

                WI_DrawPatch(DM_MATRIXX - p[i].width / 2, y, 1, 1, 1, 1,
                             &star, NULL, false, ALIGN_LEFT);
            }

            // If more than 1 member, show the member count.
            if(teamInfo[i].members > 1)
            {
                char                tmp[20];

                sprintf(tmp, "%i", teamInfo[i].members);
                M_WriteText2(x - p[i].width / 2 + 1,
                             DM_MATRIXY - WI_SPACINGY + p[i].height - 8, tmp,
                             GF_FONTA, 1, 1, 1, 1);
                M_WriteText2(DM_MATRIXX - p[i].width / 2 + 1,
                             y + p[i].height - 8, tmp, GF_FONTA, 1, 1, 1, 1);
            }
        }
        else
        {
            WI_DrawPatch(x - bp[i].width / 2, DM_MATRIXY - WI_SPACINGY, 1, 1, 1, 1,
                         &bp[i], NULL, false, ALIGN_LEFT);

            WI_DrawPatch(DM_MATRIXX - bp[i].width / 2, y, 1, 1, 1, 1,
                         &bp[i], NULL, false, ALIGN_LEFT);
        }

        x += DM_SPACINGX;
        y += WI_SPACINGY;
    }

    // Draw stats.
    y = DM_MATRIXY + 10;
    w = num[0].width;

    for(i = 0; i < NUM_TEAMS; ++i)
    {
        x = DM_MATRIXX + DM_SPACINGX;
        if(teamInfo[i].members)
        {
            for(j = 0; j < NUM_TEAMS; ++j)
            {
                if(teamInfo[j].members)
                    WI_drawNum(x + w, y, dmFrags[i][j], 2);
                x += DM_SPACINGX;
            }
            WI_drawNum(DM_TOTALSX + w, y, dmTotals[i], 2);
        }

        y += WI_SPACINGY;
    }
}

void WI_initNetgameStats(void)
{
    int                 i;

    state = ILS_SHOW_STATS;
    accelerateStage = 0;
    ngState = 1;
    cntPause = TICRATE;

    memset(cntKills, 0, sizeof(cntKills));
    memset(cntItems, 0, sizeof(cntItems));
    memset(cntSecret, 0, sizeof(cntSecret));
    memset(cntFrags, 0, sizeof(cntFrags));
    doFrags = 0;

    for(i = 0; i < NUM_TEAMS; ++i)
    {
        doFrags += teamInfo[i].totalFrags;
    }
    doFrags = !doFrags;

    WI_initAnimatedBack();
}

void WI_updateNetgameStats(void)
{
    int                 i;
    int                 fsum;
    boolean             stillTicking;

    WI_updateAnimatedBack();

    if(accelerateStage && ngState != 10)
    {
        accelerateStage = 0;
        for(i = 0; i < NUM_TEAMS; ++i)
        {
            cntKills[i] = (teamInfo[i].kills * 100) / wbs->maxKills;
            cntItems[i] = (teamInfo[i].items * 100) / wbs->maxItems;
            cntSecret[i] = (teamInfo[i].secret * 100) / wbs->maxSecret;

            if(doFrags)
                cntFrags[i] = teamInfo[i].totalFrags;
        }

        S_LocalSound(SFX_BAREXP, 0);
        ngState = 10;
    }

    if(ngState == 2)
    {
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);
        stillTicking = false;

        for(i = 0; i < NUM_TEAMS; ++i)
        {
            cntKills[i] += 2;

            if(cntKills[i] >= (teamInfo[i].kills * 100) / wbs->maxKills)
                cntKills[i] = (teamInfo[i].kills * 100) / wbs->maxKills;
            else
                stillTicking = true;
        }

        if(!stillTicking)
        {
            S_LocalSound(SFX_BAREXP, 0);
            ngState++;
        }
    }
    else if(ngState == 4)
    {
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);
        stillTicking = false;

        for(i = 0; i < NUM_TEAMS; ++i)
        {
            cntItems[i] += 2;
            if(cntItems[i] >= (teamInfo[i].items * 100) / wbs->maxItems)
                cntItems[i] = (teamInfo[i].items * 100) / wbs->maxItems;
            else
                stillTicking = true;
        }

        if(!stillTicking)
        {
            S_LocalSound(SFX_BAREXP, 0);
            ngState++;
        }
    }
    else if(ngState == 6)
    {
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        stillTicking = false;

        for(i = 0; i < NUM_TEAMS; ++i)
        {
            cntSecret[i] += 2;

            if(cntSecret[i] >= (teamInfo[i].secret * 100) / wbs->maxSecret)
                cntSecret[i] = (teamInfo[i].secret * 100) / wbs->maxSecret;
            else
                stillTicking = true;
        }

        if(!stillTicking)
        {
            S_LocalSound(SFX_BAREXP, 0);
            ngState += 1 + 2 * !doFrags;
        }
    }
    else if(ngState == 8)
    {
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        stillTicking = false;

        for(i = 0; i < NUM_TEAMS; ++i)
        {
            cntFrags[i] += 1;

            if(cntFrags[i] >= (fsum = WI_fragSum(i)))
                cntFrags[i] = fsum;
            else
                stillTicking = true;
        }

        if(!stillTicking)
        {
            S_LocalSound(SFX_PLDETH, 0);
            ngState++;
        }
    }
    else if(ngState == 10)
    {
        if(accelerateStage)
        {
            S_LocalSound(SFX_SGCOCK, 0);
            if(gameMode == commercial)
                WI_initNoState();
            else
                WI_initShowNextLoc();
        }
    }
    else if(ngState & 1)
    {
        if(!--cntPause)
        {
            ngState++;
            cntPause = TICRATE;
        }
    }
}

void WI_drawNetgameStats(void)
{
    int                 i;
    int                 x, y;
    int                 pwidth = percent.width;

    WI_slamBackground();

    // Draw animated background.
    WI_drawAnimatedBack();

    WI_drawLF();

    // Draw stat titles (top line).
    WI_DrawPatch(NG_STATSX + NG_SPACINGX - kills.width, NG_STATSY,
                 1, 1, 1, 1, &kills, NULL, false, ALIGN_LEFT);

    WI_DrawPatch(NG_STATSX + 2 * NG_SPACINGX - items.width, NG_STATSY,
                 1, 1, 1, 1, &items, NULL, false, ALIGN_LEFT);

    WI_DrawPatch(NG_STATSX + 3 * NG_SPACINGX - secret.width, NG_STATSY,
                 1, 1, 1, 1, &secret, NULL, false, ALIGN_LEFT);

    if(doFrags)
        WI_DrawPatch(NG_STATSX + 4 * NG_SPACINGX - frags.width,
                     1, 1, 1, 1, NG_STATSY, &frags, NULL, false, ALIGN_LEFT);

    // Draw stats.
    y = NG_STATSY + kills.height;

    for(i = 0; i < NUM_TEAMS; ++i)
    {
        if(!teamInfo[i].members)
            continue;

        x = NG_STATSX;
        WI_DrawPatch(x - p[i].width, y, 1, 1, 1, 1, &p[i], NULL,
                     false, ALIGN_LEFT);

        // If more than 1 member, show the member count.
        if(teamInfo[i].members > 1)
        {
            char                tmp[40];

            sprintf(tmp, "%i", teamInfo[i].members);
            M_WriteText2(x - p[i].width + 1, y + p[i].height - 8, tmp,
                         GF_FONTA, 1, 1, 1, 1);
        }

        if(i == myTeam)
            WI_DrawPatch(x - p[i].width, y, 1, 1, 1, 1, &star, NULL,
                         false, ALIGN_LEFT);

        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cntKills[i]);
        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cntItems[i]);
        x += NG_SPACINGX;
        WI_drawPercent(x - pwidth, y + 10, cntSecret[i]);
        x += NG_SPACINGX;

        if(doFrags)
            WI_drawNum(x, y + 10, cntFrags[i], -1);

        y += WI_SPACINGY;
    }
}

void WI_initStats(void)
{
    state = ILS_SHOW_STATS;
    accelerateStage = 0;
    spState = 1;
    cntKills[0] = cntItems[0] = cntSecret[0] = -1;
    cntTime = cntPar = -1;
    cntPause = TICRATE;
    WI_initAnimatedBack();
}

void WI_updateStats(void)
{
    WI_updateAnimatedBack();

    if(accelerateStage && spState != 10)
    {
        accelerateStage = 0;
        cntKills[0] = (plrs[me].kills * 100) / wbs->maxKills;
        cntItems[0] = (plrs[me].items * 100) / wbs->maxItems;
        cntSecret[0] = (plrs[me].secret * 100) / wbs->maxSecret;
        cntTime = plrs[me].time / TICRATE;
        if(wbs->parTime != -1)
            cntPar = wbs->parTime / TICRATE;

        S_LocalSound(SFX_BAREXP, 0);
        spState = 10;
    }

    if(spState == 2)
    {
        cntKills[0] += 2;

        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        if(cntKills[0] >= (plrs[me].kills * 100) / wbs->maxKills)
        {
            cntKills[0] = (plrs[me].kills * 100) / wbs->maxKills;
            S_LocalSound(SFX_BAREXP, 0);
            spState++;
        }
    }
    else if(spState == 4)
    {
        cntItems[0] += 2;

        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        if(cntItems[0] >= (plrs[me].items * 100) / wbs->maxItems)
        {
            cntItems[0] = (plrs[me].items * 100) / wbs->maxItems;
            S_LocalSound(SFX_BAREXP, 0);
            spState++;
        }
    }
    else if(spState == 6)
    {
        cntSecret[0] += 2;

        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        if(cntSecret[0] >= (plrs[me].secret * 100) / wbs->maxSecret)
        {
            cntSecret[0] = (plrs[me].secret * 100) / wbs->maxSecret;
            S_LocalSound(SFX_BAREXP, 0);
            spState++;
        }
    }
    else if(spState == 8)
    {
        if(!(bcnt & 3))
            S_LocalSound(SFX_PISTOL, 0);

        cntTime += 3;

        if(cntTime >= plrs[me].time / TICRATE)
            cntTime = plrs[me].time / TICRATE;

        if(cntPar != -1)
        {
            cntPar += 3;

            if(cntPar >= wbs->parTime / TICRATE)
            {
                cntPar = wbs->parTime / TICRATE;

                if(cntTime >= plrs[me].time / TICRATE)
                {
                    S_LocalSound(SFX_BAREXP, 0);
                    spState++;
                }
            }
        }
        else
            spState++;
    }
    else if(spState == 10)
    {
        if(accelerateStage)
        {
            S_LocalSound(SFX_SGCOCK, 0);

            if(gameMode == commercial)
                WI_initNoState();
            else
                WI_initShowNextLoc();
        }
    }
    else if(spState & 1)
    {
        if(!--cntPause)
        {
            spState++;
            cntPause = TICRATE;
        }
    }
}

void WI_drawStats(void)
{
    int                 lh; // Line height.

    lh = (3 * num[0].height) / 2;

    WI_slamBackground();

    // Draw animated background.
    WI_drawAnimatedBack();

    WI_drawLF();

    WI_DrawPatch(SP_STATSX, SP_STATSY, 1, 1, 1, 1, &kills, NULL,
                 false, ALIGN_LEFT);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, cntKills[0]);

    WI_DrawPatch(SP_STATSX, SP_STATSY + lh, 1, 1, 1, 1, &items, NULL,
                 false, ALIGN_LEFT);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + lh, cntItems[0]);

    WI_DrawPatch(SP_STATSX, SP_STATSY + 2 * lh, 1, 1, 1, 1, &sp_secret,
                 NULL, false, ALIGN_LEFT);
    WI_drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + 2 * lh, cntSecret[0]);

    WI_DrawPatch(SP_TIMEX, SP_TIMEY, 1, 1, 1, 1, &time, NULL, false,
                 ALIGN_LEFT);
    WI_drawTime(SCREENWIDTH / 2 - SP_TIMEX, SP_TIMEY, cntTime);

    if(wbs->epsd < 3 && wbs->parTime != -1)
    {
        WI_DrawPatch(SCREENWIDTH / 2 + SP_TIMEX, SP_TIMEY, 1, 1, 1, 1, &par,
                     NULL, false, ALIGN_LEFT);
        WI_drawTime(SCREENWIDTH - SP_TIMEX, SP_TIMEY, cntPar);
    }
}

void WI_checkForAccelerate(void)
{
    int                 i;
    player_t           *player;

    // Check for button presses to skip delays.
    for(i = 0, player = players; i < MAXPLAYERS; ++i, player++)
    {
        if(players[i].plr->inGame)
        {
            if(player->brain.attack)
            {
                if(!player->attackDown)
                    accelerateStage = 1;
                player->attackDown = true;
            }
            else
            {
                player->attackDown = false;
            }

            if(player->brain.use)
            {
                if(!player->useDown)
                    accelerateStage = 1;
                player->useDown = true;
            }
            else
            {
                player->useDown = false;
            }
        }
    }
}

/**
 * Called every tic when gameState == GS_INTERMISSION.
 */
void WI_Ticker(void)
{
    // Counter for general background animation.
    bcnt++;

    if(bcnt == 1)
    {
        // Intermission music.
        if(gameMode == commercial)
            S_StartMusic("dm2int", true);
        else
            S_StartMusic("inter", true);
    }

    WI_checkForAccelerate();

    switch(state)
    {
    case ILS_SHOW_STATS:
        if(deathmatch)
            WI_updateDeathmatchStats();
        else if(IS_NETGAME)
            WI_updateNetgameStats();
        else
            WI_updateStats();
        break;

    case ILS_SHOW_NEXTMAP:
        WI_updateShowNextLoc();
        break;

    case ILS_NONE:
    default:
        WI_updateNoState();
        break;
    }
}

void WI_loadData(void)
{
    int                 i, j;
    char                name[9];
    wianim_t           *a;

    if(gameMode == commercial)
        strcpy(name, "INTERPIC");
    else
        sprintf(name, "WIMAP%d", wbs->epsd);

    if(gameMode == retail)
    {
        if(wbs->epsd == 3)
            strcpy(name, "INTERPIC");
    }

    if(!Get(DD_NOVIDEO))
    {
        R_CachePatch(&bg, name);
        GL_DrawPatch(0, 0, bg.lump);
    }

    if(gameMode != commercial)
    {
        // You are here.
        R_CachePatch(&yah[0], "WIURH0");

        // You are here (alt.)
        R_CachePatch(&yah[1], "WIURH1");

        // Splat.
        R_CachePatch(&splat, "WISPLAT");

        if(wbs->epsd < 3)
        {
            for(j = 0; j < NUMANIMS[wbs->epsd]; ++j)
            {
                a = &anims[wbs->epsd][j];
                for(i = 0; i < a->numAnimFrames; ++i)
                {
                    //// \kludge >
                    if(wbs->epsd != 1 || j != 8)
                    {
                        // Animations
                        sprintf(name, "WIA%d%.2d%.2d", wbs->epsd, j, i);
                        R_CachePatch(&a->p[i], name);
                    }
                    else
                    {
                        memcpy(&a->p[i], &anims[1][4].p[i], sizeof(dpatch_t));
                    }
                    //// \kludge <
                }
            }
        }
    }

    R_CachePatch(&wiminus, "WIMINUS");

    for(i = 0; i < 10; ++i)
    {
        // Numbers 0-9.
        sprintf(name, "WINUM%d", i);
        R_CachePatch(&num[i], name);
    }

    // Percent sign.
    R_CachePatch(&percent, "WIPCNT");

    // "finished"
    R_CachePatch(&finished, "WIF");

    // "entering"
    R_CachePatch(&entering, "WIENTER");

    // "kills"
    R_CachePatch(&kills, "WIOSTK");

    // "scrt"
    R_CachePatch(&secret, "WIOSTS");

    // "secret"
    R_CachePatch(&sp_secret, "WISCRT2");

    // "items"
    R_CachePatch(&items, "WIOSTI");

    // "frgs"
    R_CachePatch(&frags, "WIFRGS");

    // ":"
    R_CachePatch(&colon, "WICOLON");

    // "time"
    R_CachePatch(&time, "WITIME");

    // "sucks"
    R_CachePatch(&sucks, "WISUCKS");

    // "par"
    R_CachePatch(&par, "WIPAR");

    // "killers" (vertical)
    R_CachePatch(&killers, "WIKILRS");

    // "victims" (horiz)
    R_CachePatch(&victims, "WIVCTMS");

    // "total"
    R_CachePatch(&total, "WIMSTT");

    // your face
    R_CachePatch(&star, "STFST01");

    // dead face
    R_CachePatch(&bstar, "STFDEAD0");

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        // "1,2,3,4"
        sprintf(name, "STPB%d", i);
        R_CachePatch(&p[i], name);

        // "1,2,3,4"
        sprintf(name, "WIBP%d", i + 1);
        R_CachePatch(&bp[i], name);
    }
}

void WI_Drawer(void)
{
    switch(state)
    {
    case ILS_SHOW_STATS:
        if(deathmatch)
            WI_drawDeathmatchStats();
        else if(IS_NETGAME)
            WI_drawNetgameStats();
        else
            WI_drawStats();
        break;

    case ILS_SHOW_NEXTMAP:
        WI_drawShowNextLoc();
        break;

    case ILS_NONE:
    default:
        WI_drawNoState();
        break;
    }
}

void WI_initVariables(wbstartstruct_t * wbstartstruct)
{
    wbs = wbstartstruct;

#ifdef RANGECHECK
    if(gameMode != commercial)
    {
        if(gameMode == retail)
            RNGCHECK(wbs->epsd, 0, 3);
        else
            RNGCHECK(wbs->epsd, 0, 2);
    }
    else
    {
        RNGCHECK(wbs->last, 0, 8);
        RNGCHECK(wbs->next, 0, 8);
    }
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
#endif

    accelerateStage = 0;
    cnt = bcnt = 0;
    firstRefresh = 1;
    me = wbs->pNum;
    myTeam = cfg.playerColor[wbs->pNum];
    plrs = wbs->plyr;

    if(!wbs->maxKills)
        wbs->maxKills = 1;
    if(!wbs->maxItems)
        wbs->maxItems = 1;
    if(!wbs->maxSecret)
        wbs->maxSecret = 1;

    if(gameMode != retail)
        if(wbs->epsd > 2)
            wbs->epsd -= 3;
}

void WI_Start(wbstartstruct_t *wbstartstruct)
{
    int                 i, j, k;
    teaminfo_t         *tin;

    GL_SetFilter(false);
    WI_initVariables(wbstartstruct);
    WI_loadData();

    // Calculate team stats.
    memset(teamInfo, 0, sizeof(teamInfo));
    for(i = 0, tin = teamInfo; i < NUM_TEAMS; ++i, tin++)
    {
        for(j = 0; j < MAXPLAYERS; ++j)
        {
            // Is the player in this team?
            if(!plrs[j].inGame || cfg.playerColor[j] != i)
                continue;

            tin->members++;

            // Check the frags.
            for(k = 0; k < MAXPLAYERS; ++k)
                tin->frags[cfg.playerColor[k]] += plrs[j].frags[k];

            // Counters.
            if(plrs[j].items > tin->items)
                tin->items = plrs[j].items;
            if(plrs[j].kills > tin->kills)
                tin->kills = plrs[j].kills;
            if(plrs[j].secret > tin->secret)
                tin->secret = plrs[j].secret;
        }

        // Calculate team's total frags.
        for(j = 0; j < NUM_TEAMS; ++j)
        {
            if(j == i) // Suicides are negative frags.
                tin->totalFrags -= tin->frags[j];
            else
                tin->totalFrags += tin->frags[j];
        }
    }

    if(deathmatch)
        WI_initDeathmatchStats();
    else if(IS_NETGAME)
        WI_initNetgameStats();
    else
        WI_initStats();
}

void WI_SetState(interludestate_t st)
{
    if(st == ILS_SHOW_STATS)
        WI_initStats();
    if(st == ILS_SHOW_NEXTMAP)
        WI_initShowNextLoc();
    if(st == ILS_NONE)
        WI_initNoState();
}
