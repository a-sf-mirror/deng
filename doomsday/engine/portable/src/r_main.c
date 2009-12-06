/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * r_main.c: Refresh Subsystem
 *
 * The refresh daemon has the highest-level rendering code.
 * The view window is handled by refresh. The more specialized
 * rendering code in rend_*.c does things inside the view window.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"
#include "de_ui.h"
#include "de_edit.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
  PROF_MOBJ_INIT_ADD,
  PROF_PARTICLE_INIT_ADD
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(ViewGrid);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern byte rendInfoRPolys;
extern byte freezeRLs;
extern boolean firstFrameAfterLoad;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int validCount = 1; // Increment every time a check is made.
int frameCount; // Just for profiling purposes.
int rendInfoTris = 0;
int useVSync = 0;
float viewX = 0, viewY = 0, viewZ = 0, viewPitch = 0;
int viewAngle = 0;
boolean setSizeNeeded;

float vx, vy, vz, vang, vpitch;
float viewsidex, viewsidey;

float fieldOfView = 95.0f;
float yfov;

int viewpw, viewph; // Viewport size, in pixels.
int viewpx, viewpy; // Viewpoint top left corner, in pixels.

// Precalculated math tables.
fixed_t* fineCosine = &finesine[FINEANGLES / 4];

float torchColor[3] = {1, 1, 1};
int torchAdditive = true;
int extraLight; // Bumped light from gun blasts.
float extraLightDelta;

float frameTimePos; // 0...1: fractional part for sharp game tics.

int loadInStartupMode = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int rendCameraSmooth = true; // Smoothed by default.

static boolean resetNextViewer = true;

static viewdata_t viewData[DDMAXPLAYERS];

static byte showFrameTimePos = false;
static byte showViewAngleDeltas = false;
static byte showViewPosDeltas = false;

static int gridCols, gridRows;
static viewport_t viewports[DDMAXPLAYERS], *currentPort;

// CODE --------------------------------------------------------------------

/**
 * Register console variables.
 */
void R_Register(void)
{
    C_VAR_INT("con-show-during-setup", &loadInStartupMode, 0, 0, 1);

    C_VAR_INT("rend-camera-smooth", &rendCameraSmooth, CVF_HIDE, 0, 1);

    C_VAR_BYTE("rend-info-deltas-angles", &showViewAngleDeltas, 0, 0, 1);
    C_VAR_BYTE("rend-info-deltas-pos", &showViewPosDeltas, 0, 0, 1);
    C_VAR_BYTE("rend-info-frametime", &showFrameTimePos, 0, 0, 1);
    C_VAR_INT("rend-info-tris", &rendInfoTris, 0, 0, 1);
    C_VAR_FLOAT("rend-surface-material-fade-seconds", &rendMaterialFadeSeconds, CVF_NO_MAX, 0, 1);
//    C_VAR_INT("rend-vsync", &useVSync, 0, 0, 1);

    C_CMD("viewgrid", "ii", ViewGrid);

#if _DEBUG
    C_CMD("updatesurfaces", "", UpdateSurfaces);
#endif

    Materials_Register();
}

float R_FacingViewerDot(vertex_t* from, vertex_t* to)
{
    // The dot product.
    return (from->pos[VY] - to->pos  [VY]) * (from->pos[VX] - vx) +
           (to->pos  [VX] - from->pos[VX]) * (from->pos[VY] - vz);
}

/**
 * Approximated! The Z axis aspect ratio is corrected.
 */
float R_PointDist3D(const float c[3])
{
    return M_ApproxDistance3f(vx - c[VX], vz - c[VY], 1.2f * (vy - c[VZ]));
}

/**
 * Don't really change anything here, because i might be in the middle of
 * a refresh.  The change will take effect next refresh.
 */
void R_SetViewWindow(int x, int y, int w, int h)
{
    viewwindowx = x;
    viewwindowy = y;
    viewwidth = w;
    viewheight = h;
}

/**
 * Retrieve the dimensions of the specified viewport by console player num.
 */
int R_GetViewPort(int player, int* x, int* y, int* w, int* h)
{
    int p = P_ConsoleToLocal(player);

    if(p != -1)
    {
        if(x) *x = viewports[p].x;
        if(y) *y = viewports[p].y;
        if(w) *w = viewports[p].width;
        if(h) *h = viewports[p].height;

        return p;
    }

    return -1;
}

