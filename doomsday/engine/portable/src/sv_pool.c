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
 * sv_pool.c: Delta Pools
 *
 * Delta Pools use PU_MAP, which means all the memory allocated for them
 * is deallocated when the map changes. Sv_InitPools() is called in
 * R_SetupMap() to clear out all the old data.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_play.h"

#include "s_main.h"
#include "sys_timer.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_DELTA_BASE_SCORE    10000

#define REG_MOBJ_HASH_SIZE          1024
#define REG_MOBJ_HASH_FUNCTION_MASK 0x3ff

// Maximum difference in plane height where the absolute height doesn't
// need to be sent.

#define PLANE_SKIP_LIMIT            (40)

// TYPES -------------------------------------------------------------------

typedef struct reg_mobj_s {
    // Links to next and prev mobj in the register hash.
    struct reg_mobj_s*  next, *prev;

    // The tic when the mobj state was last sent.
    int                 lastTimeStateSent;
    dt_mobj_t           mo; // The state of the mobj.
} reg_mobj_t;

typedef struct mobjhash_s {
    reg_mobj_t*         first, *last;
} mobjhash_t;

/**
 * One cregister_t holds the state of the entire world.
 */
typedef struct cregister_s {
    // The time the register was last updated.
    int                 gametic;

    // True if this register contains a read-only copy of the initial state
    // of the world.
    boolean             isInitial;

    // The mobjs are stored in a hash for efficiency (ID is the key).
    mobjhash_t          mobjs[REG_MOBJ_HASH_SIZE];

    dt_player_t         ddPlayers[DDMAXPLAYERS];
    dt_sector_t*        sectors;
    dt_side_t*          sideDefs;
    dt_poly_t*          polyObjs;
} cregister_t;

/**
 * Each entity (mobj, sector, side, etc.) has an origin the world.
 */
typedef struct origin_s {
    float               pos[2];
} origin_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void            Sv_RegisterWorld(gamemap_t* map, cregister_t* reg, boolean isInitial);
void            Sv_NewDelta(void* deltaPtr, deltatype_t type, uint id);
boolean         Sv_IsVoidDelta(const void* delta);
void            Sv_PoolQueueClear(pool_t* pool);
void            Sv_GenerateNewDeltas(cregister_t* reg, int clientNumber,
                                     boolean doUpdate);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The register contains the previous state of the world.
cregister_t worldRegister;

// The initial register is used when generating deltas for a new client.
cregister_t initialRegister;

// Each client has its own pool for deltas.
pool_t pools[DDMAXPLAYERS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static float deltaBaseScores[NUM_DELTA_TYPES];

// Keep this zeroed out. Used if the register doesn't have data for
// the mobj being compared.
static dt_mobj_t dummyZeroMobj;

// Origins of stuff. Calculated at the beginning of a map.
static origin_t* sectorOrigins, *sideOrigins;

// Pointer to the owner line for each side.
static linedef_t** sideOwners;

// CODE --------------------------------------------------------------------

/**
 * Called once for each map, from R_SetupMap(). Initialize the world
 * register and drain all pools.
 */
void Sv_InitPools(void)
{
    uint i, startTime;
    gamemap_t* map;

    // Clients don't register anything.
    if(isClient)
        return;

    startTime = Sys_GetRealTime();

    // Set base priority scores for all the delta types.
    for(i = 0; i < NUM_DELTA_TYPES; ++i)
    {
        deltaBaseScores[i] = DEFAULT_DELTA_BASE_SCORE;
    }

    // Priorities for all deltas that will be sent out by the server.
    // No priorities need to be declared for obsolete delta types.
    deltaBaseScores[DT_MOBJ] = 1000;
    deltaBaseScores[DT_PLAYER] = 1000;
    deltaBaseScores[DT_SECTOR] = 2000;
    deltaBaseScores[DT_SIDE] = 800;
    deltaBaseScores[DT_POLY] = 2000;
    deltaBaseScores[DT_LUMP] = 0;
    deltaBaseScores[DT_SOUND] = 2000;
    deltaBaseScores[DT_MOBJ_SOUND] = 3000;
    deltaBaseScores[DT_SECTOR_SOUND] = 5000;
    deltaBaseScores[DT_POLY_SOUND] = 5000;

    // Since the map has changed, PU_MAP memory has been freed.
    // Reset all pools (set numbers are kept, though).
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        pools[i].owner = i;
        pools[i].resendDealer = 1;
        memset(pools[i].hash, 0, sizeof(pools[i].hash));
        memset(pools[i].misHash, 0, sizeof(pools[i].misHash));
        pools[i].queueSize = 0;
        pools[i].allocatedSize = 0;
        pools[i].queue = NULL;

        // This will be set to false when a frame is sent.
        pools[i].isFirst = true;
    }

    map = DMU_CurrentMap();
    if(map)
    {
        // Find the owners of all sides.
        sideOwners = Z_Malloc(sizeof(linedef_t *) * map->numSideDefs, PU_MAP, 0);
        for(i = 0; i < map->numSideDefs; ++i)
        {
            sideOwners[i] = R_GetLineForSide(map->sideDefs[i]);
        }

        // Origins of sectors.
        sectorOrigins = Z_Malloc(sizeof(origin_t) * map->numSectors, PU_MAP, 0);
        for(i = 0; i < map->numSectors; ++i)
        {
            sector_t* sec = map->sectors[i];

            sectorOrigins[i].pos[VX] =
                (sec->bBox[BOXRIGHT] + sec->bBox[BOXLEFT]) / 2;
            sectorOrigins[i].pos[VY] =
                (sec->bBox[BOXBOTTOM] + sec->bBox[BOXTOP]) / 2;
        }

        // Origins of sides.
        sideOrigins = Z_Malloc(sizeof(origin_t) * map->numSideDefs, PU_MAP, 0);
        for(i = 0; i < map->numSideDefs; ++i)
        {
            vertex_t* vtx;

            // The side must be owned by a line.
            if(sideOwners[i] == NULL)
                continue;

            vtx = sideOwners[i]->L_v1;
            sideOrigins[i].pos[VX] =
                vtx->V_pos[VX] + sideOwners[i]->dX / 2;
            sideOrigins[i].pos[VY] =
                vtx->V_pos[VY] + sideOwners[i]->dY / 2;
        }

        // Store the current state of the world into both the registers.
        Sv_RegisterWorld(map, &worldRegister, false);
        Sv_RegisterWorld(map, &initialRegister, true);
    }

#ifdef _DEBUG
    // How much time did we spend?
    Con_Message("Sv_InitPools: World registered, done in %.2f seconds.\n",
                (Sys_GetRealTime() - startTime) / 1000.0f);
#endif
}

/**
 * Called during server shutdown (when shutting down the engine).
 */
void Sv_ShutdownPools(void)
{
}

/**
 * Called when a client joins the game.
 */
void Sv_InitPoolForClient(uint clientNumber)
{
    // Free everything that might exist in the pool.
    Sv_DrainPool(clientNumber);

    // Generate deltas by comparing against the initial state of the world.
    // The initial register remains unmodified.
    Sv_GenerateNewDeltas(&initialRegister, clientNumber, false);

    // No frames have yet been sent for this client.
    // The first frame is processed a bit more thoroughly than the others
    // (e.g. *all* sides are compared, not just a portion).
    pools[clientNumber].isFirst = true;
}

/**
 * @return              Pointer to the console's delta pool.
 */
pool_t* Sv_GetPool(uint consoleNumber)
{
    return &pools[consoleNumber];
}

/**
 * The hash function for the register mobj hash.
 */
uint Sv_RegisterHashFunction(thid_t id)
{
    return (uint) id & REG_MOBJ_HASH_FUNCTION_MASK;
}

/**
 * @return              Pointer to the register-mobj, if it already exists.
 */
reg_mobj_t*             Sv_RegisterFindMobj(cregister_t* reg, thid_t id)
{
    mobjhash_t*         hash = &reg->mobjs[Sv_RegisterHashFunction(id)];
    reg_mobj_t*         iter;

    // See if there already is a register-mobj for this id.
    for(iter = hash->first; iter; iter = iter->next)
    {
        // Is this the one?
        if(iter->mo.thinker.id == id)
        {
            return iter;
        }
    }

    return NULL;
}

/**
 * Adds a new reg_mobj_t to the register's mobj hash.
 */
reg_mobj_t* Sv_RegisterAddMobj(cregister_t* reg, thid_t id)
{
    mobjhash_t*         hash = &reg->mobjs[Sv_RegisterHashFunction(id)];
    reg_mobj_t*         newRegMo;

    // Try to find an existing register-mobj.
    if((newRegMo = Sv_RegisterFindMobj(reg, id)) != NULL)
    {
        return newRegMo;
    }

    // Allocate the new register-mobj.
    newRegMo = Z_Calloc(sizeof(reg_mobj_t), PU_MAP, 0);

    // Link it to the end of the hash list.
    if(hash->last)
    {
        hash->last->next = newRegMo;
        newRegMo->prev = hash->last;
    }
    hash->last = newRegMo;

    if(!hash->first)
    {
        hash->first = newRegMo;
    }

    return newRegMo;
}

/**
 * Removes a reg_mobj_t from the register's mobj hash.
 */
void Sv_RegisterRemoveMobj(cregister_t* reg, reg_mobj_t* regMo)
{
    mobjhash_t*         hash =
        &reg->mobjs[Sv_RegisterHashFunction(regMo->mo.thinker.id)];

    // Update the first and last links.
    if(hash->last == regMo)
    {
        hash->last = regMo->prev;
    }
    if(hash->first == regMo)
    {
        hash->first = regMo->next;
    }

    // Link out of the list.
    if(regMo->next)
    {
        regMo->next->prev = regMo->prev;
    }
    if(regMo->prev)
    {
        regMo->prev->next = regMo->next;
    }

    // Destroy the register-mobj.
    Z_Free(regMo);
}

/**
 * @return              If the mobj is on the floor; @c MININT.
 *                      If the mobj is touching the ceiling; @c MAXINT.
 *                      Otherwise returns the Z coordinate.
 */
float Sv_GetMaxedMobjZ(const mobj_t* mo)
{
    if(mo->pos[VZ] == mo->floorZ)
    {
        return DDMINFLOAT;
    }
    if(mo->pos[VZ] + mo->height == mo->ceilingZ)
    {
        return DDMAXFLOAT;
    }
    return mo->pos[VZ];
}

/**
 * Store the state of the mobj into the register-mobj.
 * Called at register init and after each delta generation cycle.
 */
void Sv_RegisterMobj(dt_mobj_t* reg, const mobj_t* mo)
{
    // (dt_mobj_t <=> mobj_t)
    // Just copy the data we need.
    reg->thinker.id = mo->thinker.id;
    reg->dPlayer = mo->dPlayer;
    reg->subsector = mo->subsector;
    reg->pos[VX] = mo->pos[VX];
    reg->pos[VY] = mo->pos[VY];
    reg->pos[VZ] = Sv_GetMaxedMobjZ(mo);
    reg->floorZ = mo->floorZ;
    reg->ceilingZ = mo->ceilingZ;
    reg->mom[MX] = mo->mom[MX];
    reg->mom[MY] = mo->mom[MY];
    reg->mom[MZ] = mo->mom[MZ];
    reg->angle = mo->angle;
    reg->selector = mo->selector;
    reg->state = mo->state;
    reg->radius = mo->radius;
    reg->height = mo->height;
    reg->ddFlags = mo->ddFlags;
    reg->floorClip = mo->floorClip;
    reg->translucency = mo->translucency;
    reg->visTarget = mo->visTarget;
}

/**
 * Reset the data of the registered mobj to reasonable defaults.
 * In effect, forces a resend of the zeroed entries as deltas.
 */
void Sv_RegisterResetMobj(dt_mobj_t* reg)
{
    reg->pos[VX] = 0;
    reg->pos[VY] = 0;
    reg->pos[VZ] = 0;
    reg->angle = 0;
    reg->selector = 0;
    reg->state = 0;
    reg->radius = 0;
    reg->height = 0;
    reg->ddFlags = 0;
    reg->floorClip = 0;
    reg->translucency = 0;
    reg->visTarget = 0;
}

/**
 * Store the state of the player into the register-player.
 * Called at register init and after each delta generation cycle.
 */
void Sv_RegisterPlayer(dt_player_t* reg, uint number)
{
#define FMAKERGBA(r,g,b,a) ( (byte)(0xff*r) + ((byte)(0xff*g)<<8) + ((byte)(0xff*b)<<16) + ((byte)(0xff*a)<<24) )

    player_t*           plr = &ddPlayers[number];
    ddplayer_t*         ddpl = &plr->shared;
    client_t*           c = &clients[number];

    reg->mobj = (ddpl->mo ? ddpl->mo->thinker.id : 0);
    reg->forwardMove = c->lastCmd->forwardMove;
    reg->sideMove = c->lastCmd->sideMove;
    reg->angle = (ddpl->mo ? ddpl->mo->angle : 0);
    reg->turnDelta = (ddpl->mo ? ddpl->mo->angle - ddpl->lastAngle : 0);
    reg->friction = ddpl->mo &&
        (gx.MobjFriction ? gx.MobjFriction(ddpl->mo) : DEFAULT_FRICTION);
    reg->extraLight = ddpl->extraLight;
    reg->fixedColorMap = ddpl->fixedColorMap;
    if(ddpl->flags & DDPF_VIEW_FILTER)
    {
        reg->filter = FMAKERGBA(ddpl->filterColor[CR],
            ddpl->filterColor[CG], ddpl->filterColor[CB],
            ddpl->filterColor[CA]);
    }
    else
    {
        reg->filter = 0;
    }
    reg->clYaw = (ddpl->mo ? ddpl->mo->angle : 0);
    reg->clPitch = ddpl->lookDir;
    memcpy(reg->psp, ddpl->pSprites, sizeof(ddpsprite_t) * 2);

#undef FMAKERGBA
}

/**
 * Store the state of the sector into the register-sector.
 * Called at register init and after each delta generation.
 *
 * @param reg           The sector register to be initialized.
 * @param number        The world sector number to be registered.
 */
void Sv_RegisterSector(gamemap_t* map, uint number, dt_sector_t* reg)
{
    uint i;
    sector_t* sec = map->sectors[number];

    reg->lightLevel = sec->lightLevel;
    memcpy(reg->rgb, sec->rgb, sizeof(reg->rgb));
    // \fixme $nplanes
    for(i = 0; i < 2; ++i) // number of planes in sector.
    {
        // Plane properties
        reg->planes[i].height = sec->planes[i]->height;
        reg->planes[i].target = sec->planes[i]->target;
        reg->planes[i].speed = sec->planes[i]->speed;
        reg->planes[i].glow = sec->planes[i]->glow;
        memcpy(reg->planes[i].glowRGB, sec->planes[i]->glowRGB,
               sizeof(reg->planes[i].glowRGB));

        // Surface properties.
        memcpy(reg->planes[i].surface.rgba, sec->planes[i]->surface.rgba,
               sizeof(reg->planes[i].surface.rgba));

        // Surface material.
        reg->planes[i].surface.material = sec->SP_planematerial(i);
    }
}

/**
 * Store the state of the side into the register-side.
 * Called at register init and after each delta generation.
 */
void Sv_RegisterSide(gamemap_t* map, uint number, dt_side_t* reg)
{
    sidedef_t* side = map->sideDefs[number];
    linedef_t* line = sideOwners[number];

    reg->top.material = side->SW_topmaterial;
    reg->middle.material = side->SW_middlematerial;
    reg->bottom.material = side->SW_bottommaterial;
    reg->lineFlags = (line ? line->flags & 0xff : 0);

    memcpy(reg->top.rgba, side->SW_toprgba, sizeof(reg->top.rgba));
    memcpy(reg->middle.rgba, side->SW_middlergba, sizeof(reg->middle.rgba));
    memcpy(reg->bottom.rgba, side->SW_bottomrgba, sizeof(reg->bottom.rgba));
    reg->middle.blendMode = side->SW_middleblendmode; // only middle supports blendmode.
    reg->flags = side->flags & 0xff;
}

/**
 * Store the state of the polyobj into the register-poly.
 * Called at register init and after each delta generation.
 */
void Sv_RegisterPoly(gamemap_t* map, uint number, dt_poly_t* reg)
{
    polyobj_t* poly = map->polyObjs[number];

    reg->dest.pos[VX] = poly->dest[VX];
    reg->dest.pos[VY] = poly->dest[VY];
    reg->speed = poly->speed;
    reg->destAngle = poly->destAngle;
    reg->angleSpeed = poly->angleSpeed;
}

/**
 * @return              @c true, if the result is not void.
 */
boolean Sv_RegisterCompareMobj(cregister_t* reg, const mobj_t* s,
                               mobjdelta_t* d)
{
    int df;
    reg_mobj_t* regMo = NULL;
    const dt_mobj_t* r = &dummyZeroMobj;

    if((regMo = Sv_RegisterFindMobj(reg, s->thinker.id)) != NULL)
    {
        // Use the registered data.
        r = &regMo->mo;
        df = 0;
    }
    else
    {
        // This didn't exist in the register, so it's a new mobj.
        df = MDFC_CREATE;
    }

    if(r->pos[VX] != s->pos[VX])
        df |= MDF_POS_X;
    if(r->pos[VY] != s->pos[VY])
        df |= MDF_POS_Y;
    if(r->pos[VZ] != Sv_GetMaxedMobjZ(s))
        df |= MDF_POS_Z;

    if(r->mom[MX] != s->mom[MX])
        df |= MDF_MOM_X;
    if(r->mom[MY] != s->mom[MY])
        df |= MDF_MOM_Y;
    if(r->mom[MZ] != s->mom[MZ])
        df |= MDF_MOM_Z;

    if(r->angle != s->angle)
        df |= MDF_ANGLE;
    if(r->selector != s->selector)
        df |= MDF_SELECTOR;
    if(r->translucency != s->translucency)
        df |= MDFC_TRANSLUCENCY;

    if(r->visTarget != s->visTarget)
        df |= MDFC_FADETARGET;

    // Mobj state sent periodically, if it keeps changing.
    if((!(s->ddFlags & DDMF_MISSILE) && regMo &&
        Sys_GetTime() - regMo->lastTimeStateSent > (60 + s->thinker.id%35) &&
        r->state != s->state) ||
       !Def_SameStateSequence(r->state, s->state))
    {
        df |= MDF_STATE;

#ifdef _DEBUG
VERBOSE2( if(regMo && Sys_GetTime() - regMo->lastTimeStateSent > (60 + s->thinker.id%35))
    Con_Message("Sv_RegisterCompareMobj: (%i) Sending state due to time.\n",
                s->thinker.id) );
#endif

        if(regMo) regMo->lastTimeStateSent = Sys_GetTime();
    }

    if(r->radius != s->radius)
        df |= MDF_RADIUS;
    if(r->height != s->height)
        df |= MDF_HEIGHT;
    if((r->ddFlags & DDMF_PACK_MASK) != (s->ddFlags & DDMF_PACK_MASK))
    {
        df |= MDF_FLAGS;
    }
    if(r->floorClip != s->floorClip)
        df |= MDF_FLOORCLIP;

    if(df)
    {
        // Init the delta with current data.
        Sv_NewDelta(d, DT_MOBJ, s->thinker.id);
        Sv_RegisterMobj(&d->mo, s);
    }

    d->delta.flags = df;

    return !Sv_IsVoidDelta(d);
}

/**
 * @return              @c true, if the result is not void.
 */
boolean Sv_RegisterComparePlayer(cregister_t* reg, uint number,
                                 playerdelta_t* d)
{
    const dt_player_t*  r = &reg->ddPlayers[number];
    dt_player_t*        s = &d->player;
    int                 df = 0;

    // Init the delta with current data.
    Sv_NewDelta(d, DT_PLAYER, number);
    Sv_RegisterPlayer(&d->player, number);

    // Determine which data is different.
    if(r->mobj != s->mobj)
        df |= PDF_MOBJ;
    if(r->forwardMove != s->forwardMove)
        df |= PDF_FORWARDMOVE;
    if(r->sideMove != s->sideMove)
        df |= PDF_SIDEMOVE;
    /*if(r->angle != s->angle)
        df |= PDF_ANGLE;*/
    if(r->turnDelta != s->turnDelta)
        df |= PDF_TURNDELTA;
    if(r->friction != s->friction)
        df |= PDF_FRICTION;
    if(r->extraLight != s->extraLight || r->fixedColorMap != s->fixedColorMap)
    {
        df |= PDF_EXTRALIGHT;
    }
    if(r->filter != s->filter)
        df |= PDF_FILTER;
/*    if(r->clYaw != s->clYaw)
        df |= PDF_CLYAW;
    if(r->clPitch != s->clPitch)
        df |= PDF_CLPITCH;*/

    /*
    // The player sprites are a bit more complicated to check.
    for(i = 0; i < 2; ++i)
    {
        int     off = 16 + i * 8;
        const ddpsprite_t *rps = r->psp + i;
        const ddpsprite_t *sps = s->psp + i;

        if(rps->statePtr != sps->statePtr)
            df |= PSDF_STATEPTR << off;

        if(rps->light != sps->light)
            df |= PSDF_LIGHT << off;
        if(rps->alpha != sps->alpha)
            df |= PSDF_ALPHA << off;
        if(rps->state != sps->state)
            df |= PSDF_STATE << off;
        if((rps->offX != sps->offX || rps->offY != sps->offY) && !i)
        {
            df |= PSDF_OFFSET << off;
        }
    }
    // Check for any psprite flags.
    if(df & 0xffff0000)
        df |= PDF_PSPRITES;
    */

    d->delta.flags = df;
    return !Sv_IsVoidDelta(d);
}

/**
 * @return              @c true, if the result is not void.
 */