/**
 * Calculate the placement and dimensions of a specific viewport.
 * Assumes that the grid has already been configured.
 */
void R_ViewPortPlacement(viewport_t* port, int x, int y)
{
    float w = theWindow->width / (float) gridCols;
    float h = theWindow->height / (float) gridRows;

    port->x = x * w;
    port->y = y * h;

    port->width = (x + 1) * w - port->x;
    port->height = (y + 1) * h - port->y;
}

/**
 * Set up a view grid and calculate the viewports.  Set 'numCols' and
 * 'numRows' to zero to just update the viewport coordinates.
 */
void R_SetViewGrid(int numCols, int numRows)
{
    int x, y, p;

    if(numCols > 0 && numRows > 0)
    {
        if(numCols > 16)
            numCols = 16;
        if(numRows > 16)
            numRows = 16;

        gridCols = numCols;
        gridRows = numRows;
    }

    // Reset all viewports to zero.
    memset(viewports, 0, sizeof(viewports));

    for(p = 0, y = 0; y < gridRows; ++y)
    {
        for(x = 0; x < gridCols; ++x, ++p)
        {
            R_ViewPortPlacement(&viewports[p], x, y);

            // The console number is -1 if the viewport belongs to no
            // one.
            viewports[p].console = displayPlayer; //clients[P_LocalToConsole(p)].viewConsole;
        }
    }
}

/**
 * One-time initialization of the refresh daemon. Called by DD_Main.
 * GL has not yet been inited.
 */
void R_Init(void)
{
    R_InitData();
    // viewwidth / viewheight / detailLevel are set by the defaults
    R_SetViewWindow(0, 0, 320, 200);
    R_InitSprites(); // Fully initialize sprites.
    R_InitTranslationTables();
    Rend_Init();
    frameCount = 0;
    R_InitViewBorder();

    // Defs have been read; we can now init models.
    R_InitModels();

    Def_PostInit();
}

/**
 * Re-initialize almost everything.
 */
void R_Update(void)
{
    uint i;

    R_UpdateTexturesAndFlats();
    R_InitTextures();
    R_InitFlats();
    R_PreInitSprites();

    // Re-read definitions.
    Def_Read();

    R_UpdateData();
    R_InitSprites(); // Fully reinitialize sprites.
    R_UpdateTranslationTables();

    R_InitModels(); // Defs might've changed.

    // Now that we've read the defs, we can load system textures.
    GL_LoadSystemTextures();

    Def_PostInit();

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t* plr = &ddPlayers[i];
        ddplayer_t* ddpl = &plr->shared;

        // States have changed, the states are unknown.
        ddpl->pSprites[0].statePtr = ddpl->pSprites[1].statePtr = NULL;
    }

    Map_Update(P_CurrentMap());

    // The rendering lists have persistent data that has changed during
    // the re-initialization.
    RL_DeleteLists();

    // Update the secondary title and the game status.
    Con_InitUI();

#if _DEBUG
    Z_CheckHeap();
#endif
}

/**
 * Shutdown the refresh daemon.
 */
void R_Shutdown(void)
{
    R_ShutdownModels();
    R_ShutdownData();
    R_ShutdownResourceLocator();
    // Most allocated memory goes down with the zone.
}

void R_ResetViewer(void)
{
    resetNextViewer = 1;
}

void R_InterpolateViewer(viewer_t* start, viewer_t* end, float pos,
                         viewer_t* out)
{
    float inv = 1 - pos;

    out->pos[VX] = inv * start->pos[VX] + pos * end->pos[VX];
    out->pos[VY] = inv * start->pos[VY] + pos * end->pos[VY];
    out->pos[VZ] = inv * start->pos[VZ] + pos * end->pos[VZ];

    out->angle = start->angle + pos * ((int) end->angle - (int) start->angle);
    out->pitch = inv * start->pitch + pos * end->pitch;
}

void R_CopyViewer(viewer_t* dst, const viewer_t* src)
{
    dst->pos[VX] = src->pos[VX];
    dst->pos[VY] = src->pos[VY];
    dst->pos[VZ] = src->pos[VZ];
    dst->angle = src->angle;
    dst->pitch = src->pitch;
}