boolean Sv_RegisterCompareSector(gamemap_t* map, uint number, cregister_t* reg,
                                 sectordelta_t* d, byte doUpdate)
{
    dt_sector_t* r = &reg->sectors[number];
    const sector_t* s = map->sectors[number];
    int df = 0;

    // Determine which data is different.
    if(s->SP_floormaterial != r->planes[PLN_FLOOR].surface.material)
        df |= SDF_FLOOR_MATERIAL;
    if(s->SP_ceilmaterial != r->planes[PLN_CEILING].surface.material)
       df |= SDF_CEILING_MATERIAL;
    if(r->lightLevel != s->lightLevel)
        df |= SDF_LIGHT;
    if(r->rgb[0] != s->rgb[0])
        df |= SDF_COLOR_RED;
    if(r->rgb[1] != s->rgb[1])
        df |= SDF_COLOR_GREEN;
    if(r->rgb[2] != s->rgb[2])
        df |= SDF_COLOR_BLUE;

    if(r->planes[PLN_FLOOR].surface.rgba[0] != s->SP_floorrgb[0])
        df |= SDF_FLOOR_COLOR_RED;
    if(r->planes[PLN_FLOOR].surface.rgba[1] != s->SP_floorrgb[1])
        df |= SDF_FLOOR_COLOR_GREEN;
    if(r->planes[PLN_FLOOR].surface.rgba[2] != s->SP_floorrgb[2])
        df |= SDF_FLOOR_COLOR_BLUE;

    if(r->planes[PLN_CEILING].surface.rgba[0] != s->SP_ceilrgb[0])
        df |= SDF_CEIL_COLOR_RED;
    if(r->planes[PLN_CEILING].surface.rgba[1] != s->SP_ceilrgb[1])
        df |= SDF_CEIL_COLOR_GREEN;
    if(r->planes[PLN_CEILING].surface.rgba[2] != s->SP_ceilrgb[2])
        df |= SDF_CEIL_COLOR_BLUE;

    if(r->planes[PLN_FLOOR].glowRGB[0] != s->SP_floorglowrgb[0])
        df |= SDF_FLOOR_GLOW_RED;
    if(r->planes[PLN_FLOOR].glowRGB[1] != s->SP_floorglowrgb[1])
        df |= SDF_FLOOR_GLOW_GREEN;
    if(r->planes[PLN_FLOOR].glowRGB[2] != s->SP_floorglowrgb[2])
        df |= SDF_FLOOR_GLOW_BLUE;

    if(r->planes[PLN_CEILING].glowRGB[0] != s->SP_ceilglowrgb[0])
        df |= SDF_CEIL_GLOW_RED;
    if(r->planes[PLN_CEILING].glowRGB[1] != s->SP_ceilglowrgb[1])
        df |= SDF_CEIL_GLOW_GREEN;
    if(r->planes[PLN_CEILING].glowRGB[2] != s->SP_ceilglowrgb[2])
        df |= SDF_CEIL_GLOW_BLUE;

    if(r->planes[PLN_FLOOR].glow != s->planes[PLN_FLOOR]->glow)
        df |= SDF_FLOOR_GLOW;
    if(r->planes[PLN_CEILING].glow != s->planes[PLN_CEILING]->glow)
        df |= SDF_CEIL_GLOW;

    // The cases where an immediate change to a plane's height is needed:
    // 1) Plane is not moving, but the heights are different. This means
    //    the plane's height was changed unpredictably.
    // 2) Plane is moving, but there is a large difference in the heights.
    //    The clientside height should be fixed.

    // Should we make an immediate change in floor height?
    if(!r->planes[PLN_FLOOR].speed && !s->planes[PLN_FLOOR]->speed)
    {
        if(r->planes[PLN_FLOOR].height != s->planes[PLN_FLOOR]->height)
            df |= SDF_FLOOR_HEIGHT;
    }
    else
    {
        if(fabs(r->planes[PLN_FLOOR].height - s->planes[PLN_FLOOR]->height)
           > PLANE_SKIP_LIMIT)
            df |= SDF_FLOOR_HEIGHT;
    }

    // How about the ceiling?
    if(!r->planes[PLN_CEILING].speed && !s->planes[PLN_CEILING]->speed)
    {
        if(r->planes[PLN_CEILING].height != s->planes[PLN_CEILING]->height)
            df |= SDF_CEILING_HEIGHT;
    }
    else
    {
        if(fabs(r->planes[PLN_CEILING].height - s->planes[PLN_CEILING]->height)
           > PLANE_SKIP_LIMIT)
            df |= SDF_CEILING_HEIGHT;
    }

    // Check planes, too.
    if(r->planes[PLN_FLOOR].target != s->planes[PLN_FLOOR]->target)
    {
        // Target and speed are always sent together.
        df |= SDF_FLOOR_TARGET | SDF_FLOOR_SPEED;
    }
    if(r->planes[PLN_FLOOR].speed != s->planes[PLN_FLOOR]->speed)
    {
        // Target and speed are always sent together.
        df |= SDF_FLOOR_SPEED | SDF_FLOOR_TARGET;
    }
    if(r->planes[PLN_CEILING].target != s->planes[PLN_CEILING]->target)
    {
        // Target and speed are always sent together.
        df |= SDF_CEILING_TARGET | SDF_CEILING_SPEED;
    }
    if(r->planes[PLN_CEILING].speed != s->planes[PLN_CEILING]->speed)
    {
        // Target and speed are always sent together.
        df |= SDF_CEILING_SPEED | SDF_CEILING_TARGET;
    }

    // Only do a delta when something changes.
    if(df)
    {
        // Init the delta with current data.
        Sv_NewDelta(d, DT_SECTOR, number);
        Sv_RegisterSector(map, number, &d->sector);

        if(doUpdate)
        {
            Sv_RegisterSector(map, number, r);
        }
    }

    if(doUpdate)
    {
        // The plane heights should be tracked regardless of the
        // change flags.
        r->planes[PLN_FLOOR].height = s->planes[PLN_FLOOR]->height;
        r->planes[PLN_CEILING].height = s->planes[PLN_CEILING]->height;
    }

    d->delta.flags = df;
    return !Sv_IsVoidDelta(d);
}

/**
 * @eturn               @c true, if the result is not void.
 */
boolean Sv_RegisterCompareSide(gamemap_t* map, uint number, cregister_t* reg,
                               sidedelta_t* d, byte doUpdate)
{
    const sidedef_t* s = map->sideDefs[number];
    const linedef_t* line = sideOwners[number];
    dt_side_t* r = &reg->sideDefs[number];
    int df = 0;
    byte lineFlags = (line ? line->flags & 0xff : 0);
    byte sideFlags = s->flags & 0xff;

    if(r->top.material != s->SW_topmaterial &&
       !(s->SW_topinflags & SUIF_MATERIAL_FIX))
    {
        df |= SIDF_TOP_MATERIAL;
        if(doUpdate)
            r->top.material = s->SW_topmaterial;
    }

    if(r->middle.material != s->SW_middlematerial &&
       !(s->SW_middleinflags & SUIF_MATERIAL_FIX))
    {
        df |= SIDF_MID_MATERIAL;
        if(doUpdate)
            r->middle.material = s->SW_middlematerial;
    }

    if(r->bottom.material != s->SW_bottommaterial &&
       !(s->SW_bottominflags & SUIF_MATERIAL_FIX))
    {
        df |= SIDF_BOTTOM_MATERIAL;
        if(doUpdate)
            r->bottom.material = s->SW_bottommaterial;
    }

    if(r->lineFlags != lineFlags)
    {
        df |= SIDF_LINE_FLAGS;
        if(doUpdate)
            r->lineFlags = lineFlags;
    }

    if(r->top.rgba[0] != s->SW_toprgba[0])
    {
        df |= SIDF_TOP_COLOR_RED;
        if(doUpdate)
            r->top.rgba[0] = s->SW_toprgba[0];
    }

    if(r->top.rgba[1] != s->SW_toprgba[1])
    {
        df |= SIDF_TOP_COLOR_GREEN;
        if(doUpdate)
            r->top.rgba[1] = s->SW_toprgba[1];
    }

    if(r->top.rgba[2] != s->SW_toprgba[2])
    {
        df |= SIDF_TOP_COLOR_BLUE;
        if(doUpdate)
            r->top.rgba[3] = s->SW_toprgba[3];
    }

    if(r->middle.rgba[0] != s->SW_middlergba[0])
    {
        df |= SIDF_MID_COLOR_RED;
        if(doUpdate)
            r->middle.rgba[0] = s->SW_middlergba[0];
    }

    if(r->middle.rgba[1] != s->SW_middlergba[1])
    {
        df |= SIDF_MID_COLOR_GREEN;
        if(doUpdate)
            r->middle.rgba[1] = s->SW_middlergba[1];
    }

    if(r->middle.rgba[2] != s->SW_middlergba[2])
    {
        df |= SIDF_MID_COLOR_BLUE;
        if(doUpdate)
            r->middle.rgba[3] = s->SW_middlergba[3];
    }

    if(r->middle.rgba[3] != s->SW_middlergba[3])
    {
        df |= SIDF_MID_COLOR_ALPHA;
        if(doUpdate)
            r->middle.rgba[3] = s->SW_middlergba[3];
    }

    if(r->bottom.rgba[0] != s->SW_bottomrgba[0])
    {
        df |= SIDF_BOTTOM_COLOR_RED;
        if(doUpdate)
            r->bottom.rgba[0] = s->SW_bottomrgba[0];
    }

    if(r->bottom.rgba[1] != s->SW_bottomrgba[1])
    {
        df |= SIDF_BOTTOM_COLOR_GREEN;
        if(doUpdate)
            r->bottom.rgba[1] = s->SW_bottomrgba[1];
    }

    if(r->bottom.rgba[2] != s->SW_bottomrgba[2])
    {
        df |= SIDF_BOTTOM_COLOR_BLUE;
        if(doUpdate)
            r->bottom.rgba[3] = s->SW_bottomrgba[3];
    }

    if(r->middle.blendMode != s->SW_middleblendmode)
    {
        df |= SIDF_MID_BLENDMODE;
        if(doUpdate)
            r->middle.blendMode = s->SW_middleblendmode;
    }

    if(r->flags != sideFlags)
    {
        df |= SIDF_FLAGS;
        if(doUpdate)
            r->flags = sideFlags;
    }

    // Was there any change?
    if(df)
    {
        // This happens quite rarely.
        // Init the delta with current data.
        Sv_NewDelta(d, DT_SIDE, number);
        Sv_RegisterSide(map, number, &d->side);
    }

    d->delta.flags = df;
    return !Sv_IsVoidDelta(d);
}

/**
 * @return              @c true, if the result is not void.
 */
boolean Sv_RegisterComparePoly(gamemap_t* map, uint number, cregister_t* reg,
                               polydelta_t *d)
{
    const dt_poly_t* r = &reg->polyObjs[number];
    dt_poly_t* s = &d->po;
    int df = 0;

    // Init the delta with current data.
    Sv_NewDelta(d, DT_POLY, number);
    Sv_RegisterPoly(map, number, &d->po);

    // What is different?
    if(r->dest.pos[VX] != s->dest.pos[VX])
        df |= PODF_DEST_X;
    if(r->dest.pos[VY] != s->dest.pos[VY])
        df |= PODF_DEST_Y;
    if(r->speed != s->speed)
        df |= PODF_SPEED;
    if(r->destAngle != s->destAngle)
        df |= PODF_DEST_ANGLE;
    if(r->angleSpeed != s->angleSpeed)
        df |= PODF_ANGSPEED;

    d->delta.flags = df;
    return !Sv_IsVoidDelta(d);
}

/**
 * @return              @c true, if the mobj can be excluded from delta
 *                      processing.
 */
boolean Sv_IsMobjIgnored(mobj_t* mo)
{
    return (mo->ddFlags & DDMF_LOCAL) != 0;
}

/**
 * @return              @c true, if the player can be excluded from delta
 *                      processing.
 */
boolean Sv_IsPlayerIgnored(int plrNum)
{
    return !ddPlayers[plrNum].shared.inGame;
}

/**
 * Initialize the register with the current state of the world.
 * The arrays are allocated and the data is copied, nothing else is done.
 *
 * An initial register doesn't contain any mobjs. When new clients enter,
 * they know nothing about any mobjs. If the mobjs were included in the
 * initial register, clients wouldn't receive much info from mobjs that
 * haven't moved since the beginning.
 */
void Sv_RegisterWorld(gamemap_t* map, cregister_t* reg, boolean isInitial)
{
    uint i;

    memset(reg, 0, sizeof(*reg));
    reg->gametic = SECONDS_TO_TICKS(gameTime);

    // Is this the initial state?
    reg->isInitial = isInitial;

    // Init sectors.
    reg->sectors = Z_Calloc(sizeof(dt_sector_t) * map->numSectors, PU_MAP, 0);
    for(i = 0; i < map->numSectors; ++i)
    {
        Sv_RegisterSector(map, i, &reg->sectors[i]);
    }

    // Init sides.
    reg->sideDefs = Z_Calloc(sizeof(dt_side_t) * map->numSideDefs, PU_MAP, 0);
    for(i = 0; i < map->numSideDefs; ++i)
    {
        Sv_RegisterSide(map, i, &reg->sideDefs[i]);
    }

    // Init polyobjs.
    reg->polyObjs = (map->numPolyObjs ?
         Z_Calloc(sizeof(dt_poly_t) * map->numPolyObjs, PU_MAP, 0) : NULL);
    for(i = 0; i < map->numPolyObjs; ++i)
    {
        Sv_RegisterPoly(map, i, &reg->polyObjs[i]);
    }
}

/**
 * Update the pool owner's info.
 */
void Sv_UpdateOwnerInfo(pool_t* pool)
{
    player_t* plr = &ddPlayers[pool->owner];
    ownerinfo_t* info = &pool->ownerInfo;

    memset(info, 0, sizeof(*info));

    // Pointer to the owner's pool.
    info->pool = pool;

    if(plr->shared.mo)
    {
        mobj_t* mo = plr->shared.mo;

        info->pos[VX] = mo->pos[VX];
        info->pos[VY] = mo->pos[VY];
        info->pos[VZ] = mo->pos[VZ];
        info->angle = mo->angle;
        info->speed = P_ApproxDistance(mo->mom[MX], mo->mom[MY]);
    }

    // The acknowledgement threshold is a multiple of the average
    // ack time of the client. If an unacked delta is not acked within
    // the threshold, it'll be re-included in the ratings.
    info->ackThreshold = Net_GetAckThreshold(pool->owner);
}

/**
 * @return              A timestamp that is used to track how old deltas are.
 */
uint Sv_GetTimeStamp(void)
{
    return Sys_GetRealTime();
}

/**
 * Initialize a new delta.
 */
void Sv_NewDelta(void* deltaPtr, deltatype_t type, uint id)
{
    delta_t*            delta = deltaPtr;

    // NOTE: This only clears the common delta_t part, not the extra data.
    memset(delta, 0, sizeof(*delta));

    delta->id = id;
    delta->type = type;
    delta->state = DELTA_NEW;
    delta->timeStamp = Sv_GetTimeStamp();
}

/**
 * @return              @c true, if the delta contains no information.
 */
boolean Sv_IsVoidDelta(const void* delta)
{
    return ((const delta_t *) delta)->flags == 0;
}

/**
 * @return              @c true, if the delta is a Sound delta.
 */
boolean Sv_IsSoundDelta(const void* delta)
{
    const delta_t*      d = delta;

    return (d->type == DT_SOUND || d->type == DT_MOBJ_SOUND ||
            d->type == DT_SECTOR_SOUND || d->type == DT_POLY_SOUND);
}

/**
 * @return              @c true, if the delta is a Start Sound delta.
 */
boolean Sv_IsStartSoundDelta(const void* delta)
{
    const sounddelta_t* d = delta;

    return Sv_IsSoundDelta(delta) &&
        ((d->delta.flags & SNDDF_VOLUME) && d->volume > 0);
}

/**
 * @return              @c true, if the delta is Stop Sound delta.
 */
boolean Sv_IsStopSoundDelta(const void* delta)
{
    const sounddelta_t* d = delta;

    return Sv_IsSoundDelta(delta) &&
        ((d->delta.flags & SNDDF_VOLUME) && d->volume <= 0);
}

/**
 * @return              @c true, if the delta is a Null Mobj delta.
 */
boolean Sv_IsNullMobjDelta(const void *delta)
{
    return ((const delta_t *) delta)->type == DT_MOBJ &&
        (((const delta_t *) delta)->flags & MDFC_NULL);
}

/**
 * @return              @c true, if the delta is a Create Mobj delta.
 */
boolean Sv_IsCreateMobjDelta(const void* delta)
{
    return ((const delta_t *) delta)->type == DT_MOBJ &&
        (((const delta_t *) delta)->flags & MDFC_CREATE);
}

/**
 * @return              @c true, if the deltas refer to the same object.
 */
boolean Sv_IsSameDelta(const void* delta1, const void* delta2)
{
    const delta_t*      a = delta1, *b = delta2;

    return (a->type == b->type) && (a->id == b->id);
}

/**
 * Makes a copy of the delta.
 */
void* Sv_CopyDelta(void* deltaPtr)
{
    void*               newDelta;
    delta_t*            delta = deltaPtr;
    size_t              size =
        (delta->type == DT_MOBJ ? sizeof(mobjdelta_t) : delta->type ==
         DT_PLAYER ? sizeof(playerdelta_t) : delta->type ==
         DT_SECTOR ? sizeof(sectordelta_t) : delta->type ==
         DT_SIDE ? sizeof(sidedelta_t) : delta->type ==
         DT_POLY ? sizeof(polydelta_t) : delta->type ==
         DT_SOUND ? sizeof(sounddelta_t) : delta->type ==
         DT_MOBJ_SOUND ? sizeof(sounddelta_t) : delta->type ==
         DT_SECTOR_SOUND ? sizeof(sounddelta_t) : delta->type ==
         DT_POLY_SOUND ? sizeof(sounddelta_t)
         /* : delta->type == DT_LUMP?   sizeof(lumpdelta_t) */
         : 0);

    if(size == 0)
    {
        Con_Error("Sv_CopyDelta: Unknown delta type %i.\n", delta->type);
    }

    newDelta = Z_Malloc(size, PU_MAP, 0);
    memcpy(newDelta, deltaPtr, size);
    return newDelta;
}

/**
 * Subtracts the contents of the second delta from the first delta.
 * Subtracting means that if a given flag is defined for both 1 and 2,
 * the flag for 1 is cleared (2 overrides 1). The result is that the
 * deltas can be applied in any order and the result is still correct.
 *
 * 1 and 2 must refer to the same entity!
 */
void Sv_SubtractDelta(void* deltaPtr1, const void* deltaPtr2)
{
    delta_t*            delta = deltaPtr1;
    const delta_t*      sub = deltaPtr2;

#ifdef _DEBUG
    if(!Sv_IsSameDelta(delta, sub))
    {
        Con_Error("Sv_SubtractDelta: Not the same!\n");
    }
#endif

    if(Sv_IsNullMobjDelta(sub))
    {
        // Null deltas kill everything.
        delta->flags = 0;
    }
    else
    {
        // Clear the common flags.
        delta->flags &= ~(delta->flags & sub->flags);
    }
}

/**
 * Applies the data in the source delta to the destination delta.
 * Both must be in the NEW state. Handles all types of deltas.
 */