const viewdata_t* R_ViewData(int localPlayerNum)
{
    assert(localPlayerNum >= 0 && localPlayerNum < DDMAXPLAYERS);

    return &viewData[localPlayerNum];
}

/**
 * The components whose difference is too large for interpolation will be
 * snapped to the sharp values.
 */
void R_CheckViewerLimits(viewer_t* src, viewer_t* dst)
{
#define MAXMOVE 32

    if(fabs(dst->pos[VX] - src->pos[VX]) > MAXMOVE ||
       fabs(dst->pos[VY] - src->pos[VY]) > MAXMOVE)
    {
        src->pos[VX] = dst->pos[VX];
        src->pos[VY] = dst->pos[VY];
        src->pos[VZ] = dst->pos[VZ];
    }
    if(abs((int) dst->angle - (int) src->angle) >= ANGLE_45)
        src->angle = dst->angle;

#undef MAXMOVE
}

/**
 * Retrieve the current sharp camera position.
 */
void R_GetSharpView(viewer_t* view, player_t* player)
{
    ddplayer_t* ddpl;

    if(!player || !player->shared.mo)
    {
        return;
    }

    ddpl = &player->shared;

    view->pos[VX] = viewX;
    view->pos[VY] = viewY;
    view->pos[VZ] = viewZ;
    /* $unifiedangles */
    view->angle = viewAngle;
    view->pitch = viewPitch;

    if((ddpl->flags & DDPF_CHASECAM) && !(ddpl->flags & DDPF_CAMERA))
    {
        /**
         * @todo This needs to be fleshed out with a proper third person
         * camera control setup. Currently we simply project the viewer's
         * position a set distance behind the player.
         */
        angle_t pitch = LOOKDIR2DEG(view->pitch) / 360 * ANGLE_MAX;
        angle_t angle = view->angle;
        float distance = 90;

        angle = view->angle >> ANGLETOFINESHIFT;
        pitch >>= ANGLETOFINESHIFT;

        view->pos[VX] -= distance * FIX2FLT(fineCosine[angle]);
        view->pos[VY] -= distance * FIX2FLT(finesine[angle]);
        view->pos[VZ] -= distance * FIX2FLT(finesine[pitch]);
    }

    // Check that the viewZ doesn't go too high or low.
    // Cameras are not restricted.
    if(!(ddpl->flags & DDPF_CAMERA))
    {
        if(view->pos[VZ] > ddpl->mo->ceilingZ - 4)
        {
            view->pos[VZ] = ddpl->mo->ceilingZ - 4;
        }

        if(view->pos[VZ] < ddpl->mo->floorZ + 4)
        {
            view->pos[VZ] = ddpl->mo->floorZ + 4;
        }
    }
}

/**
 * Update the sharp world data by rotating the stored values of plane
 * heights and sharp camera positions.
 */
void R_NewSharpWorld(void)
{
    extern boolean firstFrameAfterLoad;

    int i;
    map_t* map;

    if(firstFrameAfterLoad)
    {
        /**
         * We haven't yet drawn the world. Everything *is* sharp so simply
         * reset the viewer data.
         * \fixme A bit of a kludge?
         */
        memset(viewData, 0, sizeof(viewData));
        return;
    }

    if(resetNextViewer)
        resetNextViewer = 2;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        viewer_t sharpView;
        viewdata_t* vd = &viewData[i];
        player_t* plr = &ddPlayers[i];

        if(!(/*(plr->shared.flags & DDPF_LOCAL) &&*/ plr->shared.inGame))
            continue;

        R_GetSharpView(&sharpView, plr);

        // Update the camera angles that will be used when the camera is
        // not smoothed.
        vd->frozenAngle = sharpView.angle;
        vd->frozenPitch = sharpView.pitch;

        // The game tic has changed, which means we have an updated sharp
        // camera position.  However, the position is at the beginning of
        // the tic and we are most likely not at a sharp tic boundary, in
        // time.  We will move the viewer positions one step back in the
        // buffer.  The effect of this is that [0] is the previous sharp
        // position and [1] is the current one.

        memcpy(&vd->lastSharp[0], &vd->lastSharp[1], sizeof(viewer_t));
        memcpy(&vd->lastSharp[1], &sharpView, sizeof(sharpView));

        R_CheckViewerLimits(vd->lastSharp, &sharpView);
    }

    map = P_CurrentMap();
    if(map)
    {
        Map_UpdateWatchedPlanes(map);
        Map_UpdateMovingSurfaces(map);
    }
}