void Sv_ApplyDeltaData(void* destDelta, const void* srcDelta)
{
    const delta_t* src = srcDelta;
    delta_t* dest = destDelta;
    int sf = src->flags;

    if(src->type == DT_MOBJ)
    {
        const dt_mobj_t* s = &((const mobjdelta_t *) src)->mo;
        dt_mobj_t* d = &((mobjdelta_t *) dest)->mo;

        // *Always* set the player pointer.
        d->dPlayer = s->dPlayer;

        if(sf & (MDF_POS_X | MDF_POS_Y))
            d->subsector = s->subsector;
        if(sf & MDF_POS_X)
            d->pos[VX] = s->pos[VX];
        if(sf & MDF_POS_Y)
            d->pos[VY] = s->pos[VY];
        if(sf & MDF_POS_Z)
            d->pos[VZ] = s->pos[VZ];
        if(sf & MDF_MOM_X)
            d->mom[MX] = s->mom[MX];
        if(sf & MDF_MOM_Y)
            d->mom[MY] = s->mom[MY];
        if(sf & MDF_MOM_Z)
            d->mom[MZ] = s->mom[MZ];
        if(sf & MDF_ANGLE)
            d->angle = s->angle;
        if(sf & MDF_SELECTOR)
            d->selector = s->selector;
        if(sf & MDF_STATE)
        {
            d->state = s->state;
            d->tics = (s->state ? s->state->tics : 0);
        }
        if(sf & MDF_RADIUS)
            d->radius = s->radius;
        if(sf & MDF_HEIGHT)
            d->height = s->height;
        if(sf & MDF_FLAGS)
            d->ddFlags = s->ddFlags;
        if(sf & MDF_FLOORCLIP)
            d->floorClip = s->floorClip;
        if(sf & MDFC_TRANSLUCENCY)
            d->translucency = s->translucency;
        if(sf & MDFC_FADETARGET)
            d->visTarget = s->visTarget;
    }
    else if(src->type == DT_PLAYER)
    {
        const dt_player_t*  s = &((const playerdelta_t *) src)->player;
        dt_player_t*        d = &((playerdelta_t *) dest)->player;

        if(sf & PDF_MOBJ)
            d->mobj = s->mobj;
        if(sf & PDF_FORWARDMOVE)
            d->forwardMove = s->forwardMove;
        if(sf & PDF_SIDEMOVE)
            d->sideMove = s->sideMove;
        /*if(sf & PDF_ANGLE)
            d->angle = s->angle;*/
        if(sf & PDF_TURNDELTA)
            d->turnDelta = s->turnDelta;
        if(sf & PDF_FRICTION)
            d->friction = s->friction;
        if(sf & PDF_EXTRALIGHT)
        {
            d->extraLight = s->extraLight;
            d->fixedColorMap = s->fixedColorMap;
        }
        if(sf & PDF_FILTER)
            d->filter = s->filter;
/*        if(sf & PDF_CLYAW)
            d->clYaw = s->clYaw;
        if(sf & PDF_CLPITCH)
            d->clPitch = s->clPitch; */ /* $unifiedangles */
        if(sf & PDF_PSPRITES)
        {
            uint                i;

            for(i = 0; i < 2; ++i)
            {
                int                 off = 16 + i * 8;

                if(sf & (PSDF_STATEPTR << off))
                {
                    d->psp[i].statePtr = s->psp[i].statePtr;
                    d->psp[i].tics =
                        (s->psp[i].statePtr ? s->psp[i].statePtr->tics : 0);
                }
                if(sf & (PSDF_LIGHT << off))
                    d->psp[i].light = s->psp[i].light;
                if(sf & (PSDF_ALPHA << off))
                    d->psp[i].alpha = s->psp[i].alpha;
                if(sf & (PSDF_STATE << off))
                    d->psp[i].state = s->psp[i].state;
                if(sf & (PSDF_OFFSET << off))
                {
                    d->psp[i].offset[VX] = s->psp[i].offset[VX];
                    d->psp[i].offset[VY] = s->psp[i].offset[VY];
                }
            }
        }
    }
    else if(src->type == DT_SECTOR)
    {
        const dt_sector_t*  s = &((const sectordelta_t *) src)->sector;
        dt_sector_t*        d = &((sectordelta_t *) dest)->sector;

        if(sf & SDF_FLOOR_MATERIAL)
            d->planes[PLN_FLOOR].surface.material =
                s->planes[PLN_FLOOR].surface.material;
        if(sf & SDF_CEILING_MATERIAL)
            d->planes[PLN_CEILING].surface.material =
                s->planes[PLN_CEILING].surface.material;
        if(sf & SDF_LIGHT)
            d->lightLevel = s->lightLevel;
        if(sf & SDF_FLOOR_TARGET)
            d->planes[PLN_FLOOR].target = s->planes[PLN_FLOOR].target;
        if(sf & SDF_FLOOR_SPEED)
            d->planes[PLN_FLOOR].speed = s->planes[PLN_FLOOR].speed;
        if(sf & SDF_CEILING_TARGET)
            d->planes[PLN_CEILING].target = s->planes[PLN_CEILING].target;
        if(sf & SDF_CEILING_SPEED)
            d->planes[PLN_CEILING].speed = s->planes[PLN_CEILING].speed;
        if(sf & SDF_FLOOR_HEIGHT)
            d->planes[PLN_FLOOR].height = s->planes[PLN_FLOOR].height;
        if(sf & SDF_CEILING_HEIGHT)
            d->planes[PLN_CEILING].height = s->planes[PLN_CEILING].height;
        if(sf & SDF_COLOR_RED)
            d->rgb[0] = s->rgb[0];
        if(sf & SDF_COLOR_GREEN)
            d->rgb[1] = s->rgb[1];
        if(sf & SDF_COLOR_BLUE)
            d->rgb[2] = s->rgb[2];

        if(sf & SDF_FLOOR_COLOR_RED)
            d->planes[PLN_FLOOR].surface.rgba[0] =
                s->planes[PLN_FLOOR].surface.rgba[0];
        if(sf & SDF_FLOOR_COLOR_GREEN)
            d->planes[PLN_FLOOR].surface.rgba[1] =
                s->planes[PLN_FLOOR].surface.rgba[1];
        if(sf & SDF_FLOOR_COLOR_BLUE)
            d->planes[PLN_FLOOR].surface.rgba[2] =
                s->planes[PLN_FLOOR].surface.rgba[2];

        if(sf & SDF_CEIL_COLOR_RED)
            d->planes[PLN_CEILING].surface.rgba[0] =
                s->planes[PLN_CEILING].surface.rgba[0];
        if(sf & SDF_CEIL_COLOR_GREEN)
            d->planes[PLN_CEILING].surface.rgba[1] =
                s->planes[PLN_CEILING].surface.rgba[1];
        if(sf & SDF_CEIL_COLOR_BLUE)
            d->planes[PLN_CEILING].surface.rgba[2] =
                s->planes[PLN_CEILING].surface.rgba[2];

        if(sf & SDF_FLOOR_GLOW_RED)
            d->planes[PLN_FLOOR].glowRGB[0] = s->planes[PLN_FLOOR].glowRGB[0];
        if(sf & SDF_FLOOR_GLOW_GREEN)
            d->planes[PLN_FLOOR].glowRGB[1] = s->planes[PLN_FLOOR].glowRGB[1];
        if(sf & SDF_FLOOR_GLOW_BLUE)
            d->planes[PLN_FLOOR].glowRGB[2] = s->planes[PLN_FLOOR].glowRGB[2];

        if(sf & SDF_CEIL_GLOW_RED)
            d->planes[PLN_CEILING].glowRGB[0] = s->planes[PLN_CEILING].glowRGB[0];
        if(sf & SDF_CEIL_GLOW_GREEN)
            d->planes[PLN_CEILING].glowRGB[1] = s->planes[PLN_CEILING].glowRGB[1];
        if(sf & SDF_CEIL_GLOW_BLUE)
            d->planes[PLN_CEILING].glowRGB[2] = s->planes[PLN_CEILING].glowRGB[2];

        if(sf & SDF_FLOOR_GLOW)
            d->planes[PLN_FLOOR].glow = s->planes[PLN_FLOOR].glow;
        if(sf & SDF_CEIL_GLOW)
            d->planes[PLN_CEILING].glow = s->planes[PLN_CEILING].glow;
    }
    else if(src->type == DT_SIDE)
    {
        const dt_side_t*    s = &((const sidedelta_t *) src)->side;
        dt_side_t*          d = &((sidedelta_t *) dest)->side;

        if(sf & SIDF_TOP_MATERIAL)
            d->top.material = s->top.material;
        if(sf & SIDF_MID_MATERIAL)
            d->middle.material = s->middle.material;
        if(sf & SIDF_BOTTOM_MATERIAL)
            d->bottom.material = s->bottom.material;
        if(sf & SIDF_LINE_FLAGS)
            d->lineFlags = s->lineFlags;

        if(sf & SIDF_TOP_COLOR_RED)
            d->top.rgba[0] = s->top.rgba[0];
        if(sf & SIDF_TOP_COLOR_GREEN)
            d->top.rgba[1] = s->top.rgba[1];
        if(sf & SIDF_TOP_COLOR_BLUE)
            d->top.rgba[2] = s->top.rgba[2];

        if(sf & SIDF_MID_COLOR_RED)
            d->middle.rgba[0] = s->middle.rgba[0];
        if(sf & SIDF_MID_COLOR_GREEN)
            d->middle.rgba[1] = s->middle.rgba[1];
        if(sf & SIDF_MID_COLOR_BLUE)
            d->middle.rgba[2] = s->middle.rgba[2];
        if(sf & SIDF_MID_COLOR_ALPHA)
            d->middle.rgba[3] = s->middle.rgba[3];

        if(sf & SIDF_BOTTOM_COLOR_RED)
            d->bottom.rgba[0] = s->bottom.rgba[0];
        if(sf & SIDF_BOTTOM_COLOR_GREEN)
            d->bottom.rgba[1] = s->bottom.rgba[1];
        if(sf & SIDF_BOTTOM_COLOR_BLUE)
            d->bottom.rgba[2] = s->bottom.rgba[2];

        if(sf & SIDF_MID_BLENDMODE)
            d->middle.blendMode = s->middle.blendMode;

        if(sf & SIDF_FLAGS)
            d->flags = s->flags;
    }
    else if(src->type == DT_POLY)
    {
        const dt_poly_t*    s = &((const polydelta_t *) src)->po;
        dt_poly_t*          d = &((polydelta_t *) dest)->po;

        if(sf & PODF_DEST_X)
            d->dest.pos[VX] = s->dest.pos[VX];
        if(sf & PODF_DEST_Y)
            d->dest.pos[VY] = s->dest.pos[VY];
        if(sf & PODF_SPEED)
            d->speed = s->speed;
        if(sf & PODF_DEST_ANGLE)
            d->destAngle = s->destAngle;
        if(sf & PODF_ANGSPEED)
            d->angleSpeed = s->angleSpeed;
    }
    else if(Sv_IsSoundDelta(src))
    {
        const sounddelta_t* s = srcDelta;
        sounddelta_t*       d = destDelta;

        if(sf & SNDDF_VOLUME)
            d->volume = s->volume;
        d->sound = s->sound;
    }
    else
    {
        Con_Error("Sv_ApplyDeltaData: Unknown delta type %i.\n", src->type);
    }
}

/**
 * Merges the second delta with the first one.
 * The source and destination must refer to the same entity.
 *
 * @return              @c false, if the result of the merge is a void delta.
 */
boolean Sv_MergeDelta(void* destDelta, const void* srcDelta)
{
    const delta_t*      src = srcDelta;
    delta_t*            dest = destDelta;

#ifdef _DEBUG
    if(!Sv_IsSameDelta(src, dest))
    {
        Con_Error("Sv_MergeDelta: Not the same!\n");
    }
    if(dest->state != DELTA_NEW)
    {
        Con_Error("Sv_MergeDelta: Dest is not NEW.\n");
    }
#endif

    if(Sv_IsNullMobjDelta(dest))
    {
        // Nothing can be merged with a null mobj delta.
        return true;
    }

    if(Sv_IsNullMobjDelta(src))
    {
        // Null mobj deltas kill the destination.
        dest->flags = MDFC_NULL;
        return true;
    }

    if(Sv_IsCreateMobjDelta(dest) && Sv_IsNullMobjDelta(src))
    {
        // Applying a Null mobj delta on a Create mobj delta causes
        // the two deltas to negate each other. Returning false
        // signifies that both should be removed from the pool.
        dest->flags = 0;
        return false;
    }

    if(Sv_IsStartSoundDelta(src) || Sv_IsStopSoundDelta(src))
    {
        // Sound deltas completely override what they're being
        // merged with. (Only one sound per source.) Stop Sound deltas must
        // kill NEW sound deltas, because what is the benefit of sending
        // both in the same frame: first start a sound and then immediately
        // stop it? We don't want that.

        sounddelta_t*       destSound = destDelta;
        const sounddelta_t* srcSound = srcDelta;

        // Destination becomes equal to source.
        dest->flags = src->flags;

        destSound->sound = srcSound->sound;
        destSound->mobj = srcSound->mobj;
        destSound->volume = srcSound->volume;
        return true;
    }

    // The destination will contain all of source's data in addition
    // to the existing data.
    dest->flags |= src->flags;

    // The time stamp must NOT be always updated: the delta already
    // contains data which should've been sent some time ago. If we
    // update the time stamp now, the overdue data might not be sent.
    /* dest->timeStamp = src->timeStamp; */

    Sv_ApplyDeltaData(dest, src);
    return true;
}

/**
 * @return              The age of the delta, in milliseconds.
 */
uint Sv_DeltaAge(const delta_t* delta)
{
    return Sv_GetTimeStamp() - delta->timeStamp;
}

/**
 * Approximate the distance to the given sector. Set 'mayBeGone' to true
 * if the mobj may have been destroyed and should not be processed.
 */
float Sv_MobjDistance(const mobj_t* mo, const ownerinfo_t* info,
                      boolean isReal)
{
    float z;

    if(isReal && !P_IsUsedMobjID(DMU_CurrentMap(), mo->thinker.id))
    {
        // This mobj does not exist any more!
        return DDMAXFLOAT;
    }

    z = mo->pos[VZ];

    // Registered mobjs may have a maxed out Z coordinate.
    if(!isReal)
    {
        if(z == DDMINFLOAT)
            z = mo->floorZ;
        if(z == DDMAXFLOAT)
            z = mo->ceilingZ - mo->height;
    }

    return P_ApproxDistance3(info->pos[VX] - mo->pos[VX],
                             info->pos[VY] - mo->pos[VY],
                             info->pos[VZ] - z + mo->height / 2);
}

/**
 * Approximate the distance to the given sector.
 */
float Sv_SectorDistance(sector_t* sector, const ownerinfo_t* info)
{
    uint index = DMU_GetObjRecord(DMU_SECTOR, sector)->id - 1;

    return P_ApproxDistance3(info->pos[VX] - sectorOrigins[index].pos[VX],
                             info->pos[VY] - sectorOrigins[index].pos[VY],
                             info->pos[VZ] -
                             ((sector->planes[PLN_CEILING]->height +
                                      sector->planes[PLN_FLOOR]->height) / 2));
}

/**
 * @return              The distance to the origin of the delta's entity.
 */
float Sv_DeltaDistance(const void* deltaPtr, const ownerinfo_t* info)
{
    const delta_t* delta = deltaPtr;

    if(delta->type == DT_MOBJ)
    {
#ifdef _DEBUG
        if(delta->flags & MDFC_NULL)
        {
            delta = delta;
        }
#endif
        // Use the delta's registered mobj position. For old unacked data,
        // it may be somewhat inaccurate.
        return Sv_MobjDistance(&((mobjdelta_t *) deltaPtr)->mo, info, false);
    }

    if(delta->type == DT_PLAYER)
    {
        // Use the player's actual position.
        const mobj_t* mo = ddPlayers[delta->id].shared.mo;

        if(mo)
        {
            return Sv_MobjDistance(mo, info, true);
        }
    }

    if(delta->type == DT_SECTOR)
    {
        gamemap_t* map = DMU_CurrentMap();
        return Sv_SectorDistance(map->sectors[delta->id], info);
    }

    if(delta->type == DT_SIDE)
    {
        return P_ApproxDistance(info->pos[VX] - sideOrigins[delta->id].pos[VX],
                                info->pos[VY] - sideOrigins[delta->id].pos[VY]);
    }

    if(delta->type == DT_POLY)
    {
        gamemap_t* map = DMU_CurrentMap();
        polyobj_t* po = map->polyObjs[delta->id];

        return P_ApproxDistance(info->pos[VX] - po->pos[VX],
                                info->pos[VY] - po->pos[VY]);
    }

    if(delta->type == DT_MOBJ_SOUND)
    {
        const sounddelta_t* sound = deltaPtr;

        return Sv_MobjDistance(sound->mobj, info, true);
    }

    if(delta->type == DT_SECTOR_SOUND)
    {
        gamemap_t* map = DMU_CurrentMap();
        return Sv_SectorDistance(map->sectors[delta->id], info);
    }

    if(delta->type == DT_POLY_SOUND)
    {
        gamemap_t* map = DMU_CurrentMap();
        polyobj_t* po = map->polyObjs[delta->id];

        return P_ApproxDistance(info->pos[VX] - po->pos[VX],
                                info->pos[VY] - po->pos[VY]);
    }

    // Unknown distance.
    return 1;
}

/**
 * The hash function for the pool delta hash.
 */
deltalink_t* Sv_PoolHash(pool_t* pool, int id)
{
    return &pool->hash[(uint) id & POOL_HASH_FUNCTION_MASK];
}

/**
 * The delta is removed from the pool's delta hash.
 */
void Sv_RemoveDelta(pool_t* pool, void* deltaPtr)
{
    delta_t*            delta = deltaPtr;
    deltalink_t*        hash = Sv_PoolHash(pool, delta->id);

    // Update first and last links.
    if(hash->last == delta)
    {
        hash->last = delta->prev;
    }
    if(hash->first == delta)
    {
        hash->first = delta->next;
    }

    // Link the delta out of the list.
    if(delta->next)
    {
        delta->next->prev = delta->prev;
    }

    if(delta->prev)
    {
        delta->prev->next = delta->next;
    }

    // Destroy it.
    Z_Free(delta);
}

/**
 * Draining the pool means emptying it of all contents. (Doh?)
 */
void Sv_DrainPool(uint clientNumber)
{
    pool_t*             pool = &pools[clientNumber];
    delta_t*            delta;
    misrecord_t*        mis;
    void*               next = NULL;
    int                 i;

    // Update the number of the owner.
    pool->owner = clientNumber;

    // Reset the counters.
    pool->setDealer = 0;
    pool->resendDealer = 0;

    Sv_PoolQueueClear(pool);

    // Free all deltas stored in the hash.
    for(i = 0; i < POOL_HASH_SIZE; ++i)
    {
        for(delta = pool->hash[i].first; delta; delta = next)
        {
            next = delta->next;
            Z_Free(delta);
        }
    }

    // Free all missile records in the pool.
    for(i = 0; i < POOL_MISSILE_HASH_SIZE; ++i)
    {
        for(mis = pool->misHash[i].first; mis; mis = next)
        {
            next = mis->next;
            Z_Free(mis);
        }
    }

    // Clear all the chains.
    memset(pool->hash, 0, sizeof(pool->hash));
    memset(pool->misHash, 0, sizeof(pool->misHash));
}

/**
 * @return              The maximum distance for the sound. If the origin
 *                      is any farther, the delta will not be sent to the
 *                      client in question.
 */
float Sv_GetMaxSoundDistance(const sounddelta_t* delta)
{
    float               volume = 1;

    // Volume shortens the maximum distance (why send it if it's not
    // audible?).
    if(delta->delta.flags & SNDDF_VOLUME)
    {
        volume = delta->volume;
    }

    if(volume <= 0)
    {
        // Silence is heard all over the world.
        return DDMAXFLOAT;
    }

    return volume * soundMaxDist;
}

/**
 * @return              The flags that remain after exclusion.
 */
int Sv_ExcludeDelta(pool_t* pool, const void* deltaPtr)
{
    const delta_t*      delta = deltaPtr;
    player_t*           plr = &ddPlayers[pool->owner];
    mobj_t*             poolViewer = plr->shared.mo;
    int                 flags = delta->flags;

    // Can we exclude information from the delta? (for this player only)
    if(delta->type == DT_MOBJ)
    {
        const mobjdelta_t*  mobjDelta = deltaPtr;

        if(poolViewer && poolViewer->thinker.id == delta->id)
        {
            // This is the mobj the owner of the pool uses as a camera.
            flags &= ~MDF_CAMERA_EXCLUDE;

            // This information is sent in the PSV_PLAYER_FIX packet,
            // but only under specific circumstances. Most of the time
            // the client is responsible for updating its own pos/mom/angle.
            flags &= ~MDF_POS;
            flags &= ~MDF_MOM;
            flags &= ~MDF_ANGLE;
        }

        // What about missiles? We might be allowed to exclude some
        // information.
        if(mobjDelta->mo.ddFlags & DDMF_MISSILE)
        {
            if(!Sv_IsCreateMobjDelta(delta))
            {
                // This'll might exclude the coordinates.
                // The missile is put on record when the client acknowledges
                // the Create Mobj delta.
                flags &= ~Sv_MRCheck(pool, mobjDelta);
            }
            else if(Sv_IsNullMobjDelta(delta))
            {
                // The missile is being removed entirely.
                // Remove the entry from the missile record.
                Sv_MRRemove(pool, delta->id);
            }
        }
    }
    else if(delta->type == DT_PLAYER)
    {
        if(pool->owner == delta->id)
        {
            // All information does not need to be sent.
            flags &= ~PDF_CAMERA_EXCLUDE;

            /* $unifiedangles */
            /*
            if(!(player->flags & DDPF_FIXANGLES))
            {
                // Fixangles means that the server overrides the clientside
                // view angles. Normally they are always clientside, so
                // exclude them here.
                flags &= ~(PDF_CLYAW | PDF_CLPITCH);
            }
             */
        }
        else
        {
            // This is a remote player, the owner of the pool doesn't need
            // to know everything about it (like psprites).
            flags &= ~PDF_NONCAMERA_EXCLUDE;
        }
    }
    else if(Sv_IsSoundDelta(delta))
    {
        // Sounds that originate from too far away are not added to a pool.
        // Stop Sound deltas have an infinite max distance, though.
        if(Sv_DeltaDistance(delta, &pool->ownerInfo) >
           Sv_GetMaxSoundDistance(deltaPtr))
        {
            // Don't add it.
            return 0;
        }
    }

    // These are the flags that remain.
    return flags;
}

/**
 * When adding a delta to the pool, it subtracts from the unacked deltas
 * there and is merged with matching new deltas. If a delta becomes void
 * after subtraction, it's removed from the pool. All the processing is
 * done based on the ID number of the delta (and type), so to make things
 * more efficient, a hash table is used (key is ID).
 *
 * Deltas are unique only in the NEW state. There may be multiple UNACKED
 * deltas for the same entity.
 *
 * The contents of the delta must not be modified.
 */
void Sv_AddDelta(pool_t* pool, void* deltaPtr)
{
    delta_t*            iter, *next = NULL, *existingNew = NULL;
    delta_t*            delta = deltaPtr;
    deltalink_t*        hash = Sv_PoolHash(pool, delta->id);
    int                 flags, originalFlags;

    // Sometimes we can exclude a part of the data, if the client has no
    // use for it.
    flags = Sv_ExcludeDelta(pool, delta);

    if(!flags)
    {
        // No data remains... No need to add this delta.
        return;
    }

    // Temporarily use the excluded flags.
    originalFlags = delta->flags;
    delta->flags = flags;

    // While subtracting from old deltas, we'll look for a pointer to
    // an existing NEW delta.
    for(iter = hash->first; iter; iter = next)
    {
        // Iter is removed if it becomes void.
        next = iter->next;

        // Sameness is determined with type and ID.
        if(Sv_IsSameDelta(iter, delta))
        {
            if(iter->state == DELTA_NEW)
            {
                // We'll merge with this instead of adding a new delta.
                existingNew = iter;
            }
            else if(iter->state == DELTA_UNACKED)
            {
                // The new information in the delta overrides the info in
                // this unacked delta. Let's subtract. This way, if the
                // unacked delta needs to be resent, it won't contain
                // obsolete data.
                Sv_SubtractDelta(iter, delta);

                // Was everything removed?
                if(Sv_IsVoidDelta(iter))
                {
                    Sv_RemoveDelta(pool, iter);
                    continue;
                }
            }
        }
    }

    if(existingNew)
    {
        // Merge the new delta with the older NEW delta.
        if(!Sv_MergeDelta(existingNew, delta))
        {
            // The deltas negated each other (Null -> Create).
            // The existing delta must be removed.
            Sv_RemoveDelta(pool, existingNew);
        }
    }
    else
    {
        // Add it to the end of the hash chain. We must take a copy
        // of the delta so it can be stored in the hash.
        iter = Sv_CopyDelta(delta);

        if(hash->last)
        {
            hash->last->next = iter;
            iter->prev = hash->last;
        }
        hash->last = iter;

        if(!hash->first)
        {
            hash->first = iter;
        }
    }

    // This delta may yet be added to other pools. They should use the
    // original flags, not the ones we might've used (hackish: copying the
    // whole delta is not really an option, though).
    delta->flags = originalFlags;
}

/**
 * Add the delta to all the pools in the NULL-terminated array.
 */
void Sv_AddDeltaToPools(void* deltaPtr, pool_t** targets)
{
    for(; *targets; targets++)
    {
        Sv_AddDelta(*targets, deltaPtr);
    }
}

/**
 * All NEW deltas for the mobj are removed from the pool as obsolete.
 */
void Sv_PoolMobjRemoved(pool_t* pool, thid_t id)
{
    deltalink_t*        hash = Sv_PoolHash(pool, id);
    delta_t*            delta, *next = NULL;

    for(delta = hash->first; delta; delta = next)
    {
        next = delta->next;

        if(delta->state == DELTA_NEW && delta->type == DT_MOBJ &&
           delta->id == id)
        {
            // This must be removed!
            Sv_RemoveDelta(pool, delta);
        }
    }

    // Also check the missile record.
    Sv_MRRemove(pool, id);
}

/**
 * This is called when a mobj is removed in a predictable fashion.
 * (Mobj state is NULL when it's destroyed. Assumption: The NULL state
 * is set only when animation reaches its end.) Because the register-mobj
 * is removed, no Null Mobj delta is generated for the mobj.
 */
void Sv_MobjRemoved(gamemap_t* map, thid_t id)
{
    uint i;
    reg_mobj_t* mo = Sv_RegisterFindMobj(&worldRegister, id);

    if(mo)
    {
        Sv_RegisterRemoveMobj(&worldRegister, mo);

        // We must remove all NEW deltas for this mobj from the pools.
        // One possibility: there are mobj deltas waiting in the pool,
        // but the mobj is removed here. Because it'll be no longer in
        // the register, no Null Mobj delta is generated, and thus the
        // client will eventually receive those mobj deltas unnecessarily.

        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            if(clients[i].connected)
            {
                Sv_PoolMobjRemoved(&pools[i], id);
            }
        }
    }
}

/**
 * When a player leaves the game, his data is removed from the register.
 * Otherwise he'll not get all the data if he reconnects before the map
 * is changed.
 */
void Sv_PlayerRemoved(uint playerNumber)
{
    dt_player_t*        p = &worldRegister.ddPlayers[playerNumber];

    memset(p, 0, sizeof(*p));
}

/**
 * @return              @c true, if the pool is in the targets array.
 */
boolean Sv_IsPoolTargeted(pool_t* pool, pool_t** targets)
{
    for(; *targets; targets++)
    {
        if(pool == *targets)
            return true;
    }

    return false;
}

/**
 * Fills the array with pointers to the pools of the connected clients,
 * if specificClient is < 0.
 *
 * @return              The number of pools in the list.
 */
int Sv_GetTargetPools(pool_t** targets, int clientsMask)
{
    int                 i, numTargets = 0;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(clientsMask & (1 << i) && clients[i].connected)
        {
            targets[numTargets++] = &pools[i];
        }
    }
/*
    if(specificClient & SVSF_ specificClient >= 0)
    {
        targets[0] = &pools[specificClient];
        targets[1] = NULL;
        return 1;
    }

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        // Deltas must be generated for all connected players, even
        // if they aren't yet ready to receive them.
        if(clients[i].connected)
        {
            if(specificClient == SV_TARGET_ALL_POOLS || i != -specificClient)
                targets[numTargets++] = &pools[i];
        }
    }
*/
    // A NULL pointer marks the end of target pools.
    targets[numTargets] = NULL;

    return numTargets;
}