void R_CreateMobjLinks(map_t* map)
{
    uint i;

#ifdef DD_PROFILE
    static int p;

    if(++p > 40)
    {
        p = 0;
        PRINT_PROF( PROF_MOBJ_INIT_ADD );
    }
#endif

    if(!map)
        return;

BEGIN_PROF( PROF_MOBJ_INIT_ADD );

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t* sector = map->sectors[i];
        mobj_t* iter;

        for(iter = sector->mobjList; iter; iter = iter->sNext)
        {
            ObjBlockmap_Add(map->_objBlockmap, OT_MOBJ, iter);
        }
    }

END_PROF( PROF_MOBJ_INIT_ADD );
}

static int createObjLinksForParticles(void* ptr, void* context)
{
    generator_t* gen = (generator_t*) ptr;
    int p;

    if(!gen->thinker.function)
        return true; // Continue iteration.

    for(p = 0; p < gen->count; ++p)
    {
        particle_t* pt = &gen->ptcs[p];
        const ded_ptcstage_t* dst, *nextDst;
        const ptcstage_t* st;
        float size, invMark;

        if(!pt->gen)
            continue;

        // Lets avoid a lot of unnecessary work by prunning tiny particles early on.
        st = &gen->stages[pt->stage];
        dst = &gen->def->stages[pt->stage];
        if(pt->stage >= gen->def->stageCount.num - 1 ||
           !gen->stages[pt->stage + 1].type)
            nextDst = gen->def->stages + pt->stage; // There is no "next stage". Use the current one.
        else
            nextDst = gen->def->stages + (pt->stage + 1);

        invMark = pt->tics / (float) dst->tics;
        size = P_GetParticleRadius(dst, pt - gen->ptcs) * invMark +
            P_GetParticleRadius(nextDst, pt - gen->ptcs) * (1 - invMark);
        if(!(size > .0001f))
            continue; // Infinitely small.

        // @todo Generator should return the map its linked to.
        ObjBlockmap_Add(Map_ObjBlockmap(P_CurrentMap()), OT_PARTICLE, pt); // For spreading purposes.
    }

    return true; // Continue iteration.
}

void R_CreateParticleLinks(map_t* map)
{
#ifdef DD_PROFILE
    static int p;

    if(++p > 40)
    {
        p = 0;
        PRINT_PROF( PROF_PARTICLE_INIT_ADD );
    }
#endif

    if(!map)
        return;
    if(!useParticles)
        return;

BEGIN_PROF( PROF_PARTICLE_INIT_ADD );

    Map_IterateThinkers(map, (think_t) P_GeneratorThinker, ITF_PRIVATE,
                        createObjLinksForParticles, NULL);

END_PROF( PROF_PARTICLE_INIT_ADD );
}

/**
 * Prepare for rendering view(s) of the world.
 */
void R_BeginWorldFrame(void)
{
    map_t* map;

    if((map = P_CurrentMap()))
        Map_BeginFrame(map, resetNextViewer);
}

/**
 * Wrap up after drawing view(s) of the world.
 */
void R_EndWorldFrame(void)
{
    map_t* map;

    if((map = P_CurrentMap()))
        Map_EndFrame(map);
}

/**
 * Prepare rendering the view of the given player.
 */
void R_SetupFrame(player_t* player)
{
#define VIEWPOS_MAX_SMOOTHDISTANCE  172
#define MINEXTRALIGHTFRAMES         2

    int tableAngle;
    float yawRad, pitchRad;
    viewer_t sharpView, smoothView;
    viewdata_t* vd;

    // Reset the GL triangle counter.
    polyCounter = 0;

    viewPlayer = player;
    vd = &viewData[viewPlayer - ddPlayers];

    R_GetSharpView(&sharpView, viewPlayer);

    if(resetNextViewer ||
       V3_Distance(vd->current.pos, sharpView.pos) > VIEWPOS_MAX_SMOOTHDISTANCE)
    {
        // Keep reseting until a new sharp world has arrived.
        if(resetNextViewer > 1)
            resetNextViewer = 0;

        // Just view from the sharp position.
        R_CopyViewer(&vd->current, &sharpView);

        memcpy(&vd->lastSharp[0], &sharpView, sizeof(sharpView));
        memcpy(&vd->lastSharp[1], &sharpView, sizeof(sharpView));
    }
    // While the game is paused there is no need to calculate any
    // time offsets or interpolated camera positions.
    else //if(!clientPaused)
    {
        // Calculate the smoothed camera position, which is somewhere
        // between the previous and current sharp positions. This
        // introduces a slight delay (max. 1/35 sec) to the movement
        // of the smoothed camera.
        R_InterpolateViewer(vd->lastSharp, vd->lastSharp + 1, frameTimePos,
                            &smoothView);

        // Use the latest view angles known to us, if the interpolation flags
        // are not set. The interpolation flags are used when the view angles
        // are updated during the sharp tics and need to be smoothed out here.
        // For example, view locking (dead or camera setlock).
        if(!(player->shared.flags & DDPF_INTERYAW))
            smoothView.angle = sharpView.angle;
        if(!(player->shared.flags & DDPF_INTERPITCH))
            smoothView.pitch = sharpView.pitch;

        R_CopyViewer(&vd->current, &smoothView);

        // Monitor smoothness of yaw/pitch changes.
        if(showViewAngleDeltas)
        {
            typedef struct oldangle_s {
                double           time;
                float            yaw, pitch;
            } oldangle_t;

            static oldangle_t oldangle[DDMAXPLAYERS];
            oldangle_t* old = &oldangle[viewPlayer - ddPlayers];
            float yaw = (double)smoothView.angle / ANGLE_MAX * 360;

            Con_Message("(%i) F=%.3f dt=%-10.3f dx=%-10.3f dy=%-10.3f "
                        "Rdx=%-10.3f Rdy=%-10.3f\n",
                        SECONDS_TO_TICKS(gameTime),
                        frameTimePos,
                        sysTime - old->time,
                        yaw - old->yaw,
                        smoothView.pitch - old->pitch,
                        (yaw - old->yaw) / (sysTime - old->time),
                        (smoothView.pitch - old->pitch) / (sysTime - old->time));
            old->yaw = yaw;
            old->pitch = smoothView.pitch;
            old->time = sysTime;
        }

        // The Rdx and Rdy should stay constant when moving.
        if(showViewPosDeltas)
        {
            typedef struct oldpos_s {
                double           time;
                float            x, y, z;
            } oldpos_t;

            static oldpos_t oldpos[DDMAXPLAYERS];
            oldpos_t* old = &oldpos[viewPlayer - ddPlayers];

            Con_Message("(%i) F=%.3f dt=%-10.3f dx=%-10.3f dy=%-10.3f dz=%-10.3f\n",
                        //"Rdx=%-10.3f Rdy=%-10.3f\n",
                        SECONDS_TO_TICKS(gameTime),
                        frameTimePos,
                        sysTime - old->time,
                        smoothView.pos[0] - old->x,
                        smoothView.pos[1] - old->y,
                        smoothView.pos[2] - old->z /*,
                        smoothView.pos[0] - old->x / (sysTime - old->time),
                        smoothView.pos[1] - old->y / (sysTime - old->time)*/);
            old->x = smoothView.pos[VX];
            old->y = smoothView.pos[VY];
            old->z = smoothView.pos[VZ];
            old->time = sysTime;
        }
    }

    // Update viewer.
    tableAngle = viewData->current.angle >> ANGLETOFINESHIFT;
    viewData->viewSin = FIX2FLT(finesine[tableAngle]);
    viewData->viewCos = FIX2FLT(fineCosine[tableAngle]);

    // Calculate the front, up and side unit vectors.
    // The vectors are in the DGL coordinate system, which is a left-handed
    // one (same as in the game, but Y and Z have been swapped). Anyone
    // who uses these must note that it might be necessary to fix the aspect
    // ratio of the Y axis by dividing the Y coordinate by 1.2.
    yawRad = ((viewData->current.angle / (float) ANGLE_MAX) *2) * PI;

    pitchRad = viewData->current.pitch * 85 / 110.f / 180 * PI;

    // The front vector.
    viewData->frontVec[VX] = cos(yawRad) * cos(pitchRad);
    viewData->frontVec[VZ] = sin(yawRad) * cos(pitchRad);
    viewData->frontVec[VY] = sin(pitchRad);

    // The up vector.
    viewData->upVec[VX] = -cos(yawRad) * sin(pitchRad);
    viewData->upVec[VZ] = -sin(yawRad) * sin(pitchRad);
    viewData->upVec[VY] = cos(pitchRad);

    // The side vector is the cross product of the front and up vectors.
    M_CrossProduct(viewData->frontVec, viewData->upVec, viewData->sideVec);

    if(showFrameTimePos)
    {
        Con_Printf("frametime = %f\n", frameTimePos);
    }

    // Handle extralight (used to light up the world momentarily (used for
    // e.g. gun flashes). We want to avoid flickering, so when ever it is
    // enabled; make it last for a few frames.
    if(player->targetExtraLight != player->shared.extraLight)
    {
        player->targetExtraLight = player->shared.extraLight;
        player->extraLightCounter = MINEXTRALIGHTFRAMES;
    }

    if(player->extraLightCounter > 0)
    {
        player->extraLightCounter--;
        if(player->extraLightCounter == 0)
            player->extraLight = player->targetExtraLight;
    }
    extraLight = player->extraLight;
    extraLightDelta = extraLight / 16.0f;

    // Why?
    validCount++;

#undef MINEXTRALIGHTFRAMES
#undef VIEWPOS_MAX_SMOOTHDISTANCE
}