/**
 * Null deltas are generated for mobjs that have been destroyed.
 * The register's mobj hash is scanned to see which mobjs no longer exist.
 *
 * When updating, the destroyed mobjs are removed from the register.
 */
void Sv_NewNullDeltas(gamemap_t* map, cregister_t* reg, boolean doUpdate, pool_t** targets)
{
    int i;
    mobjhash_t* hash;
    reg_mobj_t* obj, *next = 0;
    mobjdelta_t null;

    if(!map)
        return;

    for(i = 0, hash = reg->mobjs; i < REG_MOBJ_HASH_SIZE; ++i, hash++)
    {
        for(obj = hash->first; obj; obj = next)
        {
            // This reg_mobj_t might be removed.
            next = obj->next;

            if(!P_IsUsedMobjID(map, obj->mo.thinker.id))
            {
                // This object no longer exists!
                Sv_NewDelta(&null, DT_MOBJ, obj->mo.thinker.id);
                null.delta.flags = MDFC_NULL;

                // We need all the data for positioning.
                memcpy(&null.mo, &obj->mo, sizeof(dt_mobj_t));

                Sv_AddDeltaToPools(&null, targets);

                /*#ifdef _DEBUG
                   Con_Printf("New null: %i, %s\n", obj->mo.thinker.id,
                   defs.states[obj->mo.state - states].id);
                   #endif */

                if(doUpdate)
                {
                    // Keep the register up to date.
                    Sv_RegisterRemoveMobj(reg, obj);
                }
            }
        }
    }
}

typedef struct {
    cregister_t*        reg;
    boolean             doUpdate;
    pool_t**            targets;
} newmobjdeltaparams_t;

static int newMobjDelta(void* p, void* context)
{
    newmobjdeltaparams_t* params = (newmobjdeltaparams_t*) context;
    mobj_t*             mo = (mobj_t*) p;

    // Some objects should not be processed.
    if(!Sv_IsMobjIgnored(mo))
    {
        mobjdelta_t         delta;

        // Compare to produce a delta.
        if(Sv_RegisterCompareMobj(params->reg, mo, &delta))
        {
            Sv_AddDeltaToPools(&delta, params->targets);

            if(params->doUpdate)
            {
                reg_mobj_t*         obj;

                // This'll add a new register-mobj if it doesn't
                // already exist.
                obj = Sv_RegisterAddMobj(params->reg, mo->thinker.id);
                Sv_RegisterMobj(&obj->mo, mo);
            }
        }
    }

    return true; // Continue iteration.
}

/**
 * Mobj deltas are generated for all mobjs that have changed.
 */
void Sv_NewMobjDeltas(gamemap_t* map, cregister_t* reg, boolean doUpdate, pool_t** targets)
{
    newmobjdeltaparams_t params;

    params.reg = reg;
    params.doUpdate = doUpdate;
    params.targets = targets;

    // Mobjs are always public.
    P_IterateThinkers(map, gx.MobjThinker, ITF_PUBLIC, newMobjDelta, &params);
}

/**
 * Player deltas are generated for changed player data.
 */
void Sv_NewPlayerDeltas(cregister_t* reg, boolean doUpdate, pool_t** targets)
{
    uint i;
    playerdelta_t player;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(Sv_IsPlayerIgnored(i))
            continue;

        // Compare to produce a delta.
        if(Sv_RegisterComparePlayer(reg, i, &player))
        {
            // Did the mobj change? If so, the old mobj must be zeroed
            // in the register. Otherwise, the clients may not receive
            // all the data they need (because of viewpoint exclusion
            // flags).
            if(doUpdate && (player.delta.flags & PDF_MOBJ))
            {
                reg_mobj_t*         registered =
                    Sv_RegisterFindMobj(reg, reg->ddPlayers[i].mobj);

                if(registered)
                {
                    Sv_RegisterResetMobj(&registered->mo);
                }
            }

            Sv_AddDeltaToPools(&player, targets);
        }

        if(doUpdate)
        {
            Sv_RegisterPlayer(&reg->ddPlayers[i], i);
        }

        // What about forced deltas?
        if(Sv_IsPoolTargeted(&pools[i], targets))
        {
#if 0
            if(ddPlayers[i].flags & DDPF_FIXANGLES)
            {
                Sv_NewDelta(&player, DT_PLAYER, i);
                Sv_RegisterPlayer(&player.player, i);
                //player.delta.flags = PDF_CLYAW | PDF_CLPITCH; /* $unifiedangles */

                // Once added to the pool, the information will not get lost.
                Sv_AddDelta(&pools[i], &player);

                // Doing this once is enough.
                ddPlayers[i].flags &= ~DDPF_FIXANGLES;
            }

            // Generate a FIXPOS/FIXMOM mobj delta, too?
            if(ddPlayers[i].mo && (ddPlayers[i].flags & (DDPF_FIXPOS | DDPF_FIXMOM)))
            {
                const mobj_t *mo = ddPlayers[i].mo;
                mobjdelta_t mobj;

                Sv_NewDelta(&mobj, DT_MOBJ, mo->thinker.id);
                Sv_RegisterMobj(&mobj.mo, mo);
                if(ddPlayers[i].flags & DDPF_FIXPOS)
                {
                    mobj.delta.flags |= MDF_POS;
                }
                if(ddPlayers[i].flags & DDPF_FIXMOM)
                {
                    mobj.delta.flags |= MDF_MOM;
                }

                Sv_AddDelta(&pools[i], &mobj);

                // Doing this once is enough.
                ddPlayers[i].flags &= ~(DDPF_FIXPOS | DDPF_FIXMOM);
            }
#endif
        }
    }
}

/**
 * Sector deltas are generated for changed sectors.
 */
void Sv_NewSectorDeltas(gamemap_t* map, cregister_t* reg, boolean doUpdate, pool_t** targets)
{
    uint i;
    sectordelta_t delta;

    for(i = 0; i < map->numSectors; ++i)
    {
        if(Sv_RegisterCompareSector(map, i, reg, &delta, doUpdate))
        {
            Sv_AddDeltaToPools(&delta, targets);
        }
    }
}

/**
 * Side deltas are generated for changed sides (and line flags).
 * Changes in sides (textures) are so rare that all sides need not be
 * checked on every tic.
 */
void Sv_NewSideDeltas(gamemap_t* map, cregister_t* reg, boolean doUpdate, pool_t** targets)
{
    static uint numShifts = 2, shift = 0;
    sidedelta_t delta;
    uint i, start, end;

    // When comparing against an initial register, always compare all
    // sides (since the comparing is only done once, not continuously).
    if(reg->isInitial)
    {
        start = 0;
        end = map->numSideDefs;
    }
    else
    {
        // Because there are so many sides in a typical map, the number
        // of compared sides soon accumulates to millions. To reduce the
        // load, we'll check only a portion of all sides for a frame.
        start = shift * map->numSideDefs / numShifts;
        end = ++shift * map->numSideDefs / numShifts;
        shift %= numShifts;
    }

    for(i = start; i < end; ++i)
    {
        // The side must be owned by a line.
        if(sideOwners[i] == NULL)
            continue;

        if(Sv_RegisterCompareSide(map, i, reg, &delta, doUpdate))
        {
            Sv_AddDeltaToPools(&delta, targets);
        }
    }
}

/**
 * Poly deltas are generated for changed polyobjs.
 */
void Sv_NewPolyDeltas(gamemap_t* map, cregister_t* reg, boolean doUpdate, pool_t** targets)
{
    uint i;
    polydelta_t delta;

    for(i = 0; i < map->numPolyObjs; ++i)
    {
        if(Sv_RegisterComparePoly(map, i, reg, &delta))
        {
#ifdef _DEBUG
Con_Printf("Sv_NewPolyDeltas: Change in %i\n", i);
#endif
            Sv_AddDeltaToPools(&delta, targets);
        }

        if(doUpdate)
        {
            Sv_RegisterPoly(map, i, &reg->polyObjs[i]);
        }
    }
}

/**
 * Adds a new sound delta to the appropriate pools.
 * Because the starting of a sound is in itself a 'delta-like' event,
 * there is no need for comparing or to have a register.
 * Set 'volume' to zero to create a sound-stopping delta.
 *
 * \assume: No two sounds with the same ID play at the same time from the
 *          same origin.
 */
void Sv_NewSoundDelta(int soundId, mobj_t* emitter, sector_t* sourceSector,
                      polyobj_t* sourcePoly, float volume,
                      boolean isRepeating, int clientsMask)
{
    pool_t* targets[DDMAXPLAYERS + 1];
    sounddelta_t soundDelta;
    int type = DT_SOUND, df = 0;
    uint id = soundId;

    // Determine the target pools.
    Sv_GetTargetPools(targets, clientsMask);

    if(sourceSector != NULL)
    {
        type = DT_SECTOR_SOUND;
        id = DMU_GetObjRecord(DMU_SECTOR, sourceSector)->id - 1;

        // Clients need to know which emitter to use.
        if(emitter)
        {
            if(emitter == (mobj_t*) &sourceSector->planes[PLN_FLOOR]->soundOrg)
                df |= SNDDF_FLOOR;
            else if(emitter == (mobj_t*) &sourceSector->planes[PLN_CEILING]->soundOrg)
                df |= SNDDF_CEILING;
            // else client assumes sector->soundOrg
        }
    }
    else if(sourcePoly != NULL)
    {
        type = DT_POLY_SOUND;
        id = sourcePoly->idx;
    }
    else if(emitter)
    {
        type = DT_MOBJ_SOUND;
        id = emitter->thinker.id;
        soundDelta.mobj = emitter;
    }

    // Init to the right type.
    Sv_NewDelta(&soundDelta, type, id);

    // Always set volume.
    df |= SNDDF_VOLUME;
    soundDelta.volume = volume;

    if(isRepeating)
        df |= SNDDF_REPEAT;

    // This is used by mobj/sector sounds.
    soundDelta.sound = soundId;

    soundDelta.delta.flags = df;
    Sv_AddDeltaToPools(&soundDelta, targets);
}

/**
 * @return              @c true, if the client should receive frames.
 */
boolean Sv_IsFrameTarget(uint plrNum)
{
    player_t*           plr = &ddPlayers[plrNum];
    ddplayer_t*         ddpl = &plr->shared;

    // Local players receive frames only when they're recording a demo.
    // Clients must tell us they are ready before we can begin sending.
    return (ddpl->inGame && !(ddpl->flags & DDPF_LOCAL) &&
            clients[plrNum].ready) ||
           ((ddpl->flags & DDPF_LOCAL) && clients[plrNum].recording);
}

/**
 * Compare the current state of the world with the register and add the
 * deltas to all the pools, or if a specific client number is given, only
 * to its pool (done when a new client enters the game). No deltas will be
 * generated for predictable changes (state changes, linear movement...).
 *
 * Updating the register means that the current state of the world is stored
 * in the register after the deltas have been generated.
 *
 * @param clientNumber  < 0 = All ingame clients should get the deltas.
 */
void Sv_GenerateNewDeltas(cregister_t* reg, int clientNumber,
                          boolean doUpdate)
{
    pool_t* targets[DDMAXPLAYERS + 1], **pool;
    gamemap_t* map;

    // Determine the target pools.
    Sv_GetTargetPools(targets, (clientNumber < 0 ? 0xff : (1 << clientNumber)));

    // Update the info of the pool owners.
    for(pool = targets; *pool; pool++)
    {
        Sv_UpdateOwnerInfo(*pool);
    }

    // Generate player deltas.
    Sv_NewPlayerDeltas(reg, doUpdate, targets);

    map = DMU_CurrentMap();
    if(map)
    {
        // Generate null deltas (removed mobjs).
        Sv_NewNullDeltas(map, reg, doUpdate, targets);

        // Generate mobj deltas.
        Sv_NewMobjDeltas(map, reg, doUpdate, targets);

        // Generate sector deltas.
        Sv_NewSectorDeltas(map, reg, doUpdate, targets);

        // Generate side deltas.
        Sv_NewSideDeltas(map, reg, doUpdate, targets);

        // Generate poly deltas.
        Sv_NewPolyDeltas(map, reg, doUpdate, targets);
    }

    if(doUpdate)
    {
        // The register has now been updated to the current time.
        reg->gametic = SECONDS_TO_TICKS(gameTime);
    }
}