/**
 * Draw the border around the view window.
 */
void R_RenderPlayerViewBorder(void)
{
    R_DrawViewBorder();
}

/**
 * Set the GL viewport.
 */
void R_UseViewPort(viewport_t* port)
{
    if(!port)
    {
        glViewport(0, FLIP(0 + theWindow->height - 1), theWindow->width, theWindow->height);
    }
    else
    {
        currentPort = port;
        glViewport(port->x, FLIP(port->y + port->height - 1), port->width, port->height);
    }
}

/**
 * Render a blank view for the specified player.
 */
void R_RenderBlankView(void)
{
    UI_DrawDDBackground(0, 0, 320, 200, 1);
}

/**
 * Draw the view of the player inside the view window.
 */
void R_RenderPlayerView(int num)
{
    extern boolean firstFrameAfterLoad;
    extern int psp3d, modelTriCount;

    int oldFlags = 0;
    player_t* player;

    if(num < 0 || num >= DDMAXPLAYERS)
        return; // Huh?
    player = &ddPlayers[num];

    if(!player->shared.inGame || !player->shared.mo)
        return;

    if(firstFrameAfterLoad)
    {
        // Don't let the clock run yet.  There may be some texture
        // loading still left to do that we have been unable to
        // predetermine.
        firstFrameAfterLoad = false;
        DD_ResetTimer();
    }

    // Setup for rendering the frame.
    R_SetupFrame(player);
    if(!freezeRLs)
        R_ClearSprites();

    R_ProjectPlayerSprites(); // Only if 3D models exists for them.

    // Hide the viewPlayer's mobj?
    if(!(player->shared.flags & DDPF_CHASECAM))
    {
        oldFlags = player->shared.mo->ddFlags;
        player->shared.mo->ddFlags |= DDMF_DONTDRAW;
    }
    // Go to wireframe mode?
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // GL is in 3D transformation state only during the frame.
    GL_SwitchTo3DState(true, currentPort);

    Rend_RenderMap(P_CurrentMap());

    // Orthogonal projection to the view window.
    GL_Restore2DState(1);

    // Don't render in wireframe mode with 2D psprites.
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    Rend_Draw2DPlayerSprites(); // If the 2D versions are needed.
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Fullscreen viewport.
    GL_Restore2DState(2);
    // Do we need to render any 3D psprites?
    if(psp3d)
    {
        GL_SwitchTo3DState(false, currentPort);
        Rend_Draw3DPlayerSprites();
        GL_Restore2DState(2);   // Restore viewport.
    }
    // Original matrices and state: back to normal 2D.
    GL_Restore2DState(3);

    // Back from wireframe mode?
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Now we can show the viewPlayer's mobj again.
    if(!(player->shared.flags & DDPF_CHASECAM))
        player->shared.mo->ddFlags = oldFlags;

    // Should we be counting triangles?
    if(rendInfoTris)
    {
        // This count includes all triangles drawn since R_SetupFrame.
        Con_Printf("Tris: %-4i (Mdl=%-4i)\n", polyCounter, modelTriCount);
        modelTriCount = 0;
        polyCounter = 0;
    }

    if(rendInfoLums)
    {
        Con_Printf("LumObjs: %-4i\n", LO_GetNumLuminous());
    }

    // The colored filter.
    GL_DrawFilter();
}

/**
 * Render all view ports in the viewport grid.
 */
void R_RenderViewPorts(void)
{
    int oldDisplay = displayPlayer;
    int x, y, p;
    GLbitfield bits = GL_DEPTH_BUFFER_BIT;

    if(firstFrameAfterLoad || freezeRLs)
    {
        bits |= GL_COLOR_BUFFER_BIT;
    }
    else
    {
        int i;

        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t* plr = &ddPlayers[i];

            if(!plr->shared.inGame || !(plr->shared.flags & DDPF_LOCAL))
                continue;

            if(P_IsInVoid(plr))
            {
                bits |= GL_COLOR_BUFFER_BIT;
                break;
            }
        }
    }

    // This is all the clearing we'll do.
    glClear(bits);

    // Draw a view for all players with a visible viewport.
    for(p = 0, y = 0; y < gridRows; ++y)
        for(x = 0; x < gridCols; x++, ++p)
        {
            displayPlayer = viewports[p].console;
            R_UseViewPort(viewports + p);

            if(displayPlayer < 0)
            {
                R_RenderBlankView();
                continue;
            }

            // Draw in-window game graphics (layer 0).
            gx.G_Drawer(0);

            // Draw the view border.
            R_RenderPlayerViewBorder();

            // Draw in-window game graphics (layer 1).
            gx.G_Drawer(1);

            // Increment the internal frame count. This does not
            // affect the FPS counter.
            frameCount++;
        }

    // Restore things back to normal.
    displayPlayer = oldDisplay;
    R_UseViewPort(NULL);
}

/**
 * Applies the offset from the lightModRangeto the given light value.
 *
 * The lightModRange is used to implement (precalculated) ambient light
 * limit, light range compression and light range shift.
 *
 * \note There is no need to clamp the result. Since the offset values in
 *       the lightModRange have already been clamped so that the resultant
 *       lightvalue is NEVER outside the range 0-254 when the original
 *       lightvalue is used as the index.
 *
 * @param lightvar      Ptr to the value to apply the adaptation to.
 */
void R_ApplyLightAdaptation(float* lightvar)
{
    int lightval;

    if(lightvar == NULL)
        return; // Can't apply adaptation to a NULL val ptr...

    lightval = ROUND(255.0f * *lightvar);
    if(lightval > 254)
        lightval = 254;
    else if(lightval < 0)
        lightval = 0;

    *lightvar += lightModRange[lightval];
}

/**
 * Similar to R_ApplyLightAdaptation but instead of applying light
 * adaptation directly this method returns the delta.
 *
 * @param lightvalue    Light value to look up the adaptation delta for.
 * @return int          Adaptation delta for the passed light value.
 */
float R_LightAdaptationDelta(float lightvalue)
{
    int lightval;

    lightval = ROUND(255.0f * lightvalue);
    if(lightval > 254)
        lightval = 254;
    else if(lightval < 0)
        lightval = 0;

    return lightModRange[lightval];
}

D_CMD(ViewGrid)
{
    if(argc != 3)
    {
        Con_Printf("Usage: %s (cols) (rows)\n", argv[0]);
        return true;
    }

    // Recalculate viewports.
    R_SetViewGrid(strtol(argv[1], NULL, 0), strtol(argv[2], NULL, 0));
    return true;
}