/**
 * This is called once for each frame, in Sv_TransmitFrame().
 */
void Sv_GenerateFrameDeltas(void)
{
    // Generate new deltas for all clients and update the world register.
    Sv_GenerateNewDeltas(&worldRegister, -1, true);
}

/**
 * Clears the priority queue of the pool.
 */
void Sv_PoolQueueClear(pool_t* pool)
{
    pool->queueSize = 0;
}

/**
 * Exchanges two elements in the queue.
 */
void Sv_PoolQueueExchange(pool_t* pool, int index1, int index2)
{
    delta_t *temp = pool->queue[index1];

    pool->queue[index1] = pool->queue[index2];
    pool->queue[index2] = temp;
}

/**
 * Adds the delta to the priority queue. More memory is allocated for the
 * queue if necessary.
 */
void Sv_PoolQueueAdd(pool_t* pool, delta_t* delta)
{
    int                 i, parent;

    // Do we need more memory?
    if(pool->allocatedSize == pool->queueSize)
    {
        delta_t**           newQueue;

        // Double the memory.
        pool->allocatedSize *= 2;
        if(!pool->allocatedSize)
        {
            // At least eight.
            pool->allocatedSize = 8;
        }

        // Allocate the new queue.
        newQueue = Z_Malloc(pool->allocatedSize * sizeof(delta_t *),
                            PU_MAP, 0);

        // Copy the old data.
        if(pool->queue)
        {
            memcpy(newQueue, pool->queue,
                   sizeof(delta_t *) * pool->queueSize);

            // Get rid of the old queue.
            Z_Free(pool->queue);
        }

        pool->queue = newQueue;
    }

    // Add the new delta to the end of the queue array.
    i = pool->queueSize;
    pool->queue[i] = delta;
    ++pool->queueSize;

    // Rise in the heap until the correct place is found.
    while(i > 0)
    {
        parent = HEAP_PARENT(i);

        // Is it good now?
        if(pool->queue[parent]->score >= delta->score)
            break;

        // Exchange with the parent.
        Sv_PoolQueueExchange(pool, parent, i);

        i = parent;
    }
}

/**
 * Extracts the delta with the highest priority from the queue.
 *
 * @return              @c NULL, if there are no more deltas.
 */
delta_t* Sv_PoolQueueExtract(pool_t* pool)
{
    delta_t*            max;
    int                 i, left, right, big;
    boolean             isDone;

    if(!pool->queueSize)
    {
        // There is nothing in the queue.
        return NULL;
    }

    // This is what we'll return.
    max = pool->queue[0];

    // Remove the first element from the queue.
    pool->queue[0] = pool->queue[--pool->queueSize];

    // Heapify the heap. This is O(log n).
    i = 0;
    isDone = false;
    while(!isDone)
    {
        left = HEAP_LEFT(i);
        right = HEAP_RIGHT(i);
        big = i;

        // Which child is more important?
        if(left < pool->queueSize &&
           pool->queue[left]->score > pool->queue[i]->score)
        {
            big = left;
        }
        if(right < pool->queueSize &&
           pool->queue[right]->score > pool->queue[big]->score)
        {
            big = right;
        }

        // Can we stop now?
        if(big != i)
        {
            // Exchange and continue.
            Sv_PoolQueueExchange(pool, i, big);
            i = big;
        }
        else
        {
            // Heapifying is complete.
            isDone = true;
        }
    }

    return max;
}

/**
 * Postponed deltas can't be sent yet.
 */
boolean Sv_IsPostponedDelta(void* deltaPtr, ownerinfo_t* info)
{
    delta_t*            delta = deltaPtr;
    uint                age = Sv_DeltaAge(delta);

    if(delta->state == DELTA_UNACKED)
    {
        // Is it old enough? If not, it's still possible that the ack
        // has not reached us yet.
        return age < info->ackThreshold;
    }
    else if(delta->state == DELTA_NEW)
    {
        // Normally NEW deltas are never postponed. They are sent as soon
        // as possible.
        if(Sv_IsStopSoundDelta(delta))
        {
            // Stop Sound deltas require a bit of care. To make sure they
            // arrive to the client in the correct order, we won't send
            // a Stop Sound until we can be sure all the Start Sound deltas
            // have arrived. (i.e. the pool must contain no Unacked Start
            // Sound deltas for the same source.)

            delta_t*            iter;

            for(iter = Sv_PoolHash(info->pool, delta->id)->first; iter;
                iter = iter->next)
            {
                if(iter->state == DELTA_UNACKED && Sv_IsSameDelta(iter, delta)
                   && Sv_IsStartSoundDelta(iter))
                {
                    // Must postpone this Stop Sound delta until this one
                    // has been sent.
#ifdef _DEBUG
Con_Printf("POSTPONE: Stop %i\n", delta->id);
#endif
                    return true;
                }
            }
        }
    }

    // This delta is not postponed.
    return false;
}

/**
 * Calculate a priority score for the delta. A higher score indicates
 * greater importance.
 *
 * @return              @c true iff the delta should be included in the
 *                      queue.
 */
boolean Sv_RateDelta(void* deltaPtr, ownerinfo_t* info)
{
    float               score, distance, size;
    delta_t*            delta = deltaPtr;
    int                 df = delta->flags;
    uint                age = Sv_DeltaAge(delta);

    // The importance doubles normally in 1 second.
    float               ageScoreDouble = 1.0f;

    if(Sv_IsPostponedDelta(delta, info))
    {
        // This delta will not be considered at this time.
        return false;
    }

    // Calculate the distance to the delta's origin.
    // If no distance can be determined, it's 1.0.
    distance = Sv_DeltaDistance(delta, info);
    if(distance < 1)
        distance = 1;
    distance = distance * distance; // Power of two.

    // What is the base score?
    score = deltaBaseScores[delta->type] / distance;

    // It's very important to send sound deltas in time.
    if(Sv_IsSoundDelta(delta))
    {
        // Score doubles very quickly.
        ageScoreDouble = 1;
    }

    // Deltas become more important with age (milliseconds).
    score *= 1 + age / (ageScoreDouble * 1000.0f);

    // \fixme Consider viewpoint speed and angle.

    // Priority bonuses based on the contents of the delta.
    if(delta->type == DT_MOBJ)
    {
        const mobj_t*       mo = &((mobjdelta_t *) delta)->mo;

        // Seeing new mobjs is interesting.
        if(df & MDFC_CREATE)
            score *= 1.5f;

        // Position changes are important.
        if(df & (MDF_POS_X | MDF_POS_Y))
            score *= 1.2f;

        // Small objects are not that important.
        size = MAX_OF(mo->radius, mo->height);
        if(size < 16)
        {
            // Not too small, though.
            if(size < 2)
                size = 2;

            score *= size / 16;
        }
        else if(size > 50)
        {
            // Large objects are important.
            score *= size / 50;
        }
    }
    else if(delta->type == DT_PLAYER)
    {
        // Knowing the player's mobj is quite important.
        if(df & PDF_MOBJ)
            score *= 2;
    }
    else if(delta->type == DT_SECTOR)
    {
        // Lightlevel changes are very noticeable.
        if(df & SDF_LIGHT)
            score *= 1.2f;

        // Plane movements are very important (can be seen from far away).
        if(df &
           (SDF_FLOOR_HEIGHT | SDF_CEILING_HEIGHT | SDF_FLOOR_SPEED |
            SDF_CEILING_SPEED | SDF_FLOOR_TARGET | SDF_CEILING_TARGET))
        {
            score *= 3;
        }
    }
    else if(delta->type == DT_POLY)
    {
        // Changes in speed are noticeable.
        if(df & PODF_SPEED)
            score *= 1.2f;
    }

    // This is the final score. Only positive scores are accepted in
    // the frame (deltas with nonpositive scores as ignored).
    return (delta->score = score? true : false);
}

/**
 * Calculate a priority score for each delta and build the priority queue.
 * The most important deltas will be included in a frame packet.
 * A pool is rated after new deltas have been generated.
 */
void Sv_RatePool(pool_t* pool)
{
#ifdef _DEBUG
    player_t*           plr = &ddPlayers[pool->owner];
    //client_t*           client = &clients[pool->owner];
#endif
    delta_t*            delta;
    int                 i;

#ifdef _DEBUG
    if(!plr->shared.mo)
    {
        Con_Error("Sv_RatePool: Player %i has no mobj.\n", pool->owner);
    }
#endif

    // Clear the queue.
    Sv_PoolQueueClear(pool);

    // We will rate all the deltas in the pool. After each delta
    // has been rated, it's added to the priority queue.
    for(i = 0; i < POOL_HASH_SIZE; ++i)
    {
        for(delta = pool->hash[i].first; delta; delta = delta->next)
        {
            if(Sv_RateDelta(delta, &pool->ownerInfo))
            {
                Sv_PoolQueueAdd(pool, delta);
            }
        }
    }
}

/**
 * Do special things that need to be done when the delta has been acked.
 */
void Sv_AckDelta(pool_t* pool, delta_t* delta)
{
    if(Sv_IsCreateMobjDelta(delta))
    {
        mobjdelta_t*        mobjDelta = (mobjdelta_t *) delta;

        // Created missiles are put on record.
        if(mobjDelta->mo.ddFlags & DDMF_MISSILE)
        {
            // Once again, we're assuming the delta is always completely
            // filled with valid information. (There are no 'partial' deltas.)
            Sv_MRAdd(pool, mobjDelta);
        }
    }
}

/**
 * Acknowledged deltas are removed from the pool, never to be seen again.
 * Clients ack deltas to tell the server they've received them.
 *
 * @param resent        If nonzero, ignore 'set' and ack by resend ID.
 */
void Sv_AckDeltaSet(uint clientNumber, int set, byte resent)
{
    int                 i;
    pool_t*             pool = &pools[clientNumber];
    delta_t*            delta, *next = NULL;
    boolean             ackTimeRegistered = false;

    // Iterate through the entire hash table.
    for(i = 0; i < POOL_HASH_SIZE; ++i)
    {
        for(delta = pool->hash[i].first; delta; delta = next)
        {
            next = delta->next;
            if(delta->state == DELTA_UNACKED &&
               ((!resent && delta->set == set) ||
                (resent && delta->resend == resent)))
            {
                // Register the ack time only for the first acked delta.
                if(!ackTimeRegistered)
                {
                    Net_SetAckTime(clientNumber, Sv_DeltaAge(delta));
                    ackTimeRegistered = true;
                }

                // There may be something that we need to do now that the
                // delta has been acknowledged.
                Sv_AckDelta(pool, delta);

                // This delta is now finished!
                Sv_RemoveDelta(pool, delta);
            }
        }
    }
}

/**
 * Debugging metric.
 */
uint Sv_CountUnackedDeltas(uint clientNumber)
{
    uint                i, count;
    pool_t*             pool = Sv_GetPool(clientNumber);
    delta_t*            delta;

    // Iterate through the entire hash table.
    count = 0;
    for(i = 0; i < POOL_HASH_SIZE; ++i)
    {
        for(delta = pool->hash[i].first; delta; delta = delta->next)
        {
            if(delta->state == DELTA_UNACKED)
               ++count;
        }
    }
    return count;
}
